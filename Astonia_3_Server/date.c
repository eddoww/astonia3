/*

$Id: date.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: date.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:07  sam
Added RCS tags


*/

/*
date.c (C) 2001 D.Brockhaus

Game time and date. Also calculates stuff like sunrise/sunset, solstice (longest/shortest
day of the year), equinox (day and night of same length), the state of the moon and the
daylight created by sun and moon. It updates a bunch of global variables which can be used
to determine the current date etc.
*/

#include <time.h>

#include "server.h"
#include "talk.h"
#include "date.h"

#define STARTTIME	978303600	// 01/01/2001

#define DAYLEN		(60*60*2)
#define HOURLEN		(DAYLEN/24)
#define MINLEN		(HOURLEN/60)
#define WEEKLEN		(DAYLEN*7)
#define MONTHLEN	(DAYLEN*30)
#define YEARLEN		(MONTHLEN*12)
#define MOONLEN		(DAYLEN*28)

int dlight=0;

int newmoon=0;
int fullmoon=0;
int moonsize=0;

int solstice=0;
int equinox=0;
int summer_solstice=0;
int winter_solstice=0;
int spring_equinox=0;
int fall_equinox=0;

int sunrise=0;
int sunset=0;
int moonrise=0;
int moonset=0;
int moonlight=0;
int sunlight=0;

int minute=0;	// 0...59
int hour=0;	// 0...23
int mday=0;	// 0...29
int wday=0;	// 0...6
int yday=0;	// 0...359
int week=0;	// warning: weeks since beginning of time !
int month=0;	// 0...11
int year=0;	// 0...

int moon=0;	// warning: moons since beginning of time !
int moonday=0;	// 0...27

int realtime=0;	// seconds realtime since beginning of our epoch

extern int dlight_override;
//float new_system=0.0f;
//float old_system=1.0f;

void tick_date(void)
{
	int t,daytime;
	
	t=(time_now-STARTTIME);	// local time starts on 01/01/2001

	/*{
		int start=1054677600,stop=1062540000;
		if (time_now>start) {
			if (time_now<stop) {
				new_system=(double)(time_now-start)/(double)(stop-start);
			} else new_system=1.0;
		} else new_system=0.0;
		old_system=1.0f-new_system;
	}*/

        realtime=t;

        minute=(t/MINLEN)%60;
	hour=(t/HOURLEN)%24;

	week=(t/WEEKLEN);
        wday=(t-week*WEEKLEN)/DAYLEN;
	
	month=t/MONTHLEN;
	mday=(t-month*MONTHLEN)/DAYLEN;
	month=month%12;

	year=t/YEARLEN;
	yday=(t-year*YEARLEN)/DAYLEN;

	moon=t/MOONLEN;
	moonday=(t-moon*MOONLEN)/DAYLEN;

	if (yday==0) {		// winter solstice
		sunrise=(HOURLEN*6)+90*MINLEN;
		sunset=(HOURLEN*18)-90*MINLEN;
		
		solstice=1;
		summer_solstice=0;
		winter_solstice=1;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	} else if (yday<90) {	// winter/spring
		sunrise=(HOURLEN*6)+(90-yday)*MINLEN;
		sunset=(HOURLEN*18)-(90-yday)*MINLEN;
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	} else if (yday==90) {	// spring equinox
		sunrise=(HOURLEN*6);
		sunset=(HOURLEN*18);
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=1;
		spring_equinox=1;
		fall_equinox=0;
	} else if (yday<180) {	// spring/summer
		sunrise=(HOURLEN*6)-(yday-90)*MINLEN;
		sunset=(HOURLEN*18)+(yday-90)*MINLEN;
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	} else if (yday==180) {	// summer solstice
		sunrise=(HOURLEN*6)-90*MINLEN;
		sunset=(HOURLEN*18)+90*MINLEN;
		
		solstice=1;
		summer_solstice=1;
		winter_solstice=0;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	} else if (yday<270) {	// summer/fall
		sunrise=(HOURLEN*6)-(270-yday)*MINLEN;
		sunset=(HOURLEN*18)+(270-yday)*MINLEN;
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	} else if (yday==270) {	// fall equinox
		sunrise=(HOURLEN*6);
		sunset=(HOURLEN*18);
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=1;
		spring_equinox=0;
		fall_equinox=1;
	} else {		// fall/winter
		sunrise=(HOURLEN*6)+(yday-270)*MINLEN;
		sunset=(HOURLEN*18)-(yday-270)*MINLEN;
		
		solstice=0;
		summer_solstice=0;
		winter_solstice=0;

		equinox=0;
		spring_equinox=0;
		fall_equinox=0;
	}

	if (moonday==0) {	// new moon
                moonrise=6*HOURLEN;
		moonset=(moonrise+HOURLEN*12)%DAYLEN;
		moonsize=0;
		
		newmoon=1;
		fullmoon=0;
	} else if (moonday<14) {
		moonrise=6*HOURLEN+(moonday*HOURLEN*12/14);
		moonset=(moonrise+HOURLEN*12)%DAYLEN;
		moonsize=moonday;
		
		newmoon=0;
		fullmoon=0;
	} else if (moonday==14) {
		moonrise=18*HOURLEN;
		moonset=(moonrise+HOURLEN*12)%DAYLEN;
		moonsize=14;
		
		newmoon=0;
		fullmoon=1;
	} else {
		moonrise=(6*HOURLEN+(moonday*HOURLEN*12/14))%DAYLEN;
		moonset=(moonrise+HOURLEN*12)%DAYLEN;
		moonsize=28-moonday;
		
		newmoon=0;
		fullmoon=0;
	}

	daytime=t%DAYLEN;

	if (daytime<sunrise) sunlight=0;
	else if	(daytime<sunrise+HOURLEN) sunlight=(daytime-sunrise)*255/HOURLEN;
	else if (daytime<=sunset) sunlight=255;
	else if (daytime>sunset+HOURLEN) sunlight=0;
	else if (daytime>sunset) sunlight=255-(daytime-sunset)*255/HOURLEN;

	if (moonrise<moonset) {
		if (daytime<moonrise) moonlight=0;
		else if	(daytime<moonrise+HOURLEN) moonlight=(daytime-moonrise)*255/HOURLEN;
		else if (daytime<=moonset) moonlight=255;
		else if (daytime>moonset+HOURLEN) moonlight=0;
		else if (daytime>moonset) moonlight=255-(daytime-moonset)*255/HOURLEN;
	} else {
		if (daytime<=moonset) moonlight=255;
		else if (daytime<moonset+HOURLEN) moonlight=255-(daytime-moonset)*255/HOURLEN;
                else if (daytime<moonrise) moonlight=0;
		else if (daytime<moonrise+HOURLEN) moonlight=(daytime-moonrise)*255/HOURLEN;
		else if (daytime>=moonrise) moonlight=255;
        }

	moonlight=moonlight*moonsize/63;
	
	switch(areaID) {
		case 2:		dlight=96; break;
		case 21:	dlight=64; break;	// !!!!!!!!!!!!! hack !!!!!!!!!!!!!!
		case 23:	dlight=32; break;
		case 24:	dlight=32; break;
		default:	dlight=max(sunlight,moonlight);	break;
	}
	

	if (dlight_override) dlight=dlight_override;	
}

