/*

$Id: path.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: path.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:31  sam
Added RCS tags


*/

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "server.h"
#include "direction.h"
#include "log.h"
#include "mem.h"
#include "path.h"

#define MAXNODE 4096

struct node
{
        int x,y,dir;            // coordinates of current node and dir we originally came from
        int tcost,cost;         // total (guessed) cost, cost of steps so far
        int visited;

	struct node *prev,*next;
};

struct node **nmap=NULL;
struct node *ngo;

struct node *nodes=NULL;
static int maxnode;

static int tx1,ty1,maxstep;
static int failed;

static int xcost(int fx,int fy,int tx,int ty)
{
        int dx,dy;

        dx=abs(fx-tx);
        dy=abs(fy-ty);

        if (dx>dy) return ((dx<<1)+dy);
        else return ((dy<<1)+dx);
}

static int cost(int fx,int fy)
{
        return xcost(fx,fy,tx1,ty1);
}

static int (*dr_check_target)(int)=NULL;

static int normal_check_target(int m)
{
	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
	//if (map[m].ch && (ch[map[m].ch].flags&(CF_PLAYER|CF_PLAYERLIKE)) && ch[map[m].ch].action==AC_IDLE) return 1;
        if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

int path_rect_fx;
int path_rect_tx;
int path_rect_fy;
int path_rect_ty;

int rect_check_target(int m)
{
	int x,y;

	x=m%MAXMAP;
	y=m/MAXMAP;

	if (x<path_rect_fx || x>path_rect_tx || y<path_rect_fy || y>path_rect_ty) return 0;

	if (map[m].flags&MF_MOVEBLOCK) return 0;
	if (map[m].flags&MF_DOOR) return 1;
        if (map[m].flags&MF_TMOVEBLOCK) return 0;

	return 1;
}

int add_node(int x,int y,int dir,int ccost)
{
        int m,tcost,gcost;
        struct node *node,*tmp,*prev,*next;

        if (x<1 || x>=MAXMAP || y<1 || y>=MAXMAP) return 0;

        m=x+y*MAXMAP;

        gcost=cost(x,y);		// calculate guessed cost

        tcost=ccost+gcost;		// total cost is current cost (ccost) plus guessed cost

        if ((tmp=nmap[m])!=NULL) {			// is there already a node on this map position?
                if (tmp->tcost<=tcost) return 0;        // other node is better or equal, no need to try this one

                if (!tmp->visited) {			// remove the old (worse) node from the list if we haven't visited it yet
                        prev=tmp->prev;			// if we visited it already, it won't be on the list anyway
                        next=tmp->next;

                        if (prev) prev->next=next;
                        else ngo=next;

                        if (next) next->prev=prev;
                }
                node=tmp;
        } else {					// no node on this map position, create a new one
                if (maxnode>=maxstep) { failed=1; return 0; }
                node=&nodes[maxnode++];
                node->x=x;
                node->y=y;
        }

        node->cost=ccost;
        node->tcost=tcost;
        node->visited=0;
        node->dir=dir;
        nmap[m]=node;

	// find correct position in sorted list
        for (tmp=ngo,prev=NULL; tmp; tmp=tmp->next) {
                if (tmp->tcost>=node->tcost) break;
                prev=tmp;
        }

        // previous node -> next
        if (prev) prev->next=node;
        else ngo=node;

        // current node -> prev
        node->prev=prev;

        // next node -> prev
        if (tmp) tmp->prev=node;

        // current node -> next
        node->next=tmp;

        return 1;
}

//#define dr_check_target(m)	(!(map[m].flags&(MF_MOVEBLOCK|MF_TMOVEBLOCK)))
//#define dr_check_target(m)	(!((*(unsigned char*)(&map[m].flags))&((unsigned char)(MF_MOVEBLOCK|MF_TMOVEBLOCK))))

static void add_suc(struct node *node)
{	
        if (!node->dir) {
                if (dr_check_target(node->x+node->y*MAXMAP+1)) add_node(node->x+1,node->y,DX_RIGHT,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP-1)) add_node(node->x-1,node->y,DX_LEFT,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP+MAXMAP)) add_node(node->x,node->y+1,DX_DOWN,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP-MAXMAP)) add_node(node->x,node->y-1,DX_UP,node->cost+2);

                if (dr_check_target(node->x+node->y*MAXMAP+1+MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP+1) &&
                    dr_check_target(node->x+node->y*MAXMAP+MAXMAP))
                        add_node(node->x+1,node->y+1,DX_RIGHTDOWN,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP+1-MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP+1) &&
                    dr_check_target(node->x+node->y*MAXMAP-MAXMAP))
                        add_node(node->x+1,node->y-1,DX_RIGHTUP,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP-1+MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP-1) &&
                    dr_check_target(node->x+node->y*MAXMAP+MAXMAP))
                        add_node(node->x-1,node->y+1,DX_LEFTDOWN,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP-1-MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP-1) &&
                    dr_check_target(node->x+node->y*MAXMAP-MAXMAP))
                        add_node(node->x-1,node->y-1,DX_LEFTUP,node->cost+3);
        } else {
                if (dr_check_target(node->x+node->y*MAXMAP+1)) add_node(node->x+1,node->y,node->dir,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP-1)) add_node(node->x-1,node->y,node->dir,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP+MAXMAP)) add_node(node->x,node->y+1,node->dir,node->cost+2);
                if (dr_check_target(node->x+node->y*MAXMAP-MAXMAP)) add_node(node->x,node->y-1,node->dir,node->cost+2);

                if (dr_check_target(node->x+node->y*MAXMAP+1+MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP+1) &&
                    dr_check_target(node->x+node->y*MAXMAP+MAXMAP))
                        add_node(node->x+1,node->y+1,node->dir,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP+1-MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP+1) &&
                    dr_check_target(node->x+node->y*MAXMAP-MAXMAP))
                        add_node(node->x+1,node->y-1,node->dir,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP-1+MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP-1) &&
                    dr_check_target(node->x+node->y*MAXMAP+MAXMAP))
                        add_node(node->x-1,node->y+1,node->dir,node->cost+3);

                if (dr_check_target(node->x+node->y*MAXMAP-1-MAXMAP) &&
                    dr_check_target(node->x+node->y*MAXMAP-1) &&
                    dr_check_target(node->x+node->y*MAXMAP-MAXMAP))
                        add_node(node->x-1,node->y-1,node->dir,node->cost+3);
        }
}

