/*

$Id: chat.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: chat.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:53  sam
Added RCS tags


*/

int init_chat(void);
void tick_chat(void);
int cmd_chat(int cn,char *text);
void list_chat(int cn);
void leave_chat(int cn,int nr);
void join_chat(int cn,int nr);
int tell_chat(int cnID,int coID,int staffmode,char *format,...)  __attribute__ ((format(printf,4,5)));
int server_chat(int channel,char *text);
void npc_chat(int cn,int channel,char *format,...) __attribute__ ((format(printf,3,4)));
