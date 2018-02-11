/*
 * $Id: military.h,v 1.2 2007/02/24 14:09:54 devel Exp $
 *
 * $Log: military.h,v $
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

struct mission
{
	int type;
	int opt1,opt2;
	int pts;
	int exp;
};

struct military_ppd
{
        int master_state;

        int military_pts;	// exp gained towards ranks
	int normal_exp;		// exp given out

	int mission_yday;	// day of the year the missions were created
	struct mission mis[7];	// very easy,easy,normal,hard,very hard,impossible,insane

	int took_mission;	// he accepted mission 1...7
	int took_yday;		// he took the mission on this day of the year
	int solved_mission;	// bool: mission was solved
	int solved_yday;	// solved the mission from yday (to avoid giving out more than one quest per day)
};














