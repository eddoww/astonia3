/*
 *
 * $Id: server.h,v 1.4 2006/12/14 13:58:05 devel Exp $
 *
 * $Log: server.h,v $
 * Revision 1.4  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.3  2006/08/19 17:27:52  devel
 * increased version to 3.01.00
 *
 * Revision 1.2  2005/11/27 20:03:55  ssim
 * moved CF_WON to a new bit to force all players to do it again
 *
 * Revision 1.1  2005/09/24 09:55:48  ssim
 * Initial revision
 */

#define VERSION	 0x030100

#define TICKS		24
#define TICK		(1000000ull/TICKS)

#define ITEM_DECAY_TIME			(5*60*TICKS)	// time an item which was placed on the ground takes before it gets destroyed
#define	PLAYER_BODY_DECAY_TIME		(30*60*TICKS)	// time a player body takes before getting destroyed
#define NPC_BODY_DECAY_TIME		(2*60*TICKS)	// time a NPC body takes before getting destroyed
#define NPC_BODY_DECAY_TIME_AREA32	(15*60*TICKS)	// time a NPC body takes before getting destroyed (area 32)
#define RESPAWN_TIME			(1*60*TICKS)	// respawn time of NPCs - can be overriden by giving a respawn=xxx statement in the character definition
#define LAGOUT_TIME			(5*60*TICKS)	// time it takes to lag out (time difference between loss of connection and removal of character from server)
#define REGEN_TIME			(4*TICKS)	// time character has to be idle to start regenerating

#define max(a,b)	((a) > (b) ? (a) : (b))
#define min(a,b)	((a) < (b) ? (a) : (b))

#define RANDOM(a)	(rand()%(a))

#define MAXMAP		256
#define TOTAL_MAXCHARS	2048
#define MAXITEM		(maxitem)	//6144	//16384
#define MAXCHARS	(maxchars)	//768	//2048		// MUST be a multiple of 8
#define MAXEFFECT	(maxeffect)	//512	//1024

#define MAXAREANAME	80
#define MAXPASSWORD	16
#define MAXEMAIL	80

#define POWERSCALE	1000		// ch.hp = ch.value[0][V_HP] * POWERSCALE, same for endurance and mana

// define DIST if client.h wasn't loaded already
#ifndef DIST
#define DIST		25
#endif

// *********** Server Status Variables *******

extern int maxchars,maxitem,maxeffect;
extern int quit;	// server wants to quit, stop all background tasks
extern int demon;	// server demonized?
extern int mem_usage;	// total memory usage
extern int ticker;	// tick counter
extern int sercn;	// unique (per server run) ID for characters
extern int serin;	// unique (per server run) ID for items
extern int online;	// number of players online
extern int areaID;	// ID of the area this server is responsible for
extern int areaM;	// mirror number of this server
extern int multi;	// started extra thread for database accesses
extern int server_addr;	// ip address of this server
extern int server_port;	// tcp port of this server
//extern int server_net;	// server is supposed to use this network (if multiply interfaces are present)
extern int time_now;	// current (as of this tick) time()
extern int server_idle;	// idle percentage of this server
extern int shutdown_at;
extern int shutdown_down;
extern int nologin;
extern int serverID;	// ID for client side IP translation
extern int isxmas;

extern volatile long long sent_bytes_raw;	// bytes sent including approximation of tcp/ip overhead
extern volatile long long sent_bytes;		// bytes sent
extern volatile long long rec_bytes_raw;	// bytes received including approximation of tcp/ip overhead
extern volatile long long rec_bytes;		// bytes received


// *********** MAP DEFINITION ****************

