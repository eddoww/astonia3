#define TPS             24              // ticks per second
#define TICKS           TPS             // needed in spell.h
#define MPT             (1000/TPS)      // milliseconds per tick

#define DIST		25
#define MAXCHARS	2048

#define MAXEF		64

#define CMF_LIGHT	(1+2+4+8)
#define CMF_VISIBLE	16
#define CMF_TAKE	32
#define CMF_USE		64
#define CMF_INFRA	128
#define CMF_UNDERWATER	256

#define VERSION         0x030100

struct complex_sprite
{
	unsigned int sprite;
        unsigned short c1,c2,c3,shine;
	unsigned char cr,cg,cb;
	unsigned char light,sat;
	unsigned char scale;
};

struct map
{
	// from map & item
	unsigned short int gsprite;     // background sprite
	unsigned short int gsprite2;    // background sprite
        unsigned short int fsprite;     // foreground sprite
        unsigned short int fsprite2;    // foreground sprite

	unsigned int isprite;		// item sprite
	unsigned short ic1,ic2,ic3;
	
	unsigned int flags;            	// see CMF_

	// character
	unsigned int csprite;		// character base sprite
	unsigned int cn;		// character number (for commands)
	unsigned char cflags;		// character flags
	//unsigned char level;		// character level
	unsigned char action;		// character action, duration and step
	unsigned char duration;
	unsigned char step;	
	unsigned char dir;		// direction the character is facing
	unsigned char health;		// character health (in percent)
	unsigned char mana;
	unsigned char shield;
        // 15 bytes

	// effects
	unsigned int ef[4];

	unsigned char sink;		// sink characters on this field	
        int value;                      // testing purposes only
        int mmf;                        // more flags
        char rlight;                    // real client light - 0=invisible 1=dark, 14=normal (15=bright can't happen)
	//char dim; 	what?
	struct complex_sprite rc;

        /*unsigned int rcsprite;          // real sprite of character    (csprite)
	unsigned char rc_scale;		// scale of char sprite
	unsigned int rc_tint;
	unsigned int rc_c1,rc_c2,rc_c3,rc_c4;*/

	struct complex_sprite ri;

	/*unsigned int risprite;          // real sprite of item         (isprite)
	unsigned char ri_scale;
	unsigned int ri_tint;
	unsigned short ri_c1,ri_c2,ri_c3,ri_c4;*/

	struct complex_sprite rf;
	struct complex_sprite rf2;
	struct complex_sprite rg;
	struct complex_sprite rg2;
        //unsigned int rfsprite;          // real sprite of item         (isprite)
        //unsigned int rfsprite2;         // real sprite of item         (isprite)
        //unsigned int rgsprite;          // real sprite of ground       (gsprite)
        //unsigned int rgsprite2;         // real sprite of ground       (gsprite)

	char xadd;                      // add this to the x position of the field used for c sprite
        char yadd;                      // add this to the y position of the field used for c sprite

#ifdef EDITOR
        unsigned int ex_flags;          // extended flags of the editor
        unsigned int ex_map_flags;      // copy of the server flags (for display)
        char *ex_chrname;               // chr name - beware of pointers (might explode if we dynamically change items and chars)
        char *ex_itmname;               // itm name - beware of pointers
	unsigned short sec_nr,sec_line;
#endif

};

struct cef_generic
{
	int nr;
	int type;
};

struct cef_shield
{
	int nr;
	int type;
	int cn;
        int start;
};

struct cef_strike
{
	int nr;
	int type;
	int cn;
        int x,y;	// target
};

struct cef_ball
{
	int nr;
	int type;
	int start;
	int frx,fry;	// high precision coords	
	int tox,toy;	// high precision coords	
};

struct cef_fireball
{
	int nr;
	int type;
	int start;
	int frx,fry;	// high precision coords	
	int tox,toy;	// high precision coords	
};

struct cef_edemonball
{
	int nr;
	int type;
	int start;
	int base;
	int frx,fry;	// high precision coords	
	int tox,toy;	// high precision coords
};

struct cef_flash
{
	int nr;
	int type;
	int cn;
};

struct cef_explode
{
	int nr;
	int type;
	int start;
	int base;
};

struct cef_warcry
{
	int nr;
	int type;
	int cn;
	int stop;
};

struct cef_bless
{
	int nr;
	int type;
	int cn;
	int start;
	int stop;
	int strength;
};

struct cef_heal
{
	int nr;
	int type;
	int cn;
	int start;
};

struct cef_freeze
{
	int nr;
	int type;
	int cn;
	int start;
	int stop;
};

