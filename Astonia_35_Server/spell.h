/*
 * $Id: spell.h,v 1.5 2007/07/04 09:24:53 devel Exp $
 *
 * $Log: spell.h,v $
 * Revision 1.5  2007/07/04 09:24:53  devel
 * heal duration 10s -> 8s
 *
 * Revision 1.4  2007/07/02 08:47:23  devel
 * added IDR_HEAL
 *
 * Revision 1.3  2007/05/02 13:06:23  devel
 * reduced cost of freeze, fire and flash
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */
// spell costs moved here so that NPC drivers can use them

#define BLESSCOST	(5*POWERSCALE)
#define HEALCOST	heal cost is variable (half of the hp replaced)
#define FREEZECOST	(6*POWERSCALE)
#define MAGICSHIELDCOST	magic shield cost is variable (half of the shield value)
#define FLASHCOST	(6*POWERSCALE)
#define FIREBALLCOST	(6*POWERSCALE)

#define WARCRYCOST	(12*POWERSCALE)

#define WARCRYDURATION	(TICKS*10)
#define	BLESSDURATION	(TICKS*60*5)
#define FLASHDURATION	(TICKS*5)
#define FREEZEDURATION	(TICKS*6)
#define HEALDURATION	(TICKS*8)
#define HEALTICK	(TICKS/4)

#define MAGICSHIELDMOD	2