// 64 bits for flag (static)
#define MF_FLAGCAST(a)	((unsigned int)((a)))
#define MF_MOVEBLOCK    MF_FLAGCAST((1ULL<<0))		
#define MF_SIGHTBLOCK   MF_FLAGCAST((1ULL<<1))
#define MF_TMOVEBLOCK   MF_FLAGCAST((1ULL<<2))		
#define MF_TSIGHTBLOCK  MF_FLAGCAST((1ULL<<3))
#define MF_INDOORS	MF_FLAGCAST((1ull<<4))
#define MF_RESTAREA	MF_FLAGCAST((1ull<<5))
#define MF_DOOR		MF_FLAGCAST((1ull<<6))		// set if door-like, i.e. might open when used
#define MF_SOUNDBLOCK	MF_FLAGCAST((1ull<<7))
#define MF_TSOUNDBLOCK	MF_FLAGCAST((1ull<<8))
#define MF_SHOUTBLOCK	MF_FLAGCAST((1ull<<9))		// tile blocks ALL sounds
#define MF_CLAN		MF_FLAGCAST((1ull<<10))		// clan area, allows kills at war
#define MF_ARENA	MF_FLAGCAST((1ull<<11))		// arena, anyone may kill anyone
#define MF_PEACE	MF_FLAGCAST((1ull<<12))		// no fighting at all
#define MF_NEUTRAL	MF_FLAGCAST((1ull<<13))		// NPCs will not attack peaceful characters
#define MF_FIRETHRU	MF_FLAGCAST((1ull<<14))		// fireball fly through this MF_MOVEBLOCK
#define MF_SLOWDEATH	MF_FLAGCAST((1ull<<15))		// player characters on this field take damage
#define MF_NOLIGHT	MF_FLAGCAST((1ull<<16))		// don't allow light from any character or take-able item
#define MF_NOMAGIC	MF_FLAGCAST((1ull<<17))		// don't allow any magic or stat bonuses
#define MF_UNDERWATER	MF_FLAGCAST((1ull<<18))		// map tile is under water
#define MF_NOREGEN	MF_FLAGCAST((1ull<<19))		// no regeneration on this map tile

struct map
{
        // display
        unsigned int gsprite;          	// background image
        unsigned int fsprite;         	// foreground sprite
	
	// light
        unsigned short dlight;          // percentage of dlight
        short light;           		// strength of light (objects only, daylight is computed independendly)

        // for fast access to objects & characters
        unsigned short ch;
        unsigned short it;
	
	// effects
	unsigned short ef[4];	

	unsigned int flags;       // s.a.
};

extern struct map *map;


// *********** ITEM DEFINITION ****************

#define IF_USED		(1ull<<0)
#define IF_MOVEBLOCK    (1ull<<1)
#define IF_SIGHTBLOCK   (1ull<<2)
#define IF_TAKE		(1ull<<3)
#define IF_USE		(1ull<<4)
#define IF_WNHEAD       (1ull<<5)	// can be worn on head
#define IF_WNNECK       (1ull<<6)	// etc...
#define IF_WNBODY       (1ull<<7)
#define IF_WNARMS       (1ull<<8)
#define IF_WNBELT       (1ull<<9)
#define IF_WNLEGS       (1ull<<10)
#define IF_WNFEET       (1ull<<11)
#define IF_WNLHAND      (1ull<<12)
#define IF_WNRHAND      (1ull<<13)
#define IF_WNCLOAK      (1ull<<14)
#define IF_WNLRING      (1ull<<15)
#define IF_WNRRING      (1ull<<16)
#define IF_WNTWOHANDED	(1ull<<17)	// two-handed weapon, fills both WNLHAND & WNRHAND
#define IF_AXE		(1ull<<18)
#define IF_DAGGER	(1ull<<19)
#define IF_HAND		(1ull<<20)
#define IF_SHIELD	(1ull<<21)
#define IF_STAFF	(1ull<<22)
#define IF_SWORD	(1ull<<23)
#define IF_TWOHAND	(1ull<<24)
#define IF_DOOR		(1ull<<25)	// door-like, might open when used
#define IF_QUEST	(1ull<<26)	// quest item. may not be dropped, given etc.
#define IF_SOUNDBLOCK	(1ull<<27)
#define IF_STEPACTION	(1ull<<28)	// call driver if character steps on this item
#define IF_MONEY	(1ull<<29)	// item is money (ie will get transfered on take)
#define IF_NODECAY	(1ull<<30)	// item has its own decay-logic
#define IF_FRONTWALL	(1ull<<31)	// item is part of wall but only visible if seen from in front
#define IF_DEPOT	(1ull<<32)	// player depot access item
#define IF_NODEPOT	(1ull<<33)	// may not be put in depot/sold
#define IF_NODROP	(1ull<<34)	// may not be dropped
#define IF_NOJUNK	(1ull<<35)	// may not be junked
#define IF_PLAYERBODY	(1ull<<36)	// item is a player's dead body
#define IF_BONDTAKE	(1ull<<37)	// bonded item, can only be taken by owner
#define IF_BONDWEAR	(1ull<<38)	// bonded item, can only be worn by owner
#define IF_LABITEM	(1ull<<39)	// labyrinth item - not saved during area changes
#define IF_VOID		(1ull<<40)	// item is in void, .x, .y, .carried, .contained reserved and possibly used by item driver
#define IF_NOENHANCE	(1ull<<41)	// item may not be enhanced through orbs/silver/gold/shrines etc.
#define IF_BEYONDBOUNDS	(1ull<<42)	// item may have more than 3 +20 mods
#define IF_BEYONDMAXMOD	(1ull<<43)	// item modifiers are added to character after max mod check

