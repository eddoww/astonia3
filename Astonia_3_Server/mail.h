/*

$Id: mail.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: mail.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:23  sam
Added RCS tags


*/

int sendmail(char *to,char *subject,char *body,char *from,int do_copy);

