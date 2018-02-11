/*
 * $Id: tool.h,v 1.11 2008/04/11 10:55:28 devel Exp $
 *
 * $Log: tool.h,v $
 * Revision 1.11  2008/04/11 10:55:28  devel
 * added whotutor
 *
 * Revision 1.10  2008/01/03 10:11:46  devel
 * *** empty log message ***
 *
 * Revision 1.9  2007/07/28 18:55:57  devel
 * *** empty log message ***
 *
 * Revision 1.8  2007/07/04 09:25:07  devel
 * changed end_cost
 *
 * Revision 1.7  2007/07/02 08:59:26  devel
 * *** empty log message ***
 *
 * Revision 1.6  2007/07/02 08:47:23  devel
 * added IDR_HEAL
 *
 * Revision 1.5  2007/06/22 13:04:55  devel
 * *** empty log message ***
 *
 * Revision 1.4  2007/06/07 15:29:05  devel
 * removed PK
 *
 * Revision 1.3  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.2  2006/06/23 16:21:07  ssim
 * some fixes to teufel PK hooks
 *
 */

// defining NULL ourselves saves including stdio
#ifndef NULL
#define NULL (void*)0
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(n)	(sizeof(n)/sizeof(n[0]))
#endif

// character ID: an (almost) unique ID for NPCs and players. it is persistant over login/logout
#define charID(cn)	((ch[cn].ID) ? (ch[cn].ID|0x80000000) : (ch[cn].serial&0x7fffffff))
#define charID_ID(ID)	(ID|0x80000000)

#define END_COST	(POWERSCALE/4)
#define end_cost(a)	(END_COST)

#if __GNUC_MINOR__==8
unsigned long long atoll(char *string);
#endif
int endcmp(char *a,char *b);
int speed(int speedy,int mode,int ticks);
int dx2offset(int dir,int *x,int *y,int *diag);
int offset2dx(int frx,int fry,int tox,int toy);

int can_attack(int cn,int co);
int can_wear(int cn,int in,int pos);
int can_carry(int cn,int in,int quiet);
int get_attack_skill(int cn);
int get_surround_attack_skill(int cn);
int get_parry_skill(int cn);
int die(int cnt,int sides);

int exp2level(int val);
int level2exp(int level);
void give_exp(int cn,int val);
void give_exp_bonus(int cn,int val);	// include learning clan bonus
const char *hisname(int cn);
const char *Hisname(int cn);
const char *hename(int cn);
const char *Hename(int cn);
const char *himname(int cn);

void remove_spell(int cn,int in,int pos,int cserial,int iserial);
void heal_tick(int cn,int in,int pos,int cserial,int iserial);
int may_add_spell(int cn,int driver);

int look_item(int cn,struct item *in);
int look_char(int cn,int co);
int bigdir(int dir);
int is_facing(int cn,int co);
int is_back(int cn,int co);
int immunity_reduction(int caster,int subject,int skill_strength);
int create_money_item(int amount);
int destroy_money_item(int in);
char *strcasestr(const char *haystack,const char *needle);
int create_special_item(int strength,int potionprob,int maxchance);
int create_spell_timer(int cn,int in,int pos);
int set_army_rank(int cn,int rank);
int store_citem(int cn);
char *save_number(int nr);
int get_army_rank_int(int cn);
char *get_army_rank_string(int cn);
double get_spell_average(int cn);
int lowhi_random(int val);
int edemon_reduction(int cn,int str);
void look_values_bg(int cnID,int coID);
int is_visible(int cn,int co);
int check_requirements(int cn,int in);
int can_help(int cn,int co);
int add_same_spell(int cn,int driver);
int level_value(int level);
int freeze_value(int cn,int co);
int warcry_value(int cn,int co,int pwr);
void destroy_char(int cn);
void remove_destroy_char(int cn);
void give_military_pts(int cn,int co,int pts,int exps);
int create_drop_char(char *name,int x,int y);
char *lower_case(char *src);
int check_light(int x,int y);
int check_dlight(int x,int y);
void give_military_pts_no_npc(int co,int pts,int exps);
int give_char_item(int cn,int in);
const char *Sirname(int cn);
int create_orb(void);
int take_money(int cn,int val);
int fireball_damage(int cn,int co,int str);
int strike_damage(int cn,int co,int str);
void set_item_requirements(int in);
int del_hate_ID(int cn,int uID);
int del_all_hate(int cn);
int is_hate_empty(int cn);
int get_lifeshield_max(int cn);
void set_legacy_item_lvl(int in,int cn);
void tabunga(int cn,int co,char *ptr);
void buggy_items(int cn);
int warcry_damage(int cn,int co,int pwr);
int add_spell(int cn,int driver,int duration,char *name);
void bondtake_item(int in,int cn);
void bondwear_item(int in,int cn);
int cnt_free_inv(int cn);
int spellpower(int cn,int v);
void update_item(int in);
void do_whostaff(int cnID);
void do_whotutor(int cnID);
void lollipop_bg(int cnID,int coID);
void player_use_potion(int cn);
void player_use_recall(int cn);
int level2maxitem(int level);
int create_orb2(int what);
void shutdown_warn(void);
int check_levelup(int cn);
int check_can_wear_item(int cn);
void shutdown_bg(int t,int down);
void give_money(int cn,int val,char *reason);
int count_enhancements(int in);
int calculate_complexity(int in);
double round(double);
char *bignumber(int val);
int get_complexity_cost(int in);
int set_item_name(int in);
void kick_char_by_sID(int sID);
void heal_someone(int cn,int str_heal,int str_end,int str_mana);
int has_spell(int cn,int driver);
int store_item(int cn,int in);
void update_fix_item(int in,int cn);