#define IF_WEAPON	(IF_AXE|IF_DAGGER|IF_HAND|IF_STAFF|IF_SWORD|IF_TWOHAND)
#define IF_WEAR		(IF_WNHEAD|IF_WNNECK|IF_WNBODY|IF_WNARMS|IF_WNBELT|IF_WNLEGS|IF_WNFEET|IF_WNLHAND|IF_WNRHAND|IF_WNCLOAK|IF_WNLRING|IF_WNRRING)

#define MAXMOD		5

#define IT_DR_SIZE	40

struct item
{
        unsigned long long flags;

        char name[40];
        char description[80];

        // game data
        unsigned int value;			// value in money units
	unsigned char min_level;
	unsigned char max_level;
	unsigned char needs_class;		// bitmapped: 1=must be warrior, 2=must be mage, 4=must be seyan, 8=must be arch
        unsigned char expire_sn;		// expire mini serial
	int ownerID;				// owner (for bonded items)

	// the item's values (armor, weapon value, attribute and skill modifiers etc.)
	signed short mod_index[MAXMOD];		// index into v_???, negative values are requirements, positive ones modifiers
	signed short mod_value[MAXMOD];		// the actual modifier/requirement

        // map stuff
        unsigned short x,y;			// position on map (only valid when != 0)
        unsigned short carried;			// carried by character (only valid when != 0)
	unsigned short contained;		// in container (only valid when != 0)

	unsigned short content;			// ID of container which holds the contents of this item

	// object driver
	unsigned short driver;

	// driver data
	unsigned char drdata[IT_DR_SIZE];
	unsigned int ID;
	unsigned int serial;	// per-run, per-server unique ID

	// client display
        int sprite;

	// server management of free list
	struct item *next;
};

extern struct item *it;

// *********** CHARACTER DEFINITION ****************

// FLAGS
#define CF_USED		(1ull<<0)
#define CF_IMMORTAL     (1ull<<1)       // will not suffer any damage
#define CF_GOD          (1ull<<2)       // may issue #god commands
#define CF_PLAYER       (1ull<<3)       // is a player
#define CF_STAFF        (1ull<<4)      	// member of the staff
#define CF_INVISIBLE    (1ull<<5)      	// character is completely invisible
#define CF_SHUTUP       (1ull<<6)      	// player is unable to talk till next day
#define CF_KICKED       (1ull<<7)      	// player got kicked, may not login again for a certain time
#define CF_UPDATE	(1ull<<8)	// client side update needed (values)
#define CF_RESERVED0	(1ull<<9)	// legacy flag - contents undefined
#define CF_RESERVED1	(1ull<<10)	// legacy flag - contents undefined
#define CF_DEAD		(1ull<<11)	// character is dying (death animation is running, doesnt take long)
#define CF_ITEMS	(1ull<<12)	// client side update needed (items)
#define CF_RESPAWN	(1ull<<13)	// NPC-flag: respawn?
#define CF_MALE	 	(1ull<<14)	// is male
#define CF_FEMALE	(1ull<<15)	// is female (if neither is set, its neuter)
#define CF_WARRIOR	(1ull<<16)	// is warrior - or seyan, if warrior and mage are set
#define CF_MAGE		(1ull<<17)	// is mage - or seyan, if warrior and mage are set
#define CF_ARCH		(1ull<<18)	// is arch-XXX
#define CF_RESERVED2	(1ull<<19)	// legacy flag - contents undefined
#define CF_NOATTACK	(1ull<<20)	// players cannot select him as attack target
#define CF_HASNAME	(1ull<<21)	// NPC has a proper name (ie Shiva), not a group name (Orc)
#define CF_QUESTITEM	(1ull<<22)	// this character can receive quest items through give
#define CF_INFRARED	(1ull<<23)	// this character has infrared vision (NPC version)
#define CF_PK		(1ull<<24)	// player killer (players only)
#define CF_ITEMDEATH	(1ull<<25)	// character turns into an item on death
#define CF_NODEATH	(1ull<<26)	// character does not actually die (death is handle by driver)
#define CF_NOBODY	(1ull<<27)	// character does not leave a body behind
#define CF_EDEMON	(1ull<<28)	// earth-demon capabilities
#define CF_FDEMON	(1ull<<29)	// fire-demon capabilities
#define CF_IDEMON	(1ull<<30)	// ice-demon capabilities
#define CF_NOGIVE	(1ull<<31)	// does not accept items through give
#define CF_PLAYERLIKE   (1ull<<32)      // treat like a player when it comes to can_attack
#define CF_RESERVED3   	(1ull<<33)      // legacy flag - contents undefined
#define CF_PAID   	(1ull<<34)      // player has paid
#define CF_PROF   	(1ull<<35)      // update professions
#define CF_ALIVE	(1ull<<36)	// living being (infravision etc)
#define CF_DEMON	(1ull<<37)	// demonic being (for special weapons)
#define CF_UNDEAD	(1ull<<38)	// undead being (for special weapons)
#define CF_HARDKILL	(1ull<<39)	// 'hard to kill' - needs special weapon to get killed
#define CF_NOBLESS	(1ull<<40)	// players may not bless this character
#define CF_AREACHANGE	(1ull<<41)	// in-game area change in progress
#define CF_LAG		(1ull<<42)	// create artificial lag
#define CF_RESERVED4	(1ull<<43)	// legacy flag (was: character won the game (ie killed islena))
#define CF_THIEFMODE	(1ull<<44)	// is a thief and turned on thief mode
#define CF_NOTELL	(1ull<<45)	// does not wish to receive tells
#define CF_INFRAVISION	(1ull<<46)	// this character has infrared vision (player version)
#define CF_NOMAGIC	(1ull<<47)	// character is on no-magic tile and no-magic is active
#define CF_NONOMAGIC	(1ull<<48)	// character is immune to no-magic
#define CF_OXYGEN	(1ull<<49)	// character is immune to underwater
#define CF_NOPLRATT	(1ull<<50)	// may not attack/be attack (by) player(s)
#define CF_ALLOWSWAP	(1ull<<51)	// may be swapped (in spite of being an NPC)
#define CF_LQMASTER	(1ull<<52)	// may host LQs
#define CF_HARDCORE	(1ull<<53)	// hardcore character (ie. no saves, harder penalty on death, more exp for kills)
#define CF_NONOTIFY	(1ull<<54)	// this char does not send out NT_CHAR messages
#define CF_SMALLUPDATE	(1ull<<55)	// needs set_sector at next act()
#define CF_NOWHO	(1ull<<56)	// invisible to /who
#define CF_WON		(1ull<<57)	// character won the game (ie killed islena)

