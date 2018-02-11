/*
 * $Id: container.h,v 1.3 2007/07/01 13:27:03 devel Exp $
 *
 * $Log: container.h,v $
 * Revision 1.3  2007/07/01 13:27:03  devel
 * removed /allow and helpers
 *
 * Revision 1.2  2007/02/24 14:09:54  devel
 * NWO first checkin, feb 24
 *
 */

#define MAXCONTAINER	1024
#define CONTAINERSIZE	(INVENTORYSIZE)

struct container
{
	int cn,in;			// character or item which is this container (reference object)

	int owner;			// for graves: the victim may access the grave
	int owner_not_seyan;		// for graves: new seyans may not access their old grave
	int killer;			// for graves: the killer may access the grave

	int item[CONTAINERSIZE];	// up to CONTAINERSIZE item numbers (contents of container)

	struct container *next;
};

extern struct container *con;

extern int used_containers;

int alloc_container(int cn,int in);
void free_container(int ct);
int init_container(void);
int create_item_container(int in);
int destroy_item_container(int in);
int add_item_container(int ct,int in,int pos);
int remove_item_container(int in);
int container_itemcnt(int in);










