struct single_mission
{
	int type;
	int mdidx;
	int difficulty;	
};

struct mission_ppd
{
	int missiongive_state;		// mission giver status (talk switch/case)
	int lastseenmissiongiver;
	
	int active;
	int solved;
	int points;
	int mcnt;	// number of missions accepted

	int dif_kill;
	
	struct single_mission sm[3];

	// for active mission only:
	int md_idx;
        int kill_easy[2];
	int kill_normal[2];
	int kill_hard[2];
	int kill_boss[2];
	int find_item[2];

	// custom potion creation
	int statowed,statcnt,stat[3];
};

