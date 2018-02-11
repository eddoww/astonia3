/*
 * $Id: database.c,v 1.16 2007/08/13 18:50:38 devel Exp devel $ (C) 2001 D.Brockhaus
 *
 * Database connectivity. Note that after startup, ALL database requests are handled by a background thread.
 * This is to ensure that slow responses from the database wont slow down the main server. But you need to
 * remember never to use any mysql calls from the main server thread.
 *
 * To avoid overwriting more current data, any routine wishing to save character data must use a
 * where current_area=areaID. The only exception is load_char, which uses locking.
 *
 * $Log: database.c,v $
 * Revision 1.16  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.15  2007/05/26 13:20:18  devel
 * increased karma log to 60 items
 *
 * Revision 1.14  2007/05/02 12:30:49  devel
 * *** empty log message ***
 *
 * Revision 1.13  2007/01/31 14:29:30  devel
 * fixed bug in unique constant handling
 *
 * Revision 1.12  2007/01/05 08:54:25  devel
 * added support for locked owner data
 * added too many tries error message support
 *
 * Revision 1.11  2006/12/14 14:28:50  devel
 * added karma log
 *
 * Revision 1.10  2006/12/08 10:34:46  devel
 * fixed bug in iplogging
 *
 * Revision 1.9  2006/10/06 17:26:17  devel
 * fixed bug with ipbanned msg sohown when account unpaid
 *
 * Revision 1.8  2006/09/22 12:12:06  devel
 * added iplog and ipban logic to logins
 *
 * Revision 1.7  2006/09/22 09:55:46  devel
 * added calls to badpass_ip
 *
 * Revision 1.6  2006/08/19 17:27:08  devel
 * changed security setup to new mysql version (4.x)
 *
 * Revision 1.5  2006/06/23 16:19:37  ssim
 * fixed teufel PK bugs
 *
 * Revision 1.4  2006/04/26 16:05:44  ssim
 * unknown change???
 *
 * Revision 1.3  2006/04/07 10:02:06  ssim
 * fixed hard-coded pail_till=X for a new year
 *
 * Revision 1.2  2006/03/30 12:16:01  ssim
 * changed some elogs to xlogs to avoid cluttering the error log file with unimportant errors
 *
 */


#define CHARINFO

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <signal.h>
#include <sys/time.h>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>
#include <mysql/errmsg.h>
#include <pthread.h>
#include <ctype.h>
#include <zlib.h>
#include <math.h>

#include "server.h"
#include "log.h"
#include "create.h"
#include "player.h"
#include "sleep.h"
#include "tool.h"
#include "drdata.h"
#include "drvlib.h"
#include "timer.h"
#include "direction.h"
#include "map.h"
#include "mem.h"
#include "lookup.h"
#include "chat.h"
#include "database.h"
#include "effect.h"
#include "task.h"
#include "punish.h"
#include "skill.h"
#include "talk.h"
#include "sleep.h"
#include "clan.h"
#include "libload.h"
#include "io.h"
#include "mail.h"
#include "player.h"
#include "consistency.h"
#include "btrace.h"
#include "date.h"
#include "club.h"
#include "badip.h"

#define DT_QUERY		1
#define DT_LOAD			2
#define DT_AREA			3
#define DT_CREATE_STORAGE	4
#define DT_UPDATE_STORAGE	5
#define DT_READ_STORAGE		6
#define DT_LOOKUP_ID		7
#define DT_LOOKUP_NAME		8
#define DT_READ_NOTES		9
#define DT_CLANLOG		10
#define DT_EXTERMINATE		11
#define DT_CHECK_TASK		12
#define DT_RENAME		13
#define DT_RESCUE		14
#define DT_STAT_UPDATE		15
#define DT_LASTSEEN		16
#define DT_LOCKNAME		17
#define DT_UNLOCKNAME		18
#define DT_CLUBS		19
#define DT_PVPLIST		20
#define DT_KARMALOG		21

#define MAXAREA		40
#define MAXMIRROR	27

MYSQL mysql;

static void *db_thread(void *);
static int add_query(int type,char *opt1,char *opt2,int nolock);
static void load_char(char *name,char *password);
static void update_arealist(void);
static void db_create_storage(void);
static void db_update_storage(void);
static void db_read_storage(void);
void db_lookup_id(char *tID);
void db_lookup_name(char *name);
void db_read_notes(char *suID,char *srID);
void db_karmalog(char *rID);
void db_lastseen(char *suID,char *srID);
void db_read_clanlog(char *query);
void db_exterminate(char *name,char *master);
void db_rename(char *name,char *master);
void db_lockname(char *name,char *master);
void db_unlockname(char *name,char *master);
void db_rescue_char(char *IDstring);
void db_stat_update(void);
void db_read_clubs(void);
void db_pvplist(char *killer,char *victim);
int isbanned_iplog(int ip);
void add_iplog(int ID,unsigned int ip);

static pthread_t db_tid;
static pthread_mutex_t data_mutex;
static pthread_mutex_t server_mutex;

volatile static int db_thread_quit=0;

unsigned long long db_raw=0;

unsigned int query_cnt=0,query_long=0,query_long_max=0;
unsigned long long query_time=0,query_long_time=0;

unsigned int save_char_cnt=0,save_area_cnt=0,exit_char_cnt=0,save_storage_cnt=0,save_subscriber_cnt=0,save_char_mirror_cnt=0,load_char_cnt=0;

unsigned int query_stat[20];

static char mysqlpass[80];

static void makemysqlpass(void)
{
	static char key1[]={117, 127, 98, 38, 118, 115, 100, 104,0};
	static char key2[]={"qpc74a7v"};
        static char key3[]={"bcoxsa1k"};
	int n;

	for (n=0; key1[n]; n++) {
		mysqlpass[n]=key1[n]^key2[n]^key3[n];
		//printf("%d, ",mysqlpass[n]);
	}
	mysqlpass[n]=0;
	//printf("\n%s\n",mysqlpass);
}

static void destroymysqlpass(void)
{
	bzero(mysqlpass,sizeof(mysqlpass));
}

int mysql_query_con(MYSQL *my,const char *query)
{
	int err,ret,nr;
	unsigned long long start,diff;

	//xlog("size %d, query %.80s",strlen(query),query);
	//if (!demon && areaID==14) { int tmp; tmp=RANDOM(3); if (tmp) sleep(tmp); }

	start=timel();

	while (1) {
                ret=mysql_query(my,query);

		err=mysql_errno(my);

                if (err>=CR_MIN_ERROR && err<=CR_MAX_ERROR) {
			elog("lost connection to server: %s",mysql_error(my));
			mysql_close(my);
			sleep(1);
			makemysqlpass();
			mysql_real_connect(my,"storage.astonia.com","root",mysqlpass,"mysql",0,NULL,0);
			destroymysqlpass();
		} else if (err==ER_NO_SUCH_TABLE) {
			elog("wrong database: %s",mysql_error(my));
			mysql_query(my,"use merc");
		} else break;		
	}

	db_raw+=strlen(query)+120;	// - we need to check for incoming results too!!!

	query_cnt++;
	diff=timel()-start;
	query_time+=diff;
	if (diff>10000000) {
		query_long++;
		query_long_time+=diff;
		if (diff>query_long_max) query_long_max=diff;
	}

	nr=diff/(500*1000ull);
	if (nr>=0 && nr<20) {
		query_stat[nr]++;
	}
	

	return ret;
}

volatile int db_store=0;

MYSQL_RES *mysql_store_result_cnt(MYSQL *my)
{
	db_store++;

	return mysql_store_result(my);
}

void mysql_free_result_cnt(MYSQL_RES *result)
{
	db_store--;

	mysql_free_result(result);
}



static int start_db_thread(void)
{
	pthread_attr_t attr;

	if (pthread_mutex_init(&data_mutex,NULL)) return 0;
	if (pthread_mutex_init(&server_mutex,NULL)) return 0;

	if (pthread_attr_init(&attr)) return 0;
        if (pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE)) return 0;

        if (pthread_create(&db_tid,&attr,db_thread,NULL)) return 0;

	xlog("Created background thread for database accesses");

	return 1;
}

static void wait_db_thread(void)
{
	int exit_status;

	xlog("Waiting for database thread to finish");
	db_thread_quit=1;

	pthread_join(db_tid, (void **) &exit_status);

	xlog("Database thread finished with exit code %d",exit_status);
	
	if (query_cnt==0) query_cnt=1;	// sanity check, should never happen...
        xlog("%d queries, %.2fs (%.2fms/q), %d long queries. Average long query: %.2fms, longest query: %.2fms.",
	     query_cnt,query_time/1000000.0,(double)query_time/query_cnt/1000.0,
	     query_long,
	     query_long ? query_long_time/query_long/1000.0 : 0.0,
	     query_long_max/1000.0);

	xlog("save_char_cnt=%d, save_area_cnt=%d, exit_char_cnt=%d, save_storage_cnt=%d, save_subscriber_cnt=%d, save_char_mirror_cnt=%d, load_char_cnt=%d",
	     save_char_cnt,save_area_cnt,exit_char_cnt,save_storage_cnt,save_subscriber_cnt,save_char_mirror_cnt,load_char_cnt);	
}

/*void *my_mysql_malloc(unsigned int size)
{
	return xmalloc(size,IM_MYSQL);
}

void my_mysql_free(void *ptr)
{
	return xfree(ptr);
}

void *my_mysql_realloc(void *ptr,unsigned int size)
{
	return xrealloc(ptr,size,IM_MYSQL);
}

void mysql_set_malloc_proc(void* (*new_malloc_proc)(size_t));
void mysql_set_free_proc(void (*new_free_proc)(void*));
void mysql_set_realloc_proc(void* (*new_realloc_proc)(void *,size_t));*/


