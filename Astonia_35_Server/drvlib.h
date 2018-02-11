/*
 * $Id: drvlib.h,v 1.20 2008/04/11 10:54:50 devel Exp $
 *
 * $Log: drvlib.h,v $
 * Revision 1.20  2008/04/11 10:54:50  devel
 * added arkhata drivers
 *
 * Revision 1.19  2008/02/04 10:12:55  devel
 * added sign driver
 *
 * Revision 1.18  2007/09/21 11:02:38  devel
 * gatekeeper door
 *
 * Revision 1.17  2007/08/09 11:14:57  devel
 * *** empty log message ***
 *
 * Revision 1.16  2007/07/27 06:00:42  devel
 * fire and lball delay spells
 *
 * Revision 1.15  2007/07/24 18:58:35  devel
 * rww keys stackable
 *
 * Revision 1.14  2007/07/02 08:47:23  devel
 * added IDR_HEAL
 *
 * Revision 1.13  2007/06/11 10:07:04  devel
 * removed beyond potion driver
 *
 * Revision 1.12  2007/06/08 12:09:07  devel
 * changed orbspawn -> explore
 *
 * Revision 1.11  2007/06/07 15:15:54  devel
 * added caligar stuff
 *
 * Revision 1.10  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 * Revision 1.9  2006/12/14 13:58:05  devel
 * added xmass special
 *
 * Revision 1.8  2006/06/28 23:06:35  ssim
 * added PK arena exit
 *
 * Revision 1.7  2006/06/26 18:14:11  ssim
 * added logic for rat nest quest
 *
 * Revision 1.6  2006/06/20 13:27:40  ssim
 * added arena door driver
 *
 * Revision 1.5  2005/12/07 13:36:18  ssim
 * added demon gambler
 *
 * Revision 1.4  2005/11/27 18:06:06  ssim
 * added islena's door driver
 *
 * Revision 1.3  2005/11/22 12:16:46  ssim
 * added stackable (demon) chips
 *
 * Revision 1.2  2005/11/21 15:19:25  ssim
 * added teufeldemon driver
 *
 */

// driver numbers get defined here

// character drivers
#define CDR_TUTOR		3
#define CDR_LOSTCON		5	// driver for a player character who lost his connection
#define CDR_MERCHANT		6	// generic merchant driver
#define CDR_SIMPLEBADDY		7	// generic aggressive NPC driver
#define CDR_GWENDYLON		8	// specific quest giver, area1, knight castle
#define CDR_YOAKIN		9	// specific quest giver, area1, knight castle
#define CDR_BALLTRAP		10	// specific enemy, area1, wood
#define CDR_TERION		11	// specific NPC in area1, village
#define CDR_JAMES		12	// specific NPC in area1, knight castle

