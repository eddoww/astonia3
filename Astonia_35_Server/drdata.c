/*

$Id: drdata.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: drdata.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:11  sam
Added RCS tags


*/

/*
drdata.c (C) 2001 D.Brockhaus

drdata is used to
	- temporarily remember things for NPCs
	- remember things about player characters forever
	
To use the persistent player data feature, make sure you set the PPD flag.
*/


#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "mem.h"
#include "drdata.h"

// set buf of size at "position" ID of cn. if the block exists already,
// it will be returned as is. if size is bigger than the previous biggest
// blocksize, the data block will be grown to size.
// returns buf or NULL on error
void *set_data(int cn,int ID,int size)
{
	struct data *dat,*old;
	void *tmp;
	unsigned long long prof;

	prof=prof_start(35);

	//xlog("set_data(): ID=%d, size=%d, dat=%p",ID,size,dat);

	for (dat=ch[cn].dat; dat; dat=dat->next)
		if (dat->ID==ID) break;

        if (dat) {	// already got a block for ID
		if (size>dat->size) {	// new block is bigger, re-allocate it
			tmp=xrealloc(dat->data,size,IM_DRDATA);
			if (!tmp) { elog("PANIC: realloc failed in set_data()"); return NULL; }
			mem_usage+=size-dat->size;

			bzero(tmp+dat->size,size-dat->size);

			dat->data=tmp;
			dat->size=size;
		} /*else if (size<dat->size) { safe to use this one??
			tmp=xrealloc(dat->data,size,IM_DRDATA);
			if (!tmp) { elog("PANIC: realloc failed in set_data()"); return NULL; }
			mem_usage-=size-dat->size;
			dat->data=tmp;
			dat->size=size;
		}*/
		prof_stop(35,prof);
		return dat->data;
	} else {	// no block for ID yet
                dat=xmalloc(sizeof(struct data),IM_DRHDR);
		if (!dat) { elog("PANIC: malloc failed in set_data() 1"); prof_stop(35,prof); return NULL; }

		dat->data=xmalloc(size,IM_DRDATA);
		
		if (!dat->data) { elog("PANIC: malloc failed in set_data() 2"); prof_stop(35,prof); return NULL; }

                dat->size=size;
		dat->ID=ID;

                bzero(dat->data,size);

		mem_usage+=sizeof(struct data);
		mem_usage+=size;

		// remember old first data block
		old=ch[cn].dat;
		
		// we have no previous data block, so set it to NULL
		dat->prev=NULL;

		// our next data block is the former first block (old)
		dat->next=old;

		// if there was already a first data block (old) make it remember that we're there now:
		if (old) old->prev=dat;

		// make us the first block
		ch[cn].dat=dat;

		// linked list is setup.

		prof_stop(35,prof);
		return dat->data;;
	}
}

// remove data block ID from cn
int del_data(int cn,int ID)
{
	struct data *dat,*next,*prev;

	for (dat=ch[cn].dat; dat; dat=dat->next)
		if (dat->ID==ID) break;
	if (!dat) return 0;		// we didn't find it

	// junk the data
        xfree(dat->data);

	// remember our neighbours
	next=dat->next;
	prev=dat->prev;

	// link them with each other, removing us from the list
	if (next) next->prev=prev;
	if (prev) prev->next=next;	

	// first entry needs special care...
	if (dat==ch[cn].dat) ch[cn].dat=next;

        // all done, now free the data block itself
	mem_usage-=sizeof(struct data)+dat->size;

	//dat->next=empty;
	//empty=dat;
	xfree(dat);

        return 1;
}

void *get_data(int cn,int ID,int size)
{
	struct data *dat;

	for (dat=ch[cn].dat; dat; dat=dat->next)
		if (dat->ID==ID) break;
	if (!dat) return NULL;			// we didn't find it

	if (dat->size<size) return NULL;	// user expects more memory than we have

	//xlog("get_data: ID=%d, size=%d, dat=%p",ID,size,dat);

	return dat->data;
}

void del_all_data(int cn)
{
	struct data *dat,*next;

        for (dat=ch[cn].dat; dat; dat=next) {
		
		mem_usage-=sizeof(struct data)+dat->size;

		next=dat->next;
		xfree(dat->data);
		
		//dat->next=empty;
		//empty=dat;
		xfree(dat);
	}
	
	ch[cn].dat=NULL;
}
