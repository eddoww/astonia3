/*
 * $Id: database.h,v 1.4 2006/12/14 14:29:46 devel Exp $
 *
 * $Log: database.h,v $
 * Revision 1.4  2006/12/14 14:29:46  devel
 * added karma log
 *
 * Revision 1.3  2006/09/22 09:55:46  devel
 * added ip to parameters of login
 *
 */

int init_database(void);
int save_char(int cn,int area);
int release_char(int cn);
void exit_database(void);
void sweep_database(void);
void area_alive(int godown);
int get_area(int ID,int mirror,int *pserver,int *pport);
int change_area(int cn,int area,int x,int y);
int dlog(int cn,int in,char *format,...) __attribute__ ((format(printf,3,4)));
unsigned long long get_mail(int to,int from,char *buf,unsigned long long start);
int send_mail(int to,int from,char *text);
int find_login(char *name,char *password,int *area_ptr,int *cn_ptr,int *mirror_ptr,int *ID_ptr,int vendor,int *punique,unsigned int ip);
void tick_login(void);
int query_ID(unsigned int ID);
int query_name(char *name);
int create_storage(int ID,char *desc,void *content,int size);
int check_create_storage(void);
int update_storage(int ID,int version,void *content,int size);
int check_update_storage(void);
int read_storage(int ID,int version);
int check_read_storage(int *pversion,void **pptr,int *psize);
int add_task(void *task,int len);
int add_note(int uID,int kind,int cID,unsigned char *content,int clen);
int read_notes(int uID,int rID);
void lock_server(void);
void unlock_server(void);
int get_mirror(int ID,int mirror);
int db_unpunish(int ID, void* pm,int pmlen);
void list_queries(int cn);
int add_clanlog(int clan,int serial,int cID,int prio,char *format,...) __attribute__ ((format(printf,5,6)));;
int lookup_clanlog(int cnID,int clan,int serial,int coID,int prio,int from_time,int to_time);
void call_check_task(void);
void call_area_load(void);
int exterminate(int masterID,char *name,char *staffcode);
int do_rename(int masterID,char *from,char *to);
int do_lockname(int masterID,char *from);
int do_unlockname(int masterID,char *from);
int rescue_char(int ID);
int check_area(int ID,int mirror);
void call_stat_update(void);
int lastseen(int uID,int rID);
int db_create_club(int cnr);
void schedule_clubs(void);
void db_update_club(int cnr);
void db_new_pvp(void);
void db_add_pvp(char *killer,char *victim,char *what,int damage);
int karmalog(int rID);








