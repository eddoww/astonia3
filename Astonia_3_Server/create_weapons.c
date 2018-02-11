/*

$Id: create_weapons.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: create_weapons.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:05  sam
Added RCS tags


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

void create_weapon(char *textname[],char *xdesc,char *xname,int costbase,int spritebase,int damagebase,int quality,char *required,char *type,int two)
{
	int n;

	for (n=0; n<10; n++) {
		printf("%s%dq%d:\n",xname,n+1,quality+1);
		printf("name=\"%s\"\n",textname[n]);
		printf("description=\""); printf(xdesc,textname[n]); printf("\"\n");
                if (costbase>20) printf("value=%d\n",costbase*(1<<n));
		else printf("value=%d\n",costbase*n*n+5);
                printf("sprite=%d\n",spritebase+n);
		printf("mod_index=V_WEAPON\n");
		printf("mod_value=%d\n",damagebase+(n+1)*10);
		printf("req_index=%s\n",required);
		printf("req_value=%d\n",max(1,n*10));
		printf("flag=IF_TAKE\n");
		printf("flag=IF_WNRHAND\n");
		if (two) printf("flag=IF_WNTWOHANDED\n");
                printf("flag=%s\n",type);
		printf(";\n");
	}
}

int main(void)
{
	int n;
	static int basetab[3]={-2,0,4};
	static int costtab[3]={1,5,25};

	for (n=0; n<3; n++) {
		create_weapon(staff_name,desc[n],name[3],7*costtab[n],10230+n*10,basetab[n]-2,n,"V_STAFF","IF_STAFF",1);
	}

	for (n=0; n<3; n++) {
		create_weapon(dagger_name,desc[n],name[0],5*costtab[n],10170+n*10,basetab[n]-3,n,"V_DAGGER","IF_DAGGER",0);
	}

	for (n=0; n<3; n++) {
		create_weapon(sword_name,desc[n],name[1],10*costtab[n],10200+n*10,basetab[n],n,"V_SWORD","IF_SWORD",0);
	}
	for (n=0; n<3; n++) {
		create_weapon(twohanded_name,desc[n],name[2],15*costtab[n],10260+n*10,basetab[n]+3,n,"V_TWOHAND","IF_TWOHAND",1);
	}

	return 0;
}
