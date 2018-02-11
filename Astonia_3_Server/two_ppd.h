
struct twocity_ppd
{
	int legal_status;
	int legal_fine;

	int citizen_status;

	int current_guard;
	int current_guard_time;
	int last_attack;

	int barkeeper_state;
	int guard_intro;
	
	int thief_state;
	int thief_last_seen;
	int thief_killed[6];

	int sanwyn_state;
	int sanwyn_bits;

	int thief_bits;

	int goodtile[5];

	int solved_library;
	int library_state;

	int barkeeper_last;

	int skelly_state;
	
	int alchemist_state;
};

