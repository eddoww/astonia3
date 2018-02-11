#ifdef EDITOR

typedef struct eflag
{
        char letter[2];
        char name[64];
        unsigned int mask;
}
EFLAG;

#define MAX_EFLAG       17
extern EFLAG eflags[];

#define EMODE_FLAG      0
#define EMODE_SPRITE    (MAX_EFLAG)
#define EMODE_ITM       (MAX_EFLAG+4+0)
#define EMODE_CHR       (MAX_EFLAG+4+1)
#define MAX_EMODE       (MAX_EFLAG+4+2)

#define MF_MOVEBLOCK    (1<<0)
#define MF_SIGHTBLOCK   (1<<1)
#define MF_INDOORS      (1<<2)
#define MF_RESTAREA     (1<<3)
#define MF_SOUNDBLOCK	(1<<4)
#define MF_SHOUTBLOCK	(1<<5)
#define MF_CLAN		(1<<6)
#define MF_ARENA	(1<<7)
#define MF_PEACE	(1<<8)
#define MF_NEUTRAL	(1<<9)
#define MF_FIRETHRU	(1<<10)
#define MF_SLOWDEATH	(1<<11)
#define MF_NOLIGHT	(1<<12)
#define MF_NOMAGIC	(1<<13)
#define MF_UNDERWATER	(1<<14)
#define MF_NOREGEN	(1<<15)
#define MF_RES2		(1<<16)

#define EXF_BRIGHT      (1<<0)
#define EXF_MASK        (1<<1)  // only valid in cmf (clipboard map)

#define EMAXUNI         40
#define EMAXNAME        40
#define EMAXDESC        80
#define EMAXFNAME       256

struct ifile; typedef struct ifile IFILE;
struct cfile; typedef struct cfile CFILE;
struct mfile; typedef struct mfile MFILE;

struct eitem
{
        IFILE *ifile;

        char uni[EMAXUNI];
        char name[EMAXNAME];
        char desc[EMAXDESC];
        int sprite;
};
typedef struct eitem EITEM;

struct echar
{
        CFILE *cfile;

        char uni[EMAXUNI];
        char name[EMAXNAME];
        char desc[EMAXDESC];
        int sprite;
        char dir;
};
typedef struct echar ECHAR;

struct ifile
{
        char filename[EMAXFNAME];
        int loaded;

        int icount;
        EITEM **ilist;
};
typedef struct ifile IFILE;

struct cfile
{
        char filename[EMAXFNAME];
        int loaded;

        int ccount;
        ECHAR **clist;
};
typedef struct cfile CFILE;

struct mfield
{
        unsigned int flags;

        int sprite[4];

        EITEM *itm;
        ECHAR *chr;

        // editor extensions
        unsigned int ex_flags;

};
typedef struct mfield MFIELD;

struct mundo
{
        int uid,x,y;
        MFIELD field;
};
typedef struct mundo MUNDO;

struct mfile
{
        char filename[EMAXFNAME];
        int loaded,modified;

        int width;
        int height;
        MFIELD *field;

        int offsetx,offsety;

        int uid,ubeg,uend,umax;
        MUNDO *mundo;
};
typedef struct mfile MFILE;

extern int ifilecount;
extern IFILE **ifilelist;

extern int cfilecount;
extern CFILE **cfilelist;

extern int mfilecount;
extern MFILE **mfilelist;

void find_itm_files(char *searchpath);
void find_chr_files(char *searchpath);
void find_map_files(char *searchpath);
//void find_int_files(char *searchpath);
void find_pre_files(char *searchpath);
EITEM *find_item(char *uni);
void load_itm_file(IFILE *ifile);
void load_chr_file(CFILE *cfile);
void load_map_file(MFILE *mfile);
void save_map_file(MFILE *mfile);
void free_map_file(MFILE *mfile);

void mfile_inc_undo(MFILE *mfile);
void mfile_set_undo(MFILE *mfile, int sel);
void mfile_use_undo(MFILE *mfile);

extern struct map *emap;
extern int emapdx,emapdy;

// intelligence

#define ITYPE_GROUND    1
#define ITYPE_LINEWALL  2
#define ITYPE_CHANCE    3
#define ITYPE_PRESET    4

struct itype_generic
{
        int type;
        char name[EMAXNAME];
        int flags;
        char affect;
        char emode[MAX_EMODE];
};

struct itype_ground
{
        int type;
        char name[EMAXNAME];
        int flags;
        char affect;
        char emode[MAX_EMODE];
        // snip
        int gridx;
        int gridy;
        int *sprite;
};

struct itype_linewall
{
        int type;
        char name[EMAXNAME];
        int flags;
        char affect;
        char emode[MAX_EMODE];
        // snip
        int len;
        int *sprite;
};

struct itype_chance
{
        int type;
        char name[EMAXNAME];
        int flags;
        char affect;
        char emode[MAX_EMODE];
        // snip
        int len;
        int *sprite;
        int *prob;
        int probsum;
};

struct itype_preset
{
        int type;
        char name[EMAXNAME];
        int flags;
        char affect;
        char emode[MAX_EMODE];
        // snip
        MFIELD field;
};

union itype
{
        struct itype_generic generic;
        struct itype_ground ground;
        struct itype_linewall linewall;
        struct itype_chance chance;
        struct itype_preset preset;
};

typedef union itype ITYPE;

struct pfile
{
        char filename[EMAXFNAME];
        int loaded;

        int pcount;
        ITYPE **plist;
};
typedef struct pfile PFILE;

extern int pfilecount;
extern PFILE **pfilelist;
void load_pre_file(PFILE *pfile);
void free_pre_files(void);
int get_itype(char *str);
char *get_iname(int type);


// preset
/*
struct preset
{
        char name[EMAXNAME];
        char emode[MAX_EMODE];
        MFIELD field;
};

typedef struct preset PRESET;

struct pfile
{
        char filename[EMAXFNAME];
        int loaded;

        int pcount;
        PRESET *preset_list;    // note that this is * not **
};
typedef struct pfile PFILE;

extern int pfilecount;
extern PFILE **pfilelist;
void load_pre_file(PFILE *pfile);
void free_pre_files(void);
*/

#endif
