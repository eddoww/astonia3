/*
 * $Id: clan.h,v 1.6 2007/11/04 12:59:43 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: clan.h,v $
 * Revision 1.6  2007/11/04 12:59:43  devel
 * new clan bonuses
 *
 * Revision 1.5  2007/07/27 06:00:10  devel
 * spawn frequency to 3 1/2 hours
 *
 * Revision 1.4  2007/03/03 12:15:03  devel
 * reduced spawn freq. to 11 hours
 *
 * Revision 1.3  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.2  2005/12/01 16:31:09  ssim
 * added clan message
 */

#define MAXCLAN			61
#define MAXBONUS		10
#define CLAN_CANNON_STRENGTH	5
#define BIGSPAWNFREQ 		(3*60*60+30*60)

struct clan_bonus
{
	int level;
};

struct clan_depot
{
	int money;
};
		
struct clan_treasure
{
	int jewels;
	int cost_per_week;	// in 1/1000
	int debt;		// in 1/1000
	int payed_till;		// realtime
};

struct clan_hall
{
	int tricky;		// we need to change at least one value on every save to avoid error messages
	int dummy[127];
};

#define CS_ALLIANCE	1	// needs 24h for one-sided change to treaty
				// members of clans with an alliance cannot attack each other
#define CS_PEACETREATY	2	// needs 24h for one-sided change to neutral
				// members of clans with a peace-treaty cannot attack each other
#define CS_NEUTRAL	3	// members of neutral clans cannot attack each other. needs 1h for one-sided change to war

#define CS_WAR		4       // members of clans at war can attack each other in clan areas.
				// killed enemies will not lose EXP, but they will lose their items

#define CS_FEUD		5	// members of clans with a feud can attack each other everywhere.
				// 24 hours for one-sided change to war
				// killed enemies will not lose EXP and items.

struct clan_status
{
	int members;					// number of members	(aprox)
	int serial;  		 			
	unsigned char current_relation[MAXCLAN];	// relation to clan X
	unsigned char want_relation[MAXCLAN];
	int want_date[MAXCLAN];
};

struct clan
{
	char name[80];

	char rankname[5][40];

	struct clan_bonus bonus[MAXBONUS];
	struct clan_depot depot;
	struct clan_treasure treasure;
	struct clan_status status;
	struct clan_hall hall;
        char website[80];
	char message[80];
};

void showclan(int cn);
int show_clan_info(int cn,int co,char *buf);
void show_clan_relation(int cn,int cnr);
void tick_clan(void);
int found_clan(char *name,int cn,int cnr);
int add_jewel(int nr,int cn,int cnt);
int cnt_jewels(int nr);
int set_clan_bonus(int cnr,int gnr,int glevel,int cn);
int set_clan_relation(int cnr,int onr,int rel,int cn);
int set_clan_rankname(int cnr,int rank,char *name,int cn);
int may_enter_clan(int cn,int nr);
int clan_can_attack_outside(int c1,int c2);
int clan_can_attack_inside(int c1,int c2);
int clan_alliance(int c1,int c2);
char *get_char_clan_name(int cn);
int get_char_clan(int cn);
void add_member(int cn,int cnr,char *master);
void remove_member(int cn,int cn_master);
char *get_clan_name(int cnr);
int get_clan_money(int cnr);
void clan_money_change(int cnr,int diff,int cn);
int set_clan_website(int cnr,char *site,int cn);
int set_clan_message(int cnr,char *site,int cn);
int get_clan_dungeon(int cnr,int type);
int get_clan_bonus(int cnr,int nr);
int clan_serial(int cnr);
void kill_clan(int cnr);
void show_clan_pots(int cn);
int clan_trade_bonus(int cn);
void clan_setname(int cnr,char *name);
void show_clan_message(int cn);
int get_clanalive(int cnr);
int clan_alliance_self(int c1,int c2);
int del_jewel(int nr,int cn,int cnt);
int may_join_clan(int cn,char *errbuff);
int clanslot_free(int cnr);
int clan_enhance_bonus(int cn);
int clan_smith_bonus(int cn);
int clan_preservation_bonus(int cn);