void showtime(int cn)
{
        log_char(cn,LOG_SYSTEM,0,"It's %02d:%02d on the %d/%d/%d. Sunrise is at %02d:%02d, sunset at %02d:%02d. Moonrise is at %02d:%02d, moonset at %02d:%02d.",
		hour,minute,month+1,mday+1,year,
		sunrise/HOURLEN,(sunrise%HOURLEN)/MINLEN,
		sunset/HOURLEN,(sunset%HOURLEN)/MINLEN,
		moonrise/HOURLEN,(moonrise%HOURLEN)/MINLEN,
		moonset/HOURLEN,(moonset%HOURLEN)/MINLEN);

	if (!fullmoon && !newmoon) {
		if (moonsize<3) log_char(cn,LOG_SYSTEM,0,"Quarter Moon.");
		else if (moonsize<10) log_char(cn,LOG_SYSTEM,0,"Half Moon.");
		else log_char(cn,LOG_SYSTEM,0,"Three Quarter Moon.");
	}

	if (newmoon) log_char(cn,LOG_SYSTEM,0,"Be careful, New Moon tonight!");
	if (fullmoon) log_char(cn,LOG_SYSTEM,0,"It's a fine day, Full Moon tonight!");
	
	if (summer_solstice) log_char(cn,LOG_SYSTEM,0,"It's a great day, it's Summer Solstice today!");
	if (winter_solstice) log_char(cn,LOG_SYSTEM,0,"It's a scary day, it's Winter Solstice today!");

	if (spring_equinox) log_char(cn,LOG_SYSTEM,0,"Everything is in balance, it's Spring Equinox today!");
	if (fall_equinox) log_char(cn,LOG_SYSTEM,0,"Everything is in balance, it's Fall Equinox today!");

        if (moonday<14) {
		log_char(cn,LOG_SYSTEM,0,"Next full moon is in %d days.",14-moonday);
	} else {
		log_char(cn,LOG_SYSTEM,0,"Next new moon is in %d days.",28-moonday);
	}

	if (yday<90) {
		log_char(cn,LOG_SYSTEM,0,"Spring Equinox will be in %d days.",90-yday);
	} else if (yday<180) {
		log_char(cn,LOG_SYSTEM,0,"Summer Solstice will be in %d days.",180-yday);
	} else if (yday<270) {
		log_char(cn,LOG_SYSTEM,0,"Fall Equinox will be in %d days.",270-yday);
	} else {
		log_char(cn,LOG_SYSTEM,0,"Winter Solstice will be in %d days.",360-yday);
	}
}