// start connection to database and initialise database if needed
int init_database(void)
{
        char buf[1024];

	//mysql_set_malloc_proc(my_mysql_malloc);
	//mysql_set_free_proc(my_mysql_free);
	//mysql_set_realloc_proc(my_mysql_realloc);

	// init database client
	if (!mysql_init(&mysql)) {
		elog("Failed to initialise database: Error: %s",mysql_error(&mysql));
		return 0;
	}

	// try to login as root with our password
	makemysqlpass();
	if (!mysql_real_connect(&mysql,"localhost","root",mysqlpass,"mysql",0,NULL,0)) {
		destroymysqlpass();
                xlog("Connect to database failed.");
                exit(0);
	} else {
		destroymysqlpass();
		xlog("Login to database as root OK");
	}

        // set default database to merc
	if (mysql_query(&mysql,"use merc")) {
                elog("Failed to select database merc: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
	} else xlog("Using existing database merc");	

        // sweep database, remove all players marked as online here. we're just starting, there shouldnt be any...
	sprintf(buf,"update chars set current_area=0,current_mirror=0,spacer=45 where current_area=%d and current_mirror=%d",areaID,areaM);
	if (mysql_query(&mysql,buf)) {
		elog("Could not sweep database: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return 0;
	}
	xlog("sweep_database: removed %lld characters",mysql_affected_rows(&mysql));

	sprintf(buf,"update area set players=0 where ID=%d and mirror=%d",areaID,areaM);
	if (mysql_query(&mysql,buf)) {
		elog("Could not reset area online count: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return 0;
	}

	// we need an up to date arealist before starting
	update_arealist();
	db_read_clubs();

        if (multi) return start_db_thread();
	else return 1;
}

#define MAXDRDATA	(1024*64)

// save character cn to database
// will mark cn as logged out if area is not zero
int save_char(int cn,int area)
{
	int size,expandto,add;

        unsigned char cdata[sizeof(struct character)],idata[sizeof(struct item)*(INVENTORYSIZE+1)],ddata[MAXDRDATA];
	unsigned char cbuf[sizeof(cdata)*2],ibuf[sizeof(idata)*2],dbuf[sizeof(ddata)*2];
	unsigned char buf[sizeof(cbuf)+sizeof(ibuf)+sizeof(dbuf)+80];
	int n,in,ilen=0,dlen=0;
	struct item *itmp;
	struct data *dat;

	if (ch[cn].ID==0) {
		elog("trying to save char %s with ID=0",ch[cn].name);
		btrace("save_char");
		return 1;
	}

        //xlog("save char %s (%d, ID=%d)",ch[cn].name,cn,ch[cn].ID);

	// character data
	memcpy(cdata,ch+cn,sizeof(struct character));

	// items
	itmp=(void*)(idata);

	for (n=0; n<INVENTORYSIZE; n++) {		
		if ((in=ch[cn].item[n])) {
			*itmp=it[in]; ilen+=sizeof(struct item);
			if (IDR_ISSPELL(itmp->driver)) {	// make drdata contain the remaining duration
				*(signed long*)(itmp->drdata)-=ticker;
				*(signed long*)(itmp->drdata+4)-=ticker;
				//xlog("save: remembering time for spell. time left: %ld ticks",*(unsigned long*)(itmp->drdata));
			}
			itmp++;
		}
	}

	if ((in=ch[cn].citem)) {
		*itmp++=it[in]; ilen+=sizeof(struct item);
	}

	// drdata aka ppd
	for (dat=ch[cn].dat; dat; dat=dat->next) {
		if (!(dat->ID&PERSISTENT_PLAYER_DATA)) continue;

		*(unsigned int*)(ddata+dlen)=dat->ID; dlen+=4;
		*(unsigned int*)(ddata+dlen)=dat->size; dlen+=4;
		memcpy(ddata+dlen,dat->data,dat->size); dlen+=dat->size;

		//xlog("ppd: id=%d, size=%d",(dat->ID&0xffff),dat->size);
	}

	// pad all records in the database to a multiple of 8192 bytes:
	// calculate the size the record will have in the database (mysql sucks soo bad)
	// the three blocks plus the name plus the fixed size stuff
        size=sizeof(struct character)+ilen+dlen+strlen(ch[cn].name)+15*4+1+3*2+1 +6;
	
	// empty INTs are not written:
	if (!ch[cn].karma) size-=4;
	if (!ch[cn].clan) size-=4;
	if (!ch[cn].clan_rank) size-=4;
	if (!ch[cn].clan_serial) size-=4;
	if (!ch[cn].exp) size-=4;

	// ch.current_area and ch.current_mirror are not added if set, the field spacer is used to deal with those

        expandto=((size+8191+9)/8192)*8192;
	add=expandto-size;
	if (add) {
		if (add<9) {
			elog("add<9??");
		} else {
			*(unsigned int*)(ddata+dlen)=DRD_JUNK_PPD; dlen+=4;
			*(unsigned int*)(ddata+dlen)=add-8; dlen+=4;
			memset(ddata+dlen,0,add-8); dlen+=add-8;
		}
	}

	mysql_real_escape_string(&mysql,cbuf,cdata,sizeof(struct character));
	mysql_real_escape_string(&mysql,ibuf,idata,ilen);
	mysql_real_escape_string(&mysql,dbuf,ddata,dlen);

        // note the ...and current_area=areaID and current_mirror=areaM, this makes sure that we will not overwrite
	// the data if the character already left our area (needed because this is a delayed write)
	
	if (!area) {	// just a data backup.
		sprintf(buf,"update chars set chr='%s',item='%s',ppd='%s',class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d,mirror=%d,spacer=0 where ID=%d and current_area=%d and current_mirror=%d",
			cbuf,
			ibuf,
			dbuf,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
			ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].mirror,
			ch[cn].ID,
			areaID,
			areaM);
		save_char_cnt++;
	} else {	// logout
#ifdef CHARINFO
		sprintf(buf,"update charinfo set current_area=0,logout_time=%d,class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d where ID=%d",
                        time_now,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
                        ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].ID);
		if (!add_query(DT_QUERY,buf,"save_charinfo",0)) {
			elog("Failed to update charinfo %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		}
#endif
		sprintf(buf,"update chars set chr='%s',item='%s',ppd='%s',current_area=0,current_mirror=0,allowed_area=%d,logout_time=%d,class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d,mirror=%d,spacer=42 where ID=%d and current_area=%d and current_mirror=%d",
			cbuf,
			ibuf,
			dbuf,
			area,
			time_now,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
                        ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].mirror,
			ch[cn].ID,
			areaID,
			areaM);
		exit_char_cnt++;
	}
	
        if (!add_query(DT_QUERY,buf,"save_char",0)) {
		elog("Failed to update account %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		return 0;
	}
	return 1;
}

#define COMPRESS_MAGIC	0x84736251

int compress_escape_string(MYSQL *my,unsigned char *dst,unsigned char *src,int ilen)
{
	struct z_stream_s zs;
	int ret,olen;
	unsigned char buf[ilen*2];
	void *my_zlib_malloc(void *dummy,unsigned int cnt,unsigned int size);
	void my_zlib_free(void *dummy,void *ptr);

	bzero(&zs,sizeof(zs));
	zs.zalloc=my_zlib_malloc;
	zs.zfree=my_zlib_free;

	deflateInit(&zs,1);
	zs.next_in=src;
	zs.avail_in=ilen;

	zs.next_out=buf+12;
	zs.avail_out=ilen*2-12;

	ret=deflate(&zs,Z_SYNC_FLUSH);
	deflateEnd(&zs);

	if (ret!=Z_OK) {
		elog("compression failure #1, database");
                return 0;
	}

	if (zs.avail_in) {
		elog("compression failure #2, database");
                return 0;
	 }

	olen=(ilen*2-12)-zs.avail_out;
	*(unsigned int*)(buf+0)=COMPRESS_MAGIC;
	*(unsigned int*)(buf+4)=ilen;
	*(unsigned int*)(buf+8)=olen;

        mysql_real_escape_string(my,dst,buf,olen+12);

	xlog("ilen=%d, olen=%d",ilen,olen+12);

	return olen+12;
}

int uncompress_string(unsigned char *dst,unsigned char *src,int maxout)
{
	struct z_stream_s zs;
	int ret,size;
        void *my_zlib_malloc(void *dummy,unsigned int cnt,unsigned int siz);
	void my_zlib_free(void *dummy,void *ptr);

        bzero(&zs,sizeof(zs));
	zs.zalloc=my_zlib_malloc;
	zs.zfree=my_zlib_free;

	inflateInit(&zs);

	zs.next_in=src+12;
	zs.avail_in=*(unsigned int*)(src+8);

	zs.next_out=dst;
	zs.avail_out=maxout;

	ret=inflate(&zs,Z_SYNC_FLUSH);
	inflateEnd(&zs);
	if (ret!=Z_OK) {
		elog("Compression error %d\n",ret);
                return 0;
	}

	if (zs.avail_in) { elog("HELP (%d)\n",zs.avail_in); return 0; }

	size=maxout-zs.avail_out;

	xlog("size=%d/%d",size,maxout);

	return size;
}

// save character cn to database
// will mark cn as logged out if area is not zero
int save_char_new(int cn,int area)
{
	int size,expandto,add;

        unsigned char cdata[sizeof(struct character)],idata[sizeof(struct item)*(INVENTORYSIZE+1)],ddata[MAXDRDATA];
	unsigned char cbuf[sizeof(cdata)*2],ibuf[sizeof(idata)*2],dbuf[sizeof(ddata)*2];
	unsigned char buf[sizeof(cbuf)+sizeof(ibuf)+sizeof(dbuf)+80];
	int n,in,ilen=0,dlen=0,clen=0;
	struct item *itmp;
	struct data *dat;

        //xlog("save char %s (%d, ID=%d)",ch[cn].name,cn,ch[cn].ID);

	// character data
	memcpy(cdata,ch+cn,sizeof(struct character));

	// items
	itmp=(void*)(idata);

	for (n=0; n<INVENTORYSIZE; n++) {		
		if ((in=ch[cn].item[n])) {
			*itmp=it[in]; ilen+=sizeof(struct item);
			if (IDR_ISSPELL(itmp->driver)) {	// make drdata contain the remaining duration
				*(signed long*)(itmp->drdata)-=ticker;
				*(signed long*)(itmp->drdata+4)-=ticker;
				//xlog("save: remembering time for spell. time left: %ld ticks",*(unsigned long*)(itmp->drdata));
			}
			itmp++;
		}
	}

	if ((in=ch[cn].citem)) {
		*itmp++=it[in]; ilen+=sizeof(struct item);
	}

	// drdata aka ppd
	for (dat=ch[cn].dat; dat; dat=dat->next) {
		if (!(dat->ID&PERSISTENT_PLAYER_DATA)) continue;

		*(unsigned int*)(ddata+dlen)=dat->ID; dlen+=4;
		*(unsigned int*)(ddata+dlen)=dat->size; dlen+=4;
		memcpy(ddata+dlen,dat->data,dat->size); dlen+=dat->size;

		//xlog("ppd: id=%d, size=%d",dat->ID,dat->size);
	}

	// pad all records in the database to a multiple of 8192 bytes:
	// calculate the size the record will have in the database (mysql sucks soo bad)
	// the three blocks plus the name plus the fixed size stuff
        size=0;
	
	// empty INTs are not written:
	if (!ch[cn].karma) size-=4;
	if (!ch[cn].clan) size-=4;
	if (!ch[cn].clan_rank) size-=4;
	if (!ch[cn].clan_serial) size-=4;
	if (!ch[cn].exp) size-=4;

	// ch.current_area and ch.current_mirror are not added if set, the field spacer is used to deal with those

        clen=compress_escape_string(&mysql,cbuf,cdata,sizeof(struct character));
	ilen=compress_escape_string(&mysql,ibuf,idata,ilen);
	dlen=compress_escape_string(&mysql,dbuf,ddata,dlen);

	size+=clen+ilen+dlen+strlen(ch[cn].name)+15*4+1+3*2+1+6;

        expandto=((size+8191+9)/8192)*8192;
	add=expandto-size;
	memset(ddata+dlen,0,add);
	dlen+=add;

	xlog("size=%d, expandto=%d",size,expandto);

        // note the ...and current_area=areaID and current_mirror=areaM, this makes sure that we will not overwrite
	// the data if the character already left our area (needed because this is a delayed write)
	
	if (!area) {	// just a data backup.
		sprintf(buf,"update chars set chr='%s',item='%s',ppd='%s',class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d,mirror=%d,spacer=0 where ID=%d and current_area=%d and current_mirror=%d",
			cbuf,
			ibuf,
			dbuf,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
			ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].mirror,
			ch[cn].ID,
			areaID,
			areaM);
		save_char_cnt++;
	} else {	// logout
#ifdef CHARINFO
		sprintf(buf,"update charinfo set current_area=0,logout_time=%d,class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d where ID=%d",
                        time_now,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
                        ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].ID);
		if (!add_query(DT_QUERY,buf,"save_charinfo",0)) {
			elog("Failed to update charinfo %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		}
#endif
		sprintf(buf,"update chars set chr='%s',item='%s',ppd='%s',current_area=0,current_mirror=0,allowed_area=%d,logout_time=%d,class=%u,karma=%d,clan=%d,clan_rank=%d,clan_serial=%d,experience=%d,mirror=%d,spacer=42 where ID=%d and current_area=%d and current_mirror=%d",
			cbuf,
			ibuf,
			dbuf,
			area,
			time_now,
			(unsigned int)(ch[cn].flags&0xffffffff),
			ch[cn].karma,
                        ch[cn].clan,
			ch[cn].clan_rank,
			ch[cn].clan_serial,
			max(ch[cn].exp,ch[cn].exp_used),
			ch[cn].mirror,
			ch[cn].ID,
			areaID,
			areaM);
		exit_char_cnt++;
	}
	
        if (!add_query(DT_QUERY,buf,"save_char",0)) {
		elog("Failed to update account %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		return 0;
	}
	return 1;
}

// mark char cn as not-online without saving any other data
int release_char(int cn)
{
	char buf[256];

	xlog("release char %s (%d, ID=%d)",ch[cn].name,cn,ch[cn].ID);

#ifdef CHARINFO
	sprintf(buf,"update charinfo set current_area=0 where ID=%d",ch[cn].ID);
	if (!add_query(DT_QUERY,buf,"release_charinfo",0)) {
		elog("Failed to release charinfo %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
	}
#endif

        sprintf(buf,"update chars set current_area=0,current_mirror=0,spacer=43 where ID=%d and current_area=%d and current_mirror=%d",ch[cn].ID,areaID,areaM);
	if (!add_query(DT_QUERY,buf,"release_char",0)) {
		elog("Failed to release char %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		return 0;
	}

	return 1;
}

int release_char_nolock(int cn)
{
	char buf[256];

	xlog("release char %s (%d, ID=%d)",ch[cn].name,cn,ch[cn].ID);
#ifdef CHARINFO
	sprintf(buf,"update charinfo set current_area=0 where ID=%d",ch[cn].ID);
	if (!add_query(DT_QUERY,buf,"release_charinfo",1)) {
		elog("Failed to release char %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		return 0;
	}
#endif

        sprintf(buf,"update chars set current_area=0,current_mirror=0,spacer=43 where ID=%d and current_area=%d and current_mirror=%d",ch[cn].ID,areaID,areaM);
	if (!add_query(DT_QUERY,buf,"release_char",1)) {
		elog("Failed to release char %s (ID=%d) on logout",ch[cn].name,ch[cn].ID);
		return 0;
	}

	return 1;
}

void exit_database(void)
{
        if (multi) wait_db_thread();	// wait for background thread to write back data still in buffers
	else tick_login();		// write back data still in buffers

	mysql_close(&mysql);
}

void area_alive(int godown)
{
	char buf[256];
	static int last_time=0;
	static unsigned long long last_transfer=0;
	unsigned long long diff;
	int bps=0,tdiff;
	extern int mmem_usage,last_error;

	if (last_time) {
		diff=sent_bytes_raw+rec_bytes_raw-last_transfer;
		tdiff=time_now-last_time;
		if (tdiff) bps=diff/tdiff;
	}
	last_time=time_now;
	last_transfer=sent_bytes_raw+rec_bytes_raw;

        if (!godown) {
		sprintf(buf,"update area set alive_time=%d,idle=%d,players=%d,server=%u,port=%d,bps=%d,mem_usage=%d,last_error=%d where ID=%d and mirror=%d",
			time_now,
			server_idle,
			online,
                        server_addr,
			server_port,
			bps,
			mmem_usage,
			last_error,
			areaID,
			areaM);
	} else {
		sprintf(buf,"update area set alive_time=0,idle=0,players=0,server=0,port=0,bps=0,mem_usage=0,last_error=0 where ID=%d and mirror=%d",
                        areaID,
			areaM);
	}

	if (!add_query(DT_QUERY,buf,"area_alive",0)) {
		elog("Could not add query to update area_alive");
	}
	save_area_cnt++;
}

void call_area_load(void)
{
	if (!add_query(DT_AREA,NULL,"area load",0)) {
		elog("Could note add query to load area");
	}
}

void call_check_task(void)
{
	if (!add_query(DT_CHECK_TASK,NULL,"check task",0)) {
		elog("Could note add query to check task");
	}
}

void call_stat_update(void)
{
	if (!add_query(DT_STAT_UPDATE,NULL,"stat update",0)) {
		elog("Could note add query for stat update");
	}
}

int add_task(void *task,int len)
{
	char buf[len*4+256];
	char data[len*4];

	mysql_real_escape_string(&mysql,data,task,len);

	sprintf(buf,"insert into task values(0,'%s')",data);

	if (!add_query(DT_QUERY,buf,"add_task",0)) {
		elog("Failed to add task");
		return 0;
	}
	return 1;
}

int change_area(int cn,int area,int x,int y)
{
        int server,port,nr;

	if (!get_area(area,ch[cn].mirror,&server,&port)) return 0;

        ch[cn].tmpx=x;
	ch[cn].tmpy=y;
	ch[cn].tmpa=area;
	ch[cn].flags|=CF_AREACHANGE;

	kick_char(cn);

        nr=ch[cn].player;

	if (nr) player_to_server(nr,server,port);

	//xlog("sending %s (%d,%d) to area %d",ch[cn].name,cn,nr,area);

        return 1;
}

int dlog(int cn,int in,char *format,...)
{
        char tbuf[1024],pbuf[80],xbuf[1024]={""},ibuf[1024]={""};
        va_list args;
	unsigned long long prof;
	int nr,addr;

	prof=prof_start(25);

        va_start(args,format);
        vsnprintf(tbuf,1020,format,args);
        va_end(args);

        if (cn) {
                if ((ch[cn].flags&CF_PLAYER) && (nr=ch[cn].player) && (addr=get_player_addr(nr))) sprintf(pbuf,",IP=%u.%u.%u.%u",(addr>>0)&255,(addr>>8)&255,(addr>>16)&255,(addr>>24)&255);
		else pbuf[0]=0;
		sprintf(ibuf," [name=%s,level=%d,saves=%d,hp=%d,end=%d,mana=%d,exp=%d,gold=%d,pos=%d,%d%s]",
			ch[cn].name,
			ch[cn].level,
			ch[cn].saves,
			ch[cn].hp/POWERSCALE,
			ch[cn].endurance/POWERSCALE,
			ch[cn].mana/POWERSCALE,
			ch[cn].exp,
			ch[cn].gold,
			ch[cn].x,
			ch[cn].y,
			pbuf);
	}
	if (in) {
		sprintf(xbuf," [name=%s, desc=%s, value=%d, sprite=%d, driver=%d, ID=%X, mods=(%d:%d, %d:%d, %d:%d, %d:%d, %d:%d)]",
			it[in].name,
			it[in].description,
			it[in].value,
			it[in].sprite,
			it[in].driver,
			it[in].ID,
			it[in].mod_index[0],it[in].mod_value[0],
			it[in].mod_index[1],it[in].mod_value[1],
			it[in].mod_index[2],it[in].mod_value[2],
			it[in].mod_index[3],it[in].mod_value[3],
			it[in].mod_index[4],it[in].mod_value[4]);
	}
	ilog("ID=%d: %s%s%s",cn ? ch[cn].ID : 0,tbuf,ibuf,xbuf);

        prof_stop(25,prof);

	return 1;
}

int add_note(int uID,int kind,int cID,unsigned char *content,int clen)
{
	char data[clen*2],buf[clen*2+256];

	mysql_real_escape_string(&mysql,data,(char*)content,clen);
	sprintf(buf,"insert into notes values(0,%d,%d,%d,%d,'%s')",uID,kind,cID,time_now,data);
	
	return add_query(DT_QUERY,buf,"add_note",0);
}

int add_clanlog(int clan,int serial,int cID,int prio,char *format,...)
{
	char buf[256],ebuf[512],qbuf[1024];
	va_list args;
	
	va_start(args,format);
        vsnprintf(buf,250,format,args);
        va_end(args);

	mysql_real_escape_string(&mysql,ebuf,buf,strlen(buf));

	sprintf(qbuf,"insert into clanlog values(0,%d,%d,%d,%d,%d,'%s')",
		time_now,
		clan,
		serial,
		cID,
		prio,
		ebuf);
	return add_query(DT_QUERY,qbuf,"add_clanlog",0);
}
//select 278527,clan,serial,time,content from clanlog where time>=1055577172 and time<=1059173572 and clan=14 and serial=15 and cID=104665
int lookup_clanlog(int cnID,int clan,int serial,int coID,int prio,int from_time,int to_time)
{
	char buf[512];//,len;
	int len;

        len=sprintf(buf,"select %d,clan,serial,time,content from clanlog where time>=%d and time<=%d",cnID,from_time,to_time);
	if (clan) len+=sprintf(buf+len," and clan=%d",clan);
	if (serial) len+=sprintf(buf+len," and serial=%d",serial);
	if (coID) len+=sprintf(buf+len," and cID=%d",coID);
	if (prio) len+=sprintf(buf+len," and prio<=%d",prio);
	len+=sprintf(buf+len," order by time limit 51");

        return add_query(DT_CLANLOG,buf,"lookup_clanlog",0);
}

int exterminate(int masterID,char *name,char *staffcode)
{
	char buf[80];
	sprintf(buf,"%010d%s",masterID,staffcode);

        return add_query(DT_EXTERMINATE,name,buf,0);
}

int rescue_char(int ID)
{
	char buf[80];
	
	sprintf(buf,"%d",ID);

        return add_query(DT_RESCUE,buf,"rescue char",0);
}

int do_rename(int masterID,char *from,char *to)
{
	char buf[256];
	
	sprintf(buf,"%10d:%s",masterID,to);

        return add_query(DT_RENAME,from,buf,0);
}

int do_lockname(int masterID,char *from)
{
	char buf[256];
	
	sprintf(buf,"%10d",masterID);

        return add_query(DT_LOCKNAME,from,buf,0);
}
int do_unlockname(int masterID,char *from)
{
	char buf[256];
	
	sprintf(buf,"%10d",masterID);

        return add_query(DT_UNLOCKNAME,from,buf,0);
}

void lock_server(void)
{
	pthread_mutex_lock(&server_mutex);
}

void unlock_server(void)
{
	pthread_mutex_unlock(&server_mutex);
}

#define CSS_EMPTY	0
#define CSS_CREATE	1
#define CSS_DONE	2
#define CSS_FAILED	3

struct create_storage
{
	int state;
	int ID;

        char *desc;
	void *content;
	int size;
};

static struct create_storage cs;

int create_storage(int ID,char *desc,void *content,int size)
{
	if (size>60000) {
		elog("create_storage() cannot handle more than 60000 byte objects");
		return 0;
	}
        pthread_mutex_lock(&data_mutex);
	if (cs.state!=CSS_EMPTY) { pthread_mutex_unlock(&data_mutex); return 0; }
	
	cs.state=CSS_CREATE;
	cs.ID=ID;
	cs.content=content;
	cs.size=size;
	cs.desc=desc;
	pthread_mutex_unlock(&data_mutex);

	return add_query(DT_CREATE_STORAGE,NULL,"create storage",0);
}

int check_create_storage(void)
{
	int nr=0;

	pthread_mutex_lock(&data_mutex);
	if (cs.state==CSS_DONE) {
		cs.state=CSS_EMPTY;
		nr=1;
	}
	if (cs.state==CSS_FAILED) {
		cs.state=CSS_EMPTY;
		nr=-1;
	}
	pthread_mutex_unlock(&data_mutex);
	
	return nr;
}

#define USS_EMPTY	0
#define USS_CREATE	1
#define USS_DONE	2
#define USS_FAILED	3

struct update_storage
{
	int state;
	int ID;

        int version;
	void *content;
	int size;
};

static struct update_storage us;

int update_storage(int ID,int version,void *content,int size)
{
	if (size>60000) {
		elog("update_storage() cannot handle more than 60000 byte objects");
		return 0;
	}
        pthread_mutex_lock(&data_mutex);
	if (us.state!=USS_EMPTY) { pthread_mutex_unlock(&data_mutex); return 0; }
	
	us.state=USS_CREATE;
	us.ID=ID;
	us.version=version;
	us.content=content;
	us.size=size;

	pthread_mutex_unlock(&data_mutex);

	return add_query(DT_UPDATE_STORAGE,NULL,"update storage",0);
}

int check_update_storage(void)
{
	int nr=0;

	pthread_mutex_lock(&data_mutex);
	if (us.state==USS_DONE) {
		us.state=USS_EMPTY;
		nr=1;
	}
	if (us.state==USS_FAILED) {
		us.state=USS_EMPTY;
		nr=-1;
	}
	pthread_mutex_unlock(&data_mutex);
	
	return nr;
}

#define RSS_EMPTY	0
#define RSS_CREATE	1
#define RSS_DONE	2
#define RSS_FAILED	3

struct read_storage
{
	int state;
	int ID;
	
	int version;
	void *content;
	int size;
};

struct read_storage rs;

int read_storage(int ID,int version)
{
	pthread_mutex_lock(&data_mutex);
	if (rs.state!=RSS_EMPTY) { pthread_mutex_unlock(&data_mutex); return 0; }
	
	rs.state=RSS_CREATE;
	rs.ID=ID;
	rs.version=version;

	pthread_mutex_unlock(&data_mutex);

	return add_query(DT_READ_STORAGE,NULL,"read storage",0);
}

int check_read_storage(int *pversion,void **pptr,int *psize)
{
	int nr=0;

	pthread_mutex_lock(&data_mutex);
	if (rs.state==RSS_DONE) {
		rs.state=RSS_EMPTY;
		nr=1;
		if (pversion) *pversion=rs.version;
                if (pptr) *pptr=rs.content;
		if (psize) *psize=rs.size;
	}
	if (rs.state==RSS_FAILED) {
		rs.state=RSS_EMPTY;
		nr=-1;
	}
	pthread_mutex_unlock(&data_mutex);
	
	return nr;
}

int query_name(char *name)
{
	return add_query(DT_LOOKUP_NAME,name,"lookup name",0);
}

int query_ID(unsigned int ID)
{
	char buf[80];
	
	sprintf(buf,"%u",ID);
	return add_query(DT_LOOKUP_ID,buf,"lookup ID",0);
}

int read_notes(int uID,int rID)
{
	char op1[80],op2[80];
	
	sprintf(op1,"%d",uID);
	sprintf(op2,"%d",rID);
	
	return add_query(DT_READ_NOTES,op1,op2,0);
}

int karmalog(int rID)
{
	char op1[80];
	
	sprintf(op1,"%d",rID);

	return add_query(DT_KARMALOG,op1,"karmalog",0);
}

int lastseen(int uID,int rID)
{
	char op1[80],op2[80];
	
	sprintf(op1,"%d",uID);
	sprintf(op2,"%d",rID);
	
	return add_query(DT_LASTSEEN,op1,op2,0);
}

// -------------- database thread for background database access --------------------

struct query
{
	int type;

	char *opt1;
	char *opt2;

	struct query *next;

	int nr;
};

static int running_query_nr=1;
int used_queries=0;

static struct query
	*fquery=NULL,	// free queries
	*wquery=NULL,	// top of used queries
	*equery=NULL;	// end of used queries

static int add_query(int type,char *opt1,char *opt2,int nolock)
{
	struct query *q;

	if (!nolock) pthread_mutex_lock(&data_mutex);

	q=fquery;
	if (!q) q=xmalloc(sizeof(struct query),IM_QUERY);
	else fquery=q->next;

	if (!q) { elog("memory low in add_query!"); exit(1); } // memory low!!! handle me !!!

	q->type=type;
	if (opt1) q->opt1=xstrdup(opt1,IM_QUERY); else q->opt1=NULL;
	if (opt2) q->opt2=xstrdup(opt2,IM_QUERY); else q->opt2=NULL;

	if ((opt1 && !q->opt1) || (opt2 && !q->opt2)) { elog("memory low in add_query!"); exit(1); } // memory low!!! handle me !!!

	// add to end of list, update equery and wquery
	if (equery) equery->next=q;
	else wquery=q;
	
	equery=q;
	q->next=NULL;

	q->nr=running_query_nr++;
	used_queries++;

	if (!nolock) pthread_mutex_unlock(&data_mutex);

        return 1;
}

static int remove_top_query(void)
{
	struct query *q;

	// remove from work list
	q=wquery;
	wquery=q->next;

	if (!wquery) equery=NULL;

	// add to free list
	q->next=fquery;
	fquery=q;
	used_queries--;

	return 1;
}
void check_task(void);

void list_queries(int cn)
{
	struct query *q;
	int n;
	
	pthread_mutex_lock(&data_mutex);
	for (q=wquery; q; q=q->next) {
		if (q->type!=DT_LASTSEEN && q->type!=DT_READ_NOTES && q->type!=DT_LOAD && q->type!=DT_EXTERMINATE && q->type!=DT_RENAME && q->type!=DT_LOCKNAME && q->type!=DT_UNLOCKNAME) log_char(cn,LOG_SYSTEM,0,"%d: query: %s",q->nr,q->opt2);
		if (q->type==DT_READ_NOTES) log_char(cn,LOG_SYSTEM,0,"%d: query: read notes",q->nr);
		if (q->type==DT_LASTSEEN) log_char(cn,LOG_SYSTEM,0,"%d: query: last seen",q->nr);
		if (q->type==DT_LOAD) log_char(cn,LOG_SYSTEM,0,"%d: query: load char",q->nr);
		if (q->type==DT_EXTERMINATE) log_char(cn,LOG_SYSTEM,0,"%d: query: exterminate %s",q->nr,q->opt1);
		if (q->type==DT_RENAME) log_char(cn,LOG_SYSTEM,0,"%d: query: rename %s %s",q->nr,q->opt1,q->opt2);
		if (q->type==DT_LOCKNAME) log_char(cn,LOG_SYSTEM,0,"%d: query: lock name %s",q->nr,q->opt1);
		if (q->type==DT_UNLOCKNAME) log_char(cn,LOG_SYSTEM,0,"%d: query: unlock name %s",q->nr,q->opt1);
	}
	pthread_mutex_unlock(&data_mutex);

	if (query_cnt==0) query_cnt=1;	// sanity check, should never happen...
	log_char(cn,LOG_SYSTEM,0,"%d queries, %.2fs (%.2fms/q), %d long queries. Average long query: %.2fms, longest query: %.2fms.",
	     query_cnt,query_time/1000000.0,(double)query_time/query_cnt/1000.0,
	     query_long,
	     query_long ? query_long_time/query_long/1000.0 : 0.0,
	     query_long_max/1000.0);
	log_char(cn,LOG_SYSTEM,0,"save_char_cnt=%d, save_area_cnt=%d, exit_char_cnt=%d, save_storage_cnt=%d, save_subscriber_cnt=%d, save_char_mirror_cnt=%d, load_char_cnt=%d",
	     save_char_cnt,save_area_cnt,exit_char_cnt,save_storage_cnt,save_subscriber_cnt,save_char_mirror_cnt,load_char_cnt);

	for (n=0; n<20; n++) {
		log_char(cn,LOG_SYSTEM,0,"queries <%.4fs: %d",(n+1)*500/1000.0,query_stat[n]);
	}
}

static void db_thread_sub(void)
{
	int type;
	char *opt1,*opt2;

        // get lock
	pthread_mutex_lock(&data_mutex);
	if (wquery) {

		// get strings to work on
		type=wquery->type;
		opt1=wquery->opt1;
		opt2=wquery->opt2;

		// free query
		remove_top_query();

		// free lock
		pthread_mutex_unlock(&data_mutex);

                switch(type) {
			case DT_QUERY:		if (mysql_query_con(&mysql,opt1)) {
							elog("Failed to create %s entry query=\"%60.60s\": Error: %s (%d)",opt2,opt1,mysql_error(&mysql),mysql_errno(&mysql));
						}
						if (mysql_affected_rows(&mysql)==0) elog("(%60.60s) affected %llu rows",opt1,mysql_affected_rows(&mysql));
						break;
			case DT_LOAD:		load_char(opt1,opt2);
						break;
			case DT_AREA:		update_arealist();
						break;
			case DT_CREATE_STORAGE:	db_create_storage();
						break;
			case DT_UPDATE_STORAGE:	db_update_storage();
						break;
			case DT_READ_STORAGE:	db_read_storage();
						break;
			case DT_LOOKUP_ID:	db_lookup_id(opt1);
						break;
			case DT_LOOKUP_NAME:	db_lookup_name(opt1);
						break;
			case DT_READ_NOTES:	db_read_notes(opt1,opt2);
						break;
			case DT_KARMALOG:	db_karmalog(opt1);
						break;
			case DT_CLANLOG:	db_read_clanlog(opt1);
						break;
			case DT_EXTERMINATE:	db_exterminate(opt1,opt2);
						break;
			case DT_CHECK_TASK:	check_task();
						break;
			case DT_RENAME:		db_rename(opt1,opt2);
						break;
			case DT_LOCKNAME:	db_lockname(opt1,opt2);
						break;
			case DT_UNLOCKNAME:	db_unlockname(opt1,opt2);
						break;
			case DT_RESCUE:		db_rescue_char(opt1);
						break;
			case DT_STAT_UPDATE:	db_stat_update();
						break;
			case DT_LASTSEEN:	db_lastseen(opt1,opt2);
						break;
			case DT_CLUBS:		db_read_clubs();
						break;
			case DT_PVPLIST:	db_pvplist(opt1,opt2);
						break;
		}

		// free the strings
		if (opt1) xfree(opt1);
		if (opt2) xfree(opt2);

	} else pthread_mutex_unlock(&data_mutex);
}

void *db_thread(void *dummy)
{
        sigset_t set;
        struct timeval tv;

	// block ALL signals besides programming errors, main thread shall get them!
        sigfillset(&set);
        sigdelset(&set,SIGSEGV);
	sigdelset(&set,SIGFPE);
	sigdelset(&set,SIGBUS);
        sigdelset(&set,SIGSTKFLT);
        pthread_sigmask(SIG_BLOCK,&set,NULL);

        while (42) {
		if (wquery) {	// work to be done?
			db_thread_sub();
		} else {
			if (db_thread_quit) break;
			
			tv.tv_sec=0;
			tv.tv_usec=10000;
			select(0,NULL,NULL,NULL,&tv);	// sleep for 1/100 of a second. wish i had yield(). or semaphores.
		}
	}
	xlog("DB thread exiting");

        return (void*)(0);
}

// ----------- character login -----------
// three important parts in two threads, passing data through the login structure:
// - find_login initiates the action and finishes it. it is called from the player login
//   routines.
// - load_char is called from the database thread and loads the character data from the
//   database, checks for validity of name/password and will note if the character is
//   supposed to be on a different area
// - tick_login is called from the main server loop and will take a character loaded
//   by load_char and put it into the game
// last step is then find_login again, which is called repeatedly by the player login
// routines to check if the character is ready. it will add a player to the character.
//
// note that we process only one login-request at once. this is to avoid race conditions
// with the same character trying to login from different computers at once. some thought
// would be needed to make this work in parallel.

#define LS_EMPTY	0	// no query in progress
#define LS_READ		1	// waiting for database read to finish
#define LS_CREATE	2	// waiting for character create/usurp to finish
#define LS_OK		3	// character created and ready for takeoff
#define LS_NEWAREA	4	// send player to another area
#define LS_FAILED	5	// error, send player away
#define LS_LOCKED	6	// error, send player away
#define LS_PASSWD	7	// error, send player away
#define LS_DUP		8	// error, send player away
#define LS_NOPAY	9	// error, send player away
#define LS_SHUTDOWN	10	// error, send player away
#define LS_IPLOCKED	11	// error, send player away
#define LS_NOTFIXED	12	// error, send player away
#define LS_TOOMANY	13	// error, send player away

struct login {
	int status;
	
	int didusurp;
	
	int age;
	
	// in
	char name[sizeof(ch->name)];
	char password[MAXPASSWORD];
	int vendor;
	int unique;

	// working data
	int ID;
	int current;		// DB: current area of char
	unsigned char *chr;	// DB: character data
	int chr_len;
	unsigned char *itm;	// DB: item data
	int itm_len;
	unsigned char *ppd;	// DB: persistent player data
	int ppd_len;
	int paid_till;
	int paid;
	int ip;
	
	// out
	int cn,new_area,mirror;
};

static struct login login={LS_EMPTY};

int find_login(char *name,char *password,int *area_ptr,int *cn_ptr,int *mirror_ptr,int *ID_ptr,int vendor,int *punique,unsigned int ip)
{	
	char *ptr;

	// spaces and punctuation in text keys confuse mysql. sucks.
	for (ptr=name; *ptr; ptr++) {
		if (!isalpha(*ptr)) return -3;		
	}

        //xlog("find login called: %s %s",name,password);
	//xlog("have %d %s %s",login.status,login.name,login.password);
        pthread_mutex_lock(&data_mutex);

	// remove stale login data if it wasnt collected for two ticks
	if (login.status>LS_CREATE && ticker>login.age+2) {
		//xlog("removed stale login from %s %s (status=%d)",login.name,login.password,login.status);
		if (login.status==LS_OK) {	// character has been created but client did not pick it up - we must destroy it.
			if (!login.didusurp) {
				release_char_nolock(login.cn);
				remove_destroy_char(login.cn);
			} else {
				ch[login.cn].driver=CDR_LOSTCON;
			}
		}
		login.status=LS_EMPTY;
	}

	if (login.status==LS_EMPTY) {		// no login waiting? add this one
		bzero(&login,sizeof(login));
                login.status=LS_READ;
		login.age=ticker;
		login.vendor=vendor;
		if (punique) login.unique=*punique; else login.unique=1;
		strcpy(login.name,name);
		strcpy(login.password,password);
		login.ip=ip;
		//xlog("set login.ip=%u",login.ip);
                pthread_mutex_unlock(&data_mutex);

		add_query(DT_LOAD,name,password,0);	// send query to database
                return 0;
	}

	if (strcasecmp(login.name,name)) {	// not our login? leave and try again later
		pthread_mutex_unlock(&data_mutex);
                return 0;
	}

	if (login.status==LS_FAILED) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -1;
	}

	if (login.status==LS_LOCKED) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -2;
	}

	if (login.status==LS_PASSWD) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -3;
	}

	if (login.status==LS_DUP) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -4;
	}
        if (login.status==LS_NOPAY) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -5;
	}
	if (login.status==LS_SHUTDOWN) {	// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -6;
	}

	if (login.status==LS_IPLOCKED) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -7;
	}
	if (login.status==LS_NOTFIXED) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -8;
	}

        if (login.status==LS_TOOMANY) {		// login failed. send him away and mark login as free
		login.status=LS_EMPTY;
		pthread_mutex_unlock(&data_mutex);
                return -9;
	}

	if (login.status==LS_NEWAREA) {		// he's supposed to be on a different area. sent him there (ugly) and mark login as free
		login.status=LS_EMPTY;
		if (area_ptr) *area_ptr=login.new_area;
                if (mirror_ptr) *mirror_ptr=login.mirror;
		if (cn_ptr) *cn_ptr=0;
		if (ID_ptr) *ID_ptr=login.ID;
		if (punique) *punique=login.unique;
                pthread_mutex_unlock(&data_mutex);
                return 1;
	}

	if (login.status==LS_OK) {		// ok, we got him. return character and free login
		login.status=LS_EMPTY;
                if (area_ptr) *area_ptr=0;
		if (mirror_ptr) *mirror_ptr=0;
		if (cn_ptr) *cn_ptr=login.cn;
		if (ID_ptr) *ID_ptr=login.ID;
		if (punique) *punique=login.unique;
		pthread_mutex_unlock(&data_mutex);
                return 1;
	}
	
	login.age=ticker;

	// working status, return busy
        pthread_mutex_unlock(&data_mutex);	
        return 0;
}

