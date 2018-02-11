/*

$Id: io.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: io.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:16  sam
Added RCS tags


*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <errno.h>
#include <zlib.h>

#include "server.h"
#include "client.h"
#define NEED_PLAYER_STRUCT
#include "player.h"
#undef NEED_PLAYER_STRUCT
#include "log.h"
#include "mem.h"
#include "date.h"
#include "io.h"

struct player **player;

void *my_zlib_malloc(void *dummy,unsigned int cnt,unsigned int size)
{
	return xmalloc(cnt*size,IM_ZLIB);
}

void my_zlib_free(void *dummy,void *ptr)
{
	return xfree(ptr);
}

// close connection to player nr
void exit_player(int nr)
{
	online--;
	close(player[nr]->sock);
        deflateEnd(&player[nr]->zs);
	xfree(player[nr]);
        player[nr]=NULL;
	mem_usage-=sizeof(struct player);
}

/* Process a new connection by finding, initializing and connecting a player entry to a new socket */
static void new_player(int sock)
{
        int n,nsock,len=sizeof(struct sockaddr_in); //,one=1; //,zero=0;
        u_long one=1;
        struct sockaddr_in addr;
	char buf[16];

        nsock=accept(sock,(struct sockaddr *)&addr,&len);
        if (nsock==-1) return;

        ioctl(nsock,FIONBIO,(u_long*)&one);     // non-blocking mode

	//setsockopt(nsock,IPPROTO_TCP,TCP_NODELAY,(const char *)&one,sizeof(int));
        //setsockopt(nsock,SOL_SOCKET,SO_LINGER,(const char *)&zero,sizeof(int));
        //setsockopt(nsock,SOL_SOCKET,SO_KEEPALIVE,(const char *)&one,sizeof(int));

        for (n=1; n<MAXPLAYER; n++) if (!player[n]) break;
        if (n==MAXPLAYER) { close(nsock); return; }

	player[n]=xcalloc(sizeof(struct player),IM_PLAYER); mem_usage+=sizeof(struct player);
	if (player[n]==NULL) { close(nsock); return; }

        //bzero(player+n,sizeof(player[0]));

	player[n]->zs.zalloc=my_zlib_malloc;
	player[n]->zs.zfree=my_zlib_free;

	//if (deflateInit(&player[n]->zs,1)) { close(nsock); xfree(player[n]); player[n]=NULL; mem_usage-=sizeof(struct player); return; }
	if (deflateInit2(&player[n]->zs,1,Z_DEFLATED,14,7,Z_DEFAULT_STRATEGY)) { close(nsock); xfree(player[n]); player[n]=NULL; mem_usage-=sizeof(struct player); return; }

        player[n]->sock=nsock;
        player[n]->addr=addr.sin_addr.s_addr;
        player[n]->state=ST_CONNECT;        	
	player[n]->lastcmd=ticker;

	/* buf[0]=SV_TICKER;
	*(unsigned int*)(buf+1)=ticker;
	psend(n,buf,5); */

	buf[0]=SV_REALTIME;
	*(unsigned int*)(buf+1)=realtime;
	psend(n,buf,5);

	online++;
	if (!player[n]) return;

	//xlog("new player at %d (%d.%d.%d.%d)",n,(player[n]->addr>>0)&255,(player[n]->addr>>8)&255,(player[n]->addr>>16)&255,(player[n]->addr>>24)&255);
}

static void send_player(int nr)
{
        int ret,len;
        unsigned long long prof;

        if (player[nr]->iptr<player[nr]->optr) {
                len=OBUFSIZE-player[nr]->optr;
        } else {
                len=player[nr]->iptr-player[nr]->optr;
        }

	//xlog("rem: iptr=%d, optr=%d, len=%d",player[nr]->iptr,player[nr]->optr,len);

        prof=prof_start(11); ret=send(player[nr]->sock,player[nr]->obuf+player[nr]->optr,len,0); prof_stop(11,prof);
        if (ret==-1) {  // send failure
		//xlog("send failure, kicking player %d",nr);
                kick_player(nr,NULL);
                return;
        }

        player[nr]->optr+=ret;

        if (player[nr]->optr==OBUFSIZE) player[nr]->optr=0;

	sent_bytes_raw+=ret+60;
	sent_bytes+=ret;
}

