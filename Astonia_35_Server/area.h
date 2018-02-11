/*

$Id: area.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: area.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:11:43  sam
Added RCS tags


*/

int get_section(int x,int y);
void show_section(int x,int y,int cn);
void register_npc_to_section(int cn);
int register_kill_in_section(int cn,int co);
void walk_section_msg(int cn);
void area_sound(int cn);
char *get_section_name(int x,int y);