//#define CDR_GEREWIN		14	// specific NPC in area1, knight castle
#define CDR_NOOK		15	// specific NPC in area1, knight castle
#define CDR_LYDIA		16	// specific NPC in area1, knight castle
#define CDR_ROBBER		17	// specific enemy in area1, wood
#define CDR_SANOA		18	// specific NPC (enemy) in area1, city
#define CDR_ASTURIN		19	// specific NPC (enemy) in area1, city
#define CDR_RESKIN		20	// specific NPC in area1, city
#define CDR_GUIWYNN		21	// specific quest giver, area1, city
#define CDR_BANK		22	// generic bank driver
#define CDR_LOGAIN		23	// specific quest giver, area1, city
#define CDR_SEYMOUR		24	// specific quest giver, area3, aston
#define CDR_LAMPGHOST		25	// specific lamp extinguisher, area 3, palace
#define CDR_SUPERIOR		26	// superior zombie driver in area 2
#define CDR_CLANMASTER		27	// clan foundations co, ltd.
#define CDR_CLANCLERK		28	// clan administrations co, ltd.
#define CDR_CLANGUARD		29	// clan defense co, ltd.
#define CDR_KELLY		30	// NPC in area 3
#define CDR_MOONIE		31	// stop to eat spiders
#define CDR_ASTRO1		32	// tells wild stories about the moon
#define CDR_ASTRO2		33	// gives moonie-quest in area 2
#define CDR_VAMPIRE		34	// special NPC in area 2
#define CDR_THOMAS		35	// gives moonie-quest in area 2
#define CDR_SIRJONES		36	// gives moonie-quest in area 2
#define CDR_MACRO		37	// macro daemon driver
#define CDR_VAMPIRE2		38	// special NPC in area 2
#define CDR_GATE_WELCOME	39	// gatekeeper1
#define CDR_GATE_FIGHT		40	// gatekeeper2
#define CDR_CLANROBBER		41	// clan attacks corp, ltd.
#define CDR_MILITARY_MASTER	42	// random quest giver
#define CDR_MILITARY_ADVISOR	43	//
#define CDR_FDEMON_ARMY		44	// good guys in fire area
#define CDR_FDEMON_BOSS		45	// mission giver in fire area
#define CDR_FDEMON_DEMON	46	// bad guys in fire area
#define CDR_PALACEGUARD		47	//
#define CDR_ARENAMASTER		48	//
#define CDR_ARENAFIGHTER	49	//
#define CDR_ARENAMANAGER	50	//
#define CDR_DUNGEONMASTER	51	//
#define CDR_DUNGEONFIGHTER	52	//
#define CDR_RANDOMMASTER	53	//
#define CDR_SWAMPCLARA		54	// swamp quest giver
#define CDR_PROFESSOR		55	// generic profession teacher driver
#define CDR_SWAMPMONSTER	56	// swamp
#define CDR_PALACEISLENA	57	// palace
#define CDR_FORESTIMP		58	// forest quest giver
#define CDR_FORESTMONSTER	59	// forest
#define CDR_FORESTWILLIAM	60	// forest quest giver
#define CDR_FORESTHERMIT	61	// forest quest giver
#define CDR_TWOGUARD		62	// two cities: guards
#define CDR_TWOBARKEEPER	63	// two cities: barkeeper
#define CDR_PENTER		64	// pents
#define CDR_TWOSERVANT		65	// two cities: servant in forbidden territory
#define CDR_TWOTHIEFGUARD	66	// two cities: thieves guild guard
#define CDR_TWOTHIEFMASTER	67	// two cities: thieves guild master
#define CDR_TWOROBBER		68	// two cities: robber (simple baddy with special death)
#define CDR_TWOSANWYN		69	// two cities: military quest giver
#define CDR_TWOSKELLY		70	// two cities: quest giver
#define CDR_TWOALCHEMIST	71	// two cities: quest giver
#define CDR_TRADER		72	// player trade middleman
#define CDR_NOMAD		73	// nomad: nomad
#define CDR_LQNPC		74	// lq: standard NPC
#define CDR_LQPARSER		75	// lq: parser for LQ commands
#define CDR_MADHERMIT		76	// nomad: mad hermit
#define CDR_TESTER		77	// pents
#define CDR_STRATEGY		78	// strategy: worker/fighter
#define CDR_STRATEGY_PARSER	79	// strategy: parser for commands
#define CDR_STRATEGY_BOSS	80	// strategy: quest npc
#define CDR_RATLING		81	// sewers: ratling
#define CDR_RUBY		82	// sidestory: first quest giver
#define CDR_WARPFIGHTER		83	// warped: fighter
#define CDR_WARPMASTER		84	// warped: master
#define CDR_JANITOR		85	// generic: janitor
#define CDR_SHR_WEREWOLF	86	// shrike: werewolf
#define CDR_WEREWOLF		87	// werewolf: werewolf
#define CDR_SMUGGLECOM		88	// staffer area: smuggler commander
#define CDR_SMUGGLELEAD		89	// staffer area: smuggler leader
#define CDR_CARLOS		90	// gives dragon-breath-quest
#define CDR_COUNTBRAN		91	// staffer2 area: count brannington
#define CDR_COUNTESSABRAN	92	// staffer2 area: countessa brannington
#define CDR_DAUGHTERBRAN	93	// staffer2 area: daughter brannington
#define CDR_SPIRITBRAN		94	// staffer2 area: spirit brannington
#define CDR_GUARDBRAN		95	// staffer2 area: spirit brannington
#define CDR_BRENNETHBRAN	96	// staffer2 area: brenneth brannington
#define CDR_FORESTBRAN		97	// staffer2 area: forester brannington
#define CDR_SUPERMAX		98	// past-maxes-raiser
#define CDR_BROKLIN		99	// staffer3 area
#define CDR_ARISTOCRAT		100	// staffer3 area
#define CDR_YOATIN		101	// staffer3 area
#define CDR_WHITEROBBERBOSS	102	// staffer3 area
#define CDR_DWARFCHIEF		103	// warrmine area: dwarfchief
#define CDR_GRINNICH		104	// staffer2 area: grinnich
#define CDR_SHANRA		105	// staffer2 area: shanra
#define CDR_CENTINEL		106	// staffer2 area: centinel
#define CDR_GOLEMKEYHOLDER	107	// mines: key to second mine area
#define CDR_LOSTDWARF		108	// warrmine area: lost miner
#define CDR_DWARFSHAMAN		109	// warrmine area: shaman
#define CDR_DWARFSMITH		110	// warrmine area: shaman
#define CDR_MISSIONGIVE		111	// mission area: mission giver
#define CDR_MISSIONFIGHT	112	// mission area: mission giver
#define CDR_CLUBMASTER		113	// clan foundations co, ltd.
#define CDR_TEUFELDEMON		114	// teufelheim generic demon
#define CDR_TEUFELGAMBLER	115	// teufelheim gambling demon
#define CDR_TEUFELQUEST		116	// teufelheim quest giver
#define CDR_TEUFELRAT		117	// teufelheim rat driver
#define CDR_CLANVAULT		118

