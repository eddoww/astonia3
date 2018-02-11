/*
 * $Id: server.c,v 1.9 2008/03/24 11:22:52 devel Exp $
 *
 * $Log: server.c,v $
 * Revision 1.9  2008/03/24 11:22:52  devel
 * fixed copyright notice
 *
 * Revision 1.8  2007/10/24 11:39:03  devel
 * fixed max player nr
 *
 * Revision 1.7  2007/03/06 10:44:23  devel
 * added char limit for test area
 *
 * Revision 1.6  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.5  2006/09/27 11:40:43  devel
 * fixed title msg
 *
 * Revision 1.4  2006/09/21 12:02:54  devel
 * added args[0] display of values
 *
 * Revision 1.3  2006/08/19 17:27:38  devel
 * updated (c) message to 2006
 *
 * Revision 1.2  2006/07/16 22:55:13  ssim
 * increased max chars for area 34
 */

#define __USE_BSD_SIGNAL

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <zlib.h>
#include <getopt.h>
#include <malloc.h>
#include <time.h>

#include "server.h"
#include "client.h"
#include "player.h"
#include "io.h"
#include "notify.h"
#include "libload.h"
#include "tool.h"
#include "sleep.h"
#include "log.h"
#include "create.h"
#include "direction.h"
#include "act.h"
#include "los.h"
#include "path.h"
#include "timer.h"
#include "effect.h"
#include "database.h"
#include "map.h"
#include "date.h"
#include "container.h"
#include "store.h"
#include "mem.h"
#include "sector.h"
#include "chat.h"
#include "lookup.h"
#include "clan.h"
#include "motd.h"
#include "respawn.h"
#include "consistency.h"
#include "talk.h"
#include "club.h"

unsigned long long rdtsc(void);

int
	quit=0,		// SIGINT: leave program
	demon=0,	// daemonize server?
	mem_usage=0,	// current memory usage
	ticker=0,	// time counter
	showprof=0,	// SIGTSTP: show profiler output
	sercn=0,	// unique (per server run) ID for characters
	serin=0,	// unique (per server run) ID for items
	online=0,	// number of players online
	areaID=0,	// ID of the area this server is responsible for
	areaM=0,	// our mirror number
	multi=1,	// start extra thread for database accesses
	cycles,		// last tick used X cycles
	server_addr,	// ip address of this server
	server_port,	// tcp port of this server
	time_now,       // current time()
	server_net=0,	// server is supposed to use this network (if multiply interfaces are present)
	server_idle,	// idle percentage of this server
	shutdown_at=0,
	shutdown_down=0,
	nologin=0,
	serverID=0,
	isxmas=0;

volatile long long
	sent_bytes_raw=0,	// bytes sent including approximation of tcp/ip overhead
	sent_bytes=0;		// bytes sent
			
volatile long long
	rec_bytes_raw=0,	// bytes sent including approximation of tcp/ip overhead
	rec_bytes=0;		// bytes sent


struct character *ch;
struct item *it;
struct map *map;

void sig_leave(int dummy)
{
        quit=1;
}

void sig_showprof(int dummy)
{
        showprof=1;
}

/*void sig_crash(int dummy)
{
	unsigned int *stack,n;
	volatile static int cnt=0;
	
	stack=&dummy;

	for (n=0; n<32; n++) {
		elog("crash: %p: %X",stack-n,*(stack-n));
	}
	*(stack-5)+=4;

        if (cnt++>5) abort();
}*/

void tick_char(void);

void *end_of_data_ptr;

