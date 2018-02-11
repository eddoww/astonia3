/*

$Id: ignore.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: ignore.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:15  sam
Added RCS tags


*/

int ignoring(int cn,int ID);
void ignore(int cn,int ID);
void list_ignore(int cn);
int ignore_cmd(int cn,char *name);
void clearignore_cmd(int cn);
