/*
 *
 * $Id: lab.c,v 1.2 2006/03/30 12:16:01 ssim Exp $
 *
 * $Log: lab.c,v $
 * Revision 1.2  2006/03/30 12:16:01  ssim
 * changed some elogs to xlogs to avoid cluttering the error log file with unimportant errors
 *
 */

#include "server.h"
#include "drdata.h"
#include "database.h"
#include "tool.h"
#include "create.h"
#include "map.h"
#include "drvlib.h"
#include "talk.h"
#include "consistency.h"
#include "log.h"
#include "lab.h"

int count_solved_labs(int cn)
{
	struct lab_ppd *ppd;
	int cnt,n;
	unsigned long long bit;

	if (!(ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd)))) return 0;	// OOPS

	for (n=cnt=0; n<64; n++) {
		bit=1ull<<n;
		if (ppd->solved_bits&bit) cnt++;		
	}
	return cnt;
}

static int teleport_lab(int cn,int lab_level,int do_teleport)
{
	switch(lab_level) {
		case 10:	if (ch[cn].level<10) return -10;
				if (!do_teleport || change_area(cn,22,27,242)) return 1; else return -1;
		case 15:	if (ch[cn].level<12) return -12;
				if (!do_teleport || change_area(cn,22,69,105)) return 1; else return -1;
		case 20:	if (ch[cn].level<15) return -15;
				if (!do_teleport || change_area(cn,22,227,250)) return 1; else return -1;
		case 25:	if (ch[cn].level<20) return -20;
				if (!do_teleport || change_area(cn,22,144,103)) return 1; else return -1;
		case 30:	if (ch[cn].level<25) return -25;
				if (!do_teleport || change_area(cn,22,163,243)) return 1; else return -1;
		default:	return 0;
	}
}

int teleport_next_lab(int cn,int do_teleport)
{
	struct lab_ppd *ppd;
	unsigned long long bit;
	int n,tmp;

	if (!(ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd)))) return 0;	// OOPS

	for (n=0; n<64; n++) {
                bit=1ull<<n;
		if (!(ppd->solved_bits&bit) && (tmp=teleport_lab(cn,n,do_teleport))) return tmp;
	}

	return 0;
}

// sets bits and gives exp
int set_solved_lab(int cn,int lab_level)
{
	struct lab_ppd *ppd;
	unsigned long long bit;

	if (lab_level<0 || lab_level>63) return 0;

	if (!(ppd=set_data(cn,DRD_LAB_PPD,sizeof(struct lab_ppd)))) return 0;	// OOPS

	bit=1ull<<lab_level;

	if (!(ppd->solved_bits&bit)) {
		ppd->solved_bits|=bit;
		give_exp(cn,level_value(lab_level)/5);
		log_char(cn,LOG_SYSTEM,0,"Congratulations, %s, you have solved this part of the labyrinth.",ch[cn].name);
	}

	return 1;
}

int create_lab_exit(int cn,int level)
{
	int in;

	in=create_item("labexit");
	if (!in) return 0;

	if (!drop_item_extended(in,ch[cn].x,ch[cn].y,4)) {
		destroy_item(in);
		xlog("could not drop lab exit for player %s, lab level %d at %d,%d",ch[cn].name,level,ch[cn].x,ch[cn].y);
		return 0;
	} else xlog("created lab gate for %s, level %d, at %d,%d (char is at %d,%d)",ch[cn].name,level,it[in].x,it[in].y,ch[cn].x,ch[cn].y);
	
	*(unsigned int*)it[in].drdata=ch[cn].ID;
	it[in].drdata[4]=level;

	return 1;
}







