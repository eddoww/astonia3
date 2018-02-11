#define INVDX                   4
#define INVDY                   4
#define CONDX                   4
#define CONDY                   4
#define SKLDY                   16
#define SKLWIDTH                145
#define LINEHEIGHT              10

#define FX_ITEMLIGHT            DDFX_NLIGHT
#define FX_ITEMBRIGHT           DDFX_BRIGHT
// #define FX_ITEMLIGHT_SB         DDFX_NLIGHT
// #define FX_ITEMBRIGHT_SB        DDFX_BRIGHT

// #define MAXQUICK                1301    // COMPARE WITH QUICK.C - we have one more and set all invalid access to this (so MAXQUICK is really MAXQUICK - think abbout it ;-)

struct quicks
{
        int mn[9];      // 0 for invalid neighbours
        int qi[9];      // maxqick for invalid neighbours
        int mapx;
        int mapy;
        int cx;
        int cy;
};

typedef struct quicks QUICK;

extern unsigned short int healthcolor,manacolor,endurancecolor,shieldcolor;
extern unsigned short int whitecolor,lightgraycolor,graycolor,darkgraycolor,blackcolor;
extern unsigned short int lightredcolor,redcolor,darkredcolor;
extern unsigned short int lightgreencolor,greencolor,darkgreencolor;
extern unsigned short int lightbluecolor,bluecolor,darkbluecolor;
extern unsigned short int lightorangecolor,orangecolor,darkorangecolor;
extern unsigned short int textcolor;


#define DOT_TL          0
#define DOT_BR          1
#define DOT_WEA         2
#define DOT_INV         3
#define DOT_CON         4
#define DOT_SCL         5
#define DOT_SCR         6
#define DOT_SCU         7
#define DOT_SCD         8
#define DOT_TXT         9
#define DOT_MTL         10
#define DOT_MBR         11
#define DOT_SKL         12
#define DOT_GLD         13
#define DOT_JNK         14      // trashcan
#define DOT_MOD         15      // combat and walk
#define MAX_DOT         16

#define DOTF_V          (1<<0)  // for display only (displays a VERTICAL line)
#define DOTF_H          (1<<1)  // for display only (displays a HORIZONTAL line)

struct dot
{
        int flags;

        int x;
        int y;
};

typedef struct dot DOT;         // dot 0 is top left, dot 1 is bottom right of the screen

#define BUTID_MAP       0
#define BUTID_WEA       1
#define BUTID_INV       2
#define BUTID_CON       3
#define BUTID_SCL       4
#define BUTID_SCR       5
#define BUTID_SKL       6
#define BUTID_GLD       7
#define BUTID_JNK       8
#define BUTID_MOD       9
#define BUTID_TELE	10

#define BUT_MAP         0
#define BUT_WEA_BEG     1
#define BUT_WEA_END     12
#define BUT_INV_BEG     13
#define BUT_INV_END     28
#define BUT_CON_BEG     29
#define BUT_CON_END     44
#define BUT_SCL_UP      45
#define BUT_SCL_TR      46
#define BUT_SCL_DW      47
#define BUT_SCR_UP      48
#define BUT_SCR_TR      49
#define BUT_SCR_DW      50
#define BUT_SKL_BEG     51
#define BUT_SKL_END     66
#define BUT_GLD         67
#define BUT_JNK         68
#define BUT_MOD_WALK0   69
#define BUT_MOD_WALK1   70
#define BUT_MOD_WALK2   71
#define BUT_MOD_COMB0   72
#define BUT_MOD_COMB1   73
#define BUT_MOD_COMB2   74
#define BUT_TEL		75
#define BUT_HELP_NEXT	76
#define BUT_HELP_PREV	77
#define BUT_HELP_MISC	78
#define BUT_HELP_CLOSE	79
#define BUT_EXIT	80
#define BUT_HELP	81
#define BUT_NOLOOK	82
#define BUT_COLOR	83
#define BUT_SKL_LOOK	84
#define BUT_QUEST	85
#define MAX_BUT         86

#define BUTF_NOHIT      (1<<1)  // button is ignored int hit processing
#define BUTF_CAPTURE    (1<<2)  // button captures mouse on lclick
#define BUTF_MOVEEXEC   (1<<3)  // button calls cmd_exec(lcmd) on mousemove
#define BUTF_RECT       (1<<4)  // editor - button is a rectangle

struct but
{
        int flags;      // flags

        int id;         // something an application can give a button, but it need not ;-)
        int val;        // something an application can give a button, but it need not ;-)

        int x;          // center x coordinate - or left if button is a RECT
        int y;          // center y coordinate - or top if button is a RECT
        int dx;         // width of a rect button
        int dy;         // height of a rect button

        int sqhitrad;   // hit (square) radius of this button
};

typedef struct but BUT;

extern int mapaddx,mapaddy;
extern int winxres,winyres;

extern DOT *dot;
extern BUT *but;

void set_mapoff(int cx, int cy, int mdx, int mdy);
void set_mapadd(int addx, int addy);
void mtos(int mapx, int mapy, int *scrx, int *scry);
void stom(int scrx, int scry, int *mapx, int *mapy);
void update_user_keys(void);
void cmd_proc(int key);

// tin

#define TIN_NONE        0
#define TIN_TEXT        1
#define TIN_GOLD        2
#define MAX_TIN         3

struct tin
{
        const char *set;        // take care not to give me something from the stack ;-)
        char *buf;
        int max;
        int pos;
        int sx;
        int sy;
        int dx;
        unsigned short int color;
        int maxhistory;
        char *history;
        int curhistory;         // position where to copy the command
        int runhistory;         // current selection in history
};

typedef struct tin TIN;

/*#define TIN_NONE        0
#define TIN_TEXT        1
#define TIN_GOLD        2
#define MAX_TIN         3

#define TIN_DO_ADD      0
#define TIN_DO_BS       1
#define TIN_DO_DEL      2
#define TIN_DO_ENTER    3
#define TIN_DO_HOME     4
#define TIN_DO_END      5
#define TIN_DO_MOVE     6
#define TIN_DO_SEEK     7
#define TIN_DO_CLEAR    8
#define TIN_DO_NEXTHIST 9
#define TIN_DO_PREVHIST 10

void display_tin(int t, int dd_frame);
void tin_do(int t, int tindo, char c);*/

#define CMD_RETURN	256
#define CMD_DELETE	257
#define CMD_BACK	258
#define CMD_LEFT	259
#define CMD_RIGHT	260
#define CMD_HOME	261
#define CMD_END		262
#define CMD_UP		263
#define CMD_DOWN	264