#define CDR_CALIGARGLORI        119
#define CDR_CALIGARARQUIN       120
#define CDR_CALIGARSMITH        121
#define CDR_CALIGARHOMDEN       122
#define CDR_CALIGARGUARD2       123
#define CDR_CALIGARSKELLY       124
#define CDR_CALIGARGUARD        125	// !!! was 118 !!!

#define CDR_ROUVEN		130	// imperial vault
#define CDR_RAMMY		131	// arkhata
#define CDR_JAZ			132	// arkhata
#define CDR_BRIDGEGUARD		133	// arkhata
#define CDR_FIONA		134	// arkhata
#define CDR_GLADIATOR		135	// arkhata
#define CDR_NOP			136	// arkhata
#define CDR_RAMIN		137	// arkhata
#define CDR_ARKHATASKELLY	138	// arkhata
#define CDR_ARKHATAMONK		139	// arkhata
#define CDR_BOOKEATER		140	// arkhata
#define CDR_CAPTAIN		141	// arkhata
#define CDR_JUDGE		142	// arkhata
#define CDR_FORTRESSGUARD	143	// arkhata
#define CDR_JADA		144	// arkhata
#define CDR_POTMAKER		145	// arkhata
#define CDR_HUNTER		146	// arkhata
#define CDR_THAIPAN		147	// arkhata
#define CDR_TRAINER		148	// arkhata
#define CDR_KIDNAPPEE		149	// arkhata
#define CDR_ARKHATACLERK	150	// arkhata
#define CDR_ARKHATAPRISON	151	// arkhata
#define CDR_KRENACH		152	// arkhata


#define CDR_MONK_WORKER         186     // monk nomads plain (the ones to protect)
#define CDR_MONK_GOVIDA         187     // monk nomads plain (end of salt mine quest)
#define CDR_MONK_GATAMA         188     // monk nomads plain (start of salt mine quest)
#define CDR_LAB5DAEMON          189     // lab5: master daemons of the labyrinth
#define CDR_LAB5MAGE            190     // lab5: the mage driver
#define CDR_LAB5SEYAN           191     // lab5: the seyan driver
#define CDR_LAB4SEYAN           192     // lab4: the seyan driver
#define CDR_LAB4GNALB           193     // lab4: the gnalb driver
#define CDR_LAB3PASSGUARD       194     // lab2:
#define CDR_LAB3PRISONER        195     // lab2: the prisoner of lab 3
#define CDR_LAB2HERALD          196     // lab2: herald - manciple(?) of the graveyard
#define CDR_LAB2DEAMON          197     // lab2: deamon - guard of the crypta
#define CDR_LAB2UNDEAD          198     // lab2: undead
#define CDR_LABGNOMEDRIVER	199	// lab1: torch gnome

// item drivers
#define IDR_POTION		1	// healing/mana potion
#define IDR_DOOR		2	// door (closable, lockable)
#define IDR_BALLTRAP		3	// light ball trap

#define IDR_CHEST		5	// treasure chest
#define IDR_USETRAP		6	// step on a tile to use another item

#define IDR_PALACEGATE		9	// gate in area 3
#define IDR_TELEPORT		10	// teleport to drdata[0,1],drdata[2,3] area drdata[4,5], area=0 means no change
#define IDR_NIGHTLIGHT		11	// light which gets turned off during the day
#define IDR_TORCH		12	// torch expiry and on/off
#define IDR_RECALL		13	// recall scroll
	
#define IDR_SHRINE		14	// shrine
#define IDR_FIREBALL		15	// fireball machine

#define IDR_BOOK		16	// generic driver for books of all kinds
#define IDR_ONOFFLIGHT		17	// switchable light in area 3