void showmem(void)
{
	struct mallinfo mi;
	extern int db_store,used_queries;

	mi=mallinfo();

	xlog("-------- Memory usage info: --------");
	xlog("Used characters: %5d/%5d (%4.1f%%)",used_chars,MAXCHARS,100.0/MAXCHARS*used_chars);
	xlog("Used items     : %5d/%5d (%4.1f%%)",used_items,MAXITEM,100.0/MAXITEM*used_items);
	xlog("Used effects   : %5d/%5d (%4.1f%%)",used_effects,MAXEFFECT,100.0/MAXEFFECT*used_effects);
	xlog("Used containers: %5d/%5d (%4.1f%%)",used_containers,MAXCONTAINER,100.0/MAXCONTAINER*used_containers);
	xlog("Used timers    : %5d",used_timers);
	xlog("Used notifies  : %5d",used_msgs);
	xlog("Used queries   : %5d",used_queries);
	xlog("Stored Results : %5d",db_store);
	xlog("server used    : %.3fM",mem_usage/1024.0/1024.0);
	xlog("malloc/mmap    : %.3fM in %d blocks",mi.hblkhd/1024.0/1024.0,mi.hblks);
	xlog("malloc/sbrk    : %.3fM",mi.arena/1024.0/1024.0);
	xlog("malloc/total   : %.3fM",(mi.arena+mi.hblkhd)/1024.0/1024.0);
	xlog("malloc/unused  : %.3fM in %d blocks",(mi.fordblks)/1024.0/1024.0,mi.ordblks);
	xlog("brk has grown  : %.3fM",((int)(sbrk(0))-(int)(end_of_data_ptr))/(1024.0*1024.0));
	xlog("------------------------------------");
	list_mem();
}

void prof_reset(void);

int maxchars=0,maxitem=0,maxeffect=0;

void show(char *ptr,int size)
{
	int n;

	for (n=0; n<size; n++) {
		if (*ptr<32) printf(".");
		else printf("%c",*ptr);
		ptr++;
	}
}

