/*

$Id: lookup.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: lookup.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:19  sam
Added RCS tags


*/

int lookup_name(char *name,char *realname);
int lookup_ID(char *name,int ID);
void lookup_add_cache(unsigned int ID,char *name);
int init_lookup(void);
