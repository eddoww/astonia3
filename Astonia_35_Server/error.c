/*

$Id: error.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: error.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:12  sam
Added RCS tags


*/

#include "error.h"

int error;

static char *errname[]={
	"Everything's fine, dear",
	"Illegal coordinates",
	"Target is blocked",
	"Illegal character number",
	"Illegal direction",
	"Server is too confused to honor your request",
	"No item present",
	"Item not takeable",
	"There is already an item in citem",
	"No citem present",
	"There is already an item",
	"Illegal inventory position",
	"Requirements not fulfilled",			//12
	"No character present",
	"Illegal attack (the victim is protected)",
	"Illegal item number",
	"Character is unconcious or dead",		//16
	"Not enough mana",
	"Target points to self",
	"Illegal hurt type",				//19
	"Target is not visible",
	"Target is unconcious",				//21
	"Unknown spell",
	"Item not usable",
	"Not a dead body",				//24
	"Unknown skill",
	"Illegal container position",			//26
	"Not a container",
	"Spell already working",			//28
	"Illegal store number",
	"Illegal store position",			//30
	"Item is sold out",
	"Not enough gold",				//32
	"Quest Item",
	"Access denied",				//34
	"Not idle",
	"Not a player character",			//36
	"Would have no effect",
	"Already there",				//38
	"Error number out of bounds"
};

char *get_error_string(int err)
{
	if (err<0 || err>=sizeof(errname)/sizeof(char*)) err=sizeof(errname)/sizeof(char*)-1;

	return errname[err];
}
