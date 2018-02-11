#define ISCLIENT
#include "main.h"
#include "client.h"
#include "sprite.h"

#define NULL	((void*)0)
		
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define abs(a)	((a)<0 ? (-(a)) : (a))

extern int reduce_anim;

// extern

extern int playersprite_override;

// is_..._sprite

int is_cut_sprite(int sprite)
{
        switch (sprite) {
                case 11104: case 11105: case 11106: case 11107:
                        return sprite+4;

                case 11176: return 17006;

		case 13004: case 13005: case 13006: case 13007:
			return sprite+4;

		case 14000: case 14001: case 14002: case 14003:
			return sprite+4;
		case 14010: case 14011:
			return sprite+2;
		case 14014: case 14015: case 14016: case 14017:
			return sprite+4;
		case 14030: case 14031: case 14032: case 14033:
			return sprite+4;
		case 14040: case 14041:
			return sprite+2;
		case 14060: case 14061: case 14062: case 14063:
			return sprite+4;
		case 14068: case 14069:
			return 0;
		case 14074: case 14075: case 14076: case 14077:
			return sprite+4;
		case 14090: case 14091: case 14092: case 14093:
			return sprite+4;
		case 14098: case 14099:
			return 0;
		case 14112: case 14113: case 14114: case 14115:
			return sprite+4;
		case 14120: case 14121: case 14122: case 14123:
			return sprite+4;
		case 14128: case 14129: case 14130: case 14131:
			return -(sprite+4);
		case 14167: case 14168: case 14169: case 14170:
			return sprite+4;
		case 14179: case 14180: case 14181: case 14182:
			return -(sprite+4);
		case 14303: case 14304: case 14305: case 14306: case 14307: case 14308: case 14309: case 14310: case 14311:
			return sprite+18;
		case 14339: case 14340: case 14341: case 14342:
			return sprite+4;

		case 14433: case 14434: case 14435: case 14436:
			return sprite+4;

		case 14447: case 14448: case 14449: case 14450:
			return -(sprite+4);

		case 15000: case 15001: case 15002: case 15003: case 15004: case 15005: case 15006: case 15007: case 15008:
			return sprite+10;

		case 15040: case 15041: case 15042: case 15043:
			return sprite+4;

		case 15070: return 15077;
		case 15078: return 15085;
		case 15086: return 15093;

		case 15223: case 15224: case 15225: case 15226:
			return sprite+4;

		case 15246: case 15247: case 15248: case 15249:
			return sprite+4;
		
		case 15254: case 15255:
			return 0;

		case 15256: case 15257:
			return sprite+2;

		case 15260: case 15261: case 15262: case 15263:
			return sprite+4;

		case 15340: case 15341: case 15342: case 15343:
			return sprite+4;

		case 15348: case 15349:
			return sprite+2;

		case 15446: case 15447: case 15448: case 15449: case 15450:
			return sprite+5;

                case 15456: case 15457: case 15458: case 15459: case 15460:
			return sprite+5;

		case 15466: case 15467: case 15468: case 15469: case 15470:
			return sprite+5;

		case 15476: case 15477: case 15478: case 15479: case 15480:
			return sprite+5;

		case 15486: return sprite+1;
		case 15488: return sprite+1;

		case 15490: case 15491: case 15492: case 15493:
			return sprite+4;

		case 15498: case 15499:
			return sprite+2;

		case 15502: case 15503:
			return sprite+2;

		case 16225: case 16226: case 16227: case 16228:
			return sprite+4;

		case 16233: case 16234: case 16235: case 16236:
			return sprite+4;

		case 16245: case 16246: case 16247: case 16248:
			return sprite+4;

		case 16253: case 16254: case 16255: case 16256:
			return sprite+4;

		case 16316: case 16318: case 16320: case 16322: case 16324: case 16326: case 16328: case 16330:
			return sprite+1;

		case 16406: case 16407: case 16408: case 16409:
			return sprite+4;

                case 17000: case 17001: case 17002: case 17003:
                case 17008: case 17009: case 17010: case 17011:
                case 17016: case 17017: case 17018: case 17019:
                case 17040: case 17041: case 17042: case 17043:
                        return sprite+4;

                case 17032: case 17033: case 17034: case 17035:
                        return -(sprite+4);

                case 17024: case 17025: case 17026: case 17027:
                case 17028: case 17029:
                case 17030: case 17031:
                        return 0;

                case 20668: case 20669: case 20670: case 20671:
			return -(sprite+4);

                case 20695: case 20696: case 20699: case 20700:
                        return -(sprite+2);
		case 20776: case 20778:
                        return sprite-1;

                case 20163: case 20164: case 20165: case 20166:	
			return 40000+(sprite-20163)+4;

		case 20167: case 20168: case 20169: case 20170:	
			return 40012+(sprite-20167)+4;

		case 20195: case 20196: case 20197: case 20198:
			return 50143;
		case 20909: case 20910: case 20911: case 20912: case 20913:
			return sprite+5;

                case 40000: case 40001: case 40002: case 40003:
                case 40012: case 40013: case 40014: case 40015:
                case 20336: case 20337: case 20338: case 20339:
                case 20371: case 20372: case 20373: case 20374:
                case 20437: case 20438: case 20439: case 20440:
		case 20445: case 20446: case 20447: case 20448:
		case 20465: case 20466: case 20467: case 20468:
		case 20473: case 20474: case 20475: case 20476:
		case 20628: case 20629: case 20630: case 20631:
			return sprite+4;

                case 20654: case 20655: case 20659: case 20660:
                        return sprite+3;

		case 40008: case 40009:
		case 20763: case 20764:
                // case 20423: case 20424: // dungeon_cdoor_ls
			return sprite+2;

                case 21000: case 21001: case 21002: case 21003: case 21004: case 21005: case 21006: case 21007: case 21008: case 21009:
                case 21010: case 21011: case 21012: case 21013: case 21014: case 21015: case 21016: case 21017: case 21018: case 21019:
		case 21020: case 21021: case 21022: case 21023: case 21024: case 21025: case 21026: case 21027: case 21028:
		case 21103: case 21105: case 21107: case 21109:
		case 21195: case 21197: case 21199: case 21201:
		case 40020: case 40022: case 40024: case 40026:		
		case 20481: case 20483:
			return sprite+1;

		//case 21203:
		case 21204: case 21205: case 21206: case 21207: case 21208:
			return 21196;

		case 21350: case 21351: case 21352: case 21353:
			return sprite+4;

		case 21430: case 21431: case 21432: case 21433:
			return sprite+4;

		case 21450: case 21452: case 21454: case 21456: case 21458:
		case 21460: case 21462: case 21464: case 21466: case 21468:
		case 21470: case 21472: case 21474: case 21476: case 21478:
		case 21480: case 21482: case 21484: case 21486: case 21488:
		case 21490: case 21492: case 21494:             case 21498:
		case 21500: case 21502: case 21504: case 21506: case 21508:
		case 21510: case 21512: case 21514: case 21516: case 21518:
		case 21520: case 21522: case 21524: case 21526: case 21528:
		case 21530: case 21532: case 21534: case 21536: case 21538:
		case 21540: case 21542: case 21544:
			return sprite+1;



		case 20512: case 20513:
			return 50160;

		case 20514: case 20515:
			return 50161;

		case 20272: case 20273: case 20274: case 20275:
			return 50142;

		case 50009:
			return 40016;
		
		case 50076: case 50078: case 50080: case 50082:
			return sprite+1;

		case 50116:
			return 21001;

		case 50164: case 50165:
			return 0;
		case 50330:
			return 14305+18;

		case 20427: case 20428: // dungeon walkthrough
		case 20656: case 20661:		
                        return 0;

                case 20846: case 20847: case 20848: case 20849: case 20850: case 20851:
                case 20852: case 20853: case 20854: case 20855: case 20856: case 20857:
                case 20858: case 20859: case 20860: case 20861: case 20862: case 20863:
                case 20864: case 20865: case 20866: case 20867: case 20868: case 20869:
                        return sprite+24;

		case 23093: case 23094: case 23095: case 23096:
			return sprite+4;

		case 22000: case 22001: case 22002: case 22003: case 22004: case 22005: case 22006: case 22007: case 22008:
			return sprite+9;

		case 22027: case 22028: case 22029: case 22030: case 22031: case 22032: case 22033:
			return sprite+8;

		case 50189:	
			return sprite+2;

		case 50190:
			return 0;

		case 22454: case 22455: case 22456: case 22457:
			return sprite+4;

		case 22471: case 22473: case 22474: case 22475: case 22476: case 22478:
			return sprite+8;

		case 22472: case 22477:
			return 0;

		case 23116: case 23117: case 23118: case 23119:
			return sprite+4;

		case 23126: case 23127:
			return sprite+2;
			
		case 23235: case 23236: case 23237: case 23238:
			return sprite+4;

		case 23102: case 23103: case 23104: case 23105: case 23106: case 23107: case 23108: case 23109:
		case 23110: case 23111: case 23112: case 23113: case 23114: case 23115:
			return 0;

		case 51005:	return 21435;
		case 51007:	return 21501;
		case 51009:	return 21451;
		case 51072:	return 21196;

                case 59044: case 59045: case 59046: case 59047: case 59048:
			return sprite+5;

		case 59155: case 59156: case 59157: case 59158:
		case 59163: case 59164: case 59165: case 59166:
		case 59171: case 59172: case 59173: case 59174:
		case 59185: case 59186: case 59187: case 59188:
			return sprite+4;

		case 59179: case 59180:
			return 0;

		case 59181: case 59182:
			return sprite+2;

		case 59193: return 14092+4;

		case 59484: return 15447+5;
		case 59485: return 15486+1;
		case 59486: return 15457+5;
                case 59487: return 15488+1;

		case 59790: case 59791: case 59792: case 59793: return sprite+4;

                case 60038: case 60039: case 60040: case 60041:
                case 14456: case 14457: case 14458: case 14459: // DB - check this in your maps, we have a small bug here
                        return -(sprite-19);

        }

        return sprite;
}

int is_mov_sprite(int sprite, int itemhint)
{
        switch (sprite) {
                case 20039: case 20040: case 20041: case 20042:         // wood door
                case 20122: case 20123: case 20124: case 20125:         // steel door
                case 20152: case 20153: case 20154: case 20155:         // poor wood door
                // case 20423: case 20424: case 20425: case 20426:         // funstuff_cdoor
                case 20429: case 20430: case 20431: case 20432:         // dungeon_door
		case 20461: case 20462: case 20463: case 20464:         // castlelike_door
		case 20516: case 20517: case 20518: case 20519:         // moonie_door
		case 20544: case 20545: case 20549: case 20550:         // moonie_prison_door
			return -5;
                case 20664: case 20665: case 20666: case 20667:         // grave_large_door // _AA_
                case 20695: case 20696: case 20697: case 20698:         // grave_lesssecret_door // _AA_
                case 20699: case 20700: case 20701: case 20702:         // grave_lesssecret_door // _AA_
                        return -9;		
        }
        return itemhint;
}

int is_door_sprite(int sprite)
{
        switch (sprite) {
                case 20039: case 20040: case 20041: case 20042:         // wood door
                case 20122: case 20123: case 20124: case 20125:         // steel door
                case 20152: case 20153: case 20154: case 20155:         // poor wood door
                case 20423: case 20424: case 20425: case 20426:         // funstuff_cdoor
                case 20429: case 20430: case 20431: case 20432:         // dungeon_door
		case 20461: case 20462: case 20463: case 20464:         // castlelike_door
		case 20516: case 20517: case 20518: case 20519:         // moonie_door
		case 20544: case 20545: case 20549: case 20550:         // moonie_prison_door
		case 20664: case 20665: case 20666: case 20667:         // grave_large_door // _AA_
                case 20695: case 20696: case 20697: case 20698:         // grave_lesssecret_door // _AA_
                case 20699: case 20700: case 20701: case 20702:         // grave_lesssecret_door // _AA_
                        return 1;
        }
        return 0;
}

int is_yadd_sprite(int sprite)
{
        switch (sprite) {
		case 13103: case 13104:
			return 29;

                case 20159: case 20160: case 20161: case 20162:
                        return 16;
                case 20158:
                        return 21;
		case 20569:
                        return 30;
                case 21047: case 21048: case 21049: case 21050: case 21051: case 21052: case 21053: case 21054: case 21055: case 21056:
                case 21067: case 21068: case 21069: case 21070: case 21071:
                        return 25;
		case 20784:                                             // _FRED_
                case 20785: case 20786: case 20787: case 20788:         // _FRED_
                        return 26;
                case 21057: case 21058: case 21059: case 21060:
                        return 45;
                case 21061: case 21062: case 21063: case 21064: case 21065: case 21066:
                        return 55;

		case 21118:
			return 50;

		case 21121: case 21122: case 21123: case 21124:
			return 30;

		case 21213: case 21214:
			return 25;

		case 21228:
			return 20;
		case 21662:
			return 25;

		case 50286:
			return 20;

		case 14147: case 14148: case 14149: case 14150: case 14151: case 14152: case 14153: case 14154: case 14155:
			return 20;
		case 14410:
			return 20;
		case 15539:
			return 24;

        }
        return 0;
}

