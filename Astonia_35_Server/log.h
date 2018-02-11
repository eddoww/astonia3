/*

$Id: log.h,v 1.2 2007/07/13 15:47:37 devel Exp $

$Log: log.h,v $
Revision 1.2  2007/07/13 15:47:37  devel
clog -> charlog

Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:18  sam
Added RCS tags


*/

void ilog(char *format,...) __attribute__ ((format(printf,1,2)));
void elog(char *format,...) __attribute__ ((format(printf,1,2)));
void xlog(char *format,...) __attribute__ ((format(printf,1,2)));
void charlog(int cn,char *format,...) __attribute__ ((format(printf,2,3)));
int init_log(void);
void exit_log(void);
void elog_item(int in);
void reinit_log(void);
void log_items(int cn);