int main(int argc,char *args[])
{
	int n,c;
	unsigned long long prof,start,end;

	end_of_data_ptr=sbrk(0);

	time_now=time(NULL);

	printf("\n");
	printf("   ********************************************\n");
        printf("   *     Astonia 3 - The Conflict Server      *\n");
	printf("   *             Version %d.%02d.%02d              *\n",VERSION>>16,(VERSION>>8)&255,VERSION&255);
	printf("   ********************************************\n");
        printf("   * Copyright (C) 2001-2008 Intent Software  *\n");
	printf("   * Copyright (C) 1997-2001 Daniel Brockhaus *\n");
	printf("   ********************************************\n");
	printf("\n");

        if (argc>1) {
		while (1) {
			c=getopt(argc,args,"a:m:i:dhc");
			if (c==-1) break;

			switch (c) {
				case 'a': 	if (optarg) areaID=atoi(optarg); break;
				case 'm': 	if (optarg) areaM=atoi(optarg); break;
				case 'd': 	demon=1; break;
				case 'h':	fprintf(stderr,"Usage: %s [-a <areaID>] [-m <mirror>] [-n <use this class A net>] [-d] [-c]\n\n-d Demonize\n-c Disable concurrent database access\n\n",args[0]); exit(0);
				case 'c':	multi=0; break;
				//case 'n':	if (optarg) server_net=atoi(optarg); break;
				case 'i':	if (optarg) serverID=atoi(optarg); break;
			}
		}
	}

	if (!areaID) {
		printf("No areaID given, assuming areaID=1\n");
		areaID=1;
	}
	if (!areaM) {
		printf("No mirror given, assuming areaM=1\n");
		areaM=1;
	}
	if (!serverID) {
		printf("No serverID given, assuming serverID=1\n");
		serverID=1;
	}

#ifdef STAFF
	while (!check_staff_start()) sleep(1);
#endif


        // set character number limit depending on area
	switch(areaID) {
		case 1:		maxchars=512; break;
		case 2:		maxchars=896; break;
		case 3:		maxchars=384; break;
		case 4:		maxchars=2048; break;
		case 5:		maxchars=768; break;
		case 6:		maxchars=384; break;
		case 7:		maxchars=1280; break;
		case 8:		maxchars=384; break;
		case 9:		maxchars=1280; break;
		case 10:	maxchars=512; break;
		case 11:	maxchars=384; break;
		case 12:	maxitem=1024*48; maxchars=384; break;
		case 13:	maxchars=9*50+200; break;
		case 14:	maxchars=16*50+200; break;
		case 15:	maxchars=384; break;
		case 16:	maxchars=384; break;
		case 17:	maxchars=512; break;
		case 18:	maxchars=768; break;
		case 19:	maxchars=384; break;
		case 20:	maxchars=768; break;
		case 21:	maxchars=768; break;
		case 22:	maxchars=512; break;
		case 23:	maxchars=512; break;
		case 24:	maxchars=384; break;
		case 25:	maxchars=384+8*25*2; break;
		case 26:	maxchars=256; break;
		case 27:	maxchars=2048; break;
		case 28:	maxchars=384; break;
		case 29:	maxchars=512; break;
		case 30:	maxchars=384; break;
		case 31:	maxitem=1024*40; maxchars=512; break;
		case 32:	maxchars=1280; break;
		case 33:	maxchars=1600; break;
		case 34:	maxchars=1280; break;
		case 35:	maxchars=768; break;
		case 36:	maxchars=768; break;
		case 37:	maxchars=1024; break;

		default:	maxchars=768; break;
	}
	
	// set item and effect limit
	if (!maxitem) maxitem=max(maxchars*12+10240,20480);
	if (!maxeffect) maxeffect=max(maxchars*2,1024);

	printf("serverID=%d, areaID=%d, areaM=%d, maxchars=%d, maxitem=%d, maxeffect=%d\n\n",
	       serverID,areaID,areaM,maxchars,maxitem,maxeffect);

	if (demon) {
		printf("Demonizing...\n\n");
		
		if (fork()) exit(0);
		for (n=0; n<256; n++) close(n);
		setsid();
#ifndef STAFF
		nologin=1;
#endif
	}

        // ignore the silly pipe errors:
        signal(SIGPIPE,SIG_IGN);

	// ignore sighup - just to be safe
        signal(SIGHUP,SIG_IGN);

	/*signal(SIGSEGV,sig_crash);
	signal(SIGFPE,sig_crash);
	signal(SIGBUS,sig_crash);
	signal(SIGSTKFLT,sig_crash);*/

        // shutdown gracefully if possible:
        signal(SIGQUIT,sig_leave);
        signal(SIGINT,sig_leave);
        signal(SIGTERM,sig_leave);

	// show profile info on CTRL-Z
	signal(SIGTSTP,sig_showprof);

	// init random number generator
        srand(time_now);

	if (!init_smalloc()) exit(1);
	if (!init_mem()) exit(1);
        if (!init_prof()) exit(1);
	if (!init_log()) exit(1);	
        if (!init_database()) exit(1);
	if (!init_lookup())  exit(1);
        if (!init_sector())  exit(1);
        if (!init_los()) exit(1);
	if (!init_timer()) exit(1);
        if (!init_notify()) exit(1);
	if (!init_create()) exit(1);
        if (!init_lib()) exit(1);
	if (!init_io()) exit(1);
        if (!init_path()) exit(1);	
	if (!init_effect()) exit(1);
	if (!init_container()) exit(1);
	if (!init_store()) exit(1);
	if (!init_chat()) exit(1);

	init_sound_sector();

	xlog("AreaID=%d, AreaM=%d, entering game loop...",areaID,areaM);

	dlog(0,0,"Server started");
	prof_reset();

        while (!quit) {
		sprintf(args[0],"./server -a %d -m %d -i %d # %d on %d%% load (busy)",areaID,areaM,serverID,online,(10000-server_idle)/100);
		start=rdtsc();
		lock_server();

		time_now=time(NULL);
		prof=prof_start(26); tick_date(); prof_stop(26,prof);
		prof=prof_start(22); tick_timer(); prof_stop(22,prof);
                prof=prof_start(4); tick_char(); prof_stop(4,prof);
                prof=prof_start(24); tick_effect(); prof_stop(24,prof);
		prof=prof_start(36); tick_clan(); prof_stop(36,prof);
		prof=prof_start(39); tick_club(); prof_stop(39,prof);
		prof=prof_start(5); tick_player(); prof_stop(5,prof);
		prof=prof_start(34); tick_login(); prof_stop(34,prof);
                prof=prof_start(6); pflush(); prof_stop(6,prof);
		prof=prof_start(7); io_loop(); prof_stop(7,prof);
                prof=prof_start(3); tick_chat(); prof_stop(3,prof);

		if (showprof) {
			show_prof();
			showprof=0;
		}
		
		prof=prof_start(8); prof_update(); prof_stop(8,prof);

		end=rdtsc();
		cycles=end-start;

		if ((ticker&2047)==0) {
			prof=prof_start(27); area_alive(0); prof_stop(27,prof);
			prof=prof_start(28); backup_players(); prof_stop(28,prof);
			call_stat_update();
			read_motd();			
			reinit_log();
		}

		if ((ticker&255)==0) {
			call_check_task();
			call_area_load();
			shutdown_warn();
#ifdef STAFF
			check_staff_stop();
#endif
		}

		if ((ticker&255)==168) {
			prof=prof_start(38);
			consistency_check_items();
			consistency_check_map();
			consistency_check_chars();
			consistency_check_containers();
			prof_stop(38,prof);
		}

                unlock_server();

		sprintf(args[0],"./server -a %d -m %d -i %d # %d on %d%% load (idle)",areaID,areaM,serverID,online,(10000-server_idle)/100);

		prof=prof_start(1); tick_sleep(0); prof_stop(1,prof);
		
		ticker++;
	}

        xlog("Left game loop");
	respawn_check();

	for (n=1; n<MAXCHARS; n++) {
		if (ch[n].flags&CF_PLAYER) {
			exit_char(n);
		}
	}
	area_alive(1);

        show_prof();

	dlog(0,0,"Server stopped");

	xlog("map check");
	check_map();

        exit_lib();
	exit_database();

	xlog("Clean shutdown");
	showmem();
	exit_log();
	exit_io();

        return 0;
}

