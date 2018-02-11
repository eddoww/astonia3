/*

$Id: io.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: io.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:16  sam
Added RCS tags


*/

void exit_player(int nr);
int init_io(void);
void io_loop(void);
void psend(int nr,char *buf,int len);
void pflush(void);
void exit_io(void);
