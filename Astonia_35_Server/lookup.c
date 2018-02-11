/*

$Id: lookup.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: lookup.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:19  sam
Added RCS tags


*/

#include <string.h>
#include <ctype.h>
#include <pthread.h>

#include "server.h"
#include "tool.h"
#include "mem.h"
#include "database.h"
#include "lookup.h"

#define MAXLOOK	1024

struct lookup
{
	char name[40];
	int ID;
	int created;
};

static struct lookup *lookup=NULL;
static int pos=0;

static pthread_mutex_t mutex;

// look up a player name (name->ID) in the database
// if realname!=NULL, it is set to the database representation
// of the name just looked up (use to get propper capitalization).
// returns 0 for unknown, -1 for no player with that name and >0 for ID
int lookup_name(char *name,char *realname)
{
	int n;
	unsigned int ID;
	char *ptr;

	// spaces and punctuation in text keys confuse mysql. sucks.
	for (ptr=name; *ptr; ptr++) {
		if (!isalpha(*ptr)) return -1;
	}

	if (strlen(name)>38) return -1;	
	if (strlen(name)<2) return -1;	

	if (multi) pthread_mutex_lock(&mutex);

	for (n=0; n<MAXLOOK; n++) {
		if (lookup[n].created+TICKS*60*60<ticker) continue;	// dont use entries older than one hour

		if (!strcasecmp(lookup[n].name,name)) break;
	}
	if (n==MAXLOOK) {
		if (multi) pthread_mutex_unlock(&mutex);
		query_name(name);
		return 0;
	}
	
	if (realname) strcpy(realname,lookup[n].name);

	ID=lookup[n].ID;

	if (multi) pthread_mutex_unlock(&mutex);

	return ID;
}

// look up a player ID (ID->name) in the database
// will set name to "*resolving*" and return value to 0 for unknown,
// name to player name and return value to 1 for known
// or will return -1 for no player with that ID
int lookup_ID(char *name,int ID)
{
	int n,res;

	if (multi) pthread_mutex_lock(&mutex);

	for (n=0; n<MAXLOOK; n++) {
		if (lookup[n].created+TICKS*60*60<ticker) continue;	// dont use entries older than one hour
		if (lookup[n].ID==ID) break;
	}
	if (n==MAXLOOK) {
		if (multi) pthread_mutex_unlock(&mutex);
		query_ID(ID);
		if (name) strcpy(name,"*resolving*");
                return 0;
	}
	if (lookup[n].name[0]) {
		if (isdigit(lookup[n].name[0])) {
			if (name) strcpy(name,"**deleted**");
			res=-1;
		} else {
			if (name) strcpy(name,lookup[n].name);
			res=1;
		}
	} else {
		if (name) strcpy(name,"*unknown*");
		res=-1;
	}
	
        if (multi) pthread_mutex_unlock(&mutex);

	return res;
}

void lookup_add_cache(unsigned int ID,char *name)
{
	int n;

        if (multi) pthread_mutex_lock(&mutex);
	for (n=0; n<MAXLOOK; n++) {
		if (ID!=-1 && lookup[n].ID==ID) break;
		if (name && lookup[n].name && !strcasecmp(lookup[n].name,name)) break;		
	}
	if (n==MAXLOOK) n=pos=(pos+1)%MAXLOOK;

        lookup[n].ID=ID;
	lookup[n].created=ticker;

        if (name) strcpy(lookup[n].name,name);
	else lookup[n].name[0]=0;

	if (multi) pthread_mutex_unlock(&mutex);
}

int init_lookup(void)
{
	lookup=xcalloc(sizeof(struct lookup)*MAXLOOK,IM_BASE);
	if (!lookup) return 0;

	if (multi) pthread_mutex_init(&mutex,NULL);

        return 1;
}
