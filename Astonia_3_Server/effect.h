/*

$Id: effect.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: effect.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:12  sam
Added RCS tags


*/

#define EF_NONE		0
#define EF_FIREBALL	1
#define EF_MAGICSHIELD	2
#define EF_BALL		3
#define EF_STRIKE	4	// helper for lightning ball and flash
#define EF_FLASH	5

#define EF_EXPLODE	7	// fireball explosion
#define EF_WARCRY	8
#define EF_BLESS	9
#define EF_FREEZE	10
#define EF_HEAL		11
#define EF_BURN		12
#define EF_MIST		13
#define EF_POTION	14
#define EF_EARTHRAIN	15
#define EF_EARTHMUD	16
#define EF_EDEMONBALL	17
#define EF_CURSE	18
#define EF_CAP		19
#define EF_LAG		20
#define EF_PULSE	21
#define EF_PULSEBACK	22
#define EF_FIRERING	23
#define EF_BUBBLE	24

extern int used_effects;

int init_effect(void);
void tick_effect(void);

void free_effect(int fn);
int remove_effect(int fn);

int create_fireball(int cn,int frx,int fry,int tox,int toy,int str);
int create_ball(int cn,int frx,int fry,int tox,int toy,int str);
int create_show_effect(int type,int cn,int start,int stop,int light,int strength);
int create_flash(int cn,int str,int duration);
int ishit_fireball(int cn,int frx,int fry,int tox,int toy,int (*isenemy)(int,int));
int calc_steps_ball(int cn,int frx,int fry,int tox,int toy);
int create_explosion(int maxage,int base);
int add_explosion(int fn,int x,int y);
void destroy_chareffects(int cn);
int create_earthrain(int cn,int x,int y,int strength);
int create_earthmud(int cn,int x,int y,int strength);
int create_mist(int x,int y);
int edemonball_map_block(int m);
int create_edemonball(int cn,int frx,int fry,int tox,int toy,int str,int base);
int remove_effect_char(int fn);
void create_pulse(int x,int y,int str);
int destroy_effect_type(int cn,int type);
int create_map_effect(int type,int x,int y,int start,int stop,int light,int strength);