struct cef_burn
{
	int nr;
	int type;
	int cn;
	int stop;
};

struct cef_mist
{
	int nr;
	int type;
	int start;
};

struct cef_pulse
{
	int nr;
	int type;
	int start;
};

struct cef_pulseback
{
	int nr;
	int type;
	int cn;
	int x,y;
};

struct cef_potion
{
	int nr;
	int type;
	int cn;
	int start;
	int stop;
	int strength;
};

struct cef_earthrain
{
	int nr;
	int type;
	int strength;
};

struct cef_earthmud
{
	int nr;
	int type;
};

struct cef_curse
{
	int nr;
	int type;
	int cn;
	int start;
	int stop;
	int strength;
};

struct cef_cap
{
	int nr;
	int type;
	int cn;
};

struct cef_lag
{
	int nr;
	int type;
	int cn;
};

struct cef_firering
{
	int nr;
	int type;
	int cn;
	int start;
};

struct cef_bubble
{
	int nr;
	int type;
	int yoff;
};

union ceffect
{
	struct cef_generic generic;
	struct cef_shield shield;
	struct cef_strike strike;
	struct cef_ball ball;
	struct cef_fireball fireball;
	struct cef_flash flash;
	struct cef_explode explode;
	struct cef_warcry warcry;
	struct cef_bless bless;
	struct cef_heal heal;
	struct cef_freeze freeze;
	struct cef_burn burn;
	struct cef_mist mist;
	struct cef_potion potion;
	struct cef_earthrain earthrain;
	struct cef_earthmud earthmud;
	struct cef_edemonball edemonball;
	struct cef_curse curse;
	struct cef_cap cap;
	struct cef_lag lag;
	struct cef_pulse pulse;
	struct cef_pulseback pulseback;
	struct cef_firering firering;
	struct cef_bubble bubble;
};

#define IF_USE		(1<<4)
#define IF_WNHEAD       (1<<5)	// can be worn on head
#define IF_WNNECK       (1<<6)	// etc...
#define IF_WNBODY       (1<<7)
#define IF_WNARMS       (1<<8)
#define IF_WNBELT       (1<<9)
#define IF_WNLEGS       (1<<10)
#define IF_WNFEET       (1<<11)
#define IF_WNLHAND      (1<<12)
#define IF_WNRHAND      (1<<13)
#define IF_WNCLOAK      (1<<14)
#define IF_WNLRING      (1<<15)
#define IF_WNRRING      (1<<16)
#define IF_WNTWOHANDED	(1<<17)	// two-handed weapon, fills both WNLHAND & WNRHAND


#define SV_SCROLL_UP		1
#define SV_SCROLL_DOWN		2
#define SV_SCROLL_LEFT		3
#define SV_SCROLL_RIGHT		4
#define SV_SCROLL_LEFTUP	5
#define SV_SCROLL_RIGHTUP	6
#define SV_SCROLL_LEFTDOWN	7
#define SV_SCROLL_RIGHTDOWN	8
#define SV_TEXT			9
#define SV_SETVAL0		10
#define SV_SETVAL1		11
#define SV_SETHP		12
#define SV_SETMANA		13
#define SV_SETITEM		14
#define SV_SETORIGIN		15
#define SV_SETTICK		16
#define SV_SETCITEM		17
#define SV_ACT			18
#define SV_EXIT			19
#define SV_NAME			20
#define SV_SERVER		21
#define SV_CONTAINER		22
#define SV_CONCNT		23
#define SV_ENDURANCE		24
#define SV_LIFESHIELD		25
#define SV_EXP			26
#define SV_EXP_USED		27
#define SV_PRICE		28
#define SV_CPRICE		29
#define SV_GOLD			30
#define SV_LOOKINV		31
#define SV_ITEMPRICE		32
#define SV_CYCLES		33
#define SV_CEFFECT		34
#define SV_UEFFECT		35
#define SV_REALTIME		36
#define SV_SPEEDMODE		37
#define SV_FIGHTMODE		38
#define SV_CONTYPE		39
#define SV_CONNAME		40
#define SV_LS 		  	41
#define SV_CAT			42
#define SV_LOGINDONE		43
#define SV_SPECIAL		44
#define SV_TELEPORT		45
#define SV_SETRAGE		46
#define SV_MIRROR		47
#define SV_PROF			48
#define SV_PING			49
#define SV_UNIQUE		50
#define SV_MIL_EXP		51
#define SV_QUESTLOG		52

#define SV_MAPTHIS		0
#define SV_MAPNEXT		16
#define SV_MAPOFF		32
#define SV_MAPPOS		(16+32)

