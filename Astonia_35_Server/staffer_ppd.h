/*

$Id: staffer_ppd.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: staffer_ppd.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:50  sam
Added RCS tags


*/

struct staffer_ppd
{
	int smugglecom_state;		// WARR
	int smugglecom_bits;		// WARR
	
	int carlos_state;		// RHORUN
	
	int countbran_state;		// WARR
	int countbran_bits;		// WARR
	int countessabran_state;	// WARR
	int daughterbran_state;		// WARR
	int spiritbran_state;		// WARR
	int guardbran_state;		// WARR
	int brennethbran_state;		// WARR	
	int forestbran_state;		// WARR	
	int forestbran_done;		// WARR

	int broklin_state;		// WHITE
	int aristocrat_state;		// WHITE
	int yoatin_state;		// WHITE

	int grinnich_state;		// WARR
	int shanra_state;		// WARR
	int centinel_count;		// WARR

	int dwarfchief_state;		// WARR
	int dwarfshaman_state;		// WARR
	int dwarfshaman_count;		// WARR
	int dwarfsmith_state;		// WARR
	int dwarfsmith_type;		// WARR
};

