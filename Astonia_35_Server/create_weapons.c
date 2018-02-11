/*
 * $Id: create_weapons.c,v 1.5 2007/09/03 08:57:36 devel Exp $
 *
 * $Log: create_weapons.c,v $
 * Revision 1.5  2007/09/03 08:57:36  devel
 * price change
 *
 * Revision 1.4  2007/07/09 11:21:03  devel
 * price changes
 *
 * Revision 1.3  2007/06/28 12:11:49  devel
 * staffs now require mages
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#include <stdio.h>

#define max(a,b)	((a) > (b) ? (a) : (b))

static char *staff_name[]={
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff",
	"Staff"
};

static char *dagger_name[]={
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger",
	"Dagger"
};

static char *sword_name[]={
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword",
	"Sword"
};

static char *twohanded_name[]={
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword",
	"Two-Handed Sword"
};

static char *desc[3]={
	"A %s made of iron.",
	"A %s made of steel.",
	"A %s made of mithril."
};

static char *name[4]={
	"dagger",
	"sword",
	"twohanded",
	"staff"
};

void create_weapon(char *textname[],char *xdesc,char *xname,int costbase,int spritebase,int damagebase,int quality,char *required,char *type,int two,int warrior,int mage)
{
	int n;

	for (n=0; n<10; n++) {
		printf("%s%dq%d:\n",xname,n+1,quality+1);
		printf("name=\"%s\"\n",textname[n]);
		printf("description=\""); printf(xdesc,textname[n]); printf("\"\n");
                printf("value=%d\n",costbase*(n+1)+costbase*3);
                printf("sprite=%d\n",spritebase+n);
		printf("mod_index=V_WEAPON\n");
		printf("mod_value=%d\n",damagebase+n*10+20*20);
		printf("mod_index=V_DEFENSE\n");
		printf("mod_value=-%d\n",n*2+1);
		printf("flag=IF_TAKE\n");
		printf("flag=IF_WNRHAND\n");
		if (two) printf("flag=IF_WNTWOHANDED\n");
                printf("flag=%s\n",type);
		if (warrior) printf("needs_class=1\n");
		if (mage) printf("needs_class=2\n");
		printf(";\n");
	}
}

int main(void)
{
	int n;
	static int basetab[3]={-2,0,4};
	static int costtab[3]={1,7,17};

	for (n=0; n<3; n++) {
		create_weapon(staff_name,desc[n],name[3],7*costtab[n],10230+n*10,basetab[n]+4,n,"V_STAFF","IF_STAFF",1,0,1);
	}

	for (n=0; n<3; n++) {
		create_weapon(dagger_name,desc[n],name[0],5*costtab[n],10170+n*10,basetab[n],n,"V_DAGGER","IF_DAGGER",0,0,0);
	}

	for (n=0; n<3; n++) {
		create_weapon(sword_name,desc[n],name[1],10*costtab[n],10200+n*10,basetab[n]+6,n,"V_SWORD","IF_SWORD",0,1,0);
	}
	for (n=0; n<3; n++) {
		create_weapon(twohanded_name,desc[n],name[2],15*costtab[n],10260+n*10,basetab[n]+12,n,"V_TWOHAND","IF_TWOHAND",1,1,0);
	}

	return 0;
}

















