/*
 * $Id: questlog.c,v 1.15 2008/03/24 11:22:43 devel Exp $
 *
 * $Log: questlog.c,v $
 * Revision 1.15  2008/03/24 11:22:43  devel
 * arkhata
 *
 * Revision 1.14  2007/12/10 10:12:11  devel
 * added new carlos quests
 *
 * Revision 1.13  2007/06/22 12:57:48  devel
 * added caligar
 *
 * Revision 1.12  2007/05/26 13:20:31  devel
 * added first caligar quests
 *
 * Revision 1.11  2007/03/06 10:43:42  devel
 * added destruction of quest items on bodies
 *
 * Revision 1.10  2006/10/08 17:41:10  devel
 * some small fixes
 * fixed critical bg in destroy_item_byID()
 *
 * Revision 1.9  2006/10/06 18:14:38  devel
 * made impish bearhunt repeatable
 *
 * Revision 1.8  2006/10/04 17:29:09  devel
 * made quests with usable quest items not-repeatable for now
 *
 * Revision 1.7  2006/09/28 11:58:38  devel
 * added random dungeon to questlog
 * added grimroot to questlog
 *
 * Revision 1.6  2006/09/27 11:40:43  devel
 * added questlog to brannington
 *
 * Revision 1.5  2006/09/26 10:59:25  devel
 * added questlog to nomad plains and brannington forest
 *
 * Revision 1.4  2006/09/25 14:07:57  devel
 * added questlog to forest and exkordon
 *
 * Revision 1.3  2006/09/23 10:59:49  devel
 * added swamp quests to questlog
 *
 * Revision 1.2  2006/09/21 11:24:04  devel
 * added most aston quests
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "server.h"
#include "log.h"
#include "notify.h"
#include "direction.h"
#include "do.h"
#include "path.h"
#include "error.h"
#include "drdata.h"
#include "see.h"
#include "death.h"
#include "talk.h"
#include "effect.h"
#include "tool.h"
#include "store.h"
#include "date.h"
#include "map.h"
#include "create.h"
#include "drvlib.h"
#include "libload.h"
#include "quest_exp.h"
#include "item_id.h"
#include "skill.h"
#include "area1.h"
#include "area3.h"
#include "consistency.h"
#include "database.h"
#include "questlog.h"
#include "btrace.h"
#include "player.h"
#include "depot.h"
#include "staffer_ppd.h"
#include "two_ppd.h"
#include "nomad_ppd.h"
#include "container.h"
#include "chat.h"

#define QLF_REPEATABLE	(1u<<0)
#define QLF_XREPEAT	(1u<<1)

struct questlog {
	char *name;
        int minlevel,maxlevel;
	char *giver;
	char *area;
	int exp;
	unsigned int flags;
};

struct questlog questlog[]={
	{"Lydia's Potion",1,2,"James","Cameron",15,QLF_REPEATABLE},			//0,
	{"Find the Magic Item",2,3,"Gwendylon","Cameron",75,QLF_REPEATABLE},		//1,
	{"The Second Skull",3,5,"Gwendylon","Cameron",150,QLF_REPEATABLE},		//2,
	{"The Third Skull",5,7,"Gwendylon","Cameron",300,QLF_REPEATABLE},		//3,
	{"Kill the Foul Magician",6,8,"Gwendylon","Cameron",800,QLF_REPEATABLE},	//4,
	{"Bear Hunt",6,8,"Yoakin","Cameron",600,QLF_REPEATABLE},			//5,
	{"A Fool's Request",6,8,"Nook","Cameron",400,0},				//6,
	{"Mages Gone Berserk",6,9,"Guiwynn","Cameron",800,QLF_REPEATABLE},		//7,
	{"The Recipe for Happiness",7,10,"Guiwynn","Cameron",900,QLF_REPEATABLE},	//8,
	{"Knightly Troubles",7,10,"Logain","Cameron",1200,QLF_REPEATABLE},		//9,
	{"Loisan's House",9,12,"Seymour","Aston",850,0},				//10,
	{"The Silver Skull",10,13,"Seymour","Aston",1000,0},				//11,
	{"Find Loisan",11,15,"Seymour","Aston",1500,QLF_REPEATABLE},			//12,
	{"Jeepers Creepers",12,18,"Kelly","Aston",1850,QLF_REPEATABLE},			//13,
	{"Underground Park Shrines",15,20,"Kelly","Aston",0,0},				//14, special case: exp awarded in driver, 4500 exp total
	{"In Search of Clara",20,27,"Kelly","Aston",2500,0},				//15,
	{"The Astronomer's Notes",15,20,"Gerassimo","Aston",5000,QLF_REPEATABLE},	//16,
	{"The Unwanted Tenants",9,12,"Reskin","Cameron",1250,0},			//17,
	{"The Toughest Monster",20,25,"Sir Jones","Aston",7500,0},			//18,
	{"The Toughestest Monster",20,26,"Sir Jones","Aston",12000,0},			//19,
	{"Wanted: Occult Staff",30,36,"Carlos","Aston",40000,QLF_REPEATABLE},		//20,
	{"Slay the Swampbeast",23,30,"Clara","Swamp",22500,0},				//21,
	{"Impish Bear Hunt",20,27,"William/Imp","Forest",12500,0},			//22,
	{"Praying Mantis Stew",20,27,"William","Forest",15000,0},			//23,
	{"The Spider Queen",25,30,"Hermit","Forest",25000,0},				//24,
	{"Earning the Lockpick",25,30,"Guildmaster","Exkordon",0,QLF_XREPEAT},		//25, exp awarded in driver, amount depends on robbers killed. range: 5000 to 20000
	{"Extortion",25,30,"Guildmaster","Exkordon",0,QLF_XREPEAT},			//26, exp awarded in driver, 5000 or 10000
	{"Price Fix Exposed",25,30,"Guildmaster","Exkordon",15000,QLF_XREPEAT},		//27,
	{"The Golden Lockpick",26,33,"Guildmaster","Exkordon",15000,QLF_XREPEAT},	//28,
	{"Dirty Hands",26,33,"Sanwyn","Exkordon",0,0},					//29, exp awarded in driver, 45000 total
	{"The Old Governor's Cross",33,40,"Skeleton","Exkordon",30000,QLF_REPEATABLE},	//30,
	{"Spider Poison",30,40,"Cervik","Exkordon",30000,QLF_REPEATABLE},		//31,
        {"Join the Tribe",63,80,"Kalanur","Nomad Plains",10000,0},			//32,
	{"Searching Sarkilar",63,80,"Kir Laas","Nomad Plains",450000,0},		//33,
	{"A Golden Statue",72,90,"Kir Garan","Nomad Plains",280000,0},			//34,
	{"Smuggler Book",10,15,"Imp. Commander","Below Aston 2",1000,QLF_REPEATABLE},	//35,
	{"Contraband",10,15,"Imp. Commander","Below Aston 2",0,0},			//36, exp awarded in driver, 5000 total
	{"Smuggler Leader",10,15,"Imp. Commander","Below Aston 2",2000,QLF_REPEATABLE},	//37,
	{"The Family Heirloom",32,40,"Aristocrat","Bran. Forest",40000,QLF_REPEATABLE},	//38,
	{"Bear Hunt - Again",32,36,"Yoatin","Bran. Forest",40000,QLF_REPEATABLE},	//39,
	{"The Jewels of Brannington ",34,40,"Count B.","Brannington",0,QLF_REPEATABLE},	//40, exp awarded in driver, 120k total
	{"A Grolm's Spoils",33,42,"Brenneth","Brannington",15000,QLF_REPEATABLE},	//41,
	{"A Thief's Loot ",33,42,"Brenneth","Brannington",15000,QLF_REPEATABLE},	//42,
	{"A Necromancer's Notes",33,42,"Brenneth","Brannington",15000,QLF_REPEATABLE},	//43,
	{"A Rest Disturbed",36,43,"Spirit","Brannington",60000,QLF_REPEATABLE},		//44,
	{"Searching a Miner's Tool",42,48,"Broklin","Brannington",60000,QLF_REPEATABLE},//45,
	{"A Miner's Vengeance",44,50,"Broklin","Brannington",60000,0},			//46,
	{"A Miner's Misery",85,95,"Dwarven Chief","Grimroot",285000,0},			//47,
	{"A Miner's Bane",95,105,"Dwarven Chief","Grimroot",395000,0},			//48,
	{"A Miner's Anguish",105,115,"Dwarven Chief","Grimroot",525000,0},		//49,
	{"A Miner Lost",115,125,"Dwarven Chief","Grimroot",680000,0},			//50,
	{"Lizard's Teeth",95,105,"Dwarven Shaman","Grimroot",395000,0},			//51,
	{"Collecting Berries",100,110,"Dwarven Shaman","Grimroot",455000,0},		//52,
	{"Elitist Head",105,115,"Dwarven Shaman","Grimroot",525000,0},			//53,
	
	{"Looking for Caligar",55,65,"Kelly","Aston",80000,0},				//54,
	{"Fighting Styles",55,65,"Glori","Caligar",80000,0},				//55,
	{"Obelisk Hunt",55,65,"Glori","Caligar",80000,0},				//56,
	{"Find the Keyparts",55,65,"Glori","Caligar",80000,0},				//57,
	{"Assemble the Key",55,65,"Glori","Caligar",80000,0},				//58,
	{"Amazon Invaders",55,65,"Homdem","Caligar",80000,0},				//59,
	{"The Emperor's Plaque",55,65,"Kelly","Aston",160000,0},			//60,

	{"The Imperial Vault",26,28,"Carlos","Aston",20000,0},				//61,
	{"Tunnel Magics",26,28,"Rouven","Imperial Vault",10000,0},			//62,
	{"Chronicles of Seyan",26,28,"Rouven","Imperial Vault",10000,0},		//63,

	{"Finding Arkhata",47,55,"Guard","Brannington",60000,0},			//64,
	{"Rammy's Crown",48,58,"Rammy","Arkhata",60000,0},				//65,
	{"Ishtar's Bracelet",49,59,"Jaz","Arkhata",60000,0},				//66,
	{"Queen Fiona's Ring",50,60,"Queen Fiona","Arkhata",60000,0},			//67,
	{"A Shopkeeper's Fright",51,61,"Ramin","Arkhata",60000,0},			//68,
	{"The Monks' Request",52,62,"Johnatan","Arkhata",60000,0},			//69,
	{"The Book Eater",53,63,"Tracy","Arkhata",60000,0},				//70,
	{"Entrance Passes",54,64,"Rammy","Arkhata",90000,0},				//71,
	{"The Source",60,70,"Jada","Arkhata",120000,0},					//72,
	{"Ceremonial Pot",48,58,"Pot Maker","Arkhata",60000,0},				//73,
	{"The Lost Secrets",49,59,"Thai Pan","Arkhata",60000,0},			//74,
	{"A Kidnapped Student",53,63,"Trainer","Arkhata",60000,0},			//75,
	{"The Traitors",53,63,"Clerk","Arkhata",60000,0},				//76,
	{"The Blue Harpy",58,68,"Hunter","Arkhata",60000,0},				//77,
	{"The Mysterious Language",60,65,"Johnatan","Arkhata",60000,0},			//78,


};

void questlog_open(int cn,int qnr)
{
	struct quest *quest;

	if (qnr<0 || qnr>=MAXQUEST) {
		elog("trying to set flag for quest %d",qnr);
		btrace("questlog");
		return;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return;

	quest[qnr].flags=QF_OPEN;
	sendquestlog(cn,ch[cn].player);
}

void questlog_close(int cn,int qnr)
{
	struct quest *quest;

	if (qnr<0 || qnr>=MAXQUEST) {
		elog("trying to set flag for quest %d",qnr);
		btrace("questlog");
		return;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return;

	if (quest[qnr].flags==QF_OPEN) quest[qnr].flags=QF_DONE;
	sendquestlog(cn,ch[cn].player);
}

int questlog_scale(int cnt,int ex)
{
	switch(cnt) {
		case 0:		return ex;
		case 1:		return ex*82/100;
		case 2:		return ex*68/100;
		case 3:		return ex*56/100;
		case 4:		return ex*46/100;
		case 5:		return ex*38/100;
		case 6:		return ex*32/100;
		case 7:		return ex*26/100;
		case 8:		return ex*21/100;
		case 9:		return ex*18/100;
		default:	return ex*15/100;
	}
}

int questlog_done(int cn,int qnr)
{
	struct quest *quest;
	int cnt,val;

	if (qnr<0 || qnr>=MAXQUEST) {
		elog("trying to set done for quest %d",qnr);
		btrace("questlog");
		return 0;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return 0;

	cnt=quest[qnr].done++;
	quest[qnr].flags=QF_DONE;

	val=questlog_scale(cnt,questlog[qnr].exp);

	// scale down by level for those rushing ahead
	if (ch[cn].level>44) val=min(level_value(ch[cn].level)/6,val);
	else if (ch[cn].level>19) val=min(level_value(ch[cn].level)/4,val);
        else if (ch[cn].level>4) val=min(level_value(ch[cn].level)/2,val);
	else val=min(level_value(ch[cn].level),val);

	give_exp(cn,val);
        if (questlog[qnr].exp>0) dlog(cn,0,"Received %d exp for doing quest %s for the %d. time (nominal value %d exp)",val,questlog[qnr].name,(cnt+1),questlog[qnr].exp);
	sendquestlog(cn,ch[cn].player);

	return quest[qnr].done;
}

int questlog_count(int cn,int qnr)
{
	struct quest *quest;
	if (qnr<0 || qnr>=MAXQUEST) {
		elog("trying to get count for quest %d",qnr);
		btrace("questlog");
		return 0;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return 0;

	return quest[qnr].done;
}

int questlog_isdone(int cn,int qnr)
{
	struct quest *quest;

	if (qnr<0 || qnr>=MAXQUEST) {
		elog("trying to get done for quest %d",qnr);
		btrace("questlog");
		return 0;
	}

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return 0;

	if (quest[qnr].flags==QF_DONE) return 1;
	else return 0;
}

void questlog_reopen_q0(int cn)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return;
	
	ppd->james_state=0;
	ppd->lydia_state=0;
}

int questlog_reopen_q1(int cn,int state,struct quest *quest)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return 0;
	if ((quest[1].flags&QF_OPEN) || (quest[2].flags&QF_OPEN) || (quest[3].flags&QF_OPEN) || (quest[4].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}

        ppd->gwendy_state=state;
	return 1;
}

void questlog_reopen_q5(int cn)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return;
	
	ppd->yoakin_state=0;
}

void questlog_reopen_q6(int cn)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return;
	
	ppd->nook_state=0;
}

int questlog_reopen_q7(int cn,int state,struct quest *quest)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return 0;
	if ((quest[7].flags&QF_OPEN) || (quest[8].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
	
	ppd->guiwynn_state=state;
	return 1;
}

void questlog_reopen_q9(int cn)
{
	struct area1_ppd *ppd;
	
	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return;
	
	ppd->logain_state=0;
}

int questlog_reopen_q10(int cn,int state,struct quest *quest)
{
	struct area3_ppd *ppd;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if ((quest[10].flags&QF_OPEN) || (quest[11].flags&QF_OPEN) || (quest[12].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
	
	ppd->seymour_state=state;
	return 1;
}

void questlog_reopen_q13(int cn)
{
	struct area3_ppd *ppd;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	
	ppd->kelly_state=0;
}

void questlog_reopen_q16(int cn)
{
	struct area3_ppd *ppd;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	
	ppd->astro2_state=0;
}

int questlog_reopen_q18(int cn,int state,struct quest *quest)
{
	struct area3_ppd *ppd;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if ((quest[18].flags&QF_OPEN) || (quest[19].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
	
	ppd->crypt_state=state;
	return 1;
}

void questlog_reopen_q20(int cn)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	
	ppd->carlos_state=0;
}

int questlog_reopen_q22(int cn,struct quest *quest)
{
	struct area3_ppd *ppd;
	
	ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd));
	if ((quest[22].flags&QF_OPEN) || (quest[23].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
	
	ppd->william_state=0;
	ppd->imp_state=0;
	ppd->imp_kills=0;
	return 1;
}

void questlog_reopen_q30(int cn)
{
	struct twocity_ppd *ppd;

	ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	
	ppd->skelly_state=0;
}

void questlog_reopen_q31(int cn)
{
	struct twocity_ppd *ppd;

	ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));
	
	ppd->alchemist_state=0;
}

int questlog_reopen_q35(int cn,int state,struct quest *quest)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	if ((quest[35].flags&QF_OPEN) || (quest[36].flags&QF_OPEN) || (quest[37].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
        if (state==5) ppd->smugglecom_bits=0;

	ppd->smugglecom_state=state;
	return 1;
}

void questlog_reopen_q38(int cn)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	
	ppd->aristocrat_state=0;
}

void questlog_reopen_q39(int cn)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	
	ppd->yoatin_state=0;
}

void questlog_reopen_q40(int cn)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	
	ppd->countbran_state=0;
	ppd->countbran_bits&=~(1|2|4);
}

int questlog_reopen_q41(int cn,int state,struct quest *quest)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	if ((quest[41].flags&QF_OPEN) || (quest[42].flags&QF_OPEN) || (quest[43].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
        ppd->brennethbran_state=state;
	return 1;
}

void questlog_reopen_q44(int cn)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	
	ppd->spiritbran_state=0;
}

int questlog_reopen_q45(int cn,int state,struct quest *quest)
{
	struct staffer_ppd *ppd;
	
	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));
	if ((quest[45].flags&QF_OPEN) || (quest[46].flags&QF_OPEN)) {
		log_char(cn,LOG_SYSTEM,0,"Cannot re-open more than one quest from a series.");
		return 0;
	}
        ppd->broklin_state=state;
	return 1;
}



void questlog_reopen(int cn,int qnr)
{
	struct quest *quest;
	int ret=1;

	if (qnr<0 || qnr>=MAXQUEST) return;

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return;
	if (quest[qnr].done>9) {
		log_char(cn,LOG_SYSTEM,0,"You cannot open this quest again.");
		return;
	}
	if ((!questlog[qnr].flags&QLF_REPEATABLE)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot open this quest again.");
		return;
	}
	if (!(quest[qnr].flags&QF_DONE)) {
		log_char(cn,LOG_SYSTEM,0,"You cannot open this quest at the moment.");
		return;
	}

	switch(qnr) {
		case 0:		questlog_reopen_q0(cn); break;
		case 1:		ret=questlog_reopen_q1(cn,0,quest); break;
		case 2:		ret=questlog_reopen_q1(cn,6,quest); break;
		case 3:		ret=questlog_reopen_q1(cn,10,quest); break;
		case 4:		ret=questlog_reopen_q1(cn,13,quest); break;
		case 5:		questlog_reopen_q5(cn); break;
		case 6:		ret=0; break; //questlog_reopen_q6(cn); break;
		case 7:		ret=questlog_reopen_q7(cn,0,quest); break;
		case 8:		ret=questlog_reopen_q7(cn,6,quest); break;
		case 9:		questlog_reopen_q9(cn); break;
		case 10:	ret=0; break;
		case 11:	ret=0; break;
		case 12:	ret=questlog_reopen_q10(cn,12,quest); break;
		case 13:	questlog_reopen_q13(cn); break;
		case 14:	ret=0; break;
		case 15:	ret=0; break;
		case 16:	questlog_reopen_q16(cn); break;
		case 17:	ret=0; break;
		case 18:	ret=0; break; //ret=questlog_reopen_q18(cn,0,quest); break;
		case 19:	ret=0; break; //ret=questlog_reopen_q18(cn,12,quest); break;
		case 20:	questlog_reopen_q20(cn); break;
		case 21:	ret=0; break;
		case 22:	ret=questlog_reopen_q22(cn,quest); break;
		case 23:	ret=0; break;
		case 24:	ret=0; break;
		case 25:	ret=0; break;
		case 26:	ret=0; break;
		case 27:	ret=0; break;
		case 28:	ret=0; break;
		case 29:	ret=0; break;
		case 30:	questlog_reopen_q30(cn); break;
		case 31:	questlog_reopen_q31(cn); break;
		case 32:	ret=0; break;
		case 33:	ret=0; break;
		case 34:	ret=0; break;
		case 35:	ret=questlog_reopen_q35(cn,0,quest); break;
		case 36:	ret=0; //questlog_reopen_q35(cn,5,quest); break;
		case 37:	ret=questlog_reopen_q35(cn,7,quest); break;
		case 38:	questlog_reopen_q38(cn); break;
		case 39:	questlog_reopen_q39(cn); break;
		case 40:	questlog_reopen_q40(cn); break;
		case 41:	ret=questlog_reopen_q41(cn,0,quest); break;
		case 42:	ret=questlog_reopen_q41(cn,5,quest); break;
		case 43:	ret=questlog_reopen_q41(cn,9,quest); break;
		case 44:	questlog_reopen_q44(cn); break;
		case 45:	ret=questlog_reopen_q45(cn,0,quest); break;
		case 46:	ret=0; break;
		case 47:	ret=0; break;
		case 48:	ret=0; break;
		case 49:	ret=0; break;
		case 50:	ret=0; break;
		case 51:	ret=0; break;
		case 52:	ret=0; break;
		case 53:	ret=0; break;

	}
	if (ret) quest[qnr].flags=QF_OPEN;
	sendquestlog(cn,ch[cn].player);
}

static void questlog_init_area1(int cn,struct quest *quest)
{
	struct area1_ppd *ppd;

	if (!(ppd=set_data(cn,DRD_AREA1_PPD,sizeof(struct area1_ppd)))) return;

	if (ppd->lydia_state>=6) {
		if (!quest[0].done) quest[0].done=1;
		quest[0].flags=QF_DONE;
	} else if (ppd->lydia_state>0) {
                quest[0].flags=QF_OPEN;
	} else {
		quest[0].flags=0;
	}

	if (ppd->gwendy_state>=17) {
		if (!quest[1].done) quest[1].done=1;
		quest[1].flags=QF_DONE;
		if (!quest[2].done) quest[2].done=1;
		quest[2].flags=QF_DONE;
		if (!quest[3].done) quest[3].done=1;
		quest[3].flags=QF_DONE;
		if (!quest[4].done) quest[4].done=1;
		quest[4].flags=QF_DONE;
	} else if (ppd->gwendy_state>=13) {
		if (!quest[1].done) quest[1].done=1;
		quest[1].flags=QF_DONE;
		if (!quest[2].done) quest[2].done=1;
		quest[2].flags=QF_DONE;
		if (!quest[3].done) quest[3].done=1;
		quest[3].flags=QF_DONE;
		quest[4].flags=QF_OPEN;
	} else if (ppd->gwendy_state>=10) {
		if (!quest[1].done) quest[1].done=1;
		quest[1].flags=QF_DONE;
		if (!quest[2].done) quest[2].done=1;
		quest[2].flags=QF_DONE;
		quest[3].flags=QF_OPEN;
		quest[4].flags=0;
	}  else if (ppd->gwendy_state>=6) {
		if (!quest[1].done) quest[1].done=1;
		quest[1].flags=QF_DONE;
		quest[2].flags=QF_OPEN;
		quest[3].flags=0;
		quest[4].flags=0;
	}  else if (ppd->gwendy_state>0) {
		quest[1].flags=QF_OPEN;
		quest[2].flags=0;
		quest[3].flags=0;
		quest[4].flags=0;
	} else {
		quest[1].flags=0;
		quest[2].flags=0;
		quest[3].flags=0;
		quest[4].flags=0;
	}

        if (ppd->yoakin_state>=5) {
		if (!quest[5].done) quest[5].done=1;
		quest[5].flags=QF_DONE;
	} else if (ppd->yoakin_state>0) {
                quest[5].flags=QF_OPEN;
	} else {
		quest[5].flags=0;
	}

	if (ppd->nook_state>=12) {
		if (!quest[6].done) quest[6].done=1;
		quest[6].flags=QF_DONE;
	} else if (ppd->nook_state>0) {
                quest[6].flags=QF_OPEN;
	} else {
		quest[6].flags=0;
	}

	if (ppd->guiwynn_state>=9) {
		if (!quest[7].done) quest[7].done=1;
		quest[7].flags=QF_DONE;
		if (!quest[8].done) quest[8].done=1;
		quest[8].flags=QF_DONE;
	} else if (ppd->guiwynn_state>=6) {
		if (!quest[7].done) quest[7].done=1;
		quest[7].flags=QF_DONE;
		quest[8].flags=QF_OPEN;
	} else if (ppd->guiwynn_state>0) {
                quest[7].flags=QF_OPEN;
		quest[8].flags=0;
	} else {
		quest[7].flags=0;
		quest[8].flags=0;
	}

	if (ppd->logain_state>=6) {
		if (!quest[9].done) quest[9].done=1;
		quest[9].flags=QF_DONE;
	} else if (ppd->logain_state>0) {
                quest[9].flags=QF_OPEN;
	} else {
		quest[9].flags=0;
	}

	if (ppd->reskin_state>=8) {
		if (!quest[17].done) quest[17].done=1;
		quest[17].flags=QF_DONE;
	} else if (ppd->reskin_state>=4) {
		quest[17].flags=QF_OPEN;
	} else {
		quest[17].flags=0;
	}
}

static void questlog_init_area3(int cn,struct quest *quest)
{
	struct area3_ppd *ppd;

	if (!(ppd=set_data(cn,DRD_AREA3_PPD,sizeof(struct area3_ppd)))) return;

	if (ppd->seymour_state>=16) {
		if (!quest[10].done) quest[10].done=1;
		quest[10].flags=QF_DONE;
		if (!quest[11].done) quest[11].done=1;
		quest[11].flags=QF_DONE;
		if (!quest[12].done) quest[12].done=1;
		quest[12].flags=QF_DONE;
	} else if (ppd->seymour_state>=12) {
		if (!quest[10].done) quest[10].done=1;
		quest[10].flags=QF_DONE;
		if (!quest[11].done) quest[11].done=1;
		quest[11].flags=QF_DONE;
		quest[12].flags=QF_OPEN;
	} else if (ppd->seymour_state>=10) {
		if (!quest[10].done) quest[10].done=1;
		quest[10].flags=QF_DONE;
		quest[11].flags=QF_OPEN;
		quest[12].flags=0;
	} else if (ppd->seymour_state>0) {
		quest[10].flags=QF_OPEN;
		quest[11].flags=0;
		quest[12].flags=0;
	} else {
		quest[10].flags=0;
		quest[11].flags=0;
		quest[12].flags=0;
	}

	if (ppd->kelly_state>=16) {
		if (!quest[13].done) quest[13].done=1;
		quest[13].flags=QF_DONE;
		if (!quest[14].done) quest[14].done=1;
		quest[14].flags=QF_DONE;
		if (!quest[15].done) quest[15].done=1;
		quest[15].flags=QF_DONE;
	} else if (ppd->kelly_state>=14) {
		if (!quest[13].done) quest[13].done=1;
		quest[13].flags=QF_DONE;
		if (!quest[14].done) quest[14].done=1;
		quest[14].flags=QF_DONE;
		quest[15].flags=QF_OPEN;
	} else if (ppd->kelly_state>=6) {
		if (!quest[13].done) quest[13].done=1;
		quest[13].flags=QF_DONE;
		quest[14].flags=QF_OPEN;
		quest[15].flags=0;
	} else if (ppd->kelly_state>=2) {
		quest[13].flags=QF_OPEN;
		quest[14].flags=0;
		quest[15].flags=0;
	} else {
		quest[13].flags=0;
		quest[14].flags=0;
		quest[15].flags=0;
	}

	if (ppd->astro2_state>=5) {
		if (!quest[16].done) quest[16].done=1;
		quest[16].flags=QF_DONE;
	} else if (ppd->astro2_state>0) {
		quest[16].flags=QF_OPEN;
	} else {
		quest[16].flags=0;
	}

	if (ppd->crypt_state>=15) {
		if (!quest[18].done) quest[18].done=1;
		quest[18].flags=QF_DONE;
		if (!quest[19].done) quest[19].done=1;
		quest[19].flags=QF_DONE;
	} else if (ppd->crypt_state>=12) {
		if (!quest[18].done) quest[18].done=1;
		quest[18].flags=QF_DONE;
		quest[19].flags=QF_OPEN;
	} else if (ppd->crypt_state>0) {
		quest[18].flags=QF_OPEN;
		quest[19].flags=0;
	} else {
		quest[18].flags=0;
		quest[19].flags=0;
	}

	if (ppd->clara_state>=15) {
		if (!quest[21].done) quest[21].done=1;
		quest[21].flags=QF_DONE;
	} else if (ppd->clara_state>=6) {
		quest[21].flags=QF_OPEN;
	} else {
		quest[21].flags=0;
	}

	if (ppd->william_state>=7) {
		if (!quest[22].done) quest[22].done=1;
		quest[22].flags=QF_DONE;
		if (!quest[23].done) quest[23].done=1;
		quest[23].flags=QF_DONE;
	} else if (ppd->william_state>=3) {
		if (!quest[22].done) quest[22].done=1;
		quest[22].flags=QF_DONE;
		quest[23].flags=QF_OPEN;
	} else if (ppd->william_state>0) {
                quest[22].flags=QF_OPEN;
		quest[23].flags=0;
	}

	if (ppd->hermit_state>=5) {
		if (!quest[24].done) quest[24].done=1;
		quest[24].flags=QF_DONE;
	} else if (ppd->hermit_state>0) {
		quest[24].flags=QF_OPEN;
	} else {
		quest[24].flags=0;
	}
}

static void questlog_init_staff(int cn,struct quest *quest)
{
	struct staffer_ppd *ppd;

	ppd=set_data(cn,DRD_STAFFER_PPD,sizeof(struct staffer_ppd));

	if (ppd->carlos_state>=6) {
		if (!quest[20].done) quest[20].done=1;
		quest[20].flags=QF_DONE;
	} else if (ppd->carlos_state>0) {
                quest[20].flags=QF_OPEN;
	} else {
		quest[20].flags=0;
	}

	if (ppd->smugglecom_state>=10) {
		if (!quest[35].done) quest[35].done=1;
		quest[35].flags=QF_DONE;
		if (!quest[36].done) quest[36].done=1;
		quest[36].flags=QF_DONE;
		if (!quest[37].done) quest[37].done=1;
		quest[37].flags=QF_DONE;
	} else if (ppd->smugglecom_state>=7) {
		if (!quest[35].done) quest[35].done=1;
		quest[35].flags=QF_DONE;
		if (!quest[36].done) quest[36].done=1;
		quest[36].flags=QF_DONE;
                quest[37].flags=QF_OPEN;
	} else if (ppd->smugglecom_state>=5) {
		if (!quest[35].done) quest[35].done=1;
		quest[35].flags=QF_DONE;
		quest[36].flags=QF_OPEN;
                quest[37].flags=0;
	} else if (ppd->smugglecom_state>0) {
		quest[35].flags=QF_OPEN;
                quest[36].flags=0;
		quest[37].flags=0;
	} else {
		quest[35].flags=0;
                quest[36].flags=0;
		quest[37].flags=0;
	}

	if (ppd->aristocrat_state>=8) {
		if (!quest[38].done) quest[38].done=1;
		quest[38].flags=QF_DONE;
	} else if (ppd->aristocrat_state>0) {
                quest[38].flags=QF_OPEN;
	} else quest[38].flags=0;

	if (ppd->yoatin_state>=9) {
		if (!quest[39].done) quest[39].done=1;
		quest[39].flags=QF_DONE;
	} else if (ppd->aristocrat_state>0) {
                quest[39].flags=QF_OPEN;
	} else quest[39].flags=0;

	if ((ppd->countbran_bits&(1|2|4))==(1|2|4)) {
		if (!quest[40].done) quest[40].done=1;
		quest[40].flags=QF_DONE;
	} else if (ppd->countbran_state>0) {
		quest[40].flags=QF_OPEN;
	} else quest[40].flags=0;

	if (ppd->brennethbran_state>=12) {
		if (!quest[41].done) quest[41].done=1;
		quest[41].flags=QF_DONE;
		if (!quest[42].done) quest[42].done=1;
		quest[42].flags=QF_DONE;
		if (!quest[43].done) quest[43].done=1;
		quest[43].flags=QF_DONE;
	} else if (ppd->brennethbran_state>=9) {
		if (!quest[41].done) quest[41].done=1;
		quest[41].flags=QF_DONE;
		if (!quest[42].done) quest[42].done=1;
		quest[42].flags=QF_DONE;
                quest[43].flags=QF_OPEN;
	} else if (ppd->brennethbran_state>=5) {
		if (!quest[41].done) quest[41].done=1;
		quest[41].flags=QF_DONE;
		quest[42].flags=QF_OPEN;
                quest[43].flags=0;
	} else if (ppd->brennethbran_state>0) {
                quest[41].flags=QF_OPEN;
		quest[42].flags=0;
                quest[43].flags=0;
	} else {
		quest[41].flags=0;
		quest[42].flags=0;
                quest[43].flags=0;
	}

	if (ppd->spiritbran_state>=5) {
		if (!quest[44].done) quest[44].done=1;
		quest[44].flags=QF_DONE;
	} else if (ppd->spiritbran_state>0) {
                quest[44].flags=QF_OPEN;
	} else quest[44].flags=0;

	if (ppd->broklin_state>=11) {
		if (!quest[45].done) quest[45].done=1;
		quest[45].flags=QF_DONE;
		if (!quest[46].done) quest[46].done=1;
		quest[46].flags=QF_DONE;
	} else if (ppd->broklin_state>=5) {
		if (!quest[45].done) quest[45].done=1;
		quest[45].flags=QF_DONE;
		quest[46].flags=QF_OPEN;
	} else if (ppd->broklin_state>0) {
                quest[45].flags=QF_OPEN;
		quest[46].flags=0;
	} else {
		quest[45].flags=0;
		quest[46].flags=0;
	}

	if (ppd->dwarfchief_state>=14) {
		if (!quest[47].done) quest[47].done=1;
		quest[47].flags=QF_DONE;
		if (!quest[48].done) quest[48].done=1;
		quest[48].flags=QF_DONE;
		if (!quest[49].done) quest[49].done=1;
		quest[49].flags=QF_DONE;
		if (!quest[50].done) quest[50].done=1;
		quest[50].flags=QF_DONE;
	} else if (ppd->dwarfchief_state>=11) {
		if (!quest[47].done) quest[47].done=1;
		quest[47].flags=QF_DONE;
		if (!quest[48].done) quest[48].done=1;
		quest[48].flags=QF_DONE;
		if (!quest[49].done) quest[49].done=1;
		quest[49].flags=QF_DONE;
                quest[50].flags=QF_OPEN;
	} else if (ppd->dwarfchief_state>=8) {
		if (!quest[47].done) quest[47].done=1;
		quest[47].flags=QF_DONE;
		if (!quest[48].done) quest[48].done=1;
		quest[48].flags=QF_DONE;
                quest[49].flags=QF_OPEN;
                quest[50].flags=0;
	} else if (ppd->dwarfchief_state>=5) {
		if (!quest[47].done) quest[47].done=1;
		quest[47].flags=QF_DONE;
		quest[48].flags=QF_OPEN;
                quest[49].flags=0;
                quest[50].flags=0;
	} else if (ppd->dwarfchief_state>0) {
		quest[47].flags=QF_OPEN;
		quest[48].flags=0;
                quest[49].flags=0;
                quest[50].flags=0;
	} else {
		quest[47].flags=0;
		quest[48].flags=0;
                quest[49].flags=0;
                quest[50].flags=0;
	}

	if (ppd->dwarfshaman_state>=9) {
		if (!quest[51].done) quest[51].done=1;
		quest[51].flags=QF_DONE;
		if (!quest[52].done) quest[52].done=1;
		quest[52].flags=QF_DONE;
		if (!quest[53].done) quest[53].done=1;
		quest[53].flags=QF_DONE;
	} else if (ppd->dwarfshaman_state>=6) {
		if (!quest[51].done) quest[51].done=1;
		quest[51].flags=QF_DONE;
		if (!quest[52].done) quest[52].done=1;
		quest[52].flags=QF_DONE;
                quest[53].flags=QF_OPEN;
	} else if (ppd->dwarfshaman_state>=3) {
		if (!quest[51].done) quest[51].done=1;
		quest[51].flags=QF_DONE;
		quest[52].flags=QF_OPEN;
                quest[53].flags=0;
	} else if (ppd->dwarfshaman_state>0) {
                quest[51].flags=QF_OPEN;
		quest[52].flags=0;
                quest[53].flags=0;
	} else {
		quest[51].flags=0;
		quest[52].flags=0;
                quest[53].flags=0;
	}
}

static void questlog_init_twocity(int cn,struct quest *quest)
{
	struct twocity_ppd *ppd;

	ppd=set_data(cn,DRD_TWOCITY_PPD,sizeof(struct twocity_ppd));

	if (ppd->thief_state>=20) {
		if (!quest[25].done) quest[25].done=1;
		quest[25].flags=QF_DONE;
		if (!quest[26].done) quest[26].done=1;
		quest[26].flags=QF_DONE;
		if (!quest[27].done) quest[27].done=1;
		quest[27].flags=QF_DONE;
		if (!quest[28].done) quest[28].done=1;
		quest[28].flags=QF_DONE;
	} else if (ppd->thief_state>=18) {
		if (!quest[25].done) quest[25].done=1;
		quest[25].flags=QF_DONE;
		if (!quest[26].done) quest[26].done=1;
		quest[26].flags=QF_DONE;
		if (!quest[27].done) quest[27].done=1;
		quest[27].flags=QF_DONE;
		quest[28].flags=QF_OPEN;
	} else if (ppd->thief_state>=14) {
		if (!quest[25].done) quest[25].done=1;
		quest[25].flags=QF_DONE;
		if (!quest[26].done) quest[26].done=1;
		quest[26].flags=QF_DONE;
                quest[27].flags=QF_OPEN;
		quest[28].flags=0;
	} else if (ppd->thief_state>=10) {
		if (!quest[25].done) quest[25].done=1;
		quest[25].flags=QF_DONE;
                quest[26].flags=QF_OPEN;
                quest[27].flags=0;
		quest[28].flags=0;
	} else if (ppd->thief_state>=5) {
                quest[25].flags=QF_OPEN;
                quest[26].flags=0;
                quest[27].flags=0;
		quest[28].flags=0;
	} else {
                quest[25].flags=0;
                quest[26].flags=0;
                quest[27].flags=0;
		quest[28].flags=0;
	}

	if (ppd->sanwyn_state>=8) {
		if (!quest[29].done) quest[29].done=1;
		quest[29].flags=QF_DONE;
	} else if (ppd->sanwyn_state>0) {
                quest[29].flags=QF_OPEN;
	} else quest[29].flags=0;

	if (ppd->skelly_state>=3) {
		if (!quest[30].done) quest[30].done=1;
		quest[30].flags=QF_DONE;
	} else if (ppd->skelly_state>0) {
                quest[30].flags=QF_OPEN;
	} else quest[30].flags=0;

	if (ppd->alchemist_state>=5) {
		if (!quest[31].done) quest[31].done=1;
		quest[31].flags=QF_DONE;
	} else if (ppd->alchemist_state>0) {
                quest[31].flags=QF_OPEN;
	} else quest[31].flags=0;
}

static void questlog_init_nomad(int cn,struct quest *quest)
{
	struct nomad_ppd *ppd;

	ppd=set_data(cn,DRD_NOMAD_PPD,sizeof(struct nomad_ppd));

	if (ppd->nomad_state[1]>=9) {
		if (!quest[32].done) quest[32].done=1;
		quest[32].flags=QF_DONE;
	} else if (ppd->nomad_state[1]>0) {
                quest[32].flags=QF_OPEN;
	} else {
		quest[32].flags=0;
	}

	if (ppd->nomad_state[4]>=4) {
		if (!quest[33].done) quest[33].done=1;
		quest[33].flags=QF_DONE;
	} else if (ppd->nomad_state[4]>0) {
                quest[33].flags=QF_OPEN;
	} else {
		quest[33].flags=0;
	}

	if (ppd->nomad_state[5]>=4) {
		if (!quest[34].done) quest[34].done=1;
		quest[34].flags=QF_DONE;
	} else if (ppd->nomad_state[5]>0) {
                quest[34].flags=QF_OPEN;
	} else {
		quest[34].flags=0;
	}
}


void questlog_init(int cn)
{
	struct quest *quest;

	if (!(quest=set_data(cn,DRD_QUESTLOG_PPD,sizeof(struct quest)*MAXQUEST))) return;
	
	if (quest[MAXQUEST-1].done==55) return;

	questlog_init_area1(cn,quest);
	questlog_init_area3(cn,quest);
	questlog_init_staff(cn,quest);
	questlog_init_twocity(cn,quest);
	questlog_init_nomad(cn,quest);

	quest[MAXQUEST-1].done=55;
}

int destroy_item_from_body(int cn,int IID)
{
        char buf[80];

        sprintf(buf,"%10dX%10d",ch[cn].ID,IID);
	server_chat(1036,buf);

        return 1;
}

int remove_item_from_body_bg(int cnID,int IID)
{
	int n,cnt=0,m,in;

	for (n=1; n<MAXCONTAINER; n++) {
		if (!con[n].in) continue;			// empty or not item container (grave)
		if (con[n].owner!=charID_ID(cnID)) continue;	// not owned by cn

		for (m=0; m<CONTAINERSIZE; m++) {
			if ((in=con[n].item[m]) && it[in].ID==IID) {
				dlog(0,in,"destroying quest item in grave (questlog, ID=%d, IID=%X)",cnID,IID);
				destroy_item(in);
				con[n].item[m]=0;
				cnt++;
			}
		}
		

	}
	//if (cnt) tell_chat(0,cnID,1,"Area %d: Removed %d items from corpses you cheat.",areaID,cnt);

	return cnt;
}

void destroy_item_byID(int cn,int ID)
{
	struct depot_ppd *ppd;
	int n,in;

	ppd=set_data(cn,DRD_DEPOT_PPD,sizeof(struct depot_ppd));

	for (n=0; n<INVENTORYSIZE; n++) {
		if (n>=12 && n<30) continue;
		
		if ((in=ch[cn].item[n])) {
			if (it[in].ID==ID) {
				destroy_item(ch[cn].item[n]);
				ch[cn].item[n]=0;
				ch[cn].flags|=CF_ITEMS;
			}
		}
	}

	if ((in=ch[cn].citem)) {
		if (it[in].ID==ID) {
			destroy_item(ch[cn].citem);
			ch[cn].citem=0;
			ch[cn].flags|=CF_ITEMS;
		}
	}

	if (ppd) {
		for (n=0; n<MAXDEPOT; n++) {
			if (ppd->itm[n].flags) {
                                if (ppd->itm[n].ID==ID) {
					ppd->itm[n].flags=0;
				}
			}
		}
	}

	destroy_item_from_body(cn,ID);
}











