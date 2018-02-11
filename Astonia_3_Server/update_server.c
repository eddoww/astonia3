/*

$Id: update_server.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: update_server.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:13:01  sam
Added RCS tags


*/

#define __USE_BSD_SIGNAL

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>

volatile int quit=0;
volatile int reload=1;

#define min(a,b) ((a)<(b) ? (a) : (b))

#define MAXCHAT		256
#define MAXFILE		256
#define BLOCKSIZE	4096
#define INBUFSIZE	1024
#define OUTBUFSIZE	(BLOCKSIZE+4)

struct chat
{
	int sock;
	char inbuf[INBUFSIZE];
	char obuf[OUTBUFSIZE];
	int iptr,optr,in_len;

	int handle;
};

struct file
{
	char name[16];
	int change;
	int size;
	int sum;
};

struct file *file;
int maxfile=0;
struct chat *chat;

unsigned long long sent_bytes=0;

void sig_leave(int dummy)
{
        quit=1;
}

void sig_reload(int dummy)
{
        reload=1;
}

void kick_chat(int nr)
{
        close(chat[nr].sock);
	chat[nr].sock=0;

	if (chat[nr].handle!=-1) close(chat[nr].handle);

	printf("kicked client %d\n",nr);
}

void send_chat(int nr)
{
        int ret,len;
        char *ptr;

        if (chat[nr].iptr<chat[nr].optr) {
                len=OUTBUFSIZE-chat[nr].optr;
                ptr=chat[nr].obuf+chat[nr].optr;
        } else {
                len=chat[nr].iptr-chat[nr].optr;
                ptr=chat[nr].obuf+chat[nr].optr;
        }

        ret=send(chat[nr].sock,ptr,len,0);
        if (ret==-1) {  // send failure
		printf("send failure on %d\n",nr);
                kick_chat(nr);
                return;
        }

	//printf("sent %d bytes to client %d\n",ret,nr);

        chat[nr].optr+=ret;

        if (chat[nr].optr==OUTBUFSIZE) chat[nr].optr=0;

	sent_bytes+=ret;
}

int csend(int nr,unsigned char *buf,int len)
{
        int size;

        if (chat[nr].sock==0) return -1;

        while (len) {
		if (chat[nr].iptr>=chat[nr].optr) {
			size=OUTBUFSIZE-chat[nr].iptr;
		} else {
			size=chat[nr].optr-chat[nr].iptr;
		}
		size=min(size,len);
		memcpy(chat[nr].obuf+chat[nr].iptr,buf,size);

		chat[nr].iptr+=size; if (chat[nr].iptr==OUTBUFSIZE) chat[nr].iptr=0;
		buf+=size; len-=size;

		if (chat[nr].iptr==chat[nr].optr) {
			printf("send buffer overflow, kicking client %d\n",nr);
                        kick_chat(nr);
                        return -1;
		}
		//xlog("add: iptr=%d, optr=%d, len=%d, size=%d",player[nr]->iptr,player[nr]->optr,len,size);
	}
        return 0;
}

void new_chat(int sock)
{
        int nr,nsock,len=sizeof(struct sockaddr_in),one=1;
        struct sockaddr_in addr;

        nsock=accept(sock,(struct sockaddr *)&addr,&len);
        if (nsock==-1) return;

        ioctl(nsock,FIONBIO,(u_long*)&one);     // non-blocking mode

	for (nr=1; nr<MAXCHAT; nr++) if (!chat[nr].sock) break;
        if (nr==MAXCHAT) { close(nsock); return; }

	bzero(chat+nr,sizeof(struct chat));

        chat[nr].sock=nsock;
	chat[nr].handle=-1;

	printf("accepted new client %d (%d)\n",nr,maxfile);

	csend(nr,(void*)&maxfile,sizeof(maxfile));
        csend(nr,(void*)file,sizeof(file[0])*maxfile);	
}


void rec_chat(int nr)
{
        int len;	

        len=recv(chat[nr].sock,(char*)chat[nr].inbuf+chat[nr].in_len,INBUFSIZE-chat[nr].in_len,0);
	printf("received %d bytes from client %d\n",len,nr);

        if (len<1) {    // receive failure
                //if (errno!=EWOULDBLOCK) {
			printf("receive error on %d\n",nr);
                        kick_chat(nr);
                //}
                return;
        }
        chat[nr].in_len+=len;
}

void send_block(int nr)
{
	char buf[BLOCKSIZE];
	int ret;

        ret=read(chat[nr].handle,buf,BLOCKSIZE);
	if (ret>0) csend(nr,buf,ret);

	if (ret<BLOCKSIZE) {
		close(chat[nr].handle);
		chat[nr].handle=-1;
		printf("finished download file for %d\n",nr);		
	}
	
}