// ****** profiler ********

//#define BIGPROF

#ifdef BIGPROF
#define MAXDEPTH	10
#define MAXPROF		1000

struct profile
{
	unsigned long long cycles;
	unsigned long long calls;
	unsigned char task[MAXDEPTH];
};

static struct profile prof[MAXPROF];
static unsigned int maxprof=0;
#endif

struct proftab
{
	unsigned long long cycles;
};

static struct proftab proftab[100];

static char *profname[100]={
"*ALL*",		//0
"IDLE",			//1
"sector_hear",		//2
"tick_chat",		//3
"tick_char",		//4
"tick_player",		//5
"pflush",		//6
"io_loop",		//7
"prof_update",		//8
"rec_player",		//9
"send_player",		//10
"send()",		//11
"deflate()",		//12
"los_can_see",		//13
"reset_los",		//14
"build_los",		//15
"add_light",		//16
"pathfinder",		//17
"notify_area",		//18
"char_driver",		//19
"notify_char",		//20
"set_sector",		//21
"tick_timer",		//22
"act",			//23
"tick_effect",		//24
"dlog",			//25
"tick_date",		//26
"area_alive",		//27
"backup_players",	//28
"load_player",		//29
"compute_dlight",	//30
"reset_dlight",		//31
"remove_lights",	//32
"add_lights",		//33
"tick_login",		//34
"set_data",		//35
"tick_clan",		//36
"background database",	//37
"consistency",		//38
"tick_club",		//39
};

#ifdef BIGPROF
static unsigned char task_stack[MAXDEPTH];
#endif
static int depth=0;

int init_prof(void)
{
        bzero(proftab,sizeof(proftab));
#ifdef BIGPROF
	bzero(prof,sizeof(prof));
#endif

        return 1;
}

unsigned long long prof_start(int task)
{
#ifdef BIGPROF
        task_stack[depth]=task;
#endif

	if (depth<9) depth++;
	else elog("depth overflow (%d)!",task);

	return rdtsc();
}

void prof_stop(int task,unsigned long long cycle)
{
	long long td;
#ifdef BIGPROF
	int n;

	task_stack[depth]=0;
#endif

	depth--;
	
	td=rdtsc()-cycle;
	if (td<0 || td>1000000000) { xlog("prof overflow %lld (%llu,%llu) %s %d",td,rdtsc(),cycle,profname[task],task); return; }

	if (task>=0 && task<100) {
		proftab[task].cycles+=td;
	}

        if (!depth) {
		proftab[0].cycles+=td;		
	}

#ifdef BIGPROF
	for (n=0; n<maxprof; n++)
		if (!memcmp(prof[n].task,task_stack,MAXDEPTH)) break;
	if (n==maxprof) {
		memcpy(prof[n].task,task_stack,MAXDEPTH);
		if (maxprof<MAXPROF-1) maxprof++;
	}
	prof[n].calls++;
	prof[n].cycles+=td;	
#endif
}

void prof_update(void)
{
	int n;

        for (n=0; n<100; n++) {
		proftab[n].cycles=proftab[n].cycles*0.999;
	}
	server_idle=10000.0/proftab[0].cycles*proftab[1].cycles;
}

