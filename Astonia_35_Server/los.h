/*

$Id: los.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: los.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:20  sam
Added RCS tags


*/

#define MAXDIST		25
#define SIZE		(MAXDIST*2+1)

struct los
{
	int x,y;
	int xoff,yoff;
	int maxdist;
	unsigned char tab[SIZE][SIZE];
};

extern struct los *los;

#ifdef NEED_FAST_LOS
static inline int fast_los(int cn,int x,int y)
{
        x=x+los[cn].xoff;
	y=y+los[cn].yoff;

	if (los[cn].tab[x][y]) return 1;
	else return 0;
}
#endif

#ifdef NEED_FAST_LOS_LIGHT
static inline int fast_los_light(int cn,int x,int y,int maxdist)
{
        x=x+los[cn].xoff;
	y=y+los[cn].yoff;

	return los[cn].tab[x][y]>=maxdist ? 0 : los[cn].tab[x][y];
}
#endif


int init_los(void);
void reset_los(int xc,int yc);
int los_can_see(int cn,int xc,int yc,int tx,int ty,int maxdist);
int update_los(int cn,int xc,int yc,int maxdist);
