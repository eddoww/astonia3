/*

$Id: create.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: create.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:03  sam
Added RCS tags


*/

extern int used_chars;
extern int used_items;


int init_create(void);
int create_item(char *name);
int create_char_nr(int tmp,int tmpa);
int create_char(char *name,int tmpa);
void update_char(int cn);
int alloc_char(void);
void free_char(int cn);
int getfirst_char(void);
int getnext_char(int cn);
void copy_char(struct character *dst,struct character *src);
int alloc_item(void);
void free_item(int in);
int destroy_items(int cn);
int lookup_char(char *name);
int lookup_item(char *name);
int create_item_nr(int tmp);