int get_chr_height(int csprite)
{
	//csprite=playersprite_override;

        switch (csprite) {
		case 20: 	return -35;
		case 21: 	return -20;
		
		case 247:	return -54;
		case 248:	return -58;
		case 249:	return -62;
		case 250:	return -66;

		case 255:	return -54;
		case 256:	return -58;
		case 257:	return -62;
		case 258:	return -66;

		case 304:	return -70;

                default: 	return -50;
        }
}

#define IRGB(r,g,b) (((r)<<10)|((g)<<5)|((b)<<0))

// charno to scale / colors
int trans_charno(int csprite,int *pscale,int *pcr,int *pcg,int *pcb,int *plight,int *psat,int *pc1,int *pc2,int *pc3,int *pshine)
{
	int scale=100,cr=0,cg=0,cb=0,light=0,sat=0,c1=0,c2=0,c3=0,shine=0,helper;

	switch(csprite) {
                case 121:	csprite=8; scale=105; break; //skelly
		case 122:	csprite=8; scale=110; break; //skelly med
		case 123:	csprite=8; scale=115; break; //skelly high
		case 124:	csprite=8; scale=120; break; //skelly higher
		case 125:	csprite=16; scale=100; cb=50; break; //ice creeper
		case 126:	csprite=16; scale=100; cr=50; break; //fire creeper
		case 127:	csprite=16; scale=120; break;	// huge creeper
		
                case 134:	csprite=21; scale=60; light=-50; cg=80; break;	// small spider
		
		case 135:	csprite=22; scale=75; break;	// young moony
		case 136:	csprite=22; scale=100; break;	// moony
		case 137:	csprite=22; scale=100; break;	// moony warrior
		case 138:	csprite=22; scale=110; break;	// moony guard
		case 139:	csprite=22; scale=125; break;	// moony elite
					
		case 140:	csprite=13; scale=90; c3=0x001f; break;		// bearded mage in blue
		case 141:	csprite=13; scale=105; c3=0x03700; break;	// bearded mage in green
		case 142:	csprite=13; scale=95; c3=0x7c1f; break;		// bearded mage in purple

		case 143:	csprite=4; scale=90; c3=0x001f; break;		// merchant in blue
		case 144:	csprite=4; scale=105; c3=0x03700; break;	// merchant in green
		case 145:	csprite=4; scale=95; c3=0x7c1f; break;		// merchant in purple

		case 146:	csprite=2; c1=0x4108; c2=0x1884; c3=0x2884; break; // thief
		case 147:	csprite=4; c1=0x1092; c2=0x49c8; c3=0x1092; break; // merchant
		case 148:	csprite=1; c1=0x2108; c2=0x6284; c3=0x318C; break; // sanoa
		case 149:	csprite=2; c1=0x4108; c2=0x1884; c3=0x2884; break; // asturin
		case 150:	csprite=3; c1=0x4252; c2=0x40C6; c3=0x294A; break; // guiwynn
		case 151:	csprite=2; c1=0x4863; c2=0x5A44; c3=0x5a44; break; // logain
		case 152:	csprite=1; c1=0x18c6; c2=0x3104; c3=0x2108; break; // kelly
		case 153:	csprite=2; c1=0x1098; c2=0x3104; c3=0x1090; break; // gatekeeper

		case 154:	csprite=29; scale=95; sat=10; cr=50; break; //earth demon dark brown
		case 155:	csprite=29; sat=10; cr=70; break; //earth demon light brown
		case 156:	csprite=30; break;				// ratling

		case 157:	csprite=39;					// fire demon
				helper=tick&31;
				if (helper>15) helper=32-helper;
				if (reduce_anim) helper=16;
				sat=5; light=-80; cr=60+helper*3;
				break;

		case 158:	csprite=2; c1=0x108c; c2=0x3d88; c3=0x108c; break;	// bert, warrior
		case 159:	csprite=4; c1=0x108c; c2=0x3d88; c3=0x108c; break;	// bert, mage

		case 160:	csprite=2; c1=0x108c; c2=0x48c0; c3=0x3842; break;	// josh, warrior
		case 161:	csprite=4; c1=0x108c; c2=0x48c0; c3=0x3842; break;	// josh, mage

		case 162:	csprite=2; c1=0x108c; c2=0x4a04; c3=0x3842; break;	// will, warrior
		case 163:	csprite=4; c1=0x108c; c2=0x4a04; c3=0x3842; break;	// will, mage

		case 164:	csprite=2; c1=0x2188; c2=0x18c6; c3=0x1204; break;	// james, warrior
		case 165:	csprite=4; c1=0x2188; c2=0x18c6; c3=0x1204; break;	// james, mage

		case 166:	csprite=2; c1=0x2188; c2=0x4a04; c3=0x1204; break;	// carl, warrior
		case 167:	csprite=4; c1=0x2188; c2=0x4a04; c3=0x1204; break;	// carl, mage

		case 168:	csprite=2; c1=0x2188; c2=0x3d88; c3=0x3842; break;	// jim, warrior
		case 169:	csprite=4; c1=0x2188; c2=0x3d88; c3=0x3842; break;	// jim, mage

		case 170:	csprite=2; c1=0x4a52; c2=0x18c6; c3=0x2108; break;	// brad, warrior
		case 171:	csprite=4; c1=0x4a52; c2=0x18c6; c3=0x2108; break;	// brad, mage

		case 173:	csprite=8; scale=110; cr=50; break; // fire skelly
		case 174:	csprite=8; scale=110; cb=50; break; // ice skelly

		case 176:	csprite=1; c1=0x18cc; c2=0x5a68; c3=0x5ad6; break;	// jenny, warrior
		case 177:	csprite=3; c1=0x18cc; c2=0x5a68; c3=0x5ad6; break;	// jenny, mage

		case 178:	csprite=1; c1=0x5294; c2=0x14a5; c3=0x108c; break;	// sarah, warrior
		case 179:	csprite=3; c1=0x5294; c2=0x14a5; c3=0x108c; break;	// sarah, mage

		case 180:	csprite=1; c1=0x481c; c2=0x394a; c3=0x3084; break;	// sue, warrior
		case 181:	csprite=3; c1=0x481c; c2=0x394a; c3=0x3084; break;	// sue, mage

		case 182:	csprite=1; c1=0x18cc; c2=0x394a; c3=0x5ad6; break;	// peggy, warrior
		case 183:	csprite=3; c1=0x18cc; c2=0x394a; c3=0x5ad6; break;	// peggy, mage
									
		case 184:	csprite=1; c1=0x5294; c2=0x5a68; c3=0x108c; break;	// mary, warrior
		case 185:	csprite=3; c1=0x5294; c2=0x5a68; c3=0x108c; break;	// mary, mage
									
		case 186:	csprite=1; c1=0x481c; c2=0x14a5; c3=0x3084; break;	// clara, warrior
		case 187:	csprite=3; c1=0x481c; c2=0x14a5; c3=0x3084; break;	// clara, mage

		case 188:	csprite=1; c1=0x320c; c2=0x4944; c3=0x4084; break;	// beth, warrior
		case 189:	csprite=3; c1=0x320c; c2=0x4944; c3=0x4084; break;	// beth, mage

		case 190:	csprite=33; cr=50; break;		// fire world, demon2

		case 200:	csprite=54; c1=0x3def; c2=0x0300; c3=0x4108; break;	// fred, misc merchant, area 1
		case 201:	csprite=59; c1=0x4042; c2=0x2118; c3=0x4210; break;	// talaman, banker, area 1
		case 202:	csprite=47; c1=0x1204; c2=0x1184; c3=0x1CE7; break;	// egbert, armor merchant, area 1
		case 203:	csprite=47; c1=0x2308; c2=0x4310; c3=0x3108; break;	// dolf, weapon merchant, area 1
		case 204:	csprite=55; c1=0x6050; c2=0x0858; c3=0x0858; break;	// nook, jester, area 1
		case 205:	csprite=49; c1=0x3def; c2=0x294a; c3=0x5220; break;	// reskin, barkeep, area 1
		case 206:	csprite=52; c1=0x4980; c2=0x4a40; c3=0x0000; break;	// logain, teach, area 1
		case 207:	csprite=54; c1=0x3def; c2=0x294a; c3=0x5220; break;	// terion, chatter, area 1
		case 208:	csprite=49; c1=0x70e7; c2=0x4084; c3=0x730c; break;	// asturin, thief, area 1
		case 209:	csprite=49; c1=0x2884; c2=0x2884; c3=0x1ce7; break;	// weak thief guard, thief, area 1
                case 210:	csprite=49; c1=0x21c8; c2=0x4108; c3=0x59e7; break;	// guild thief, thief, area 1
		case 211:	csprite=49; c1=0x21c8; c2=0x4108; c3=0x4948; break;	// guild thief, thief, area 1
		case 212:	csprite=49; c1=0x21c8; c2=0x4108; c3=0x14a5; break;	// guild thief, thief, area 1
                case 213:	csprite=57; c1=0x739c; c2=0x739c; c3=0x1ce7; break;	// guild assassin, thief, area 1
		case 214:	csprite=57; c1=0x5294; c2=0x5294; c3=0x3d47; break;	// guild assassin, thief, area 1
		case 215:	csprite=49; c1=0x5294; c2=0x5294; c3=0x3d47; break;	// guild master, thief, area 1

		case 216:	csprite=43; light=40; break;				// weak zombie, area 2
		case 217:	csprite=43; light=40; cr=10; break;			// zombie, area 2
		case 218:	csprite=43; light=40; cb=10; break;			// strong zombie, area 2
		case 219:	csprite=43; light=40; cg=10; break;			// superior zombie, area 2

		case 220:	csprite=36; light=-10; cg=20; scale=90; break;		// weak 'hunter' collector
		case 221:	csprite=36; light=-10; cg=20; scale=100; break;         // 'hunter' collector
		case 222:	csprite=36; light=-10; cg=20; scale=110; break;		// strong 'hunter' collector

		case 223:	csprite=35; cg=10; scale=90; break;			// weak 'huntress' supervisor
		case 224:	csprite=35; cg=10; scale=100; break;			// weak 'huntress' supervisor
		case 225:	csprite=35; cg=10; scale=110; break;			// weak 'huntress' supervisor

		case 226:	csprite=42; sat=15; cb=60; light=100; scale=110; break;	// ice golem
		case 227:	csprite=39; sat=12; cb=15; light=30; scale=110; break;	// ice lord
		case 228:	csprite=44; sat=15; cb=60; light=100; break;	// ice eye

		case 229:	csprite=59; c1=0x5000; c2=0x3ca5; c3=0x5294; break;		// wesley, banker, area 3
		case 230:	csprite=47; c1=0x5000; c2=0x3ca5; c3=0x5294; break;		// karl, merchant, area 3
		case 231:	csprite=47; c1=0x21e8; c2=0x1144; c3=0x1ce7; scale=95; break;	// ludwig, merchant, area 3
		case 232:	csprite=47; c1=0x3c05; c2=0x6012; c3=0x5ad6; break;		// norbert, merchant, area 3
		case 233:	csprite=47; c1=0x6fb7; c2=0x2110; c3=0x5ad6; break;		// manfred, merchant, area 3
		case 234:	csprite=54; c1=0x6fb7; c2=0x2110; c3=0x5ad6; break;		// jeremy, merchant, area 3

		case 235:	csprite=55; c1=IRGB(31, 0, 0); c2=IRGB(31, 0, 0); c3=0x5ad6; break;	// clan clerks
		case 236:       csprite=55; c1=IRGB( 0,31, 0); c2=IRGB( 0,31, 0); c3=0x5ad6; break;
		case 237:       csprite=55; c1=IRGB( 0, 0,31); c2=IRGB( 0, 0,31); c3=0x5ad6; break;
		case 238:       csprite=55; c1=IRGB(31,31, 0); c2=IRGB(31,31, 0); c3=0x5ad6; break;
		case 239:       csprite=55; c1=IRGB(31, 0,31); c2=IRGB(31, 0,31); c3=0x5ad6; break;
		case 240:       csprite=55; c1=IRGB( 0,31,31); c2=IRGB( 0,31,31); c3=0x5ad6; break;
		case 241:       csprite=55; c1=IRGB(31,16,16); c2=IRGB(31,16,16); c3=0x5ad6; break;
		case 242:       csprite=55; c1=IRGB(16,16,31); c2=IRGB(16,16,31); c3=0x5ad6; break;
			
		case 243:	csprite=33; sat=20; light=0; shine=0; scale=80; break;		// mine, silver golems
		case 244:	csprite=33; sat=20; light=10; shine=10; scale=85; break;
		case 245:	csprite=33; sat=20; light=20; shine=20; scale=90; break;
		case 246:	csprite=33; sat=20; light=30; shine=30; scale=95; break;
		case 247:	csprite=33; sat=20; light=40; shine=40; scale=100; break;
		case 248:	csprite=33; sat=20; light=50; shine=50; scale=105; break;
		case 249:	csprite=33; sat=20; light=60; shine=60; scale=110; break;
		case 250:	csprite=33; sat=20; light=70; shine=70; scale=115; break;

		case 251:	csprite=33; sat=20; light=35; shine=35; scale=80; cr=50; cg=50; break;	// mine, gold golems
		case 252:	csprite=33; sat=20; light=40; shine=40; scale=85; cr=50; cg=50; break;
		case 253:	csprite=33; sat=20; light=45; shine=45; scale=90; cr=50; cg=50; break;
		case 254:	csprite=33; sat=20; light=50; shine=50; scale=95; cr=50; cg=50; break;
		case 255:	csprite=33; sat=20; light=55; shine=55; scale=100; cr=50; cg=50; break;
		case 256:	csprite=33; sat=20; light=60; shine=60; scale=105; cr=50; cg=50; break;
		case 257:	csprite=33; sat=20; light=65; shine=65; scale=110; cr=50; cg=50; break;
		case 258:	csprite=33; sat=20; light=70; shine=70; scale=115; cr=50; cg=50; break;

		case 259:	csprite=55; c1=IRGB(24, 8, 8); c2=IRGB(24, 8, 8); c3=0x5ad6; break;	// clan clerks cont.
		case 260:	csprite=55; c1=IRGB( 8,24, 8); c2=IRGB( 8,24, 8); c3=0x5ad6; break;
		case 261:	csprite=55; c1=IRGB( 8, 8,24); c2=IRGB( 8, 8,24); c3=0x5ad6; break;
		case 262:	csprite=55; c1=IRGB(24,24, 8); c2=IRGB(24,24, 8); c3=0x5ad6; break;
		case 263:	csprite=55; c1=IRGB(24, 8,24); c2=IRGB(24, 8,24); c3=0x5ad6; break;
		case 264:	csprite=55; c1=IRGB( 8,24,24); c2=IRGB( 8,24,24); c3=0x5ad6; break;
		case 265:	csprite=55; c1=IRGB(24,24,24); c2=IRGB(24,24,24); c3=0x5ad6; break;
		case 266:	csprite=55; c1=IRGB(16,16,16); c2=IRGB(16,16,16); c3=0x5ad6; break;

		case 267:	csprite=50; c1=IRGB(31, 0, 0); c2=IRGB(31, 0, 0); c3=0x5ad6; break;	// clan guards 1
		case 268:       csprite=50; c1=IRGB( 0,31, 0); c2=IRGB( 0,31, 0); c3=0x5ad6; break;
		case 269:       csprite=50; c1=IRGB( 0, 0,31); c2=IRGB( 0, 0,31); c3=0x5ad6; break;
		case 270:       csprite=50; c1=IRGB(31,31, 0); c2=IRGB(31,31, 0); c3=0x5ad6; break;
		case 271:       csprite=50; c1=IRGB(31, 0,31); c2=IRGB(31, 0,31); c3=0x5ad6; break;
		case 272:       csprite=50; c1=IRGB( 0,31,31); c2=IRGB( 0,31,31); c3=0x5ad6; break;
		case 273:       csprite=50; c1=IRGB(31,16,16); c2=IRGB(31,16,16); c3=0x5ad6; break;
		case 274:       csprite=50; c1=IRGB(16,16,31); c2=IRGB(16,16,31); c3=0x5ad6; break;
		case 275:	csprite=50; c1=IRGB(24, 8, 8); c2=IRGB(24, 8, 8); c3=0x5ad6; break;
		case 276:	csprite=50; c1=IRGB( 8,24, 8); c2=IRGB( 8,24, 8); c3=0x5ad6; break;
		case 277:	csprite=50; c1=IRGB( 8, 8,24); c2=IRGB( 8, 8,24); c3=0x5ad6; break;
		case 278:	csprite=50; c1=IRGB(24,24, 8); c2=IRGB(24,24, 8); c3=0x5ad6; break;
		case 279:	csprite=50; c1=IRGB(24, 8,24); c2=IRGB(24, 8,24); c3=0x5ad6; break;
		case 280:	csprite=50; c1=IRGB( 8,24,24); c2=IRGB( 8,24,24); c3=0x5ad6; break;
		case 281:	csprite=50; c1=IRGB(24,24,24); c2=IRGB(24,24,24); c3=0x5ad6; break;
		case 282:	csprite=50; c1=IRGB(16,16,16); c2=IRGB(16,16,16); c3=0x5ad6; break;

		case 283:	csprite=51; c1=IRGB(31, 0, 0); c2=IRGB(31, 0, 0); c3=0x5ad6; break;	// clan guards 2
		case 284:       csprite=51; c1=IRGB( 0,31, 0); c2=IRGB( 0,31, 0); c3=0x5ad6; break;
		case 285:       csprite=51; c1=IRGB( 0, 0,31); c2=IRGB( 0, 0,31); c3=0x5ad6; break;
		case 286:       csprite=51; c1=IRGB(31,31, 0); c2=IRGB(31,31, 0); c3=0x5ad6; break;
		case 287:       csprite=51; c1=IRGB(31, 0,31); c2=IRGB(31, 0,31); c3=0x5ad6; break;
		case 288:       csprite=51; c1=IRGB( 0,31,31); c2=IRGB( 0,31,31); c3=0x5ad6; break;
		case 289:       csprite=51; c1=IRGB(31,16,16); c2=IRGB(31,16,16); c3=0x5ad6; break;
		case 290:       csprite=51; c1=IRGB(16,16,31); c2=IRGB(16,16,31); c3=0x5ad6; break;
		case 291:	csprite=51; c1=IRGB(24, 8, 8); c2=IRGB(24, 8, 8); c3=0x5ad6; break;
		case 292:	csprite=51; c1=IRGB( 8,24, 8); c2=IRGB( 8,24, 8); c3=0x5ad6; break;
		case 293:	csprite=51; c1=IRGB( 8, 8,24); c2=IRGB( 8, 8,24); c3=0x5ad6; break;
		case 294:	csprite=51; c1=IRGB(24,24, 8); c2=IRGB(24,24, 8); c3=0x5ad6; break;
		case 295:	csprite=51; c1=IRGB(24, 8,24); c2=IRGB(24, 8,24); c3=0x5ad6; break;
		case 296:	csprite=51; c1=IRGB( 8,24,24); c2=IRGB( 8,24,24); c3=0x5ad6; break;
		case 297:	csprite=51; c1=IRGB(24,24,24); c2=IRGB(24,24,24); c3=0x5ad6; break;
		case 298:	csprite=51; c1=IRGB(16,16,16); c2=IRGB(16,16,16); c3=0x5ad6; break;

		case 299:	csprite=8; cg=5; light=-80; shine=70; break;	// dark skeleton, random dungeon

		case 300:	csprite=24; cr=20; cg=20; light=10; scale=80; break;	//swamp beasts
                case 301:	csprite=24; cr=25; light=-40; shine=0; break;
		case 302:	csprite=24; sat=8; light=20; scale=105; break;
		case 303:	csprite=24; cg=10; light=-20; shine=10; scale=110; break;
		case 304:	csprite=24; cg=10; light=-20; shine=25; scale=150; break;
						
		case 305:	csprite=58; c1=IRGB(16,16,31); c2=IRGB(4,4,12); c3=IRGB(28,24,0); break;	// islena

		case 306:	csprite=12; sat=10; light=30; break;				// forest: bear
		case 307:	csprite=19; sat=8; cr=15; light=15; scale=120; break;		// forest: wolf
		case 308:	csprite=21; cg=120; light=-125; scale=80; break; 	// forest: spider
		case 309:	csprite=21; cg=120; light=-125; scale=120; break;	// forest: spider queen
		case 310:	csprite=40; cg=40; light=-10; break;			// forest: preying mantiss
		case 311:	csprite=46; c1=0x41aa; c2=0x360a; c3=0x7f92; scale=80; shine=25; break;	// forest: imp
		case 312:	csprite=53; c1=0x41aa; c2=0x360a; c3=0x7f92; break;	// forest: robber
		case 313:	csprite=58; c1=IRGB(18,18,18); c2=IRGB(22,22,22); c3=IRGB(12,7,6); break; // zoetje
		case 314:	csprite=13; c3=0x3def; break; // forest: hermit
		case 315:	csprite=54; c1=0x7380; c2=0x739c; break;	// exkordon: cook
		case 316:	csprite=59; c1=0x7380; c2=0x739c; c3=0x739c;  break;	// exkordon: governor double

		case 317:	csprite=113; c1=0x4829; c2=0x18EF; c3=0x6646; break;	// exkordon guard with torch
		case 318:	csprite=111; c1=0x4829; c2=0x18EF; c3=0x6646; break;	// exkordon guard

		case 319:	csprite=30; light=-50; cg=40; break;			// exkordon ratling
		case 320:	csprite=49; c1=0x2421; c2=0x3C22; c3=0x6646; break;	// exkordon thief guard
		case 321:	csprite=49; c1=0x3031; c2=0x2837; c3=0x1C42; break;	// exkordon thief guard
		case 322:	csprite=41; cr=10; scale=110; break;			// exkordon statue
		case 323:	csprite=41; cr=10; scale=120; break;                    // exkordon statue boss
		case 324:	csprite=13; c3=0x6E82; cg=70; light=-10; break;		// random: wizard of yendor
		case 325:	csprite=9; cg=20; scale=110; light=-10; break;		// exkordon: green light skelly
		case 326:	csprite=9; cr=20; scale=110; light=-10; break;		// exkordon: red light skelly
		case 327:	csprite=9; scale=110; light=-10; break;	     	   	// exkordon: no light skelly
		case 328:	csprite=21; cr=120; cg=60; light=-125; break;		// exkordon: spider
		case 329:	csprite=21; cr=100; cg=80; light=-125; break;           // exkordon: spider
		case 330:	csprite=21; cr=120; cg=30; light=-125; break;		// exkordon: spider

		case 331:	csprite=36; light=-10; cr=20; scale=100; break;         // exkordon: collector

		case 332:	csprite=43; light=50; cr=10; shine=20; break;		// exkordon: zombie

		case 333:	csprite=8; shine=20; light=-20; break;				// bones: skeleton
		case 334:	csprite=43; light=30; sat=20; break;				// bones: zombie

		case 335:	csprite=19; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		case 336:	csprite=19; sat=20; cr=40; cg=35; light=45; break;	  		// nomad: wolf2
		case 337:	csprite=19; sat=20; cr=45; cg=30; light=20; scale=105; break;	  	// nomad: wolf3
		case 338:	csprite=19; sat=20; light=125; scale=110; break;  			// nomad: white wolf

		case 339:	csprite=14; c1=0x2965; c2=0x318D; c3=0x4DC1; break;	// nomad: nomad1
		case 340:	csprite=14; c1=0x2965; c2=0x318D; c3=0x1C41; break;	// nomad: nomad2
		case 341:	csprite=11; c1=0x2165; c2=0x30A5; c3=0x1C41; break;	// nomad: harpy1
		case 342:	csprite=11; c1=0x2172; c2=0x1C2E; c3=0x1C4E; break;	// nomad: harpy2
		case 343:	csprite=11; c1=0x2172; c2=0x198A; c3=0x1C4E; break;	// nomad: harpy3
		case 344:	csprite=48; c1=0x2A5F; c2=0x4A55; c3=0x3421; break;	// nomad: valkyrie1
		case 345:	csprite=48; c1=0x52DF; c2=0x4A55; c3=0x3421; break;	// nomad: valkyrie2

		case 346:	csprite=34; c1=0x7FFF; c2=0x56B5; c3=0x7FDF; break;	// nomad: evil monk
		case 347:	csprite=34; c1=0x5C34; c2=0x3C54; c3=0x7FDF; break;	// nomad: sarkilar (monk)
		case 348:	csprite=34; c1=0x7329; c2=0x6681; c3=0x7FDF; break;	// nomad: good monk 1
		case 349:	csprite=34; c1=0x2C39; c2=0x1CEF; c3=0x7FDF; break;	// nomad: good monk 2
		
		case 350:	csprite=35; cr=20; cb=40; light=-20; break;		// light&dark lab, gnome 2

		case 351:	csprite=100; c1=0x0819; c2=0x2014; c3=0x5E00; break;	// imperial army seyan male
		case 352:	csprite=110; c1=0x0819; c2=0x2014; c3=0x5E00; break;	// imperial army seyan female

		case 353:	csprite=35; sat=20; cr=40; break;			// strategy, slot1
                case 354:	csprite=35; sat=20; cg=40; break;	                // strategy, slot2
		case 355:	csprite=35; sat=20; cb=40; break;                       // strategy, slot3
		case 356:	csprite=35; sat=20; cr=40; cg=40; break;                // strategy, slot4

		case 357:	csprite=35; sat=20; light=40; break;                    // strategy, neutral worker

		case 360:	csprite=45; c1=0x0431; c2=0x2110; c3=0x6622; break;

		case 361:	csprite=39; sat=20; light=-50; cr=30; break;	// hell demon

                case 400:       csprite=16; scale=180; break;                           // lab3 large creeper
                case 401:       csprite=22; c1=0x3D88; c2=0x3D46; c3=0x6A8C; scale=96; break;           // lab4 gnalb guard
                case 402:       csprite=22; c1=0x3D88; c2=0x2D26; c3=0x34E6; scale=90; break;           // lab4 gnalb
                case 403:       csprite=22; c1=0x3D88; c2=0x2D26; c3=0x4D50; scale=96; break;           // lab4 gnalb mage
                case 404:       csprite=22; c1=0x3D88; c2=0x2D26; c3=0x6F0F; scale=100; break;          // lab4 gnalb king
                case 405:       csprite=22; c1=0x55A6; c2=0x1839; c3=0x5568; scale=95; break;           // lab4 strong gnalb

                case 406:       csprite=32; scale=110; break;            // lab5 daemon1
                case 407:       csprite=27; scale=110; break;            // lab5 daemon2
                case 408:       csprite=29; scale=110; break;            // lab5 daemon3

                case 409:	csprite=34; c1=0x7D31; c2=0x316B; c3=0x5568; break;	// nomad: saltmine gatama
                case 411:	csprite=34; c1=0x6E0C; c2=0x62B5; c3=0x5568; break;	// nomad: saltmine worker
                case 412:	csprite=42; cr=40; cg=70; cb=60; break;	                // nomad: saltmine golem made of stone

                case 410:	csprite=34; c1=0x7D21; c2=0x7BBC; c3=0x5568; break;	// north: govida        !!! take care of the nr.
                case 413:       csprite=48; c1=0x197A; c2=0x4A55; c3=0x3421; break;	// north: valkyrie blue
                case 414:       csprite=48; c1=0x7063; c2=0x4A55; c3=0x3421; break;	// north: valkyrie red
                case 415:       csprite=48; c1=0x7F28; c2=0x7F28; c3=0x7F28; break;	// north: valkyrie gold
                case 416:       csprite=48; c1=0xFFFF; c2=0xFFFF; c3=0xFFFF; break;	// north: valkyrie white

                case 417:       csprite=39; scale=100; break;                           // north: ice deamon
                case 418:       csprite=39; scale=103; break;                           // north: ice deamon
                case 419:       csprite=39; scale=116; break;                           // north: ice deamon
                case 420:       csprite=39; scale=119; break;                           // north: ice deamon
                case 421:       csprite=39; scale=112; break;                           // north: ice deamon
                case 422:       csprite=39; scale=115; break;                           // north: ice deamon
                case 423:       csprite=39; scale=118; break;                           // north: ice deamon
                case 424:       csprite=39; scale=121; break;                           // north: ice deamon
                case 425:       csprite=39; scale=124; break;                           // north: ice deamon
		case 426:       csprite=39; scale=127; break;                           // north: ice deamon

                case 500:	csprite=55; c1=IRGB(31,24,24); c2=IRGB(31,24,24); c3=0x5ad6; break;	// clan clerks 17 to 32
		case 501:	csprite=55; c1=IRGB(24,31,24); c2=IRGB(24,31,24); c3=0x5ad6; break;
		case 502:	csprite=55; c1=IRGB(24,24,31); c2=IRGB(24,24,31); c3=0x5ad6; break;
		case 503:	csprite=55; c1=IRGB(31,31,24); c2=IRGB(31,31,24); c3=0x5ad6; break;
		case 504:	csprite=55; c1=IRGB(31,24,31); c2=IRGB(31,24,31); c3=0x5ad6; break;
		case 505:	csprite=55; c1=IRGB(24,31,31); c2=IRGB(24,31,31); c3=0x5ad6; break;
		case 506:	csprite=55; c1=IRGB(31, 8, 8); c2=IRGB(31, 8, 8); c3=0x5ad6; break;
		case 507:	csprite=55; c1=IRGB( 8, 8,31); c2=IRGB( 8, 8,31); c3=0x5ad6; break;
		case 508:	csprite=55; c1=IRGB(16, 8, 8); c2=IRGB(16, 8, 8); c3=0x5ad6; break;
		case 509:	csprite=55; c1=IRGB( 8,16, 8); c2=IRGB( 8,16, 8); c3=0x5ad6; break;
		case 510:	csprite=55; c1=IRGB( 8, 8,16); c2=IRGB( 8, 8,16); c3=0x5ad6; break;
		case 511:	csprite=55; c1=IRGB(16,16, 8); c2=IRGB(16,16, 8); c3=0x5ad6; break;
		case 512:	csprite=55; c1=IRGB(16, 8,16); c2=IRGB(16, 8,16); c3=0x5ad6; break;
		case 513:	csprite=55; c1=IRGB( 8,16,16); c2=IRGB( 8,16,16); c3=0x5ad6; break;
		case 514:	csprite=55; c1=IRGB( 8,31, 8); c2=IRGB( 8,31, 8); c3=0x5ad6; break;
		case 515:	csprite=55; c1=IRGB(31, 8,31); c2=IRGB(31, 8,31); c3=0x5ad6; break;

		case 516:	csprite=50; c1=IRGB(31,24,24); c2=IRGB(31,24,24); c3=0x5ad6; break;	// clan guards 1 (17 to 32)
		case 517:	csprite=50; c1=IRGB(24,31,24); c2=IRGB(24,31,24); c3=0x5ad6; break;
		case 518:	csprite=50; c1=IRGB(24,24,31); c2=IRGB(24,24,31); c3=0x5ad6; break;
		case 519:	csprite=50; c1=IRGB(31,31,24); c2=IRGB(31,31,24); c3=0x5ad6; break;
		case 520:	csprite=50; c1=IRGB(31,24,31); c2=IRGB(31,24,31); c3=0x5ad6; break;
		case 521:	csprite=50; c1=IRGB(24,31,31); c2=IRGB(24,31,31); c3=0x5ad6; break;
		case 522:	csprite=50; c1=IRGB(31, 8, 8); c2=IRGB(31, 8, 8); c3=0x5ad6; break;
		case 523:	csprite=50; c1=IRGB( 8, 8,31); c2=IRGB( 8, 8,31); c3=0x5ad6; break;
		case 524:	csprite=50; c1=IRGB(16, 8, 8); c2=IRGB(16, 8, 8); c3=0x5ad6; break;
		case 525:	csprite=50; c1=IRGB( 8,16, 8); c2=IRGB( 8,16, 8); c3=0x5ad6; break;
		case 526:	csprite=50; c1=IRGB( 8, 8,16); c2=IRGB( 8, 8,16); c3=0x5ad6; break;
		case 527:	csprite=50; c1=IRGB(16,16, 8); c2=IRGB(16,16, 8); c3=0x5ad6; break;
		case 528:	csprite=50; c1=IRGB(16, 8,16); c2=IRGB(16, 8,16); c3=0x5ad6; break;
		case 529:	csprite=50; c1=IRGB( 8,16,16); c2=IRGB( 8,16,16); c3=0x5ad6; break;
		case 530:	csprite=50; c1=IRGB( 8,31, 8); c2=IRGB( 8,31, 8); c3=0x5ad6; break;
		case 531:	csprite=50; c1=IRGB(31, 8,31); c2=IRGB(31, 8,31); c3=0x5ad6; break;

		case 532:	csprite=51; c1=IRGB(31,24,24); c2=IRGB(31,24,24); c3=0x5ad6; break;	// clan guards 2 (17 to 32)
		case 533:	csprite=51; c1=IRGB(24,31,24); c2=IRGB(24,31,24); c3=0x5ad6; break;
		case 534:	csprite=51; c1=IRGB(24,24,31); c2=IRGB(24,24,31); c3=0x5ad6; break;
		case 535:	csprite=51; c1=IRGB(31,31,24); c2=IRGB(31,31,24); c3=0x5ad6; break;
		case 536:	csprite=51; c1=IRGB(31,24,31); c2=IRGB(31,24,31); c3=0x5ad6; break;
		case 537:	csprite=51; c1=IRGB(24,31,31); c2=IRGB(24,31,31); c3=0x5ad6; break;
		case 538:	csprite=51; c1=IRGB(31, 8, 8); c2=IRGB(31, 8, 8); c3=0x5ad6; break;
		case 539:	csprite=51; c1=IRGB( 8, 8,31); c2=IRGB( 8, 8,31); c3=0x5ad6; break;
		case 540:	csprite=51; c1=IRGB(16, 8, 8); c2=IRGB(16, 8, 8); c3=0x5ad6; break;
		case 541:	csprite=51; c1=IRGB( 8,16, 8); c2=IRGB( 8,16, 8); c3=0x5ad6; break;
		case 542:	csprite=51; c1=IRGB( 8, 8,16); c2=IRGB( 8, 8,16); c3=0x5ad6; break;
		case 543:	csprite=51; c1=IRGB(16,16, 8); c2=IRGB(16,16, 8); c3=0x5ad6; break;
		case 544:	csprite=51; c1=IRGB(16, 8,16); c2=IRGB(16, 8,16); c3=0x5ad6; break;
		case 545:	csprite=51; c1=IRGB( 8,16,16); c2=IRGB( 8,16,16); c3=0x5ad6; break;
		case 546:	csprite=51; c1=IRGB( 8,31, 8); c2=IRGB( 8,31, 8); c3=0x5ad6; break;
		case 547:	csprite=51; c1=IRGB(31, 8,31); c2=IRGB(31, 8,31); c3=0x5ad6; break;

		case 550:	csprite=19; sat=10; cr=5; light=-5; scale=135; break;		// moors: wolf
		case 551:	csprite=37; sat=10; cr=5; light=-50; break;			// moors: werewolf
			
		case 552:	csprite=30; light=-40; sat=10; cg=30; scale=80; break;
		case 553:	csprite=30; light=-40; sat=10; cg=30; scale=90; break;
		case 554:	csprite=30; light=-40; sat=10; cg=30; scale=100; break;
		case 555:	csprite=30; light=-40; sat=10; cg=30; scale=110; break;

		case 556:	csprite=30; light=10; sat=15; cb=20; scale= 60; break;
		case 557:	csprite=30; light=10; sat=15; cb=20; scale= 80; break;
		case 558:	csprite=30; light=10; sat=15; cb=20; scale=100; break;
		case 559:	csprite=30; light=10; sat=15; cb=20; scale=120; break;
		case 560:	csprite=30; light=10; sat=15; cb=20; scale=140; break;
		case 561:	csprite=30; light=10; sat=15; cb=20; scale=160; break;

		case 562:	csprite=30; light=-20; sat=15; cr=60; scale= 60; break;
		case 563:	csprite=30; light=-20; sat=15; cr=60; scale= 80; break;
		case 564:	csprite=30; light=-20; sat=15; cr=60; scale=100; break;
		case 565:	csprite=30; light=-20; sat=15; cr=60; scale=120; break;
		case 566:	csprite=30; light=-20; sat=15; cr=60; scale=140; break;
		case 567:	csprite=30; light=-20; sat=15; cr=60; scale=160; break;

		//case 305:	csprite=58; c1=IRGB(16,16,31); c2=IRGB(4,4,12); c3=IRGB(28,24,0); break;	// islena
		//case 313:	csprite=58; c1=IRGB(18,18,18); c2=IRGB(22,22,22); c3=IRGB(12,7,6); break; // zoetje

		case 568:	csprite=58; c1=IRGB(16,31,16); c2=IRGB(6,20,6); c3=IRGB(28,16,16); break; // queen fiona
	}
					
	if (pscale) *pscale=scale;
	if (pcr) *pcr=cr;
	if (pcg) *pcg=cg;
	if (pcb) *pcb=cb;
	if (plight) *plight=light;
	if (psat) *psat=sat;
	if (pc1) *pc1=c1;
	if (pc2) *pc2=c2;
	if (pc3) *pc3=c3;
	if (pshine) *pshine=shine;

	return csprite;
}