static void login_ok(int cn,int didusurp)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_OK;
	login.cn=cn;
	login.age=ticker;
	login.didusurp=didusurp;
	pthread_mutex_unlock(&data_mutex);
}

static void login_failed(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_FAILED;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_locked(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_LOCKED;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_iplocked(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_IPLOCKED;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_notfixed(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_NOTFIXED;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_passwd(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_PASSWD;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_toomany(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_TOOMANY;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_nopay(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_NOPAY;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_dup(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_DUP;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_shutdown(void)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_SHUTDOWN;
	login.age=ticker;
	pthread_mutex_unlock(&data_mutex);
}

static void login_newarea(int area,int mirror)
{
	pthread_mutex_lock(&data_mutex);
	login.status=LS_NEWAREA;
	login.age=ticker;
	login.new_area=area;
	login.mirror=mirror;
	pthread_mutex_unlock(&data_mutex);
}

void check_prof_max(int cn)
{
	int cnt,n;

	cnt=ch[cn].value[1][V_PROFESSION];

	for (n=0; n<P_MAX; n++) {
		if (ch[cn].prof[n]>cnt) {
			ch[cn].prof[n]=cnt;
			cnt=0;
		} else cnt-=ch[cn].prof[n];
	}
}

void tick_login(void)
{
	int cn,n,in,pos,ppd_id,ppd_size,newbie=0;
	unsigned char *ppd;
	struct item *itmp;
        unsigned long long flags;
	char buf[256];

        // do background thread work if running single-threaded
	while (!multi && wquery) {
		unsigned long long prof;

		prof=prof_start(37);
		unlock_server();
		db_thread_sub();
		lock_server();
		prof_stop(37,prof);
	}

	// make sure input data is ready
	pthread_mutex_lock(&data_mutex);
	if (login.status!=LS_CREATE) {	// no work
		pthread_mutex_unlock(&data_mutex);
                return;
	}
	pthread_mutex_unlock(&data_mutex);

	if (login.current) {	
		// he's supposed to be online here. find the corresponding character
		for (n=1; n<MAXCHARS; n++) {
			if (ch[n].flags && ch[n].ID==login.ID) {	// and return it
				if (ch[n].flags&CF_PLAYER) {
					//xlog("usurping existing character %s (%d)",ch[n].name,n);
					xfree(login.chr); xfree(login.itm); xfree(login.ppd);
					ch[n].flags|=CF_AREACHANGE;
					
					if (ch[n].player) {
						exit_player(ch[n].player);
						ch[n].player=0;
					}

                                        ch[n].driver=0;	// disable lostcon
					login_ok(n,1);
					return;
				} else elog("character %s (%d) has ID %d but isn't a player",ch[n].name,n,ch[n].ID);
			}
		}		
		// the character was online when we read the values from the database, but isn't online anymore
		// that means the database values we read are invalid, and the player needs to try again, hence login_failed
		login_failed();	
		return;
	}

        // is it a new account?
	flags=*(unsigned long long*)(login.chr);

	//xlog("tick_login(): creating new character for %s",login.name);

	if (!(flags&CF_USED)) {		// character marked as unused, new account
		if (flags&CF_WARRIOR) {
			if (flags&CF_MALE) cn=create_char("new_warrior_m",0);
			else cn=create_char("new_warrior_f",0);
		} else {
			if (flags&CF_MALE) cn=create_char("new_mage_m",0);
			else cn=create_char("new_mage_f",0);
		}
		if (!cn) {
			elog("tick_login: create newbie failed");
			xfree(login.chr); xfree(login.itm); xfree(login.ppd);
			login_failed();
			return;
		}
		
		ch[cn].flags|=flags;
		
                ch[cn].tmpa=ch[cn].resta=1;
		ch[cn].tmpx=ch[cn].restx=126;
		ch[cn].tmpy=ch[cn].resty=179;

		ch[cn].channel|=(1|128|256);	// make them join channel info, area and mirror

		newbie=1;

		sprintf(buf,"0000000000°c17%s°c18, a new player, has entered the game.",login.name);
		server_chat(1,buf);

	} else {						// existing account, retrieve items
		cn=alloc_char();
		if (!cn) {
			elog("alloc_char returned error");
			xfree(login.chr); xfree(login.itm); xfree(login.ppd);
			login_failed();
			return;
		}
		
		// character data
		copy_char(ch+cn,(void*)login.chr);
		ch[cn].serial=sercn++;
		ch[cn].ef[0]=ch[cn].ef[1]=ch[cn].ef[2]=ch[cn].ef[3]=0;

		// items
		itmp=(void*)(login.itm);

		for (n=0; n<INVENTORYSIZE; n++) {
                        if (ch[cn].item[n]) {
				if (IDR_DONTSAVE(itmp->driver) || (itmp->flags&IF_LABITEM)) {
					itmp++;
					ch[cn].item[n]=0;
					continue;
				}
				in=alloc_item();
				if (!in) {
					// should destroy character and items !!!
					elog("tick_login(): could not alloc item");
					xfree(login.chr); xfree(login.itm); xfree(login.ppd);
					login_failed();
					return;
				}	

				it[in]=*itmp++;
				it[in].carried=cn;
				it[in].x=it[in].y=it[in].contained=0;
				it[in].serial=serin++;

                                if (!(it[in].flags)) {
                                        elog("trying to load already free item %s for %s",it[in].name,ch[cn].name);
                                        it[in].flags=IF_USED;
                                        free_item(in);
                                        ch[cn].item[n]=0;
                                        continue;
                                }
                                if (IDR_ISSPELL(it[in].driver)) {
                                        *(signed long*)(it[in].drdata)+=ticker;
					*(signed long*)(it[in].drdata+4)+=ticker;
					create_spell_timer(cn,in,n);					
				} else {
					if (it[in].driver) {
                                                call_item(it[in].driver,in,0,ticker+1);
					}
				}
				
				/*// !!! hack to change sprites of legacy items !!!
				if (it[in].sprite>64000) {
					if (it[in].sprite>=100000) {
						it[in].sprite=it[in].sprite-100000+50000;
					} else xlog("strange item sprite found for %s: %s %d",ch[cn].name,it[in].name,it[in].sprite);
				}

				// !!! hack to remove max_damage and cur_damage values !!! //
				if (*(unsigned int*)(&it[in].min_level)==2500 || *(unsigned int*)(&it[in].min_level)==100) {
					it[in].min_level=0;
					it[in].max_level=0;
					it[in].needs_class=0;
					it[in].expire_sn=0;
				}*/

                                ch[cn].item[n]=in;

				set_legacy_item_lvl(in,cn);
			}
		}
		if (ch[cn].citem) {
			if (IDR_DONTSAVE(itmp->driver) || (itmp->flags&IF_LABITEM)) {
				itmp++;
				ch[cn].citem=0;
			} else {
				in=alloc_item();
				if (!in) {
					// should destroy character and items !!!
					elog("tick_login(): could not alloc item");
					xfree(login.chr); xfree(login.itm); xfree(login.ppd);
					login_failed();
					return;
				}	
	
				it[in]=*itmp++;
				it[in].carried=cn;
				it[in].x=it[in].y=it[in].contained=0;
	
				ch[cn].citem=in;

				set_legacy_item_lvl(in,cn);
	
				if (it[in].driver) {
                                        call_item(it[in].driver,in,0,ticker+1);
				}
			}
		}

		// drdata aka persistent player data (ppd)
                ch[cn].dat=NULL;	// start with empty set...
		
		for (pos=0; pos<login.ppd_len; ) {
                        ppd_id=*(unsigned int*)(login.ppd+pos); pos+=4;
			ppd_size=*(unsigned int*)(login.ppd+pos); pos+=4;

                        if (ppd_id!=DRD_JUNK_PPD) {
				ppd=set_data(cn,ppd_id,ppd_size);
				memcpy(ppd,login.ppd+pos,ppd_size);
			}
			pos+=ppd_size;			
		}

		// update legacy arches to have new skills
		if (ch[cn].flags&CF_ARCH) {
			if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==CF_WARRIOR) {
				if (ch[cn].value[1][V_RAGE]==0) ch[cn].value[1][V_RAGE]=1;
			}
			if ((ch[cn].flags&(CF_WARRIOR|CF_MAGE))==CF_MAGE) {
				if (ch[cn].value[1][V_DURATION]==0) ch[cn].value[1][V_DURATION]=1;
			}
		}
		{
			int expe,diff;

			diff=ch[cn].exp-ch[cn].exp_used;

			expe=calc_exp(cn);
			//charlog(cn,"calc exp = %d (%d), used exp = %d (%d)",exp,exp2level(exp),ch[cn].exp_used,exp2level(ch[cn].exp_used));
			if (ch[cn].exp_used!=expe) {
				elog("char %s: used_exp=%d, calculated=%d",ch[cn].name,ch[cn].exp_used,expe);
			}
			ch[cn].exp_used=expe;
			ch[cn].exp=max(ch[cn].exp,ch[cn].exp_used+diff);
		}
	}

	xfree(login.chr); xfree(login.itm); xfree(login.ppd);

	// initialize character data
	ch[cn].ID=login.ID;
	ch[cn].mirror=login.mirror;
	strcpy(ch[cn].name,login.name);
	ch[cn].arg=NULL;
	ch[cn].msg=NULL;
        ch[cn].msg_last=NULL;
        ch[cn].driver=0;
	ch[cn].x=ch[cn].y=ch[cn].tox=ch[cn].toy=0;
	ch[cn].regen_ticker=0;
	ch[cn].last_regen=ticker;
        ch[cn].player=0;
	ch[cn].paid_till=login.paid_till;
	if (login.paid) ch[cn].flags|=CF_PAID;
	else ch[cn].flags&=~CF_PAID;
	
	if (!ch[cn].prof[P_THIEF]) ch[cn].flags&=~CF_THIEFMODE;

        if (!ch[cn].tmpa) ch[cn].tmpa=areaID;
	if (ch[cn].resta<1 || ch[cn].resta>=MAXAREA ||
	    ch[cn].restx<1 || ch[cn].restx>=MAXMAP ||
	    ch[cn].resty<1 || ch[cn].resty>=MAXMAP ||
	    ch[cn].tmpa<1 || ch[cn].tmpa>=MAXAREA ||
	    ch[cn].tmpx<1 || ch[cn].tmpx>=MAXMAP ||
	    ch[cn].tmpy<1 || ch[cn].tmpy>=MAXMAP) {
		ch[cn].tmpa=ch[cn].resta=1;
		ch[cn].tmpx=ch[cn].restx=126;
		ch[cn].tmpy=ch[cn].resty=179;
	}
	
	if (ch[cn].level>=20 && !ch[cn].value[1][V_PROFESSION]) {
		ch[cn].value[1][V_PROFESSION]=1;
	}
	check_prof_max(cn);
	check_can_wear_item(cn);

        ch[cn].flags|=CF_ALIVE;

	/*if (!(ch[cn].flags&CF_MAGE)) {
		while (ch[cn].value[1][V_PULSE]>1) lower_value(cn,V_PULSE);
		if (ch[cn].value[1][V_PULSE]==1) ch[cn].value[1][V_PULSE]=0;
	} else {
		if (ch[cn].value[1][V_PULSE]==0) ch[cn].value[1][V_PULSE]=1;
	}
	
	while (ch[cn].value[1][V_ARCANE]>1 && lower_value(cn,V_ARCANE)) ;
	if (ch[cn].value[1][V_ARCANE]==1) ch[cn].value[1][V_ARCANE]=0;*/

	if (ch[cn].karma<-50 || ch[cn].karma>0) ch[cn].karma=0;

	if (!ch[cn].c1 || !ch[cn].c2 || !ch[cn].c3) {
		ch[cn].c1=0x3def;
		ch[cn].c2=0x294a;
		ch[cn].c3=0x5220;
	}

	if (!isalpha(ch[cn].staff_code[0]) || !isalpha(ch[cn].staff_code[1]) || ch[cn].staff_code[2] || ch[cn].staff_code[3]) {
		ch[cn].staff_code[0]='A';
		ch[cn].staff_code[1]='A';
		ch[cn].staff_code[2]=0;
		ch[cn].staff_code[3]=0;
	}
	
        update_char(cn);
	if (newbie) {
		ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;
		ch[cn].endurance=ch[cn].value[0][V_ENDURANCE]*POWERSCALE;
		ch[cn].mana=ch[cn].value[0][V_MANA]*POWERSCALE;
	}
	ch[cn].dir=DX_RIGHTDOWN;
	ch[cn].group=cn+65536;
	ch[cn].flags&=~(CF_DEAD|CF_LAG|CF_NOGIVE);
	ch[cn].action=ch[cn].duration=ch[cn].step=0;
	ch[cn].player=0;
	ch[cn].regen_ticker=ticker;

	if ((areaID!=20 || !(ch[cn].flags&CF_LQMASTER)) && !(ch[cn].flags&CF_GOD)) ch[cn].flags&=~(CF_INVISIBLE|CF_IMMORTAL|CF_INFRARED);

        if (!drop_char_extended(cn,ch[cn].tmpx,ch[cn].tmpy,6)) {
		xlog("drop failed for %s at %d,%d in tick_login()",ch[cn].name,ch[cn].tmpx,ch[cn].tmpy);
		destroy_char(cn);
                release_char(cn);
		login_failed();
		return;
	}

        login_ok(cn,0);
}


// also checks paid status and sets vendor (ouch!)
static int load_char_pwd(char *pass,int sID,int *ppaid_till,int *ppaid,int vendor)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256];
	int creation_time,paid_till,t;

	                    //     0             1         2      3      4      5          6
	sprintf(buf,"select password,creation_time,paid_till,vendor,locked,banned,credit_cvs from subscriber where ID=%d",sID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select subscriber ID=%d: Error: %s (%d)",sID,mysql_error(&mysql),mysql_errno(&mysql));
                return 1;
	}
        if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return 1;
	}
	if (mysql_num_rows(result)==0) {
		elog("subscriber %d not found??",sID);
		mysql_free_result_cnt(result);
                return 1;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("load_char_pwd: fetch_row returned NULL");
		mysql_free_result_cnt(result);
                return 1;
	}
	if (!row[0]) {
		elog("load_char_pwd: one of the values NULL");
		mysql_free_result_cnt(result);
                return 1;
	}

	if (strcmp(pass,row[0])) {
		mysql_free_result_cnt(result);
                return 1;
	}

	if (row[4] && row[4][0]=='Y') {
		mysql_free_result_cnt(result);
		return 2;
	}

        if (isbanned_iplog(login.ip) && (!row[5] || row[5][0]!='N')) {
		mysql_free_result_cnt(result);
                return 3;
	}

	if (row[1]) creation_time=atoi(row[1]); else creation_time=0;
	if (row[2]) paid_till=atoi(row[2]); else paid_till=0;

	if (paid_till && (!row[6] || row[6][0]!='F')) {
		mysql_free_result_cnt(result);
		return 5;
	}

#ifdef STAFF
	paid_till=time_now+60*60*24;
#endif

        if (paid_till && (paid_till>time_now || paid_till>creation_time+60*60*24*7*4)) {
		if (paid_till&1) t=paid_till;				// 12 hour paid account?
		else t=(paid_till+60*60*24-1)&0xfffffffe;		// paid account?
		if (ppaid) *ppaid=paid_till;
	} else {
		t=(creation_time+60*60*24*28+60*60*24-1)&0xfffffffe;	// new testers get four weeks
		if (ppaid) *ppaid=0;
	}

	if (ppaid_till) *ppaid_till=t;

        //xlog("%.2f days left for %d (%d)",(t-time_now)/(60.0*60*24),sID,paid_till);

	if (t<time_now) {
		mysql_free_result_cnt(result);
                return 4;
	}


	if (vendor && (!row[3] || atoi(row[3])==0)) {
		sprintf(buf,"update subscriber set vendor=%d where ID=%d",vendor,sID);
		if (mysql_query_con(&mysql,buf))
			elog("Failed to update vendor ID=%d, vendor=%d: Error: %s (%d)",sID,vendor,mysql_error(&mysql),mysql_errno(&mysql));		
		save_subscriber_cnt++;
	}

        // all fine
	mysql_free_result_cnt(result);
	return 0;
}

static int load_char_dup(int ID,int sID)
{
	MYSQL_RES *result;
        char buf[256];

	if (sID==1) return 1;	// hack for easier testing

	sprintf(buf,"select sID from chars where sID=%d and ID!=%d and current_area!=0 limit 1",sID,ID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select subscriber ID=%d: Error: %s (%d)",sID,mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
	}
        if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
	}

	// found another character online?
	if (mysql_num_rows(result)>0) {
		mysql_free_result_cnt(result);
                return 0;
	}

	// all fine
	mysql_free_result_cnt(result);
	return 1;
}

// load character from database, checking for validity of password
// and allowed_area.
static void load_char(char *name,char *password)
{
	MYSQL_RES *result;
	MYSQL_ROW row;	
	char buf[256];	
	int current,allowed,ID,mirror,tomirror,current_mirror,newmirror=0,tmp,sID;
	int paid_till,paid;
        unsigned long *len;

        // make sure input data is ready
	pthread_mutex_lock(&data_mutex);
        if (login.status!=LS_READ) {
		pthread_mutex_unlock(&data_mutex);
		elog("load_char got called but no login is waiting (%s,%s,%d)",name,password,login.status);
		return;
	}
	pthread_mutex_unlock(&data_mutex);

	//xlog("name=%s (%s), ip=%u",name,login.name,login.ip);
	if (is_badpass_ip(&mysql,login.ip)) {
                xlog("ip blocked for %s (%d.%d.%d.%d)",name,(login.ip>>24)&255,(login.ip>>16)&255,(login.ip>>8)&255,(login.ip>>0)&255);
		login_toomany();
		return;
	}

        // lock character and subscriber table to avoid misshap
	if (mysql_query_con(&mysql,"lock tables chars write, subscriber write, iplog write, ipban write")) {
		elog("Failed to lock chars, subscriber table: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		login_failed();
		return;
	}

	// read the data we need
	//                    0   1    2    3            4            5      6  7   8      9		 10
        sprintf(buf,"select sID,chr,item,name,current_area,allowed_area,locked,ID,ppd,mirror,current_mirror from chars where name='%s'",login.name);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select account name=%s: Error: %s (%d)",login.name,mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");
		login_failed();
		return;
	}
        if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");
		login_failed();
		return;
	}
	if (mysql_num_rows(result)==0) {
		mysql_free_result_cnt(result);
                mysql_query_con(&mysql,"unlock tables");
		login_passwd();
		return;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("load_char: fetch_row returned NULL");
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_failed();
		return;
	}
	if (!row[0] || !row[1] || !row[2] || !row[3] || !row[4] || !row[5] || !row[6] || !row[7] || !row[8]) {
		elog("load_char: one of the values NULL");
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_failed();
		return;
	}

	sID=atoi(row[0]);
	ID=atoi(row[7]);

	// currently locked?
	if (row[6][0]!='N') {
		xlog("ID=%d (%s) is locked",ID,row[3]);
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_locked();
		return;
	}

        if ((tmp=load_char_pwd(login.password,atoi(row[0]),&paid_till,&paid,login.vendor))) {
		if (tmp==1) xlog("password for ID=%d (%s) is wrong",ID,row[3]);
		else if (tmp==2) xlog("account locked for ID=%d (%s)",ID,row[3]);
		else if (tmp==3) xlog("account ipbanned for ID=%d (%s) (%d.%d.%d.%d)",ID,row[3],(login.ip>>24)&255,(login.ip>>16)&255,(login.ip>>8)&255,(login.ip>>0)&255);
		else if (tmp==5) xlog("account not fixed for ID=%d (%s)",ID,row[3]);
		else xlog("account not paid for for ID=%d (%s)",ID,row[3]);
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		if (tmp==1) { login_passwd(); add_badpass_ip(&mysql,login.ip); }
		else if (tmp==2) login_locked();
		else if (tmp==3) login_iplocked();
		else if (tmp==5) login_notfixed();
                else login_nopay();		
		return;
	}

	if (!paid && ((struct character*)(row[1]))->karma<-4 && (((struct character*)(row[1]))->flags&CF_USED)) {
		xlog("ID=%d (%s) locked by -5 karma rule.",ID,row[3]);
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_locked();
		return;
	}

	if (!load_char_dup(ID,atoi(row[0]))) {	
		xlog("duplicate login for ID=%d (%s)",ID,row[3]);
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_dup();
		return;
	}

	if (nologin && atoi(row[0])!=1) {
                mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		login_shutdown();
		return;
	}
	
        current=atoi(row[4]);
	allowed=atoi(row[5]);
	if (row[10]) current_mirror=atoi(row[10]);
	else current_mirror=0;
	
	if (row[9]) mirror=atoi(row[9]);
	else mirror=0;
	if (!mirror) { newmirror=1; mirror=RANDOM(26)+1; }
		
        login.ID=ID;

	// is he supposed to be somewhere else?
        if (allowed!=areaID) {	// nope, send him to where he belongs
		//xlog("character is supposed to enter area %d (%d)",allowed,areaID);
		mysql_free_result_cnt(result);
		if (newmirror) {
			sprintf(buf,"update chars set mirror=%d where ID=%d",mirror,ID);
			mysql_query_con(&mysql,buf);
			save_char_mirror_cnt++;
		}
		mysql_query_con(&mysql,"unlock tables");
		if (current) {
			if (check_area(current,current_mirror)) login_newarea(current,current_mirror);
			else login_newarea(-1,current_mirror);
		} else login_newarea(allowed,get_mirror(allowed,mirror));
		return;
	}

	// database thinks the player is currently online somewhere
	if (current) {
		if (current!=areaID || current_mirror!=areaM) {	// online in a different area. send him there.
			//xlog("character is marked active on different area/mirror (%d/%d)",current,current_mirror);
			mysql_free_result_cnt(result);
			if (newmirror) {
				sprintf(buf,"update chars set mirror=%d where ID=%d",mirror,ID);
				mysql_query_con(&mysql,buf);
				save_char_mirror_cnt++;
			}
			mysql_query_con(&mysql,"unlock tables");
			
			if (check_area(current,current_mirror)) login_newarea(current,current_mirror);
			else login_newarea(-1,current_mirror);
                        return;
		}
		
		// he's supposed to be online here... we should simply set login.current and leave, instead of reading all the data.
		
		login.current=current;
	}
	
	tomirror=get_mirror(areaID,mirror);
	if (areaM!=tomirror) {
		//xlog("character is supposed to enter mirror %d",mirror);
		mysql_free_result_cnt(result);
		if (newmirror) {
			sprintf(buf,"update chars set mirror=%d where ID=%d",mirror,ID);
			mysql_query_con(&mysql,buf);
			save_char_mirror_cnt++;
		}
		mysql_query_con(&mysql,"unlock tables");
		login_newarea(allowed,tomirror);
		return;
	}
	if (1 || *(unsigned int*)(row[1])!=COMPRESS_MAGIC) {
		// copy data to login structure
		len=mysql_fetch_lengths(result);
	
		//xlog("chr.len=%ld, itm.len=%ld, ppd.len=%ld, total=%d",len[1],len[2],len[8],len[1]+len[2]+len[8]);
	
		login.chr=xcalloc(max(sizeof(struct character),len[1]),IM_DATABASE);
		login.itm=xmalloc(len[2],IM_DATABASE);
		login.ppd=xmalloc(len[8],IM_DATABASE);
	
		if (!login.chr || !login.itm || !login.ppd) {
			elog("memory low in load_char");	// !!! handle gracefully !!!
			exit(1);
		}
		
		login.chr_len=len[1];
		login.itm_len=len[2];
		login.ppd_len=len[8];
	
		if (len[1]==1028) {	// ver 1 char
			
			memcpy(login.chr,row[1],300);
			
			memcpy(login.chr+300+10,row[1]+300,76);		
			memcpy(login.chr+376+20,row[1]+376,len[1]-376);
	
			xlog("converted version1 char");
	
		} else memcpy(login.chr,row[1],len[1]);	// current ver
	
		memcpy(login.itm,row[2],len[2]);
		memcpy(login.ppd,row[8],len[8]);
	} else {
		login.chr=xcalloc(max(sizeof(struct character),*(unsigned int*)(row[1]+4)),IM_DATABASE);
		login.itm=xmalloc(*(unsigned int*)(row[2]+4),IM_DATABASE);
		login.ppd=xmalloc(*(unsigned int*)(row[8]+4),IM_DATABASE);
	
		login.chr_len=uncompress_string(login.chr,row[1],*(unsigned int*)(row[1]+4));
		login.itm_len=uncompress_string(login.itm,row[2],*(unsigned int*)(row[2]+4));
		login.ppd_len=uncompress_string(login.ppd,row[8],*(unsigned int*)(row[8]+4));
	}

        login.mirror=mirror;
        login.paid_till=paid_till;
	login.paid=paid;

        strcpy(login.name,row[3]);

        mysql_free_result_cnt(result);

        // mark character as online in database
	sprintf(buf,"update chars set current_area=%d, current_mirror=%d, allowed_area=%d, login_time=%d, spacer=0 where ID=%d",
		areaID,
		areaM,
		areaID,
		time_now,
		ID);

	load_char_cnt++;

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to update account %s (ID=%d) on login: Error: %s (%d)",login.name,ID,mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");

		login_failed();
		return;
	}
	mysql_query_con(&mysql,"unlock tables");

        add_iplog(sID,login.ip);

