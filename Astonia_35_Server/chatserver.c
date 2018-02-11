/*

$Id: chatserver.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: chatserver.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:53  sam
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
#include <ctype.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <errno.h>

volatile int quit=0;

#define MAXCHAT		56
#define INBUFSIZE	1024
#define OUTBUFSIZE	65536

struct chat
{
	int sock;
	char inbuf[INBUFSIZE];
	char obuf[OUTBUFSIZE];
	int iptr,optr,in_len;
};

struct chat *chat;

void sig_leave(int dummy)
{
        quit=1;
}

void kick_chat(int nr)
{
        close(chat[nr].sock);
	chat[nr].sock=0;

	printf("kicked client %d\n",nr);
}

void new_chat(int sock)
{
        int n,nsock,len=sizeof(struct sockaddr_in),one=1;
        struct sockaddr_in addr;

        nsock=accept(sock,(struct sockaddr *)&addr,&len);
        if (nsock==-1) return;

        ioctl(nsock,FIONBIO,(u_long*)&one);     // non-blocking mode

	for (n=1; n<MAXCHAT; n++) if (!chat[n].sock) break;
        if (n==MAXCHAT) { close(nsock); return; }

	bzero(chat+n,sizeof(struct chat));

        chat[n].sock=nsock;
	chat[n].in_len=0;
	chat[n].iptr=0;
	chat[n].optr=0;

	printf("accepted new client %d\n",n);
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
                kick_chat(nr);
                return;
        }

	printf("sent %d bytes to client %d\n",ret,nr);

        chat[nr].optr+=ret;

        if (chat[nr].optr==OUTBUFSIZE) chat[nr].optr=0;
}

int internal_chat(unsigned char *buf,int len)
{
	return 0; // ... !!!!!!!!!
}

int csend(int nr,unsigned char *buf,int len)
{
        int tmp;

        if (chat[nr].sock==0) return -1;

        while (len) {
                tmp=chat[nr].iptr+1;
                if (tmp==OUTBUFSIZE) tmp=0;

                if (tmp==chat[nr].optr) {
                        kick_chat(nr);
                        return -1;
                }

                chat[nr].obuf[chat[nr].iptr]=*buf++;
                chat[nr].iptr=tmp;
                len--;
        }
        return 0;
}

void rec_chat(int nr)
{
        int len,n,channel;

        len=recv(chat[nr].sock,(char*)chat[nr].inbuf+chat[nr].in_len,INBUFSIZE-chat[nr].in_len,0);
	printf("received %d bytes from client %d\n",len,nr);

        if (len<1) {    // receive failure
                //if (errno!=EWOULDBLOCK) {
                        kick_chat(nr);
                //}
                return;
        }
        chat[nr].in_len+=len;

        while (chat[nr].in_len>1) {
		len=*(unsigned short*)(chat[nr].inbuf)+2;

		if (len>1000) {
			kick_chat(nr);
			return;
		}

                if (chat[nr].in_len>=len) {
			channel=*(unsigned short*)(chat[nr].inbuf+2);
			if (channel==1025) {
				internal_chat(chat[nr].inbuf+4,len-4);
			} else {
				for (n=0; n<MAXCHAT; n++)
					csend(n,chat[nr].inbuf,len);
			}

			chat[nr].in_len-=len;
			if (chat[nr].in_len) memmove(chat[nr].inbuf,chat[nr].inbuf+len,chat[nr].in_len);
		} else break;
	}	
}

int main(void)
{
	int sock,one=1,fmax,cnt,n;
	fd_set in_fd,out_fd;
        struct sockaddr_in addr;

	// ignore the silly pipe errors and hangups:
        signal(SIGPIPE,SIG_IGN);
        signal(SIGHUP,SIG_IGN);

        // shutdown gracefully if possible:
        signal(SIGQUIT,sig_leave);
        signal(SIGINT,sig_leave);
        signal(SIGTERM,sig_leave);

	chat=calloc(MAXCHAT,sizeof(struct chat));

        sock=socket(PF_INET,SOCK_STREAM,0);
        if (sock==-1) return 0;

        ioctl(sock,FIONBIO,(u_long*)&one);      // non-blocking mode
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&one,sizeof(int));

        addr.sin_family=AF_INET;
        addr.sin_port=htons(5554);
        addr.sin_addr.s_addr=0;

        if (bind(sock,(struct sockaddr *)&addr,sizeof(addr))) return 0;

        if (listen(sock,50)) return 0;

	while (!quit) {	
		FD_ZERO(&in_fd); FD_ZERO(&out_fd); fmax=0;

                FD_SET(sock,&in_fd);
                if (sock>fmax) fmax=sock;

                for (n=1; n<MAXCHAT; n++) {
                        if (chat[n].sock) {
                                if (chat[n].in_len<INBUFSIZE) {
                                        FD_SET(chat[n].sock,&in_fd);
                                        if (chat[n].sock>fmax) fmax=chat[n].sock;
                                }
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
