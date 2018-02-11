/*
 * $Id: depot.h,v 1.3 2007/06/13 09:27:52 devel Exp $
 *
 * $Log: depot.h,v $
 * Revision 1.3  2007/06/13 09:27:52  devel
 * added payment tokens and free space to depot
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#define MAXDEPOT	120

struct depot_ppd
{
	unsigned int changes;		// serial number, gets increased with each change to depot contents
	unsigned int lastsave;		// serial number of last save
	unsigned int loaded;		// depot was loaded correctly and can be used
	int gold;			// gold in depot (bank account)
	int payment_tokens;		// number of payment tokens the player owns (for use with trader or own account)
	int dummy[11];			// free for future extensions
        struct item itm[MAXDEPOT];	// items in depot
};
int swap_depot(int cn,int nr);
void player_depot(int cn,int nr,int flag,int fast);
void depot_sort(int cn);








