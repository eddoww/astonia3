/*
 *  $Id: log.c,v 1.4 2007/07/24 18:55:56 devel Exp $
 *
 * $Log: log.c,v $
 * Revision 1.4  2007/07/24 18:55:56  devel
 * logging complexity and quality for items
 *
 * Revision 1.3  2007/07/13 15:47:16  devel
 * clog -> charlog
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#include "server.h"
#include "player.h"
#include "depot.h"
#include "date.h"
#include "drdata.h"
#include "database.h"
#include "log.h"

static FILE *logfp;
static FILE *infofp;
static FILE *errorfp;

int last_error=0;

static pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;

// info logfile output. works just like printf BUT does NOT need a '\n'.
// it prepends the current date and area number to the output. maximum
// output size is 1024 bytes, but you deserve a spanking anyway if you feed it
// more than 200 bytes.
void ilog(char *format,...)
{
        va_list args;
        char buf[1024];
        struct tm *tm;

	if (!infofp) return;

	pthread_mutex_lock(&log_mutex);
        va_start(args,format);
        vsnprintf(buf,1024,format,args);
        va_end(args);

        tm=localtime((time_t*)&time_now);
        fprintf(infofp,"%02d.%02d.%02d %02d:%02d:%02d [%03d-%02d]: %s\n",
		tm->tm_mday,tm->tm_mon+1,tm->tm_year-100,tm->tm_hour,tm->tm_min,tm->tm_sec,areaID,areaM,
		buf);
        fflush(infofp);
	pthread_mutex_unlock(&log_mutex);
}

// error logfile output. works just like printf BUT does NOT need a '\n'.
// it prepends the current date and area number to the output. maximum
// output size is 1024 bytes, but you deserve a spanking anyway if you feed it
// more than 200 bytes.
void elog(char *format,...)
{
        va_list args;
        char buf[1024];
        struct tm *tm;

	pthread_mutex_lock(&log_mutex);
	last_error=time_now;

        va_start(args,format);
        vsnprintf(buf,1024,format,args);
        va_end(args);

        tm=localtime((time_t*)&time_now);
        fprintf(errorfp,"%02d.%02d.%02d %02d:%02d:%02d [%03d-%02d]: %s\n",
		tm->tm_mday,tm->tm_mon+1,tm->tm_year-100,tm->tm_hour,tm->tm_min,tm->tm_sec,areaID,areaM,
		buf);
        fflush(errorfp);
	pthread_mutex_unlock(&log_mutex);
}

// generic logfile output. works just like printf BUT does NOT need a '\n'.
// it prepends the current date and area number to the output. maximum
// output size is 1024 bytes, but you deserve a spanking anyway if you feed it
// more than 200 bytes.
void xlog(char *format,...)
{
        va_list args;
        char buf[1024];
        struct tm *tm;

	pthread_mutex_lock(&log_mutex);
        va_start(args,format);
        vsnprintf(buf,1024,format,args);
        va_end(args);

        tm=localtime((time_t*)&time_now);
        fprintf(logfp,"%02d.%02d.%02d %02d:%02d:%02d [%03d-%02d]: %s\n",
		tm->tm_mday,tm->tm_mon+1,tm->tm_year-100,tm->tm_hour,tm->tm_min,tm->tm_sec,areaID,areaM,
		buf);
        fflush(logfp);
	pthread_mutex_unlock(&log_mutex);
}

// a logfile entry about a character (cn). works just like printf BUT does NOT need a '\n'.
// it prepends the current date, area number and character ID/name to the output. maximum
// output size is 1024 bytes, but you deserve a spanking anyway if you feed it
// more than 200 bytes.
void charlog(int cn,char *format,...)
{
        va_list args;
        char buf[1024],buf2[1024],*name,pbuf[80];
	int addr,nr;

	if (cn<1 || cn>=MAXCHARS) name="ILLEGAL CN";
	else name=ch[cn].name;

        va_start(args,format);
        vsnprintf(buf,900,format,args);
        va_end(args);

	if ((ch[cn].flags&CF_PLAYER) && (nr=ch[cn].player) && (addr=get_player_addr(nr))) sprintf(pbuf,",IP=%u.%u.%u.%u",(addr>>0)&255,(addr>>8)&255,(addr>>16)&255,(addr>>24)&255);
	else pbuf[0]=0;

        sprintf(buf2,"%s (%d): %s [ID=%d%s]",name,cn,buf,ch[cn].ID,pbuf);

	xlog("%s",buf2);
}

static int logday=-1;

// called at startup
int init_log(void)
{
	char buf[80];
	struct tm *tm;

	if (demon) {
		tm=localtime((time_t*)&time_now);
		logday=tm->tm_mday;
		
		sprintf(buf,"%02d_%02d_%02d.log",tm->tm_year%100,tm->tm_mon+1,tm->tm_mday);
		logfp=fopen(buf,"a");
		if (!logfp) return 0;

                sprintf(buf,"%02d_%02d_%02d.ilog",tm->tm_year%100,tm->tm_mon+1,tm->tm_mday);
		infofp=fopen(buf,"a");
		if (!infofp) return 0;

		sprintf(buf,"%02d_%02d_%02d.elog",tm->tm_year%100,tm->tm_mon+1,tm->tm_mday);
		errorfp=fopen(buf,"a");
		if (!errorfp) return 0;
	} else {
                logfp=stdout;
		infofp=stdout;
		errorfp=stdout;	
	}
	
	return 1;
}

// called on exit
void exit_log(void)
{
	if (demon) {		
		fclose(logfp);
		fclose(infofp);
		fclose(errorfp);
	}
}

void reinit_log(void)
{
	struct tm *tm;
	
	tm=localtime((time_t*)&time_now);
	if (demon && logday!=tm->tm_mday) {
		xlog("switching logfiles - last entry in old log");
		pthread_mutex_lock(&log_mutex);
		exit_log(); init_log();
		pthread_mutex_unlock(&log_mutex);
		xlog("switching logfiles - first entry in new log");
	}
}

void elog_item(int in)
{
	int n;

	if (in<1 || in>=MAXITEM) {
		elog("elog_item failed: illegal item number %d",in);
		return;
	}
	elog("ITEM: name=%s, description=%s",it[in].name,it[in].description);
	elog("ITEM: value=%d, ID=%d, driver=%d",it[in].value,it[in].ID,it[in].driver);

	for (n=0; n<MAXMOD; n++) {
		elog("ITEM: mod %d: index=%d, value=%d",n,it[in].mod_index[n],it[in].mod_value[n]);
	}
}

void log_item(int cn,struct item *in)
{
        ilog("ID=%d: %s eqlog [name=%s, desc=%s, value=%d, sprite=%d, driver=%d, ID=%X, mods=(%d:%d, %d:%d, %d:%d, %d:%d, %d:%d), complex=%d, quality=%d]",
   	     ch[cn].ID,
	     ch[cn].name,
	     in->name,
	     in->description,
	     in->value,
	     in->sprite,
	     in->driver,
	     in->ID,
	     in->mod_index[0],in->mod_value[0],
	     in->mod_index[1],in->mod_value[1],
	     in->mod_index[2],in->mod_value[2],
	     in->mod_index[3],in->mod_value[3],
	     in->mod_index[4],in->mod_value[4],
	     in->complexity,
	     in->quality);
}

void log_items(int cn)
{
	int n,a,b;
	struct depot_ppd *ppd;

	// i assume there are more elegant ways to get one log output per day, but this one should work...
	a=ch[cn].login_time/(60*60*24);
	b=realtime/(60*60*24);
        if (b<=a) return;

	ilog("ID=%d: %s eqlog worn and inventory:",ch[cn].ID,ch[cn].name);
        for (n=0; n<INVENTORYSIZE; n++) {
		if (n>11 && n<30) continue;	// dont log spells
		if (ch[cn].item[n]) log_item(cn,it+ch[cn].item[n]);
	}

	ilog("ID=%d: %s eqlog depot:",ch[cn].ID,ch[cn].name);
	if ((ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd)))) {
		for (n=0; n<MAXDEPOT; n++) {
			if (ppd->itm[n].flags) log_item(cn,ppd->itm+n);
		}
	}
}
