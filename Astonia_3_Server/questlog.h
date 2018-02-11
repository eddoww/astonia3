/*
 * $Id: questlog.h,v 1.4 2006/10/04 17:30:57 devel Exp $
 *
 * $Log: questlog.h,v $
 * Revision 1.4  2006/10/04 17:30:57  devel
 * *** empty log message ***
 *
 * Revision 1.3  2006/09/25 14:08:51  devel
 * added questlog to forest and exkordon
 *
 * Revision 1.2  2006/09/21 11:24:04  devel
 * added most aston quests
 *
 */

#define MAXQUEST	100
#define QF_OPEN		1
#define QF_DONE		2

struct quest {
	unsigned char done:6;
	unsigned char flags:2;
};

void questlog_open(int cn,int qnr);
int questlog_done(int cn,int qnr);
void questlog_init(int cn);
void questlog_reopen(int cn,int qnr);
int questlog_isdone(int cn,int qnr);
void destroy_item_byID(int cn,int ID);
int questlog_count(int cn,int qnr);
int questlog_scale(int cnt,int ex);
void questlog_close(int cn,int qnr);