#define IDR_TRANSPORT		18	// transport system
#define IDR_STATSCROLL		19	// increase value drdata[0] by drdata[1]
	
#define IDR_CLANSPAWN		20	//
#define IDR_CLANJEWEL		21	//

#define IDR_PARKSHRINE		23	// shrine to be discovered in area2
#define IDR_FLAMETHROW		24	// the spaceballs flame thrower
#define IDR_STEPTRAP		25	// trigger other item on use
#define IDR_SPIKETRAP		26	// step-on trap
#define IDR_CHESTSPAWN		27	// spawn enemy from chest
#define IDR_EXTINGUISH		28	// remove burn effect
#define IDR_ASSEMBLE		29	// assemble item

#define IDR_PENT		30	// pentagram
#define IDR_TELE_DOOR		31	// teleport to other side of object

#define IDR_FLASK		32	// basic alchemy uses empty flask to work with
#define IDR_FLOWER		33	// pick flower from flower :P

#define IDR_RANDCHEST		34	// random money in chest-like items (respawn time: 1 hour)
#define IDR_DEMONSHRINE		35	// demon learning shrines

#define IDR_EDEMONBALL		36	// boom boom
#define IDR_EDEMONSWITCH	37	// no boom boom?
#define IDR_EDEMONGATE		38	// demon demon
#define IDR_EDEMONLOADER	39	// edemon loader
#define IDR_EDEMONLIGHT		40	// edemon light
#define IDR_EDEMONDOOR		41	// edemon door
#define IDR_EDEMONBLOCK		42	// edemon blocking chest
#define IDR_EDEMONTUBE		43	// edemon teleport tube

