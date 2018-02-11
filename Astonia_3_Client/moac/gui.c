
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#pragma hdrstop

#define ISCLIENT
#include "main.h"
#include "dd.h"
#include "client.h"
#include "skill.h"
#include "sprite.h"
#include "gui.h"
#include "resource.h"
#include "spell.h"
#include "sound.h"

// extern

extern int quit;
extern HINSTANCE instance;
extern HWND mainwnd;
void set_map_values(struct map *cmap, int attick);
extern void display_game(void);
extern void prefetch_game(int attick);
extern void init_game(void);
extern void exit_game(void);
void display_cmd(void);
extern int gfx_force_png;
extern int gfx_force_dh;
extern int mirror,newmirror;

#define MAXHELP		24
#define MAXQUEST2	10

#ifdef EDITOR
extern int init_editor(void);
extern int exit_editor(void);
extern void editor_keyproc(int wparam);
extern int editor_mouseproc(int msg);
extern int editor_sizeproc(void);
extern void editor_tinproc(int t, char *buf);
#endif

void cmd_add_text(char *buf);

// globals

int skip=1,idle=0,tota=1,frames=0;
int winxres,winyres;

// globals display

int display_vc=0;

int playersprite_override=0;
int lightquality=0;
int nocut=0;
int allcut=0;

int update_skltab=0;
int show_look=0;

unsigned short int healthcolor,manacolor,endurancecolor,shieldcolor;
unsigned short int whitecolor,lightgraycolor,graycolor,darkgraycolor,blackcolor;
unsigned short int lightredcolor,redcolor,darkredcolor;
unsigned short int lightgreencolor,greencolor,darkgreencolor;
unsigned short int lightbluecolor,bluecolor,darkbluecolor;
unsigned short int textcolor;
unsigned short int lightorangecolor,orangecolor,darkorangecolor;

unsigned int now;

HCURSOR c_only,c_take,c_drop,c_attack,c_raise,c_give,c_use,c_usewith,c_swap,c_sell,c_buy,c_look,c_set,c_spell,c_pix,c_say,c_junk,c_get;
HCURSOR cur_cursor=NULL;
int mousex,mousey,vk_shift,vk_control,vk_alt,vk_rbut,vk_lbut,shift_override=0,control_override=0;
int mousedx,mousedy;
int vk_item,vk_char,vk_spell;

int vk_special=0,vk_special_time=0;

DOT *dot=NULL;
BUT *but=NULL;

// globals wea

int weatab[12]={9,6,8,11,0,1,2,4,5,3,7,10};
char weaname[12][32]={"RING","HAND","HAND","RING","NECK","HEAD","BACK","BODY","BELT","ARMS","LEGS","FEET"};

// globals skltab

#define STV_EMPTYLINE           -1
#define STV_JUSTAVALUE          -2      // value is in curr

struct skltab
{
        int v;                          // negative v-values indicate a special display (empty lines, negative exp, etc...)
	int button;			// show button
        char name[80];
        int base;
        int curr;
        int raisecost;
        int barsize;                    // positive is blue, negative is red
};

typedef struct skltab SKLTAB;

// globals spell

struct spell
{
        int cl;                         // id of spell sent to server (0=look/spellmode change)
        char name[40];                  // name in text display
};

typedef struct spell SPELL;

// globals keytab

#define TGT_MAP 1
#define TGT_ITM 2
#define TGT_CHR 3
#define TGT_SLF 4

struct keytab
{
        int keycode;
	int userdef;
        int vk_item,vk_char,vk_spell;
        char name[40];
        int tgt;
        int cl_spell;
        int skill;
	unsigned int usetime;
};

typedef struct keytab KEYTAB;

KEYTAB keytab[]= {
{ '1',0,0,1,0,"FIREBALL"        ,TGT_CHR,CL_FIREBALL, 		V_FIREBALL },
{ '2',0,0,1,0,"LIGHTNINGBALL"     ,TGT_CHR,CL_BALL, 		V_FLASH },
{ '3',0,0,1,0,"FLASH"             ,TGT_SLF,CL_FLASH, 		V_FLASH },
{ '4',0,0,1,0,"FREEZE"            ,TGT_SLF,CL_FREEZE, 		V_FREEZE },
{ '5',0,0,1,0,"SHIELD"            ,TGT_SLF,CL_MAGICSHIELD,	V_MAGICSHIELD },
{ '6',0,0,1,0,"BLESS"             ,TGT_CHR,CL_BLESS,		V_BLESS },
{ '7',0,0,1,0,"HEAL"              ,TGT_CHR,CL_HEAL,		V_HEAL },
{ '8',0,0,1,0,"WARCRY"            ,TGT_SLF,CL_WARCRY,		V_WARCRY },
{ '9',0,0,1,0,"PULSE"             ,TGT_SLF,CL_PULSE,		V_PULSE },
{ '0',0,0,1,0,"FIRERING"          ,TGT_SLF,CL_FIREBALL,		V_FIREBALL },

{ '1',0,0,1,1,"FIREBALL"          ,TGT_CHR,CL_FIREBALL,		V_FIREBALL },
{ '2',0,0,1,1,"LIGHTNINGBALL"     ,TGT_CHR,CL_BALL,		V_FLASH },
{ '3',0,0,1,1,"FLASH"             ,TGT_SLF,CL_FLASH,		V_FLASH },
{ '4',0,0,1,1,"FREEZE"            ,TGT_SLF,CL_FREEZE,		V_FREEZE },
{ '5',0,0,1,1,"SHIELD"            ,TGT_SLF,CL_MAGICSHIELD,	V_MAGICSHIELD },
{ '6',0,0,1,1,"BLESS"             ,TGT_CHR,CL_BLESS,		V_BLESS },
{ '7',0,0,1,1,"HEAL"              ,TGT_CHR,CL_HEAL,		V_HEAL },
{ '8',0,0,1,1,"WARCRY"            ,TGT_SLF,CL_WARCRY,		V_WARCRY },
{ '9',0,0,1,1,"PULSE"             ,TGT_SLF,CL_PULSE,		V_PULSE },
{ '0',0,0,1,1,"FIRERING"          ,TGT_SLF,CL_FIREBALL,		V_FIREBALL},

{ '1',0,0,0,1,"FIREBALL"          ,TGT_MAP,CL_FIREBALL,           V_FIREBALL },
{ '2',0,0,0,1,"LIGHTNINGBALL"     ,TGT_MAP,CL_BALL,               V_FLASH },
{ '3',0,0,0,1,"FLASH"             ,TGT_SLF,CL_FLASH,              V_FLASH },
{ '4',0,0,0,1,"FREEZE"            ,TGT_SLF,CL_FREEZE,             V_FREEZE },
{ '5',0,0,0,1,"SHIELD"            ,TGT_SLF,CL_MAGICSHIELD,        V_MAGICSHIELD },
{ '6',0,0,0,1,"BLESS SELF"        ,TGT_SLF,CL_BLESS,              V_BLESS },
{ '7',0,0,0,1,"HEAL SELF"         ,TGT_SLF,CL_HEAL,               V_HEAL },
{ '8',0,0,0,1,"WARCRY"            ,TGT_SLF,CL_WARCRY,             V_WARCRY },
{ '9',0,0,0,1,"PULSE"             ,TGT_SLF,CL_PULSE,		V_PULSE },
{ '0',0,0,0,1,"FIRERING"          ,TGT_SLF,CL_FIREBALL,		V_FIREBALL},
};

int max_keytab=sizeof(keytab)/sizeof(KEYTAB);

struct special_tab
{
	char *name;
	int shift_over;
	int control_over;
	int spell,target;
	int req;
};

struct special_tab special_tab[]={
	{"Walk",		0,0,0,			0,		0},
	{"Use/Take",		1,0,0,			0,		0},
	{"Attack/Give",		0,1,0,			0,		0},
	{"Warcry",		0,0,CL_WARCRY,		TGT_SLF,	V_WARCRY},
	{"Pulse",		0,0,CL_PULSE,		TGT_SLF,	V_PULSE},
        {"Fireball-CHAR",	0,1,CL_FIREBALL,	TGT_CHR,	V_FIREBALL},
	{"Fireball-MAP",	0,0,CL_FIREBALL,	TGT_MAP,	V_FIREBALL},
	{"Firering",		0,0,CL_FIREBALL,	TGT_SLF,	V_FIREBALL},
	{"LBall-CHAR",		0,1,CL_BALL,		TGT_CHR,	V_FLASH},
	{"LBall-MAP",		0,0,CL_BALL,		TGT_MAP,	V_FLASH},
	{"Flash",		0,0,CL_FLASH,		TGT_SLF,	V_FLASH},
	{"Freeze",		0,0,CL_FREEZE,		TGT_SLF,	V_FREEZE},
	{"Shield",		0,0,CL_MAGICSHIELD,	TGT_SLF,	V_MAGICSHIELD},
	{"Bless-SELF",		0,0,CL_BLESS,		TGT_SLF,	V_BLESS},
	{"Bless-CHAR",		0,1,CL_BLESS,		TGT_CHR,	V_BLESS},
	{"Heal-SELF",		0,0,CL_HEAL,		TGT_SLF,	V_HEAL},
	{"Heal-CHAR",		0,1,CL_HEAL,		TGT_CHR,	V_HEAL}
};
int max_special=sizeof(special_tab)/sizeof(special_tab[0]);

int fkeyitem[4];

// globals cmd

int plrmn;                      // mn of player

int mapsel;                     // mn
int itmsel;                     // mn
int chrsel;                     // mn
int invsel;                     // index into item
int weasel;                     // index into weatab
int consel;                     // index into item
int splsel;
int sklsel;
int butsel;                     // is always set, if any of the others is set
int telsel;
int helpsel;
int questsel;
int colsel;
int skl_look_sel;

int capbut;                     // the button capturing the mouse

int takegold;                   // the amout of gold to take

char hitsel[256];               // something in the text (dx_drawtext()) is selected

SKLTAB *skltab=NULL;
int skltab_max=0;
int skltab_cnt=0;

int invoff,max_invoff;
int conoff,max_conoff;
int skloff,max_skloff;

int lcmd;
int rcmd;

int curspell_l=4;               // index into spelltab
int curspell_r=6;               // index into spelltab

#define CMD_NONE                0
#define CMD_MAP_MOVE            1
#define CMD_MAP_DROP            2

#define CMD_ITM_TAKE            3
#define CMD_ITM_USE             4
#define CMD_ITM_USE_WITH        5

#define CMD_CHR_ATTACK          6
#define CMD_CHR_GIVE            7

#define CMD_INV_USE             8
#define CMD_INV_USE_WITH        9
#define CMD_INV_TAKE            10
#define CMD_INV_SWAP            11
#define CMD_INV_DROP            12

#define CMD_WEA_USE             13
#define CMD_WEA_USE_WITH        14
#define CMD_WEA_TAKE            15
#define CMD_WEA_SWAP            16
#define CMD_WEA_DROP            17

#define CMD_CON_TAKE            18
#define CMD_CON_BUY             19
#define CMD_CON_SWAP            20
#define CMD_CON_DROP            21
#define CMD_CON_SELL            22

#define CMD_MAP_LOOK            23
#define CMD_ITM_LOOK            24
#define CMD_CHR_LOOK            25
#define CMD_INV_LOOK            26
#define CMD_WEA_LOOK            27
#define CMD_CON_LOOK            28

#define CMD_MAP_CAST_L          29
#define CMD_ITM_CAST_L          30
#define CMD_CHR_CAST_L          31
#define CMD_MAP_CAST_R          32
#define CMD_ITM_CAST_R          33
#define CMD_CHR_CAST_R          34
#define CMD_MAP_CAST_K        	35
#define CMD_CHR_CAST_K        	36
#define CMD_SLF_CAST_K        	37

#define CMD_SPL_SET_L           38
#define CMD_SPL_SET_R           39

#define CMD_SKL_RAISE           40

#define CMD_INV_OFF_UP          41
#define CMD_INV_OFF_DW          42
#define CMD_INV_OFF_TR          43

#define CMD_SKL_OFF_UP          44
#define CMD_SKL_OFF_DW          45
#define CMD_SKL_OFF_TR          46

#define CMD_CON_OFF_UP          47
#define CMD_CON_OFF_DW          48
#define CMD_CON_OFF_TR          49

#define CMD_USE_FKEYITEM        50

#define CMD_SAY_HITSEL          51

#define CMD_DROP_GOLD           52
#define CMD_TAKE_GOLD           53

#define CMD_JUNK_ITEM           54

#define CMD_SPEED0              55
#define CMD_SPEED1              56
#define CMD_SPEED2              57
#define CMD_COMBAT0             58
#define CMD_COMBAT1             59
#define CMD_COMBAT2             60

#define CMD_CON_FASTTAKE	61
#define CMD_CON_FASTBUY		62
#define CMD_CON_FASTSELL	63
#define CMD_TELEPORT		64
#define CMD_CON_FASTDROP	65

#define CMD_HELP_NEXT           66
#define CMD_HELP_PREV           67
#define CMD_HELP_MISC           68
#define CMD_HELP_CLOSE		69
#define CMD_EXIT		70
#define CMD_HELP		71
#define CMD_NOLOOK		72

#define CMD_COLOR		73
#define CMD_SKL_LOOK		74
#define CMD_QUEST		75

// globals tin (text input)
//TIN *tin;
//int curtin;

//void display_tin(int t,int something);
//void tin_do(int t, int tindo, char c);

// dot and but helpers

void set_dot(int didx, int x, int y, int flags)
{
        PARANOIA( if (didx<0 || didx>=MAX_DOT) paranoia("set_dot: ill didx=%d",didx); )

        dot[didx].flags=flags;
        dot[didx].x=x;
        dot[didx].y=y;
}

void set_but(int bidx, int x, int y, int hitrad, int id, int val, int flags)
{
        PARANOIA( if (bidx<0 || bidx>=MAX_BUT) paranoia("set_but: ill bidx=%d",bidx); )

        but[bidx].flags=flags;

        but[bidx].id=id;
        but[bidx].val=val;

        but[bidx].x=x;
        but[bidx].y=y;

        but[bidx].sqhitrad=hitrad*hitrad;
}

// transformation

int mapoffx,mapoffy;
int mapaddx,mapaddy;

void set_mapoff(int cx, int cy, int mdx, int mdy)
{
        mapoffx=(cx-(mdx/2-mdy/2)*(FDX/2));
        mapoffy=(cy-(mdx/2+mdy/2)*(FDY/2));
}

void set_mapadd(int addx, int addy)
{
        mapaddx=addx;
        mapaddy=addy;
}

void mtos(int mapx, int mapy, int *scrx, int *scry)
{
        *scrx=(mapoffx+mapaddx) + (mapx-mapy)*(FDX/2);
        *scry=(mapoffy+mapaddy) + (mapx+mapy)*(FDY/2);
}

void stom(int scrx, int scry, int *mapx, int *mapy)
{
        scrx-=(mapoffx+mapaddx);
        scry-=(mapoffy+mapaddy)-10;
        *mapy=(40*scry-20*scrx-1)/(20*40);      // ??? -1 ???
        *mapx=(40*scry+20*scrx)/(20*40);
}

// dx

void dx_copysprite_emerald(int scrx, int scry, int emx, int emy)
{
        DDFX ddfx;

        bzero(&ddfx,sizeof(ddfx));
        ddfx.sprite=37;
        ddfx.align=DD_OFFSET;
        ddfx.clipsx=emx*10;
        ddfx.clipsy=emy*10;
        ddfx.clipex=ddfx.clipsx+10;
        ddfx.clipey=ddfx.clipsy+10;
        ddfx.ml=ddfx.ll=ddfx.rl=ddfx.ul=ddfx.dl=DDFX_NLIGHT;
	ddfx.scale=100;
        dd_copysprite_fx(&ddfx,scrx-ddfx.clipsx-5,scry-ddfx.clipsy-5);
}

void dx_drawtext_gold(int x, int y, unsigned short int color, int amount)
{
        if (amount>99) dd_drawtext_fmt(x,y,color,DD_CENTER|DD_FRAME|DD_SMALL,"%d.%02dG",amount/100,amount%100);
        else dd_drawtext_fmt(x,y,color,DD_CENTER|DD_FRAME|DD_SMALL,"%ds",amount);
}

// display

static void display_wear(void)
{
        int b,i,x,y,yt;
	unsigned int sprite;
	unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,light,sat;
	DDFX fx;

        for (b=BUT_WEA_BEG; b<=BUT_WEA_END; b++) {

                i=b-BUT_WEA_BEG;

                x=but[b].x;
                y=but[b].y;
                yt=y+23;

		dd_copysprite(SPR_ITPAD,x,y,DDFX_NLIGHT,DD_CENTER);
                if (i==weasel) dd_copysprite(SPR_ITSEL,x,y,DDFX_NLIGHT,DD_CENTER);
                if (item[weatab[i]]) {
			
			//dd_copysprite(14,x,y,DDFX_NLIGHT,DD_CENTER);
			
			bzero(&fx,sizeof(fx));

			sprite=trans_asprite(0,item[weatab[i]],tick,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
			fx.sprite=sprite;
			fx.c1=c1;
			fx.c2=c2;
			fx.c3=c3;
			fx.cr=cr;
			fx.cg=cg;
			fx.cb=cb;
			fx.clight=light;
			fx.sat=sat;
			fx.shine=shine;
                        fx.scale=scale;
			fx.sink=0;
			fx.align=DD_CENTER;
			fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=i==weasel?FX_ITEMBRIGHT:FX_ITEMLIGHT;

			//dd_copysprite(item[weatab[i]],x,y,i==weasel?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_CENTER);
			dd_copysprite_fx(&fx,x,y);
		} //else dd_copysprite(13,x,y,DDFX_NLIGHT,DD_CENTER);

                //if (mousex>=160 && mousex<=640 && mousey>=0 && mousey<=40 && !vk_item && capbut==-1) dd_drawtext(x,yt,textcolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
                if (butsel>=BUT_WEA_BEG && butsel<=BUT_WEA_END && !vk_item && capbut==-1) dd_drawtext(x,yt,textcolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);

                if ((cflags&IF_WNRRING) && i== 0) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNRHAND) && i== 1) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNLHAND) && i== 2 && !(cflags&IF_WNTWOHANDED)) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
                if ((cflags&IF_WNTWOHANDED) && i==2) dd_drawtext(x,yt,redcolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNLRING) && i== 3) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNNECK)  && i== 4) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNHEAD)  && i== 5) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNCLOAK) && i== 6) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNBODY)  && i== 7) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNBELT)  && i== 8) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNARMS)  && i== 9) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNLEGS)  && i==10) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
		if ((cflags&IF_WNFEET)  && i==11) dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);

                if (i==2 && item[weatab[1]] && (item_flags[weatab[1]]&IF_WNTWOHANDED) ) dd_copysprite(5,x,y,DDFX_NLIGHT,DD_CENTER);

                if (con_cnt && con_type==2 && itemprice[weatab[i]]) dx_drawtext_gold(x,y+12,textcolor,itemprice[weatab[i]]);
        }
}

