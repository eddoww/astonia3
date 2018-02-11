/*

$Id: military.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: military.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:26  sam
Added RCS tags


*/

#define MAXADVISOR	20

struct mission
{
	int type;
	int opt1,opt2;
	int pts;
	int exp;
};

struct military_ppd
{
	int current_pts;	// unused 'recommendation' pts
	int master_state;

	int current_advisor;	// re-using storage ID
	int advisor_state;
	int advisor_cost;
	int advisor_storage_nr;

	int advisor_last[MAXADVISOR];

	int military_pts;	// exp gained towards ranks
	int normal_exp;		// exp given out

	int mission_yday;	// day of the year the missions were created
	struct mission mis[5];	// easy,normal,hard,impossible,insane

	int took_mission;	// he accepted mission 1...5
	int took_yday;		// he took the mission on this day of the year
	int solved_mission;	// bool: mission was solved
	int solved_yday;	// solved the mission from yday (to avoid giving out more than one quest per day)

	int recommend;		// to remember if we mentioned a recommendation already
};