static int csend(int nr,unsigned char *buf,int len)
{
        int size;

        if (!player[nr]) return -1;

	while (len) {
		if (player[nr]->iptr>=player[nr]->optr) {
			size=OBUFSIZE-player[nr]->iptr;
		} else {
			size=player[nr]->optr-player[nr]->iptr;
		}
		size=min(size,len);
		memcpy(player[nr]->obuf+player[nr]->iptr,buf,size);

		player[nr]->iptr+=size; if (player[nr]->iptr==OBUFSIZE) player[nr]->iptr=0;
		buf+=size; len-=size;

		if (player[nr]->iptr==player[nr]->optr) {
			//xlog("send buffer overflow, kicking player %d",nr);
                        kick_player(nr,NULL);
                        return -1;
		}
		//xlog("add: iptr=%d, optr=%d, len=%d, size=%d",player[nr]->iptr,player[nr]->optr,len,size);
	}
        return 0;
}

static void rec_player(int nr)
{
        int len;

        len=recv(player[nr]->sock,(char*)player[nr]->inbuf+player[nr]->in_len,256-player[nr]->in_len,0);

        if (len<1) {    // receive failure
                if (errno!=EWOULDBLOCK) {
			//xlog("receive failure, kicking player %d",nr);
                        kick_player(nr,NULL);
                }
                return;
        }
        player[nr]->in_len+=len;
	
	rec_bytes_raw+=len+60;
	rec_bytes+=len;
}

static int io_sock;

void io_loop(void)
{
        int n,fmax=0,tmp;						
        fd_set in_fd,out_fd;
        struct timeval tv;
        int panic=0;
	unsigned long long prof;

	if (ticker%8) return;

 	while (panic++<100) {	
                FD_ZERO(&in_fd); FD_ZERO(&out_fd); fmax=0;

                FD_SET(io_sock,&in_fd);
                if (io_sock>fmax) fmax=io_sock;

                for (n=1; n<MAXPLAYER; n++) {
                        if (player[n]) {
                                if (player[n]->in_len<256) {
                                        FD_SET(player[n]->sock,&in_fd);
                                        if (player[n]->sock>fmax) fmax=player[n]->sock;
                                }
			}
			if (player[n]) {
                                if (player[n]->iptr!=player[n]->optr) {
                                        FD_SET(player[n]->sock,&out_fd);
                                        if (player[n]->sock>fmax) fmax=player[n]->sock;
                                }
                        }
                }

                tv.tv_sec=0;
                tv.tv_usec=0;

                tmp=select(fmax+1,&in_fd,&out_fd,NULL,&tv);

                if (tmp<1) break;

                if (FD_ISSET(io_sock,&in_fd)) new_player(io_sock);

                for (n=1; n<MAXPLAYER; n++) {
                        if (!player[n]) continue;
                        if (FD_ISSET(player[n]->sock,&in_fd)) { prof=prof_start(9); rec_player(n); prof_stop(9,prof); }
			if (!player[n]) continue;	// yuck - rec_player might have kicked the player
			if (FD_ISSET(player[n]->sock,&out_fd)){ prof=prof_start(10); send_player(n); prof_stop(10,prof); }
                }
        }
}


/*unsigned int get_interface2(int sock)
{
	int n;
	unsigned int addr;
	struct ifconf ifc;
	struct ifreq req[16];

	bzero(&ifc,sizeof(ifc));
	bzero(&req,sizeof(req));

	ifc.ifc_len=sizeof(req);
	ifc.ifc_ifcu.ifcu_req=req;
	
	if (ioctl(sock,SIOCGIFCONF,&ifc)) return 0;

	for (n=0; n<16; n++) {
		//xlog("buf[%d]=%d (%c)",n,buf[n],buf[n]);			
                addr=htonl(((struct sockaddr_in *)&ifc.ifc_ifcu.ifcu_req[n].ifr_ifru.ifru_addr)->sin_addr.s_addr);
                if (addr) {
			xlog("interface %s: %d.%d.%d.%d",ifc.ifc_ifcu.ifcu_req[n].ifr_ifrn.ifrn_name,(addr>>24)&255,(addr>>16)&255,(addr>>8)&255,(addr>>0)&255);
		}
		if (addr && addr!=((127<<24)+(1<<0)) && (!server_net || server_net==(addr>>24))) return addr;
	}
	return 0;
}*/

