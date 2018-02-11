/*
 * $Id: balance.c,v 1.6 2007/12/30 12:49:18 devel Exp $ (c) 2007 D.Brockhaus
 *
 * $Log: balance.c,v $
 * Revision 1.6  2007/12/30 12:49:18  devel
 * added rage to immunity against freeze and WC
 *
 * Revision 1.5  2007/09/21 10:58:59  devel
 * tactics fixed
 *
 * Revision 1.4  2007/07/27 05:58:38  devel
 * flash stronger
 *
 * Revision 1.3  2007/07/24 18:53:37  devel
 * rage also affects immunity
 *
 * Revision 1.2  2007/02/26 14:31:37  devel
 * mage melee weakened
 * surround hit weakened
 *
 * Revision 1.1  2007/02/24 14:09:54  devel
 * Initial revision
 *
 */

#include "server.h"
#include "log.h"
#include "talk.h"
#include "btrace.h"
#include "balance.h"

int get_fight_skill(int cn)
{
	int in,m=0;

	if (cn<1 || cn>=MAXCHARS) {
		elog("get_fight_skill(): called with illegal character %d",cn);
		return 0;
	}

	in=ch[cn].item[WN_RHAND];

	if (in<0 || in>=MAXITEM) {
		elog("get_fight_skill(): character %s (%d) is wielding illegal item %d. Removing.",ch[cn].name,cn,in);
		ch[cn].item[WN_RHAND]=0;
		in=0;
	}

	if (!in || !(it[in].flags&IF_WEAPON)) {	// not wielding anything, or not wielding a weapon
		return ch[cn].value[0][V_HAND];
	}

	if (it[in].flags&IF_HAND) m=max(m,ch[cn].value[0][V_HAND]);
	if (it[in].flags&IF_DAGGER) m=max(m,ch[cn].value[0][V_DAGGER]);
	if (it[in].flags&IF_STAFF) m=max(m,ch[cn].value[0][V_STAFF]);
	if (it[in].flags&IF_SWORD) m=max(m,ch[cn].value[0][V_SWORD]);
	if (it[in].flags&IF_TWOHAND) m=max(m,ch[cn].value[0][V_TWOHAND]);

	return m;
}

double get_spell_average(int cn)
{
	return ( ch[cn].value[0][V_BLESS]+
		 ch[cn].value[0][V_HEAL]+
		 ch[cn].value[0][V_FREEZE]+
		 ch[cn].value[0][V_MAGICSHIELD]+
		 ch[cn].value[0][V_FLASH]+
		 ch[cn].value[0][V_FIRE])/6.0;
}

int tactics2skill(int val)
{
        return val*0.15;
}

int tactics2spell(int val)
{
        return val*0.15;
}

int tactics2melee(int val)
{
	return tactics2skill(val)*3;
}

int spellpower(int cn,int v)
{
	return ch[cn].value[0][v]+tactics2spell(ch[cn].value[0][V_TACTICS]);
}

int get_parry_skill(int cn)
{
	int val;
	
        if (ch[cn].value[1][V_PARRY]) val=get_fight_skill(cn)+ch[cn].value[0][V_PARRY]*2+tactics2melee(ch[cn].value[0][V_TACTICS]);
	else if (ch[cn].flags&CF_EDEMON) val=get_fight_skill(cn)*3.5;
	else if (ch[cn].value[1][V_MAGICSHIELD]) val=get_fight_skill(cn)+ch[cn].value[0][V_MAGICSHIELD]*2-min(10,ch[cn].level/2);
	else val=get_fight_skill(cn)+get_spell_average(cn)*2;

        return val;
}

int get_attack_skill(int cn)
{
	int val;
	
	if (ch[cn].value[1][V_ATTACK]) val=get_fight_skill(cn)+ch[cn].value[0][V_ATTACK]*2+tactics2melee(ch[cn].value[0][V_TACTICS]);
	else if (ch[cn].flags&CF_EDEMON) val=get_fight_skill(cn)*3.5;
	else {
		val=get_fight_skill(cn)+get_spell_average(cn)*2-ch[cn].level*3-20;
		if (val<0) val=0;		
	}

        return val;
}

int get_surround_attack_skill(int cn)
{
	int val;
	
	if (ch[cn].value[1][V_SURROUND]) val=get_fight_skill(cn)+
						ch[cn].value[0][V_SURROUND]*2+
						tactics2melee(ch[cn].value[0][V_TACTICS]) -20;
	else return 0;

	return val;
}

