/*
 *
 * $Id: clan.h,v 1.3 2007/12/10 10:12:53 devel Exp $ (c) 2005 D.Brockhaus
 *
 * $Log: clan.h,v $
 * Revision 1.3  2007/12/10 10:12:53  devel
 * clan changes
 *
 * Revision 1.2  2005/12/01 16:31:09  ssim
 * added clan message
 *
 */

#define MAXCLAN		32
#define MAXBONUS	14

struct clan_bonus
{
	int level;
};

struct clan_store
{
	int used;
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

struct clan_dungeon
{
	int warrior[2][6];	// [0][x] total, [1][x] use per dungeon
	int mage[2][6];         // [0][x] total, [1][x] use per dungeon
	int seyan[2][6];	// [0][x] total, [1][x] use per dungeon
	int teleport[2];	// [0] total, [1] use per dungeon
	int fake[2];		// [0] total, [1] use per dungeon
	int key[2];		// [0] total, [1] use per dungeon

	unsigned short int alc_pot[2][6];	// 2*6*2=24 bytes
	unsigned short int simple_pot[3][3];	// 3*3*2=18 bytes

	unsigned short reserved1;

	unsigned int doraid;			// 4 bytes
	unsigned int raidonstart;		// +4 bytes = 8 bytes

	int training_score;			// 4 bytes
	int last_training_update;		// +4 bytes = 8 bytes

	char dummy[160-24-18-10-8];
};

struct clan
{
	char name[80];

	char rankname[5][40];

	struct clan_bonus bonus[MAXBONUS];
	struct clan_store store[2];
	struct clan_depot depot;
	struct clan_treasure treasure;
	struct clan_status status;
        char website[80];
	char message[80];
        struct clan_dungeon dungeon;
};

void showclan(int cn);
int show_clan_info(int cn,int co,char *buf);
void show_clan_relation(int cn,int cnr);
void tick_clan(void);
int found_clan(char *name,int cn,int *pclan);
int add_jewel(int nr,int cn);
int cnt_jewels(int nr);
int set_clan_bonus(int cnr,int gnr,int glevel,int cn);
int set_clan_relation(int cnr,int onr,int rel,int cn);
int set_clan_rankname(int cnr,int rank,char *name,int cn);
int may_enter_clan(int cn,int nr);
int clan_can_attack_outside(int c1,int c2);
int clan_can_attack_inside(int c1,int c2);
int clan_alliance(int c1,int c2);
void steal_jewel(int nr);
char *get_char_clan_name(int cn);
int get_char_clan(int cn);
void add_member(int cn,int cnr,char *master);
void remove_member(int cn,int cn_master);
char *get_clan_name(int cnr);
int get_clan_money(int cnr);
void clan_money_change(int cnr,int diff,int cn);
void clan_dungeon_chat(char *ptr);
int get_clan_dungeon_cost(int type,int number);
int set_clan_dungeon_use(int cnr,int type,int number,int cn);
int set_clan_website(int cnr,char *site,int cn);
int set_clan_message(int cnr,char *site,int cn);
int get_clan_dungeon(int cnr,int type);
int get_clan_bonus(int cnr,int nr);
int clan_serial(int cnr);
void kill_clan(int cnr);
void show_clan_pots(int cn);
int clan_trade_bonus(int cn);
int add_simple_potion(int nr,int co);
int get_clan_raid(int cnr);
int set_clan_raid(int cnr,int cn,int onoff);
int set_clan_raid_god(int cnr,int cn,int onoff);
int add_alc_potion(int nr,int in);
void clan_setname(int cnr,char *name);
int score_to_level(int score);
int clan_get_training_score(int cnr);
void show_clan_message(int cn);

