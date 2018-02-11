/*

$Id: command.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: command.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:59  sam
Added RCS tags


*/

int command(int cn,char *ptr);
void demonspeak(int cn,int nr,char *buf);
int cmdcmp(char *ptr,char *cmd,int minlen);