void prof_reset(void)
{
	int n;

	for (n=0; n<100; n++) {
		proftab[n].cycles=0;
	}
}

int profsort(const void *a,const void *b)
{
	int n,m;

	n=*(int*)(a);
	m=*(int*)(b);

	if (proftab[m].cycles>proftab[n].cycles) return 1;
	if (proftab[m].cycles<proftab[n].cycles) return -1;

	return 0;
}

#ifdef BIGPROF
int profcomp(const void *va,const void *vb)
{
	const struct profile *a=va,*b=vb;
	int n;

	for (n=0; n<MAXDEPTH; n++) {
		if (a->task[n]<b->task[n]) return -1;
		if (a->task[n]>b->task[n]) return 1;
	}
	return 0;
}
#endif

/*void xcheck(void)
{
	int co,n,ser,in,cn;
	int storage[MAXCHARS];

	for (cn=1; cn<2048; cn++) {
                if (!ch[cn].flags) continue;

                if (!strcmp(ch[cn].name,"Demon")) {
			int in,n,ser;
	
			for (in=1; in<16384; in++) {
				if (it[in].driver!=30) continue;
				
				for (n=0; n<3; n++) {
					co=*(unsigned short*)(it[in].drdata+4+n*4);
					ser=*(unsigned short*)(it[in].drdata+6+n*4);
					if (co==cn && ser==ch[cn].serial) break;

					if (cn==co) printf("co=%d, ser=%d (%d,%d)\n",co,ser,cn,ch[cn].serial);
				}
				if (co==cn && ser==ch[cn].serial) break;
			}
			if (co!=cn || ser!=ch[cn].serial) {
				printf("%d: %d,%d: %d; I'm lost!\n",cn,ch[cn].x,ch[cn].y,ch[cn].serial);
			}
		} else printf("%d: %s\n",cn,ch[cn].name);
	}

#if 0				
	for (n=0; n<MAXCHARS; n++) {
		if (!(storage[n]=alloc_char())) break;
	}
	xlog("%d free chars",n);
	for (n--; n>=0; n--) {
		free_char(storage[n]);
	}
#endif
}*/

void show_prof(void)
{
	int n,m;
	double proz;
	int tab[100];
#ifdef BIGPROF
	unsigned long long total=0;
#endif

	for (n=0; n<100; n++) {
		tab[n]=n;
	}

	qsort(tab,100,sizeof(int),profsort);

	xlog("----- Profile: -----");

	for (n=1; n<100; n++) {
		m=tab[n];
		if (!proftab[m].cycles) break;
		proz=100.0/proftab[0].cycles*proftab[m].cycles;
		if (proz<0.1) break;
                xlog("%-13.13s %5.2f%%",profname[m],proz);
	}
	xlog("--------------------");	
	showmem();
#ifdef BIGPROF
	
	qsort(prof,maxprof,sizeof(struct profile),profcomp);
	
	for (n=0; n<maxprof; n++) if (!prof[n].task[1]) total+=prof[n].cycles;
	
	for (n=0; n<maxprof; n++) {
		for (m=0; m<10 && prof[n].task[m]; m++) {
			printf("%s ",profname[prof[n].task[m]]);
		}
		printf("%llu calls, %llu cycles, %.2f%%\n",prof[n].calls,prof[n].cycles,100.0/total*prof[n].cycles);
	}

	bzero(prof,sizeof(prof));
	maxprof=0;
#endif
	//xcheck();

	xlog("serials: char=%d, item=%d",sercn,serin);
}

void cmd_show_prof(int cn)
{
	int n,m;
	double proz;
	int tab[100];

	for (n=0; n<100; n++) {
		tab[n]=n;
	}

	qsort(tab,100,sizeof(int),profsort);

	log_char(cn,LOG_SYSTEM,0,"--- Profile ---");
	for (n=1; n<100; n++) {
		m=tab[n];
		if (!proftab[m].cycles) break;
		proz=100.0/proftab[0].cycles*proftab[m].cycles;
		if (proz<0.5) break;
                log_char(cn,LOG_SYSTEM,0,"%-13.13s \020%.2f%%",profname[m],proz);
	}
	log_char(cn,LOG_SYSTEM,0,"---------------");
}












