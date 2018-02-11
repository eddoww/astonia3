#define MAXNOMAD	10

#define TM_TRIBE1	1
#define TM_TRIBE2	2
#define TM_TRIBE3	4


struct nomad_ppd
{
	int nomad_state[MAXNOMAD];
	int nomad_win[MAXNOMAD];
	int open_roll1,open_roll2,open_roll3,open_bet;
	int tribe_member;
};

