/*

$Id: spell.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: spell.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:48  sam
Added RCS tags


*/
// spell costs moved here so that NPC drivers can use them

#define BLESSCOST	(2*POWERSCALE)
#define HEALCOST	heal cost is variable (half of the hp replaced)
#define FREEZECOST	(2*POWERSCALE)
#define MAGICSHIELDCOST	magic shield cost is variable (half of the shield value)
#define FLASHCOST	(3*POWERSCALE)
#define FIREBALLCOST	(3*POWERSCALE)

#define WARCRYCOST	warcry costs half endurace, no matter what

#define WARCRYDURATION	(TICKS*4)
#define	BLESSDURATION	(TICKS*60*2)
#define FLASHDURATION	(TICKS*2)
#define FREEZEDURATION	(TICKS*4)
