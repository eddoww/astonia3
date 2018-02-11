#define FDX             40
#define FDY             20

#define SPR_WALK        11

#define SPR_FFIELD      10
#define SPR_FIELD       12

#define SPR_ITPAD       13
#define SPR_ITSEL       14

#define SPR_SCRUP       27 // 20 // 17
#define SPR_SCRLT       28 // 21 // 19
#define SPR_SCRDW       29 // 23 // 18
#define SPR_SCRRT       30 // 22 // 19
#define SPR_SCRBAR      26

#define SPR_RAISE       19
#define SPR_CLOSE       36
#define SPR_TEXTF       35

#define SPR_GOLD_BEG    100
#define SPR_GOLD_END    109

#define GND_LAY         100
#define GND2_LAY        101
#define GNDSHD_LAY      102
#define GNDSTR_LAY      103
#define GNDTOP_LAY      104
#define GNDSEL_LAY      105
#define GME_LAY         110
#define GME_LAY2        111
#define GMEGRD_LAYADD   500
#define TOP_LAY         1000

int is_top_sprite(int sprite, int itemhint);
int is_cut_sprite(int sprite);
int is_mov_sprite(int sprite, int itemhint);
int is_door_sprite(int sprite);
int is_yadd_sprite(int sprite);
int get_chr_height(int csprite);
int get_lay_sprite(int sprite,int lay);

int get_player_sprite(int nr, int zdir, int action, int step, int duration);

/*
void trans_gsprite(int mn, struct map *cmap, int attick);
void trans_isprite(int mn, struct map *cmap, int attick);
void trans_fsprite(int mn, struct map *cmap, int attick);
*/
int trans_asprite(int mn, int sprite, int attick,
		  unsigned char *pscale,
		  unsigned char *pcr,
		  unsigned char *pcg,
		  unsigned char *pcb,
		  unsigned char *plight,
		  unsigned char *psat,
		  unsigned short *pc1,
		  unsigned short *pc2,
		  unsigned short *pc3,
		  unsigned short *pshine);
void trans_csprite(int mn, struct map *cmap, int attick);
int trans_charno(int csprite,int *pscale,int *pcr,int *pcg,int *pcb,int *plight,int *psat,int *pc1,int *pc2,int *pc3,int *pshine);
int additional_sprite(int sprite,int attick);
int no_alpha_sprite(int sprite);
int get_offset_sprite(int sprite,int *px,int *py);
