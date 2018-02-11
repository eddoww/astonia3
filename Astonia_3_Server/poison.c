/*

$Id: poison.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: poison.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:35  sam
Added RCS tags


*/

#include "server.h"
#include "tool.h"
#include "talk.h"
#include "log.h"
#include "death.h"
#include "timer.h"
#include "create.h"
#include "drvlib.h"
#include "consistency.h"
#include "poison.h"


#define POISONDURATION	(TICKS*60*60*2)

void poison_someone(int cn,int pwr,int type)
{
	int fre,in,endtime;

	if (type<0 || type>3) return;
        if (!(fre=may_add_spell(cn,IDR_POISON0+type))) return;

        in=create_item("poison");
	if (!in) return;

        it[in].mod_value[0]=-1;
	
	it[in].driver=IDR_POISON0+type;

	endtime=ticker+POISONDURATION;

	*(signed long*)(it[in].drdata)=endtime;
	*(signed long*)(it[in].drdata+4)=ticker;
	*(unsigned short*)(it[in].drdata+8)=pwr;
	*(unsigned short*)(it[in].drdata+10)=9;

	it[in].carried=cn;

        ch[cn].item[fre]=in;

        create_spell_timer(cn,in,fre);

	update_char(cn);

	log_char(cn,LOG_SYSTEM,0,"You have been poisoned.");
}

void poison_callback(int cn,int in,int pos,int cserial,int iserial)
{
	int pwr,tick;

	// char alive and the right one?
	if (!(ch[cn].flags) || ch[cn].serial!=cserial) return;

	// item existant and the right one?
	if (!(it[in].flags) || it[in].serial!=iserial) return;

	// item where we expect it to be?
	if (ch[cn].item[pos]!=in) return;

	pwr=*(unsigned short*)(it[in].drdata+8);
	tick=*(unsigned short*)(it[in].drdata+10);
	if (pwr>20) pwr=20;
	if (pwr<1) pwr=1;	

	switch(it[in].driver) {
		case IDR_POISON0:
		case IDR_POISON1:
		case IDR_POISON2:
		case IDR_POISON3:	if (!tick) {
						if (it[in].mod_value[0]>-1000) it[in].mod_value[0]--;
						update_char(cn);
					}
					hurt(cn,POWERSCALE/3,0,1,0,50);
					break;
	}
	if (tick==0) tick=9;
	else tick--;
	*(unsigned short*)(it[in].drdata+10)=tick;
	
	set_timer(ticker+TICKS*2/pwr,poison_callback,cn,in,pos,ch[cn].serial,it[in].serial);
}

int remove_all_poison(int cn)
{
	int n,in,flag=0;

	for (n=12; n<30; n++) {
		if ((in=ch[cn].item[n]) && it[in].driver>=IDR_POISON0 && it[in].driver<=IDR_POISON3) {
			ch[cn].item[n]=0;
			free_item(in);
			flag=1;
		}
	}
	if (flag) {
		update_char(cn);
	}
	return flag;
}

int remove_poison(int cn,int type)
{
	int n,in,flag=0;

	if (type<0 || type>3) return 0;

	for (n=12; n<30; n++) {
		if ((in=ch[cn].item[n]) && it[in].driver==IDR_POISON0+type) {
			ch[cn].item[n]=0;
			free_item(in);
			flag=1;
		}
	}
	if (flag) {
		update_char(cn);
	}
	return flag;
}
