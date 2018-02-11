/*

$Id: mem.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: mem.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:25  sam
Added RCS tags


*/

#define IM_BASE		1
#define IM_TEMP		2
#define IM_CHARARGS	3
#define IM_TALK		4
#define IM_DRDATA	5
#define IM_DRHDR	6
#define IM_QUERY	7
#define IM_DATABASE	8
#define IM_NOTIFY	9
#define IM_TIMER	10
#define IM_PLAYER	11
#define IM_STORE	12
#define IM_STORAGE	13
#define IM_ZLIB		14

void *xmalloc(int size, int ID);
void *xcalloc(int size,int ID);
void *xrealloc(void *ptr,int size,int ID);
void xfree(void *ptr);
void *xstrdup(char *ptr,int ID);
void list_mem(void);
int init_mem(void);
int init_smalloc(void);
