struct skill
{
	char name[80];
	int base1,base2,base3;
        int cost;		// 0=not raisable, 1=skill, 2=attribute, 3=power
	int start;		// start value, pts up to this value are free
};

extern char *skilldesc[];

extern struct skill skill[];

int raise_cost(int v,int n);