static void display_look(void)
{
        int b,i,x,y; //,yt;
	unsigned int sprite,tint;
	unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,light,sat;
	DDFX fx;
	extern char look_name[],look_desc[];
	extern int looksprite,lookc1,lookc2,lookc3;
	static int look_anim=4,look_step=0,look_dir=0;

	dd_copysprite(994,151,50,DDFX_NLIGHT,DD_NORMAL);

        for (b=BUT_WEA_BEG; b<=BUT_WEA_END; b++) {
                i=b-BUT_WEA_BEG;

                x=but[b].x;
                y=but[b].y+50;
                //yt=y+23;

                dd_copysprite(SPR_ITPAD,x,y,DDFX_NLIGHT,DD_CENTER);
                if (lookinv[weatab[i]]) {
			
                        bzero(&fx,sizeof(fx));

			sprite=trans_asprite(0,lookinv[weatab[i]],tick,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
			fx.sprite=sprite;
			fx.c1=c1;
			fx.c2=c2;
			fx.c3=c3;
			fx.shine=shine;
			fx.cr=cr;
			fx.cg=cg;
			fx.cb=cb;
			fx.clight=light;
			fx.sat=sat;
			fx.scale=scale;
			fx.sink=0;
			fx.align=DD_CENTER;
			fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=FX_ITEMLIGHT;
			dd_copysprite_fx(&fx,x,y);
		}
                //dd_drawtext(x,yt,whitecolor,DD_CENTER|DD_SMALL|DD_FRAME,weaname[i]);
        }
        dd_drawtext(220,100,0xffff,DD_LEFT,look_name);
        dd_drawtext_break(220,110,440,0xffff,DD_LEFT,look_desc);

	{
		int csprite,scale,cr,cg,cb,light,sat,c1,c2,c3,shine;

		bzero(&fx,sizeof(fx));
	
		csprite=trans_charno(looksprite,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
	
		fx.sprite=get_player_sprite(csprite,look_dir,look_anim,look_step,16);
		look_step++;
		if (look_step==16) {
			look_step=0;
			look_anim++;
			if (look_anim>6) {
				look_anim=4;
				look_dir+=2;
				if (look_dir>7) look_dir=0;
			}
		}
		fx.scale=scale;
		fx.shine=shine;
                fx.cr=cr;
		fx.cg=cg;
		fx.cb=cb;
		fx.clight=light;
		fx.sat=sat;
		
		if (looksprite<120) {
			fx.c1=lookc1;
			fx.c2=lookc2;
			fx.c3=lookc3;
		} else {
			fx.c1=c1;
			fx.c2=c2;
			fx.c3=c3;
		}
		fx.sink=0;
		fx.align=DD_OFFSET;
		fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=FX_ITEMLIGHT;
		dd_copysprite_fx(&fx,190,160);		
	}
}

static void display_inventory(void)
{
        int b,i,x,y,yt;
        int c;// ,fkey[4];
        static char *fstr[4]={"F1","F2","F3","F4"};
	unsigned int sprite,tint;
	unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,light,sat;
	DDFX fx;

        // fkey[0]=fkey[1]=fkey[2]=fkey[3]=0;

        for (b=BUT_INV_BEG; b<=BUT_INV_END; b++) {

                i=30+invoff*INVDX+b-BUT_INV_BEG;
                c=(i-2)%4;

                x=but[b].x;
                y=but[b].y;
                yt=y+12;

                dd_copysprite(SPR_ITPAD,x,y,DDFX_NLIGHT,DD_CENTER);
                if (i==invsel) dd_copysprite(SPR_ITSEL,x,y,DDFX_NLIGHT,DD_CENTER);
                if (item[i]) {
			
			//dd_copysprite(14,x,y,DDFX_NLIGHT,DD_CENTER);

			bzero(&fx,sizeof(fx));

			sprite=trans_asprite(0,item[i],tick,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
			fx.sprite=sprite;
			fx.shine=shine;
			fx.c1=c1;
			fx.c2=c2;
			fx.c3=c3;
			fx.cr=cr;
			fx.cg=cg;
			fx.cb=cb;
			fx.clight=light;
			fx.sat=sat;
			fx.scale=scale;
			fx.sink=0;
			fx.align=DD_CENTER;
			fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=i==invsel?FX_ITEMBRIGHT:FX_ITEMLIGHT;
                        dd_copysprite_fx(&fx,x,y);
			if ((sprite=additional_sprite(item[i],tick))!=0) {
				fx.sprite=sprite;
				dd_copysprite_fx(&fx,x,y);
			}
			
		} //else dd_copysprite(13,x,y,DDFX_NLIGHT,DD_CENTER);
                if (fkeyitem[c]==i) dd_drawtext(x,y-18,textcolor,DD_SMALL|DD_CENTER|DD_FRAME,fstr[c]);
		if (con_cnt && con_type==2 && itemprice[i]) dx_drawtext_gold(x,yt,textcolor,itemprice[i]);
        }
}

static void display_container(void)
{
        int b,i,x,y,yt;
        unsigned short int color;
	unsigned int sprite,tint;
	unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,light,sat;
	DDFX fx;

        dd_copysprite(SPR_TEXTF,dot[DOT_CON].x-20,dot[DOT_CON].y-55,DDFX_NLIGHT,DD_NORMAL);
        if (con_type==1) dd_drawtext(dot[DOT_CON].x,dot[DOT_CON].y-50+2,textcolor,DD_LEFT|DD_LARGE,con_name);
	else dd_drawtext_fmt(dot[DOT_CON].x,dot[DOT_CON].y-50+2,textcolor,DD_LEFT|DD_LARGE,"%s's Shop",con_name);

        for (b=BUT_CON_BEG; b<=BUT_CON_END; b++) {

                i=conoff*CONDX+b-BUT_CON_BEG;

                x=but[b].x;
                y=but[b].y;
                yt=y+12;

                dd_copysprite(SPR_ITPAD,x,y,DDFX_NLIGHT,DD_CENTER);
                if (i==consel/* && price[i]<=gold*/) dd_copysprite(SPR_ITSEL,x,y,DDFX_NLIGHT,DD_CENTER);
                if (i>=con_cnt) continue;
                if (container[i]) {
                        bzero(&fx,sizeof(fx));

			sprite=trans_asprite(0,container[i],tick,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
			fx.sprite=sprite;
			fx.shine=shine;
			fx.c1=c1;
			fx.c2=c2;
			fx.c3=c3;
			fx.cr=cr;
			fx.cg=cg;
			fx.cb=cb;
			fx.clight=light;
			fx.sat=sat;
			fx.scale=scale;
			fx.sink=0;
			fx.align=DD_CENTER;
			fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=i==consel?FX_ITEMBRIGHT:FX_ITEMLIGHT;
                        dd_copysprite_fx(&fx,x,y);
			//dd_copysprite(container[i],x,y,i==consel/* && price[i]<=gold*/?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_CENTER);
		} //else dd_copysprite(14,x,y,DDFX_NLIGHT,DD_CENTER);

                if (con_type==2 && price[i]) {
                        if (price[i]>gold && i!=consel) color=darkredcolor;
                        else if (price[i]>gold && i==consel) color=redcolor;
                        else if (i==consel) color=whitecolor;
                        else color=textcolor;

                        dx_drawtext_gold(x,yt,color,price[i]);
                }
        }
}

static void display_scrollbars(void)
{
        dd_copysprite(SPR_SCRBAR,but[BUT_SCL_UP].x,but[BUT_SCL_UP].y+(but[BUT_SCL_DW].y-but[BUT_SCL_UP].y)/2,DDFX_NLIGHT,DD_OFFSET);
        // dd_copysprite(34,but[BUT_SCL_UP].x,but[BUT_SCL_UP].y+4,DDFX_NLIGHT,DD_OFFSET);
        // dd_copysprite(34,but[BUT_SCL_DW].x,but[BUT_SCL_DW].y-4,DDFX_NLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRUP,but[BUT_SCL_UP].x,but[BUT_SCL_UP].y,butsel==BUT_SCL_UP?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRLT,but[BUT_SCL_TR].x,but[BUT_SCL_TR].y,butsel==BUT_SCL_TR?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRDW,but[BUT_SCL_DW].x,but[BUT_SCL_DW].y,butsel==BUT_SCL_DW?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);

        dd_copysprite(SPR_SCRBAR,but[BUT_SCR_UP].x,but[BUT_SCR_UP].y+(but[BUT_SCR_DW].y-but[BUT_SCR_UP].y)/2,DDFX_NLIGHT,DD_OFFSET);
        // dd_copysprite(32,but[BUT_SCR_UP].x,but[BUT_SCR_UP].y+4,DDFX_NLIGHT,DD_OFFSET);
        // dd_copysprite(32,but[BUT_SCR_DW].x,but[BUT_SCR_DW].y-4,DDFX_NLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRUP,but[BUT_SCR_UP].x,but[BUT_SCR_UP].y,butsel==BUT_SCR_UP?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRRT,but[BUT_SCR_TR].x,but[BUT_SCR_TR].y,butsel==BUT_SCR_TR?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);
        dd_copysprite(SPR_SCRDW,but[BUT_SCR_DW].x,but[BUT_SCR_DW].y,butsel==BUT_SCR_DW?FX_ITEMBRIGHT:FX_ITEMLIGHT,DD_OFFSET);
}

static void display_citem(void)
{
        int x,y;
	unsigned int sprite,tint;
	unsigned short c1,c2,c3,shine;
	unsigned char scale,cr,cg,cb,light,sat;
	DDFX fx;

	//dd_copysprite(50473,mousex,mousey,14,0);
	//dd_drawtext_fmt(mousex,mousey-10,whitecolor,DD_CENTER|DD_FRAME|DD_SMALL,"x=%d,y=%d",mousex,mousey);

        // trashcan
        if (vk_item) {
                x=but[BUT_JNK].x;
                y=but[BUT_JNK].y;
		dd_copysprite(25,x,y,lcmd==CMD_JUNK_ITEM?DDFX_BRIGHT:DDFX_NLIGHT,DD_CENTER);
        }

        // citem
	if (!csprite) return;

        if (capbut==-1) {
                x=mousex;
                y=mousey;
        }
        else return;

	bzero(&fx,sizeof(fx));

	sprite=trans_asprite(0,csprite,tick,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
	fx.sprite=sprite;
	fx.shine=shine;
	fx.c1=c1;
	fx.c2=c2;
	fx.c3=c3;
	fx.cr=cr;
	fx.cg=cg;
	fx.cb=cb;
	fx.clight=light;
	fx.sat=sat;
	fx.scale=scale;
	fx.sink=0;
	fx.align=DD_CENTER;
	fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=FX_ITEMLIGHT;
	dd_push_clip();
	dd_more_clip(0,0,800,600);
        dd_copysprite_fx(&fx,x,y);
	if ((sprite=additional_sprite(csprite,tick))!=0) {
		fx.sprite=sprite;
		dd_copysprite_fx(&fx,x,y);
	}

        if (cprice) dx_drawtext_gold(x,y+5+12,textcolor,cprice);
	dd_pop_clip();
}

static void display_skill(void)
{
        int b,i,x,y,yt,bsx,bex,bsy,barsize;
        char buf[256];

        for (b=BUT_SKL_BEG; b<=BUT_SKL_END; b++) {

                i=skloff+b-BUT_SKL_BEG;

                x=but[b].x;
                y=but[b].y;
                yt=y-4;
                bsx=x+10;
                bex=x+SKLWIDTH;
                bsy=y+4;

                if (i>=skltab_cnt) continue;

                if (!(but[b].flags&BUTF_NOHIT)) {
                        if (i==sklsel) dx_copysprite_emerald(x,y,4,2);
                        else dx_copysprite_emerald(x,y,4,1);
                } else if (skltab[i].button) dx_copysprite_emerald(x,y,1,0);

                if (skltab[i].v==STV_EMPTYLINE) continue;

		if (skltab[i].v==STV_JUSTAVALUE) {
                        dd_drawtext(bsx,yt,textcolor,DD_LARGE|DD_LEFT,skltab[i].name);
                        dd_drawtext_fmt(bex,yt,textcolor,DD_LARGE|DD_RIGHT,"%d",skltab[i].curr);
                        continue;
                }

                barsize=skltab[i].barsize;
                if (barsize>0) dd_rect(bsx,bsy,bsx+barsize,bsy+1,bluecolor);
                else if (barsize<0) dd_rect(bsx,bsy,bex+barsize,bsy+1,redcolor);

		switch(skltab[i].v) {
			case V_WEAPON:
			case V_SPEED:
			case V_LIGHT:
			case V_COLD:		sprintf(buf,"%d",skltab[i].curr);
						break;
                        case V_ARMOR:		sprintf(buf,"%.2f",skltab[i].curr/20.0);
						break;
			case V_MANA:		sprintf(buf,"%d/%2d/%2d",mana,skltab[i].base,skltab[i].curr);
						break;
			case V_HP:
						if (lifeshield) sprintf(buf,"%d+%d/%2d/%2d",hp,lifeshield,skltab[i].base,skltab[i].curr);
						else sprintf(buf,"%d/%2d/%2d",hp,skltab[i].base,skltab[i].curr);
						break;
			case V_ENDURANCE:	sprintf(buf,"%d/%2d/%2d",endurance,skltab[i].base,skltab[i].curr);
						break;
			default:		if (skltab[i].v>=V_PROFBASE) sprintf(buf,"%d",skltab[i].base);
						else sprintf(buf,"%2d/%2d",skltab[i].base,skltab[i].curr);
						break;
		}

                dd_drawtext(bsx,yt,textcolor,DD_LARGE|DD_LEFT,skltab[i].name);
                dd_drawtext(bex,yt,textcolor,DD_LARGE|DD_RIGHT,buf);
        }
}

static int vk_special_dec(void)
{
	int n,panic=99;

	for (n=(vk_special+max_special-1)%max_special; panic--; n=(n+max_special-1)%max_special) {
		if (!special_tab[n].req || value[0][special_tab[n].req]) {
			vk_special=n;
			return 1;
		}
	}
	return 0;
}

static int vk_special_inc(void)
{
	int n,panic=99;

	for (n=(vk_special+1)%max_special; panic--; n=(n+1)%max_special) {	
		if (!special_tab[n].req || value[0][special_tab[n].req]) {
			vk_special=n;
			return 1;
		}
	}
	return 0;
}

static void display_keys(void)
{
        int i,x,u;
        char buf[256];
        unsigned short int col;

        for (u=i=0; i<max_keytab; i++) {
                if ((keytab[i].vk_item  && !vk_item ) || (!keytab[i].vk_item  && vk_item )) continue;
                if ((keytab[i].vk_char  && !vk_char ) || (!keytab[i].vk_char  && vk_char )) continue;
                if ((keytab[i].vk_spell && !vk_spell) || (!keytab[i].vk_spell && vk_spell)) continue;

                if (keytab[i].usetime>now-300) col=bluecolor; else col=textcolor;
		
		x=10+u++*((800-20)/10);
		
		if (keytab[i].skill==-1) continue;
		if (!value[0][keytab[i].skill]) continue;

		if (keytab[i].userdef) sprintf(buf,"%c/%c %s",keytab[i].keycode,keytab[i].userdef,keytab[i].name,keytab[i].usetime);
		else sprintf(buf,"%c %s",keytab[i].keycode,keytab[i].name,keytab[i].usetime);
                dd_drawtext(x,427,col,DD_LEFT|DD_SMALL|DD_FRAME,buf);
                // dd_drawtext(5,50+u*10,col,DD_LEFT|DD_SMALL|DD_FRAME,buf);
        }

	dd_push_clip();
	dd_more_clip(0,0,800,600);

	if (now-vk_special_time<2000) {
		int n,panic=99;

		for (n=(vk_special+1)%max_special,i=-1; panic-- && i>-3; n=(n+1)%max_special) {	
			if (!special_tab[n].req || value[0][special_tab[n].req]) {
                                dd_drawtext(mousex+9,mousey-3+i*10,graycolor,DD_LEFT|DD_FRAME,special_tab[n].name);
				i--;
			}
		}
		dd_drawtext(mousex+9,mousey-3,whitecolor,DD_LEFT|DD_FRAME,special_tab[vk_special].name);		

		for (n=(vk_special+max_special-1)%max_special,i=1; panic-- && i<3; n=(n+max_special-1)%max_special) {
			if (!special_tab[n].req || value[0][special_tab[n].req]) {
                                dd_drawtext(mousex+9,mousey-3+i*10,graycolor,DD_LEFT|DD_FRAME,special_tab[n].name);
				i++;
			}
		}
	}
	dd_pop_clip();
}

#define TELE_X	100
#define TELE_Y	100

int teleporter=0;
extern int may_teleport[64+32];

static int tele[64*2]={
	133,229,	//0	Cameron
	-1,-1,		//1
	143,206,	//2	Aston
	370,191,	//3	Tribe of the Isara
	370,179,	//4	Tribe of the Cerasa
	370,167,	//5	Cerasa Maze
	370,155,	//6	Cerasa Tunnels
	370,143,	//7	Zalina Entrance
	370,131,	//8	Tribe of the Zalina
	130,123,	//9	Teufelheim
	-1,-1,		//10
	-1,-1,		//11
	458,108,	//12	Ice 8
	458,96, 	//13	Ice 7
	458,84,		//14	Ice 6
	458,72,		//15	Ice 5
	458,60,		//16	Ice 4
	225,123,	//17	Nomad Plains
	-1,-1,		//18	
	-1,-1,		//19	
	162,180,	// 20 forest
	164,167,	// 21 exkordon
	194,146,	// 22 brannington
	174,115,	// 23 grimroot
	139,149,	// 24 caligar
	205,132,	// 25 arkhata
	0,0,		
};

static int mirror_pos[26*2]={
	346,210,
	346,222,
	346,234,
	346,246,
	346,258,
	346,270,
	346,282,
	346,294,

	384,210,
	384,222,
	384,234,
	384,246,
	384,258,
	384,270,
	384,282,
	384,294,

	429,210,
	429,222,
	429,234,
	429,246,
	429,258,
	429,270,
	429,282,
	429,294,

	469,210,
	469,222
};

static int clan_offset=0;

int get_teleport(int x,int y)
{
	int n;

	if (!teleporter) return -1;	

	// map teleports
	for (n=0; n<64; n++) {
                if (!tele[n*2]) break;
		if (tele[n*2]==-1) continue;
		if (!may_teleport[n]) continue;
		
		if (abs(tele[n*2]+TELE_X-x)<8 && abs(tele[n*2+1]+TELE_Y-y)<8) return n;
		
	}

	// clan teleports
	for (n=0; n<8; n++) {
		if (!may_teleport[n+64+clan_offset]) continue;

                if (abs(TELE_X+337-x)<8 && abs(TELE_Y+24+n*12-y)<8) return n+64;
	}
        for (n=0; n<8; n++) {
		if (!may_teleport[n+64+8+clan_offset]) continue;

                if (abs(TELE_X+389-x)<8 && abs(TELE_Y+24+n*12-y)<8) return n+64+8;
	}

	// mirror selector
	for (n=0; n<26; n++) {
		if (abs(mirror_pos[n*2]+TELE_X-x)<8 && abs(mirror_pos[n*2+1]+TELE_Y-y)<8) return n+101;
	}

	if (abs(389+TELE_X-x)<8 && abs(24+8*12+TELE_Y-y)<8) return 1042;

	return -1;
}

void display_teleport(void)
{
        int n;

	if (!teleporter) return;

        if (!clan_offset) dd_copysprite(53519,TELE_X+520/2,TELE_Y+320/2,14,0);
	else dd_copysprite(53520,TELE_X+520/2,TELE_Y+320/2,14,0);

        for (n=0; n<64; n++) {
		if (!tele[n*2]) break;
		if (tele[n*2]==-1) continue;
		
		if (!may_teleport[n]) dx_copysprite_emerald(tele[n*2]+TELE_X,tele[n*2+1]+TELE_Y,2,0);
		else if (telsel==n) dx_copysprite_emerald(tele[n*2]+TELE_X,tele[n*2+1]+TELE_Y,2,2);
		else dx_copysprite_emerald(tele[n*2]+TELE_X,tele[n*2+1]+TELE_Y,2,1);
	}

	for (n=0; n<8; n++) {
		if (!may_teleport[n+64+clan_offset]) dx_copysprite_emerald(337+TELE_X,24+n*12+TELE_Y,3,0);
                else if (telsel==n+64+clan_offset) dx_copysprite_emerald(337+TELE_X,24+n*12+TELE_Y,3,2);
		else dx_copysprite_emerald(337+TELE_X,24+n*12+TELE_Y,3,1);
	}
	for (n=0; n<8; n++) {
		if (8+clan_offset+n==31) continue;
                if (!may_teleport[n+64+8+clan_offset]) dx_copysprite_emerald(389+TELE_X,24+n*12+TELE_Y,3,0);
                else if (telsel==n+64+8+clan_offset) dx_copysprite_emerald(389+TELE_X,24+n*12+TELE_Y,3,2);
		else dx_copysprite_emerald(389+TELE_X,24+n*12+TELE_Y,3,1);
	}

	for (n=0; n<26; n++) {
		if (telsel==n+101) dx_copysprite_emerald(mirror_pos[n*2]+TELE_X,mirror_pos[n*2+1]+TELE_Y,1,2);
		else if (newmirror==n+1) dx_copysprite_emerald(mirror_pos[n*2]+TELE_X,mirror_pos[n*2+1]+TELE_Y,1,1);
		else dx_copysprite_emerald(mirror_pos[n*2]+TELE_X,mirror_pos[n*2+1]+TELE_Y,1,0);
	}

	if (telsel==1042) dx_copysprite_emerald(389+TELE_X,24+8*12+TELE_Y,2,2);
	else dx_copysprite_emerald(389+TELE_X,24+8*12+TELE_Y,2,1);

}

char tutor_text[1024]={""};
int show_tutor=0;

void display_tutor(void)
{
	int x,y,n,mx=626,my=416;
	char *ptr,buf[80];

	if (!show_tutor) return;
	
        dd_rect(220,350,630,426,rgb2scr[IRGB(24,22,16)]);
	dd_line(220,350,630,350,rgb2scr[IRGB(12,10,4)]);
	dd_line(630,350,630,426,rgb2scr[IRGB(12,10,4)]);
	dd_line(220,426,360,426,rgb2scr[IRGB(12,10,4)]);
	dd_line(220,350,220,426,rgb2scr[IRGB(12,10,4)]);
	
	x=224; y=354; ptr=tutor_text;
	while (*ptr) {
		while (*ptr==' ') ptr++;
		while (*ptr=='$') {
			ptr++;
			x=224;
			y+=10;
                        if (y>=my) break;
		}
		while (*ptr==' ') ptr++;
		n=0;
                while (*ptr && *ptr!=' ' && *ptr!='$' && n<79) buf[n++]=*ptr++;
		buf[n]=0;
		if (x+dd_textlength(DD_LEFT|DD_LARGE,buf)>=mx) {
			x=224;
			y+=10;
			if (y>=my) break;			
		}
		x=dd_drawtext(x,y,rgb2scr[IRGB(12,10,4)],DD_LEFT|DD_LARGE,buf)+3;
	}
	
}

#define COLO_X	(400-120/2)
#define COLO_Y	(300-120/2)

int show_color=0,show_cur=0;
int show_color_c[3]={1,1,1};
int show_cx=0;

void display_color(void)
{
	int csprite,scale,cr,cg,cb,light,sat,c1,c2,c3,shine;
	static int col_anim=4,col_step=0,col_dir=0;	
	DDFX fx;

	if (!show_color) return;

        dd_copysprite(51082,COLO_X,COLO_Y,14,0);

	if (show_cur==0) dx_copysprite_emerald(COLO_X-38,COLO_Y+40,2,2);
	else dx_copysprite_emerald(COLO_X-38,COLO_Y+40,2,1);
	if (show_cur==1) dx_copysprite_emerald(COLO_X-38+12,COLO_Y+40,2,2);
	else dx_copysprite_emerald(COLO_X-38+12,COLO_Y+40,2,1);
	if (show_cur==2) dx_copysprite_emerald(COLO_X-38+24,COLO_Y+40,2,2);
	else dx_copysprite_emerald(COLO_X-38+24,COLO_Y+40,2,1);

	dd_copysprite(51083,COLO_X-55,COLO_Y-50+64-IGET_R(show_color_c[show_cur])*2,14,0);
	dd_copysprite(51083,COLO_X-55+20,COLO_Y-50+64-IGET_G(show_color_c[show_cur])*2,14,0);
	dd_copysprite(51083,COLO_X-55+40,COLO_Y-50+64-IGET_B(show_color_c[show_cur])*2,14,0);

        bzero(&fx,sizeof(fx));

	csprite=trans_charno(map[MAPDX*MAPDY/2].csprite,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);

	fx.sprite=get_player_sprite(csprite,col_dir,col_anim,col_step,16);
	col_step++;
	if (col_step==16) {
		col_step=0;
		col_anim++;
		if (col_anim>6) {
			col_anim=4;
			col_dir+=2;
			if (col_dir>7) col_dir=0;
		}
	}
	fx.scale=scale;
	fx.shine=shine;
	fx.cr=cr;
	fx.cg=cg;
	fx.cb=cb;
	fx.clight=light;
	fx.sat=sat;
	
        fx.c1=show_color_c[0];
	fx.c2=show_color_c[1];
	fx.c3=show_color_c[2];
	
	fx.sink=0;
	fx.align=DD_OFFSET;
	fx.ml=fx.ll=fx.rl=fx.ul=fx.dl=FX_ITEMLIGHT;
	dd_copysprite_fx(&fx,COLO_X+30,COLO_Y);
}

int get_color(int x,int y)
{
	if (!show_color) return -1;

	if (abs(x-COLO_X+38)<4 && abs(y-COLO_Y-39)<4) return 1;
	if (abs(x-COLO_X+26)<4 && abs(y-COLO_Y-39)<4) return 2;
	if (abs(x-COLO_X+14)<4 && abs(y-COLO_Y-39)<4) return 3;

	if (abs(x-COLO_X+50)<11 && abs(y-COLO_Y+18)<34) { show_cx=64-(y-COLO_Y+50); return 4; }
	if (abs(x-COLO_X+30)<11 && abs(y-COLO_Y+18)<34) { show_cx=64-(y-COLO_Y+50); return 5; }
	if (abs(x-COLO_X+10)<11 && abs(y-COLO_Y+18)<34) { show_cx=64-(y-COLO_Y+50); return 6; }

	if (abs(x-COLO_X-43)<10 && abs(y-COLO_Y-39)<5) return 7;
	if (abs(x-COLO_X-19)<10 && abs(y-COLO_Y-39)<5) return 8;

        //addline("x=%d, y=%d",x-COLO_X,y-COLO_Y);

	if (x-COLO_X<-60 || x-COLO_X>60 || y-COLO_Y<-60 || y-COLO_Y>60) return -1;
	
	return 0;
}

void cmd_color(int nr)
{
	int val;
	char buf[80];

	//addline("got %d / %d",nr,show_cx);
	
	switch(nr) {
		case 1:		show_cur=0; break;
		case 2:		show_cur=1; break;
		case 3:		show_cur=2; break;
		case 4:		val=max(min(31,show_cx/2),1);
				show_color_c[show_cur]=IRGB(
						val,
						IGET_G(show_color_c[show_cur]),
						IGET_B(show_color_c[show_cur]));
				break;
		case 5:		val=max(min(31,show_cx/2),1);
				show_color_c[show_cur]=IRGB(
					IGET_R(show_color_c[show_cur]),
					val,
					IGET_B(show_color_c[show_cur]));
                                break;
		case 6:		val=max(min(31,show_cx/2),1);
				show_color_c[show_cur]=IRGB(
					IGET_R(show_color_c[show_cur]),
					IGET_G(show_color_c[show_cur]),
					val);
				break;
		case 7:		show_color=0; break;
		case 8:		sprintf(buf,"/col1 %d %d %d",IGET_R(show_color_c[0]),IGET_G(show_color_c[0]),IGET_B(show_color_c[0]));
				cmd_text(buf);
				sprintf(buf,"/col2 %d %d %d",IGET_R(show_color_c[1]),IGET_G(show_color_c[1]),IGET_B(show_color_c[1]));
				cmd_text(buf);
				sprintf(buf,"/col3 %d %d %d",IGET_R(show_color_c[2]),IGET_G(show_color_c[2]),IGET_B(show_color_c[2]));
				cmd_text(buf);
				break;
	}
}
// date stuff
#define DAYLEN		(60*60*2)
#define HOURLEN		(DAYLEN/24)
#define MINLEN		(HOURLEN/60)

void trans_date(int t,int *phour,int *pmin)
{
        if (pmin) *pmin=(t/MINLEN)%60;
	if (phour) *phour=(t/HOURLEN)%24;
}

static char time_text[120];

static void display_screen(void)
{
        int h,m;
	int h1,h2,m1,m2;
	static int rh1=0,rh2=0,rm1=0,rm2=0;
	extern int realtime;

        //dd_copysprite(9,0,0,DDFX_NLIGHT,DD_NORMAL);
	dd_copysprite(999,0,0,DDFX_NLIGHT,DD_NORMAL);
	dd_copysprite(998,0,430,DDFX_NLIGHT,DD_NORMAL);

        trans_date(realtime,&h,&m);

	h1=h/10*3;
	h2=h%10*3;
	m1=m/10*3;
	m2=m%10*3;

        if (h1!=rh1) rh1++;
	if (rh1==30) rh1=0;

	if (h2!=rh2) rh2++;
	if (rh2==30) rh2=0;

	if (m1!=rm1) rm1++;
	if (rm1==18) rm1=0;

	if (m2!=rm2) rm2++;
	if (rm2==30) rm2=0;

        dd_copysprite(200+rh1,730+0*10-2,5+3,DDFX_NLIGHT,DD_NORMAL);
        dd_copysprite(200+rh2,730+1*10-2,5+3,DDFX_NLIGHT,DD_NORMAL);
        dd_copysprite(200+rm1,734+2*10-2,5+3,DDFX_NLIGHT,DD_NORMAL);
        dd_copysprite(200+rm2,734+3*10-2,5+3,DDFX_NLIGHT,DD_NORMAL);

	sprintf(time_text,"%02d:%02d Astonia Standard Time",h,m);
}

static void display_text(void)
{
	dd_display_text();

	if (dd_scantext(mousex,mousey,hitsel)) ; else hitsel[0]=0;
	display_cmd();
	
	/*if (dd_scantext(mousex,mousey,hitsel)) {
		dd_drawtext(230,587,redcolor,DD_LARGE|DD_LEFT,hitsel);
        } else { display_cmd(); hitsel[0]=0; } */
}

static void display_gold(void)
{
        int x,y,tg;

        x=but[BUT_GLD].x;
        y=but[BUT_GLD].y;

        dd_copysprite(SPR_GOLD_BEG+7,x,y,lcmd==CMD_TAKE_GOLD || lcmd==CMD_DROP_GOLD ?DDFX_BRIGHT:DDFX_NLIGHT,DD_CENTER);

        if (capbut==BUT_GLD) {
                dx_drawtext_gold(x,y,textcolor,takegold);
                dx_drawtext_gold(x,y+12,textcolor,gold-takegold);
        }
        else {
                dx_drawtext_gold(x,y+12,textcolor,gold);
        }
}

static void display_mode(void)
{
        static char *speedtext[3]={"NORMAL","FAST","STEALTH"};
        //static char *combattext[3]={"BALANCED","OFFENSIVE","DEFENSIVE"};
        int sel,seltxt,lg;
        unsigned short int col;

        // walk
        if (butsel>=BUT_MOD_WALK0 && butsel<=BUT_MOD_WALK2) { seltxt=butsel-BUT_MOD_WALK0; lg=2; col=seltxt==pspeed?lightbluecolor:bluecolor; }
        else { seltxt=pspeed; lg=1; col=lightbluecolor; }
        sel=pspeed;

        dx_copysprite_emerald(but[BUT_MOD_WALK0].x,but[BUT_MOD_WALK0].y,4,(sel==0?lg:0));
        dx_copysprite_emerald(but[BUT_MOD_WALK1].x,but[BUT_MOD_WALK1].y,4,(sel==1?lg:0));
        dx_copysprite_emerald(but[BUT_MOD_WALK2].x,but[BUT_MOD_WALK2].y,4,(sel==2?lg:0));
	
	dd_drawtext(but[BUT_MOD_WALK0].x,but[BUT_MOD_WALK0].y+7,bluecolor,DD_SMALL|DD_FRAME|DD_CENTER,"F6");
	dd_drawtext(but[BUT_MOD_WALK1].x,but[BUT_MOD_WALK1].y+7,bluecolor,DD_SMALL|DD_FRAME|DD_CENTER,"F5");
	dd_drawtext(but[BUT_MOD_WALK2].x,but[BUT_MOD_WALK2].y+7,bluecolor,DD_SMALL|DD_FRAME|DD_CENTER,"F7");

        if (*speedtext[sel]) dd_drawtext(but[BUT_MOD_WALK0].x,but[BUT_MOD_WALK0].y-13,col,DD_SMALL|DD_CENTER|DD_FRAME,speedtext[seltxt]);

        /*// combat
        if (butsel>=BUT_MOD_COMB0 && butsel<=BUT_MOD_COMB2) { seltxt=butsel-BUT_MOD_COMB0; lg=2; col=seltxt==pcombat?lightbluecolor:bluecolor; }
        else { seltxt=pcombat; lg=1; col=lightbluecolor; }
        sel=pcombat;

        dx_copysprite_emerald(but[BUT_MOD_COMB0].x,but[BUT_MOD_COMB0].y,4,(sel==0?lg:0));
        dx_copysprite_emerald(but[BUT_MOD_COMB1].x,but[BUT_MOD_COMB1].y,4,(sel==1?lg:0));
        dx_copysprite_emerald(but[BUT_MOD_COMB2].x,but[BUT_MOD_COMB2].y,4,(sel==2?lg:0));

        if (*combattext[sel]) dd_drawtext(but[BUT_MOD_COMB0].x,but[BUT_MOD_COMB0].y-13,col,DD_SMALL|DD_CENTER|DD_FRAME,combattext[seltxt]);*/

}

static char bless_text[120];
static char freeze_text[120];
static char potion_text[120];
static char rage_text[120];
static char level_text[120];
static char rank_text[120];

static void display_mouseover(void)
{
	if (mousey>=496 && mousey<=551) {
		if (mousex>=207 && mousex<=214) dd_drawtext(mousex,mousey-16,0xffff,DD_BIG|DD_FRAME|DD_CENTER,rage_text);
		if (mousex>=197 && mousex<=204) dd_drawtext(mousex,mousey-16,0xffff,DD_BIG|DD_FRAME|DD_CENTER,bless_text);
		if (mousex>=187 && mousex<=194) dd_drawtext(mousex,mousey-16,0xffff,DD_BIG|DD_FRAME|DD_CENTER,freeze_text);
		if (mousex>=177 && mousex<=184) dd_drawtext(mousex,mousey-16,0xffff,DD_BIG|DD_FRAME|DD_CENTER,potion_text);
	}

	if (mousex>=25 && mousex<=135) {
		if (mousey>=5 && mousey<=13) dd_drawtext(mousex+16,mousey-4,0xffff,DD_BIG|DD_FRAME,level_text);
		if (mousey>=22 && mousey<=30) dd_drawtext(mousex+16,mousey-4,0xffff,DD_BIG|DD_FRAME,rank_text);
	}

	if (mousex>=728 && mousex<=772 && mousey>=7 && mousey<=17) dd_drawtext(mousex-16,mousey-4,0xffff,DD_BIG|DD_FRAME|DD_RIGHT,time_text);
}

static void display_selfspells(void)
{
	int n,nr,cn,step;
	extern unsigned char ueffect[];

	cn=map[mapmn(MAPDX/2,MAPDY/2)].cn;
        if (!cn) return;

	sprintf(bless_text,"Bless: Not active");
	sprintf(freeze_text,"Freeze: Not active");
	sprintf(potion_text,"Potion: Not active");
	
	for (n=0; n<4; n++) {
		nr=find_cn_ceffect(cn,n);
		if (nr==-1) continue;
		
		switch(ceffect[nr].generic.type) {
			case 9:		step=50-50*(ceffect[nr].bless.stop-tick)/(ceffect[nr].bless.stop-ceffect[nr].bless.start);
					dd_push_clip();
					dd_more_clip(0,0,800,119+430);
					//if (step>=40 && (tick&4)) dd_copysprite(997,179+2*10,68+430+step,DDFX_BRIGHT,DD_NORMAL);

					if (ceffect[nr].bless.stop-tick<24*30 && (tick&4)) dd_copysprite(997,179+2*10,68+430+step,DDFX_BRIGHT,DD_NORMAL);
					else dd_copysprite(997,179+2*10,68+430+step,DDFX_NLIGHT,DD_NORMAL);
					dd_pop_clip();
					sprintf(bless_text,"Bless: %ds to go",(ceffect[nr].bless.stop-tick)/24);
                                        break;
			case 11:	step=50-50*(ceffect[nr].freeze.stop-tick)/(ceffect[nr].freeze.stop-ceffect[nr].freeze.start);
					dd_push_clip();
					dd_more_clip(0,0,800,119+430);
					dd_copysprite(997,179+1*10,68+430+step,DDFX_NLIGHT,DD_NORMAL);
					dd_pop_clip();
					sprintf(freeze_text,"Freeze: %ds to go",(ceffect[nr].freeze.stop-tick)/24);
                                        break;

			case 14:        step=50-50*(ceffect[nr].potion.stop-tick)/(ceffect[nr].potion.stop-ceffect[nr].potion.start);
					dd_push_clip();
					dd_more_clip(0,0,800,119+430);
					if (step>=40 && (tick&4)) dd_copysprite(997,179+0*10,68+430+step,DDFX_BRIGHT,DD_NORMAL);
					else dd_copysprite(997,179+0*10,68+430+step,DDFX_NLIGHT,DD_NORMAL);
					dd_pop_clip();
					sprintf(potion_text,"Potion: %ds to go",(ceffect[nr].potion.stop-tick)/24);
                                        break;
		}
	}

	/*for (nr=0; nr<64; nr++) {
		dd_drawtext_fmt(10,100+nr*7,ueffect[nr] ? 0xffff : 0x3030,DD_SMALL|DD_FRAME,"%d (%s)",
						 ceffect[nr].generic.type,
						 is_char_ceffect(ceffect[nr].generic.type) ? name[ceffect[nr].strike.cn] : "mapeffect");
	}*/
}

static void display_exp(void)
{
	int level,step,total,expe,cn,clevel,nlevel;
	static int last_exp=0,exp_ticker=0;

	sprintf(level_text,"Level: unknown");

	cn=map[MAPDX*MAPDY/2].cn;
	level=player[cn].level;

	expe=experience; //max(experience,experience_used);
	clevel=exp2level(expe);
	nlevel=level+1;
	
	step=level2exp(nlevel)-expe;
	total=level2exp(nlevel)-level2exp(clevel);
	if (step>total) step=total;	// ugh. fix for level 1 with 0 exp

	//dd_drawtext_fmt(20,20,0xffff,DD_SMALL|DD_FRAME,"Level %d",clevel);
	//dd_drawtext_fmt(110,20,0xffff,DD_SMALL|DD_FRAME,"Level %d",nlevel);

        //dd_drawtext_fmt(50,28,0xffff,DD_SMALL|DD_FRAME,"In-Game Level %d",level);

        if (total) {
                if (last_exp!=expe) {
			exp_ticker=3;
			last_exp=expe;
		}

		dd_push_clip();
		dd_more_clip(0,0,31+100-100*step/total,8+7);
                dd_copysprite(996,31,7,exp_ticker ? DDFX_BRIGHT : DDFX_NLIGHT,DD_NORMAL);
                dd_pop_clip();

                if (exp_ticker) exp_ticker--;

		sprintf(level_text,"Level: From %d to %d",clevel,nlevel);
	}
}

static char *rankname[]={
        "nobody",               //0
        "Private",              //1
        "Private First Class",  //2
        "Lance Corporal",       //3
        "Corporal",             //4
        "Sergeant",             //5     lvl 30
        "Staff Sergeant",       //6
        "Master Sergeant",      //7
        "First Sergeant",       //8     lvl 45
        "Sergeant Major",       //9
        "Second Lieutenant",    //10    lvl 55
        "First Lieutenant",     //11
        "Captain",              //12
        "Major",                //13
        "Lieutenant Colonel",   //14
        "Colonel",              //15
        "Brigadier General",    //16
        "Major General",        //17
        "Lieutenant General",   //18
        "General",              //19
        "Field Marshal",        //20    lvl 105
        "Knight of Astonia",    //21
        "Baron of Astonia",     //22
        "Earl of Astonia",      //23
        "Warlord of Astonia"    //24    lvl 125
};

static int mil_rank(int exp)
{
	int n;

	for (n=1; n<50; n++) {
		if (exp<n*n*n) return n-1;
	}
	return 99;
}

static void display_military(void)
{
	int step,total,rank,cost1,cost2;
	extern int mil_exp;

	sprintf(rank_text,"Rank: none or unknown");

        rank=mil_rank(mil_exp);
	cost1=rank*rank*rank;
	cost2=(rank+1)*(rank+1)*(rank+1);

	total=cost2-cost1;
	step=mil_exp-cost1;
	if (step>total) step=total;

	//addline("total=%d, step=%d, cost1=%d, cost2=%d (%d)",total,step,cost1,cost2,mil_exp);

        if (mil_exp && total) {
		if (rank<24) {
			dd_push_clip();
			dd_more_clip(0,0,31+100*step/total,8+24);
			dd_copysprite(993,31,24,DDFX_NLIGHT,DD_NORMAL);
			dd_pop_clip();
	
			sprintf(rank_text,"Rank: '%s' to '%s'",rankname[rank],rankname[rank+1]);
		} else sprintf(rank_text,"Rank: Warlord of Astonia");
	}
}

static void display_rage(void)
{
	int step;

	sprintf(rage_text,"Rage: Not active");

	if (!value[0][V_RAGE] || !rage) return;
	
	step=50-50*rage/value[0][V_RAGE];
	dd_push_clip();
	dd_more_clip(0,0,800,119+430);
	dd_copysprite(997,179+3*10,68+430+step,DDFX_NLIGHT,DD_NORMAL);
	dd_pop_clip();	

	sprintf(rage_text,"Rage: %d%%",100*rage/value[0][V_RAGE]);
}

void display_game_special(void)
{
	extern int display_gfx,display_time;
	int dx;

	if (!display_gfx) return;
	
        switch(display_gfx) {
		case 1:		dd_copysprite(50473,343,540,14,0); break;
		case 2:		dd_copysprite(50473,423,167,14,0); break;
		case 3:		dx=(tick-display_time)*450/120;
				if (dx<450) dd_copysprite(50475,175+dx,60,14,0);
				break;
		case 4:		dd_copysprite(50475,218,60,14,0); break;
		case 5:		dd_copysprite(50475,257,60,14,0); break;
		case 6:		dd_copysprite(50475,23,45,14,0); break;
		case 7:		dd_copysprite(50475,75,47,14,0); break;
		case 8:		dd_copysprite(50475,763,62,14,0); break;
		
		case 9:		dx=(tick-display_time)*150/120;
				if (dx<150) dd_copysprite(50474,188,447+dx,14,0);
				break;

		case 10:	dd_copysprite(50474,205,459,14,0); break;

		case 11:	dx=(tick-display_time)*150/120;
				if (dx<150) dd_copysprite(50476,200,440+dx,14,0);
				break;

		case 12:	dx=(tick-display_time)*150/120;
				if (dx<150) dd_copysprite(50476,618,445+dx,14,0);
				break;

		case 13:	dd_copysprite(50476,625,456,14,0); break;
		case 14:	dd_copysprite(50476,700,456,14,0); break;
		case 15:	dd_copysprite(50476,741,456,14,0); break;
		
		case 16:	dd_copysprite(50476,353,203,14,0); break;

		case 17:	dd_copysprite(50473,722,382,14,0); dd_copysprite(50475,257,60,14,0); break;

		default:	dd_copysprite(display_gfx,550,250,14,0); break;
	}
}

static int start_time=0;
char perf_text[256];

static void display(void)
{
	extern int sc_hit,sc_miss,sc_maxstep,vc_hit,vc_miss,vc_unique,vc_unique24,sc_blits,sm_cnt;
	extern int server_cycles,rec_bytes,sent_bytes;
	extern int fsprite_cnt,f2sprite_cnt,gsprite_cnt,g2sprite_cnt,isprite_cnt,csprite_cnt,q_size,vc_cnt,sc_cnt,ap_cnt,np_cnt,tp_cnt,sc_time,vm_cnt,vm_time,qs_time,bless_time,dg_time,ds_time,vi_time,im_time;
	extern int textdisplayline,textnextline;
	int tt_time=0,mis_time;
	double trans;
	static int unique24=0,lastuni=0;
	extern int vc_time,ap_time,tp_time;	//,ol_time,sb_time;
	static double vc_avg=0,ap_avg=0,tp_avg=0,sc_avg=0,tt_avg=0,vm_avg=0,qs_avg=0,bt_avg=0,mis_avg=0,dg_avg=0,ds_avg=0,vi_avg=0,im_avg;
	static int cnt=1;
	extern int socktimeout,kicked_out;
	int current_time,t,start;
	extern int mirror;
	extern int memptrs[MAX_MEM];
	extern int memsize[MAX_MEM];
	extern int memused;
	extern int memptrused;

        start=GetTickCount();
        if (sockstate<4 && ((t=time(NULL)-socktimeout)>10 || !originx)) {
                dd_rect(0,0,800,600,blackcolor);
                display_screen();
                display_text();
                if ((now/1000)&1) dd_drawtext(800/2,600/2-60,redcolor,DD_CENTER|DD_LARGE,"not connected");
                dd_copysprite(60,800/2,(600-240)/2,DDFX_NLIGHT,DD_CENTER);
                dd_copysprite(1,mousex,mousey+5,DDFX_BRIGHT,DD_CENTER);
                if (!kicked_out) {
			dd_drawtext_fmt(800/2,600/2-40,textcolor,DD_SMALL|DD_CENTER|DD_FRAME,"Trying to establish connection. %d seconds...",t);
			if (t>15) {
				dd_drawtext_fmt(800/2,600/2-20,textcolor,DD_LARGE|DD_CENTER|DD_FRAME,"If you have connection problems, please try a different connection in the lower left of the client startup screen.");
				dd_drawtext_fmt(800/2,600/2- 0,textcolor,DD_LARGE|DD_CENTER|DD_FRAME,"Additional information can be found at www.astonia.com.");
			}
		}
                return;
        }

        dd_push_clip();
	dd_more_clip(dot[DOT_MTL].x,dot[DOT_MTL].y,dot[DOT_MBR].x,dot[DOT_MBR].y);
        display_game();
        dd_pop_clip();

        display_screen();

        display_keys();
	if (show_look) display_look();
	display_wear();
	display_inventory();
	if (con_cnt) display_container(); else display_skill();
	display_scrollbars();
	display_text();
	display_gold();
	display_mode();
        display_selfspells();
        display_exp();
	display_military();
        display_teleport();
        display_color();
        display_rage();
        display_game_special();
        display_tutor();
        display_citem();		

/*#ifdef DOSOUND
	if (display_vc) sound_display();
#endif*/

        if (display_vc) {
		dd_drawtext_fmt(650,5,0xffff,DD_SMALL|DD_FRAME,"Mirror %d",mirror);
		dd_drawtext_fmt(650,15,0xffff,DD_SMALL|DD_LEFT|DD_FRAME,"skip=%3.0f%%",100.0*skip/tota);
		dd_drawtext_fmt(650,25,0xffff,DD_SMALL|DD_LEFT|DD_FRAME,"idle=%3.0f%%",100.0*idle/tota);
	} else dd_drawtext_fmt(650,15,0xffff,DD_SMALL|DD_FRAME,"Mirror %d",mirror);

	//sprintf(perf_text,"idle=%03d%%, skip=%03d%%, vc=%3d/%.2fms, sb=%d, vm=%3d/%.2fms, sc=%3d sm=%.2fms, ap=%3d/%.2fms, np=%4d, tp=%4d/%.2fms, qs=%.2fms, bt=%.2fms, dg=%.2fms, ds=%.2fms, vi=%.2fms, im=%.2fms, tt=%.2fms",100*idle/tota,100*skip/tota,vc_cnt,vc_avg,sc_blits,vm_cnt,vm_avg,sc_cnt,sc_avg,ap_cnt,ap_avg,np_cnt,tp_cnt,tp_avg,qs_avg,bt_avg,dg_avg,ds_avg,vi_avg,im_avg,tt_avg);
	sprintf(perf_text,"mem usage=%.2f/%.2fMB, %.2f/%.2fKBlocks",
		memsize[0]/1024.0/1024.0,memused/1024.0/1024.0,
		memptrs[0]/1024.0,memptrused/1024.0);
	//if (display_vc) dd_drawtext_fmt(2,62,whitecolor,DD_SMALL|DD_LEFT|DD_FRAME,perf_text);
	
	tt_time+=GetTickCount()-start;


#ifdef DEVELOPER
        /*if (!start_time) start_time=GetTickCount();
	current_time=GetTickCount();

	if (start_time==current_time) trans=42;
	else trans=rec_bytes/(double)(current_time-start_time);

        if (idle>skip) dd_drawtext_fmt(2,52,whitecolor,DD_SMALL|DD_LEFT|DD_FRAME,"idle=%2.0f%% trans=%.2fK/s vc=%2.2fms, vm=%.2fms, sm=%d/%.2fms",100.0*idle/tota,trans,vc_avg,vm_avg,sm_cnt,sc_avg);
	else dd_drawtext_fmt(2,52,whitecolor,DD_SMALL|DD_LEFT|DD_FRAME,"skip=%2.0f%% trans=%.2fK/s vc=%2.2fms, vm=%.2fms, sm=%d/%.2fms",100.0*skip/tota,trans,vc_avg,vm_avg,sm_cnt,sc_avg);
	
        dd_drawtext_fmt(2,62,whitecolor,DD_SMALL|DD_LEFT|DD_FRAME,"vc=%3d/%.2fms, sb=%d, vm=%3d/%.2fms, sc=%3d sm=%.2fms, ap=%3d/%.2fms, np=%4d, tp=%4d/%.2fms, qs=%.2fms, bt=%.2fms, dg=%.2fms, ds=%.2fms, vi=%.2fms, im=%.2fms, tt=%.2fms, mis=%.2fms vc_miss=%d",vc_cnt,vc_avg,sc_blits,vm_cnt,vm_avg,sc_cnt,sc_avg,ap_cnt,ap_avg,np_cnt,tp_cnt,tp_avg,qs_avg,bt_avg,dg_avg,ds_avg,vi_avg,im_avg,tt_avg,mis_avg,vc_miss); //,pre_done,bltdone);

        mis_time=tt_time-vc_time-vm_time-sc_time-ap_time-tp_time-qs_time-bless_time-dg_time-ds_time-vi_time;*/
#endif

	sc_hit=sc_miss=sc_maxstep=vc_hit=vc_miss=vc_unique=sc_blits=sm_cnt=0;
	vc_cnt=sc_cnt=ap_cnt=np_cnt=tp_cnt=vm_cnt=0;
	if (cnt<99) {
		vc_avg=(vc_avg*(100.0-100/cnt)+vc_time*(100.0/cnt))/100.0;
		vm_avg=(vm_avg*(100.0-100/cnt)+vm_time*(100.0/cnt))/100.0;
		sc_avg=(sc_avg*(100.0-100/cnt)+sc_time*(100.0/cnt))/100.0;
		ap_avg=(ap_avg*(100.0-100/cnt)+ap_time*(100.0/cnt))/100.0;
		tp_avg=(tp_avg*(100.0-100/cnt)+tp_time*(100.0/cnt))/100.0;
		tt_avg=(tt_avg*(100.0-100/cnt)+tt_time*(100.0/cnt))/100.0;
		qs_avg=(qs_avg*(100.0-100/cnt)+qs_time*(100.0/cnt))/100.0;
		bt_avg=(bt_avg*(100.0-100/cnt)+bless_time*(100.0/cnt))/100.0;
		mis_avg=(mis_avg*(100.0-100/cnt)+mis_time*(100.0/cnt))/100.0;
		dg_avg=(dg_avg*(100.0-100/cnt)+dg_time*(100.0/cnt))/100.0;
		ds_avg=(ds_avg*(100.0-100/cnt)+ds_time*(100.0/cnt))/100.0;
		vi_avg=(vi_avg*(100.0-100/cnt)+vi_time*(100.0/cnt))/100.0;
		im_avg=(im_avg*(100.0-100/cnt)+im_time*(100.0/cnt))/100.0;
		cnt++;
	} else {
		vc_avg=vc_avg*0.99+vc_time*0.01;
		vm_avg=vm_avg*0.99+vm_time*0.01;
		sc_avg=sc_avg*0.99+sc_time*0.01;
		ap_avg=ap_avg*0.99+ap_time*0.01;
		tp_avg=tp_avg*0.99+tp_time*0.01;
		tt_avg=tt_avg*0.99+tt_time*0.01;
		qs_avg=qs_avg*0.99+qs_time*0.01;
		bt_avg=bt_avg*0.99+bless_time*0.01;
		mis_avg=mis_avg*0.99+mis_time*0.01;
		dg_avg=dg_avg*0.99+dg_time*0.01;
		ds_avg=ds_avg*0.99+ds_time*0.01;
		vi_avg=vi_avg*0.99+vi_time*0.01;
		im_avg=im_avg*0.99+im_time*0.01;
	}
	/*if (tt_time>100) note("tt_time=%d",tt_time);
	if (vm_time>20) note("vm_time=%d",vm_time);
	if (vc_time>20) note("vc_time=%d",vc_time);
	if (sc_time>20) note("sc_time=%d",sc_time);
	if (ap_time>20) note("ap_time=%d",ap_time);
	if (tp_time>20) note("tp_time=%d",tp_time);
        if (qs_time>20) note("qs_time=%d",qs_time);
	if (bless_time>20) note("bless_time=%d",bless_time);
	if (ds_time>20) note("ds_time=%d",ds_time);
	if (vi_time>20) note("vi_time=%d",vi_time);
	if (im_time>20) note("im_time=%d",im_time);*/

	vm_time=vc_time=sc_time=ap_time=tp_time=tt_time=qs_time=bless_time=dg_time=ds_time=vi_time=im_time=0; //sb_time=ol_time=0;
	if (tick/24!=lastuni/24) { unique24=vc_unique24; vc_unique24=0; lastuni=tick; }
	fsprite_cnt=f2sprite_cnt=gsprite_cnt=g2sprite_cnt=isprite_cnt=csprite_cnt=0;		

	display_mouseover();
}

// cmd

static void set_cmd_cursor(int cmd)
{
        HCURSOR cursor;

        // cursor
        switch (cmd) {
                case CMD_MAP_MOVE:      cursor=c_only; break;
                case CMD_MAP_DROP:      cursor=c_drop; break;

                case CMD_ITM_TAKE:      cursor=c_take; break;
                case CMD_ITM_USE:       cursor=c_use; break;
                case CMD_ITM_USE_WITH:  cursor=c_usewith; break;

                case CMD_CHR_ATTACK:    cursor=c_attack; break;
                case CMD_CHR_GIVE:      cursor=c_give; break;

                case CMD_INV_USE:       cursor=c_use; break;
                case CMD_INV_USE_WITH:  cursor=c_usewith; break;
                case CMD_INV_TAKE:      cursor=c_take; break;
                case CMD_INV_SWAP:      cursor=c_swap; break;
                case CMD_INV_DROP:      cursor=c_drop; break;

                case CMD_WEA_USE:       cursor=c_use; break;
                case CMD_WEA_USE_WITH:  cursor=c_usewith; break;
                case CMD_WEA_TAKE:      cursor=c_take; break;
                case CMD_WEA_SWAP:      cursor=c_swap; break;
                case CMD_WEA_DROP:      cursor=c_drop; break;

		case CMD_CON_TAKE:      cursor=c_take; break;
		case CMD_CON_FASTTAKE:	cursor=c_take; break;	// needs different cursor!!!

		case CMD_CON_BUY:       cursor=c_buy; break;
		case CMD_CON_FASTBUY:   cursor=c_buy; break;	// needs different cursor!!!

                case CMD_CON_SWAP:      cursor=c_swap; break;
                case CMD_CON_DROP:      cursor=c_drop; break;
		case CMD_CON_SELL:      cursor=c_sell; break;
		case CMD_CON_FASTSELL:  cursor=c_sell; break;	// needs different cursor!!!
		case CMD_CON_FASTDROP:  cursor=c_drop; break;	// needs different cursor!!!

                case CMD_MAP_LOOK:      cursor=c_look; break;
                case CMD_ITM_LOOK:      cursor=c_look; break;
                case CMD_CHR_LOOK:      cursor=c_look; break;
                case CMD_INV_LOOK:      cursor=c_look; break;
                case CMD_WEA_LOOK:      cursor=c_look; break;
                case CMD_CON_LOOK:      cursor=c_look; break;

                case CMD_MAP_CAST_L:    cursor=c_spell; break;
                case CMD_ITM_CAST_L:    cursor=c_spell; break;
                case CMD_CHR_CAST_L:    cursor=c_spell; break;
                case CMD_MAP_CAST_R:    cursor=c_spell; break;
                case CMD_ITM_CAST_R:    cursor=c_spell; break;
                case CMD_CHR_CAST_R:    cursor=c_spell; break;
                case CMD_SPL_SET_L:     cursor=c_set; break;
                case CMD_SPL_SET_R:     cursor=c_set; break;

                case CMD_SKL_RAISE:     cursor=c_raise; break;

                case CMD_SAY_HITSEL:    cursor=c_say; break;

                case CMD_DROP_GOLD:     cursor=c_drop; break;
                case CMD_TAKE_GOLD:     cursor=c_take; break;

                case CMD_JUNK_ITEM:     cursor=c_junk; break;


                case CMD_SPEED0:
                case CMD_SPEED1:
                case CMD_SPEED2:
                case CMD_COMBAT0:
                case CMD_COMBAT1:
		case CMD_COMBAT2:       cursor=c_set; break;

		case CMD_TELEPORT:	cursor=c_take; break;

		case CMD_HELP_NEXT:
		case CMD_HELP_PREV:	
		case CMD_HELP_CLOSE:	cursor=c_use; break;

		case CMD_HELP_MISC:	if (helpsel!=-1) cursor=c_use;
					else if (questsel!=-1) cursor=c_use;
					else cursor=c_only;
					break;

		case CMD_HELP:		cursor=c_use; break;
		case CMD_QUEST:		cursor=c_use; break;
		case CMD_EXIT:		cursor=c_use; break;
		case CMD_NOLOOK:	cursor=c_use; break;

		case CMD_COLOR:		cursor=c_use; break;

                default:                cursor=c_only; break;
        }

        if (cur_cursor!=cursor) {
                SetCursor(cursor);
                cur_cursor=cursor;
        }
}

void set_cmd_key_states(void)
{
        POINT p;
	extern int x_offset,y_offset;

        vk_shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)|shift_override;
        vk_control=GetAsyncKeyState(VK_CONTROL)&0x8000|control_override;
        vk_alt=GetAsyncKeyState(VK_MENU)&0x8000;
        //vk_lbut=GetAsyncKeyState(VK_LBUTTON)&0x8000;
        //vk_rbut=GetAsyncKeyState(VK_RBUTTON)&0x8000;

        vk_char=vk_control;
        vk_item=vk_shift;
        vk_spell=vk_alt;

        GetCursorPos(&p);
        if (dd_windowed) ScreenToClient(mainwnd,&p);
        mousex=p.x-x_offset;
        mousey=p.y-y_offset;
}

static int get_near_ground(int x, int y)
{
        int mapx,mapy;
        extern int display_help,display_quest;

	if (display_help || display_quest) stom(x-110,y,&mapx,&mapy);
        else stom(x,y,&mapx,&mapy);

        if (mapx<0 || mapy<0 || mapx>=MAPDX || mapy>=MAPDY) return -1;

        return mapmn(mapx,mapy);
}

static int get_near_item(int x, int y, int flag, int small)
{
        int mapx,mapy,sx,sy,ex,ey,mn,scrx,scry,nearest=-1,look;
        double dist,nearestdist=100000000;
	extern int display_help,display_quest;

	if (display_help || display_quest) stom(mousex-110,mousey,&mapx,&mapy);
        else stom(mousex,mousey,&mapx,&mapy);

        if (small) look=0; else look=MAPDX;

        sx=max(0,mapx-look);
        sy=max(0,mapy-look);;
        ex=min(MAPDX-1,mapx+look);
        ey=min(MAPDY-1,mapy+look);

        for (mapy=sy; mapy<=ey; mapy++) {
                for (mapx=sx; mapx<=ex; mapx++) {

                        mn=mapmn(mapx,mapy);

                        if (!(map[mn].rlight)) continue;
                        if (!(map[mn].flags&flag)) continue;
                        if (!(map[mn].isprite)) continue;

                        mtos(mapx,mapy,&scrx,&scry);
			if (display_help || display_quest) scrx+=110;

                        dist=(x-scrx)*(x-scrx)+(y-scry)*(y-scry);

                        if (dist<nearestdist) {
                                nearestdist=dist;
                                nearest=mn;
                        }
                }
        }

        return nearest;
}

static int get_near_char(int x, int y)
{
        int mapx,mapy,sx,sy,ex,ey,mn,scrx,scry,nearest=-1,look;
        double dist,nearestdist=100000000;
	extern int display_help,display_quest;

	if (display_help || display_quest) stom(mousex-110,mousey,&mapx,&mapy);
        else stom(mousex,mousey,&mapx,&mapy);

        look=MAPDX;

        sx=max(0,mapx-look);
        sy=max(0,mapy-look);;
        ex=min(MAPDX-1,mapx+look);
        ey=min(MAPDY-1,mapy+look);

        for (mapy=sy; mapy<=ey; mapy++) {
                for (mapx=sx; mapx<=ex; mapx++) {

                        mn=mapmn(mapx,mapy);

                        if (!(map[mn].rlight)) continue;
                        if (!(map[mn].csprite)) continue;

                        mtos(mapx,mapy,&scrx,&scry);
			if (display_help || display_quest) scrx+=110;

                        dist=(x-scrx)*(x-scrx)+(y-scry)*(y-scry);

                        if (dist<nearestdist) {
                                nearestdist=dist;
                                nearest=mn;
                        }
                }
        }

        return nearest;
}

static int get_near_button(int x, int y)
{
        int b;
        int n=-1,ndist=1000000,dist;
        int scrx,scry,mapx,mapy;

        if (x<0 || y<0 || x>=XRES || y>=YRES) return -1;

        for (b=0; b<MAX_BUT; b++) {

                if (but[b].flags&BUTF_NOHIT) continue;

                dist=(but[b].x-x)*(but[b].x-x)+(but[b].y-y)*(but[b].y-y);
                if (dist>but[b].sqhitrad) continue;

                if (dist>ndist) continue;

                ndist=dist;
                n=b;
        }

        return n;
}

static void set_invoff(int bymouse, int ny)
{
        if (bymouse) {
                // invoff=(ny-(but[BUT_SCR_UP].y+10))*max_invoff/(but[BUT_SCR_DW].y-but[BUT_SCR_UP].y-20); // DIVISION BY ZERO can't happen
                invoff+=mousedy/LINEHEIGHT;
                mousedy=mousedy%LINEHEIGHT;
        }
        else invoff=ny;

        if (invoff<0) invoff=0;
        if (invoff>max_invoff) invoff=max_invoff;

        but[BUT_SCR_TR].y=but[BUT_SCR_UP].y+10+(but[BUT_SCR_DW].y-but[BUT_SCR_UP].y-20)*invoff/max(1,max_invoff);
}

static void set_skloff(int bymouse, int ny)
{
        if (bymouse) {
                // skloff=(ny-(but[BUT_SCL_UP].y+10))*max_skloff/(but[BUT_SCL_DW].y-but[BUT_SCL_UP].y-20); // DIVISION BY ZERO can't happen
                skloff+=mousedy/LINEHEIGHT;
                mousedy=mousedy%LINEHEIGHT;
        }
        else skloff=ny;

        if (skloff<0) skloff=0;
        if (skloff>max_skloff) skloff=max_skloff;

        if (!con_cnt) but[BUT_SCL_TR].y=but[BUT_SCL_UP].y+10+(but[BUT_SCL_DW].y-but[BUT_SCL_UP].y-20)*skloff/max(1,max_skloff);
}

static void set_conoff(int bymouse, int ny)
{
        if (bymouse) {
                // conoff=(ny-(but[BUT_SCL_UP].y+10))*max_conoff/(but[BUT_SCL_DW].y-but[BUT_SCL_UP].y-20); // DIVISION BY ZERO can't happen
                conoff+=mousedy/LINEHEIGHT;
                mousedy=mousedy%LINEHEIGHT;
        }
        else conoff=ny;

        if (conoff<0) conoff=0;
        if (conoff>max_conoff) conoff=max_conoff;

        if (con_cnt) but[BUT_SCL_TR].y=but[BUT_SCL_UP].y+10+(but[BUT_SCL_DW].y-but[BUT_SCL_UP].y-20)*conoff/max(1,max_conoff);
}

static void set_skltab(void)
{
        int i,use,flag,n;
        int experience_left,raisecost;
	static int itab[V_MAX+1]={
		-1,
		0,1,2,				// powers
                3,4,5,6,			// bases
		7,8,9,10,38,41,			// armor etc
		12,13,14,15,16,40,		// fight skills
		17,18,19,20,21,22,23,24,	// 2ndary fight skills
		28,29,30,31,32,33,34,11,39,	// spells
		25,26,27,35,36,37,		// misc skills		
		42,
		43,44,45,46,47,48,49,50,51,52,
		53,54,55,56,57,58,59,60,61,62
	};

        experience_left=experience-experience_used;

        //for (flag=use=0,i=-1; i<V_MAX; i++) {
	for (flag=use=0,n=0; n<=V_MAX; n++) {

		i=itab[n];

                if (flag && (i==0 || i==3 || i==7 || i==12 || i==17 || i==25 || i==28 || i==42 || i==43)) {

                        if (use==skltab_max) skltab=xrealloc(skltab,(skltab_max+=8)*sizeof(SKLTAB),MEM_GUI);

                        bzero(&skltab[use],sizeof(SKLTAB));
                        skltab[use].v=STV_EMPTYLINE;

                        use++;
                        flag=0;
                }

                if (i==-1) {
                        // negative exp

                        if (experience_left>=0) continue;

                        if (use==skltab_max) skltab=xrealloc(skltab,(skltab_max+=8)*sizeof(SKLTAB),MEM_GUI);

                        strcpy(skltab[use].name,"Negative experience");
                        skltab[use].v=STV_JUSTAVALUE;
                        skltab[use].curr=(int)(-1000.0*experience_left/max(1,experience_used));
			skltab[use].button=0;

                        use++;
                        flag=1;

                }
                else if (value[0][i] || value[1][i] || i==V_WEAPON || i==V_ARMOR || i==V_SPEED || i==V_LIGHT) {

                        if (use==skltab_max) skltab=xrealloc(skltab,(skltab_max+=8)*sizeof(SKLTAB),MEM_GUI);

                        if (value[1][i] && i!=V_DEMON && i!=V_COLD && i<V_PROFBASE) skltab[use].button=1;
			else skltab[use].button=0;
			
			skltab[use].v=i;

			strcpy(skltab[use].name,skill[i].name);
                        skltab[use].base=value[1][i];
                        skltab[use].curr=value[0][i];
                        skltab[use].raisecost=raisecost=raise_cost(i,value[1][i]);

			if (i==V_WEAPON || i==V_ARMOR || i==V_SPEED || i==V_LIGHT || i==V_DEMON || i==V_COLD || i>=V_PROFBASE) {
				skltab[use].barsize=0;
			} else if (experience_left>=0) {
                                if (raisecost>0 && experience_left>=raisecost) skltab[use].barsize=max(1,raisecost*(SKLWIDTH-10)/experience_left);
                                else if (experience_left>=0 && raisecost>0) skltab[use].barsize=-experience_left*(SKLWIDTH-10)/raisecost;
                                else skltab[use].barsize=0;
                        } else skltab[use].barsize=0;

                        use++;
                        flag=1;
                }
        }

	skltab_cnt=use;
        max_skloff=max(0,skltab_cnt-SKLDY);

        set_skloff(0,skloff);
}

static void set_button_flags(void)
{
        int b,i;

        if (con_cnt) {
                for (b=BUT_CON_BEG; b<=BUT_CON_END; b++) but[b].flags&=~BUTF_NOHIT;
                for (b=BUT_SKL_BEG; b<=BUT_SKL_END; b++) but[b].flags|=BUTF_NOHIT;
        }
        else {
                for (b=BUT_SKL_BEG; b<=BUT_SKL_END; b++) {
                        i=skloff+b-BUT_SKL_BEG;
                        if (i>=skltab_cnt || !skltab[i].button || skltab[i].barsize<=0) but[b].flags|=BUTF_NOHIT;
                        else but[b].flags&=~BUTF_NOHIT;
                }
        }

}

static int is_fkey_use_item(int i)
{
	switch(item[i]) {
		case 10290:	
		case 10294:
		case 10298:
		case 10302:
		case 10000:
		case 50204:
		case 50205:
		case 50206:
		case 50207:
		case 50208:
		case 50209:
		case 50211:
		case 50212:	return 0;
		default:	return item_flags[i]&IF_USE;
	}	
}

static int get_skl_look(int x,int y)
{
	int b,i;
	for (b=BUT_SKL_BEG; b<=BUT_SKL_END; b++) {
		i=skloff+b-BUT_SKL_BEG;
		if (i>=skltab_cnt) continue;
		//addline("i=%d, b=%d, x=%d (%d,%d), y=%d (%d,%d)",i,b,x,but[b].x+10,but[b].x+60,y,but[b].y,but[b].y+10);
		if (x>but[b].x-5 && x<but[b].x+70 && y>but[b].y-5 && y<but[b].y+5) return skltab[i].v;
	}
	return -1;
}

static void cmd_look_skill(int nr)
{
	if (nr>=0 && nr<=V_MAX) {
		addline("%s: %s",skill[nr].name,skilldesc[nr]);
	} else addline("Unknown.");
}

static void set_cmd_states(void)
{
        int b,i,c;
        static int oldconcnt=0; // ;-)
	extern int display_help,display_quest;
	static char title[256];
	char buf[256];

        set_cmd_key_states();
        set_map_values(map,tick);
	set_mapadd(-map[mapmn(MAPDX/2,MAPDY/2)].xadd,-map[mapmn(MAPDX/2,MAPDY/2)].yadd);

        // update
        if (update_skltab) { set_skltab(); update_skltab=0; }
        if (oldconcnt!=con_cnt) {
                conoff=0;
                max_conoff=(con_cnt/CONDX)-CONDY;
		oldconcnt=con_cnt;
                set_conoff(0,conoff);
                set_skloff(0,skloff);
        }
        max_invoff=((INVENTORYSIZE-30)/INVDX)-INVDY;
        set_button_flags();

        plrmn=mapmn(MAPDX/2,MAPDY/2);

        sprintf(buf,"%s - Astonia 3 v%d.%d.%d - (%u.%u.%u.%u:%u)",
		(map[plrmn].cn && player[map[plrmn].cn].name[0]) ? player[map[plrmn].cn].name : "Someone",
		(VERSION>>16)&255,(VERSION>>8)&255,(VERSION)&255,
		(target_server>>24)&255,
		(target_server>>16)&255,
		(target_server>>8)&255,
		(target_server>>0)&255,
		target_port);
	if (strcmp(title,buf)) {
		SetWindowText(mainwnd,buf);
		strcpy(title,buf);
	}

	// update fkeyitem
	fkeyitem[0]=fkeyitem[1]=fkeyitem[2]=fkeyitem[3]=0;
	for (i=30; i<30+INVENTORYSIZE; i++) {
		c=(i-2)%4;
		if (fkeyitem[c]==0 && (is_fkey_use_item(i))) fkeyitem[c]=i;
	}

        // a button captured - we leave all as is was (i know it's hard to update before, but i have to for the scrollbars)
        if (capbut!=-1) {
                // some very simple stuff is right here
                if (capbut==BUT_GLD) {
                        takegold+=(mousedy/2)*(mousedy/2) * (mousedy<=0?1:-1);

                        if (takegold<0) takegold=0;
                        if (takegold>gold) takegold=gold;

                        mousedy=0;
                }
                return;
        }

        // reset
        butsel=mapsel=itmsel=chrsel=invsel=weasel=consel=splsel=sklsel=telsel=helpsel=colsel=skl_look_sel=questsel=-1;

	// hit teleport?
	telsel=get_teleport(mousex,mousey);
	if (telsel!=-1) butsel=BUT_TEL;

	colsel=get_color(mousex,mousey);
	if (colsel!=-1) butsel=BUT_COLOR;

        if ((display_help || display_quest) && butsel==-1) {
		if (mousex>=0 && mousex<=222 && mousey>=0+40 && mousey<=394+40) {
			butsel=BUT_HELP_MISC;

			if (display_help==1 && mousex>=7 && mousex<=136 && mousey>=234 && mousey<=234+MAXHELP*10) {	// 312
				helpsel=(mousey-234)/10+2;
				if (mousex>110) helpsel+=12;
				
				if (helpsel<2 || helpsel>MAXHELP) helpsel=-1;				
			}

			if (display_quest && mousex>=165 && mousex<=199) {
				int tmp,y;

				tmp=(mousey-55)/40;
				y=tmp*40+55;
				if (tmp>=0 && tmp<=8 && mousey>=y && mousey<=y+10) {
					questsel=tmp;
				}
			}
		}
		if (mousex>=177 && mousex<=196 && mousey>=378+40 && mousey<=385+40) {
			butsel=BUT_HELP_PREV;
		}
		if (mousex>=200 && mousex<=219 && mousey>=378+40 && mousey<=385+40) {
			butsel=BUT_HELP_NEXT;
		}
		if (mousex>=211 && mousex<=218 && mousey>=3+40 && mousey<=10+40) {
			butsel=BUT_HELP_CLOSE;
		}

	}
	if (mousex>=704 && mousex<=739 && mousey>=22 && mousey<=30) butsel=BUT_HELP;
	if (mousex>=741 && mousex<=775 && mousey>=22 && mousey<=30) butsel=BUT_QUEST;
	if (mousex>=704 && mousex<=723 && mousey>=7 && mousey<=18) butsel=BUT_EXIT;
	if (mousex>=643 && mousex<=650 && mousey>=53 && mousey<=60) butsel=BUT_NOLOOK;

        // hit map
        if (!hitsel[0] && butsel==-1 && mousex>=dot[DOT_MTL].x && mousey>=dot[DOT_MTL].y && mousex<dot[DOT_MBR].x && mousey<dot[DOT_MBR].y) {
                if (vk_char) chrsel=get_near_char(mousex,mousey);
                if (chrsel==-1 && vk_item) itmsel=get_near_item(mousex,mousey,CMF_USE|CMF_TAKE,csprite);
                if (chrsel==-1 && itmsel==-1 &&!vk_char && (!vk_item || csprite)) mapsel=get_near_ground(mousex,mousey);

                if (mapsel!=-1 || itmsel!=-1 || chrsel!=-1)  butsel=BUT_MAP;
        }

        if (!hitsel[0] && butsel==-1) {
                butsel=get_near_button(mousex,mousey);

                // translate button
                if (butsel>=BUT_INV_BEG && butsel<=BUT_INV_END) invsel=30+invoff*INVDX+butsel-BUT_INV_BEG;
                else if (butsel>=BUT_WEA_BEG && butsel<=BUT_WEA_END) weasel=butsel-BUT_WEA_BEG;
                else if (butsel>=BUT_CON_BEG && butsel<=BUT_CON_END) consel=conoff*CONDX+butsel-BUT_CON_BEG;
                else if (butsel>=BUT_SKL_BEG && butsel<=BUT_SKL_END) sklsel=skloff+butsel-BUT_SKL_BEG;
        }

        // set lcmd
        lcmd=CMD_NONE;

        if (mapsel!=-1 && !vk_item && !vk_char) lcmd=CMD_MAP_MOVE;
	if (mapsel!=-1 &&  vk_item && !vk_char && csprite) lcmd=CMD_MAP_DROP;

	if (itmsel!=-1 &&  vk_item && !vk_char && !csprite && map[itmsel].flags&CMF_USE) lcmd=CMD_ITM_USE;
	if (itmsel!=-1 &&  vk_item && !vk_char && !csprite && map[itmsel].flags&CMF_TAKE) lcmd=CMD_ITM_TAKE;
	if (itmsel!=-1 &&  vk_item && !vk_char &&  csprite && map[itmsel].flags&CMF_USE) lcmd=CMD_ITM_USE_WITH;

	if (chrsel!=-1 && !vk_item &&  vk_char && !csprite) lcmd=CMD_CHR_ATTACK;
	if (chrsel!=-1 && !vk_item &&  vk_char &&  csprite) lcmd=CMD_CHR_GIVE;

	if (invsel!=-1 && !vk_item && !vk_char && !csprite &&  item[invsel] && (!con_type || !con_cnt)) lcmd=CMD_INV_USE;
	if (invsel!=-1 && !vk_item && !vk_char && !csprite && !item[invsel] && (!con_type || !con_cnt)) lcmd=CMD_INV_USE;		// fake
	if (invsel!=-1 && !vk_item && !vk_char &&  csprite &&  item[invsel] && (!con_type || !con_cnt)) lcmd=CMD_INV_USE_WITH;

	if (invsel!=-1 && !vk_item && !vk_char && !csprite &&  item[invsel] &&  con_type==2 &&  con_cnt) lcmd=CMD_CON_FASTSELL;
	if (invsel!=-1 && !vk_item && !vk_char && !csprite && !item[invsel] &&  con_type==2 &&  con_cnt) lcmd=CMD_CON_FASTSELL;	// fake
	if (invsel!=-1 && !vk_item && !vk_char &&  csprite &&  item[invsel] &&  con_type==2 &&  con_cnt) lcmd=CMD_CON_FASTSELL;
	
	if (invsel!=-1 && !vk_item && !vk_char && !csprite &&  item[invsel] &&  con_type==1 &&  con_cnt) lcmd=CMD_CON_FASTDROP;
	if (invsel!=-1 && !vk_item && !vk_char && !csprite && !item[invsel] &&  con_type==1 &&  con_cnt) lcmd=CMD_CON_FASTDROP;	// fake
	if (invsel!=-1 && !vk_item && !vk_char &&  csprite &&  item[invsel] &&  con_type==1 &&  con_cnt) lcmd=CMD_CON_FASTDROP;

	if (invsel!=-1 && !vk_item && !vk_char &&  csprite && !item[invsel]) lcmd=CMD_INV_USE_WITH;	// fake
	if (invsel!=-1 &&  vk_item && !vk_char && !csprite &&  item[invsel]) lcmd=CMD_INV_TAKE;
	if (invsel!=-1 &&  vk_item && !vk_char && !csprite && !item[invsel]) lcmd=CMD_INV_TAKE;  // fake - slot is empty so i can't take
	if (invsel!=-1 &&  vk_item && !vk_char &&  csprite &&  item[invsel]) lcmd=CMD_INV_SWAP;
	if (invsel!=-1 &&  vk_item && !vk_char &&  csprite && !item[invsel]) lcmd=CMD_INV_DROP;

	if (weasel!=-1 && !vk_item && !vk_char && !csprite &&  item[weatab[weasel]]) lcmd=CMD_WEA_USE;
	if (weasel!=-1 && !vk_item && !vk_char && !csprite && !item[weatab[weasel]]) lcmd=CMD_WEA_USE;		// fake
	if (weasel!=-1 && !vk_item && !vk_char &&  csprite &&  item[weatab[weasel]]) lcmd=CMD_WEA_USE_WITH;
	if (weasel!=-1 && !vk_item && !vk_char &&  csprite && !item[weatab[weasel]]) lcmd=CMD_WEA_USE_WITH;	// fake
	if (weasel!=-1 &&  vk_item && !vk_char && !csprite &&  item[weatab[weasel]]) lcmd=CMD_WEA_TAKE;
	if (weasel!=-1 &&  vk_item && !vk_char && !csprite && !item[weatab[weasel]]) lcmd=CMD_WEA_TAKE; // fake - slot is empty so i can't take
	if (weasel!=-1 &&  vk_item && !vk_char &&  csprite &&  item[weatab[weasel]]) lcmd=CMD_WEA_SWAP;
	if (weasel!=-1 &&  vk_item && !vk_char &&  csprite && !item[weatab[weasel]]) lcmd=CMD_WEA_DROP;

	if (consel!=-1 &&  vk_item && !vk_char && !csprite && con_type==1 && con_cnt &&  container[consel]) lcmd=CMD_CON_TAKE;
	if (consel!=-1 &&  vk_item && !vk_char && !csprite && con_type==1 && con_cnt && !container[consel]) lcmd=CMD_CON_TAKE;  // fake - slot is empty so i can't take (buy is also not possible)
	
	if (consel!=-1 && !vk_item && !vk_char && !csprite && con_type==1 && con_cnt &&  container[consel]) lcmd=CMD_CON_FASTTAKE;
	if (consel!=-1 && !vk_item && !vk_char && !csprite && con_type==1 && con_cnt && !container[consel]) lcmd=CMD_CON_FASTTAKE;	// fake

	if (consel!=-1 &&  vk_item && !vk_char && !csprite && con_type==2 && con_cnt                     ) lcmd=CMD_CON_BUY;
	if (consel!=-1 && !vk_item && !vk_char && !csprite && con_type==2 && con_cnt                     ) lcmd=CMD_CON_FASTBUY;

	if (consel!=-1 &&  vk_item && !vk_char &&  csprite && con_type==1 && con_cnt &&  container[consel]) lcmd=CMD_CON_SWAP;
	if (consel!=-1 &&  vk_item && !vk_char &&  csprite && con_type==1 && con_cnt && !container[consel]) lcmd=CMD_CON_DROP;
	if (consel!=-1 &&  vk_item && !vk_char &&  csprite && con_type==2 && con_cnt                      ) lcmd=CMD_CON_SELL;

	if (splsel!=-1 && !vk_item && !vk_char) lcmd=CMD_SPL_SET_L;

	if (telsel!=-1) lcmd=CMD_TELEPORT;
	if (colsel!=-1) lcmd=CMD_COLOR;
	

        if (lcmd==CMD_NONE) {
                if (butsel==BUT_SCR_UP) lcmd=CMD_INV_OFF_UP;
                if (butsel==BUT_SCR_DW) lcmd=CMD_INV_OFF_DW;
                if (butsel==BUT_SCR_TR && !vk_lbut) lcmd=CMD_INV_OFF_TR;

                if (butsel==BUT_SCL_UP && !con_cnt) lcmd=CMD_SKL_OFF_UP;
                if (butsel==BUT_SCL_DW && !con_cnt) lcmd=CMD_SKL_OFF_DW;
                if (butsel==BUT_SCL_TR && !con_cnt && !vk_lbut) lcmd=CMD_SKL_OFF_TR;

                if (butsel==BUT_SCL_UP &&  con_cnt) lcmd=CMD_CON_OFF_UP;
                if (butsel==BUT_SCL_DW &&  con_cnt) lcmd=CMD_CON_OFF_DW;
                if (butsel==BUT_SCL_TR &&  con_cnt && !vk_lbut) lcmd=CMD_CON_OFF_TR;

                if (sklsel!=-1) lcmd=CMD_SKL_RAISE;

                if (hitsel[0]) lcmd=CMD_SAY_HITSEL;

                if ( vk_item && butsel==BUT_GLD && csprite>=SPR_GOLD_BEG && csprite<=SPR_GOLD_END) lcmd=CMD_DROP_GOLD;
                if (!vk_item && butsel==BUT_GLD && csprite>=SPR_GOLD_BEG && csprite<=SPR_GOLD_END) { takegold=cprice; lcmd=CMD_TAKE_GOLD; }
                if (!vk_item && butsel==BUT_GLD && !csprite) { takegold=0; lcmd=CMD_TAKE_GOLD; }
                if ( vk_item && butsel==BUT_JNK) lcmd=CMD_JUNK_ITEM;

                if (butsel>=BUT_MOD_WALK0 && butsel<=BUT_MOD_WALK2) lcmd=CMD_SPEED0+butsel-BUT_MOD_WALK0;
                if (butsel>=BUT_MOD_COMB0 && butsel<=BUT_MOD_COMB2) lcmd=CMD_COMBAT0+butsel-BUT_MOD_COMB0;

		if (butsel==BUT_HELP_MISC) lcmd=CMD_HELP_MISC;
		if (butsel==BUT_HELP_PREV) lcmd=CMD_HELP_PREV;
		if (butsel==BUT_HELP_NEXT) lcmd=CMD_HELP_NEXT;
		if (butsel==BUT_HELP_CLOSE) lcmd=CMD_HELP_CLOSE;
		if (butsel==BUT_EXIT) lcmd=CMD_EXIT;
		if (butsel==BUT_HELP) lcmd=CMD_HELP;
		if (butsel==BUT_QUEST) lcmd=CMD_QUEST;
		if (butsel==BUT_NOLOOK) lcmd=CMD_NOLOOK;
        }


        // set rcmd
        rcmd=CMD_NONE;

	skl_look_sel=get_skl_look(mousex,mousey);
        if (con_cnt==0 && skl_look_sel!=-1) rcmd=CMD_SKL_LOOK;
        else if (!vk_spell) {
                if (mapsel!=-1) rcmd=CMD_MAP_LOOK;
                if (itmsel!=-1) rcmd=CMD_ITM_LOOK;
                if (chrsel!=-1) rcmd=CMD_CHR_LOOK;
                if (invsel!=-1) rcmd=CMD_INV_LOOK;
                if (weasel!=-1) rcmd=CMD_WEA_LOOK;
                if (consel!=-1) rcmd=CMD_CON_LOOK;
                if (splsel!=-1) rcmd=CMD_SPL_SET_R;
        } else {
                if (mapsel!=-1) rcmd=CMD_MAP_CAST_R;
                if (itmsel!=-1) rcmd=CMD_ITM_CAST_R;
                if (chrsel!=-1) rcmd=CMD_CHR_CAST_R;
                if (splsel!=-1) rcmd=CMD_SPL_SET_R;
        }

        // set cursor

        if (vk_rbut) set_cmd_cursor(rcmd); else set_cmd_cursor(lcmd);
}

static void exec_cmd(int cmd, int a)
{
	extern int display_help,display_quest;

        switch (cmd) {
                case CMD_NONE:          return;

                case CMD_MAP_MOVE:      cmd_move(originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX); return;
                case CMD_MAP_DROP:      cmd_drop(originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX); return;

                case CMD_ITM_TAKE:      cmd_take(originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX); return;
                case CMD_ITM_USE:       cmd_use(originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX); return;
                case CMD_ITM_USE_WITH:  cmd_use(originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX); return;

                case CMD_CHR_ATTACK:    cmd_kill(map[chrsel].cn); return;
                case CMD_CHR_GIVE:      cmd_give(map[chrsel].cn); return;

                case CMD_INV_USE:       cmd_use_inv(invsel); return;
                case CMD_INV_USE_WITH:  cmd_use_inv(invsel); return;
                case CMD_INV_TAKE:      cmd_swap(invsel); return;
                case CMD_INV_SWAP:      cmd_swap(invsel); return;
		case CMD_INV_DROP:      cmd_swap(invsel); return;

		case CMD_CON_FASTDROP:  cmd_fastsell(invsel); return;
		case CMD_CON_FASTSELL:  cmd_fastsell(invsel); return;

                case CMD_WEA_USE:       cmd_use_inv(weatab[weasel]); return;
                case CMD_WEA_USE_WITH:  cmd_use_inv(weatab[weasel]); return;
                case CMD_WEA_TAKE:      cmd_swap(weatab[weasel]); return;
                case CMD_WEA_SWAP:      cmd_swap(weatab[weasel]); return;
                case CMD_WEA_DROP:      cmd_swap(weatab[weasel]); return;

                case CMD_CON_TAKE:      //return;
                case CMD_CON_BUY:       //return;
                case CMD_CON_SWAP:      //return;
                case CMD_CON_DROP:      //return;
		case CMD_CON_SELL:      cmd_con(consel); return;
		case CMD_CON_FASTTAKE:
		case CMD_CON_FASTBUY:	cmd_con_fast(consel); return;

                case CMD_MAP_LOOK:      cmd_look_map(originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX); return;
                case CMD_ITM_LOOK:      cmd_look_item(originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX); return;
                case CMD_CHR_LOOK:      cmd_look_char(map[chrsel].cn); return;
                case CMD_INV_LOOK:      cmd_look_inv(invsel); return;
                case CMD_WEA_LOOK:      cmd_look_inv(weatab[weasel]); return;
                case CMD_CON_LOOK:      cmd_look_con(consel); return;

                case CMD_MAP_CAST_L:    cmd_some_spell(/*spelltab[curspell_l].cl*/CL_FIREBALL,originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX,0); break;
                case CMD_ITM_CAST_L:    cmd_some_spell(/*spelltab[curspell_l].cl*/CL_FIREBALL,originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX,0); break;
                case CMD_CHR_CAST_L:    cmd_some_spell(/*spelltab[curspell_l].cl*/CL_FIREBALL,0,0,map[chrsel].cn); break;
                case CMD_MAP_CAST_R:    cmd_some_spell(/*spelltab[curspell_r].cl*/CL_BALL,originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX,0); break;
                case CMD_ITM_CAST_R:    cmd_some_spell(/*spelltab[curspell_r].cl*/CL_BALL,originx-MAPDX/2+itmsel%MAPDX,originy-MAPDY/2+itmsel/MAPDX,0); break;
		case CMD_CHR_CAST_R:    cmd_some_spell(/*spelltab[curspell_r].cl*/CL_BALL,0,0,map[chrsel].cn); break;

		case CMD_SLF_CAST_K:	cmd_some_spell(a,0,0,map[plrmn].cn); break;
                case CMD_MAP_CAST_K:    cmd_some_spell(a,originx-MAPDX/2+mapsel%MAPDX,originy-MAPDY/2+mapsel/MAPDX,0); break;
                case CMD_CHR_CAST_K:    cmd_some_spell(a,0,0,map[chrsel].cn); break;

                case CMD_SPL_SET_L:     curspell_l=splsel; break;
                case CMD_SPL_SET_R:     curspell_r=splsel; break;

                case CMD_SKL_RAISE:     cmd_raise(skltab[sklsel].v); break;

                case CMD_INV_OFF_UP:    set_invoff(0,invoff-1); break;
                case CMD_INV_OFF_DW:    set_invoff(0,invoff+1); break;
                case CMD_INV_OFF_TR:    set_invoff(1,0/*mousey*/); break;

                case CMD_SKL_OFF_UP:    set_skloff(0,skloff-1); break;
                case CMD_SKL_OFF_DW:    set_skloff(0,skloff+1); break;
                case CMD_SKL_OFF_TR:    set_skloff(1,0/*mousey*/); break;

                case CMD_CON_OFF_UP:    set_conoff(0,conoff-1); break;
                case CMD_CON_OFF_DW:    set_conoff(0,conoff+1); break;
                case CMD_CON_OFF_TR:    set_conoff(1,0/*mousey*/); break;

		case CMD_SAY_HITSEL:    cmd_add_text(hitsel); break;

                case CMD_USE_FKEYITEM:	cmd_use_inv(fkeyitem[a]); return;

                case CMD_DROP_GOLD:     cmd_drop_gold(); return;
                case CMD_TAKE_GOLD:     cmd_take_gold(takegold); return;

                case CMD_JUNK_ITEM:     cmd_junk_item(); return;

                case CMD_SPEED0:        if (pspeed!=0) cmd_speed(0); return;
                case CMD_SPEED1:        if (pspeed!=1) cmd_speed(1); return;
                case CMD_SPEED2:        if (pspeed!=2) cmd_speed(2); return;
                case CMD_COMBAT0:       if (pcombat!=0) cmd_combat(0); return;
                case CMD_COMBAT1:       if (pcombat!=1) cmd_combat(1); return;
		case CMD_COMBAT2:       if (pcombat!=2) cmd_combat(2); return;

		case CMD_TELEPORT:	if (telsel==1042) clan_offset=16-clan_offset;
					else {
						if (telsel>=64 && telsel<=100) cmd_teleport(telsel+clan_offset);
						else cmd_teleport(telsel);
					}
					return;
		case CMD_COLOR:		cmd_color(colsel); return;
		case CMD_SKL_LOOK:	cmd_look_skill(skl_look_sel); return;

		case CMD_HELP_NEXT:	if (display_help) { display_help++; if (display_help>MAXHELP) display_help=1; }
					if (display_quest) { display_quest++; if (display_quest>MAXQUEST2) display_quest=1; }
					return;
		case CMD_HELP_PREV:	if (display_help) { display_help--; if (display_help<1) display_help=MAXHELP; }
					if (display_quest) { display_quest--; if (display_quest<1) display_quest=MAXQUEST2; }
					return;
		case CMD_HELP_CLOSE:	display_help=0; display_quest=0; return;
		case CMD_HELP_MISC:	if (helpsel>0 && helpsel<=MAXHELP && display_help) display_help=helpsel;
					if (questsel!=-1) quest_select(questsel);
					return;
		case CMD_HELP:		if (display_help) display_help=0;
					else { display_help=1; display_quest=0; }
					return;
		case CMD_QUEST:		if (display_quest) display_quest=0;
					else { display_quest=1; display_help=0; }
					return;

		case CMD_EXIT:		quit=1; return;
		case CMD_NOLOOK:	show_look=0; return;

        }
        return;
}

#define GEN_SET_ALPHA           1 // a
#define GEN_SET_GAMMA           2 // a
#define GEN_FORCE_PNG           3 // a developer only
#define GEN_SET_LIGHTQUALITY    4 // a
#define GEN_SET_LIGHTEFFECT	5
#define GEN_FORCE_DH            6 // a developer only

#pragma argsused
int exec_gen(int gen, int a, char *c)
{
        switch(gen) {
                case GEN_SET_ALPHA:
                        if (a<1) return -1;
                        if (a>32) return -1;
                        dd_usealpha=a;
                        dd_reset_cache(1,1,1);
                        return dd_usealpha;
		case GEN_SET_GAMMA:
			if (a<1) return -1;
                        if (a>31) return -1;
			dd_gamma=a;
			//dd_reset_cache(1,1,1);
			dd_exit_cache(); dd_init_cache();
                        return dd_gamma;
                case GEN_FORCE_PNG:
                        gfx_force_png=a;
                        dd_reset_cache(1,1,1);
                        return gfx_force_png;
                case GEN_SET_LIGHTQUALITY:
                        lightquality=a;
                        return lightquality;
		case GEN_SET_LIGHTEFFECT:
			if (a<1) return -1;
                        if (a>31) return -1;
			dd_lighteffect=a;
			//dd_reset_cache(1,1,1);
			dd_exit_cache(); dd_init_cache();
                        return dd_lighteffect;
		case GEN_FORCE_DH:
                        gfx_force_dh=a;
                        dd_reset_cache(1,1,1);
                        return gfx_force_dh;
        }
        return 0;
}

/*void moac_tinproc(int t, char *buf)
{
        if (t!=TIN_TEXT) return;

        if (!strncmp(buf,"#ps ",3)) {
                playersprite_override=atoi(&buf[3]);
        }
        else if (!strncmp(buf,"#ua ",4)) {
                exec_gen(GEN_SET_ALPHA,atoi(&buf[4]),NULL);
                addline("alpha=%d",dd_usealpha);
        }
        else if (!strncmp(buf,"#lq",3)) {
                exec_gen(GEN_SET_LIGHTQUALITY,(lightquality+1)%4,NULL);
                addline("lightquality=%d",lightquality);
        }
        else if (!strncmp(buf,"#gamma ",7)) {
                exec_gen(GEN_SET_GAMMA,atoi(&buf[7]),NULL);
                addline("using gamma %d",dd_gamma);
        }
	else if (!strncmp(buf,"#light ",7)) {
                exec_gen(GEN_SET_LIGHTEFFECT,atoi(&buf[7]),NULL);
                addline("using light %d",dd_lighteffect);
        }
        else if (!strncmp(buf,"#png",4)) {
                exec_gen(GEN_FORCE_PNG,gfx_force_png^1,NULL);
                addline("png=%d",gfx_force_png);
        }
        else {
#ifdef EDITOR
                if (editor) editor_tinproc(t,buf); else
#endif
                cmd_text(buf);
        }

        tin_do(t,TIN_DO_CLEAR,0);
}*/

// tin

/*void init_tin(int t, int max, int sx, int sy, int dx, unsigned short int color, const char *set, int maxhistory)
{
        if (!tin) {
                tin=xcalloc(MAX_TIN*sizeof(TIN),MEM_GUI);
                curtin=0;
        }

        PARANOIA( if (max<=1) paranoia("init_tin: tin[%d] max<=1",t); )
        PARANOIA( if (t<1 || t>=MAX_TIN) paranoia("init_tin: tin[%d] out of range",t); )
        PARANOIA( if (tin[t].max) paranoia("init_tin: tin[%d] already inited",t); )
        PARANOIA( if (!set) paranoia("init_tin: even tin[%d] needs a set",t); )

        tin[t].max=max;
        tin[t].buf=xcalloc(tin[t].max,MEM_GUI);
        tin[t].set=set;
        tin[t].sx=sx;
        tin[t].sy=sy;
        tin[t].dx=dx;
        tin[t].color=color;
        tin[t].maxhistory=maxhistory;
        tin[t].history=xcalloc(tin[t].max*tin[t].maxhistory,MEM_GUI);
        tin[t].curhistory=0;
        tin[t].runhistory=-1;
}

void exit_tin(void)
{
        int i;

        if (!tin) return;

        for (i=0; i<MAX_TIN; i++) {
                xfree(tin[i].buf);
                xfree(tin[i].history);
        }
        xfree(tin);
        tin=NULL;
        curtin=0;
}

void tin_do(int t, int tindo, char c)
{
        char *buf;
        int pos,max,i;
        static int callenter=0;

        if (t==0) return;

        PARANOIA( if (!tin) paranoia("tin_do: tin not inited at all"); )
        PARANOIA( if (t<1 || t>=MAX_TIN) paranoia("tin_do: tin[%d] out of range",t); )
        PARANOIA( if (!tin[t].max) paranoia("tin_do: tin[%d] not inited",t); )

        buf=tin[t].buf;
        pos=tin[t].pos;
        max=tin[t].max;

        switch(tindo)
        {
                case TIN_DO_ADD:
                        for (i=0; ; i++) if (tin[t].set[i]==0) return; else if(tin[t].set[i]==c) break;
                        if (pos==max-1) goto beep;
                        if (buf[max-1]) MessageBeep(-1);
                        memmove(buf+pos+1,buf+pos,max-pos-2);
                        buf[pos]=c;
                        tin[t].pos++;
                        tin[t].runhistory=-1;
                        return;

                case TIN_DO_DEL:
                        if (!buf[pos]) goto beep;
                        memmove(buf+pos,buf+pos+1,max-pos-1);
                        tin[t].runhistory=-1;
                        return;

                case TIN_DO_BS:
                        if (pos==0) goto beep;
                        memmove(buf+pos-1,buf+pos,max-pos);
                        tin[t].pos--;
                        tin[t].runhistory=-1;
                        return;

                case TIN_DO_MOVE:
                        if (c<0) pos--; else if (c>0) pos++;
                        if (pos<0 || (pos>0 && !buf[pos-1])) goto beep;
                        tin[t].pos=pos;
                        return;

                case TIN_DO_HOME:
                        if (pos==0) goto beep;
                        tin[t].pos=0;
                        return;

                case TIN_DO_END:
                        pos=strlen(buf);
                        if (pos==tin[t].pos) goto beep;
                        tin[t].pos=pos;
                        return;

                case TIN_DO_SEEK:
                        if (c<0) {
                                if (pos>0) pos--;
                                while (pos>0 && isalnum(buf[pos-1])) pos--;
                        }
                        else if (c>0) {
                                if (buf[pos]) pos++;
                                while (buf[pos] && (pos==0 || isalnum(buf[pos-1]))) pos++;
                        }
                        if (tin[t].pos==pos) goto beep;
                        tin[t].pos=pos;
                        return;

                case TIN_DO_CLEAR:
                        // copy to history
                        strcpy(tin[t].history+tin[t].max*tin[t].curhistory,tin[t].buf);
                        tin[t].curhistory=(tin[t].curhistory+1)%tin[t].maxhistory;
                        tin[t].runhistory=-1;

                        // reset buf
                        tin[t].buf[tin[t].pos=0]=0;
                        return;

                case TIN_DO_ENTER:
                        if (callenter) return;
                        callenter=1;
                        moac_tinproc(t,tin[t].buf);
                        callenter=0;
                        return;

                case TIN_DO_PREVHIST:
                        if (tin[t].runhistory==-1) {
                                strcpy(tin[t].history+tin[t].max*tin[t].curhistory,tin[t].buf);
                                tin[t].runhistory=(tin[t].curhistory+tin[t].maxhistory-1)%tin[t].maxhistory;
                        }
                        else if ((tin[t].runhistory+tin[t].maxhistory-1)%tin[t].maxhistory!=tin[t].curhistory) tin[t].runhistory=(tin[t].runhistory+tin[t].maxhistory-1)%tin[t].maxhistory;
                        else goto beep;

                        sprintf(tin[t].buf,"%s",tin[t].history+tin[t].max*tin[t].runhistory);
                        tin[t].pos=strlen(tin[t].buf);

                        return;

                case TIN_DO_NEXTHIST:
                        if (tin[t].runhistory==-1) goto beep;
                        else if (tin[t].runhistory!=tin[t].curhistory) tin[t].runhistory=(tin[t].runhistory+1)%tin[t].maxhistory;
                        else goto beep;

                        sprintf(tin[t].buf,"%s",tin[t].history+tin[t].max*tin[t].runhistory);
                        tin[t].pos=strlen(tin[t].buf);
                        return;
        }

        return;
beep:
        MessageBeep(-1);
}

void display_tin(int t, int dd_frame)
{
        int x,y,cx;
        PARANOIA( if (!tin) paranoia("display_tin: tin not inited at all"); )
        PARANOIA( if (t<1 || t>=MAX_TIN) paranoia("display_tin: tin[%d] out of range",t); )
        PARANOIA( if (!tin[t].max) paranoia("display_tin: tin[%d] not inited",t); )

        x=tin[t].sx;
        y=tin[t].sy;

        dd_drawtext(x,y,tin[t].color,DD_LEFT|DD_LARGE|dd_frame,tin[t].buf);
        if ((now/500)&1) {
                cx=x-1+dd_textlen(DD_LEFT|DD_LARGE,tin[t].buf,tin[t].pos);
                dd_drawtext(cx,y,tin[t].color,DD_LEFT|DD_LARGE,"|");
        }
}*/

#define MAXCMDLINE	199
#define MAXHIST		20
static char cmdline[MAXCMDLINE+1]={""};
static char *history[MAXHIST];
static int cmdcursor=0,cmddisplay=0,cmdlen=0,histpos=-1;
extern char user_keys[];

void update_user_keys(void)
{
	int n;

	for (n=0; n<10; n++) {
		keytab[n].userdef=user_keys[n];
		keytab[n+10].userdef=user_keys[n];
		keytab[n+20].userdef=user_keys[n];
	}
}

char *strcasestr(const char *haystack,const char *needle)
{
        const char *ptr;

        for (ptr=needle; *haystack; haystack++) {
                if (toupper(*ptr)==toupper(*haystack)) {
                        ptr++;
                        if (!*ptr) return (char*)(haystack+(needle-ptr+1));
                } else ptr=needle;
        }
        return NULL;
}

int client_cmd(char *buf)
{
	extern void save_options(void);

	if (!strncmp(buf,"#ps ",3)) {
                playersprite_override=atoi(&buf[3]);
		return 1;
        }
        if (!strncmp(buf,"#ua ",4)) {
                exec_gen(GEN_SET_ALPHA,atoi(&buf[4]),NULL);
                addline("alpha=%d",dd_usealpha);
		return 1;
        }
        if (!strncmp(buf,"#lq",3)) {
                exec_gen(GEN_SET_LIGHTQUALITY,(lightquality+1)%4,NULL);
                addline("lightquality=%d",lightquality);
		return 1;
        }
        if (!strncmp(buf,"#gamma ",7)) {
                exec_gen(GEN_SET_GAMMA,atoi(&buf[7]),NULL);
                addline("using gamma %d",dd_gamma);
		return 1;
        }
	if (!strncmp(buf,"#light ",7)) {
                exec_gen(GEN_SET_LIGHTEFFECT,atoi(&buf[7]),NULL);
                addline("using light %d",dd_lighteffect);
		return 1;
        }
        if (!strncmp(buf,"#png",4)) {
                exec_gen(GEN_FORCE_PNG,gfx_force_png^1,NULL);
                addline("png=%d",gfx_force_png);
		return 1;
        }
	if (!strncmp(buf,"#dh",3)) {
                exec_gen(GEN_FORCE_DH,gfx_force_dh^1,NULL);
                addline("dh=%d",gfx_force_dh);
		return 1;
        }
	if (!strncmp(buf,"#col1",5) || !strncmp(buf,"#col2",5) || !strncmp(buf,"#col3",5) ||
	    !strncmp(buf,"/col1",5) || !strncmp(buf,"/col2",5) || !strncmp(buf,"/col3",5)) {
		show_color=1;
		show_cur=0;
		show_color_c[0]=map[MAPDX*MAPDY/2].rc.c1;
		show_color_c[1]=map[MAPDX*MAPDY/2].rc.c2;
		show_color_c[2]=map[MAPDX*MAPDY/2].rc.c3;
		return 1;
	}
	if (!strncmp(buf,"#set ",5) || !strncmp(buf,"/set ",5)) {
                int what,key;
		char *ptr;
		
		ptr=buf+5;
		
		while (isspace(*ptr)) ptr++;
                what=atoi(ptr);
		if (what==0) what=9;
		else what--;
		
		while (isdigit(*ptr)) ptr++;
		while (isspace(*ptr)) ptr++;
		key=toupper(*ptr);

		if (what<0 || what>9) {
			addline("Spell is out of bounds (must be between 0 and 9)");
			return 1;
		}
		if (key<'A' || key>'Z') {
			addline("Key is out of bounds (must be between A and Z)");
			return 1;
		}
		user_keys[what]=key;
		update_user_keys();
		save_options();

		addline("Set key %c for spell %d.",key,what==9 ? 0 : what+1);
		
		return 1;
        }
	if (strcasestr(buf,password)) {
		addline("°c3Sorry, but you are not allowed to say your password. No matter what you're promised, do not give your password to anyone! The only things which happened to players who did are: Loss of all items, lots of negative experience, bad karma and locked characters. If you really, really think you have to tell your password to someone, then I'm sure you'll find a way around this block.");
		return 1;
	}
#ifdef EDITOR
	if (editor) {
		editor_tinproc(42,buf);
	}
#endif

	return 0;
}

char rem_buf[10][256]={""};
int rem_in=0,rem_out=0;
void cmd_remember(char *ptr)
{
	char *start=ptr,*dst;
	char tmp[256];

	if (*ptr!='#' && *ptr!='/') return; ptr++;
	if (*ptr!='t' && *ptr!='T') return; ptr++;
	if (*ptr==' ') goto do_remember;
	if (*ptr!='e' && *ptr!='E') return; ptr++;
	if (*ptr==' ') goto do_remember;
	if (*ptr!='l' && *ptr!='L') return; ptr++;
	if (*ptr==' ') goto do_remember;
	if (*ptr!='l' && *ptr!='L') return; ptr++;
	if (*ptr==' ') goto do_remember;
	return;

do_remember:
	while (isspace(*ptr)) ptr++;
	while (*ptr && !isspace(*ptr)) ptr++;
	
        for (dst=tmp; start<=ptr; *dst++=*start++) ;
	*dst=0;

	if (strcmp(tmp,rem_buf[rem_in])) {
		rem_in=(rem_in+1)%10;
		strcpy(rem_buf[rem_in],tmp);
	}
	rem_out=rem_in;
}

void cmd_fetch(char *ptr)
{
	if (rem_out!=(rem_in+1)%10) strcpy(ptr,rem_buf[rem_out]);
	rem_out=(rem_out+9)%10;
}

void cmd_proc(int key)
{
        switch(key) {
		case CMD_BACK:	if (cmdcursor<1) break;
				memmove(cmdline+cmdcursor-1,cmdline+cmdcursor,MAXCMDLINE-cmdcursor);
				cmdline[MAXCMDLINE-1]=0;
				cmdcursor--;				
				break;
		case CMD_DELETE:
				memmove(cmdline+cmdcursor,cmdline+cmdcursor+1,MAXCMDLINE-cmdcursor-1);
				cmdline[MAXCMDLINE-1]=0;
				break;
		
		case CMD_LEFT:	if (cmdcursor>0) cmdcursor--;
				break;
		
		case CMD_RIGHT:	if (cmdcursor<MAXCMDLINE-1) {
					if (cmdline[cmdcursor]==0) cmdline[cmdcursor]=' ';
					cmdcursor++;
				}
				break;
		
		case CMD_HOME:	cmdcursor=0;				
				break;

		case CMD_END:	for (cmdcursor=MAXCMDLINE-2; cmdcursor>=0; cmdcursor--) if (cmdline[cmdcursor]) break; cmdcursor++;
				break;

		case CMD_UP:	if (histpos<MAXHIST-1 && history[histpos+1]) histpos++;
				else break;
                                bzero(cmdline,sizeof(cmdline));
				strcpy(cmdline,history[histpos]);
				for (cmdcursor=MAXCMDLINE-2; cmdcursor>=0; cmdcursor--) if (cmdline[cmdcursor]) break; cmdcursor++;
                                break;

		case CMD_DOWN:	if (histpos>0) histpos--;
				else { bzero(cmdline,sizeof(cmdline)); cmdcursor=0; histpos=-1; break; }
                                bzero(cmdline,sizeof(cmdline));
				strcpy(cmdline,history[histpos]);
				for (cmdcursor=MAXCMDLINE-2; cmdcursor>=0; cmdcursor--) if (cmdline[cmdcursor]) break; cmdcursor++;
				break;
		
		case 13:	if (!client_cmd(cmdline) && cmdline[0]) cmd_text(cmdline);
				cmd_remember(cmdline);
				if (history[MAXHIST-1]) xfree(history[MAXHIST-1]);
                                memmove(history+1,history,sizeof(history)-sizeof(history[0]));
				history[0]=xstrdup(cmdline,MEM_TEMP);
				cmdcursor=cmddisplay=0; histpos=-1;
				bzero(cmdline,sizeof(cmdline));
				break;

		case 9:		bzero(cmdline,sizeof(cmdline));
				cmd_fetch(cmdline);
				for (cmdcursor=MAXCMDLINE-2; cmdcursor>=0; cmdcursor--) if (cmdline[cmdcursor]) break; cmdcursor++;
				break;

                default:	if (key<32 || key>127) { /* addline("%d",key); */ break; }
				if (cmdcursor<MAXCMDLINE-1) {
					memmove(cmdline+cmdcursor+1,cmdline+cmdcursor,MAXCMDLINE-cmdcursor-1);
					cmdline[cmdcursor++]=key;
				}
				break;
	}
}

void cmd_add_text(char *buf)
{
	while (*buf) cmd_proc(*buf++);	
}

void display_cmd(void)
{
	int n,x,tmp;

	if (cmdcursor<cmddisplay) cmddisplay=0;


	for (x=0,n=cmdcursor; n>cmddisplay; n--) {
		x+=dd_char_len(cmdline[n]);
		if (x>625-230-4) {
			cmddisplay=n;
			break;
		}
	}

        for (x=0,n=cmddisplay; n<MAXCMDLINE; n++) {
		tmp=dd_drawtext_char(230+x,587,cmdline[n],rgb2scr[IRGB(31,31,31)]);
		if (n==cmdcursor) {
			if (cmdline[n]) dd_shaded_rect(230+x-1,587,230+x+tmp+1,587+9);
			else dd_shaded_rect(230+x,587,230+x+4,587+9);
		}
                x+=tmp;
		if (x>625-230) break;
	}	
}

void insert_text(void)
{
	HANDLE h;
	char *ptr;

	if (!vk_shift) return;

	OpenClipboard(mainwnd);
	h=GetClipboardData(CF_TEXT);
	if (h && (ptr=GlobalLock(h))) {
		while (*ptr) {
			if (*ptr>=32) cmd_proc(*ptr);
			ptr++;
		}
		GlobalUnlock(h);
	} else MessageBeep(-1);
        CloseClipboard();
}


// key and char proc
int display_help=0;
int display_quest=0;
void gui_keyproc(int wparam)
{
        int i;
	extern int display_gfx;

        switch(wparam) {

		case VK_ESCAPE:         cmd_stop(); show_look=0; display_gfx=0; teleporter=0; show_tutor=0; display_help=0; display_quest=0; show_color=0; return;
		case VK_F1:            	if (fkeyitem[0]) exec_cmd(CMD_USE_FKEYITEM,0); return;
		case VK_F2:            	if (fkeyitem[1]) exec_cmd(CMD_USE_FKEYITEM,1); return;
		case VK_F3:            	if (fkeyitem[2]) exec_cmd(CMD_USE_FKEYITEM,2); return;
		case VK_F4:            	if (fkeyitem[3]) exec_cmd(CMD_USE_FKEYITEM,3); return;
		
		case VK_F5:		cmd_speed(1); return;
                case VK_F6:             cmd_speed(0); return;
		case VK_F7:             cmd_speed(2); return;

		case VK_F8:		nocut^=1; return;

		case VK_F9:		if (display_quest) display_quest=0;
					else { display_help=0; display_quest=1; } return;

		case VK_F10:		display_vc^=1; return;

		case VK_F11:            if (display_help) display_help=0;
					else { display_quest=0; display_help=1; } return;
                case VK_F12:            quit=1; return;

                case VK_RETURN:         cmd_proc(CMD_RETURN); return;
                case VK_DELETE:         cmd_proc(CMD_DELETE); return;
                case VK_BACK:           cmd_proc(CMD_BACK); return;
                case VK_LEFT:           cmd_proc(CMD_LEFT); return;
                case VK_RIGHT:          cmd_proc(CMD_RIGHT); return;
                case VK_HOME:           cmd_proc(CMD_HOME); return;
                case VK_END:            cmd_proc(CMD_END); return;
                case VK_UP:             cmd_proc(CMD_UP); return;
		case VK_DOWN:           cmd_proc(CMD_DOWN); return;

		case VK_INSERT:		insert_text(); return;

		case VK_NUMPAD0:
		case VK_NUMPAD1:
		case VK_NUMPAD2:
		case VK_NUMPAD3:
		case VK_NUMPAD4:
		case VK_NUMPAD5:
		case VK_NUMPAD6:
		case VK_NUMPAD7:
		case VK_NUMPAD8:
		case VK_NUMPAD9:
			wparam=wparam-96+'0';

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
                        if (!vk_item && !vk_char && !vk_spell) return;

                        for (i=0; i<max_keytab; i++) {

                                if (keytab[i].keycode!=wparam && keytab[i].userdef!=wparam) continue;

                                if ((keytab[i].vk_item  && !vk_item ) || (!keytab[i].vk_item  && vk_item )) continue;
                                if ((keytab[i].vk_char  && !vk_char ) || (!keytab[i].vk_char  && vk_char )) continue;
                                if ((keytab[i].vk_spell && !vk_spell) || (!keytab[i].vk_spell && vk_spell)) continue;

                                if (keytab[i].cl_spell) {
                                        if (keytab[i].tgt==TGT_MAP) exec_cmd(CMD_MAP_CAST_K,keytab[i].cl_spell);
                                        else if (keytab[i].tgt==TGT_CHR) exec_cmd(CMD_CHR_CAST_K,keytab[i].cl_spell);
                                        else if (keytab[i].tgt==TGT_SLF) exec_cmd(CMD_SLF_CAST_K,keytab[i].cl_spell);
                                        else return;     // hu ?
                                        keytab[i].usetime=now;
                                        return;
                                }
                                return;
                        }
                        return;

                case VK_PRIOR:  dd_text_pageup(); break;
		case VK_NEXT:   dd_text_pagedown(); break;
        }
}

LRESULT FAR PASCAL _export main_wnd_proc(HWND wnd, UINT msg,WPARAM wparam, LPARAM lparam)
{
	PAINTSTRUCT ps;
        char buf[1024];
        int i,m;
	extern int x_offset,y_offset;
	static int delta=0,mdown=0;

        if (quit) {
                if (msg==WM_DESTROY) PostQuitMessage(0);
                return DefWindowProc(wnd,msg,wparam,lparam);
        }

	switch (msg) {
		case    WM_SYSKEYDOWN:
                case 	WM_KEYDOWN:
#ifdef EDITOR
                        if (editor) editor_keyproc(wparam);
                        else
#endif
                        gui_keyproc(wparam);
			return 0;

		case WM_CHAR:
			if (!(lparam&(1u<<30))) 	// only process VM_CHAR if it is not the result of ALT-num
				cmd_proc(wparam);
			return 0;

		case WM_PAINT:
			BeginPaint(wnd,&ps);
			EndPaint(wnd,&ps);
			return 0;

                case WM_SETCURSOR:
                        SetCursor(cur_cursor);
                        break;

                case WM_CREATE:
                        mainwnd=wnd;
                        return 0;

                case WM_SIZE:
                        winxres=LOWORD(lparam);
                        winyres=HIWORD(lparam);
#ifdef EDITOR
                        if (editor) editor_sizeproc();
#endif
                        return 0;

                case WM_DESTROY:
                        mainwnd=NULL;
                        quit=1;
			PostQuitMessage(0);
			return 0;

                case WM_MOUSEMOVE:
                        mousex=(signed short int) LOWORD(lparam);
                        mousey=(signed short int) HIWORD(lparam);
#ifdef EDITOR
                        if (editor) return editor_mouseproc(msg);
#endif

                        if (capbut!=-1) {
                                if (mousex!=XRES/2 || mousey!=YRES/2) {
                                        POINT p;
                                        mousedx+=mousex-(XRES/2);
                                        mousedy+=mousey-(YRES/2);
                                        p.x=XRES/2;
                                        p.y=YRES/2;
                                        ClientToScreen(wnd,&p);
                                        SetCursorPos(p.x,p.y);
                                }
                        }

                        if (butsel!=-1 && vk_lbut && (but[butsel].flags&BUTF_MOVEEXEC)) exec_cmd(lcmd,0);

                        return 0;

                case WM_LBUTTONDOWN:
                        vk_lbut=1;			
#ifdef EDITOR
                        if (editor) return editor_mouseproc(msg);
#endif
                        if (butsel!=-1 && capbut==-1 && (but[butsel].flags&BUTF_CAPTURE)) {
                                POINT p;
                                ShowCursor(0);
                                SetCapture(wnd);
                                mousedx=0;
                                mousedy=0;
                                p.x=XRES/2;
                                p.y=YRES/2;
                                ClientToScreen(wnd,&p);
                                SetCursorPos(p.x,p.y);
                                capbut=butsel;
                        }
                        return 0;

                case WM_RBUTTONUP:
                        vk_rbut=0;
#ifdef EDITOR
                        if (editor) return editor_mouseproc(msg);
#endif
                        exec_cmd(rcmd,0);
			return 0;			


		case WM_MBUTTONUP:
			shift_override=0;
			control_override=0;
			mdown=0;
			if (special_tab[vk_special].spell) {
				if (special_tab[vk_special].target==TGT_MAP) exec_cmd(CMD_MAP_CAST_K,special_tab[vk_special].spell);
				else if (special_tab[vk_special].target==TGT_CHR) exec_cmd(CMD_CHR_CAST_K,special_tab[vk_special].spell);
                                else if (special_tab[vk_special].target==TGT_SLF) exec_cmd(CMD_SLF_CAST_K,special_tab[vk_special].spell);
                                return 0;
			}
			// fall through intended
		case WM_LBUTTONUP:
                        vk_lbut=0;			
#ifdef EDITOR
                        if (editor) return editor_mouseproc(msg);
#endif
                        if (capbut!=-1) {
                                POINT p;
                                // ClipCursor(NULL);
                                p.x=but[capbut].x+x_offset; p.y=but[capbut].y+y_offset; ClientToScreen(wnd,&p);
                                SetCursorPos(p.x,p.y);
                                ReleaseCapture();
                                ShowCursor(1);
                                // ShowCursor(1);
                                if (!(but[capbut].flags&BUTF_MOVEEXEC)) exec_cmd(lcmd,0);
                                capbut=-1;
                        }
                        else exec_cmd(lcmd,0);
                        return 0;

                case WM_RBUTTONDOWN:
                        vk_rbut=1;
#ifdef EDITOR
                        if (editor) return editor_mouseproc(msg);
#endif
                        return 0;

		case WM_MOUSEWHEEL:
			if (!(short)(HIWORD(wparam))) return 0;
			
			delta+=(short)(HIWORD(wparam));

			while (delta>=120) { vk_special_inc(); delta-=120; }
			while (delta<=-120) { vk_special_dec(); delta+=120; }
                        vk_special_time=now;

			if (mdown) {
				shift_override=special_tab[vk_special].shift_over;
				control_override=special_tab[vk_special].control_over;
			}

                        return 0;

		case WM_MBUTTONDOWN:
			shift_override=special_tab[vk_special].shift_over;
			control_override=special_tab[vk_special].control_over;
			mdown=1;
			return 0;

	}
        return DefWindowProc(wnd,msg,wparam,lparam);
}

int main_init(void)
{
        int i,x,y;

        // set_mapoff(400,270);
        // set_mapadd(0,0);

        c_only=LoadCursor(instance,MAKEINTRESOURCE(IDCU_ONLY));
        c_take=LoadCursor(instance,MAKEINTRESOURCE(IDCU_TAKE));
        c_drop=LoadCursor(instance,MAKEINTRESOURCE(IDCU_DROP));
        c_attack=LoadCursor(instance,MAKEINTRESOURCE(IDCU_ATTACK));
        c_raise=LoadCursor(instance,MAKEINTRESOURCE(IDCU_RAISE));
        c_give=LoadCursor(instance,MAKEINTRESOURCE(IDCU_GIVE));
        c_use=LoadCursor(instance,MAKEINTRESOURCE(IDCU_USE));
        c_usewith=LoadCursor(instance,MAKEINTRESOURCE(IDCU_USEWITH));
        c_swap=LoadCursor(instance,MAKEINTRESOURCE(IDCU_SWAP));
        c_sell=LoadCursor(instance,MAKEINTRESOURCE(IDCU_SELL));
        c_buy=LoadCursor(instance,MAKEINTRESOURCE(IDCU_BUY));
        c_look=LoadCursor(instance,MAKEINTRESOURCE(IDCU_LOOK));
        c_set=LoadCursor(instance,MAKEINTRESOURCE(IDCU_SET));
        c_spell=LoadCursor(instance,MAKEINTRESOURCE(IDCU_SPELL));
        c_pix=LoadCursor(instance,MAKEINTRESOURCE(IDCU_PIX));
        c_say=LoadCursor(instance,MAKEINTRESOURCE(IDCU_SAY));
        c_junk=LoadCursor(instance,MAKEINTRESOURCE(IDCU_JUNK));
        c_get=LoadCursor(instance,MAKEINTRESOURCE(IDCU_GET));

        whitecolor      =rgb2scr[IRGB(31,31,31)];
        lightgraycolor  =rgb2scr[IRGB(28,28,28)];
        graycolor       =rgb2scr[IRGB(22,22,22)];
        darkgraycolor   =rgb2scr[IRGB(15,15,15)];
        blackcolor      =rgb2scr[IRGB(0,0,0)];

        lightredcolor   =rgb2scr[IRGB(31,0,0)];
        redcolor        =rgb2scr[IRGB(22,0,0)];
        darkredcolor    =rgb2scr[IRGB(15,0,0)];

        lightgreencolor =rgb2scr[IRGB(0,31,0)];
        greencolor      =rgb2scr[IRGB(0,22,0)];
        darkgreencolor  =rgb2scr[IRGB(0,15,0)];

        lightbluecolor  =rgb2scr[IRGB(5,15,31)];
        bluecolor       =rgb2scr[IRGB(3,10,22)];
        darkbluecolor   =rgb2scr[IRGB(1,5,15)];

        lightorangecolor=rgb2scr[IRGB(31,20,16)];
        orangecolor     =rgb2scr[IRGB(31,16,8)];
        darkorangecolor =rgb2scr[IRGB(15,8,4)];

        textcolor       =rgb2scr[IRGB(27,22,22)];

        healthcolor     =lightredcolor;//rgb2scr[IRGB(31,5,0)];
        manacolor       =lightbluecolor;//rgb2scr[IRGB(10,10,31)];
        endurancecolor  =rgb2scr[IRGB(31,31,5)];
        shieldcolor     =rgb2scr[IRGB(31,15,5)];

        // dots
        dot=xmalloc(MAX_DOT*sizeof(DOT),MEM_GUI);
        set_dot(DOT_TL          ,0      ,0      ,0);
        set_dot(DOT_BR          ,800    ,600   ,0);
        set_dot(DOT_WEA         ,180    ,20     ,DOTF_H|DOTF_V);
        set_dot(DOT_INV         ,660    ,458    ,DOTF_H|DOTF_V);
        set_dot(DOT_CON         ,20     ,458    ,DOTF_H|DOTF_V);
        set_dot(DOT_SCL         ,160+5  ,0      ,DOTF_V);
        set_dot(DOT_SCR         ,640-5  ,0      ,DOTF_V);
        set_dot(DOT_SCU         ,0      ,445    ,DOTF_H);
        set_dot(DOT_SCD         ,0      ,590    ,DOTF_H);
        set_dot(DOT_TXT         ,230    ,438    ,DOTF_H|DOTF_V);
        set_dot(DOT_MTL         ,0      ,39     ,DOTF_H);
        set_dot(DOT_MBR         ,800    ,437    ,DOTF_H);
        set_dot(DOT_SKL         ,8      ,438+5  ,DOTF_V);
        set_dot(DOT_GLD         ,195    ,580    ,DOTF_H|DOTF_V);
        set_dot(DOT_JNK         ,610    ,580    ,DOTF_H|DOTF_V);
        set_dot(DOT_MOD         ,181    ,438+15 ,DOTF_H|DOTF_V);

        // buts
        but=xmalloc(MAX_BUT*sizeof(BUT),MEM_GUI);

        set_but(BUT_MAP,800/2,270,0,BUTID_MAP,0,BUTF_NOHIT);

        for (i=0; i<12; i++) set_but(BUT_WEA_BEG+i,dot[DOT_WEA].x+i*FDX,dot[DOT_WEA].y+0,40,BUTID_WEA,0,0);
        for (x=0; x<4; x++) for (y=0; y<4; y++) set_but(BUT_INV_BEG+x+y*4,dot[DOT_INV].x+x*FDX,dot[DOT_INV].y+y*FDX,40,BUTID_INV,0,0);
        for (x=0; x<4; x++) for (y=0; y<4; y++) set_but(BUT_CON_BEG+x+y*4,dot[DOT_CON].x+x*FDX,dot[DOT_CON].y+y*FDX,40,BUTID_CON,0,0);
        for (i=0; i<16; i++) set_but(BUT_SKL_BEG+i,dot[DOT_SKL].x,dot[DOT_SKL].y+i*LINEHEIGHT,40,BUTID_SKL,0,0);

        set_but(BUT_SCL_UP,dot[DOT_SCL].x+0,dot[DOT_SCU].y+0 ,30,BUTID_SCL, 0,0);
        set_but(BUT_SCL_TR,dot[DOT_SCL].x+0,dot[DOT_SCU].y+10,40,BUTID_SCL, 0,BUTF_CAPTURE|BUTF_MOVEEXEC);
        set_but(BUT_SCL_DW,dot[DOT_SCL].x+0,dot[DOT_SCD].y+0 ,30,BUTID_SCL, 0,0);

        set_but(BUT_SCR_UP,dot[DOT_SCR].x+0,dot[DOT_SCU].y+0 ,30,BUTID_SCR, 0,0);
        set_but(BUT_SCR_TR,dot[DOT_SCR].x+0,dot[DOT_SCU].y+10,40,BUTID_SCR, 0,BUTF_CAPTURE|BUTF_MOVEEXEC);
        set_but(BUT_SCR_DW,dot[DOT_SCR].x+0,dot[DOT_SCD].y+0 ,30,BUTID_SCR, 0,0);

        set_but(BUT_GLD,dot[DOT_GLD].x+0,dot[DOT_GLD].y+0 ,30,BUTID_GLD,    0,BUTF_CAPTURE);

        set_but(BUT_JNK,dot[DOT_JNK].x+0,dot[DOT_JNK].y+0 ,30,BUTID_JNK,    0,0);

        set_but(BUT_MOD_WALK0,dot[DOT_MOD].x+1*14,dot[DOT_MOD].y+0*30,30,BUTID_MOD,0,0);
        set_but(BUT_MOD_WALK1,dot[DOT_MOD].x+0*14,dot[DOT_MOD].y+0*30,30,BUTID_MOD,0,0);
        set_but(BUT_MOD_WALK2,dot[DOT_MOD].x+2*14,dot[DOT_MOD].y+0*30,30,BUTID_MOD,0,0);
        set_but(BUT_MOD_COMB0,dot[DOT_MOD].x+1*10,dot[DOT_MOD].y+1*30,30,BUTID_MOD,0,0);
        set_but(BUT_MOD_COMB1,dot[DOT_MOD].x+0*10,dot[DOT_MOD].y+1*30,30,BUTID_MOD,0,0);
        set_but(BUT_MOD_COMB2,dot[DOT_MOD].x+2*10,dot[DOT_MOD].y+1*30,30,BUTID_MOD,0,0);

        // tin
        /*init_tin(TIN_TEXT,256,dot[DOT_TXT].x,dot[DOT_TXT].y+LINEHEIGHT*MAXLINESHOW,MAXLINEWIDTH,textcolor,DD_LARGE_CHARSET,64);
        init_tin(TIN_GOLD,256,80,20,24,textcolor,"0123456789.",1);
        curtin=TIN_TEXT;*/

        // other
        set_invoff(0,0);
        set_skloff(0,0);
        set_conoff(0,0);
        capbut=-1;

        // more inits
#ifdef EDITOR
        if (editor) init_editor();
#endif
        init_game();

        return 0;
}

void main_exit(void)
{
        xfree(dot);
        dot=NULL;
        xfree(but);
        but=NULL;
        xfree(skltab);
        skltab=NULL;
        skltab_max=0;
        skltab_cnt=0;

        // more exits
#ifdef EDITOR
        if (editor) exit_editor();
#endif
        //exit_tin();
        exit_game();
}

void flip_at(unsigned int t)
{
	MSG msg;
        unsigned int tnow;

        do {
                while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		tnow=GetTickCount();
		if (dd_windowed && GetActiveWindow()!=mainwnd) {
			Sleep(100);
		} else Sleep(1);
#ifdef DOSOUND
		sound_mood();
#endif
	} while (t>tnow);

        dd_flip();	
}

unsigned int nextframe;

int main_loop(void)
{
        MSG msg;
        int tmp,timediff,flag,ltick=0;
        unsigned int utmp;
	extern int q_size;

        nextframe=GetTickCount()+MPT;

        while (!quit) {

                now=GetTickCount();

                poll_network();

#ifdef DOSOUND
		sound_gc();
#endif

		// check if we can go on
                if (sockstate>2) {
                        // prefetch as much as possible
                        while (next_tick()) ;	//prefetch_game(tick+q_size);

                        // get one tick to display
                        do_tick(); ltick++;
			//if (ltick%(TICKS*5)==0) cmd_ping();

                        if (ltick==TICKS*10) {
				struct client_info ci;

				dd_get_client_info(&ci);
				ci.idle=100*idle/tota;
				ci.skip=100*skip/tota;
				cl_client_info(&ci);				
			}
			
                        if (sockstate==4 && ltick%TICKS==0) {
				cl_ticker();
			}			
                }

                if (sockstate==4) timediff=nextframe-GetTickCount(); else timediff=1;

                if (dd_islost()) {
                        while (42) {
                                while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                                        TranslateMessage(&msg);
                                        DispatchMessage(&msg);
                                }

                                utmp=GetTickCount();
                                if (nextframe<=utmp) break;     // while (nextframe>GetTickCount());
                                Sleep(min(50,nextframe-utmp));

                        }

                        dd_restore();
                }
                else {
                        if (timediff>-MPT/2) {

                                set_cmd_states();
				
                                display();				

                                timediff=nextframe-GetTickCount();
                                if (timediff>0) idle+=timediff; else skip-=timediff;

				frames++;

                                flip_at(nextframe);				
                        }
                        else {
                                skip-=timediff;

                                while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                                        TranslateMessage(&msg);
                                        DispatchMessage(&msg);
                                }								
                        }
                }		

		if (lasttick+q_size>0) tmp=MPT*12/(lasttick+q_size);
		//if (lasttick+q_size>0) tmp=MPT-MPT/10;
                else tmp=MPT+MPT/10;

                //note("tmp=%d, size=%d",tmp,lasttick+q_size);
		
                nextframe+=tmp;
                tota+=tmp;
                if (tick%24==0) { tota/=2; skip/=2; idle/=2; frames/=2; }//{ tota=2; skip=idle=1; }		
        }

        close_client();

        return 0;
}

