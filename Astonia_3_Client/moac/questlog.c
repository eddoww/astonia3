#include <windows.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define ISCLIENT
#include "main.h"
#include "dd.h"
#include "client.h"
#include "sprite.h"
#include "gui.h"
#include "spell.h"
#include "edit.h"
#include "sound.h"

#define QLF_REPEATABLE	(1u<<0)
#define QLF_XREPEAT	(1u<<1)

static int havequest=0;

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
	{"The Emperor's Plaque",55,65,"Kelly","Aston",240000,0},			//60,

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
static int questonscreen[10];

int questproz(int cnt){
	int n;
	float val=100.0;

	for (n=0; n<cnt; n++) {
		val*=0.825;
	}
	return (int)val;
}

int questlist[MAXQUEST],questinit=0;
int questcmp(const void *a,const void *b)
{
	int *va=(int*)a,*vb=(int*)b;

	if (questlog[*va].minlevel!=questlog[*vb].minlevel) return questlog[*vb].minlevel-questlog[*va].minlevel;
	if (questlog[*va].maxlevel!=questlog[*vb].maxlevel) return questlog[*vb].maxlevel-questlog[*va].maxlevel;
	return *va-*vb;
}

int do_display_random(void)
{
	int y=55,x,n,idx,bit,m;
	static short indec[10]={0,11,24,38,43,57,64,76,83,96};
	static short bribes[10]={0,15,22,34,48,54,67,78,86,93};
	static short welding[10]={0,18,27,32,46,52,62,72,81,98};
	static short edge[10]={0,13,26,36,42,59,66,74,88,91};
	static short kindness[10]={0,21,55};
	static short jobless[10]={0,20,45,61,82,97};
	static short security[10]={0,10,29,41,58,69,75,85,94};
	
	dd_drawtext((10+204)/2,y,whitecolor,DD_CENTER,"Random Dungeon"); y+=24;

	dd_drawtext_fmt(10,y,graycolor,0,"Continuity: %d",shrine.continuity); y+=12;
	
	x=dd_drawtext(10,y,graycolor,0,"Indecisiveness: ");
	for (n=1; n<10; n++) {
		idx=n/32;
		bit=1<<(n&31);
		if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (indec[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",indec[n]);
		}
	}
	y+=12;
	
	x=dd_drawtext(10,y,graycolor,0,"Bribes: ");
	for (n=1; n<10; n++) {
		m=n+10;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (bribes[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",bribes[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Welding: ");
	for (n=1; n<10; n++) {
		m=n+20;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (welding[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",welding[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"LOE: ");
	for (n=1; n<10; n++) {
		m=n+30;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (edge[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",edge[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Kindness: ");
	for (n=1; n<3; n++) {
		m=n+40;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (kindness[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",kindness[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Security: ");
	for (n=1; n<9; n++) {
		m=n+53;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (security[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",security[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Jobless: ");
	for (n=1; n<6; n++) {
		m=n+63;
		idx=m/32;
		bit=1<<(m&31);
                if (shrine.used[idx]&bit) {
			x=dd_drawtext(x,y,graycolor,0,"- ");
		} else {
			if (jobless[n]<shrine.continuity) x=dd_drawtext_fmt(x,y,graycolor,0,"%d ",jobless[n]);
		}
	}
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Vitality: ");
	if (shrine.used[50/32]&(1<<(50&31))) dd_drawtext(x,y,graycolor,0,"- "); else if (30<shrine.continuity) dd_drawtext(x,y,graycolor,0,"30");
	y+=12;
	
	x=dd_drawtext(10,y,graycolor,0,"Death: ");
	if (shrine.used[51/32]&(1<<(51&31))) dd_drawtext(x,y,graycolor,0,"- "); else if (37<shrine.continuity) dd_drawtext(x,y,graycolor,0,"37");
	y+=12;

	x=dd_drawtext(10,y,graycolor,0,"Braveness: ");
	if (shrine.used[52/32]&(1<<(52&31))) dd_drawtext(x,y,graycolor,0,"- "); else if (51<shrine.continuity) dd_drawtext(x,y,graycolor,0,"51");
	y+=12;

	y+=12;
	y=dd_drawtext_break(10,y,204,graycolor,0,"Only shrines in dungeons you have already solved (used the continuity shrine), but not yet used, are shown. The continuity shrine shown is the first one you haven't used yet.");

	return y;
}

int do_display_questlog(int nr)
{				
	int y=55,off,n,cnt,pass,m;
	char buf[256];

	if (!questinit) {
		for (n=0; n<sizeof(questlog)/sizeof(questlog[0]); n++) questlist[n]=n;
		qsort(questlist,sizeof(questlog)/sizeof(questlog[0]),sizeof(int),questcmp);
	}
	

	if (!havequest) {
		cmd_getquestlog();
		havequest=1;
	}

	if (nr==10) return do_display_random();
	
	off=(nr-1)*9;

	for (n=0; n<10; n++) questonscreen[n]=-1;

	for (pass=cnt=0; pass<2; pass++) {
		for (m=0; m<sizeof(questlog)/sizeof(questlog[0]); m++) {
			n=questlist[m];
	
			if ((pass==0 && (quest[n].flags)==QF_OPEN && quest[n].done<10) || (pass==1 && (quest[n].flags)==QF_DONE)) {
				if (cnt>=off) {
					if ((questlog[n].flags&QLF_REPEATABLE) && (quest[n].flags&QF_DONE) && quest[n].done<10) dd_drawtext(200,y,lightbluecolor,DD_RIGHT,"Re-Open");
					if ((questlog[n].flags&QLF_XREPEAT) && (quest[n].flags&QF_DONE)) dd_drawtext(200,y,graycolor,DD_RIGHT,"(Junk Item)");
					sprintf(buf,"Quest: %s",questlog[n].name);
					dd_drawtext(10,y,whitecolor,0,buf); y+=10;
					
					sprintf(buf,"From: %s in %s.",questlog[n].giver,questlog[n].area);
					dd_drawtext(10,y,graycolor,0,buf); y+=10;
			
					if (questlog[n].flags&(QLF_REPEATABLE|QLF_XREPEAT)) {
						sprintf(buf,"Done: %d time%s (%d%% exp). %s.",quest[n].done,quest[n].done!=1?"s":"",questproz(quest[n].done),(quest[n].flags&QF_DONE)?"Done":"Open");
						dd_drawtext(10,y,graycolor,0,buf); y+=10;
					} else {
						sprintf(buf,"Done: %d time. (not repeatable). %s.",quest[n].done,(quest[n].flags&QF_DONE)?"Done":"Open");
						dd_drawtext(10,y,graycolor,0,buf); y+=10;
					}
					y+=10;
				}
				if (cnt-off>=0 && cnt-off<10) questonscreen[cnt-off]=n;
				cnt++;
				if (cnt>=off+9) break;
			}		
		}
		if (cnt>=off+9) break;
	}
	if (cnt==0) {
		y+=50;
		y=dd_drawtext((10+202)/2,y,whitecolor,DD_CENTER,"Your questlog is empty.");
	}

        return y;
}


void quest_select(int nr)
{
	if (nr<0 || nr>9) return;
	
	if (questonscreen[nr]!=-1) {
		cmd_reopen_quest(questonscreen[nr]);
	}
}