// VALUES
#define V_HP		0
#define V_ENDURANCE	1
#define V_MANA		2
#define V_WIS         	3
#define V_INT          	4
#define V_AGI         	5
#define V_STR       	6
#define V_ARMOR		7
#define V_WEAPON	8
#define V_LIGHT		9
#define V_SPEED		10
#define V_PULSE        	11
#define V_DAGGER       	12
#define V_HAND         	13
#define V_STAFF        	14
#define V_SWORD        	15
#define V_TWOHAND      	16
#define V_ARMORSKILL   	17
#define V_ATTACK       	18
#define V_PARRY	       	19
#define V_WARCRY       	20
#define V_TACTICS      	21
#define V_SURROUND     	22
#define V_BODYCONTROL	23
#define V_SPEEDSKILL	24
#define V_BARTER       	25
#define V_PERCEPT      	26
#define V_STEALTH      	27
#define V_BLESS		28
#define V_HEAL		29
#define V_FREEZE	30
#define V_MAGICSHIELD	31
#define V_FLASH		32
#define V_FIREBALL	33
#define V_FIRE		V_FIREBALL
#define V_EMPTY		34
#define V_REGENERATE	35
#define V_MEDITATE	36
#define V_IMMUNITY	37
#define V_DEMON		38
#define V_DURATION	39
#define V_RAGE		40
#define V_COLD		41
#define V_PROFESSION	42
#define V_MAX	       	43

// Professions
#define P_ATHLETE	0
#define P_ALCHEMIST	1
#define P_MINER		2
#define P_ASSASSIN	3
#define P_THIEF		4
#define P_LIGHT		5
#define P_DARK		6
#define P_TRADER	7
#define P_MERCENARY	8
#define P_CLAN		9
#define P_HERBALIST	10
#define P_DEMON		11
#define P_MAX		20

#define LENDESC		160

#define WN_NECK         0
#define WN_HEAD         1
#define WN_CLOAK        2
#define WN_ARMS         3
#define WN_BODY         4
#define WN_BELT         5
#define WN_RHAND        6       // weapon
#define WN_LEGS         7
#define WN_LHAND        8       // shield
#define WN_RRING        9
#define WN_FEET         10
#define WN_LRING        11

#define SM_NORMAL	0
#define SM_FAST		1
#define SM_STEALTH	2

#define INVENTORYSIZE	110

// driver data block definition
struct data
{
	int ID;
	int size;

