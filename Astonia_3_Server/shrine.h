/*

$Id: shrine.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: shrine.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:45  sam
Added RCS tags


*/

#define MAXSHRINE	256

#define DEATH_SHRINE		(51)
#define DEATH_SHRINE_INDEX	(DEATH_SHRINE/32)
#define DEATH_SHRINE_BIT	(1u<<(DEATH_SHRINE&31))

struct shrine_ppd
{
	unsigned int used[MAXSHRINE/32];
	unsigned char continuity;
};