/*unsigned int get_interface(void)
{
	char hostname[256];
	struct hostent *he;
	unsigned int addr,n;

        if (gethostname(hostname,sizeof(hostname))) return 0;
	if (!(he=gethostbyname(hostname))) return 0;

	for (n=0; he->h_addr_list[n]; n++) {
		addr=htonl(*(unsigned int*)(he->h_addr_list[n]));
		xlog("%s: %d.%d.%d.%d",hostname,(addr>>24)&255,(addr>>16)&255,(addr>>8)&255,(addr>>0)&255);
		//if (addr && addr!=((127<<24)+(1<<0)) && (!server_net || server_net==(addr>>24))) return addr;
	}
	return 0;
}*/

int init_io(void)
{
	int sock,port;
	u_long one=1;
        struct sockaddr_in addr;

	player=xcalloc(sizeof(struct player*)*MAXPLAYER,IM_BASE);
	if (!player) return 0;

        xlog("Allocated players: %.2fM (%d*%d)",sizeof(struct player*)*MAXPLAYER/1024.0/1024.0,sizeof(struct player*),MAXPLAYER);
	mem_usage+=sizeof(struct player*)*MAXPLAYER;

	xlog("Size of player data: %d bytes",sizeof(struct player));

        sock=socket(PF_INET,SOCK_STREAM,0);
        if (sock==-1) return 0;

        ioctl(sock,FIONBIO,(u_long*)&one);      // non-blocking mode
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&one,sizeof(int));

	for (port=5556; port<5600; port++) {
		addr.sin_family=AF_INET;
		addr.sin_port=htons(port);
		addr.sin_addr.s_addr=0;
	
		if (!bind(sock,(struct sockaddr *)&addr,sizeof(addr))) break;
	}

        if (listen(sock,50)) return 0;

        io_sock=sock;

	/*if (!(server_addr=get_interface2(sock))) {
		elog("could not find interface (net=%d)",server_net);
		return 0;
	}*/
	server_addr=((212<<24)|(202<<16)|(240<<8)|(67<<0))+serverID-1;
	
	server_port=port;

	xlog("IO Init done: ID=%d (%d.%d.%d.%d:%d)",serverID,(server_addr>>24)&255,(server_addr>>16)&255,(server_addr>>8)&255,(server_addr>>0)&255,server_port);

	return 1;
}

void exit_io(void)
{
	close(io_sock);
}

void psend(int nr,char *buf,int len)
{
	if (!player[nr]) return;
	
	if (player[nr]->tptr+len>=OBUFSIZE) {
		xlog("tptr overflow!");
		kick_player(nr,NULL);
		return;
	}
	memcpy(player[nr]->tbuf+player[nr]->tptr,buf,len);
	player[nr]->tptr+=len;
}

// careful here, any csend might clear player[n]!
void pflush(void)
{
	int n,ilen,olen,csize,ret,olow,ohigh;
	unsigned char obuf[OBUFSIZE];
	unsigned long long prof;
	
	for (n=1; n<MAXPLAYER; n++) {
		if (!player[n]) continue;
	
		ilen=player[n]->tptr;

		if (ilen>16) {
			player[n]->zs.next_in=player[n]->tbuf;
			player[n]->zs.avail_in=ilen;
	
			player[n]->zs.next_out=obuf;
			player[n]->zs.avail_out=OBUFSIZE;

                        prof=prof_start(12); ret=deflate(&player[n]->zs,Z_SYNC_FLUSH); prof_stop(12,prof);
			if (ret!=Z_OK) {
				elog("compression failure #1, kicking player %d",n);
				kick_player(n,NULL);
				continue;
			}
	
			if (player[n]->zs.avail_in) {
				elog("compression failure #2, kicking player %d",n);
				kick_player(n,NULL);
				continue;
			 }
	
			csize=OBUFSIZE-player[n]->zs.avail_out;
	
			olen=(csize);
			
			if (olen>63) {			
				ohigh=(olen>>8)|0x80;
				olow=olen&255;
	
				csend(n,(void*)(&ohigh),1);
				csend(n,(void*)(&olow),1);
			} else {
				ohigh=olen|(0x40|0x80);
				csend(n,(void*)(&ohigh),1);
			}			
                        csend(n,obuf,csize);			
		} else {
			csize=player[n]->tptr;

			olen=(csize);

			if (olen>63) {			
				ohigh=olen>>8;
				olow=olen&255;
	
				csend(n,(void*)(&ohigh),1);
				csend(n,(void*)(&olow),1);
			} else {
				ohigh=olen|0x40;
				csend(n,(void*)(&ohigh),1);
			}
			if (ilen && player[n]) csend(n,player[n]->tbuf,ilen);
		}

		//xlog("ilen=%d, olen=%d",ilen,olen);
	
		if (player[n]) player[n]->tptr=0;
	}
}

