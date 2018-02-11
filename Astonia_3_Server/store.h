/*

$Id: store.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: store.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:53  sam
Added RCS tags


*/

#define MAXSTORE	64
#define STORESIZE	(INVENTORYSIZE)

struct ware
{
	int cnt;
	int always;
	struct item item;
};

struct store
{
        int gold;
	int pricemulti;

	struct ware ware[STORESIZE];
};

extern struct store **store;

int init_store(void);
int salesprice(int cn,int co,int nr);
int buyprice(int cn,int in);
int sell(int cn,int co,int nr);
int buy(int cn,int co);
int create_store(int cn,int ignore,int pricemulti);
void player_store(int cn,int nr,int flag,int fast);
int add_special_store(int cn);
