/*
 * $Id: escape.c,v 1.4 2007/09/21 11:00:58 devel Exp $
 *
 * $Log: escape.c,v $
 * Revision 1.4  2007/09/21 11:00:58  devel
 * tactics fix
 *
 * Revision 1.3  2007/09/11 17:08:02  devel
 * harder escape when warcried
 *
 * Revision 1.2  2007/08/09 11:12:50  devel
 * different display for escape texts
 *
 * Revision 1.1  2007/07/04 09:21:44  devel
 * Initial revision
 *
 */

#include <stdlib.h>
#include "server.h"
#include "talk.h"
#include "escape.h"
#include "act.h"
#include "balance.h"
#include "drvlib.h"
#include "tool.h"

static void check_enemies(int cn)
{
	int n,co;

	for (n=0; n<4; n++) {
		if (!(co=ch[cn].enemy[n])) continue;
		if (co<1 || co>=MAXCHARS) { ch[cn].enemy[n]=0; continue; }
		if (!ch[co].flags) { ch[cn].enemy[n]=0; continue; }
                if (ch[co].action!=AC_ATTACK1 && ch[co].action!=AC_ATTACK2 && ch[co].action!=AC_ATTACK3) { ch[cn].enemy[n]=0; continue; }
		if (ch[co].act1!=cn) { ch[cn].enemy[n]=0; continue; }
	}
}

int char_can_flee(int cn)
{
        int per=0,co,ste,m,chance;

        if (!ch[cn].enemy[0] &&
            !ch[cn].enemy[1] &&
            !ch[cn].enemy[2] &&
            !ch[cn].enemy[3]) return 1;

	check_enemies(cn);

	if (!ch[cn].enemy[0] &&
            !ch[cn].enemy[1] &&
            !ch[cn].enemy[2] &&
            !ch[cn].enemy[3]) return 1;

        if (ch[cn].escape_timer>=ticker) return 0;

        for (m=0; m<4; m++)
                if ((co=ch[cn].enemy[m])!=0) per+=ch[co].value[0][V_PERCEPT]+tactics2skill(ch[co].value[0][V_TACTICS]+14);

	if (has_spell(cn,IDR_WARCRY) || has_spell(cn,IDR_FREEZE)) ste=(ch[cn].value[0][V_STEALTH]+tactics2skill(ch[co].value[0][V_TACTICS]+14))/2;
	else ste=ch[cn].value[0][V_STEALTH]+tactics2skill(ch[co].value[0][V_TACTICS]+14);;

        chance=ste*10/per;
        if (chance<0) chance=0;
        if (chance>18) chance=18;

        if (RANDOM(20)<=chance) {
		if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"#01You manage to escape!");
                for (m=0; m<4; m++) ch[cn].enemy[m]=0;
                return 1;
        }

        ch[cn].escape_timer=ticker+TICKS;
        if (ch[cn].flags&CF_PLAYER) log_char(cn,LOG_SYSTEM,0,"#02You cannot escape!");

        return 0;
}

void add_enemy(int cn,int cc)
{
	check_enemies(cn);

        if (ch[cn].enemy[0]!=cc &&
            ch[cn].enemy[1]!=cc &&
            ch[cn].enemy[2]!=cc &&
            ch[cn].enemy[3]!=cc) {
                if (!ch[cn].enemy[0]) ch[cn].enemy[0]=cc;
                else if (!ch[cn].enemy[1]) ch[cn].enemy[1]=cc;
                else if (!ch[cn].enemy[2]) ch[cn].enemy[2]=cc;
                else if (!ch[cn].enemy[3]) ch[cn].enemy[3]=cc;
        }
}

