#ifdef CHARINFO
	sprintf(buf,"update charinfo set current_area=%d, login_time=%d where ID=%d",areaID,time_now,ID);

        if (mysql_query_con(&mysql,buf)) {
		elog("Failed to update charinfo %s (ID=%d) on login: Error: %s (%d)",login.name,ID,mysql_error(&mysql),mysql_errno(&mysql));
	}
#endif
	//xlog("db-unique-in: %d",login.unique);
	if (login.unique==0) {
		mysql_query_con(&mysql,"lock tables constants write");
		mysql_query_con(&mysql,"select val from constants where name='unique'");
                if ((result=mysql_store_result_cnt(&mysql)) && (row=mysql_fetch_row(result))) {
			login.unique=atoi(row[0])+1;
			mysql_free_result_cnt(result);
			sprintf(buf,"update constants set val='%d' where name='unique'",login.unique);
			mysql_query_con(&mysql,buf);
		} else elog("unique-error: %s",mysql_error(&mysql));
		mysql_query_con(&mysql,"unlock tables");
	}
	//xlog("db-unique-out: %d",login.unique);

	pthread_mutex_lock(&data_mutex);
	login.status=LS_CREATE;
	pthread_mutex_unlock(&data_mutex);	
}

struct area
{
	int server;
	int port;
	int status;
};

