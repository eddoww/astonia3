#define MAXCLUB		16384
#define CLUBOFFSET	1024

struct club
{
	char name[80];
	int serial;
	int paid;
	int money;
};

extern struct club club[MAXCLUB];

int get_char_club(int cn);
int show_club_info(int cn,int co,char *buf);
void tick_club(void);
int rename_club(int nr,char *name);
void kill_club(int cnr);
void showclub(int cn);
int create_club(char *name);
