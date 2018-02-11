/*
 * $Id: store.c,v 1.6 2007/09/21 11:01:41 devel Exp $
 *
 * $Log: store.c,v $
 * Revision 1.6  2007/09/21 11:01:41  devel
 * tactics fix
 *
 * Revision 1.5  2007/08/09 11:14:30  devel
 * statistics
 *
 * Revision 1.4  2007/07/24 18:56:47  devel
 * made /support work with buy and sell
 *
 * Revision 1.3  2007/07/04 09:24:04  devel
 * added /support support
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "error.h"
#include "create.h"
#include "tool.h"
#include "talk.h"
#include "mem.h"
#include "statistics.h"
#include "drvlib.h"
#include "consistency.h"
#include "map.h"
#include "depot.h"
#include "store.h"
#include "database.h"
#include "clan.h"
#include "balance.h"
#include "prof.h"

struct store **store;

static int objcmp(struct item *a,struct item *b)
{
	if (strcmp(a->description,b->description)) return 0;
	if (memcmp(a->drdata,b->drdata,sizeof(a->drdata))) return 0;
	if (a->driver!=b->driver) return 0;
	if (a->flags!=b->flags) return 0;
        if (strcmp(a->name,b->name)) return 0;
	if (a->sprite!=b->sprite) return 0;
	if (a->value!=b->value) return 0;
	if (a->ownerID!=b->ownerID) return 0;
	if (memcmp(a->mod_index,b->mod_index,sizeof(a->mod_index))) return 0;
	if (memcmp(a->mod_value,b->mod_value,sizeof(a->mod_value))) return 0;
	if (a->quality!=b->quality) return 0;
	if (a->complexity!=b->complexity) return 0;


	return 1;
}

int salesprice(int cn,int co,int nr)
{
	int s,price,skill;
	double mult;
	

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	s=ch[cn].store;
	if (s<1 || s>=MAXSTORE) { error=ERR_ILLEGAL_STORENO; return 0; }
	if (nr<0 || nr>=STORESIZE) { error=ERR_ILLEGAL_STOREPOS; return 0; }

	if (store[s]->ware[nr].cnt) {
		price=store[s]->ware[nr].item.value;
		
		skill=ch[co].value[0][V_BARTER]+tactics2skill(ch[co].value[0][V_TACTICS])+get_support_prof(co,P_TRADER)*5+clan_trade_bonus(co);
		
		mult=1.55-(skill/1000.0);
		if (mult<1.05) mult=1.05;
		
		price=price*mult;

	} else price=0;

        return price;
}

int buyprice(int cn,int in)
{
	int price,skill;
	double mult;

	if (in<1 || in>=MAXITEM) { error=ERR_ILLEGAL_ITEMNO; return 0; }

	price=it[in].value;
	
	if (!(it[in].flags&IF_MONEY)) {
		skill=ch[cn].value[0][V_BARTER]+tactics2skill(ch[cn].value[0][V_TACTICS])+get_support_prof(cn,P_TRADER)*5+clan_trade_bonus(cn);
		
		mult=0.75+(skill/2500.0);
		if (mult>0.95) mult=0.95;
		
		price=price*mult;
	}

	return price;
}

// merchant selling
int sell(int cn,int co,int nr)
{
	int s,price,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	s=ch[cn].store;
	if (s<1 || s>=MAXSTORE) { error=ERR_ILLEGAL_STORENO; return 0; }
	if (nr<0 || nr>=STORESIZE) { error=ERR_ILLEGAL_STOREPOS; return 0; }
	if (ch[co].citem) { error=ERR_HAVE_CITEM; return 0; }

	if (store[s]->ware[nr].cnt<1) { error=ERR_SOLD_OUT; return 0; }

	price=salesprice(cn,co,nr);
	
	if (ch[co].gold<price) { error=ERR_GOLD_LOW; return 0; }

	in=alloc_item();
	if (!in) { error=ERR_CONFUSED; return 0; }
	
        it[in]=store[s]->ware[nr].item;

	if (!can_carry(co,in,0)) { error=ERR_NOT_TAKEABLE; return 0; }

	dlog(co,in,"bought %s for %.2fG",it[in].name,price/100.0);

	ch[co].gold-=price;

	ch[co].citem=in;
	ch[co].flags|=CF_ITEMS;
	it[in].carried=co;

	if (!store[s]->ware[nr].always) { store[s]->ware[nr].cnt--; }

	return price;
}

static void add_item_to_store(int s,int in)
{
	int n;

	for (n=0; n<STORESIZE; n++)
		if (store[s]->ware[n].cnt && objcmp(&store[s]->ware[n].item,&it[in]))
			break;
	if (n==STORESIZE) {	// no such item in stock yet
		for (n=0; n<STORESIZE; n++)
			if (!store[s]->ware[n].cnt)
				break;
		if (n==STORESIZE) {	// no free item slot, overwrite random non-always one
			do {
				n=RANDOM(STORESIZE);
			} while (store[s]->ware[n].always);
			
			store[s]->ware[n].cnt=0;
		}
		store[s]->ware[n].item=it[in];
	}
	store[s]->ware[n].cnt++;
}

int add_special_store(int cn)
{
	int in,str,s;

        if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	s=ch[cn].store;
	if (s<1 || s>=MAXSTORE) { error=ERR_ILLEGAL_STORENO; return 0; }

	str=RANDOM(20)+1;

	in=create_special_item(str,1,1000);

	add_item_to_store(s,in);

	destroy_item(in);

	return 1;
}

// merchant buying
int buy(int cn,int co)
{
	int s,price,in;

	if (cn<1 || cn>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }
	if (co<1 || co>=MAXCHARS) { error=ERR_ILLEGAL_CHARNO; return 0; }

	s=ch[cn].store;
	if (s<1 || s>=MAXSTORE) { error=ERR_ILLEGAL_STORENO; return 0; }
        if (!(in=ch[co].citem)) { error=ERR_NO_CITEM; return 0; }

	if (!(it[in].flags&(IF_QUEST|IF_NODEPOT|IF_BONDTAKE|IF_LABITEM|IF_MONEY))) {	// only add non-quest items to store
		add_item_to_store(s,in);
	}
	
        price=buyprice(co,in);
        ch[co].gold+=price; stats_update(co,0,price);

	dlog(co,in,"sold %s for %.2fG",it[in].name,price/100.0);

	ch[co].citem=0;
	ch[co].flags|=CF_ITEMS;
	free_item(in);

	return price;
}

int create_store(int cn,int ignore)
{
	int s,n,in;

	for (s=1; s<MAXSTORE; s++)
		if (!store[s]) break;
	if (s==MAXSTORE) return 0;

	store[s]=xcalloc(sizeof(struct store),IM_STORE);

	ch[cn].store=s;

        for (n=ignore; n<INVENTORYSIZE-30; n++) {
		if (!(in=ch[cn].item[n+30])) continue;

		remove_item_char(in);
		store[s]->ware[n-ignore].cnt=1;
		store[s]->ware[n-ignore].always=1;
                store[s]->ware[n-ignore].item=it[in];

		ch[cn].item[n+30]=0;
		free_item(in);
	}

	return 1;
}

// character (usually a player) cn is using store NR
// flag=1 buy/sell, flag=0 look
void player_store(int cn,int nr,int flag,int fast)
{
	int co,s,price;

	co=ch[cn].merchant;
	if (co<1 || co>=MAXCHARS) {
		elog("player_store: got illegal co=%d",co);
		return;
	}

	if (nr<0 || nr>=STORESIZE) {
		elog("player_store: got illegal nr=%d",nr);
	}

        if (flag) {
		if (ch[cn].citem) {
			price=buy(co,cn);
			if (price) {
				if (price<100) log_char(cn,LOG_SYSTEM,0,"Sold for %dS",price);
				else log_char(cn,LOG_SYSTEM,0,"Sold for %dG %dS",price/100,price%100);
			}
		} else {
			price=sell(co,cn,nr);
			if (price) {
				if (price<100) log_char(cn,LOG_SYSTEM,0,"Bought for %dS",price);
				else log_char(cn,LOG_SYSTEM,0,"Bought for %dG %dS",price/100,price%100);
				if (fast) store_citem(cn);
			} else if (error==ERR_GOLD_LOW) {
				log_char(cn,LOG_SYSTEM,0,"Sorry, that's too expensive for you.");
			}
		}
	} else {
		s=ch[co].store;
		if (s<1 || s>MAXSTORE) {
			elog("player_store: got illegal s=%d",s);
			return;
		}
		if (!store[s]->ware[nr].cnt) return;
		look_item(cn,&store[s]->ware[nr].item);
	}
}

int init_store(void)
{
	store=xcalloc(sizeof(struct store*)*MAXSTORE,IM_BASE);
	if (!store) return 0;
	xlog("Allocated store: %.2fM (%d*%d)",sizeof(struct store*)*MAXSTORE/1024.0/1024.0,sizeof(struct store*),MAXSTORE);
	mem_usage+=sizeof(struct store*)*MAXSTORE;

	return 1;
}