#define SV_MAP01		64
#define SV_MAP10		128
#define SV_MAP11		(64+128)

#define CL_NOP			1
#define CL_MOVE			2
#define CL_SWAP			3
#define CL_TAKE			4
#define CL_DROP			5
#define CL_KILL			6
#define CL_CONTAINER		7
#define CL_TEXT			8
#define CL_USE			9
#define CL_BLESS		10
#define CL_FIREBALL		11
#define CL_HEAL			12
#define CL_MAGICSHIELD		13
#define CL_FREEZE		14
#define CL_RAISE		15	
#define CL_USE_INV		16	
#define CL_FLASH		17
#define CL_BALL			18
#define CL_WARCRY		19
#define CL_LOOK_CONTAINER	20
#define CL_LOOK_MAP		21
#define CL_LOOK_INV		22
#define CL_LOOK_CHAR		23
#define CL_LOOK_ITEM		24
#define CL_GIVE			25
#define CL_SPEED		26
#define CL_STOP			27
#define CL_TAKE_GOLD		28
#define CL_DROP_GOLD		29
#define CL_JUNK_ITEM		30
#define CL_CLIENTINFO		31
#define CL_FIGHTMODE		32
#define CL_TICKER		33
#define CL_CONTAINER_FAST	34
#define CL_FASTSELL		35
#define CL_LOG			36
#define CL_TELEPORT		37
#define CL_PULSE		38
#define CL_PING			39
#define CL_GETQUESTLOG		40
#define CL_REOPENQUEST		41

// header for client

#ifdef ISCLIENT

#define V_HP		0
#define V_ENDURANCE	1
#define V_MANA		2

#define V_WIS         	3
#undef  V_INT           // everyone likes windoof
#define V_INT          	4
#define V_AGI         	5
#define V_STR       	6

#define V_ARMOR		7
#define V_WEAPON	8
#define V_LIGHT		9
#define V_SPEED		10

#define V_PULSE		11
#define V_DAGGER       	12
#define V_HAND         	13
#define V_STAFF        	14
#define V_SWORD        	15
#define V_TWOHAND      	16

#define V_ARMORSKILL   	17
#define V_ATTACK       	18
#define V_PARRY	       	19
#define V_WARCRY       	20
//#define V_BERSERK      	21
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
//#define V_BALL		34

#define V_REGENERATE	35
#define V_MEDITATE	36
#define V_IMMUNITY	37

#define V_DEMON		38
#define V_RAGE		40
#define V_COLD		41
#define V_PROFESSION	42

#define V_PROFBASE	43
#define P_MAX		20
#define V_MAX	       	(V_PROFBASE+P_MAX)

//#define MAX_ITEM	70
#define INVENTORYSIZE	110
#define CONTAINERSIZE	(INVENTORYSIZE)


#define PAC_IDLE	0			
#define PAC_MOVE	1
#define PAC_TAKE	2
#define PAC_DROP	3
#define PAC_KILL	4
#define PAC_USE		5
#define PAC_BLESS	6
#define PAC_HEAL	7
#define PAC_FREEZE	8
#define PAC_FIREBALL	9
#define PAC_BALL	10
#define PAC_MAGICSHIELD	11
#define PAC_FLASH	12
#define PAC_WARCRY	13
#define PAC_LOOK_MAP	14
#define PAC_GIVE	15
#define PAC_BERSERK	16

extern int quit;
void addline(const char *format,...);

#define MAPDX			(DIST*2+1)
#define MAPDY			(DIST*2+1)
#define MAXMN                   (MAPDX*MAPDY)

/*
#define DX_UP                   1
#define DX_DOWN                 2
#define DX_LEFT                 3
#define DX_RIGHT                4
#define DX_LEFTUP               5
#define DX_LEFTDOWN             6
#define DX_RIGHTUP              7					
#define DX_RIGHTDOWN            8					
									
#define DX_N                    DX_LEFTUP				
#define DX_S                    DX_RIGHTDOWN
#define DX_W                    DX_LEFTDOWN
#define DX_E                    DX_RIGHTUP
#define DX_NW                   DX_LEFT
#define DX_SW                   DX_DOWN
#define DX_NE                   DX_UP
#define DX_SE                   DX_RIGHT
*/

#define MAX_INBUF	        0xFFFFF
#define MAX_OUTBUF	        0xFFFFF

// #define mapmn(x,y) 	(x+y*MAPDX)
static int mapmn(int x, int y)
{
	if (x<0 || y<0 || x>=MAPDX || y>=MAPDY) {
		// printf("noe %d %d\n",x,y);
		return -1;
	}
	return (x+y*MAPDX);
}