// asprite

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
		  unsigned short *pshine)
{
        // if (!isprite) return 0;
        int help,scale=100,cr=0,cg=0,cb=0,light=0,sat=0,nr,c1=0,c2=0,c3=0,shine=0,edi=0;

#ifdef EDITOR
	if (editor) edi=1;
#endif	

        switch (sprite) {
                case 60042: sprite=1012+((attick/8)%8); break;  // north pent
                case 60043: sprite=1012+((attick/4)%8); break;
                case 60044: sprite=1012+((attick/2)%8); break;
                case 60045: sprite=1012+((attick/1)%8); break;

                case 11204: sprite=sprite+((attick/4)%3); break; // xmas tree

                case 11127:                                                                                     // lab5_regen
                        help=(attick/4)%16;
                        if (help<8) sprite=sprite+help; else sprite=sprite+15-help;
                        break;

                case 11139: sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8; break;       // lab5_light

		case 13063:
		case 13071:
		case 13079:
		case 13087:
		case 13095:	
		case 13214:
		case 13223:	sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/5)%8; break;

		case 13232:	sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/5)%16; break;
                //--
		case 12163:
                case 14353:	// lava_ground_circle
			help=(mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick/31);
			if (help%17<14) sprite=14353+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/5)%8;
			else if (help%17<16) sprite=14353+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/9)%8;
			else sprite=14353+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			//c3=IRGB(16,0,0);
			break;
		case 12164: case 12165: case 12166:
		case 14361:     // lava_ground_noise
			sprite=14361+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/5)%8;
			//c3=IRGB(16,0,0);
			break;
		case 1024: 	// lava_fire - spell misuse
			sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%10;
                        break;
		case 1034: 	// lava_zish - spell misuse
			sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%24;
			break;

		case 1060:	// labyrinth gate
		case 1061:
		case 1062:
		case 1063:
		case 1064:
		case 1065:
		case 1066:
		case 1067:
		case 1068:
		case 1069:
		case 1070:
		case 1071:
		case 1072:
		case 1073:
		case 1074:
		case 1075:
		case 1076:
		case 1077:
		case 1078:
		case 1079:
		case 1080:
		case 1081:
		case 1082:
		case 1083:
		case 1084:
		case 1085:
		case 1086:
		case 1087:
		case 1088:
		case 1089:
		case 1090:
		case 1091:
		case 1092:
		case 1093:
		case 1094:
		case 1095:
		case 1096:
		case 1097:
		case 1098:
		case 1099:
		case 1100:
		case 1101:
		case 1102:
		case 1103:
		case 1104:
		case 1105:
		case 1106:
		case 1107:
		case 1108:
		case 1109:
		case 1110:
		case 1111:
		case 1112:
		case 1113:
		case 1114:
		case 1115:
		case 1116:
		case 1117:
		case 1118:
		case 1119:
		case 1120:
		case 1121:
		case 1122:
		case 1123:
		case 1124:
		case 1125:
		case 1126:
		case 1127:
		case 1128:
		case 1129:
		case 1130:
		case 1131:	c1=c2=c3=IRGB(20,20,30); break;

		


                case 14363:	// special lava
				sprite=14361+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/8)%8;
				light=-30; cr=-10;
				break;	
		case 14364:	// special lava
				sprite=14361+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/16)%8;
				light=-60; cr=-20;
				break;	
		case 14365:	// special lava
				sprite=14361+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/32)%8;
				light=-90; cr=-30;
				break;	
		case 14366:	// special lava
				sprite=14361+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/64)%8;
				light=-120; cr=-40;
				break;	
		//--
			
		case 14205:	// green book
		case 14207:
			c2=IRGB(0,16,0);
			break;
		case 14224:	// switch green
			c2=IRGB(0,16,0);
			break;
		case 14228:	// defense station destroyed grey
			c2=IRGB(4,4,4);
			break;
		case 14136:	// green edemon tube on
			sprite=14136+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,16,0);
			break;
		case 14137:	// edemon tube off
			sprite=14136;	//+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(4,4,4);
			break;
		case 14138:	// orange edemon tube on
			sprite=14136+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,12,0);
			break;
		case 14139:	// red edemon tube on
			sprite=14136+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14140:	// blue edemon tube on
			sprite=14136+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,0,16);
			break;
		case 14141:	// white edemon tube on
			sprite=14136+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,16,24);
                        break;
		case 14159:	// green edemon cannon on
			sprite=14159+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,16,0);
			break;
		case 14160:	// edemon cannon off
                        c2=IRGB(4,4,4);
			break;
		case 14161:	// orange edemon cannon on
			sprite=14159+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,12,0);
			break;
		case 14162:	// red edemon cannon on
			sprite=14159+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14163:	// blue edemon cannon on
			sprite=14159+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,0,16);
			break;
		case 14164:	// white edemon cannon on
			sprite=14159+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,16,24);
			break;
		case 14190:	// green edemon light
			sprite=14190+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,16,0);
			break;
		case 14191:	// orange edemon light
			sprite=14190+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,12,0);
			break;
		case 14192:	// red edemon light
			sprite=14190+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14193:	// blue edemon light
			sprite=14190+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,0,16);
			break;
		case 14194:	// white edemon light
			sprite=14190+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,16,24);
			break;
		case 14200:	// edemon suspensor green
			sprite=14200+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%4;
			c2=IRGB(0,16,0);
			break;
		case 14201:	// edemon suspensor orange
			sprite=14200+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%4;
			c2=IRGB(16,12,0);
			break;
		case 14202:	// edemon suspensor red
			sprite=14200+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%4;
			c2=IRGB(16,0,0);
			break;
		case 14203:	// edemon suspensor blue
			sprite=14200+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%4;
			c2=IRGB(0,0,16);
			break;
		case 14275:	// edemon gate green
			sprite=14275+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(0,16,0);
			break;
		case 14276:	// edemon gate orange
			sprite=14275+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,12,0);
			break;
		case 14277:	// edemon gate red
			sprite=14275+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14256:	// edemon crystal orange
			c2=IRGB(16,12,0);
			break;
		case 14241:	// edemon loader ground growing orange
		case 14242:
		case 14243:
		case 14244:
		case 14245:
		case 14246:
		case 14247:
			c2=IRGB(16,12,0);
			break;
		case 14248:	// edemon loader ground orange
			sprite=14248+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,12,0);
			break;
		case 14257:	// edemon loader platform orange
		case 14258:
		case 14259:
		case 14260:
		case 14261:
		case 14262:
			c2=IRGB(16,12,0);
			break;

		//--
		case 14378:	// edemon cannon off
			sprite=14378; c2=IRGB(4,4,4);
			break;
                case 14379:	// red edemon cannon on
			sprite=14378+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14380:	// edemon cannon off
			sprite=14389; c2=IRGB(4,4,4);
			break;
		case 14381:	// red edemon cannon on
			sprite=14386+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14382:	// edemon cannon off
			sprite=14394; c2=IRGB(4,4,4);
			break;
		case 14383:	// red edemon cannon on
			sprite=14394+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14384:	// edemon cannon off
			sprite=14402; c2=IRGB(4,4,4);
			break;
		case 14385:	// red edemon cannon on
			sprite=14402+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		case 14411:
			sprite=14411+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			break;
		
		case 16035: sprite=16039; break;	// moving door to correct sprite no
		case 16036: sprite=16043; break;	// moving door to correct sprite no
		case 16037: sprite=16043; break;	// moving door to correct sprite no
		case 16038: sprite=16039; break;	// moving door to correct sprite no

                case 16051: sprite=16055; break;	// moving door to correct sprite no
		case 16052: sprite=16059; break;	// moving door to correct sprite no
		case 16053: sprite=16059; break;	// moving door to correct sprite no
		case 16054: sprite=16055; break;	// moving door to correct sprite no

		case 16055: sprite=16087; break;	// moving door to correct sprite no
		case 16056: sprite=16091; break;	// moving door to correct sprite no
		case 16057: sprite=16091; break;	// moving door to correct sprite no
		case 16058: sprite=16087; break;	// moving door to correct sprite no

		case 21340: c2=IRGB(0,0,abs(31-(attick%63))); break;

		case 21681: sprite=21689; break;
		case 21682: sprite=21693; break;
		case 21683: sprite=21693; break;
		case 21684: sprite=21689; break;

		case 21685: sprite=21705; break;
		case 21686: sprite=21709; break;
		case 21687: sprite=21709; break;
		case 21688: sprite=21705; break;

                case 20026:     // standinglight
                        sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			break;
		case 50022:    // torch
		case 10004:     // mr_torch
			sprite=10004+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			break;
                case 20044:     // mr_wall_torch_ds
                case 20054:     // mr_wall_torch_ls
                case 20064:     // mr_wall_torch_lh
		case 20074:     // mr_wall_torch_dh
		case 20283:	// transport red
                        sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			break;
		case 20284:	// transport green
                        sprite=20283+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			c3=IRGB(0,12,4);
			break;

		case 20926:
			sprite=20926+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			break;

		/*case 20934:
			sprite=20934+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%16;
			break;*/

                case 21086:     // mr_redcandela_on
                case 21090:     // mr_redcandelb_on
                case 21094:     // mr_redcandelc_on
                        sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%3;
			break;
                case 11113:     // gnalb_fireplace_key_on
		case 20111:     // lr_ruinlight
		case 20521:     // moonie_standlight_on
		case 20530:     // moonie_fireplace_on
		case 20704:
		case 20713:
		case 20722:
		case 20731:
                        sprite=sprite+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			break;
		case 20754:     // grave_firecan_on                                             // _FRED_
                        sprite=sprite+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%3; // _FRED_
                        break;                                                                  // _FRED_
                case 20388:     // dungeon_walllight_se_on
                case 20397:     // dungeon_walllight_sw_on
                case 20406:     // dungeon_walllight_nw_on
                case 20415:     // dungeon_walllight_ne_on
                        help=((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+attick/10+rrand(2));
                        if ((help%=50)>15) sprite=sprite+5;
                        else if (help<8) sprite=sprite+help;
			else sprite=sprite+15-help;
			break;

		/*case 20797:
		case 20798:
		case 20799:
		case 20800:
		case 20801:
		case 20802:
		case 20803:
		case 20804:
		case 20805:	light=10; break;*/

                case 22500:	// sewer outlet
			sprite=sprite+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			break;
		case 22508:	// sewer ground
			sprite=sprite+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			break;
		case 50132:	// green creeper lights
		case 50136:
			sprite=sprite+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%4;
			break;
		case 50265:
			sprite=sprite+((int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%16);
			break;
		case 50289:
			help=((int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%16);
			if (help>7) help=15-help;
                        sprite=sprite+help;
			break;
		case 50297:
			help=((int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%16);
			if (help>7) help=15-help;
                        sprite=sprite+help;
			break;		

		case 51085:
		case 51086:	c2=IRGB(0,0,abs(30-(attick%61))/2+10); light=abs(30-(attick%61))/2; cg=10; shine=5; break;
		
		case 51098:	c2=IRGB(0,0,abs(30-(attick%61))/2+10); break;

		case 51600:
			sprite=sprite+((int)(((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick))%8)*2);
			break;
		case 51601:
			sprite=sprite+((int)(((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick))%8)*2);
			break;

		case 51617:	cr=-30; break;	// leather gloves

		case 51625:	help=((int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%10);	// glowing steel door
				if (help>4) help=9-help;
				sprite=sprite+help;
				break;

		case 51632:	light=-20; shine=5; cr=10; break; // shrike stone

		case 56002:	help=(attick/2)%8; if (help>3) sprite=56002+7-help; else sprite=56002+help; break; // pulsing ring

// !! -------------------- !! pseudo sprites start here !! --------------------- !!
		case 59000:	sprite=14205; c2=IRGB(16,12,0); break; // edemon book orange
		case 59001:	sprite=14207; c2=IRGB(16,12,0); break; // edemon book orange
		case 59002:	// lava ground dark
		case 59003:
		case 59004:
		case 59005:
		case 59006:
		case 59007:
		case 59008:
		case 59009:
		case 59010:	sprite=sprite-59002+14330; light=-20; break;
		case 59011:	// lava ground not that dark
		case 59012:
		case 59013:
		case 59014:
		case 59015:
		case 59016:
		case 59017:
		case 59018:
		case 59019:	sprite=sprite-59011+14330; light=-10; break;
		case 59020:	sprite=14256; c2=IRGB(12,0,0); scale=75; break;

		case 59021:	// edemon loader ground growing red
		case 59022:
		case 59023:
		case 59024:
		case 59025:
		case 59026:
		case 59027:
		case 59028:
			sprite=sprite-59021+14241;
			c2=IRGB(16,0,0);
			break;
		case 59029:	// edemon loader ground red
			sprite=14248+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8;
			c2=IRGB(16,0,0);
			break;
		
		case 59030:	sprite=14257; c2=IRGB(20,0,0); break;	// edemon loader platform red
		case 59031:	sprite=14257; c2=IRGB(18,0,0); break;	// edemon loader platform red
		case 59032:	sprite=14257; c2=IRGB(16,0,0); break;	// edemon loader platform red
		case 59033:	sprite=14257; c2=IRGB(14,0,0); break;	// edemon loader platform red
                case 59034:	// edemon loader platform red
		case 59035:
		case 59036:
		case 59037:
		case 59038:
		case 59039:
			sprite=sprite-59034+14257;
			c2=IRGB(12,0,0);
			break;

		case 59040:	sprite=14256; c2=IRGB(14,0,0); scale=80; break;	// edemon loader platform red
		case 59041:	sprite=14256; c2=IRGB(16,0,0); scale=90; break;	// edemon loader platform red
		case 59042:	sprite=14256; c2=IRGB(18,0,0); scale=100; break; // edemon loader platform red
		case 59043:	sprite=14256; c2=IRGB(20,0,0); scale=110; break; // edemon loader platform red

		case 59044:
		case 59045:
		case 59046:
		case 59047:
		case 59048:
		case 59049:
		case 59050:
		case 59051:
		case 59052:
		case 59053:	sprite=sprite+20909-59044; cr=20; cg=10; light=30; break;	// palisade

		case 59054:
		case 59055:
		case 59056:
		case 59057:
		case 59058:
		case 59059:
		case 59060:
		case 59061:
		case 59062:
		case 59063:
		case 59064:
		case 59065:
		case 59066:
		case 59067:	sprite=sprite+21302-59054; cr=10; light=-20; break;	// krüge

		case 59068:
		case 59069:
		case 59070:
		case 59071:
		case 59072:
		case 59073:
		case 59074:
		case 59075:
		case 59076:
		case 59077:
		case 59078:
		case 59079:
		case 59080:
		case 59081:	sprite=sprite+21302-59068; cr=15; light=-30; break;	// krüge

		case 59082:
		case 59083:
		case 59084:
		case 59085:	sprite=sprite+21252-59082; scale=80; break;	// weapons decoration

		case 59086:
		case 59087:
		case 59088:
		case 59089:
		case 59090:
		case 59091:
		case 59092:
		case 59093:	sprite=sprite+21256-59086; cr=40; cg=40; light=-10; break;	// shields decoration
			
		case 59094:
		case 59095:
		case 59096:
		case 59097:
		case 59098:
		case 59099:     sprite=sprite+21209-59094; sat=10; cr=-10; cg=-10; light=10; cb=5; break;	// rich table/chair

		case 59100:
		case 59101:
		case 59102:
		case 59103:
		case 59104:
		case 59105:     sprite=sprite+21209-59100; sat=15; cr=-30; cg=-30; light=15; cb=-5; break;	// rich table/chair

		case 59106:
		case 59107:	sprite=sprite+21209-59106; c3=IRGB(0,0,24); break;	// rich chair

		case 59108:
		case 59109:
		case 59110:
		case 59111:
		case 59112:
		case 59113:
		case 59114:
		case 59115:
		case 59116:	sprite=sprite+12090-59108; cr=30; cb=-20; cg=10; light=20; sat=5; break;
		case 59117:
		case 59118:
		case 59119:
		case 59120:	sprite=sprite+12110-59117; cr=30; cb=-20; cg=10; light=20; sat=5; break;

		case 59121:
		case 59122:
		case 59123:
		case 59124:
		case 59125:
		case 59126:
		case 59127:
		case 59128:
		case 59129:	sprite=sprite+20318-59121; cr=-10; cg=-10; sat=5; light=-60; break;

		case 59130:
		case 59131:
		case 59132:
		case 59133:
		case 59134:
		case 59135:
		case 59136:
		case 59137:
		case 59138:	sprite=sprite+20309-59130; sat=10; light=-90; break;
		
		case 59139:
		case 59140:	sprite=sprite+21269-59139; sat=5; light=-30; cg=30; break;

		case 59141:
		case 59142:	sprite=sprite+21309-59141; light=-80; cr=10; cg=45; break;
			
		case 59143:
		case 59144:	sprite=sprite+21240-59143; light=-120; cg=30; cb=-30; break;

		case 59145:
		case 59146:
		case 59147:
		case 59148:
		case 59149:
		case 59150:	sprite=sprite+21209-59145; light=-70; cg=30; sat=10; break;

		case 59151:
		case 59152:
		case 59153:
		case 59154:	sprite=sprite+21252-59151; scale=80; cg=30; light=-20; break;	// weapons decoration

		case 59155:
		case 59156:
		case 59157:
		case 59158:
		case 59159:
		case 59160:
		case 59161:
		case 59162:	sprite=sprite+14030-59155; light=-30; cr=10; break;

		case 59163:
		case 59164:
		case 59165:
		case 59166:
		case 59167:
		case 59168:
		case 59169:
		case 59170:	sprite=sprite+14060-59163; light=-30; cr=10; break;

		case 59171:
		case 59172:
		case 59173:
		case 59174:
		case 59175:
		case 59176:
		case 59177:
		case 59178:
		case 59179:
		case 59180:
		case 59181:
		case 59182:
		case 59183:
		case 59184:
		case 59185:
		case 59186:
		case 59187:
		case 59188:
		case 59189:
		case 59190:
		case 59191:
		case 59192:	sprite=sprite+14090-59171; light=-20; cg=20; break;

		case 59193:	sprite=14092; light=-5; break;
		case 59194:	sprite=0; break;

		case 59195:	c2=IRGB(abs(30-(attick%61))/4,abs(30-(attick%61))/2+5,abs(30-(attick%61))/4); light=abs(30-(attick%61))/3; shine=5; sprite=51085; break;
		case 59196:     sprite=51110+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8; c2=IRGB(0,16,0); break;
		case 59197:     sprite=51110+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8; c2=IRGB(16,12,0); break;
		case 59198:     sprite=51110+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8; c2=IRGB(16,0,0); break;
		case 59199:     sprite=51110+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8; c2=IRGB(0,0,16); break;

                case 59200:
		case 59201:
		case 59202:
		case 59203:
		case 59204:
		case 59205:
		case 59206:
		case 59207:
		case 59208:
		case 59209:
                case 59210:
		case 59211:
		case 59212:
		case 59213:
		case 59214:
		case 59215:
		case 59216:
		case 59217:
		case 59218:
		case 59219:
		case 59220:
		case 59221:
		case 59222:
		case 59223:
		case 59224:
		case 59225:
		case 59226:
		case 59227:
		case 59228:
		case 59229:
		case 59230:
		case 59231:
		case 59232:
		case 59233:
		case 59234:
		case 59235:
		case 59236:
		case 59237:
		case 59238:
		case 59239:	sprite=sprite-59200+10120; sat=20; light=-30; cr=50; cg=50; shine=50; break;

		case 59240:
		case 59241:
		case 59242:
		case 59243:
		case 59244:
		case 59245:
		case 59246:
		case 59247:
		case 59248:
		case 59249:	sprite=sprite-59240+10190; sat=20; light=-30; cr=50; cg=50; shine=50; break;

		case 59250:
		case 59251:
		case 59252:
		case 59253:
		case 59254:
		case 59255:
		case 59256:
		case 59257:
		case 59258:
		case 59259:	sprite=sprite-59250+10220; sat=20; light=-30; cr=50; cg=50; shine=50; break;

		case 59260:
		case 59261:
		case 59262:
		case 59263:
		case 59264:
		case 59265:
		case 59266:
		case 59267:
		case 59268:
		case 59269:	sprite=sprite-59260+10250; sat=20; light=-30; cr=50; cg=50; shine=50; break;

		case 59270:
		case 59271:
		case 59272:
		case 59273:
		case 59274:
		case 59275:
		case 59276:
		case 59277:
		case 59278:
		case 59279:	sprite=sprite-59270+10280; sat=20; light=-30; cr=50; cg=50; shine=50; break;



		case 59288:	sprite=50510; sat=20; light=50; cr=80; cg=80; shine=95; break;
                case 59280:	sprite=50025; sat=20; light=0; cr=50; cg=50; shine=60; break;
		case 59281:	sprite=50026; sat=20; light=0; cr=50; cg=50; shine=60; break;
		case 59282:	sprite=50122; sat=20; light=60; cr=80; cg=80; shine=85; break;
		case 59283:	sprite=50123; sat=20; light=60; cr=80; cg=80; shine=85; break;
		case 59284:	sprite=50124; sat=20; light=60; cr=80; cg=80; shine=85; break;
		case 59285:	sprite=50125; sat=20; light=60; cr=80; cg=80; shine=85; break;
		case 59286:	sprite=50126; sat=20; light=60; cr=80; cg=80; shine=85; break;
		case 59287:	sprite=50141; sat=20; light=0; cr=50; cg=50; shine=60; break;
		case 59289:	sprite=50512; sat=20; light=40; cr=80; cg=80; shine=85; break;
		case 59290:	sprite=50513; sat=20; light=60; cr=80; cg=80; shine=95; break;
		case 59291:	sprite=51617; sat=20; light=20; cr=65; cg=80; shine=95; break;

		case 59299:	sprite=51617; sat=20; light=40; shine=100; break;
		case 59300:
		case 59301:
		case 59302:
		case 59303:
		case 59304:
		case 59305:
		case 59306:
		case 59307:
		case 59308:
		case 59309:
                case 59310:
		case 59311:
		case 59312:
		case 59313:
		case 59314:
		case 59315:
		case 59316:
		case 59317:
		case 59318:
		case 59319:
		case 59320:
		case 59321:
		case 59322:
		case 59323:
		case 59324:
		case 59325:
		case 59326:
		case 59327:
		case 59328:
		case 59329:
		case 59330:
		case 59331:
		case 59332:
		case 59333:
		case 59334:
		case 59335:
		case 59336:
		case 59337:
		case 59338:
		case 59339:	sprite=sprite-59300+10120; sat=20; light=-10; shine=40; break;

		case 59340:
		case 59341:
		case 59342:
		case 59343:
		case 59344:
		case 59345:
		case 59346:
		case 59347:
		case 59348:
		case 59349:	sprite=sprite-59340+10190; sat=20; light=-10; shine=40; break;

		case 59350:
		case 59351:
		case 59352:
		case 59353:
		case 59354:
		case 59355:
		case 59356:
		case 59357:
		case 59358:
		case 59359:	sprite=sprite-59350+10220; sat=20; light=-10; shine=40; break;

		case 59360:
		case 59361:
		case 59362:
		case 59363:
		case 59364:
		case 59365:
		case 59366:
		case 59367:
		case 59368:
		case 59369:	sprite=sprite-59360+10250; sat=20; light=-10; shine=40; break;

		case 59370:
		case 59371:
		case 59372:
		case 59373:
		case 59374:
		case 59375:
		case 59376:
		case 59377:
		case 59378:
		case 59379:	sprite=sprite-59370+10280; sat=20; light=-10; shine=40; break;

		case 59388:	sprite=50510; sat=20; light=80; shine=95; break;
                case 59380:	sprite=50025; sat=20; light=0; shine=60; break;
		case 59381:	sprite=50026; sat=20; light=0; shine=60; break;
		case 59382:	sprite=50122; sat=20; light=90; shine=85; break;
		case 59383:	sprite=50123; sat=20; light=90; shine=85; break;
		case 59384:	sprite=50124; sat=20; light=90; shine=85; break;
		case 59385:	sprite=50125; sat=20; light=90; shine=85; break;
		case 59386:	sprite=50126; sat=20; light=90; shine=85; break;
		case 59387:	sprite=50141; sat=20; light=0; shine=60; break;
		case 59389:	sprite=50512; sat=20; light=40; shine=85; break;
		case 59390:	sprite=50513; sat=20; light=90; shine=95; break;

		case 59391:	sprite=51055; c1=IRGB(16,2,2); c2=IRGB(10,2,2); break;
		case 59392:	sprite=51055; c1=IRGB(2,16,2); c2=IRGB(10,2,2); break;
		case 59393:	sprite=51055; c1=IRGB(2,2,16); c2=IRGB(10,2,2); break;
		case 59394:	sprite=51055; c1=IRGB(16,16,2); c2=IRGB(10,2,2); break;

		case 59395:	sprite=51055; c1=IRGB(16,2,2); c2=IRGB(2,10,2); break;
		case 59396:	sprite=51055; c1=IRGB(2,16,2); c2=IRGB(2,10,2); break;
		case 59397:	sprite=51055; c1=IRGB(2,2,16); c2=IRGB(2,10,2); break;
		case 59398:	sprite=51055; c1=IRGB(16,16,2); c2=IRGB(2,10,2); break;

		case 59399:	sprite=51055; c1=IRGB(16,2,2); c2=IRGB(2,2,10); break;
		case 59400:	sprite=51055; c1=IRGB(2,16,2); c2=IRGB(2,2,10); break;
		case 59401:	sprite=51055; c1=IRGB(2,2,16); c2=IRGB(2,2,10); break;
		case 59402:	sprite=51055; c1=IRGB(16,16,2); c2=IRGB(2,2,10); break;

		case 59403:	sprite=51055; c1=IRGB(16,2,2); c2=IRGB(4,4,4); break;
		case 59404:	sprite=51055; c1=IRGB(2,16,2); c2=IRGB(4,4,4); break;

		case 59405:	
		case 59406:	
		case 59407:	
		case 59408:	
		case 59409:	
		case 59410:	
		case 59411:	
		case 59412:	
		case 59413:	sprite=sprite-59405+20815; light=60*edi; break;

		case 59414:	
		case 59415:	
		case 59416:	
		case 59417:	
		case 59418:	
		case 59419:	
		case 59420:	
		case 59421:	
		case 59422:	sprite=sprite-59414+20815; light=40*edi; break;

		case 59423:	
		case 59424:	
		case 59425:	
		case 59426:	
		case 59427:	
		case 59428:	
		case 59429:	
		case 59430:	
		case 59431:	sprite=sprite-59423+20815; light=20*edi; break;


		case 59432:
		case 59433:
		case 59434:
		case 59435:
		case 59436:
		case 59437:
		case 59438:
		case 59439:	
		case 59440:	sprite=sprite-59432+51057; cr=20; cg=20; light=10; scale=80; break;	//swamp beasts

		
		case 59441:
		case 59442:
		case 59443:
		case 59444:
		case 59445:
		case 59446:
		case 59447:
		case 59448:
		case 59449:	sprite=sprite-59441+51057; cr=25; light=-40; shine=0; break;

                case 59450:
		case 59451:
		case 59452:
		case 59453:
		case 59454:
		case 59455:	
		case 59456:
		case 59457:
		case 59458:	sprite=sprite-59450+51057; sat=8; light=20; scale=105; break;
		
		case 59459:
                case 59460:
		case 59461:
		case 59462:
		case 59463:
		case 59464:
		case 59465:
		case 59466:
		case 59467:	sprite=sprite-59459+51057; cg=10; light=-20; shine=10; scale=110; break;

		case 59468:	sprite=51068; cr=20; cg=20; light=10; scale=80; break;
		case 59469:	sprite=51068; cr=25; light=-40; shine=0; break;
		case 59470:	sprite=51068; sat=8; light=20; scale=105; break;
		case 59471:	sprite=51068; cg=10; light=-20; shine=10; scale=110; break;
		case 59472:	sprite=51068; cg=10; light=-20; shine=25; scale=150; break;
		
		case 59473:	sprite=51084; sat=20; light=25; shine=40; break;
		case 59474:	sprite=51084; sat=20; light=25; cr=50; cg=50; shine=50; break;

		case 59475:
		case 59476:
		case 59477:
		case 59478:
		case 59479:
		case 59480:
		case 59481:
		case 59482:
		case 59483:	sprite=sprite-59475+22462; light=10-40*edi; cr=-10; cg=5; shine=15; break;

		case 59484:	sprite=15447; break;	// copied here so they work as a normal door
		case 59485:	sprite=15486; break;	// copied here so they work as a normal door

		case 59486:	sprite=15457; break;	// copied here so they work as a normal door
		case 59487:	sprite=15488; break;	// copied here so they work as a normal door

		case 59488:	sprite=20640; cr=50; break;
		case 59489:	sprite=20640; cg=50; break;
		case 59490:	sprite=20640; cb=50; break;
		case 59491:	sprite=20640; cr=40; cg=40; light=20; break;
		case 59492:	sprite=20640; sat=20; light=-50; break;
		case 59493:	sprite=20640; sat=20; light=50; break;

		
		case 59494:     // green mr_wall_torch_ds
			sprite=sprite-59494+20044+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cg=50; cr=-100;
			break;
		case 59495:     // green mr_wall_torch_ls
			sprite=sprite-59495+20054+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cg=50; cr=-100;
			break;
		case 59496:     // green mr_wall_torch_lh
			sprite=sprite-59496+20064+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cg=50; cr=-100;
			break;
		case 59497:     // green mr_wall_torch_dh		
                        sprite=sprite-59497+20074+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cg=50; cr=-100;
			break;

		case 59498:
		case 59499:
		case 59500:
		case 59501:
		case 59502:
		case 59503:
		case 59504:
		case 59505:
		case 59506:	sprite=sprite-59498+20636; cg=15; light=-40; break;	// green lighted floor

		case 59507:
		case 59508:
		case 59509:
		case 59510:
		case 59511:
		case 59512:	sprite=sprite-59507+15279; cg=20; light=-20; break;	// green lighted bones

		case 59513:	sprite=15432; cg=20; light=-20; break;			// green lighted skelly chair
		case 59514:	sprite=15439; cg=20; light=-20; break;			// empty green lighted skelly chair

		case 59515:     // red mr_wall_torch_ds
			sprite=sprite-59515+20044+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cr=50; cg=-50;
			break;
		case 59516:     // red mr_wall_torch_ls
			sprite=sprite-59516+20054+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cr=50; cg=-50;
			break;
		case 59517:     // red mr_wall_torch_lh
			sprite=sprite-59517+20064+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cr=50; cg=-50;
			break;
		case 59518:     // red mr_wall_torch_dh		
                        sprite=sprite-59518+20074+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; cr=50; cg=-50;
			break;

		case 59519:
		case 59520:
		case 59521:
		case 59522:
		case 59523:
		case 59524:
		case 59525:
		case 59526:
		case 59527:	sprite=sprite-59519+20636; cr=15; light=-40; break;	// red lighted floor

		case 59528:
		case 59529:
		case 59530:
		case 59531:
		case 59532:
		case 59533:	sprite=sprite-59528+15279; cr=20; light=-20; break;	// red lighted bones

		case 59534:	sprite=15432; cr=20; light=-20; break;			// red lighted skelly chair
		case 59535:	sprite=15439; cr=20; light=-20; break;			// empty red lighted skelly chair

		case 59536:
		case 59537:
		case 59538:
		case 59539:
		case 59540:
		case 59541:
		case 59542:
		case 59543:
		case 59544:	sprite=sprite-59536+20636; light=-40; break;	// unlighted floor

		case 59545:	
		case 59546:	sprite=sprite-59545+20122; sat=20; cg=60; light=20; shine=30; break;

		case 59547:	
		case 59548:	sprite=sprite-59547+20122; sat=20; cr=60; light=20; shine=30; break;

		case 59549:	sprite=50004; sat=20; cg=60; light=20; shine=30; break;
		case 59550:	sprite=50004; sat=20; cr=60; light=20; shine=30; break;

		case 59551:	sprite=15432; break;			// nolight skelly chair
		case 59552:	sprite=15439; break;			// empty nolight skelly chair
		case 59553:	sprite=20779; light=70; shine=70; cr=75; cg=75; break;

		case 59554:
		case 59555:
		case 59556:
		case 59557:
		case 59558:
		case 59559:
		case 59560:
		case 59561:
		case 59562:	sprite=sprite+20300-59554; light=-50; cr=15; break;	// dark orange sand ground

		case 59563:
		case 59564:
		case 59565:
		case 59566:
		case 59567:
		case 59568:
		case 59569:
		case 59570:	sprite=sprite+14030-59563; light=-50; cr=30; cg=10; break;	// dark orange stone walls

		case 59571:
		case 59572:
		case 59573:
		case 59574:	sprite=sprite+20272-59571; c1=IRGB(20,8,0); break;

		case 59575:
		case 59576:
		case 59577:
		case 59578:
		case 59579:
		case 59580:
		case 59581:
		case 59582:
		case 59583:	sprite=sprite+20327-59575; cr=5; sat=10; light=-100; break;

		case 59584:
		case 59585:
		case 59586:
		case 59587:
		case 59588:
		case 59589:
		case 59590:
		case 59591:
		case 59592:	sprite=sprite+20327-59584; cr=5; sat=7; light=-50; break;

		case 59593:
		case 59594:
		case 59595:
		case 59596:
		case 59597:
		case 59598:
		case 59599:
		case 59600:	sprite=sprite+14060-59593; light=15; cr=-10; cg=-15; cb=5; break;


		case 59601:
		case 59602:
		case 59603:
		case 59604:
		case 59605:
		case 59606:
		case 59607:
		case 59608:
		case 59609:
		case 59610:
		case 59611:
		case 59612:
		case 59613:
		case 59614:
		case 59615:
		case 59616:
		case 59617:
		case 59618:	sprite=sprite+23900-59601; light-=30; break;

		case 59619:
		case 59620:
		case 59621:
		case 59622:
		case 59623:
		case 59624:
		case 59625:
		case 59626:
		case 59627:	sprite=sprite+23036-59619; light-=30; break;

		case 59628:
		case 59629:
		case 59630:
		case 59631:
		case 59632:
		case 59633:
		case 59634:
		case 59635:
		case 59636:	sprite=sprite+23927-59628; light-=30; break;

		case 59637:
		case 59638:
		case 59639:
		case 59640:
		case 59641:
		case 59642:
		case 59643:
		case 59644:
		case 59645:	sprite=sprite+21789-59637; light=15; cr=25; cg=10; break;

		case 59646:
		case 59647:
		case 59648:
		case 59649:
		case 59650:
		case 59651:
		case 59652:
		case 59653:
		case 59654:	sprite=sprite+12050-59646; sat=12; light=25; cr=20; cg=15; break;

		case 59655:	sprite=51099; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		case 59656:	sprite=51100; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		case 59657:	sprite=51101; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		case 59658:	sprite=51102; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		case 59659:	sprite=51103; sat=20; cr=40; cg=30; light=40; scale=95; break;	  	// nomad: wolf1
		
		case 59660:	sprite=51099; sat=20; light=125; scale=110; break;  			// nomad: white wolf
		case 59661:	sprite=51100; sat=20; light=125; scale=110; break;  			// nomad: white wolf
		case 59662:	sprite=51101; sat=20; light=125; scale=110; break;  			// nomad: white wolf
		case 59663:	sprite=51102; sat=20; light=125; scale=110; break;  			// nomad: white wolf
		case 59664:	sprite=51103; sat=20; light=125; scale=110; break;  			// nomad: white wolf

		case 59665:     sprite=51110+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%8; c2=IRGB(16,16,24); break;

		case 59666:
		case 59667:
		case 59668:
		case 59669:	sprite=sprite-59666+51104; cg=40; light=-90; shine=0; break;
		
		case 59670:	
		case 59671:	
		case 59672:	
		case 59673:	sprite=sprite-59670+17000; cg=40; light=-50; shine=0; break;

		case 59674:
		case 59675:
		case 59676:
		case 59677:     sprite=sprite-59674+17032; cg=40; light=-50; shine=0; break;
		
		case 59678:
		case 59679:
		case 59680:
		case 59681:	sprite=sprite-59678+17048; light=30; cr=40; shine=10; break;

                case 59682:
		case 59683:
		case 59684:
		case 59685:	sprite=sprite-59682+51104; cg=30; cr=30; light=-90; shine=0; break;		
		case 59686:	
		case 59687:	
		case 59688:	
		case 59689:	sprite=sprite-59686+17000; cg=30; cr=30; light=-50; shine=0; break;
		case 59690:
		case 59691:
		case 59692:
		case 59693:     sprite=sprite-59690+17032; cg=30; cr=30; light=-50; shine=0; break;

		case 59694:
		case 59695:
		case 59696:
		case 59697:	sprite=sprite-59694+51104; cr=40; light=-90; shine=0; break;		
		case 59698:	
		case 59699:	
		case 59700:	
		case 59701:	sprite=sprite-59698+17000; cr=40; light=-50; shine=0; break;
		case 59702:
		case 59703:
		case 59704:
		case 59705:     sprite=sprite-59702+17032; cr=40; light=-50; shine=0; break;

		case 59706:
		case 59707:
		case 59708:
		case 59709:	sprite=sprite-59706+51104; cb=50; cr=-10; light=-100; shine=0; break;		
		case 59710:	
		case 59711:	
		case 59712:	
		case 59713:	sprite=sprite-59710+17000; cb=50; cr=-10; light=-60; shine=0; break;
		case 59714:
		case 59715:
		case 59716:
		case 59717:     sprite=sprite-59714+17032; cb=50; cr=-10; light=-60; shine=0; break;

		case 59718:
		case 59719:
		case 59720:
		case 59721:	sprite=sprite-59718+51104; cb=40; cr=-10; light=-40; shine=5; break;		
		case 59722:	
		case 59723:	
		case 59724:	
		case 59725:	sprite=sprite-59722+17000; cb=40; cr=-10; light=-10; shine=5; break;
		case 59726:
		case 59727:
		case 59728:
		case 59729:     sprite=sprite-59726+17032; cb=40; cr=-10; light=-10; shine=5; break;


		case 59730:     // blue mr_wall_torch_ds
			sprite=20044+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; sat=20; cb=50;
			break;
		case 59731:     // blue mr_wall_torch_ls
			sprite=20054+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; sat=20; cb=50;
			break;
		case 59732:     // blue mr_wall_torch_lh
			sprite=20064+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; sat=20; cb=50;
			break;
		case 59733:     // blue mr_wall_torch_dh		
                        sprite=20074+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=-20; sat=20; cb=50;
			break;

		case 59734:     // blue mr_wall_torch_ds
			sprite=20044+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=20; sat=20; cb=30;
			break;
		case 59735:     // blue mr_wall_torch_ls
			sprite=20054+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=20; sat=20; cb=30;
			break;
		case 59736:     // blue mr_wall_torch_lh
			sprite=20064+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=20; sat=20; cb=30;
			break;
		case 59737:     // blue mr_wall_torch_dh		
                        sprite=20074+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/4)%8;
			light=20; sat=20; cb=30;
			break;

		case 59738:     sprite=51118; c2=IRGB(0,16,0); break;

		case 59739:
		case 59740:
		case 59741:
		case 59742:	sprite=sprite-59739+17048; sat=20; light=30; cg=50; shine=10; break;

		case 59743:	// edemon suspensor white
			sprite=14200+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/3)%4;
			c2=IRGB(16,16,24);
			break;

		case 59744:	// moors ground tiles
		case 59745:	
		case 59746:	
		case 59747:	
		case 59748:	
		case 59749:	
		case 59750:	
		case 59751:	
		case 59752:	
		case 59753:	
		case 59754:	
		case 59755:	
		case 59756:	
		case 59757:	
		case 59758:	
		case 59759:	
		case 59760:	
		case 59761:	sprite=sprite-59744+20797; light=65; sat=12; shine=18; cb=5; break;

		case 59762:	
		case 59763:	
		case 59764:	
		case 59765:	
		case 59766:	
		case 59767:	
		case 59768:	
		case 59769:	
		case 59770:	sprite=sprite-59762+21780; light=-20; shine=5; cr=10; break; // shrike stones

		case 59771:	sprite=10318; sat=15; cg=70; light=-40; break;
		case 59772:	sprite=50337; scale=150; sat=20; cr=80; shine=10; light=-10; break;

		case 59780:	
		case 59781:
		case 59782:
		case 59783:
		case 59784:
		case 59785:
		case 59786:
		case 59787:
		case 59788:	sprite=sprite-59780+30060; shine=10; light=-55; cg=50; cr=20; break;

		case 59790:
		case 59791:
		case 59792:
		case 59793:
		case 59794:
		case 59795:
		case 59796:
		case 59797:	sprite=sprite-59790+14030; cg=50; light=-55; break;

		case 59798:	sprite=10004+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
				light=-20; cg=100; cr=-100;
				break;
		case 59799:	sprite=50023; light=-20; cg=100; cr=-100; break;

		case 59800:	sprite=10004+((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
				light=-20; cb=100; cr=-100;
				break;
		case 59801:	sprite=50023; light=-20; cb=100; cr=-100; break;		

		case 59802:	// sewer outlet
			sprite=22500+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			c3=IRGB(4,6,0);
			break;
		case 59803:	// sewer ground
			sprite=22508+(int)((mn%MAPDX+originx)+(mn/MAPDX+originy)*256+(attick)/2)%8;
			c3=IRGB(4,6,0);
			break;
		case 59804:	sprite=50075; scale=35; break;	// small iron pot



// -- ankhs pseudo sprites -----------------------------------------------------------------------------------------------------------

                case 60000: case 60001: case 60002: case 60003: case 60004:                             // white berries
                case 60005: case 60006: case 60007: case 60008: case 60009:                             // white berries
                case 60010: case 60011: case 60012: case 60013: case 60014:                             // white berries
                                sprite=11020+(sprite-60000); sat=10; light=125; scale=70; break;

                case 60015: case 60016: case 60017: case 60018: case 60019:                             // norther land wallset
                case 60020: case 60021: case 60022: case 60023: case 60024:
                case 60025: case 60026: case 60027: case 60028: case 60029:
                case 60030: case 60031: case 60032: case 60033: case 60034:
                case 60035: case 60036: case 60037: case 60038: case 60039:
                case 60040: case 60041:
                                sprite=14433+(sprite-60015); sat=10; break;

// !! -------------------- !! pseudo sprites end here !! --------------------- !!
	}
	
	if (sprite>=100000) {
		nr=trans_charno((sprite-100000)/1000,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);
		sprite=nr*1000+sprite%1000+100000;
	}

        if (pscale) *pscale=scale;
	if (pcr) *pcr=cr;
	if (pcg) *pcg=cg;
	if (pcb) *pcb=cb;
	if (plight) *plight=light;
	if (psat) *psat=sat;
	if (pc1) *pc1=c1;
	if (pc2) *pc2=c2;
	if (pc3) *pc3=c3;	
	if (pshine) *pshine=shine;

        return sprite;
}

// csprite

int get_player_sprite(int nr, int zdir, int action, int step, int duration)
{
        int base;
	
	if (nr>100) nr=trans_charno(nr,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);

        base=100000+nr*1000;

	if (action==0) switch(nr) {
			       case 45:	// !!!
                               case 63:
			       case 64:
			       case 68:
			       case 69:
			       case 73:
			       case 74:
			       case 78:
			       case 79:
			       case 83:
			       case 84:
			       case 88:
			       case 89:
			       case 93:
			       case 94:
			       case 98:
			       case 99:
                               case 103:
			       case 104:
			       case 108:
			       case 109:
			       case 113:
			       case 114:
			       case 118:
			       case 119:
			       case 360:
					action=60;
					step=tick%16;
					duration=16;
					break;
			       default: break;
	}

        if (nr==21) { // spiders action override
                if (action==2 || action==3 || (action>=6 && action<=49) || action>60) action=4;
        }

        switch (action) {
                case 0:  return base+  0+zdir*1;                                // idle
                case 1:  return base+  8+zdir/1*8 + step*8/duration;            // walk
                case 2:  return base+104+zdir/2*8 + step*8/duration;            // take
                case 3:  return base+104+zdir/2*8 + step*8/duration;            // drop
                case 4:  return base+136+zdir/2*8 + step*8/duration;            // attack1
                case 5:  return base+168+zdir/2*8 + step*8/duration;            // attack2
                case 6:  return base+200+zdir/2*8 + step*8/duration;            // attack3
                case 7:  return base+ 72+zdir/2*8 + step*8/duration;            // use

                case 10: return base+232+zdir/1*8 + step*4/duration;            // fireball 1
                case 11: return base+236+zdir/1*8 + step*4/duration;            // fireball 2
                case 12: return base+232+zdir/1*8 + step*4/duration;            // lightning ball 1
                case 13: return base+236+zdir/1*8 + step*4/duration;            // lightning ball 2


                case 14:  return base+296+zdir/2*8 + step*8/duration;           // magic shield
                case 15:  return base+296+zdir/2*8 + step*8/duration;           // flash
                case 16:  return base+296+zdir/2*8 + step*8/duration;           // bless - self
                case 17:  return base+232+zdir/1*8 + step*4/duration;           // bless 1
                case 18:  return base+236+zdir/1*8 + step*4/duration;           // bless 2
                case 19:  return base+296+zdir/2*8 + step*8/duration;           // heal - self
                case 20:  return base+232+zdir/1*8 + step*4/duration;           // heal 1
                case 21:  return base+236+zdir/1*8 + step*4/duration;           // heal 2
                case 22:  return base+296+zdir/2*8 + step*8/duration;           // freeze
                case 23:  return base+296+zdir/2*8 + step*8/duration;           // warcry
		case 24:  return base+ 72+zdir/2*8 + step*8/duration;           // give
		case 25:  return base+296+zdir/2*8 + step*8/duration;           // earth command
		case 26:  return base+296+zdir/2*8 + step*8/duration;           // earth mud
		case 27:  return base+296+zdir/2*8 + step*8/duration;           // pulse
		case 28:  return base+296+zdir/2*8 + step*8/duration;           // pulse


		case 50:  return base+328+zdir/2*8 + step*8/duration;           // die
		case 60:  return base+800+zdir/1*8 + step*8/duration;		// idle animated
        }
	return base;
}

#pragma argsused
void trans_csprite(int mn, struct map *cmap, int attick)
{
	int dirxadd[8]={+1, 0,-1,-2,-1, 0,+1,+2 };
	int diryadd[8]={+1,+2,+1, 0,-1,-2,-1, 0 };
        // int base;
        int csprite;
	int scale,cr,cg,cb,light,sat,c1,c2,c3,shine;

        if (playersprite_override && mn==mapmn(MAPDX/2,MAPDY/2)) csprite=playersprite_override; else csprite=cmap[mn].csprite;

	csprite=trans_charno(csprite,&scale,&cr,&cg,&cb,&light,&sat,&c1,&c2,&c3,&shine);

        cmap[mn].rc.sprite=get_player_sprite(csprite,cmap[mn].dir-1,cmap[mn].action,cmap[mn].step,cmap[mn].duration);
	cmap[mn].rc.scale=scale;

	cmap[mn].rc.shine=shine;
	cmap[mn].rc.cr=cr;
	cmap[mn].rc.cg=cg;
	cmap[mn].rc.cb=cb;
	cmap[mn].rc.light=light;
	cmap[mn].rc.sat=sat;
	
	if (cmap[mn].csprite<120) {
                cmap[mn].rc.c1=player[cmap[mn].cn].c1;
		cmap[mn].rc.c2=player[cmap[mn].cn].c2;
		cmap[mn].rc.c3=player[cmap[mn].cn].c3;
	} else {
		cmap[mn].rc.c1=c1;
		cmap[mn].rc.c2=c2;
		cmap[mn].rc.c3=c3;		
	}

        if (cmap[mn].duration && cmap[mn].action==1) {
                cmap[mn].xadd=20*(cmap[mn].step)*dirxadd[cmap[mn].dir-1]/cmap[mn].duration;
                cmap[mn].yadd=10*(cmap[mn].step)*diryadd[cmap[mn].dir-1]/cmap[mn].duration;
        }
        else {
                cmap[mn].xadd=0;
                cmap[mn].yadd=0;		
        }	
}

int get_lay_sprite(int sprite,int lay)
{
	switch(sprite) {
		case 14363: case 14364: case 14365: case 14366:
			return GND_LAY;

		case 14004:
		case 14034:
		case 14064:
		case 14096:
		case 15250:
		case 20441:
		case 21196:
		case 40004:
		case 50077:
		case 59159:
		case 59167:
                case 50143:
                case 20469: case 20470: case 20471: case 20472:
                case 20477: case 20478: case 20479: case 20480:
                case 23239: case 23240: case 23241: case 23242:
                case 60019: case 60020: case 60021: case 60022:
			return GME_LAY;

		case 51096: return GND_LAY-10;
	}

	return lay;
}

int get_offset_sprite(int sprite,int *px,int *py)
{
	int x=0,y=0;

	switch(sprite) {
		case 16035:
		case 16036:     x=6; y=8; break;
		case 16037:
		case 16038:	x=-6; y=8; break;

		case 16051:
		case 16052:     x=6; y=8; break;
		case 16053:
		case 16054:	x=-6; y=8; break;

		case 16055:
		case 16056:     x=6; y=8; break;
		case 16057:
		case 16058:	x=-6; y=8; break;

		case 21681:
		case 21682:     x=6; y=8; break;
		case 21683:
		case 21684:	x=-6; y=8; break;

		case 21685:
		case 21686:     x=6; y=8; break;
		case 21687:
		case 21688:	x=-6; y=8; break;
	}

	if (px) *px=x;
	if (py) *py=y;

	if ((x || y)) return 1;
	else return 0;
}

int no_alpha_sprite(int sprite)
{
	if (sprite>=21430 && sprite<=21710) return 31;	// ice area, ice palace
	
	switch(sprite) {
		case 21350:
		case 21351:
		case 21352:
		case 21353:
		case 21354:
		case 21355:
		case 21356:
		case 21357:

		case 21410:	
		case 21411:	
		case 21412:	
		case 21413:	
		case 21414:	
		case 21415:	
		case 21416:	
		case 21417:	
		case 21418:	
		case 21419:	
		case 21420:	
		case 21421:	
		case 21422:	
		case 21423:	
		case 21424:	
		case 21425:	
		case 21426:	
		case 21427:	return 16;

		default:	return 0;
	}

}

int additional_sprite(int sprite,int attick)
{
	switch(sprite) {
		case 50495: case 50496: case 50497: case 50498:	
			return 50500+attick%6;

		default: return 0;
	}
}