#define IDR_FDEMONLIGHT		44	// fdemon light
#define IDR_FDEMONLOADER	45	// fdemon loader
#define IDR_FDEMONCANNON	46	// fdemon cannon
#define IDR_FDEMONGATE		47	// fdemon gate
#define IDR_FDEMONWAYPOINT	48	// fdemon waypoint
#define IDR_FDEMONFARM		49	// fdemon farm
#define IDR_FDEMONBLOOD		50	// fdemon golem blood
#define IDR_FDEMONLAVA		51	// fdemon lava
#define IDR_MELTINGKEY		52	// ice
#define IDR_ITEMSPAWN		53	// ice
#define IDR_WARMFIRE		54	// ice
#define IDR_BACKTOFIRE		55	// ice
#define IDR_PALACEBOMB		56	// palace
#define IDR_PALACECAP		57	// palace
#define IDR_FREAKDOOR		58	// ice
#define IDR_PALACEKEY		59	// palace
#define IDR_MINEWALL		60	// mine
#define IDR_ENHANCE		61	// base: silver/gold, enhance item
#define IDR_MINEDOOR		62	// mine
#define IDR_TOPLIST		63	// arena
#define IDR_FOOD		64	// generic eating socials
#define IDR_DUNGEONTELE		65	// dungeon
#define IDR_DUNGEONFAKE		66	// dungeon
#define IDR_DUNGEONDOOR		67	// dungeon
#define IDR_DUNGEONKEY		68	// dungeon
#define IDR_RANDOMSHRINE	69	// random
#define IDR_TRAPDOOR		70	// random
#define IDR_JUNKPILE		71	// random
#define IDR_GASTRAP		72	// random
#define IDR_SWAMPARM		73	// swamp
#define IDR_SWAMPWHISP		74	// swamp
#define IDR_SWAMPSPAWN		75	// swamp
#define IDR_PALACEDOOR		76	// palace
#define IDR_FORESTSPADE		77	// forest
#define IDR_FORESTCHEST		78	// forest
#define IDR_PICKDOOR		79	// two (base?)
#define IDR_PICKCHEST		80	// two (base?)
#define IDR_PENTBOSSDOOR	81	// pentagram
#define IDR_BURNDOWN		82	// two
#define IDR_ENCHANTITEM		83	// base: add/increase modifier on an item
#define IDR_EXPLORE		84	// base
#define IDR_BOOKCASE		85	// two
#define IDR_COLORTILE		86	// two
#define IDR_SKELRAISE		87	// two
#define IDR_SPECIAL_POTION	88	// base: special potions (antidot, security)
#define IDR_BONEBRIDGE		89	// bones
#define IDR_BONELADDER		90	// bones
#define IDR_BONEHOLDER		91	// bones
#define IDR_BONEWALL		92	// bones
#define IDR_INFINITE_CHEST	93	// base: create item
#define IDR_BONEHINT		94	// bones
#define IDR_NOMADDICE		95	// nomad
#define IDR_NOMADSTACK		96	// base: stackable items
#define IDR_LFREDUCT		97	// base: shrine to reduce LF for spell change
#define IDR_LQ_DOOR		98	// lq: door
#define IDR_LQ_CHEST		99	// lq: chest
#define IDR_LQ_KEY		100	// lq: key
#define IDR_LABENTRANCE		101	// gatekeeper: labyrinth entrance
#define IDR_LABEXIT		102	// base: labyrinth (solved) exit
#define IDR_LQ_TICKER		103	// lq: ticker for respawn etc.
#define IDR_LQ_ENTRANCE		104	// lq: player entrance
#define IDR_STR_MINE		105	// strategy: mine
#define IDR_STR_STORAGE		106	// strategy: storage
#define IDR_STR_SPAWNER		107	// strategy: spawner
#define IDR_STR_DEPOT		108	// strategy: depot
#define IDR_STR_TICKER		109	// strategy: ticker
#define IDR_NOSNOW		110	// strategy: nosnow
#define IDR_RATCHEST		111	// sewers: crates (chests)
#define IDR_WARPTELEPORT	112	// warped: teleport
#define IDR_WARPTRIALDOOR	113	// warped: door to trial room
#define IDR_WARPBONUS		114	// warped: bonus (chest?)
#define IDR_WARPKEYSPAWN	115	// warped: teleport key spawner
#define IDR_WARPKEYDOOR		116	// warped: key-eating door
#define IDR_TOYLIGHT		117	// light with usable switch
#define IDR_SHRIKEAMULET	118	// shrike: assemble amulet
#define IDR_SHRIKE		119	// shrike: misc items
#define IDR_WEREWOLF		120	// werewolf: misc items
#define IDR_STAFFER		121	// staffer area: misc items
#define IDR_STAFFER2		122	// staffer2 area: misc items
#define IDR_STAFFER3		123	// staffer3 area: misc items
#define IDR_CLANSPAWNEXIT	124	// exit from clan spawners (like recall)
#define IDR_MINEKEYDOOR		125	// teleport to golem room
#define IDR_MINEGATEWAYKEY	126	// assemble key for mine gateway
#define IDR_MINEGATEWAY		127	// the actual door
#define IDR_OXYPOTION		128	// warrmines: oxygen potion
#define IDR_PICKBERRY		129	// warrmines: pick berry from bush
#define IDR_LIZARDFLOWER	130	// warrmines: flower mixer
#define IDR_MISSIONCHEST	131	// mission: special chests
#define IDR_DECAYITEM		132	// decaying stat mod item
#define IDR_TUNNELDOOR		134	// long tunnel magic door
#define IDR_TUNNELDOOR2		135	// long tunnel magic door
#define IDR_DEMONCHIP		136	// demon chip stacks
#define IDR_TEUFELDOOR		137	// sprite-dependant doors in teufelheim
#define IDR_ISLENADOOR		138	// only allow one-at-a-time to fight islena
#define IDR_TEUFELARENA		139	// entrance to a teufel pk arena
#define IDR_TEUFELRATNEST	140	// a rat spawner in teufelheim
#define IDR_TEUFELARENAEXIT	141	// entrance from teufel pk arena
#define IDR_XMASTREE		142	// xmastree special
#define IDR_XMASMAKER		143	// xmaslolli maker
#define IDR_CLANDEFENSE		144	// clan defense
#define IDR_CALIGARFLAME        145     //
#define IDR_CALIGAR             146     // !!! was 144 !!!
#define IDR_RWWKEY		147
#define IDR_BETAITEMMAKER	148	// remove me
#define IDR_GATEKEEPER		149
#define IDR_SIGN		150	// generic sign
#define IDR_ARKHATA		151	//

#define IDR_DOUBLE_DOOR         187     // double door driver (in base.c)
#define IDR_SALTMINE_ITEM       188     // nomads plain (saltmine) items

#define IDR_LAB5_ITEM           190     // lab5: item driver of all lab4 items
#define IDR_LAB4_ITEM           191     // lab4: item driver of all lab4 items
#define IDR_LAB3_SPECIAL        192     // lab3: some simple special items (like the entry door, etc)
#define IDR_LAB3_PLANT          193     // lab3: plant (and berry) driver
#define IDR_LAB2_REGENERATE     194     // lab2: graves throwing out enemies
#define IDR_LAB2_STEPACTION     195     // lab2: water, dish and altar driver
#define IDR_LAB2_WATER          196     // lab2: hat, cape, boots, belt and amulet driver
#define IDR_LAB2_GRAVE          197     // lab2: the regenerate spell item
#define IDR_DEATHFIBRIN		198     // lab1: staff
#define IDR_LABTORCH		199     // lab1: torch

