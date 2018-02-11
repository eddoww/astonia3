/*
 * $Id: balance.h,v 1.7 2008/01/03 10:11:36 devel Exp $
 *
 * $Log: balance.h,v $
 * Revision 1.7  2008/01/03 10:11:36  devel
 * slight increase of fireball damage
 *
 * Revision 1.6  2007/09/21 11:04:53  devel
 * tactics fix
 *
 * Revision 1.5  2007/07/24 18:58:10  devel
 * rage and flash stronger
 *
 * Revision 1.4  2007/05/09 11:34:32  devel
 * increased strength of rage, fire
 *
 * Revision 1.3  2007/02/26 14:33:02  devel
 * weakened spells
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#define ATTACK_DIV		10		// turns an average of 50 weapon value into an average of 5 damage (before armor)

#define FIREBALL_DAMAGE		(85/ATTACK_DIV) // makes it the same as weapon damage
#define STRIKE_DAMAGE		(56/ATTACK_DIV) // ...

#define RAGEMOD			4		// rage skill is divided by this before being used

int skilldiff2percent(int diff);
int tactics2spell(int val);
int tactics2skill(int val);










