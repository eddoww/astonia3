/*
 * $Id: client.h,v 1.3 2006/09/14 09:55:22 devel Exp $
 *
 * $Log: client.h,v $
 * Revision 1.3  2006/09/14 09:55:22  devel
 * added questlog
 *
 * Revision 1.2  2005/12/07 13:35:56  ssim
 * added transmission of military exp
 */

#ifndef DIST
#define DIST		25
#endif

#define CMF_LIGHT	(1+2+4+8)
#define CMF_VISIBLE	16
#define CMF_TAKE	32
#define CMF_USE		64
#define CMF_INFRA	128
#define CMF_UNDERWATER	256

struct cmap
{
        unsigned int gsprite;          	// background image
        unsigned int fsprite;         	// foreground sprite
	unsigned int isprite;		// item sprite
	unsigned short ef[4];
	
        unsigned int csprite;		// character base sprite
	unsigned int cn;		// character number (for commands)

	unsigned int flags;             // see CMF_

	unsigned char action;		// character action, duration and step
	unsigned char duration;

	unsigned char step;	
	unsigned char dir;		// direction the character is facing
	unsigned char health;		// character health (in percent)
	unsigned char mana;		// character mana (in percent)
        unsigned char shield;		// character shield (in percent)
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

struct cef_pulseback
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

struct cef_edemonball
{
	int nr;
	int type;
	int start;
	int base;
	int frx,fry;	// high precision coords
	int tox,toy;	// high precision coords
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
#define SV_ORIGIN		15
#define SV_TICKER		16
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

// MAX SV IS 63 !

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







