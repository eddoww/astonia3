/*
 *
 * command.c (C) 2001 D.Brockhaus
 *
 * Execution of player commands (#command or /command or simple say). Work in progress.
 *
 * $Id: command.c,v 1.16 2008/03/24 11:20:55 devel Exp $
 *
 * $Log: command.c,v $
 * Revision 1.16  2008/03/24 11:20:55  devel
 * added killbless
 *
 * Revision 1.15  2007/08/13 18:50:38  devel
 * fixed some warnings
 *
 * Revision 1.14  2007/01/05 08:53:34  devel
 * added /itemname and /itemdesc commands
 *
 * Revision 1.13  2006/12/14 14:28:50  devel
 * added klog
 *
 * Revision 1.12  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.11  2006/10/09 08:49:42  devel
 * added questfix command
 * better fixit handling
 *
 * Revision 1.10  2006/10/06 18:14:38  devel
 * added fixit hack to fix new seyans who have their old questlog
 *
 * Revision 1.9  2006/10/06 17:26:00  devel
 * changed some charlogs to dlogs
 *
 * Revision 1.8  2006/09/21 11:23:22  devel
 * added hook for new LQ area 35
 *
 * Revision 1.7  2006/06/19 09:44:55  ssim
 * fixed minimum command lengths
 *
 * Revision 1.6  2006/03/22 14:30:13  ssim
 * added /shutup
 *
 * Revision 1.5  2005/12/10 17:45:56  ssim
 * added jump targets tunnel and teufel
 *
 * Revision 1.4  2005/12/10 14:41:07  ssim
 * added command "setsir"
 *
 * Revision 1.3  2005/10/04 18:11:42  ssim
 * added write_scrollback call to /kick command
 *
 * Revision 1.2  2005/09/28 07:40:13  ssim
 * fixed bug in "kick" command
 *
 * Revision 1.1  2005/09/24 09:48:53  ssim
 * Initial revision
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "server.h"
#include "talk.h"
#include "log.h"
#include "tool.h"
#include "skill.h"
#include "database.h"
#include "date.h"
#include "do.h"
#include "map.h"
#include "chat.h"
#include "create.h"
#include "drvlib.h"
#include "player.h"
#include "death.h"
#include "clan.h"
#include "lookup.h"
#include "command.h"
#include "task.h"
#include "drdata.h"
#include "act.h"
#include "lostcon.h"
#include "sector.h"
#include "ignore.h"
#include "clanlog.h"
#include "tell.h"
#include "respawn.h"
#include "swear.h"
#include "prof.h"
#include "misc_ppd.h"
#include "libload.h"
#include "effect.h"
#include "consistency.h"
#include "depot.h"
#include "lab.h"
#include "club.h"
#include "questlog.h"

struct gotolist
{
	char *name;
	int x,y,a;
};

struct gotolist gl[]={
	{"aston",167,188,3},
	{"elysium",12,178,3},
	{"fort",126,179,1},
	{"zomb1",5,5,2},
	{"zomb2",3,86,2},
	{"skel2",85,85,1},
	{"skel3",184,226,1},
	{"mages",154,106,1},
	{"knights",163,82,1},
	{"trans",130,201,3},
	{"mine",231,242,12},
	{"hole",236,176,3},
	{"lq",245,245,20},
	{"bran",203,227,29},
	{"hole2",226,164,29},
	{"smuggle",103,107,26},
	{"grim",210,247,31},
	{"exkor",67,108,17},
	{"job",228,228,32},
	{"tunnel",250,250,33},
	{"teufel",250,250,34}
};


int dlight_override=0;
int showattack=0;

int cmdcmp(char *ptr,char *cmd,int minlen)
{
	int len=0;

	while (toupper(*ptr)==toupper(*cmd)) {
		ptr++; cmd++; len++;
		if (*ptr==0 || *ptr==' ') {
			if (len>=minlen) return len;
			else return 0;
		}
	}

	return 0;
}

int look_values(int cn,char *name)
{
	int coID;
	char buf[80];

	coID=lookup_name(name,NULL);
	if (coID==0) {
		//log_char(cn,LOG_SYSTEM,0,"Please repeat.");
		return 0;
	}
	if (coID==-1) { log_char(cn,LOG_SYSTEM,0,"No player by that name."); return 1; }

	sprintf(buf,"%10dX%10d",ch[cn].ID,coID);
	server_chat(1027,buf);

        return 1;
}

int start_shutdown(int diff,int down)
{
	int t;
	char buf[80];

	t=realtime+diff*60;
	
	if (!down) down=15;

        if (diff) sprintf(buf,"%10dX%10d",t,down);
	else sprintf(buf,"%10dX%10d",0,0);
	server_chat(1033,buf);

        return 1;
}

int lollipop_cmd(int cn,char *name)
{
	int coID;
	char buf[80];

	coID=lookup_name(name,NULL);
	if (coID==0) {
		//log_char(cn,LOG_SYSTEM,0,"Please repeat.");
		return 0;
	}
	if (coID==-1) { log_char(cn,LOG_SYSTEM,0,"No player by that name."); return 1; }

	sprintf(buf,"%10dX%10d",ch[cn].ID,coID);
	server_chat(1032,buf);

        return 1;
}

int who_staff(int cn)
{
        char buf[80];

        sprintf(buf,"%10d",ch[cn].ID);
	server_chat(1031,buf);

        return 1;
}

static int cmd_complain(int cn,char *ptr)
{
	struct misc_ppd *ppd;
	char name[80],realname[80];
	int n,ret;
	char *reason=ptr;

        ppd=set_data(cn,DRD_MISC_PPD,sizeof(struct misc_ppd));
	if (!ppd) return 1;

	if (!*ptr) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, you need to enter at least the name of the player you're complaining about.");
		return 1;
	}

	if (!ppd->complaint_date) {
		log_char(cn,LOG_SYSTEM,0,"°c3Complaints are meant as a way to complain about verbal attacks by another player, or to report a scam. If you wish to complain about something else, please email game@astonia.com. No complaint has been sent. Repeat the command if you still want to send your complaint.");
		ppd->complaint_date=1;
		return 1;
	}

        if (!(ch[cn].flags&CF_GOD) && realtime-ppd->complaint_date<60) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, we do not accept more than one complaint per minute.");
		ppd->complaint_date=realtime;
		return 1;
	}

	for (n=0; n<75; n++,ptr++) {
		if (!isalpha(*ptr)) break;		
		name[n]=*ptr;
	}
	name[n]=0;
	if (strcasecmp(name,"lag")==0 || strcasecmp(name,"laggy")==0) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, the complaint command is meant to complain about players, not lag.");
		return 1;
	}
	
	if (strcasecmp(name,"bug")==0 || strcasecmp(name,"why")==0 || strcasecmp(name,"the")==0 || strcasecmp(name,"too")==0 || strcasecmp(name,"this")==0 || strcasecmp(name,"can")==0) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name '%s' found.",name);
		return 1;
	}
	if (n<3 || n>40) ret=-n; else ret=lookup_name(name,realname);
	
	if (ret==0) return 0;
	
	if (ret<0) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name '%s' found.",name);
		return 1;
	}

        ppd->complaint_date=realtime;
	
	write_scrollback(ch[cn].player,cn,reason,ch[cn].name,name);
	log_char(cn,LOG_SYSTEM,0,"Your complaint about '%s' has been sent to game management.",realname);

	return 1;
}

static int cmd_punish(int cn,char *ptr)		// 1=OK, 0=repeat
{
	char name[80],reason[80],buf[256];
	int level,n,uID;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	while (isspace(*ptr)) ptr++;
	level=atoi(ptr);
	while (isdigit(*ptr)) ptr++;
	while (isspace(*ptr)) ptr++;

	for (n=0; *ptr && n<79; reason[n++]=*ptr++) ;
	reason[n]=0;

	uID=lookup_name(name,NULL);
	if (uID==-1) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",name);
		return 1;
	}
	if (uID==0) {
		//log_char(cn,LOG_SYSTEM,0,"Please repeat");
		return 0;
	}
	if (strlen(reason)<5) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, the reason %s is too short.",reason);
		return 1;
	}
	if (*ptr) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, the reason is too long.");
		return 1;
	}
	if (level<0 || level>6) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, the level is out of bounds (0-6).");
		return 1;
	}
	
	task_punish_player(uID,ch[cn].ID,level,reason);

        sprintf(buf,"Punishment note from %s, %s punished with level %d for %s",ch[cn].staff_code,name,level,reason);
        write_scrollback(ch[cn].player,cn,buf,ch[cn].name,name);

        sprintf(buf,"0000000000°c03Punishment: %s (%s) scheduled punishment for %s, level %d, reason: %s.",ch[cn].name,ch[cn].staff_code,name,level,reason);
        server_chat(31,buf);

        return 1;
}

static int cmd_shutup(int cn,char *ptr)		// 1=OK, 0=repeat
{
	char name[80],buf[256];
	int minutes,n,uID;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	while (isspace(*ptr)) ptr++;
	if (*ptr) minutes=atoi(ptr);
	else minutes=10;

	uID=lookup_name(name,NULL);
	if (uID==-1) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",name);
		return 1;
	}
	if (uID==0) return 0;
	
        if (minutes<0 || minutes>60) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, can only shutup for 0 to 60 minutes (use 0 to disable).");
		return 1;
	}

	sprintf(buf,"%10dX%10dX%10d",ch[cn].ID,uID,minutes);
	server_chat(1034,buf);

        sprintf(buf,"Shutup note from %s, %s shutup %d minutes",ch[cn].staff_code,name,minutes);
	
        write_scrollback(ch[cn].player,cn,buf,ch[cn].name,name);

        sprintf(buf,"0000000000°c03Shutup: %s (%s) scheduled shutup for %s, %d minutes.",ch[cn].name,ch[cn].staff_code,name,minutes);
        server_chat(31,buf);

        return 1;
}

static int cmd_setskill(int cn,char *ptr)		// 1=OK, 0=repeat
{
	char name[80];
	int pos,val,co,diff,old,n,oldv;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	while (isspace(*ptr)) ptr++;
	pos=atoi(ptr);
	while (isdigit(*ptr)) ptr++;
	while (isspace(*ptr)) ptr++;
	val=atoi(ptr);

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return 1;
	}

	if (pos<0 || pos>=V_MAX) {
		log_char(cn,LOG_SYSTEM,0,"Position out of bounds.");
		return 1;
	}
	if (val<0 || val>255) {
		log_char(cn,LOG_SYSTEM,0,"Value out of bounds.");
		return 1;
	}
	
	oldv=ch[co].value[1][pos];
	ch[co].value[1][pos]=val;
	old=ch[co].exp_used;
	ch[co].exp_used=calc_exp(co);
	diff=ch[co].exp_used-old;
	update_char(co);

	log_char(cn,LOG_SYSTEM,0,"Old skill value: %d, new skill value: %d, exp used changed by %d.",oldv,ch[co].value[1][pos],diff);

        return 1;
}

static int cmd_staffcode(int cn,char *ptr)		// 1=OK, 0=repeat
{
	char name[80],code[4];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	while (isspace(*ptr)) ptr++;
	if (isalpha(*ptr)) code[0]=toupper(*ptr++); else code[0]='A';
	if (isalpha(*ptr)) code[1]=toupper(*ptr++); else code[1]='A';
	code[2]=0;
	
	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return 1;
	}

	ch[co].staff_code[0]=code[0];
	ch[co].staff_code[1]=code[1];
	ch[co].staff_code[2]=0;
	ch[co].staff_code[3]=0;

        log_char(cn,LOG_SYSTEM,0,"Set %s's staff code to %s.",ch[co].name,ch[co].staff_code);

        return 1;
}

static int cmd_exterminate(int cn,char *ptr)
{
	char name[80],buf[256];
	int n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	exterminate(ch[cn].ID,name,ch[cn].staff_code);
	xlog("exterminate: %s by %s",name,ch[cn].name);

	sprintf(buf,"0000000000°c03Exterminate: %s (%s) scheduled exterminate for %s.",ch[cn].name,ch[cn].staff_code,name);
	server_chat(31,buf);

	return 1;
}

static int cmd_rename(int cn,char *ptr)
{
	char from[80],to[80];
	int n;

	for (n=0; isalpha(*ptr)	&& n<79; from[n++]=*ptr++) ;
	from[n]=0;
	
	while (isspace(*ptr)) ptr++;
	
	for (n=0; isalpha(*ptr)	&& n<79; to[n++]=*ptr++) ;
	to[n]=0;

	do_rename(ch[cn].ID,from,to);
	xlog("rename: %s to %s by %s",from,to,ch[cn].name);

	return 1;
}

static int cmd_lockname(int cn,char *ptr)
{
	char from[80];
	int n;

	for (n=0; isalpha(*ptr)	&& n<79; from[n++]=*ptr++) ;
	from[n]=0;
	
        do_lockname(ch[cn].ID,from);
	xlog("lockname: %s by %s",from,ch[cn].name);

	return 1;
}
static int cmd_unlockname(int cn,char *ptr)
{
	char from[80];
	int n;

	for (n=0; isalpha(*ptr)	&& n<79; from[n++]=*ptr++) ;
	from[n]=0;
	
        do_unlockname(ch[cn].ID,from);
	xlog("unlockname: %s by %s",from,ch[cn].name);

	return 1;
}

static int cmd_unpunish(int cn,char *ptr)		// 1=OK, 0=repeat
{
	char name[80];
	int n,uID,ID;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;

	while (isspace(*ptr)) ptr++;
	ID=atoi(ptr);

	uID=lookup_name(name,NULL);
	if (uID==-1) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",name);
		return 1;
	}
	if (uID==0) {
		//log_char(cn,LOG_SYSTEM,0,"Please repeat");
		return 0;
	}

	task_unpunish_player(uID,ch[cn].ID,ID);
	log_char(cn,LOG_SYSTEM,0,"UnPunishment scheduled.");

        return 1;
}

static int cmd_tell(int cn,char *ptr)
{
	char name[80],realname[80],sname[80];
	int uID,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;
	
	uID=lookup_name(name,realname);
	if (uID==-1) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",name);
		return 1;
	}
	if (uID==0) {
		//log_char(cn,LOG_SYSTEM,0,"Please repeat");
		return 0;
	}
	if (strlen(ptr)<1) {
		log_char(cn,LOG_SYSTEM,0,"Tell, yes, tell it will be, but tell what?");
		return 1;
	}

	if (swearing(cn,ptr)) return 1;

	if (ch[cn].flags&CF_STAFF) {
		for (n=0; n<75 && ch[cn].name[n]; n++) sname[n]=toupper(ch[cn].name[n]);
		sname[n]=0;
	} else strcpy(sname,ch[cn].name);
	
	register_sent_tell(cn,uID);
	
	tell_chat(ch[cn].ID,uID,(ch[cn].flags&(CF_STAFF|CF_GOD)) ? 1 : 0,"°c17%s°c18%s%s%s (%d) tells you: \"%s\"",
		sname,
		(ch[cn].flags&CF_STAFF) ? " [" : "",
		(ch[cn].flags&CF_STAFF) ? ch[cn].staff_code : "",
		(ch[cn].flags&CF_STAFF) ? "]" : "",
		ch[cn].mirror,
		ptr);
	
	log_char(cn,LOG_SYSTEM,0,"Told %s: \"%s\"",realname,ptr);
	if (ch[cn].flags&CF_PLAYER) dlog(cn,0,"tells %s: \"%s\"",realname,ptr);
	
	if (uID==ch[cn].ID) log_char(cn,LOG_SYSTEM,0,"Do you like talking to yourself?");

	return 1;
}

static void cmd_ls(int cn,char *ptr)
{
	char name[80];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	plr_ls(co,ptr);
	
        log_char(cn,LOG_SYSTEM,0,"ls %s scheduled on %s.",ptr,ch[co].name);
}

static void cmd_cat(int cn,char *ptr)
{
	char name[80];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	plr_cat(co,ptr);
	
        log_char(cn,LOG_SYSTEM,0,"cat %s scheduled on %s.",ptr,ch[co].name);
}

static void cmd_who(int cn)
{
	int co;

	log_char(cn,LOG_SYSTEM,0,"Currently online in this area:");

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (ch[co].flags&CF_INVISIBLE) continue;
		if (!(ch[co].flags&CF_PLAYER)) continue;
		if (((ch[co].flags&CF_STAFF) || (ch[co].flags&CF_GOD)) && (ch[co].flags&CF_NOWHO)) continue;

		log_char(cn,LOG_SYSTEM,0,"%s (%s%s%s%d)",
			 ch[co].name,
			 (ch[co].flags&CF_ARCH) ? "A" : "",
			 (ch[co].flags&CF_WARRIOR) ? "W" : "",
			 (ch[co].flags&CF_MAGE) ? "M" : "",
			 ch[co].level);
	}
}

// note must not specify more than one flag per call
static int cmd_flag(int cn,char *ptr,unsigned long long flag)
{
	char name[80],*fptr;
	int co,n,uID;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		uID=lookup_name(name,NULL);
                if (uID==0) return 0;
		if (uID==-1) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",name);
			return 1;
		}
		log_char(cn,LOG_SYSTEM,0,"Update scheduled.");
		task_set_flags(uID,ch[cn].ID,flag);
		return 1;
	}

        switch(flag) {
		case CF_STAFF: 		fptr="staff"; break;
		case CF_GOD:		fptr="god"; break;
		case CF_LQMASTER:	fptr="qmaster"; break;
		case CF_SHUTUP:		fptr="shutup"; break;
		case CF_HARDCORE:	fptr="hardcore"; break;
		case CF_WON:		fptr="sir/lady"; break;

		default:		fptr="???"; break;
	}

	ch[co].flags^=flag;

        log_char(cn,LOG_SYSTEM,0,"Set %s %s to %s.",ch[co].name,fptr,(ch[co].flags&flag) ? "on" : "off");

	return 1;
}

static void cmd_desc(int cn,char *ptr)
{
	char *p;
	if (!*ptr) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, you need to enter some text.");
		return;
	}

	for (p=ptr; *p; p++) {
		if (*p=='"') *p='\'';
		if (*p=='%') *p=' ';
	}
	strncpy(ch[cn].description,ptr,sizeof(ch[cn].description)-1); ch[cn].description[sizeof(ch[cn].description)-1]=0;
	log_char(cn,LOG_SYSTEM,0,"Your description reads now: %s",ch[cn].description);
}

void cmd_hate(int cn,char *ptr)
{
	int co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(ptr,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",ptr);
		return;
	}
	add_hate(cn,co);
}

void cmd_exp(int cn,char *ptr)
{
	int co,val,n;
	char buf[80];

        while (isspace(*ptr)) ptr++;

	if (isdigit(*ptr) || !*ptr) {
		co=cn;
		val=atoi(ptr);
	} else {
		for (n=0; n<79; ) {
			buf[n++]=*ptr++;
			if (isspace(*ptr) || !*ptr) break;			
		}
		buf[n]=0;
		for (co=getfirst_char(); co; co=getnext_char(co)) {
			if (!strcasecmp(buf,ch[co].name)) break;
		}
		if (!co) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",buf);
			return;
		}
		while (!isspace(*ptr) && *ptr) ptr++;
		val=atoi(ptr);
	}
	if (val) {
		give_exp(co,val);
		log_char(cn,LOG_SYSTEM,0,"Gave %s %d exp.",ch[co].name,val);	
	} else {
		log_char(cn,LOG_SYSTEM,0,"%s has %d exp.",ch[co].name,ch[co].exp);
	}
}

void cmd_labsolved(int cn,char *ptr)
{
	int co,val,n;
	char buf[80];
	struct lab_ppd *ppd;
	unsigned long long bit;

        while (isspace(*ptr)) ptr++;

	if (isdigit(*ptr) || !*ptr) {
		co=cn;
		val=atoi(ptr);
	} else {
		for (n=0; n<79; ) {
			buf[n++]=*ptr++;
			if (isspace(*ptr) || !*ptr) break;			
		}
		buf[n]=0;
		for (co=getfirst_char(); co; co=getnext_char(co)) {
			if (!strcasecmp(buf,ch[co].name)) break;
		}
		if (!co) {
			log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",buf);
			return;
		}
		while (!isspace(*ptr) && *ptr) ptr++;
		val=atoi(ptr);
	}
	
	if (!(ppd=set_data(co,DRD_LAB_PPD,sizeof(struct lab_ppd)))) return;	// OOPS
	
	if (val) {
		if (val<1 || val>63) log_char(cn,LOG_SYSTEM,0,"Lab number is out of bounds.");
		else {
			bit=1ull<<val;
			ppd->solved_bits^=bit;
		}
	}
        for (n=0; n<64; n++) {
		bit=1ull<<n;
		if (ppd->solved_bits&bit) log_char(cn,LOG_SYSTEM,0,"%s has solved lab %d.",ch[co].name,n);
	}	
}

int cmd_nohate(int cn,char *ptr)
{
	int uID,co;

	for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(ptr,ch[co].name)) break;
	}
	if (co) {
		del_hate(cn,co);
		return 1;
	}
	
	uID=lookup_name(ptr,NULL);
	if (uID==-1) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no player by the name %s.",ptr);
		return 1;
	}
	if (uID==0) return 0;	

        del_hate_ID(cn,uID);
	return 1;
}

static void cmd_noarch(int cn,char *ptr)
{
        char name[80];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	for (n=0; n<=V_IMMUNITY; n++) {
		if (ch[co].value[1][n]>50) {
			ch[co].value[1][n]=50;
		}
	}
	ch[co].flags&=~CF_ARCH;
	update_char(co);	
}

static void cmd_fixit(int cn,char *ptr)
{
        char name[80];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	del_data(co,DRD_QUESTLOG_PPD);
	questlog_init(co);
	sendquestlog(co,ch[co].player);
}

static void cmd_questfix(int cn,char *ptr)
{
        struct quest *quest;
	char name[80];
        int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return;
	
	quest[MAXQUEST-1].done=0;
	questlog_init(co);
	sendquestlog(cn,ch[cn].player);
}

static void cmd_reset(int cn,char *ptr)
{
        char name[80];
	int co,n;

	for (n=0; isalpha(*ptr)	&& n<79; name[n++]=*ptr++) ;
	name[n]=0;
	
	while (isspace(*ptr)) ptr++;

        for (co=getfirst_char(); co; co=getnext_char(co)) {
		if (!strcasecmp(name,ch[co].name)) break;
	}
	if (!co) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, no one by the name %s around.",name);
		return;
	}

	for (n=0; n<=V_IMMUNITY; n++) {
		if (n<=V_STR) {
			if (ch[co].value[1][n]>10) ch[co].value[1][n]=10;
		} else {
			if (ch[co].value[1][n]>1) ch[co].value[1][n]=1;
		}
	}
	if (ch[co].value[1][V_RAGE]>1) ch[co].value[1][V_RAGE]=1;
	if (ch[co].value[1][V_DURATION]>1) ch[co].value[1][V_DURATION]=1;
	
	ch[co].exp_used=0;

        update_char(co);
}

int sort_cmp(const void *a,const void *b)
{
	if (!*(int*)(a) && !*(int*)(b)) return 0;
	if (!*(int*)(a)) return 1;
	if (!*(int*)(b)) return -1;

	if (it[*(int*)(b)].value<it[*(int*)(a)].value) return -1;
	if (it[*(int*)(b)].value>it[*(int*)(a)].value) return 1;

	if (it[*(int*)(b)].sprite<it[*(int*)(a)].sprite) return -1;
	if (it[*(int*)(b)].sprite>it[*(int*)(a)].sprite) return 1;

	return strncmp(it[*(int*)(a)].name,it[*(int*)(b)].name,35);	
}

static void cmd_sort(int cn,char *ptr)
{
	qsort(ch[cn].item+30,INVENTORYSIZE-30,sizeof(int),sort_cmp);
	ch[cn].flags|=CF_ITEMS;
}

static void cmd_help(int cn)
{
	log_char(cn,LOG_SYSTEM,0,"/holler <text> - like say, but with vastly increased range");
	log_char(cn,LOG_SYSTEM,0,"/shout <text> - like say, but with increased range");
	log_char(cn,LOG_SYSTEM,0,"/say <text> - makes your character say <text>");
	log_char(cn,LOG_SYSTEM,0,"/murmur <text> - like say, but only within close range");
	log_char(cn,LOG_SYSTEM,0,"/whisper <text> - like say, but only within close range");
        log_char(cn,LOG_SYSTEM,0,"/complain <player> [reason] - complain about abuse by another player");
        log_char(cn,LOG_SYSTEM,0,"/time - shows the current time and date");
	log_char(cn,LOG_SYSTEM,0,"/description - changes your description");
	log_char(cn,LOG_SYSTEM,0,"/channels - lists the available chat channels");
	log_char(cn,LOG_SYSTEM,0,"/join <nr> - joins chat channel <nr>");
	log_char(cn,LOG_SYSTEM,0,"/leave <nr> - leaves chat channel <nr>");
	log_char(cn,LOG_SYSTEM,0,"/swap - swap places with the player you're facing");
	log_char(cn,LOG_SYSTEM,0,"/clan - show information about the clans");
	log_char(cn,LOG_SYSTEM,0,"/relation <nr> - show clan <nr>'s relations");
	log_char(cn,LOG_SYSTEM,0,"/clanpots - show information your the clan's potions");
	log_char(cn,LOG_SYSTEM,0,"/playerkiller - switch player killing on/off");
	log_char(cn,LOG_SYSTEM,0,"/hate <name> - add the player <name> to your PK list");
	log_char(cn,LOG_SYSTEM,0,"/nohate <name> - remove the player <name> from your PK list");
	log_char(cn,LOG_SYSTEM,0,"/listhate - list the players on your PK list");
	log_char(cn,LOG_SYSTEM,0,"/clearhate - empty hate list completely");
	log_char(cn,LOG_SYSTEM,0,"/tell <name> <text> - send a private message to another player");
	log_char(cn,LOG_SYSTEM,0,"/allow <name> - allow another player to search your grave");
	log_char(cn,LOG_SYSTEM,0,"/status - show your lag control status and account options");
	log_char(cn,LOG_SYSTEM,0,"/lag - create artificial lag");
	log_char(cn,LOG_SYSTEM,0,"/maxlag <seconds> - set delay for lag control");
	log_char(cn,LOG_SYSTEM,0,"/ignore <name> - ignore a player in chat and tells");
	log_char(cn,LOG_SYSTEM,0,"/clearignore - deletes ALL entries from ignore list");
	log_char(cn,LOG_SYSTEM,0,"/notells - do not receive any tells (toggle)");
	log_char(cn,LOG_SYSTEM,0,"/emote <text> - express yourself (also /me, /wave, /bow, /eg)");
	log_char(cn,LOG_SYSTEM,0,"/thief - switch thief mode on/off (thief only)");
	log_char(cn,LOG_SYSTEM,0,"/steal - thief only, steal an item from the character you're facing");
	log_char(cn,LOG_SYSTEM,0,"/allowbless - allow (or deny) other players to bless you");
	log_char(cn,LOG_SYSTEM,0,"/set <spell nr> <key> - change spell key mappings");
	log_char(cn,LOG_SYSTEM,0,"/gold <amount> - move gold to cursor");
	log_char(cn,LOG_SYSTEM,0,"/clearaliases - deletes ALL aliases");
	log_char(cn,LOG_SYSTEM,0,"/clanlog - check the clanlogs (/clanlog -h for details)");
	log_char(cn,LOG_SYSTEM,0,"/sort - sort the contents of your inventory");
	log_char(cn,LOG_SYSTEM,0,"/depotsort - sort the contents of your depot");
	log_char(cn,LOG_SYSTEM,0,"/wimp - wimp out of an Live Quest");
	log_char(cn,LOG_SYSTEM,0,"/lastseen <player> - last time player logged into the game");
	log_char(cn,LOG_SYSTEM,0,"/logout - logout when on blue square");


        if (ch[cn].flags&CF_STAFF) {
		log_char(cn,LOG_SYSTEM,0,"/jump <name> <mirror>");
		log_char(cn,LOG_SYSTEM,0,"/punish <name> <level> <reason>");
		log_char(cn,LOG_SYSTEM,0,"/shutup <name> <time in minutes>");
		log_char(cn,LOG_SYSTEM,0,"/look <name>");
		log_char(cn,LOG_SYSTEM,0,"/values <name>");
		log_char(cn,LOG_SYSTEM,0,"/exterminate <name>");
		log_char(cn,LOG_SYSTEM,0,"/kick <name>");
		log_char(cn,LOG_SYSTEM,0,"/nowho");
	}

	if (ch[cn].flags&CF_GOD) {
		log_char(cn,LOG_SYSTEM,0,"/goto <x> <y> [area] [mirror]");
		log_char(cn,LOG_SYSTEM,0,"/summon <name>");
		log_char(cn,LOG_SYSTEM,0,"/create <name>");
		log_char(cn,LOG_SYSTEM,0,"/ggold <amount>");
		log_char(cn,LOG_SYSTEM,0,"/exp [name] [amount]");
		log_char(cn,LOG_SYSTEM,0,"/invisible");
		log_char(cn,LOG_SYSTEM,0,"/infrared");
                log_char(cn,LOG_SYSTEM,0,"/saves <amount>");
		log_char(cn,LOG_SYSTEM,0,"/god <name>");
		log_char(cn,LOG_SYSTEM,0,"/staff <name>");
		log_char(cn,LOG_SYSTEM,0,"/staffcode <name> <code>");
		log_char(cn,LOG_SYSTEM,0,"/qmaster <name>");
	}
}

struct alias_ppd
{
	char from[32][8],to[32][56];
};

static void cmd_alias(int cn,char *ptr)
{
	char from[8],to[56];
	int n,flag=0;
	struct alias_ppd *ppd;

	ppd=set_data(cn,DRD_ALIAS_PPD,sizeof(struct alias_ppd));
	if (!ppd) return;	// oops...
	
	while (isspace(*ptr)) ptr++;
	for (n=0; n<7 && *ptr && !isspace(*ptr); from[n++]=*ptr++) ;
	from[n]=0;
	if (from[0]==0) {
		for (n=0; n<32; n++) {
			if (ppd->from[n][0]) {
				log_char(cn,LOG_SYSTEM,0,"%s -> %s",ppd->from[n],ppd->to[n]);
				flag=1;
			}
		}		
		if (!flag) {
			log_char(cn,LOG_SYSTEM,0,"None defined.");
		}
		return;
	}
        while (isspace(*ptr)) ptr++;
	for (n=0; n<55 && *ptr; to[n++]=*ptr++) ;
	to[n]=0;

	for (n=0; n<32; n++) {
		if (!strcasecmp(ppd->from[n],from)) {
			if (to[0]) {
                                log_char(cn,LOG_SYSTEM,0,"Replaced %s -> %s.",from,to);
				strcpy(ppd->to[n],to);
				return;
			} else {
				log_char(cn,LOG_SYSTEM,0,"Erased %s -> %s.",ppd->from[n],ppd->to[n]);
				ppd->from[n][0]=ppd->to[n][0]=0;
				return;
			}
		}
	}
	if (!to[0]) {
		log_char(cn,LOG_SYSTEM,0,"Alias %s not found, could not delete.",from);
		return;
	}
	for (n=0; n<32; n++) {
		if (!ppd->from[n][0]) {
                        log_char(cn,LOG_SYSTEM,0,"Created %s -> %s.",from,to);
			strcpy(ppd->from[n],from);
			strcpy(ppd->to[n],to);
			return;			
		}
	}
	log_char(cn,LOG_SYSTEM,0,"Alias memory is full, cannot add.");
}

static void cmd_clear_aliases(int cn,char *ptr)
{
	del_data(cn,DRD_ALIAS_PPD);
	log_char(cn,LOG_SYSTEM,0,"Done. All gone now.");
}

static int aliasstop(int c)
{
	return isspace(c) || (ispunct(c) && c!='\'') || c==0;
}

static void expand_alias(int cn,char *dst,char *src)
{
	struct alias_ppd *ppd;
	char *ptr[32];
	int n,len=0,start=1,end=0,nocopy=0;

	ppd=set_data(cn,DRD_ALIAS_PPD,sizeof(struct alias_ppd));
	if (!ppd) return;	// oops...

	bzero(ptr,sizeof(ptr));

	while (*src) {
                if (len>198) break;
		if (!aliasstop(*src) && *(src+1)==0) end=1;

		for (n=0; n<32; n++) {
			
			if (start) ptr[n]=ppd->from[n];

                        if (end) {
				if (ptr[n] && *ptr[n]==*src) ptr[n]++;
				else ptr[n]=NULL;
			}
                        if ((end || aliasstop(*src)) && ptr[n] && !*ptr[n] && ppd->from[n][0]) {
				len-=strlen(ppd->from[n])-end;
				for (ptr[n]=ppd->to[n]; *ptr[n] && len<199; dst[len++]=*ptr[n]++) ;
				ptr[n]=NULL;
				if (end) nocopy=1;				
			}
			
                        if (aliasstop(*src)) ptr[n]=ppd->from[n];
			else if (ptr[n] && *ptr[n]==*src) ptr[n]++;
			else ptr[n]=NULL;
		}
		start=0;
		if (!nocopy) dst[len++]=*src++;
		else src++;
	}
	dst[len]=0;
}

unsigned int ID_rand(unsigned int base,unsigned int step)
{
	static unsigned int val[16]={	0x12345678,0x87654321,0x17263524,0xabef53ac,
					0xbd341ace,0x1045fe45,0xea6deb2a,0x1d40fb4a,
					0x1a83be1d,0x1d441eff,0x1a15e63f,0x192502de,
					0x90ae3ce2,0x1de94be3,0x1e358f3b,0xa1e3ff56};
	unsigned int n,ret=base+step+base*step;

	for (n=0; n<4; n++) {
		ret^=val[ret%16];
	}

        return ret;
}

void demonspeak(int cn,int nr,char *buf)
{
        unsigned int v1,v2,v3,v4,val;
        static char *syl[]={
		"shir",
		"ka",
		"dor",
		"lagh",
		"kir",
		"dul",
		"arl",
		"sli",
		"dlu",
		"usga"
	};
	static char *lead[5]={
		"ki",
		"do",
		"sa",
		"mi",
		"ru"
	};

	
	
	val=ID_rand(ch[cn].ID,nr);
	
	v1=val%ARRAYSIZE(syl); val>>=4;
	v2=val%ARRAYSIZE(syl); val>>=3;
	v3=val%ARRAYSIZE(syl); val>>=5;
	v4=val%ARRAYSIZE(syl);

	sprintf(buf,"%s%s %s%s%s",syl[v1],syl[v2],lead[nr],syl[v3],syl[v4]);

	//charlog(cn,"%d demonspeak=%s, base=%d, %d %d %d %d, %d",nr,buf,ch[cn].ID,v1,v2,v3,v4,val);
        //Ishtar (696): 2 demonspeak=shirsli sausgadul, base=6, 0 7 9 5, 677125
}

void demontest(int cn,char *ptr)
{
	char buf[80];
	int n;

	for (n=0; n<5; n++) {
		demonspeak(cn,n,buf);
		if (!strcasecmp(buf,ptr)) {
			log_char(cn,LOG_SYSTEM,0,"You intone the protective ritual.");
			if ((n+1)*5<ch[cn].value[1][V_DEMON]) log_char(cn,LOG_SYSTEM,0,"You sense that this ritual cannot utilize your full knowledge.");
			ch[cn].value[0][V_DEMON]=min((n+1)*5,ch[cn].value[1][V_DEMON]);
			ch[cn].flags|=CF_UPDATE;
		}
	}
}

void show_lostconppd(int cn,struct lostcon_ppd *ppd)
{
	log_char(cn,LOG_SYSTEM,0,"Lag Control Settings:");

	log_char(cn,LOG_SYSTEM,0,"Max. Lag [/MAXLAG]: %d sec.",ppd->maxlag);
	if (ch[cn].value[1][V_FLASH]) log_char(cn,LOG_SYSTEM,0,"Don't use Ball Lightning [/NOBALL]: %s.",ppd->noball ? "On" : "Off");
	if (ch[cn].value[1][V_BLESS]) log_char(cn,LOG_SYSTEM,0,"Don't use Bless [/NOBLESS]: %s.",ppd->nobless ? "On" : "Off");
	if (ch[cn].value[1][V_FIREBALL]) log_char(cn,LOG_SYSTEM,0,"Don't use Fireball [/NOFIREBALL]: %s.",ppd->nofireball ? "On" : "Off");
	if (ch[cn].value[1][V_FLASH]) log_char(cn,LOG_SYSTEM,0,"Don't use Lightning Flash [/NOFLASH]: %s.",ppd->noflash ? "On" : "Off");
	if (ch[cn].value[1][V_FREEZE]) log_char(cn,LOG_SYSTEM,0,"Don't use Freeze [/NOFREEZE]: %s.",ppd->nofreeze ? "On" : "Off");
	if (ch[cn].value[1][V_HEAL]) log_char(cn,LOG_SYSTEM,0,"Don't use Heal [/NOHEAL]: %s.",ppd->noheal ? "On" : "Off");
	if (ch[cn].value[1][V_MAGICSHIELD]) log_char(cn,LOG_SYSTEM,0,"Don't use Magic Shield [/NOSHIELD]: %s.",ppd->noshield ? "On" : "Off");
	if (ch[cn].value[1][V_PULSE]) log_char(cn,LOG_SYSTEM,0,"Don't use Pulse [/NOPULSE]: %s.",ppd->nopulse ? "On" : "Off");
	if (ch[cn].value[1][V_WARCRY]) log_char(cn,LOG_SYSTEM,0,"Don't use Warcry [/NOWARCRY]: %s.",ppd->nowarcry ? "On" : "Off");

	log_char(cn,LOG_SYSTEM,0,"Don't use Healing Potions [/NOLIFE]: %s.",ppd->nolife ? "On" : "Off");
	log_char(cn,LOG_SYSTEM,0,"Don't use Mana Potions [/NOMANA]: %s.",ppd->nomana ? "On" : "Off");
	log_char(cn,LOG_SYSTEM,0,"Don't use Combo Potions [/NOCOMBO]: %s.",ppd->nocombo ? "On" : "Off");
	log_char(cn,LOG_SYSTEM,0,"Don't use Recall Scroll [/NORECALL]: %s.",ppd->norecall ? "On" : "Off");
	log_char(cn,LOG_SYSTEM,0,"Don't Move [/NOMOVE]: %s.",ppd->nomove ? "On" : "Off");

	log_char(cn,LOG_SYSTEM,0,"Automation Settings:");
	if (ch[cn].value[1][V_BLESS]) log_char(cn,LOG_SYSTEM,0,"Automatic Re-Bless [/AUTOBLESS]: %s.",ppd->autobless ? "On" : "Off");	
	if (ch[cn].value[1][V_PULSE]) log_char(cn,LOG_SYSTEM,0,"Automatic Pulse [/AUTOPULSE]: %s.",ppd->autopulse ? "On" : "Off");	
	log_char(cn,LOG_SYSTEM,0,"Automatic Turning [/AUTOTURN]: %s.",ppd->autoturn ? "On" : "Off");

	log_char(cn,LOG_SYSTEM,0,"Protection Settings:");
	log_char(cn,LOG_SYSTEM,0,"Allow others to bless me [/ALLOWBLESS]: %s.",(ch[cn].flags&CF_NOBLESS) ? "No" : "Yes");
}

void cmd_logout(int cn)
{
	int m;

	if (ch[cn].x>1 && ch[cn].x<MAXMAP && ch[cn].y>1 && ch[cn].y<MAXMAP) {
		m=ch[cn].x+ch[cn].y*MAXMAP;
		if (map[m].flags&MF_RESTAREA) {
			dlog(cn,0,"Used /logout");
			exit_char(cn);
			if (ch[cn].player) player_client_exit(ch[cn].player,"Logout upon player request.");
			return;
		}
	}
	log_char(cn,LOG_SYSTEM,0,"You are not on a blue square.");
}

int underwater(int cn)
{
	int n,m,in;

	if (ch[cn].x<1 || ch[cn].x>=MAXMAP-1 || ch[cn].y<1 || ch[cn].y>=MAXMAP-1) return 0;
	m=ch[cn].x+ch[cn].y*MAXMAP;
	if (map[m].flags&MF_UNDERWATER) {
		for (n=0; n<30; n++)
			if ((in=ch[cn].item[n]) && in>0 && in<MAXITEM && it[in].driver==IDR_UWTALK) return 0;			
		
		return 1;
	}
	return 0;
}

int in_arena(int cn)
{
        return (map[ch[cn].x+ch[cn].y*MAXMAP].flags&MF_ARENA)!=0;
}

int is_lqmaster(int cn)
{
	if (ch[cn].flags&CF_GOD) return 1;
        if (areaID!=20) return 0;
        if (!(ch[cn].flags&CF_LQMASTER)) return 0;
	return 1;
}

void cmd_renclan(int cn,char *ptr)
{
	int cnr,n;
	char name[80];

	if (areaID!=3) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, this command only works in Aston.");
		return;
	}

	while (isspace(*ptr)) ptr++;
	
	cnr=atoi(ptr);
	if (cnr<1 || cnr>=MAXCLAN) {
		log_char(cn,LOG_SYSTEM,0,"Clan number must be between 1 and %d.",MAXCLAN-1);
		return;
	}

	while (isdigit(*ptr)) ptr++;
	while (isspace(*ptr)) ptr++;

	for (n=0; n<79; n++) {
		if (!ptr[n]) break;
		name[n]=ptr[n];
	}
	name[n]=0;
	clan_setname(cnr,name);
	dlog(cn,0,"changed clan %d name to '%s'",cnr,name);

	log_char(cn,LOG_SYSTEM,0,"Clan %d name changed to \"%s\".",cnr,name);
}

void cmd_renclub(int cn,char *ptr)
{
	int cnr,n;
	char name[80];

	if (areaID!=3) {
		log_char(cn,LOG_SYSTEM,0,"Sorry, this command only works in Aston.");
		return;
	}

	while (isspace(*ptr)) ptr++;
	
	cnr=atoi(ptr);
	if (cnr<1 || cnr>=MAXCLUB) {
		log_char(cn,LOG_SYSTEM,0,"Club number must be between 1 and %d.",MAXCLUB-1);
		return;
	}

	while (isdigit(*ptr)) ptr++;
	while (isspace(*ptr)) ptr++;

	for (n=0; n<79; n++) {
		if (!ptr[n]) break;
		name[n]=ptr[n];
	}
	name[n]=0;
	if (!rename_club(cnr,name)) {
		log_char(cn,LOG_SYSTEM,0,"That didn't work. The name is either taken or illegal.");
		return;
	}
	dlog(cn,0,"changed club %d name to '%s'",cnr,name);

	log_char(cn,LOG_SYSTEM,0,"Club %d name changed to \"%s\".",cnr,name);
}

int command(int cn,char *ptr)	// 1=ok, 0=repeat
{
	int len;
	char buf[256];
	struct lostcon_ppd *ppd;

	while (isspace(*ptr)) ptr++;

        if ((len=cmdcmp(ptr,"#alias",2)) || (len=cmdcmp(ptr,"/alias",2))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_alias(cn,ptr);
		return 1;
	}
	if ((len=cmdcmp(ptr,"#clearaliases",13)) || (len=cmdcmp(ptr,"/clearaliases",13))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_clear_aliases(cn,ptr);
		return 1;
	}

	expand_alias(cn,buf,ptr); ptr=buf;

	if (areaID==20 && char_driver(CDR_LQPARSER,CDT_SPECIAL,cn,(int)(ptr),0)==2) return 1;
	if (areaID==35 && char_driver(CDR_LQPARSER,CDT_SPECIAL,cn,(int)(ptr),0)==2) return 1;
	if (areaID==23 && char_driver(CDR_STRATEGY_PARSER,CDT_SPECIAL,cn,(int)(ptr),0)==2) return 1;
	if (areaID==24 && char_driver(CDR_STRATEGY_PARSER,CDT_SPECIAL,cn,(int)(ptr),0)==2) return 1;

        if (*ptr!='#' && *ptr!='/') {
		if (ch[cn].flags&CF_SHUTUP) {		
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		} if (underwater(cn)) {
			say(cn,"Blub.");
		} else {
			demontest(cn,ptr);
			if (!swearing(cn,ptr)) say(cn,"%s",ptr);
		}
		return 1;
	}

	ptr++;

        while (isspace(*ptr)) ptr++;

        if ((len=cmdcmp(ptr,"holler",0))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
		if (ch[cn].flags&CF_SHUTUP)
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		else if (!swearing(cn,ptr)) {
			if (underwater(cn)) say(cn,"Blub.");
			else holler(cn,"%s",ptr);
		}
		return 1;
	}

	if ((len=cmdcmp(ptr,"shout",0))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
		if (ch[cn].flags&CF_SHUTUP)
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		else if (!swearing(cn,ptr)) {
			if (underwater(cn)) say(cn,"Blub.");
			else shout(cn,"%s",ptr);
		}
		return 1;
	}

	if ((len=cmdcmp(ptr,"say",0))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
		if (ch[cn].flags&CF_SHUTUP)
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		else if (!swearing(cn,ptr)) {
			if (underwater(cn)) say(cn,"Blub.");
			else say(cn,"%s",ptr);
		}
		return 1;
	}

	if ((len=cmdcmp(ptr,"murmur",0))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
		if (ch[cn].flags&CF_SHUTUP)
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		else if (!swearing(cn,ptr)) {
			if (underwater(cn)) say(cn,"Blub.");
			else murmur(cn,"%s",ptr);
		}
		return 1;
	}

	if ((len=cmdcmp(ptr,"whisper",0))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
		if (ch[cn].flags&CF_SHUTUP)
			log_char(cn,LOG_SYSTEM,0,"Sorry, you cannot say anything right now.");
		else if (!swearing(cn,ptr)) {
			if (underwater(cn)) say(cn,"Blub.");
			else whisper(cn,"%s",ptr);
		}
		return 1;
	}

	if ((len=cmdcmp(ptr,"clanpots",5))) {
                show_clan_pots(cn);
		return 1;
	}
	if ((len=cmdcmp(ptr,"clan",0))) {
                showclan(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"club",0))) {
                showclub(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"clearignore",11))) {
                clearignore_cmd(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"wimp",4))) {
                log_char(cn,LOG_SYSTEM,0,"You're not in the live quest area. You'll have to wimp out on your own here... That means: RUN!");
		return 1;
	}

	if ((len=cmdcmp(ptr,"relation",0))) {
		int nr;
		
		ptr+=len; while (isspace(*ptr)) ptr++;
                nr=atoi(ptr);

		show_clan_relation(cn,nr ? nr : ch[cn].clan);
		return 1;
	}

	if ((len=cmdcmp(ptr,"exp",3)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len;
		cmd_exp(cn,ptr);
	        return 1;
	}
	if ((len=cmdcmp(ptr,"labsolved",8)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len;
		cmd_labsolved(cn,ptr);
	        return 1;
	}
	if ((len=cmdcmp(ptr,"laugh",5)) && (ch[cn].flags&CF_GOD)) {
                sound_area(ch[cn].x,ch[cn].y,13);
	        return 1;
	}

	if ((len=cmdcmp(ptr,"ggold",5)) && (ch[cn].flags&CF_GOD)) {
		int gold;

                ptr+=len; while (isspace(*ptr)) ptr++;

		gold=atoi(ptr);

		ch[cn].gold+=gold*100;
		ch[cn].flags|=CF_ITEMS;

		return 1;
	}
	if ((len=cmdcmp(ptr,"shutdown",8)) && (ch[cn].flags&CF_GOD)) {
		int diff,down;

                ptr+=len; while (isspace(*ptr)) ptr++;
		diff=atoi(ptr);
		
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		down=atoi(ptr);

		start_shutdown(diff,down);

		return 1;
	}
	if ((len=cmdcmp(ptr,"gold",4))) {
		int gold,in;

                ptr+=len; while (isspace(*ptr)) ptr++;

		gold=atoi(ptr)*100;

		if (gold<1) {
			log_char(cn,LOG_SYSTEM,0,"Hu?");
			return 1;
		}

		if (gold>ch[cn].gold) {
			log_char(cn,LOG_SYSTEM,0,"You do not have that much gold.");
			return 1;
		}
		if (ch[cn].citem) {
			log_char(cn,LOG_SYSTEM,0,"Please free your hand (mouse cursor) first.");
			return 1;
		}
		in=create_money_item(gold);
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"took from #gold %d",gold);
		ch[cn].citem=in;
		it[in].carried=cn;
		ch[cn].flags|=CF_ITEMS;
		ch[cn].gold-=gold;

		return 1;
	}

	if ((len=cmdcmp(ptr,"maxlag",4))) {
		int lag;

                ptr+=len; while (isspace(*ptr)) ptr++;

		lag=atoi(ptr);

		if (lag<3 || lag>20) {
			log_char(cn,LOG_SYSTEM,0,"Number must be between 3 and 20.");
			return 1;
		}
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			ppd->maxlag=lag;

			log_char(cn,LOG_SYSTEM,0,"Set delay for lag control to kick in to %d seconds.",ppd->maxlag);
                }

                return 1;
	}

	if ((len=cmdcmp(ptr,"setxmas",7)) && (ch[cn].flags&CF_GOD)) {
		int flag;

                ptr+=len; while (isspace(*ptr)) ptr++;

		flag=atoi(ptr);
		log_char(cn,LOG_SYSTEM,0,"Setting christmas special to %d, old value was %d.",flag,isxmas);
		sprintf(buf,"%10dX%10dX",0,flag);
		server_chat(1035,buf);

                return 1;
	}

	if ((len=cmdcmp(ptr,"sprite",6)) && (ch[cn].flags&CF_GOD)) {
		int spr;

                ptr+=len; while (isspace(*ptr)) ptr++;

		spr=atoi(ptr);

		ch[cn].sprite=spr;
		set_sector(ch[cn].x,ch[cn].y);

		return 1;
	}

        if (cmdcmp(ptr,"color",4) && (ch[cn].flags&CF_GOD)) {
		log_char(cn,LOG_SYSTEM,0,"c1=%X, c2=%X, c3=%X",ch[cn].c1,ch[cn].c2,ch[cn].c3);
		xlog("csprite=%d; c1=0x%04X; c2=0x%04X; c3=0x%04X;",ch[cn].sprite,ch[cn].c1,ch[cn].c2,ch[cn].c3);
		return 1;
	}
        if ((len=cmdcmp(ptr,"col1",4))) {
		int col1,col2,col3,n;

                ptr+=len; while (isspace(*ptr)) ptr++;
		col1=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col2=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col3=atoi(ptr);

		ch[cn].c1=(col1<<10)+(col2<<5)+col3;
		
		for (n=1; n<MAXPLAYER; n++)
			set_player_knows_name(n,cn,0);

		return 1;
	}
	if ((len=cmdcmp(ptr,"col2",4))) {
		int col1,col2,col3,n;

                ptr+=len; while (isspace(*ptr)) ptr++;
		col1=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col2=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col3=atoi(ptr);

		ch[cn].c2=(col1<<10)+(col2<<5)+col3;
		
		for (n=1; n<MAXPLAYER; n++)
			set_player_knows_name(n,cn,0);

		return 1;
	}
	if ((len=cmdcmp(ptr,"col3",4))) {
		int col1,col2,col3,n;

                ptr+=len; while (isspace(*ptr)) ptr++;
		col1=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col2=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		col3=atoi(ptr);

		ch[cn].c3=(col1<<10)+(col2<<5)+col3;
		
		for (n=1; n<MAXPLAYER; n++)
			set_player_knows_name(n,cn,0);

		return 1;
	}

	if ((len=cmdcmp(ptr,"saves",4)) && (ch[cn].flags&CF_GOD)) {
		int saves;

                ptr+=len; while (isspace(*ptr)) ptr++;

		saves=atoi(ptr);

		ch[cn].saves=saves;

		return 1;
	}

	if ((len=cmdcmp(ptr,"immortal",2)) && is_lqmaster(cn)) {
		ch[cn].flags^=CF_IMMORTAL;

		log_char(cn,LOG_SYSTEM,0,"Immortal is %s.",(ch[cn].flags&CF_IMMORTAL) ? "on" : "off");

                return 1;
	}

	if ((len=cmdcmp(ptr,"infrared",3)) && is_lqmaster(cn)) {
		ch[cn].flags^=CF_INFRARED;

		log_char(cn,LOG_SYSTEM,0,"Infrared is %s.",(ch[cn].flags&CF_INFRARED) ? "on" : "off");

                return 1;
	}

	if ((len=cmdcmp(ptr,"playerkiller",12))) {
                if (ch[cn].flags&CF_PK) {
			if (ch[cn].action!=AC_IDLE || ticker-ch[cn].regen_ticker<TICKS*3) {
				log_char(cn,LOG_SYSTEM,0,"Pant, pant. Too tired.");
			} else {
				leave_pk(cn);
			}
		} else {
                        if (ch[cn].level<10) log_char(cn,LOG_SYSTEM,0,"Sorry, you may not become a player killer before reaching level 10.");
			else if (!(ch[cn].flags&CF_PAID)) log_char(cn,LOG_SYSTEM,0,"Sorry, only paying players may become player killers.");
			else log_char(cn,LOG_SYSTEM,0,"°c3Please take a moment to consider this decision. If another player kills you, he will be able to take all your belongings, or kill you over and over again. Do you really want this? Type: '/iwilldie %d' to confirm.",ch[cn].ID);
		}

		log_char(cn,LOG_SYSTEM,0,"PK is %s.",(ch[cn].flags&CF_PK) ? "on" : "off");
		
                return 1;
	}

	if ((len=cmdcmp(ptr,"iwilldie",8))) {
		ptr+=len;
                if (ch[cn].flags&CF_PK) {
			if (ch[cn].action!=AC_IDLE || ticker-ch[cn].regen_ticker<TICKS*3) {
				log_char(cn,LOG_SYSTEM,0,"Pant, pant. Too tired.");
			} else {
				leave_pk(cn);
			}
		} else {
                        if (ch[cn].level<10) log_char(cn,LOG_SYSTEM,0,"Sorry, you may not become a player killer before reaching level 10.");
			else if (!(ch[cn].flags&CF_PAID)) log_char(cn,LOG_SYSTEM,0,"Sorry, only paying players may become player killers.");
                        else {
				if (atoi(ptr)!=ch[cn].ID) log_char(cn,LOG_SYSTEM,0,"Please type: '/playerkiller' first.");
				else join_pk(cn);
			}
		}

		log_char(cn,LOG_SYSTEM,0,"PK is %s.",(ch[cn].flags&CF_PK) ? "on" : "off");
		
                return 1;
	}

        if ((len=cmdcmp(ptr,"invisible",3)) && is_lqmaster(cn)) {
		ch[cn].flags^=CF_INVISIBLE;

		log_char(cn,LOG_SYSTEM,0,"Invisible is %s.",(ch[cn].flags&CF_INVISIBLE) ? "on" : "off");

                return 1;
	}

	if ((len=cmdcmp(ptr,"dlight",6)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		dlight_override=atoi(ptr);

		return 1;
	}

	if ((len=cmdcmp(ptr,"showattack",6)) && (ch[cn].flags&CF_GOD)) {
                showattack=1-showattack;
		return 1;
	}

	if ((len=cmdcmp(ptr,"join",0))) {
		int nr;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		join_chat(cn,nr);

                return 1;
	}

	if ((len=cmdcmp(ptr,"joinclan",8)) && (ch[cn].flags&CF_GOD)) {
		int nr,n;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		if (nr>=0 && nr<MAXCLAN) {
			ch[cn].clan=nr;
			ch[cn].clan_serial=clan_serial(nr);
			ch[cn].clan_rank=4;
		}

		for (n=1; n<MAXPLAYER; n++)
			set_player_knows_name(n,cn,0);

                return 1;
	}

	if ((len=cmdcmp(ptr,"joinclub",8)) && (ch[cn].flags&CF_GOD)) {
		int nr,n;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		if (nr>=0 && nr<MAXCLUB) {
			ch[cn].clan=nr+CLUBOFFSET;
			ch[cn].clan_serial=club[nr].serial;
			ch[cn].clan_rank=2;
		}

		for (n=1; n<MAXPLAYER; n++)
			set_player_knows_name(n,cn,0);

                return 1;
	}

	if ((len=cmdcmp(ptr,"killclan",8)) && (ch[cn].flags&CF_GOD)) {
		int nr;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		if (nr>0 && nr<MAXCLAN) {
			kill_clan(nr);
		}

                return 1;
	}
	if ((len=cmdcmp(ptr,"killclub",8)) && (ch[cn].flags&CF_GOD)) {
		int nr;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		if (nr>0 && nr<MAXCLAN) {
			kill_club(nr);
		}

                return 1;
	}

	if ((len=cmdcmp(ptr,"punish",6)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
                ptr+=len; while (isspace(*ptr)) ptr++;
		
		return cmd_punish(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"shutup",6)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
                ptr+=len; while (isspace(*ptr)) ptr++;
		
		return cmd_shutup(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"rename",6)) && (ch[cn].flags&(CF_GOD))) {
                ptr+=len; while (isspace(*ptr)) ptr++;
		
		return cmd_rename(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"lockname",8)) && (ch[cn].flags&(CF_GOD))) {
                ptr+=len; while (isspace(*ptr)) ptr++;
		
		return cmd_lockname(cn,ptr);
	}
	if ((len=cmdcmp(ptr,"unlockname",10)) && (ch[cn].flags&(CF_GOD))) {
                ptr+=len; while (isspace(*ptr)) ptr++;
		
		return cmd_unlockname(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"unpunish",8)) && (ch[cn].flags&(CF_GOD))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_unpunish(cn,ptr);
	}

        if ((len=cmdcmp(ptr,"leave",0))) {
		int nr;

                ptr+=len; while (isspace(*ptr)) ptr++;

		nr=atoi(ptr);

		leave_chat(cn,nr);

                return 1;
	}

	if ((len=cmdcmp(ptr,"channels",0))) {
		
		list_chat(cn);

                return 1;
	}

	if ((len=cmdcmp(ptr,"listhate",0))) {
		
		list_hate(cn);

                return 1;
	}

	/*if ((len=cmdcmp(ptr,"system",6)) && (ch[cn].flags&CF_GOD)) {
		extern float new_system,old_system;
		
		log_char(cn,LOG_SYSTEM,0,"system new=%.3f, old=%.3f",new_system,old_system);

                return 1;
	}*/

	if ((len=cmdcmp(ptr,"allow",3))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		return allow_body(cn,ptr);		
	}

	if ((len=cmdcmp(ptr,"ignore",3))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		return ignore_cmd(cn,ptr);		
	}

	if ((len=cmdcmp(ptr,"values",6)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		return look_values(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"lollipop",8)) && (ch[cn].flags&(CF_GOD))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		return lollipop_cmd(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"whostaff",4)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
                who_staff(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"hate",3))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		cmd_hate(cn,ptr);
		return 1;
	}

	if ((len=cmdcmp(ptr,"nohate",3))) {
                ptr+=len;
		
		while (isspace(*ptr)) ptr++;

		return cmd_nohate(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"clearhate",9))) {
                del_all_hate(cn);
		log_char(cn,LOG_SYSTEM,0,"Hate list has been erased.");
		return 1;
	}

        if ((len=cmdcmp(ptr,"goto",3)) && is_lqmaster(cn)) {
		int x,y,a=0,m;

                ptr+=len;
		while (isspace(*ptr)) ptr++;

		x=atoi(ptr);
                if (!x) {
			int n;

			for (n=0; n<sizeof(gl)/sizeof(struct gotolist); n++) {
				if (!strcasecmp(gl[n].name,ptr)) break;
			}
			if (n==sizeof(gl)/sizeof(struct gotolist)) {
				for (n=1; n<MAXCHARS; n++) {
					if (!(ch[n].flags)) continue;
					if (!strcasecmp(ch[n].name,ptr)) break;
				}
				if (n<MAXCHARS) {
					x=ch[n].x;
					y=ch[n].y;
				} else x=y=0;
			} else {
				x=gl[n].x;
				y=gl[n].y;
				a=gl[n].a;
			}
		} else {
			while (isdigit(*ptr)) ptr++;
			while (isspace(*ptr)) ptr++;
		
			y=atoi(ptr);

			if (!y) {
				switch(tolower(*ptr)) {
					case 'n':	y=ch[cn].y-x; x=ch[cn].x-x; break;
					case 's':	y=ch[cn].y+x; x=ch[cn].x+x; break;
					case 'w':	y=ch[cn].y+x; x=ch[cn].x-x; break;
					case 'e':	y=ch[cn].y-x; x=ch[cn].x+x; break;
					default:	x=y=0; break;
				}
			} else {
				while (isdigit(*ptr)) ptr++;
				while (isspace(*ptr)) ptr++;
		
				a=atoi(ptr);
			}
		}
		
		while (!isspace(*ptr) && *ptr) ptr++;
		while (isspace(*ptr)) ptr++;
		
		m=atoi(ptr);

		if (m>0 && m<27) {
			ch[cn].mirror=m;
			if (!a) a=areaID;
		}

		if (a==areaID && !m) a=0;
		if (!(ch[cn].flags&CF_GOD)) a=0;

		if (!a) {
			if (x>0 && x<MAXMAP-1 && y>0 && y<MAXMAP-1) {
				if (teleport_char_driver(cn,x,y)) charlog(cn,"goto %d %d",x,y);
			}
		} else {
			change_area(cn,a,x,y);
		}

                return 1;
	}

	if ((len=cmdcmp(ptr,"jump",4)) && (ch[cn].flags&(CF_STAFF|CF_GOD))) {
		int x=0,y=0,a=0,m=0,n;

                ptr+=len;
		while (isspace(*ptr)) ptr++;

		if (isdigit(*ptr)) {
			m=atoi(ptr);

                        while (!isspace(*ptr) && *ptr) ptr++;
			while (isspace(*ptr)) ptr++;
		}

		for (n=0; n<sizeof(gl)/sizeof(struct gotolist); n++) {
			if (!strcasecmp(gl[n].name,ptr)) break;
		}
		if (n<sizeof(gl)/sizeof(struct gotolist)) {
			x=gl[n].x;
			y=gl[n].y;
			a=gl[n].a;
		}

                if (a==areaID && !m) a=0;

		if (ch[cn].action!=AC_IDLE || ticker-ch[cn].regen_ticker<TICKS*3) {
			log_char(cn,LOG_SYSTEM,0,"Pant, pant. Too tired.");
		} else if (x>0 && x<MAXMAP-1 && y>0 && y<MAXMAP-1) {
			if (!a) {
				if (teleport_char_driver(cn,x,y)) charlog(cn,"jump %d %d",x,y);			
			} else {
				dlog(cn,0,"jump %d %d %d %d",x,y,a,m);
				if (m>0 && m<27) {
					ch[cn].mirror=m;
					if (!a) a=areaID;
				}
				change_area(cn,a,x,y);
			}
		} else log_char(cn,LOG_SYSTEM,0,"hu?");

                return 1;
	}

	if ((len=cmdcmp(ptr,"summon",3)) && (ch[cn].flags&CF_GOD)) {
		int co;

                ptr+=len;
		while (isspace(*ptr)) ptr++;

		for (co=1; co<MAXCHARS; co++) {
			if (!(ch[co].flags)) continue;
			if (!strcasecmp(ch[co].name,ptr)) break;
		}
		if (co<MAXCHARS) {
                        if (teleport_char_driver(co,ch[cn].x,ch[cn].y)) dlog(cn,0,"summon %s %d",ch[co].name,co);
		}

                return 1;
	}

	if ((len=cmdcmp(ptr,"kick",4)) && (ch[cn].flags&(CF_STAFF|CF_GOD))) {
		int co;

                ptr+=len;
		while (isspace(*ptr)) ptr++;

		for (co=1; co<MAXCHARS; co++) {
			if (!(ch[co].flags&CF_PLAYER)) continue;
			if (!strcasecmp(ch[co].name,ptr)) break;
		}
		if (co<MAXCHARS) {
			char reason[256];

			log_char(cn,LOG_SYSTEM,0,"Kicked %s.",ptr);
			dlog(cn,0,"kick %s %d",ch[co].name,co);

			sprintf(reason,"Kick note from %s, %s kicked",ch[cn].staff_code,ch[co].name);
                        write_scrollback(ch[cn].player,cn,reason,ch[cn].name,ch[co].name);
			
			exit_char(co);
			if (ch[co].player) player_client_exit(ch[co].player,"You have been kicked by game administration.");
		} else log_char(cn,LOG_SYSTEM,0,"No player by the name %s.",ptr);

                return 1;
	}

	if ((len=cmdcmp(ptr,"create",3)) && (ch[cn].flags&CF_GOD)) {
		int in,temp;

                ptr+=len;
		while (isspace(*ptr)) ptr++;

		if (ch[cn].citem) {
			log_char(cn,LOG_SYSTEM,0,"Please empty your mouse cursor first.");
			return 1;
		}
		temp=lookup_item(ptr);
		if (!temp) {
			log_char(cn,LOG_SYSTEM,0,"No such template exists.");
			return 1;
		}

		if ((in=create_item_nr(temp))) {
			if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"took from #create");
			ch[cn].citem=in;
			ch[cn].flags|=CF_ITEMS;
			it[in].carried=cn;
			if (it[in].flags&IF_BONDTAKE) bondtake_item(in,cn);
			if (it[in].flags&IF_BONDWEAR) bondwear_item(in,cn);
		}

                return 1;
	}
	
        if ((len=cmdcmp(ptr,"lag",3))) {
		if (!(ch[cn].flags&CF_LAG) && in_arena(cn)) {
			log_char(cn,LOG_SYSTEM,0,"You cannot simulate lag in an arena.");
		} else if (!(ch[cn].flags&CF_LAG) && !is_hate_empty(cn)) {
			log_char(cn,LOG_SYSTEM,0,"You cannot simulate lag while your hate list is not empty.");
		} else {
			ch[cn].flags^=CF_LAG;
			log_char(cn,LOG_SYSTEM,0,"Turned artificial lag %s.",(ch[cn].flags&CF_LAG) ? "on" : "off");
			if (ch[cn].flags&CF_LAG) log_char(cn,LOG_SYSTEM,0,"PLEASE turn this option off (type /lag again) before you complain about lag!");
		}
                return 1;
	}

	if ((len=cmdcmp(ptr,"thief",3))) {
		ch[cn].flags^=CF_THIEFMODE;
                log_char(cn,LOG_SYSTEM,0,"Turned thief mode %s.",(ch[cn].flags&CF_THIEFMODE) ? "on" : "off");
		update_char(cn);
                return 1;
	}

	if ((len=cmdcmp(ptr,"notells",3))) {
		ch[cn].flags^=CF_NOTELL;
                log_char(cn,LOG_SYSTEM,0,"Turned no-tell mode %s.",(ch[cn].flags&CF_NOTELL) ? "on" : "off");
		update_char(cn);
                return 1;
	}

        if ((len=cmdcmp(ptr,"complain",4))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_complain(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"description",3))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_desc(cn,ptr);

		return 1;
	}

	if ((len=cmdcmp(ptr,"tell",0))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_tell(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"staffcode",6)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_staffcode(cn,ptr);
	}

        if ((len=cmdcmp(ptr,"time",2))) {
                showtime(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"help",0))) {
                cmd_help(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"swap",0))) {
                char_swap(cn);
		return 1;
	}

        if ((len=cmdcmp(ptr,"look",4)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
		char name[80]={"oops"};
		int ID;

                ptr+=len; while (isspace(*ptr)) ptr++;

		ID=lookup_name(ptr,name);
		
		if (ID==0) return 0;
                if (ID==-1) {
			log_char(cn,LOG_SYSTEM,0,"No character by the name %s.",ptr);
			return 1;
		}
		read_notes(ID,ch[cn].ID);

		return 1;
	}

	if ((len=cmdcmp(ptr,"klog",4)) && (ch[cn].flags&(CF_GOD|CF_STAFF))) {
                karmalog(ch[cn].ID);
		return 1;
	}

	if ((len=cmdcmp(ptr,"lastseen",4))) {
		char name[80]={"oops"};
		int ID;

                ptr+=len; while (isspace(*ptr)) ptr++;

		ID=lookup_name(ptr,name);
		
		if (ID==0) return 0;
                if (ID==-1) {
			log_char(cn,LOG_SYSTEM,0,"No character by the name %s.",ptr);
			return 1;
		}
		lastseen(ID,ch[cn].ID);

		return 1;
	}

	if ((len=cmdcmp(ptr,"query",5)) && (ch[cn].flags&(CF_GOD))) {
		list_queries(cn);
		return 1;
	}
        if ((len=cmdcmp(ptr,"noarch",6)) && (ch[cn].flags&(CF_GOD))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_noarch(cn,ptr);
		return 1;
	}
	if ((len=cmdcmp(ptr,"fixit",5)) && (ch[cn].flags&(CF_GOD))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_fixit(cn,ptr);
		return 1;
	}
	if ((len=cmdcmp(ptr,"questfix",8)) && (ch[cn].flags&(CF_GOD))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_questfix(cn,ptr);
		return 1;
	}
	if ((len=cmdcmp(ptr,"reset",5)) && (ch[cn].flags&(CF_GOD))) {
		ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_reset(cn,ptr);
		return 1;
	}

	if ((len=cmdcmp(ptr,"noprof",6)) && (ch[cn].flags&(CF_GOD))) {
		int n;

		for (n=0; n<P_MAX; n++) ch[cn].prof[n]=0;
                ch[cn].flags|=CF_PROF;
		update_char(cn);
		return 1;
	}

	if ((len=cmdcmp(ptr,"#ls",3)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_ls(cn,ptr);
                return 1;
	}

	if ((len=cmdcmp(ptr,"#cat",4)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_cat(cn,ptr);
                return 1;
	}

	if ((len=cmdcmp(ptr,"god",3)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_flag(cn,ptr,CF_GOD);
	}

	if ((len=cmdcmp(ptr,"setsir",6)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_flag(cn,ptr,CF_WON);
	}

	if ((len=cmdcmp(ptr,"heal",4)) && (ch[cn].flags&CF_GOD)) {
                ch[cn].hp=ch[cn].value[0][V_HP]*POWERSCALE;

		return 1;
	}

        if ((len=cmdcmp(ptr,"staff",4)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_flag(cn,ptr,CF_STAFF);
	}
	if ((len=cmdcmp(ptr,"nowho",5)) && (ch[cn].flags&(CF_STAFF|CF_GOD))) {
                ch[cn].flags^=CF_NOWHO;
		log_char(cn,LOG_SYSTEM,0,"NoWho %s.",(ch[cn].flags&CF_NOWHO) ? "enabled" : "disabled");
		return 1;
	}
	if ((len=cmdcmp(ptr,"hardcore",8)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_flag(cn,ptr,CF_HARDCORE);
	}
	if ((len=cmdcmp(ptr,"qmaster",7)) && (ch[cn].flags&CF_GOD)) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		return cmd_flag(cn,ptr,CF_LQMASTER);
	}

	if ((len=cmdcmp(ptr,"sort",2))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		cmd_sort(cn,ptr);
                return 1;
	}
	
	if ((len=cmdcmp(ptr,"depotsort",6))) {
                ptr+=len; while (isspace(*ptr)) ptr++;

		depot_sort(cn);
                return 1;
	}

	if (cmdcmp(ptr,"who",0)) {
                cmd_who(cn);
                return 1;
	}

	if (cmdcmp(ptr,"noball",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->noball) ppd->noball=0; else ppd->noball=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}
	if (cmdcmp(ptr,"nobless",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nobless) ppd->nobless=0; else ppd->nobless=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nofireball",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nofireball) ppd->nofireball=0; else ppd->nofireball=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"noflash",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->noflash) ppd->noflash=0; else ppd->noflash=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nofreeze",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nofreeze) ppd->nofreeze=0; else ppd->nofreeze=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}
	if (cmdcmp(ptr,"noheal",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->noheal) ppd->noheal=0; else ppd->noheal=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"noshield",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->noshield) ppd->noshield=0; else ppd->noshield=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nowarcry",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nowarcry) ppd->nowarcry=0; else ppd->nowarcry=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nolife",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nolife) ppd->nolife=0; else ppd->nolife=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nomana",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nomana) ppd->nomana=0; else ppd->nomana=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nocombo",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nocombo) ppd->nocombo=0; else ppd->nocombo=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"nomove",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nomove) ppd->nomove=0; else ppd->nomove=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}
	if (cmdcmp(ptr,"norecall",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->norecall) ppd->norecall=0; else ppd->norecall=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}
	if (cmdcmp(ptr,"nopulse",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->nopulse) ppd->nopulse=0; else ppd->nopulse=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"autobless",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->autobless) ppd->autobless=0; else ppd->autobless=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"autoturn",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->autoturn) ppd->autoturn=0; else ppd->autoturn=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"autopulse",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			if (ppd->autopulse) ppd->autopulse=0; else ppd->autopulse=1;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"allowbless",5)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
			ch[cn].flags^=CF_NOBLESS;
			show_lostconppd(cn,ppd);
                }
                return 1;
	}

	if (cmdcmp(ptr,"killbless",5)) {
		int n,in;
		for (n=12; n<30; n++) {
			if ((in=ch[cn].item[n]) && it[in].driver==IDR_BLESS) {
				destroy_effect_type(cn,EF_BLESS);
				destroy_item(in);
				ch[cn].item[n]=0;
				update_char(cn);
				log_char(cn,LOG_SYSTEM,0,"Done.");
				return 1;
			}
		}
		log_char(cn,LOG_SYSTEM,0,"No Bless found.");
                return 1;
	}

	if (cmdcmp(ptr,"status",0)) {
		if ((ppd=set_data(cn,DRD_LOSTCON_PPD,sizeof(struct lostcon_ppd)))) {
                        show_lostconppd(cn,ppd);
                }
		log_char(cn,LOG_SYSTEM,0,"Account Status:");
		if (ch[cn].paid_till&1) log_char(cn,LOG_SYSTEM,0,"Your account will expire in %d hours, %d minutes and %d seconds.",
						 (ch[cn].paid_till-time_now)/(60*60),
						 ((ch[cn].paid_till-time_now)/60)%60,
						 (ch[cn].paid_till-time_now)%60);
		else log_char(cn,LOG_SYSTEM,0,"Your account will expire in %d days.",(ch[cn].paid_till-time_now)/(60*60*24));
		if (ch[cn].flags&CF_PAID) log_char(cn,LOG_SYSTEM,0,"Paid Account");
		else log_char(cn,LOG_SYSTEM,0,"Trial Account");
		
                return 1;
	}

	if ((len=cmdcmp(ptr,"clanlog",7))) {
                return cmd_clanlog(cn,ptr+len);
	}

	if ((len=cmdcmp(ptr,"renclan",7))) {
                cmd_renclan(cn,ptr+len);

		return 1;
	}
	if ((len=cmdcmp(ptr,"renclub",7))) {
                cmd_renclub(cn,ptr+len);

		return 1;
	}

	if ((len=cmdcmp(ptr,"exterminate",11)) && (ch[cn].flags&(CF_STAFF|CF_GOD))) {
		ptr+=len; while (isspace(*ptr)) ptr++;
                return cmd_exterminate(cn,ptr);
	}

	if ((len=cmdcmp(ptr,"respawn",7)) && (ch[cn].flags&CF_GOD)) {
		respawn_check();
                return 1;
	}

	if ((len=cmdcmp(ptr,"office",6)) && (ch[cn].flags&CF_GOD)) {
		if (areaID!=3) change_area(cn,3,11,195);
		else teleport_char_driver(cn,11,195);
                return 1;
	}

	if ((len=cmdcmp(ptr,"emote",2)) || (len=cmdcmp(ptr,"me",2))) {
		ptr+=len;
		while (isspace(*ptr)) ptr++;
                if (!swearing(cn,ptr)) {
			if (underwater(cn)) emote(cn,"feels wet");
			else emote(cn,"%s",ptr);
		}
                return 1;
	}

	if ((len=cmdcmp(ptr,"wave",2))) {
                if (!swearing(cn,"")) emote(cn,"waves happily");
                return 1;
	}

	if ((len=cmdcmp(ptr,"bow",2))) {
                if (!swearing(cn,"")) emote(cn,"bows deeply");
                return 1;
	}

	if ((len=cmdcmp(ptr,"eg",2))) {
                if (!swearing(cn,"")) emote(cn,"grins evilly");
                return 1;
	}

	if ((len=cmdcmp(ptr,"steal",5))) {
                cmd_steal(cn);
                return 1;
	}

	if ((len=cmdcmp(ptr,"logout",6))) {
                cmd_logout(cn);
                return 1;
	}

	if ((len=cmdcmp(ptr,"prof",4)) && (ch[cn].flags&CF_GOD)) {
                cmd_show_prof(cn);
                return 1;
	}

        if ((len=cmdcmp(ptr,"itemmod",7)) && (ch[cn].flags&(CF_GOD))) {
		int pos,nr,in,val;

		ptr+=len;
		while (isspace(*ptr)) ptr++;

		pos=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
                while (isspace(*ptr)) ptr++;
		nr=atoi(ptr);
		while (isdigit(*ptr)) ptr++;
                while (isspace(*ptr)) ptr++;
		val=atoi(ptr);

		if (!(in=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"Need citem.");
			return 1;
		}
		if (pos<0 || pos>=MAXMOD) {
			log_char(cn,LOG_SYSTEM,0,"Pos out of bounds.");
			return 1;
		}
		if (nr<0 || nr>=V_MAX) {
			log_char(cn,LOG_SYSTEM,0,"Nr out of bounds.");
			return 1;
		}
		if (val<0 || val>=22) {
			log_char(cn,LOG_SYSTEM,0,"Val out of bounds.");
			return 1;
		}

		it[in].mod_index[pos]=nr;
		it[in].mod_value[pos]=val;
		
                set_item_requirements(in);
		look_item(cn,it+in);

                return 1;
	}

	if ((len=cmdcmp(ptr,"itemdesc",8)) && (ch[cn].flags&(CF_GOD))) {
		char desc[80];
		int in,n;

		ptr+=len;
		while (isspace(*ptr)) ptr++;

		for (n=0; n<79; n++) {
			if (!ptr[n]) break;
			desc[n]=ptr[n];
		}	
		desc[n]=0;

		if (!(in=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"Need citem.");
			return 1;
		}

		strcpy(it[in].description,desc);
                look_item(cn,it+in);

                return 1;
	}

	if ((len=cmdcmp(ptr,"itemname",8)) && (ch[cn].flags&(CF_GOD))) {
		char name[80];
		int in,n;

		ptr+=len;
		while (isspace(*ptr)) ptr++;

		for (n=0; n<79; n++) {
			if (!ptr[n]) break;
			name[n]=ptr[n];
		}	
		name[n]=0;

		if (!(in=ch[cn].citem)) {
			log_char(cn,LOG_SYSTEM,0,"Need citem.");
			return 1;
		}

		strcpy(it[in].name,name);
                look_item(cn,it+in);

                return 1;
	}

	if ((len=cmdcmp(ptr,"setskill",8)) && (ch[cn].flags&(CF_GOD))) {
		ptr+=len;
		while (isspace(*ptr)) ptr++;
		
		return cmd_setskill(cn,ptr);
	}

        if ((len=cmdcmp(ptr,"setlevel",8)) && (ch[cn].flags&(CF_GOD))) {
		int level,n,in;

		ptr+=len;
		level=atoi(ptr);
		ch[cn].exp=level2exp(level);
		ch[cn].level=level;

		if (level<30) {
			ch[cn].flags&=~CF_ARCH;
			ch[cn].value[1][V_DURATION]=0;
			ch[cn].value[1][V_RAGE]=0;
		}
		if (level>35) {
			ch[cn].flags|=CF_ARCH;
			if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==CF_MAGE && ch[cn].value[1][V_DURATION]==0) ch[cn].value[1][V_DURATION]=1;
			if ((ch[cn].flags&(CF_MAGE|CF_WARRIOR))==CF_WARRIOR && ch[cn].value[1][V_RAGE]==0) ch[cn].value[1][V_RAGE]=1;
		}
		destroy_chareffects(cn);
		for (n=12; n<30; n++) {
			if ((in=ch[cn].item[n])) {
				destroy_item(in);
				ch[cn].item[n]=0;
			}
		}

		return 1;
	}

        if (cmd_chat(cn,ptr)) return 1;

        log_char(cn,LOG_SYSTEM,0,"Unknown command.");

	return 1;
}