int skilldiff2percent(int diff)
{
	if (diff<- 62) return 10;
	if (diff<- 60) return 11;
	if (diff<- 58) return 12;
	if (diff<- 56) return 13;
	if (diff<- 54) return 14;
	if (diff<- 52) return 15;
	if (diff<- 50) return 16;
	if (diff<- 48) return 17;
	if (diff<- 46) return 18;
	if (diff<- 44) return 19;
        if (diff<- 42) return 20;
	if (diff<- 40) return 21;
	if (diff<- 38) return 22;
	if (diff<- 36) return 23;
	if (diff<- 34) return 24;
	if (diff<- 32) return 25;
	if (diff<- 30) return 26;
	if (diff<- 28) return 27;
	if (diff<- 26) return 28;
	if (diff<- 24) return 29;
	if (diff<- 22) return 30;
	
	if (diff>  62) return 90;
	if (diff>  60) return 89;
	if (diff>  58) return 88;
	if (diff>  56) return 87;
	if (diff>  54) return 86;
	if (diff>  52) return 85;
	if (diff>  50) return 84;
	if (diff>  48) return 83;
	if (diff>  46) return 82;
	if (diff>  44) return 81;
        if (diff>  42) return 80;
	if (diff>  40) return 79;
	if (diff>  38) return 78;
        if (diff>  36) return 77;
	if (diff>  34) return 76;
	if (diff>  32) return 75;
	if (diff>  30) return 74;
	if (diff>  28) return 73;
	if (diff>  26) return 72;
	if (diff>  24) return 71;
	if (diff>  22) return 70;
	
	switch(diff) {
		case -22:	return 31;
		case -21:	return 31;
		case -20:	return 32;
		case -19:	return 32;
		case -18:	return 33;
		case -17:	return 33;
		case -16:	return 34;
		case -15:	return 34;
		case -14:	return 35;
		case -13:	return 35;
		case -12:	return 36;
		case -11:	return 36;
		case -10:	return 37;
		case - 9:	return 37;
		case - 8:	return 38;
		case - 7:	return 38;
		case - 6:	return 39;
		case - 5:	return 39;
		case - 4:	return 40;
		case - 3:	return 42;
		case - 2:	return 44;
		case - 1:	return 47;
		case   0:	return 50;
		case   1:	return 53;
		case   2:	return 56;
		case   3:	return 59;
		case   4:	return 60;
		case   5:	return 61;
		case   6:	return 61;
		case   7:	return 62;
		case   8:	return 62;
		case   9:	return 63;
		case  10:	return 63;
		case  11:	return 64;
		case  12:	return 64;
		case  13:	return 65;
		case  14:	return 65;
		case  15:	return 66;
		case  16:	return 66;
		case  17:	return 67;
		case  18:	return 67;
		case  19:	return 68;
		case  20:	return 68;
		case  21:	return 69;
		case  22:	return 69;
		default:	btrace("impossible in skilldiff2percent"); return 50;
	}
}

// reduce skill_strength according to subjects immunity
// skill_strength is in the usual bounds (about 7...200)
// subject's immunity will be about the same
int immunity_reduction(int caster,int subject,int skill_strength)
{
	int immun,stren,diff,dam;
	extern int showspell;
	
	immun=ch[subject].value[0][V_IMMUNITY];
	if (ch[subject].value[1][V_TACTICS]) immun+=tactics2skill(ch[subject].value[0][V_TACTICS]+14);
	if (ch[subject].value[1][V_RAGE]) immun+=ch[subject].rage/POWERSCALE/RAGEMOD;

	diff=skill_strength-immun;
        //stren=max(0.0,1.0+diff/20.0+skill_strength/100.0)*POWERSCALE;
	
	dam=skilldiff2percent(diff);

	stren=(dam/100.0)*POWERSCALE;

	if (showspell) say(caster,"skill=%d, immun=%d, diff=%d, stren=%.2f",skill_strength,immun,diff,stren/1000.0);
	
        return stren;
}

// returns the resulting speed reduction for cn casting freeze on co
// str>=0 means freeze would fail.
int freeze_value(int cn,int co)
{
	int str,dam,diff,immun;

        immun=ch[co].value[0][V_IMMUNITY];
	if (ch[co].value[1][V_TACTICS]) immun+=tactics2skill(ch[co].value[0][V_TACTICS]+14);
	if (ch[co].value[1][V_RAGE]) immun+=ch[co].rage/POWERSCALE/RAGEMOD;
	
	str=spellpower(cn,V_FREEZE);

	diff=str-immun;
        if (diff<-50) return 0;

	dam=skilldiff2percent(diff);

        str=dam;

        if ((ch[cn].flags&CF_IDEMON) && ch[co].value[0][V_COLD]>ch[cn].value[1][V_DEMON]) {
		str-=(ch[co].value[0][V_COLD]-ch[cn].value[1][V_DEMON])*10;		
	}

	return -str;
}

// returns the resulting speed reduction for cn casting warcry on co
// str>=0 means warcry would fail.
int warcry_value(int cn,int co,int pwr)
{
	int str,immun,diff,dam;

	immun=ch[co].value[0][V_IMMUNITY];
	if (ch[co].value[1][V_TACTICS]) immun+=tactics2skill(ch[co].value[0][V_TACTICS]+14);
	if (ch[co].value[1][V_RAGE]) immun+=ch[co].rage/POWERSCALE/RAGEMOD;

	diff=pwr-immun;
	if (diff<-50) return 0;

	dam=skilldiff2percent(diff);
	str=dam;

	return -str*(1.0+str/400.0);;
}

int warcry_damage(int cn,int co,int pwr)
{
	return immunity_reduction(cn,co,pwr);
}

int fireball_damage(int cn,int co,int str)
{
        return immunity_reduction(cn,co,str)*FIREBALL_DAMAGE*(1.0+str/400.0);
}

int strike_damage(int cn,int co,int str)
{
        return immunity_reduction(cn,co,str)*STRIKE_DAMAGE*(1.0+str/400.0);
}





















