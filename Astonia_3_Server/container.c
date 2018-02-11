/*

$Id: container.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: container.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:02  sam
Added RCS tags


*/

/*
container.c (C) 2001 D.Brockhaus

Containers are items which can contain other items, like chests or dead bodies.
This file contains functions to create and manipulate containers.

*/

#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "log.h"
#include "create.h"
#include "mem.h"
#include "consistency.h"
#include "container.h"

struct container *con;

static struct container *fcontainer;

int used_containers;

// get new container from free list
int alloc_container(int cn,int in)
{
	int ct;
	struct container *tmp;

	tmp=fcontainer;
	if (!tmp) { elog("alloc_item: MAXITEM reached!"); return 0; }

	fcontainer=tmp->next;

	ct=tmp-con;

	con[ct].cn=cn;
	con[ct].in=in;
	con[ct].owner=0;

	used_containers++;

	return ct;
}

// release (delete) container to free list
void free_container(int ct)
{
	if (!con[ct].cn && !con[ct].in) {
		elog("trying to free already free container %d",ct);
		return;
	}

	con[ct].cn=0;
	con[ct].in=0;

	con[ct].next=fcontainer;
	fcontainer=con+ct;

	used_containers--;
}

// create a container and attach it to item in
int create_item_container(int in)
{
	int ct;

	if (in<1 || in>=MAXITEM) {
		elog("create_item_container(): trying to create container for illegal item %d",in);
		return 0;
	}

	ct=alloc_container(0,in);
	if (!ct) return 0;

	bzero(con[ct].item,sizeof(con[ct].item));
	it[in].content=ct;

	return ct;
}

// remove and free the container attached to in
int destroy_item_container(int in)
{
	int n,ct,in2;

	if (in<1 || in>=MAXITEM) {
		elog("destroy_item_container(): trying to destroy container for illegal item %d",in);
		return 0;
	}

	ct=it[in].content;
	if (!ct) return 0;

	if (ct<1 || ct>=MAXCONTAINER) {
		elog("destroy_item_container(): trying to destroy illegal container %d",ct);
		return 0;
	}

	if (!con[ct].cn && !con[ct].in) {
		elog("destroy_item_container(): trying to destroy already erased container %d",ct);
		return 0;
	}

        for (n=0; n<CONTAINERSIZE; n++) {
		if ((in2=con[ct].item[n])) {
			if (in2<1 || in2>=MAXITEM){
				elog("destroy_item_container(): found illegal item %d in container %d (item %d)",in2,ct,in);
			} else {			
				free_item(in2);
			}
		}
	}
	free_container(ct);

	it[in].content=0;

	return 1;
}

// add item in to container ct at position pos.
int add_item_container(int ct,int in,int pos)
{
	int n;

	if (ct<1 || ct>=MAXCONTAINER) {
		elog("add_item_container: illegal container %d",ct);
		return 0;
	}
	if (in<1 || in>=MAXITEM) {
		elog("add_item_container: illegal item %d",in);
		return 0;
	}
	if (pos==-1) {
		for (n=0; n<CONTAINERSIZE; n++)
			if (!con[ct].item[n]) break;
		if (n==CONTAINERSIZE) return 0;
		
		pos=n;
	}
	if (pos<0 || pos>=CONTAINERSIZE) {
		elog("add_item_container: illegal pos %d",pos);
		return 0;
	}

	if (con[ct].item[pos]) return 0;

	con[ct].item[pos]=in;

	it[in].contained=ct;
	it[in].carried=0;
	it[in].x=it[in].y=0;

	return 1;
}

// remove item in from its container.
int remove_item_container(int in)
{
	int n,ct;

	if (in<1 || in>=MAXITEM) {
		elog("remove_item_container: illegal item %d",in);
		return 0;
	}

	ct=it[in].contained;

	if (ct<1 || ct>=MAXCONTAINER) {
		elog("remove_item_container: illegal container %d",ct);
		return 0;
	}	

	for (n=0; n<CONTAINERSIZE; n++)
		if (con[ct].item[n]==in) break;
	if (n==CONTAINERSIZE) return 0;

	con[ct].item[n]=0;
	it[in].contained=0;

	return 1;
}

// count the number of items in a container
int container_itemcnt(int in)
{
	int n,ct,cnt=0;

	if (in<1 || in>=MAXITEM) {
		elog("container_itemcnt: illegal item %d",in);
		return 0;
	}

	ct=it[in].content;

	if (ct<1 || ct>=MAXCONTAINER) {
		elog("container_itemcnt: illegal container %d",ct);
		return 0;
	}	

	for (n=0; n<CONTAINERSIZE; n++)
		if (con[ct].item[n]) cnt++;

        return cnt;
}

// called at startup
int init_container(void)
{
	int n;

	con=xcalloc(sizeof(struct container)*MAXCONTAINER,IM_BASE);
	if (!con) return 0;
	xlog("Allocated containers: %.2fM (%d*%d)",sizeof(struct container)*MAXCONTAINER/1024.0/1024.0,sizeof(struct container),MAXCONTAINER);
	mem_usage+=sizeof(struct container)*MAXCONTAINER;

	// add containers to free list
	for (n=1; n<MAXCONTAINER; n++) { con[n].cn=42; free_container(n); }

	used_containers=0;

	return 1;
}