// item drivers >=1000 are used as identity tags only
// >=1000 && <2000 are used for spells which need to get a timer restarted when character is loaded from database
#define IDR_BLESS	1000	// spell bless
#define IDR_FREEZE	1001	// spell freeze
#define IDR_FLASH	1002	// spell flash
#define IDR_WARCRY	1003	// skill warcry
#define IDR_ARMOR	1004
#define IDR_WEAPON	1005
#define IDR_MANA	1006
#define IDR_HP		1007
#define IDR_BURN	1008
#define IDR_POTION_SP	1009
#define IDR_CURSE	1010
#define IDR_POISON0	1011
#define IDR_POISON1	1012
#define IDR_POISON2	1013
#define IDR_POISON3	1014
#define IDR_FIRE	1015
#define IDR_INFRARED	1016
#define IDR_NONOMAGIC	1017
#define IDR_OXYGEN	1018
#define IDR_UWTALK	1019
#define IDR_HEAL	1020
#define IDR_FLASHY	1021

#define IDR_ISSPELL(a)	((a)>=1000 && (a)<2000)
#define IDR_DONTSAVE(a)	((a)==IDR_CURSE || (a)==IDR_MELTINGKEY || (a)==IDR_BACKTOFIRE || (a)==IDR_PALACEBOMB || (a)==IDR_PALACECAP || (a)==IDR_FLASHY || (a)==IDR_FIRE)
#define IDR_ONECARRY(a)	((a)==IDR_CLANJEWEL || (a)==IDR_PALACEBOMB || (a)==IDR_PALACECAP)

int walk_or_use_driver(int cn,int dir);					
int move_driver(int cn,int tx,int ty,int mindist);
int attack_driver(int cn,int co);
int give_driver(int cn,int co);
int take_driver(int cn,int in);
int drop_driver(int cn,int x,int y);
int scan_item_driver(int cn);
int mem_add_driver(int cn,int co,int nr);
int mem_check_driver(int cn,int co,int nr);
char *nextnv(char *ptr,char *name,char *value);
int char_dist(int cn,int co);
int free_citem_driver(int cn);
int is_valid_enemy(int cn,int co,int mem);
int use_driver(int cn,int in,int spec);
int remove_item(int in);
int call_item(int driver,int in,int cn,int due);
int fight_driver_add_enemy(int cn,int co,int hurtme,int visible);
int fight_driver_update(int cn);
int fight_driver_attack_visible(int cn,int nomove);
int fight_driver_follow_invisible(int cn);
int fight_driver_set_dist(int cn,int home_dist,int char__dist,int stop_dist);
int fight_driver_set_home(int cn,int x,int y);
int fight_driver_reset(int cn);
int fight_driver_flee(int cn);
int dist_from_home(int cn,int co);
int is_wearing(int cn,int dr0,int dr1,int dr2,int dr3,int dr4);
void destroy_item(int in);
void standard_message_driver(int cn,struct msg *msg,int agressive,int helper);
int regenerate_driver(int cn);
int spell_self_driver(int cn);
int is_closed(int x,int y);
int is_room_empty(int xs,int ys,int xe,int ye);
int use_item_at(int cn,int x,int y,int spec);
int has_item(int cn,int ID);
int tile_char_dist(int cn,int co);
int add_bonus_spell(int cn,int driver,int strength,int duration);
int map_dist(int fx,int fy,int tx,int ty);
int fireball_driver(int cn,int co,int serial);
int teleport_char_driver(int cn,int x,int y);
int secure_move_driver(int cn,int tx,int ty,int dir,int ret,int lastact);
int ball_driver(int cn,int co,int serial);
int step_char_dist(int cn,int co);
int swap_move_driver(int cn,int tx,int ty,int mindist);
int fight_driver_remove_enemy(int cn,int co);
int tmove_driver(int cn,int tx,int ty,int mindist);
int fight_driver_pulse_value(int cn);
void fight_driver_note_hit(int cn);
void mem_erase_driver(int cn,int nr);
int teleport_char_driver_extended(int cn,int x,int y,int maxdist);
int attack_driver_nomove(int cn,int co);
















