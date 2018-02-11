/*

$Id: light.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: light.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:18  sam
Added RCS tags


*/

#define LIGHTDIST	10

void add_effect_light(int x,int y,int light);
void remove_effect_light(int x,int y,int light);
void add_char_light(int cn);
void remove_char_light(int cn);
void add_item_light(int in);
void remove_item_light(int in);
int compute_dlight(int xc,int yc);
void compute_shadow(int xc,int yc);
int reset_dlight(int xc,int yc);
int remove_lights(int xc,int yc);
int add_lights(int xc,int yc);
void compute_groundlight(int x,int y);