static int onmap(int x, int y)
{
	if (x<0 || y<0 || x>=MAPDX || y>=MAPDY) return -1;       // field 0,0 is declared to be not not on the map !
        return (x+y*MAPDX);
}

int record_client(char *filename);
int open_client(char *username, char *password);
int close_client(void);
int poll_network(void);
int next_tick(void);
int do_tick(void);
int init_network(void);
void exit_network(void);
int find_ceffect(int fn);
int find_cn_ceffect(int cn,int skip);

void cmd_move(int x, int y);
void cmd_swap(int with);
void cmd_use_inv(int with);
void cmd_take(int x, int y);
void cmd_look_map(int x, int y);
void cmd_look_item(int x, int y);
void cmd_look_inv(int pos);
void cmd_look_char(int cn);
void cmd_use(int x, int y);
void cmd_drop(int x, int y);
void cmd_speed(int mode);
void cmd_combat(int mode);
void cmd_log(char *text);
void cmd_stop(void);
void cmd_kill(int cn);
void cmd_give(int cn);
void cmd_some_spell(int spell, int x, int y, int chr);
void cmd_raise(int vn);
void cmd_text(char *text);
void cmd_con(int pos);
void cmd_look_con(int pos);
void cmd_drop_gold(void);
void cmd_take_gold(int amount);
void cmd_junk_item(void);
void cmd_getquestlog(void);
void cmd_reopen_quest(int nr);

extern int sock;
extern int sockstate;
extern unsigned int socktime;
extern int target_port;
extern int target_server;
extern int update_server;
extern int base_server;
extern int backup_server;
extern int developer_server;
extern char username[40];
extern char password[16];
extern unsigned int unique;

// struct client

extern int tick;
extern int lasttick;                    // ticks in inbuf

// extern int lastticksize;             // size inbuf must reach to get the last tick complete in the queue

// extern struct z_stream_s zs;

// extern int ticksize;
// extern int inused;
// extern int indone;
// extern unsigned char inbuf[MAX_INBUF];

// extern int outused;
// extern unsigned char outbuf[MAX_OUTBUF];

extern int act;
extern int actx;
extern int acty;

extern unsigned int cflags;		// current item flags
extern unsigned int csprite;		// and sprite

extern int originx;
extern int originy;
extern struct map map[MAPDX*MAPDY];
extern struct map map2[MAPDX*MAPDY];

extern int value[2][V_MAX];
extern int item[INVENTORYSIZE];
extern int item_flags[INVENTORYSIZE];
extern int hp;
extern int mana;
extern int rage;
extern int endurance;
extern int lifeshield;
extern int experience;
extern int experience_used;
extern int gold;

extern union ceffect ceffect[MAXEF];
extern unsigned char ueffect[MAXEF];

struct player
{
	char name[80];
	int csprite;
	short level;
	unsigned short c1,c2,c3;
	unsigned char clan;
	unsigned char pk_status;
};

extern struct player player[MAXCHARS];

extern int con_cnt;
extern int con_type;
extern char con_name[80];
extern int container[CONTAINERSIZE];
extern int price[CONTAINERSIZE];
extern int itemprice[CONTAINERSIZE];
extern int cprice;

extern int lookinv[12];
extern int looklevel;

extern int pspeed;   // 0=ill 1=stealth 2=normal 3=fast
extern int pcombat;  // 0=ill 1=defensive 2=balanced 3=offensive

#endif

#define CL_MAX_SURFACE	32

struct client_surface
{
	unsigned int xres:14;
	unsigned int yres:14;
	unsigned int type:4;
};

struct client_info
{
	unsigned int skip;
	unsigned int idle;
	unsigned int vidmemtotal;
	unsigned int vidmemfree;
	unsigned int systemtotal;
	unsigned int systemfree;
	struct client_surface surface[CL_MAX_SURFACE];
};

#define MAXQUEST	100
#define QF_OPEN		1
#define QF_DONE		2
struct quest {
	unsigned char done:6;
	unsigned char flags:2;
};
extern struct quest quest[];

#define MAXSHRINE	256
struct shrine_ppd
{
	unsigned int used[MAXSHRINE/32];
	unsigned char continuity;
};
extern struct shrine_ppd shrine;


int exp2level(int val);
void cmd_fastsell(int with);
void cmd_con_fast(int pos);
void cmd_teleport(int nr);
void cl_client_info(struct client_info *ci);
void cl_ticker(void);
int level2exp(int level);
int is_char_ceffect(int type);