	void *data;
	struct data *next,*prev;
};

struct character
{
	unsigned long long flags;

        char name[40];
        char description[LENDESC];
	unsigned int ID;

        unsigned int player;		// corresponding player[] entry

        unsigned int sprite;
        unsigned int sound;

        // character stats
	// [0]=total value
        // [1]=bare value, 0=unknown, not raisable

	short value[2][V_MAX];

        // temporary attributes
        int hp;
	int endurance;
        int mana;
	int lifeshield;

	int regen_ticker;

	// experience, level
        unsigned int exp;
	unsigned int exp_used;
        unsigned int level;
	
	// posessions
        unsigned int gold;

	unsigned int citem;	// current item (under mouse cursor)

        unsigned int item[INVENTORYSIZE];	// 0-11 worn, 12-29 spells, 30-(INVENTORYSIZE-1) inventory

	// chat
	unsigned int channel;	// bitmap showing which channels player is receiving
	
        // map stuff
        unsigned short x,y;     // current position x,y
        unsigned short tox,toy; // target coordinates, where the char will be next turn
        unsigned char dir;      // direction character is facing

	unsigned int sec_prev;	// management of sector list
	unsigned int sec_next;  // management of sector list

        // action etc.
	unsigned short action;
	unsigned short duration;
	unsigned short step;
	unsigned int act1,act2;

	unsigned char speed_mode;	// SM_NORMAL, SM_FAST, SM_STEALTH
	unsigned short merchant;	// merchant the character is currently dealing with
        unsigned int con_in;		// container-IN the character is currently looking at

	// driver
	unsigned short driver;	        // driver number
	unsigned short store;	        // store the character is operating
	struct data *dat;	        // pointer to list of driver data
	struct msg *msg,*msg_last;	// messages for driver

	// argument, directly passed from the zone file
	char *arg;

	// data used by NPC driver and respawn logic
	unsigned int group;	// group the character belongs to. NPCs must not have group 0.
	unsigned int serial;	// per-run, per-server unique ID
	unsigned int tmp;	// template number used for character creation
	unsigned int tmpx,tmpy;	// position to put char when respawning
	unsigned int tmpa;	// player: area to go to, NPC: per-run unique ID
	unsigned int respawn;	// time to respawn
	unsigned int class;	// class the NPC belongs to (for First-Kill-Bonus)

	// rest area data
	unsigned int resta;
	unsigned int restx;
	unsigned int resty;

	// server management of use/free list
	struct character *next,*prev;

	// character effects
	int ef[4];

	// player-only data
	int saves;			// saves the character has accumulated

	// clan stuff
	int clan;			// member of clan X, 0=not a clan member
	int clan_rank;			// rank in current clan, 0=normal member, 4=clan leader
	
        char staff_code[4];		// re-used old clan value: staffer code for easy recognition

	int clan_serial;		// used to check if the clan still exists

	// punishment & staff stuff
	int karma;

	// statistics
	int login_time;			// realtime
	int deaths;			// number of deaths
	int got_saved;			// number of saves

	// colorization
	unsigned short c1,c2,c3;

	// rage data
	int rage;
	
	// warcry repeat
	int last_regen;

	// mirrorring
	int mirror;

	// payment info
	int paid_till;

	// professions
	unsigned char prof[P_MAX];
};

extern struct character *ch;

// *********** EFFECT DEFINITION **************
#define MAXFIELD	256

struct effect
{
	int type;			// type of effect
	int serial;			// serial number of effect
	struct effect *prev,*next;	// pointers to list

	int start,stop;			// ticker effect started, ends

	int cn,sercn;			// creator of effect (caster)
	int strength;			// strength of effect (usually skill level of caster)
	int light;			// light emitted by effect

	int field_cnt;			// number of fields influenced
	int m[MAXFIELD];		// index numbers of these fields

	int efcn;			// cn the effect is on - this may or may not be the caster!

	int frx,fry,tox,toy;		// for missile effects: start and target
	int x,y;			// current position of effect
	int lx,ly;			// last position of effect
	int number_of_enemies;		// number of enemies hit by effect last tick

	int base_sprite;		// for explosion: sprite number
} *ef;

// *********** BASIC LIBRARY ******************
unsigned long long prof_start(int task);
void prof_stop(int task,unsigned long long cycle);
void show_prof(void);
int init_prof(void);
void prof_update(void);
void cmd_show_prof(int cn);

// *******************************
#ifndef EXTERNAL_PROGRAM
#define malloc	error error error
#define calloc	error error error
#define realloc	error error error
#define free	error error error
#undef strdup
#define strdup	error error error
#endif
