/*

$Id: path.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: path.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:31  sam
Added RCS tags


*/

int init_path(void);
int pathfinder(int fx,int fy,int tx,int ty,int mindist,int (*check_target)(int),int maxstephint);
int pathcost(void);

int pathbestdir(void);
int pathbestx(void);
int pathbesty(void);
int pathbestcost(void);
int pathnodes(void);
int pathbestdist(void);

extern int path_rect_fx;
extern int path_rect_tx;
extern int path_rect_fy;
extern int path_rect_ty;

int rect_check_target(int m);

