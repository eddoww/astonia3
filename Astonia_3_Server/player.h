/*
 *
 * $Id: player.h,v 1.3 2006/09/14 09:55:22 devel Exp $
 *
 * $Log: player.h,v $
 * Revision 1.3  2006/09/14 09:55:22  devel
 * added questlog
 *
 * Revision 1.2  2005/12/07 13:36:34  ssim
 * added military exp transmission
 *
 */

#define MAXPLAYER	256

#define ST_CONNECT	1
#define ST_NORMAL	2
#define ST_EXIT		3

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
#define PAC_FIREBALL2	16
#define PAC_BALL2	17
#define PAC_TELEPORT	18
#define PAC_PULSE	19

#define MAXEF		64

#ifdef NEED_PLAYER_STRUCT
#define OBUFSIZE	16384

#define MAXSCROLLBACK	8192

struct cmd_queue
{
	int action;
	int act1,act2;
};

struct player
{
	// connection data
	int sock;
	unsigned int addr;

	// idle ticker
	int lastcmd;

	// input buffer
	unsigned char inbuf[256];
	int in_len;

	// output ring buffer
        unsigned char obuf[OBUFSIZE];
	int iptr,optr;

	// pre-compress buffer
	unsigned char tbuf[OBUFSIZE];
	int tptr;

	// state of connection (NEW/LOGIN/PLAYING etc)
        unsigned int state;

	// character data
	unsigned long long usnr;	// character ID
	unsigned int cn;		// character number on current server
	char passwd[16];		// character password as sent from client
	char command[256];		// command issued by player
	
        // for compression
        struct z_stream_s zs;

	// cache of client data
	struct cmap cmap[(DIST*2+1)*(DIST*2+1)];	// map
	int x,y;					// last position on map
	
	short value[2][V_MAX];				// character values
	
	unsigned int item[INVENTORYSIZE];		// item carried by character
	unsigned int item_flags[INVENTORYSIZE];		// item flags for inv
	unsigned int item_price[INVENTORYSIZE];		// item price for inv
	
	short hp,mana,endurance,lifeshield;		// actual power values
	
	unsigned int citem_sprite,citem_flags;		// current item (mouse cursor) sprite and flags
	
	unsigned char nameflag[TOTAL_MAXCHARS/8];		// flag is set if client has the name for character X
	
	unsigned int exp,exp_used,gold,mil_exp;
	unsigned int speed_mode,fight_mode;
	int rage;
	
	int con_type;					// 0 = none, 1 = dead body, 2 = store
	char con_name[80];
	int con_cnt;					// number of items in container/store
	int container[INVENTORYSIZE];			// sprites of items in container/store
	int price[INVENTORYSIZE];			// prices in store
	int cprice;

	union ceffect ceffect[MAXEF];
	unsigned int seffect[MAXEF];
	unsigned long long uf;

	// player character driver
	int action;					// action
	int act1,act2;					// vars
	struct cmd_queue queue[16];
	int ticker;					// last tick we received from client
	int next_fightback_cn;
	int next_fightback_serial;
	int next_fightback_ticker;
	int nofight_timer;

	unsigned char svactbuf[7];

	// speed-ups
	int dlight;

	// statistics
	int login_time;

	// complaint buffer
	char scrollback[MAXSCROLLBACK];
	int scrollpos;
};

extern struct player **player;
//extern struct player *player;
#endif

void tick_player(void);
int log_player(int nr,int color,char *format,...);
void set_player_knows_name(int nr,int cn,int flag);
void kick_player(int nr,char *reason);
void exit_char(int cn);
void backup_players(void);
void player_to_server(int nr,unsigned int server,int port);
void plr_send_inv(int cn,int co);
void player_driver(int cn,int ret,int last_action);
void player_idle(int nr);
void player_client_exit(int nr,char *reason);
void kick_char(int cn);
void write_scrollback(int nr,int cn,char *reason,char *namea,char *nameb);
void plr_ls(int cn,char *dir);
void plr_cat(int cn,char *dir);
void player_special(int cn,unsigned int type,unsigned int opt1,unsigned int opt2);
void reset_name(int cn);
void sanity_check(int cn);
void player_reset_map_cache(int nr);
void exit_char_player(int cn);
unsigned int get_player_addr(int nr);
void sendquestlog(int cn,int nr);




