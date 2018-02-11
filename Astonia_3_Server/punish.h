/*

$Id: punish.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: punish.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:38  sam
Added RCS tags


*/

struct punishment
{
	int level;
	int exp;
	int karma;
        char reason[80];
};

int punish(int pID,struct character *co,int level,char *reason,int *plock,int *pkick);
int unpunish(int pID,struct character *co,int ID,int *plock,int *pkick);
void list_punishment(int rID,struct punishment *pun,int cID,int date,int ID);
