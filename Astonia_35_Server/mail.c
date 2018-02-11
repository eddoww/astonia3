/*

$Id: mail.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: mail.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:22  sam
Added RCS tags


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "mail.h"

int sendmail(char *to,char *subject,char *body,char *from,int do_copy)
{
	int in[2],pid,sock;
	char tmp[256];

	if (socketpair(AF_UNIX,SOCK_STREAM,0,in)) return 0;

	pid=fork();
	if (!pid) {
		close(in[0]);
		dup2(in[1],0);
		dup2(in[1],1);
		dup2(in[1],2);

		sprintf(tmp,"-f%s",from);
		execl("/usr/lib/sendmail","/usr/lib/sendmail",tmp,"-t",NULL);
		exit(1);
	}
        sock=in[0];
	close(in[1]);

        sprintf(tmp,"From: \"Astonia 3 Support\" <%s>\n",from);
        write(sock,tmp,strlen(tmp));

        sprintf(tmp,"To: %s\n",to);
        write(sock,tmp,strlen(tmp));

        if (do_copy) {
		sprintf(tmp,"Bcc: copy@astonia.com\n");
		write(sock,tmp,strlen(tmp));
	}

        sprintf(tmp,"Subject: %s\n",subject);
        write(sock,tmp,strlen(tmp));

	write(sock,"\n",1);
	
        write(sock,body,strlen(body));
	
        write(sock,"\n.\n",3);

	waitpid(pid,NULL,0);

        close(sock);

	return 1;
}
