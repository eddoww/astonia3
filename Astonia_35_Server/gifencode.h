struct image
{
	int width;
	int height;
	int sx,sy;
	int ex,ey;
	int dx,dy;
	unsigned char *ptr;
};

void make_gif(int width,int height,int ncol,unsigned char *red,unsigned char *green,unsigned char *blue,unsigned char *image);
struct image *make_image(int width,int height);
void make_raster(struct image *i,int xstep,int ystep,int color,float high,float low,float high2,float low2,char *title);
void set_pix(struct image *i,int x,int y,int color);
int drawtext(struct image *i,int sx, int sy, int color, int fontsize, const char *text);
