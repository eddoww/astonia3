/*

$Id: lab.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: lab.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:16  sam
Added RCS tags


*/

struct lab_ppd
{
	unsigned long long solved_bits;		// max 64 labs
	unsigned long dummy[8];			// reserved space for logic extensions

	// individual lab data follows

        // lab1/20: light and dark
        // forgot to set a dummy, but who cares.

        // lab2/30: undead
        unsigned char timesgotcryptgold;        // the name says it all ;)
        unsigned char timesgotyardgold;         // the name says it all ;)
        unsigned char herald_talkstep;          // conversation is at this point
        unsigned char graveversion;             // just if we change something, we'll increase the inside version number and rearrange the graves
        unsigned char graveindex[4];            // index into the described graves. slot 0=
        unsigned long lab2_dummy[8];            // reserved for later extensions

        // lab3/25: underwater
        unsigned char password1[8];             // first part of the password
        unsigned char password2[8];             // second part of the password
        unsigned char prisoner_talkstep;        // conversation is at this point
        unsigned char guard_talkstep;           // conversation is at this point (used by guard and door)
        unsigned char align_dummy1[2];
        unsigned long lab3_dummy[8];            // reserved for later extensions

        // lab4/10: gnalbgaves
        unsigned long lab4_dummy[8];            // reserved for later extensions

        // lab5/15: hard life (no regenerate)
        unsigned long lab5_dummy[8];            // reserved for later extensions
};

int count_solved_labs(int cn);
int teleport_next_lab(int cn,int do_teleport);
int set_solved_lab(int cn,int lab_level);
int create_lab_exit(int cn,int level);

