/*
 * $Id: notify.h,v 1.2 2008/03/24 11:23:56 devel Exp $
 *
 * $Log: notify.h,v $
 * Revision 1.2  2008/03/24 11:23:56  devel
 * arkhata
 *
 */

#define NT_CHAR		1		// there is a character
#define NT_ITEM		2               // there is an item
#define NT_GOTHIT	3		// we got hit!!
#define NT_DIDHIT	4		// we hit someone!!
#define NT_SEEHIT	5		// we've seen someone hitting someone
#define NT_DEAD		6		// someone died
#define NT_SPELL	7		// someone cast a spell
#define NT_GIVE		8		// someone gave us an item
#define NT_CREATE	9		// character has just been created

#define NT_TEXT		200		// text message

#define NT_NPC          300             // NPC to NPC communication, dat1=ID, dat2,dat3 content

// ID of NT_NPC-message
#define NTID_MERCHANT   	1		// merchants discussing among themselves
#define NTID_TERION		2		// terion, above1
#define NTID_ASTURIN		3		// asturin, above1
#define NTID_GATEKEEPER		4		// gatekeeper
#define NTID_DIDSAY		5		// someone said something...
#define NTID_TUTORIAL		6
#define NTID_PALACE_ALERT	7
#define NTID_ARENA		8	
#define NTID_DUNGEON		9
#define NTID_TWOCITY		10              // two cities: guard calling for help
#define NTID_TWOCITY_PICK	11		// two cities: lock has been picked
#define NTID_DICE		12		// nomad: dice roll	
#define	NTID_LABGNOMETORCH	13		// lab1: torch turned off
#define	NTID_LAB2_DEAMONCHECK	14		// lab2:
#define	NTID_SALTMINE_USEITEM	15		// saltmine message to use the ladder item
#define NTID_GLADIATOR		16

struct msg
{
	int type,dat1,dat2,dat3;

	struct msg *next,*prev;
};

extern int used_msgs;

int init_notify(void);
void notify_char(int cn,int type,int dat1,int dat2,int dat3);
void notify_area(int xc,int yc,int type,int dat1,int dat2,int dat3);
void purge_messages(int cn);
void remove_message(int cn,struct msg *msg);
void notify_all(int type,int dat1,int dat2,int dat3);
void notify_area_shout(int xc,int yc,int type,int dat1,int dat2,int dat3);










