/*

$Id: fight.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: fight.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:13  sam
Added RCS tags


*/

struct person
{
	unsigned int cn;
	unsigned int ID;

	unsigned short lastx,lasty;
	unsigned char visible;
	unsigned char hurtme;
};

struct fight_driver_data
{
	struct person enemy[10];

	int start_dist;		// distance from respawn point at which to start attacking
	int stop_dist;		// distance from respawn point at which to stop attacking
	int char_dist;		// distance from character we start attacking

	int home_x,home_y;	// position to compare start_dist and start_dist with

	int lasthit;
};