void parse_cmd(int nr)
{
	char buf1[128],buf2[128];

	printf("parse cmd called for %d, len=%d\n",nr,chat[nr].in_len);

	if (chat[nr].in_len>=16) {
		if (strchr(chat[nr].inbuf,'/')) {
			printf("illegal request\n");
			kick_chat(nr);
			return;
		}
		strncpy(buf1,chat[nr].inbuf,16); buf1[15]=0;
		sprintf(buf2,"client/%s",buf1);
		chat[nr].handle=open(buf2,O_RDONLY);

		if (chat[nr].handle==-1) {
			printf("requested file not found (%s)\n",buf2);
			kick_chat(nr);
			return;
		}
		printf("client %d requested file %s.\n",nr,buf2);

                chat[nr].in_len-=16;
		if (chat[nr].in_len) memmove(chat[nr].inbuf,chat[nr].inbuf+16,chat[nr].in_len);

		if (chat[nr].iptr==chat[nr].optr) send_block(nr);
	}

	printf("parse cmd called for %d, len=%d (exit)\n",nr,chat[nr].in_len);
}

void do_reload(void)
{
	DIR *dir;
	struct dirent *de;
	struct stat st;
	char name[128];
	unsigned char buf[512];
	int size,change,nr=0,tsize=0,n,handle,ret;
	unsigned int sum;

	dir=opendir("client");
	if (dir) {
		while ((de=readdir(dir))) {
			if (*de->d_name=='.') continue;
                        sprintf(name,"./client/%s",de->d_name);

			if (stat(name,&st)) continue;

			size=st.st_size; tsize+=size;
			change=st.st_mtime;

			handle=open(name,O_RDONLY);
			if (handle==-1) continue;
			
			sum=0;
                        while (42) {
				ret=read(handle,buf,512);
				if (ret<1) break;
				
				for (n=0; n<ret; n++) {
					sum+=buf[n];
				}
			}
			close(handle);

			printf("found file %s, size %d, change %d, sum %d\n",de->d_name,size,change,sum);

			strcpy(file[nr].name,de->d_name);
			file[nr].size=size;
			file[nr].change=change;
			file[nr].sum=sum;
			nr++;
		}
		closedir(dir);
	}	

	maxfile=nr;
	printf("%d files, %dK\n",nr,tsize>>10);

	// stop all from downloading old data
	for (n=0; n<MAXCHAT; n++) {
		if (chat[n].sock) {
			kick_chat(n);
		}
	}
}

int main(void)
{
	int sock,one=1,fmax,cnt,n,last_sent_check=0,now;
	fd_set in_fd,out_fd;
        struct sockaddr_in addr;
	unsigned long long last_sent_bytes=0,diff;

	// ignore the silly pipe errors and hangups:
        signal(SIGPIPE,SIG_IGN);
        signal(SIGHUP,sig_reload);

        // shutdown gracefully if possible:
        signal(SIGQUIT,sig_leave);
        signal(SIGINT,sig_leave);
        signal(SIGTERM,sig_leave);

	chat=calloc(MAXCHAT,sizeof(struct chat));
	file=calloc(MAXFILE,sizeof(struct file));

        sock=socket(PF_INET,SOCK_STREAM,0);
        if (sock==-1) return 0;

        ioctl(sock,FIONBIO,(u_long*)&one);      // non-blocking mode
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&one,sizeof(int));

        addr.sin_family=AF_INET;
        addr.sin_port=htons(6214);
        addr.sin_addr.s_addr=0;
        if (bind(sock,(struct sockaddr *)&addr,sizeof(addr))) return 0;

        if (listen(sock,50)) return 0;

        while (!quit) {	
		if (reload) {
			do_reload();
			reload=0;
		}
		
		// slow it down to 500KBit/s
		now=time(NULL);
		diff=sent_bytes-last_sent_bytes;
		if (diff>1024*50) { sleep(1); last_sent_bytes=min(sent_bytes,last_sent_bytes+1024*50); }
		else if (last_sent_check<now) {
			last_sent_bytes=sent_bytes;
			last_sent_check=now;
		}

		FD_ZERO(&in_fd); FD_ZERO(&out_fd); fmax=0;

                FD_SET(sock,&in_fd); if (sock>fmax) fmax=sock;

                for (n=1; n<MAXCHAT; n++) {
                        if (chat[n].sock) {
                                if (chat[n].in_len<INBUFSIZE) {
                                        FD_SET(chat[n].sock,&in_fd);
                                        if (chat[n].sock>fmax) fmax=chat[n].sock;
                                }
                                if (chat[n].handle!=-1 && chat[n].iptr==chat[n].optr) send_block(n);
				
				//printf("handle=%d, len=%d\n",chat[n].handle,chat[n].in_len);
                                if (chat[n].handle==-1 && chat[n].in_len) parse_cmd(n);

				if (chat[n].iptr!=chat[n].optr) {
                                        FD_SET(chat[n].sock,&out_fd);
                                        if (chat[n].sock>fmax) fmax=chat[n].sock;
                                }
                        }
                }

                cnt=select(fmax+1,&in_fd,&out_fd,NULL,NULL);

		if (cnt<1) continue;

                if (FD_ISSET(sock,&in_fd)) new_chat(sock);

                for (n=1; n<MAXCHAT; n++) {
                        if (!chat[n].sock) continue;			
                        if (FD_ISSET(chat[n].sock,&in_fd)) rec_chat(n);
			if (FD_ISSET(chat[n].sock,&out_fd)) send_chat(n);
                }
	}

        return 1;
}