static struct area area[MAXAREA][MAXMIRROR];

static void update_arealist(void)
{
	char buf[256];
	MYSQL_RES *result;
	MYSQL_ROW row;
	unsigned int server,port,ID,mirror;
	int alive;

        sprintf(buf,"select ID,server,port,alive_time,mirror from area");
	if (mysql_query_con(&mysql,buf)) {
		elog("update_arealist: Could not read area table: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("update_arealist: Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	while ((row=mysql_fetch_row(result))) {
		if (!row[0] || !row[1] || !row[2] || !row[3] || !row[4]) {
			elog("update_arealist: Incomplete row: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
			continue;
		}

		ID=atoi(row[0]);
		server=strtoul(row[1],(char **)NULL, 10);
		port=atoi(row[2]);
		alive=atoi(row[3]);
		mirror=atoi(row[4]);

                if (ID<1 || ID>=MAXAREA) {
			elog("update_arealist: got weird ID %d, ignoring",ID);
			continue;
		}
		if (mirror<1 || mirror>=MAXMIRROR) {
			elog("update_arealist: got weird mirror %d, ignoring",mirror);
		}

		area[ID][mirror].server=server;
		area[ID][mirror].port=port;
		if (time_now-alive<60*5) area[ID][mirror].status=1;
		else area[ID][mirror].status=0;
	}

	mysql_free_result_cnt(result);
}

int check_area(int ID,int mirror)
{
	if (ID<1 || ID>=MAXAREA) {
		elog("check_area: got weird ID %d, ignoring",ID);
                return 0;
	}
	if (mirror<1 || mirror>=MAXMIRROR) {
		elog("check_area: got weird mirror %d, ignoring",mirror);
                return 0;
	}
	if (area[ID][mirror].status==1) return 1;
	else return 0;
}

int get_mirror(int ID,int mirror)
{
	int n,good=0;

	if (ID<1 || ID>=MAXAREA) {
		elog("get_mirror: got weird ID %d, ignoring",ID);
                return 0;
	}
	if (mirror<1 || mirror>=MAXMIRROR) {
		elog("get_mirror: got weird mirror %d, ignoring",mirror);
                return 0;
	}

        for (n=mirror; n>0; n--) {
                if (area[ID][n].status!=1) continue;
                return n;
	}
        for (n=MAXMIRROR-1; n>mirror; n--) {
                if (area[ID][n].status!=1) continue;
		good=n;
	}
        if (good) return good;
	
	return 0;
}

int get_area(int ID,int mirror,int *pserver,int *pport)
{
	int tomirror;

	if (ID<1 || ID>=MAXAREA) {
		elog("get_area: got weird ID %d, ignoring",ID);
		return 0;
	}
	if (mirror<1 || mirror>=MAXMIRROR) {
		elog("get_area: got weird mirror %d, ignoring",mirror);
		return 0;
	}
        if (!(tomirror=get_mirror(ID,mirror))) return 0;

	if (pserver) *pserver=area[ID][tomirror].server;
	if (pport) *pport=area[ID][tomirror].port;

	return 1;
}

static void db_create_storage(void)
{
	char buf[1024*64*2],data[1024*64*2];

	pthread_mutex_lock(&data_mutex);
	if (cs.state!=CSS_CREATE) {
		pthread_mutex_unlock(&data_mutex);
		elog("db_create_storage(): got called, but no request waiting!");
		return;
	}
	pthread_mutex_unlock(&data_mutex);

	mysql_real_escape_string(&mysql,data,(char*)cs.content,cs.size);
	sprintf(buf,"insert into storage values(%d,'%s',1,'%s')",cs.ID,cs.desc,data);
	
	if (mysql_query_con(&mysql,buf)) {
		//elog("Failed to create storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		pthread_mutex_lock(&data_mutex);
		cs.state=CSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	
	pthread_mutex_lock(&data_mutex);
	cs.ID=mysql_insert_id(&mysql);
	cs.state=CSS_DONE;
	pthread_mutex_unlock(&data_mutex);
}

static void db_update_storage(void)
{
        char buf[1024*64*2],data[1024*64*2];

	pthread_mutex_lock(&data_mutex);
	if (us.state!=USS_CREATE) {
		pthread_mutex_unlock(&data_mutex);
		elog("db_update_storage(): got called, but no request waiting!");
		return;
	}
	pthread_mutex_unlock(&data_mutex);

        mysql_real_escape_string(&mysql,data,(char*)us.content,us.size);
	sprintf(buf,"update storage set content='%s',version=%d where ID=%d and version=%d",data,us.version+1,us.ID,us.version);
	save_storage_cnt++;
	
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to update storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		//mysql_query_con(&mysql,"unlock tables");
		pthread_mutex_lock(&data_mutex);
		us.state=USS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	if (mysql_affected_rows(&mysql)==0) {	// version was changed, and no row fit
		pthread_mutex_lock(&data_mutex);
		us.state=USS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	//mysql_query_con(&mysql,"unlock tables");
	
	pthread_mutex_lock(&data_mutex);
        us.state=USS_DONE;
	pthread_mutex_unlock(&data_mutex);
}

static void db_read_storage(void)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	unsigned long *len;
	char buf[256];
	int version;

	pthread_mutex_lock(&data_mutex);
	if (rs.state!=RSS_CREATE) {
		pthread_mutex_unlock(&data_mutex);
		elog("db_read_storage(): got called, but no request waiting!");
		return;
	}
	pthread_mutex_unlock(&data_mutex);

	//-------

	sprintf(buf,"select version from storage where ID=%d",rs.ID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to read storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result in read_storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (mysql_num_rows(result)==0) {
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("read_storage(): fetch_row returned NULL");
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (!row[0]) {
		elog("read_storage(): one of the values NULL");
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	version=atoi(row[0]);

	mysql_free_result_cnt(result);

	if (version==rs.version) {
		pthread_mutex_lock(&data_mutex);
		rs.state=RSS_DONE;
		rs.content=NULL;
                rs.size=0;
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	//-------

	sprintf(buf,"select content,version from storage where ID=%d",rs.ID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to read storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result in read_storage: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (mysql_num_rows(result)==0) {
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("read_storage(): fetch_row returned NULL");
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}
	if (!row[0] || !row[1]) {
		elog("read_storage(): one of the values NULL");
		mysql_free_result_cnt(result);
                pthread_mutex_lock(&data_mutex);
		rs.state=RSS_FAILED;
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	len=mysql_fetch_lengths(result);

	pthread_mutex_lock(&data_mutex);
	rs.state=RSS_DONE;
	rs.content=xmalloc(len[0],IM_STORAGE);
	if (!rs.content) rs.state=RSS_FAILED;
	else memcpy(rs.content,row[0],len[0]);
	rs.version=atoi(row[1]);
        rs.size=len[0];
	pthread_mutex_unlock(&data_mutex);

	mysql_free_result_cnt(result);
}

void db_lookup_id(char *tID)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256],name[80];
	unsigned int ID;

        ID=strtoul(tID,(char **)NULL, 10);

	// avoid suplicate lookups
	if (lookup_ID(NULL,ID)) return;

	sprintf(buf,"select name from chars where ID=%d",ID);

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to lookup ID: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if ((row=mysql_fetch_row(result)) && row[0]) {

		strncpy(name,row[0],40); name[39]=0;

		mysql_free_result_cnt(result);

		lookup_add_cache(ID,name);
		
		return;
	}

	mysql_free_result_cnt(result);

	lookup_add_cache(ID,NULL);
}

void db_lookup_name(char *name)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256],realname[80];
	int ID;

        // avoid suplicate lookups
	if (lookup_name(name,NULL)) return;

	sprintf(buf,"select ID,name from chars where name='%s'",name);

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to lookup name: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if ((row=mysql_fetch_row(result)) && row[0] && row[1]) {

		ID=strtoul(row[0],(char**)NULL,10);
                strncpy(realname,row[1],40); realname[39]=0;

		mysql_free_result_cnt(result);

		lookup_add_cache(ID,realname);
		
		return;
	}

	mysql_free_result_cnt(result);

	lookup_add_cache(-1,name);

	return;
}

void check_task(void)
{
	char buf[256];
	MYSQL_RES *result;
	MYSQL_ROW row;
	int del,ID;

        // get a write lock on task, notes and chars table. sigh. SQL sucks...
	if (mysql_query_con(&mysql,"lock tables task write, chars write, notes write, charinfo write")) {
		elog("Failed to lock task table: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	sprintf(buf,"select ID,content from task");

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to get content from task: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        while ((row=mysql_fetch_row(result)) && row[0] && row[1]) {
		ID=atoi(row[0]);

		xlog("processing task %d",ID);
		del=process_task(row[1]);
                if (del) {
			sprintf(buf,"delete from task where ID=%d",ID);

			if (mysql_query_con(&mysql,buf)) {
				elog("Failed to delete task from task: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));				
			} else xlog("deleted task %d, done",ID);
		}
	}

	mysql_free_result_cnt(result);
	
	mysql_query_con(&mysql,"unlock tables");
}

void db_read_notes(char *suID,char *srID)
{
	char buf[256];
	MYSQL_RES *result;
	MYSQL_ROW row;
	int kind,uID,rID,date;

	uID=atoi(suID);
	rID=atoi(srID);

        sprintf(buf,"select kind,content,cID,date,ID from notes where uID=%d",uID);

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to get content from notes: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	tell_chat(0,rID,1,"Start of Notes:");

        while ((row=mysql_fetch_row(result)) && row[0] && row[1] && row[2]) {
		kind=atoi(row[0]);
		if (row[3]) date=atoi(row[3]); else date=0;

		switch(kind) {
			case 1:		list_punishment(rID,(void*)row[1],atoi(row[2]),date,atoi(row[4])); break;
		}

	}
        tell_chat(0,rID,1,"End of Notes");

	mysql_free_result_cnt(result);
}

void karmalog_s(int rID,struct punishment *pun,int cID,int date,int ID,int uID)
{
	char name[80],offender[80];
	struct tm *tm;

	lookup_ID(name,cID);
	lookup_ID(offender,uID);
	
	tm=localtime((void*)&date);

	tell_chat(0,rID,1,"%s, %d Karma from %s for %s at %02d:%02d:%02d.",
		  offender,
                  pun->karma,
		  name,
		  pun->reason,
                  tm->tm_hour,
		  tm->tm_min,
		  tm->tm_sec);	
}


void db_karmalog(char *xrID)
{
	char buf[256];
	MYSQL_RES *result;
	MYSQL_ROW row;
	int kind,rID,date;

	rID=atoi(xrID);

        sprintf(buf,"select kind,content,cID,date,ID,uID from notes where date>=%d order by date desc limit 60",time_now-60*60*24);

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to get content from notes: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	tell_chat(0,rID,1,"Karmalog:");

        while ((row=mysql_fetch_row(result)) && row[0] && row[1] && row[2]) {
		kind=atoi(row[0]);
		if (row[3]) date=atoi(row[3]); else date=0;

		switch(kind) {
			case 1:		karmalog_s(rID,(void*)row[1],atoi(row[2]),date,atoi(row[4]),atoi(row[5])); break;
		}

	}
        tell_chat(0,rID,1,"---");

	mysql_free_result_cnt(result);
}

void db_lastseen(char *suID,char *srID)
{
	char buf[256];
	MYSQL_RES *result;
	MYSQL_ROW row;
	int t,uID,rID,cl;

	uID=atoi(suID);
	rID=atoi(srID);

        sprintf(buf,"select name,login_time,logout_time,class,creation_time from charinfo where ID=%d",uID);

	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to get times from charinfo: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if ((row=mysql_fetch_row(result)) && row[0] && row[1] && row[2] && row[3] && row[4]) {
		cl=atoi(row[3]);
		if (cl&CF_GOD) {
			tell_chat(0,rID,1,"%s was seen quite recently.",row[0]);
		} else {
			t=time_now-max(max(atoi(row[1]),atoi(row[2])),atoi(row[4]));
			tell_chat(0,rID,1,"%s was last seen %d days, %d hours, %d minutes ago.",row[0],t/(60*60*24),(t/(60*60))%24,(t/60)%60);
		}
	}

	mysql_free_result_cnt(result);
}

int db_unpunish(int ID, void* pm,int pmlen)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256];

	sprintf(buf,"select content from notes where ID=%d",ID);
        if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select note ID=%d: Error: %s (%d)",ID,mysql_error(&mysql),mysql_errno(&mysql));
		return 0;
	}
	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return 0;
	}
	if (!(row=mysql_fetch_row(result))) {
		elog("db_unpunish: fetch_row returned NULL");
		mysql_free_result_cnt(result);
                return 0;
	}
	if (!row[0]) {
		elog("db_unpunish: one of the values NULL");
		mysql_free_result_cnt(result);
                return 0;
	}

	memcpy(pm,row[0],pmlen);
	mysql_free_result_cnt(result);

        sprintf(buf,"delete from notes where ID=%d",ID);
        if (mysql_query_con(&mysql,buf)) {
		elog("Failed to delete note ID=%d: Error: %s (%d)",ID,mysql_error(&mysql),mysql_errno(&mysql));
		return 0;
	}
	return 1;
}

void db_read_clanlog(char *query)
{
	char clan_name[80];
	MYSQL_RES *result;
	MYSQL_ROW row;
	int cnID,date,cnr,serial,cnt=0;
	struct tm *tm;

        if (mysql_query_con(&mysql,query)) {
		elog("Failed to read clanlog: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        // 0    1      2    3	 4
	//%d,clan,serial,time,text
        while ((row=mysql_fetch_row(result)) && row[0] && row[1] && row[2] && row[3] && row[4]) {
		cnID=atoi(row[0]);
		date=atoi(row[3]);
		
		cnr=atoi(row[1]);
		serial=atoi(row[2]);
		if (clan_serial(cnr)==serial && get_clan_name(cnr)) strcpy(clan_name,get_clan_name(cnr));
		else sprintf(clan_name,"Former clan %d",cnr);

		tm=localtime((time_t*)&date);
		
		cnt++;
		if (cnt>50) {
			tell_chat(0,cnID,1,"Not all entries displayed. Use the same query with -s %d to continue the listing.",(time_now-date+60*60-1)/(60*60));
			break;
		}

                tell_chat(0,cnID,1,"At %02d:%02d on %02d/%02d/%02d, %s: %s",
			  tm->tm_hour,
			  tm->tm_min,
			  tm->tm_mon+1,
			  tm->tm_mday,
			  tm->tm_year%100,
			  clan_name,			
                          row[4]);
	}
        mysql_free_result_cnt(result);

        if (cnt==0) {
		cnID=atoi(query+6);
		tell_chat(0,cnID,1,"No matching records.");
	}
}

void db_exterminate(char *name,char *master)
{
	MYSQL_RES *result;
	MYSQL_ROW row;

	int masterID,sID,nrc,nrb;
	char query[256],sub[256],code[4];

	masterID=atoi(master);
	code[0]=master[10]; code[1]=master[11]; code[2]=0;

	sprintf(query,"select sID from chars where name='%s'",name);
	if (mysql_query_con(&mysql,query)) {
		elog("Failed to select sID: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if ((row=mysql_fetch_row(result)) && row[0]) {
		sID=atoi(row[0]);
		mysql_free_result_cnt(result);
/*#ifdef CHARINFO
		sprintf(query,"update charinfo set locked='Y' where sID=%d",sID);
		if (mysql_query_con(&mysql,query)) {
			elog("exterminate: Failed to lock charinfo: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
#endif

                sprintf(query,"update chars set locked='Y' where sID=%d",sID);
		if (mysql_query_con(&mysql,query)) {
			elog("exterminate: Failed to lock chars: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
		nrc=mysql_affected_rows(&mysql);*/

		sprintf(query,"update subscriber set locked='Y' where ID=%d",sID);
		if (mysql_query_con(&mysql,query)) {
			elog("exterminate: Failed to lock subscriber: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
		nrc=mysql_affected_rows(&mysql);
		
		sprintf(query,"insert ipban select 0,ip,%d,%d,%d from iplog where sID=%d",
			time_now,
			time_now+60*60*24*7*4,
			sID,
			sID);
		if (mysql_query_con(&mysql,query)) {
			elog("exterminate: Failed to insert bans: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
                nrb=mysql_affected_rows(&mysql);
		
		sprintf(query,"update subscriber set banned='I' where ID=%d",sID);
		if (mysql_query_con(&mysql,query)) {
			elog("exterminate: Failed set banned=I: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
		
		tell_chat(0,masterID,1,"Locked %d accounts and %d IP addresses.",nrc,nrb);

		sprintf(sub,"EXTERMINATE %s, called by %s (%d)",name,code,masterID);
		sendmail("exterminate@astonia.com",sub,"...","auto@astonia.com",0);
	} else {
		tell_chat(0,masterID,1,"Player '%s' not found.",name);
		mysql_free_result_cnt(result);
	}
}

void db_rename(char *name,char *master_to)
{
	int masterID,nr,len;
	char query[256],to[256],*ptr;

	masterID=atoi(master_to);
	strcpy(to,master_to+11);

	for (ptr=to,len=0; *ptr; ptr++,len++) {
		if (len==0) *ptr=toupper(*ptr);
		else *ptr=tolower(*ptr);
		if (!isalpha(*ptr)) {
			tell_chat(0,masterID,1,"Illegal name.");
			return;
		}
	}
	if (len<3 || len>35) {
		tell_chat(0,masterID,1,"Name too long or too short.");
		return;
	}

	sprintf(query,"update chars set name='%s' where name='%s'",to,name);
	if (mysql_query_con(&mysql,query)) {
		tell_chat(0,masterID,1,"Failed to change name: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        nr=mysql_affected_rows(&mysql);

#ifdef CHARINFO
	sprintf(query,"update charinfo set name='%s' where name='%s'",to,name);
	if (mysql_query_con(&mysql,query)) {
		elog("Failed to change name in charinfo: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}
#endif

	if (nr) tell_chat(0,masterID,1,"Changed %s to %s. The change will be visible after the next login.",name,to);
	else tell_chat(0,masterID,1,"Didn't work, most probable cause: %s not found.",name);
}

void db_rescue_char(char *IDstring)
{
	int ID;
	MYSQL_RES *result;
	MYSQL_ROW row;
	unsigned char cbuf[sizeof(struct character)*2];
	char buf[256+sizeof(cbuf)];
	int current_area,allowed_area,mirror,current_mirror;
	struct character *tmp;
	unsigned long *len;

        ID=atoi(IDstring);
	elog("db_rescue_char %d",ID);

	// lock character and subscriber table to avoid misshap
	if (mysql_query_con(&mysql,"lock tables chars write")) {
		elog("Failed to lock chars: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	// read the data we need
	//                  0   1            2            3      4
        sprintf(buf,"select chr,current_area,allowed_area,mirror,current_mirror from chars where ID=%d",ID);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed to select char ID=%d: Error: %s (%d)",ID,mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");		
		return;
	}
        if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");
		return;
	}
	if (mysql_num_rows(result)==0) {
		elog("char to rescue not found");
		mysql_free_result_cnt(result);
                mysql_query_con(&mysql,"unlock tables");
		return;
	}
        if (!(row=mysql_fetch_row(result))) {
		elog("rescue_char: fetch_row returned NULL");
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		return;
	}
	if (!row[0] || !row[1] || !row[2] || !row[3] || !row[4]) {
		elog("rescue_char: one of the values NULL");
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		return;
	}

	len=mysql_fetch_lengths(result);

	if (len[0]!=sizeof(struct character)) {
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		xlog("rescue: char size wrong");
		return;
	}

	current_area=atoi(row[1]);
	allowed_area=atoi(row[2]);
	mirror=atoi(row[3]);
	current_mirror=atoi(row[4]);
	tmp=(void*)(row[0]);

	if (get_area(3,mirror,NULL,NULL)) {
		current_area=0;
		current_mirror=0;
                tmp->restx=tmp->tmpx=167;
		tmp->resty=tmp->tmpy=188;
		tmp->resta=tmp->tmpa=allowed_area=3;
	} else if (get_area(1,mirror,NULL,NULL)) {
		current_area=0;
		current_mirror=0;
		tmp->restx=tmp->tmpx=126;
		tmp->resty=tmp->tmpy=179;
		tmp->resta=tmp->tmpa=allowed_area=1;
	}
#ifdef STAFF
	else if (get_area(27,mirror,NULL,NULL)) {
		current_area=0;
		current_mirror=0;
		tmp->restx=tmp->tmpx=250;
		tmp->resty=tmp->tmpy=250;
		tmp->resta=tmp->tmpa=allowed_area=27;
		tmp->flags|=CF_GOD;
	}	
#endif
	else {
		mysql_free_result_cnt(result);
		mysql_query_con(&mysql,"unlock tables");
		xlog("cannot rescue, area 1 and 3 are down too");
		return;
	}

	mysql_free_result_cnt(result);

	mysql_real_escape_string(&mysql,cbuf,(void*)tmp,sizeof(struct character));
	sprintf(buf,"update chars set current_area=%d,allowed_area=%d,current_mirror=%d,chr='%s',spacer=44 where ID=%d",
		current_area,
		allowed_area,
		current_mirror,
		cbuf,
		ID);

	if (mysql_query_con(&mysql,buf)) {
                elog("rescue_char: Could not write char: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		mysql_query_con(&mysql,"unlock tables");
                return;
	}
	elog("rescued %d to %d",ID,allowed_area);
	mysql_query_con(&mysql,"unlock tables");
}

int db_set_stat(char *name,int mod,int value)
{
	char query[256];

	sprintf(query,"replace stats set name='%s%03d', value=%d",name,mod,value);
	if (mysql_query_con(&mysql,query)) {
                elog("set_stat: Could not replace: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	return 1;
}

int db_add_stat(char *name,int mod,int value)
{
	char query[256];

	// try update first (ie real add)
	sprintf(query,"update stats set value=value+%d where name='%s%03d'",value,name,mod);
	if (mysql_query_con(&mysql,query)) {	
                elog("add_stat: Could not update1: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	if (mysql_affected_rows(&mysql)) return 1;

	// try create next
	sprintf(query,"insert stats set name='%s%03d',value=%d",name,mod,value);
	if (!mysql_query_con(&mysql,query)) return 1;

	// try update again, we assume sombody created the name in the meantime
	sprintf(query,"update stats set value=value+%d where name='%s%03d'",value,name,mod);
	if (mysql_query_con(&mysql,query)) {	
                elog("add_stat: Could not update2: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	return 1;
}

int db_max_stat(char *name,int mod,int value)
{
	char query[256];

	// try update first (ie real add)
	sprintf(query,"update stats set value=greatest(value,%d) where name='%s%03d'",value,name,mod);
	if (mysql_query_con(&mysql,query)) {	
                elog("add_stat: Could not update1: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	if (mysql_affected_rows(&mysql)) return 1;

	// try create next
	sprintf(query,"insert stats set name='%s%03d',value=%d",name,mod,value);
	if (!mysql_query_con(&mysql,query)) return 1;

	// try update again, we assume sombody created the name in the meantime
	sprintf(query,"update stats set value=greatest(value,%d) where name='%s%03d'",value,name,mod);
	if (mysql_query_con(&mysql,query)) {	
                elog("add_stat: Could not update2: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	return 1;
}

int db_reset_stat(char *name,int mod)
{
	char query[256];

	sprintf(query,"update stats set value=0 where name='%s%03d'",name,mod);
	if (mysql_query_con(&mysql,query)) {
                elog("set_stat: Could not replace: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));		
                return 0;
	}
	return 1;
}


void db_stat_update(void)
{
	int diff,now,reset;
	static int lastticker=0;
	static int lastreseth=0;
	static int lastresetd=0;
	static int lastresetw=0;

	diff=ticker-lastticker;
	now=time_now;

        db_add_stat("PLRONH",(now/60/60)%(24*7),online*diff/TICKS);
	reset=((now/60/60)+2)%(24*7);
	if (reset!=lastreseth) {
		db_reset_stat("PLRONH",reset);
		lastreseth=reset;
		xlog("reset plronh %d",reset);
	}
	
	db_add_stat("PLROND",(now/60/60/24)%(365),online*diff/TICKS);
	reset=((now/60/60/24)+2)%(365);
	if (reset!=lastresetd) {
		db_reset_stat("PLROND",reset);
		lastresetd=reset;
	}

	db_add_stat("PLRONW",(now/60/60/24/7)%(52*4),online*diff/TICKS);
	reset=((now/60/60/24/7)+2)%(52*4);
	if (reset!=lastresetw) {
		db_reset_stat("PLRONW",reset);
		lastresetw=reset;
	}

	lastticker=ticker;
}

void db_lockname(char *name,char *master)
{
	int masterID,nr,len;
	char query[256],*ptr;

	masterID=atoi(master);

	for (ptr=name,len=0; *ptr; ptr++,len++) {
                *ptr=tolower(*ptr);
		if (!isalpha(*ptr)) {
			tell_chat(0,masterID,1,"Illegal name.");
			return;
		}
	}
	if (len<3 || len>35) {
		tell_chat(0,masterID,1,"Name too long or too short.");
		return;
	}

	sprintf(query,"insert badname values(0,'%s')",name);
	if (mysql_query_con(&mysql,query)) {
		tell_chat(0,masterID,1,"Failed to insert name: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        nr=mysql_affected_rows(&mysql);

	if (nr) tell_chat(0,masterID,1,"Added %s to bad name database.",name);
	else tell_chat(0,masterID,1,"Didn't work, most probable cause: %s already in bad name database.",name);
}

void db_unlockname(char *name,char *master)
{
	int masterID,nr,len;
	char query[256],*ptr;

	masterID=atoi(master);

	for (ptr=name,len=0; *ptr; ptr++,len++) {
                *ptr=tolower(*ptr);
		if (!isalpha(*ptr)) {
			tell_chat(0,masterID,1,"Illegal name.");
			return;
		}
	}
	if (len<3 || len>35) {
		tell_chat(0,masterID,1,"Name too long or too short.");
		return;
	}

	sprintf(query,"delete from badname where bad='%s'",name);
	if (mysql_query_con(&mysql,query)) {
		tell_chat(0,masterID,1,"Failed to delete name: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        nr=mysql_affected_rows(&mysql);

	if (nr) tell_chat(0,masterID,1,"Deleted %s from bad name database.",name);
	else tell_chat(0,masterID,1,"Didn't work, most probable cause: %s not in bad name database.",name);
}


//--------- clubs -----------

struct club club[MAXCLUB];

//create table clubs ( ID int primary key, name char(80) not null, paid int not null, money int not null, serial int not null );

int db_create_club(int cnr)
{
	char buf[256];
	
	sprintf(buf,"insert clubs values(%d,'%s',%d,%d,%d)",cnr,club[cnr].name,club[cnr].paid,club[cnr].money,club[cnr].serial);
	add_query(DT_QUERY,buf,"create club",0);

	return 0;
}

void db_read_clubs(void)
{
	char name[80];
	int ID,money,paid,serial;
	MYSQL_RES *result;
	MYSQL_ROW row;
	extern int club_update_done;

        if (mysql_query_con(&mysql,"select * from clubs")) {
		elog("read_clubs: Could not read clubs table: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("read_clubs: Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	while ((row=mysql_fetch_row(result))) {
		if (!row[0] || !row[1] || !row[2] || !row[3] || !row[4]) {
			elog("read_clubs: Incomplete row: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
			continue;
		}

		ID=atoi(row[0]);
		strncpy(name,row[1],75); name[75]=0;
                paid=atoi(row[2]);
		money=atoi(row[3]);
		serial=atoi(row[4]);

                if (ID<1 || ID>=MAXCLUB) {
			elog("read_clubs: got weird ID %d, ignoring",ID);
			continue;
		}
		strcpy(club[ID].name,name);
		club[ID].money=money;
		club[ID].paid=paid;
		club[ID].serial=serial;
		//xlog("got club %d, name %s, paid %d, money %d, serial %d",ID,club[ID].name,club[ID].paid,club[ID].money,club[ID].serial);
	}

	mysql_free_result_cnt(result);
	
	club_update_done=1;
}

void db_update_club(int cnr)
{
	char buf[256];

	sprintf(buf,"update clubs set name='%s', paid=%d, money=%d, serial=%d where ID=%d",club[cnr].name,club[cnr].paid,club[cnr].money,club[cnr].serial,cnr);
	add_query(DT_QUERY,buf,"update club",0);
}

void schedule_clubs(void)
{
	add_query(DT_CLUBS,NULL,"read club",0);
}

static int pvp_counter=0;

void db_new_pvp(void)
{
	pvp_counter++;
}

void db_add_pvp(char *killer,char *victim,char *what,int damage)
{
	char buf[256];
	long long ID;

	ID=pvp_counter+(((long long)realtime)<<32);

	sprintf(buf,"insert pvp values(%lld,\"%s\",\"%s\",\"%s\",%d,%d)",ID,killer,victim,what,damage,realtime);

	add_query(DT_QUERY,buf,"insert pvp",0);
	if (!strcmp(what,"kill")) add_query(DT_PVPLIST,killer,victim,0);
}

void db_pvplist(char *killer,char *victim)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256];
	int cnt;

	// check if this is not the first kill
	sprintf(buf,"select count(*) from pvp where cc=\"%s\" and co=\"%s\" and what='kill' and date>%d",killer,victim,realtime-24*60*60);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed check pvplist: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}
	if (!(result=mysql_store_result_cnt(&mysql))) {
		elog("Failed to store result: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
                return;
	}
	if (!(row=mysql_fetch_row(result))) {
		mysql_free_result_cnt(result);
		return;
	}
	cnt=atoi(row[0]);
        mysql_free_result_cnt(result);
	if (cnt>1) return;

	sprintf(buf,"update pvplist set kills=kills+1 where name=\"%s\"",killer);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed update pvplist: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
	}
        if (mysql_affected_rows(&mysql)==0) {
		sprintf(buf,"insert pvplist values(\"%s\",1,0)",killer);
		if (mysql_query_con(&mysql,buf)) {
			elog("Failed insert pvplist: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
	}
	sprintf(buf,"update pvplist set deaths=deaths+1 where name=\"%s\"",victim);
	if (mysql_query_con(&mysql,buf)) {
		elog("Failed update pvplist: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
	}
        if (mysql_affected_rows(&mysql)==0) {
		sprintf(buf,"insert pvplist values(\"%s\",0,1)",victim);
		if (mysql_query_con(&mysql,buf)) {
			elog("Failed insert pvplist: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		}
	}
}



void add_iplog(int ID,unsigned int ip)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	char buf[256];

	sprintf(buf,"select ID,use_count from iplog where ip=%u and sID=%u",ip&0xffffff00,ID);

	if (mysql_query(&mysql,buf)) {
		elog("insert iplog: select: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

	if (!(result=mysql_store_result(&mysql))) {
		elog("insert iplog: store: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}

        if ((row=mysql_fetch_row(result))) { 	// found suitable entry
		int use_count;
		
		use_count=atoi(row[1]);
                mysql_free_result(result);
		
		sprintf(buf,"update iplog set use_time=%d,use_count=%d where ID=%s",(int)time(NULL),use_count+1,row[0]);
	} else {				// no suitable entry found
		mysql_free_result(result);

		sprintf(buf,"insert into iplog values(0,%u,%d,1,%d)",ip&0xffffff00,(int)time(NULL),ID);
	}

	if (mysql_query(&mysql,buf)) {
		elog("insert/update: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return;
	}
}

int isbanned_iplog(int ip)
{
	MYSQL_RES *result;
	MYSQL_ROW row;
	int flag=0;
	char buf[256];

	sprintf(buf,"select ID from ipban where ip=%u limit 1",ip&0xffffff00);

	if (mysql_query(&mysql,buf)) {
		elog("<p>select: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return 1;
	}

	if (!(result=mysql_store_result(&mysql))) {
		elog("store: Error: %s (%d)",mysql_error(&mysql),mysql_errno(&mysql));
		return 1;
	}

        if ((row=mysql_fetch_row(result))) {
                flag=1;		
	}
	mysql_free_result(result);

	return flag;
}