static struct node *goal,*best;

int astar(int fx,int fy,int mindist)
{
        struct node *node;
	int dist,bestdist=999,step=0;

        node=&nodes[maxnode++];
        node->x=fx;
        node->y=fy;
        node->dir=0;
        node->cost=0;
        node->tcost=node->cost+cost(fx,fy);
        ngo=node;
        nmap[fx+fy*MAXMAP]=node;

        while (ngo && !failed) {

                // get first node in go list
                node=ngo;

		// check if the node is our target
		dist=abs(node->x-tx1)+abs(node->y-ty1);
		
		if (dist<bestdist) { best=node; bestdist=dist; }
		if (dist==mindist) { goal=node; return node->dir; }

                // remove node from go list
                ngo=ngo->next;
                if (ngo) ngo->prev=NULL;

		// mark node as visited
                node->visited=1;

		add_suc(node);

		step++;
        }

        return -1;
}

// find path for character cn to x,y. will return if
// a path to a tile which is no further than mindist
// away is found
int pathfinder(int fx,int fy,int tx,int ty,int mindist,int (*check_target)(int),int maxstephint)
{
        int tmp,n;
	unsigned long long prof;

        if (fx<1 || fx>=MAXMAP-1) return -1;
        if (fy<1 || fy>=MAXMAP-1) return -1;
        if (tx<1 || tx>=MAXMAP-1) return -1;
        if (ty<1 || ty>=MAXMAP-1) return -1;

	// are we there already?
	if (abs(fx-tx)+abs(fy-ty)==mindist) return -1;
	//if (fx==tx && fy==ty && mindist==0) return -1;

        tx1=tx;
        ty1=ty;
	if (check_target) dr_check_target=check_target;
	else dr_check_target=normal_check_target;

	// shortcut: if the target isn't reachable, there's not much sense in trying...
        if (mindist==0 && !dr_check_target(tx1+ty1*MAXMAP)) return -1;

	prof=prof_start(17);

        maxstep=min(MAXNODE,(abs(fx-tx)+abs(fy-ty))*20+500);
        if (maxstephint && maxstephint<maxstep) maxstep=maxstephint;

        maxnode=0;
	failed=0;
	goal=NULL;
	best=NULL;

	tmp=astar(fx,fy,mindist);

        for (n=0; n<maxnode; n++) {
                nmap[nodes[n].x+nodes[n].y*MAXMAP]=NULL;
        }

	prof_stop(17,prof);

	return tmp;
}

int pathbestdir(void)
{
	if (best) return best->dir;
	else return -1;
}

int pathbestx(void)
{
	if (best) return best->x;
	else return 0;
}

int pathbesty(void)
{
	if (best) return best->y;
	else return 0;
}

int pathbestdist(void)
{
	if (best) return abs(best->x-tx1)+abs(best->y-ty1);
	else return 0;	
}

int pathbestcost(void)
{
	if (best) return best->cost;
	else return 0;
}

int pathcost(void)
{
	if (goal) return goal->cost;
	else return 0;
}

int pathnodes(void)
{
	return maxnode;
}

int init_path(void)
{
        nmap=xcalloc(MAXMAP*MAXMAP*sizeof(struct node *),IM_BASE);
        if (!nmap) return 0;
        xlog("Allocated nmap: %.2fM (%d*%d)",sizeof(struct node *)*MAXMAP*MAXMAP/1024.0/1024.0,sizeof(struct node *),MAXMAP*MAXMAP);
	mem_usage+=sizeof(struct node *)*MAXMAP*MAXMAP;

        nodes=xcalloc(sizeof(struct node)*MAXNODE,IM_BASE);
        if (!nodes) return 0;
	xlog("Allocated nodes: %.2fM (%d*%d)",sizeof(struct node)*MAXNODE/1024.0/1024.0,sizeof(struct node),MAXNODE);
	mem_usage+=sizeof(struct node)*MAXNODE;

        return 1;
}

