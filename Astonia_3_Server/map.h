/*

$Id: map.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: map.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:24  sam
Added RCS tags


*/

int set_item_map(int in,int x,int y);
int remove_item_map(int in);
int remove_item_char(int in);
int set_char(int cn,int x,int y,int nosteptrap);
int remove_char(int cn);
int drop_item(int in,int x,int y);
int drop_char(int cn,int x,int y,int nosteptrap);
int replace_item_char(int old,int new);
void check_map(void);
int item_drop_char(int in,int cn);
int drop_item_extended(int in,int x,int y,int maxdist);
int drop_char_extended(int cn,int x,int y,int maxdist);
