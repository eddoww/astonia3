/*

$Id: mem.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: mem.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:24  sam
Added RCS tags


*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <malloc.h>
#include <pthread.h>

#include "log.h"
#include "btrace.h"
#include "mem.h"

#undef DEBUG

static pthread_mutex_t alloc_mutex;

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))

void *srealloc(void *ptr,int nbytes,int nr);
void *smalloc(int nbytes,int nr);
void sfree(void *ap);
int get_region_size(int nr);
int get_region_freesize(int nr);
int get_region_free(int nr);
int _list_free(int nr);


void db_free(void *ptr);
void *db_realloc(void *ptr,int size);
void *db_alloc(int size);

struct mem_block
{
	int ID;
	int size;
	struct mem_block *prev,*next;

#ifdef DEBUG
	char magic[32];
#endif

	char content[0];
};

struct mem_block *mb=NULL;

static int msize=0;

static char *memname[]={
	"none",
	"IM_BASE",
	"IM_TEMP",
	"IM_CHARARGS",
	"IM_TALK",
	"IM_DRDATA",
	"IM_DRHDR",
	"IM_QUERY",
	"IM_DATABASE",
	"IM_NOTIFY",
	"IM_TIMER",
	"IM_PLAYER",
	"IM_STORE",
	"IM_STORAGE",
        "IM_ZLIB",
	"IM_MYSQL",
	"unknown1",
	"unknown2",
	"unknown3",
	"unknown4"
};


static void rem_block(struct mem_block *mem)
{
	struct mem_block *prev,*next;

	//xlog("remove block at %p, size %d",mem,mem->size);

	pthread_mutex_lock(&alloc_mutex);

	msize-=mem->size;

	prev=mem->prev;
	next=mem->next;
	
	if (prev) prev->next=next;
	else mb=next;
	
	if (next) next->prev=prev;

	pthread_mutex_unlock(&alloc_mutex);
}

static void add_block(struct mem_block *mem)
{
	msize+=mem->size;
	if (msize>100*1024*1024) kill(getpid(),11);

	pthread_mutex_lock(&alloc_mutex);

        mem->prev=NULL;
	mem->next=mb;
	if (mb) mb->prev=mem;
	mb=mem;

	pthread_mutex_unlock(&alloc_mutex);

	//xlog("add block at %p, size %d",mem,mem->size);
}

void *xmalloc(int size, int ID)
{
	struct mem_block *mem;

#ifdef DEBUG
	mem=smalloc(size+sizeof(struct mem_block)+32,ID);
#else
	mem=smalloc(size+sizeof(struct mem_block),ID);
#endif
	if (!mem) return NULL;

        mem->ID=ID;
	mem->size=size;

#ifdef DEBUG
	memcpy(mem->magic,       "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",32);
	memcpy(mem->content+size,"abcdefghijklmnopqrstuvwxyz678901",32);
#endif
	
	add_block(mem);

	return mem->content;
}

void *xcalloc(int size,int ID)
{
	void *ptr;

	ptr=xmalloc(size,ID);
	if (!ptr) return NULL;
	
	bzero(ptr,size);

	return ptr;
}

void *xrealloc(void *ptr,int size,int ID)
{
	struct mem_block *mem;

	if (ptr==NULL) return xcalloc(size,ID);

	mem=(void*)((char*)(ptr)-sizeof(struct mem_block));

	rem_block(mem);

#ifdef DEBUG
	mem=srealloc(mem,size+sizeof(struct mem_block)+32,ID);
#else
	mem=srealloc(mem,size+sizeof(struct mem_block),ID);
#endif
	if (!mem) return NULL;

	mem->size=size;
	mem->ID=ID;

#ifdef DEBUG
	memcpy(mem->magic,       "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",32);
	memcpy(mem->content+size,"abcdefghijklmnopqrstuvwxyz678901",32);
#endif

	add_block(mem);

	return mem->content;
}

void xfree(void *ptr)
{
	struct mem_block *mem;

	mem=(void*)((char*)(ptr)-sizeof(struct mem_block));

#ifdef DEBUG
	if (memcmp(mem->magic,"ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",32)) {
		elog("memory fail: top, type=%d, size=%d",mem->ID,mem->size);
		elog("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			     mem->content[0],
			     mem->content[1],
			     mem->content[2],
			     mem->content[3],
			     mem->content[4],
			     mem->content[5],
			     mem->content[6],
			     mem->content[7],
			     mem->content[8],
			     mem->content[9],
			     mem->content[10],
			     mem->content[11],
			     mem->content[12],
			     mem->content[13],
			     mem->content[14],
			     mem->content[15]);
	}
	if (memcmp(mem->content+mem->size,"abcdefghijklmnopqrstuvwxyz678901",32)) {
		elog("memory fail: bottom, type=%d, size=%d",mem->ID,mem->size);
		elog("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			     mem->content[0],
			     mem->content[1],
			     mem->content[2],
			     mem->content[3],
			     mem->content[4],
			     mem->content[5],
			     mem->content[6],
			     mem->content[7],
			     mem->content[8],
			     mem->content[9],
			     mem->content[10],
			     mem->content[11],
			     mem->content[12],
			     mem->content[13],
			     mem->content[14],
			     mem->content[15]);
	}
	bzero(mem->magic,32);
	bzero(mem->content+mem->size,32);
#endif

        rem_block(mem);

	sfree(mem);
}

void *xstrdup(char *src,int ID)
{
	int size;
	char *dst;

	size=strlen(src)+1;
	
	dst=xmalloc(size,ID);
	if (!dst) return NULL;
	
	memcpy(dst,src,size);

	return dst;
}

void list_mem(void)
{
	struct mem_block *mem;
	int cnt[100],size[100],n,tcnt=0,tsize=0,rsize=0;
	extern int mmem_usage;
	extern int get_region_maxsize(int nr);

	bzero(cnt,sizeof(cnt));
	bzero(size,sizeof(size));
	
#ifdef DEBUG
	xlog("mem-DEBUG version");
#endif		

	pthread_mutex_lock(&alloc_mutex);
        for (mem=mb; mem; mem=mem->next) {
		//xlog("%p: size=%d, ID=%s",mem,mem->size,memname[mem->ID]);
		if (mem->ID>99 || mem->ID<1) {
			elog("list_mem found illegal mem ID: %d at %p, giving up.",mem->ID,mem);
			pthread_mutex_unlock(&alloc_mutex);
			return;
		}
#ifdef DEBUG
		if (memcmp(mem->magic,"ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",32)) {
			elog("memory fail: top, type=%d, size=%d, p=%p",mem->ID,mem->size,mem);
			elog("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			     mem->content[0],
			     mem->content[1],
			     mem->content[2],
			     mem->content[3],
			     mem->content[4],
			     mem->content[5],
			     mem->content[6],
			     mem->content[7],
			     mem->content[8],
			     mem->content[9],
			     mem->content[10],
			     mem->content[11],
			     mem->content[12],
			     mem->content[13],
			     mem->content[14],
			     mem->content[15]);
		}
		if (memcmp(mem->content+mem->size,"abcdefghijklmnopqrstuvwxyz678901",32)) {
			elog("memory fail: bottom, type=%d, size=%d, p=%p",mem->ID,mem->size,mem);
			elog("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			     mem->content[0],
			     mem->content[1],
			     mem->content[2],
			     mem->content[3],
			     mem->content[4],
			     mem->content[5],
			     mem->content[6],
			     mem->content[7],
			     mem->content[8],
			     mem->content[9],
			     mem->content[10],
			     mem->content[11],
			     mem->content[12],
			     mem->content[13],
			     mem->content[14],
			     mem->content[15]);
		}
#endif
		cnt[mem->ID]++;
		size[mem->ID]+=mem->size;
		tcnt++;
		tsize+=mem->size;
	}

	for (n=0; n<100; n++) {		
                if (get_region_size(n)) {			
			xlog("%-10.10s: %4d blks, %8.2fK (size %7.3fM/%3.0fM %5.1f%%, free: %8.2fK, %4d blks)",
			     memname[n],
			     cnt[n],size[n]/(1024.0),
			     get_region_size(n)/1024.0/1024.0,
			     get_region_maxsize(n)/1024.0/1024.0,
			     100.0/get_region_maxsize(n)*get_region_size(n),
			     get_region_freesize(n)/1024.0,
			     get_region_free(n));
			rsize+=get_region_size(n);
			//_list_free(n);
		}
	}
	xlog("total: %d blocks, total size: %.2fM (%.2fM, r=%.2fM/%.2fM)",tcnt,tsize/(1024.0*1024.0),msize/(1024.0*1024.0),rsize/1024.0/1024.0,mmem_usage/1024.0/1024.0);
	pthread_mutex_unlock(&alloc_mutex);
}

int init_mem(void)
{
	if (pthread_mutex_init(&alloc_mutex,NULL)) return 0;

	return 1;
}


//-------------------------------------------

#define MAXREGION	100
struct header
{
	struct header *ptr;
	int size;
};

struct region
{
        int usedsize,havesize;
	int usedblocks,freeblocks,freesize;

	void *top;
	struct header base;
	struct header *allocp;	
};

struct region region[MAXREGION];

int mmem_usage=0;

struct reg_addr
{
	unsigned char *start;
	unsigned int size;
};

struct reg_addr reg_addr[32];	

int init_smalloc(void)
{
	int n,addr;
	static int sizes[32]={
		0,		// 0, not used
		48*1024*1024,	// IM_BASE, 	48M	1
		12*1024*1024,	// IM_TEMP,	12M	2
		12*1024*1024,	// IM_CHARARG,	12M	3
		12*1024*1024,	// IM_TALK,	12M	4
		96*1024*1024,	// IM_DRDATA,	96M	5
		12*1024*1024,	// IM_DRHDR,	12M	6
		12*1024*1024,	// IM_QUERY	12M	7
		12*1024*1024,	// IM_DATABASE	12M	8
		16*1024*1024,	// IM_NOTIFY	16M	9
		12*1024*1024,	// IM_TIMER	12M	10
		64*1024*1024,	// IM_PLAYER	64M	11
		12*1024*1024,	// IM_STORE	12M	12
		12*1024*1024,	// IM_STORAGE	12M	13
		128*1024*1024,	// IM_ZLIB     128M	14
		0*1024*1024,	//			15
		0*1024*1024,	//			16
		0*1024*1024,	//			17
		0*1024*1024,	//			18
		0*1024*1024,	//			19
		0*1024*1024,	//			20
		0*1024*1024,	//			21
		0*1024*1024,	//			22
		0*1024*1024,	//			23
		0*1024*1024,	//			24
		0*1024*1024,	//			25
		0*1024*1024,	//			26
		0*1024*1024,	//			27
		0*1024*1024,	//			28
		0*1024*1024,	//			29
		0*1024*1024,	//			30
		0*1024*1024	//			31
	};

	for (n=0,addr=0x50000000; n<32; n++) {
		reg_addr[n].start=(void*)addr;
		reg_addr[n].size=sizes[n];
		addr+=sizes[n];
		/*printf("%s: start=%p, size=%.2fM, end=%p\n",
		     n<15 ? memname[n] : "unused",
			reg_addr[n].start,
			reg_addr[n].size/(1024.0*1024),
			reg_addr[n].start+reg_addr[n].size);*/
	}
        return 1;
}

int smalloc_addr2nr(unsigned char *ap)
{
	int n;

	for (n=0; n<32; n++) {
		if (reg_addr[n].start<ap && reg_addr[n].start+reg_addr[n].size>ap) return n;
	}
	elog("could not find region for %p",ap);
	exit(1);
}

// must be called with mutex locked
void *size_region(int nr,int diff)
{
	int change;
	unsigned char *start;

	if (nr<0 || nr>=32) return NULL;

        region[nr].usedsize+=diff;
        start=reg_addr[nr].start;	//(void*)(0x50000000+(nr*24*1024*1024)); !!!!!!!!!!

	change=region[nr].usedsize-region[nr].havesize;
	if (change>=0) change=((change+4095)/4096)*4096;
	else change=(change/4096)*4096;

	if (region[nr].havesize+change>=reg_addr[nr].size) {
		elog("trying to allocate too much memory in region %d. terminating.",nr);
		btrace("memory");
		kill(getpid(),11);
	}

        if (change>0 && mmap(start+region[nr].havesize,change,PROT_READ|PROT_WRITE,MAP_ANON|MAP_PRIVATE,0,region[nr].havesize)==(void*)(-1)) return 0;

	region[nr].havesize+=change;
	mmem_usage+=change;
	
        if (change<0 && munmap(start+region[nr].havesize,-change)) return NULL;

	region[nr].top=start+region[nr].usedsize;

	return start+region[nr].usedsize-diff;
}

int get_region_size(int nr)
{
	if (nr<0 || nr>=32) return 0;
	
	return region[nr].havesize;
}

int get_region_free(int nr)
{
	if (nr<0 || nr>=32) return 0;
	
	return region[nr].freeblocks;
}

int get_region_freesize(int nr)
{
	if (nr<0 || nr>=32) return 0;
	
	return region[nr].freesize;
}

int get_region_maxsize(int nr)
{
	if (nr<0 || nr>=32) return 0;
	
	return reg_addr[nr].size;
}

/*int _list_free(int nr)
{
	struct header *p;
	
        for (p=region[nr].base.ptr; ; ) {
		xlog("block at %p, size %d",p,p->size*sizeof(struct header));
		p=p->ptr;
		if (p==region[nr].base.ptr) break;		
	}	
	
	return 0;
}*/

int scompact(int nr)
{	
	struct header *p,*q;
	int size;

        if ((q=region[nr].allocp)==NULL) return 0;

	for (p=q->ptr; ; q=p,p=p->ptr) {
		if (p+p->size==region[nr].top) {
			
			size=p->size;

			region[nr].allocp=q->ptr=p->ptr;
			region[nr].freeblocks--;
			region[nr].freesize-=size*sizeof(struct header);
			size_region(nr,-size*sizeof(struct header));

                        return size;
		}
		if (p==region[nr].allocp) return 0;
	}	
}

void sfree_nolock(void *ap,int dorelease)
{
	struct header *p,*q,*start=NULL;
	int nr;

        nr=smalloc_addr2nr(ap);	//(((int)ap)-0x50000000)/(24*1024*1024); !!!!!!!!!!!!!!!

	//xlog("free for nr %d address %p",nr,ap);

	region[nr].freeblocks++;
	region[nr].usedblocks--;

	p=(struct header*)ap-1;

	region[nr].freesize+=p->size*sizeof(struct header);

	for (q=region[nr].allocp; !(p>q && p<q->ptr); q=q->ptr) {	
		if (q>=q->ptr && (p>q || p<q->ptr)) break;
		start=q;
	}

        if (p+p->size==q->ptr) {
		p->size+=q->ptr->size;
		p->ptr=q->ptr->ptr;
		region[nr].freeblocks--;
	} else {
		p->ptr=q->ptr;
	}
	if (q+q->size==p) {
		q->size+=p->size;
		q->ptr=p->ptr;
		region[nr].freeblocks--;
	} else {
		q->ptr=p;
	}
	region[nr].allocp=q;

	if (dorelease) scompact(nr);
}

void sfree(void *ap)
{
	pthread_mutex_lock(&alloc_mutex);

	sfree_nolock(ap,1);

	pthread_mutex_unlock(&alloc_mutex);
}

// must be called with mutex locked
struct header *morecore(int nu,int nr)
{
	char *cp;
	struct header *up;

        if (!(cp=size_region(nr,nu*sizeof(struct header)))) return NULL;

	//xlog("Adding region %d at %p, size %d",nr,cp,nu*sizeof(struct header));

	up=(void*)cp;
	up->size=nu;
	sfree_nolock(up+1,0);
	region[nr].usedblocks++;	// this free cheats...

	return region[nr].allocp;
}

void *smalloc(int nbytes,int nr)
{	
	struct header *p,*q;
	int nunits;

	pthread_mutex_lock(&alloc_mutex);

	nunits=1+(nbytes+sizeof(struct header)-1)/sizeof(struct header);

	if ((q=region[nr].allocp)==NULL) {
		region[nr].base.ptr=region[nr].allocp=q=&region[nr].base;
		region[nr].base.size=0;
	}

	for (p=q->ptr; ; q=p,p=p->ptr) {
		if (p->size>=nunits) {
			if (p->size==nunits) {
				q->ptr=p->ptr;
				region[nr].freeblocks--;
			} else {
				p->size-=nunits;
				p+=p->size;
				p->size=nunits;
			}
			region[nr].allocp=q;
                        region[nr].usedblocks++;
			region[nr].freesize-=nunits*sizeof(struct header);
			
			pthread_mutex_unlock(&alloc_mutex);
			return p+1;
		}
		if (p==region[nr].allocp) {		
			if ((p=morecore(nunits,nr))==NULL) {
				pthread_mutex_unlock(&alloc_mutex);
				return NULL;
			}
		}
	}
}

void *srealloc(void *ptr,int nbytes,int nr)
{
	void *next,*tmp;
	int size;

	tmp=smalloc(nbytes,IM_TEMP); size=min(nbytes,((struct header*)ptr-1)->size*sizeof(struct header)-sizeof(struct header));
	memcpy(tmp,ptr,size);

	sfree(ptr);

	next=smalloc(nbytes,nr);
	if (!ptr) return next;
	
	memcpy(next,tmp,size);
	sfree(tmp);
	
	return next;
}

//-------------------------------------------


/*#define TESTCNT		10000000
#define KEEPPTRCNT	1000
#define PTRCNT		2000

#define TESTMINSIZE	64
#define TESTMAXSIZE	(64*1024)
#define RANDOM(a)	(rand()%(a))

void minfo(void)
{
	int nr,used=0,ublocks=0,fblocks=0;

	for (nr=0; nr<3; nr++) {
		scompact(nr);
		//printf("region %d: size=%.2fM/%.2fM (%d/%d blocks)\n",nr,region[nr].usedsize/1024.0/1024.0,region[nr].havesize/1024.0/1024.0,region[nr].freeblocks,region[nr].usedblocks);
		used+=region[nr].havesize;
		ublocks+=region[nr].usedblocks;
		fblocks+=region[nr].freeblocks;
		
	}
	printf("size=%.2fM (%d/%d blocks)\n",used/1024.0/1024.0,fblocks,ublocks);
}

void memtest(void)
{
	char *a,*b,*c;

	a=smalloc(200,1);
	b=smalloc(200,1);
	c=smalloc(200,1);

	sfree(a);
	sfree(b);
	sfree(c);
}


void memtest2(void)
{
	void *ptr[PTRCNT],*kptr[KEEPPTRCNT];
        int n,i,keep=0;

        bzero(ptr,sizeof(ptr));
	bzero(kptr,sizeof(kptr));

	for (n=0; n<TESTCNT; n++) {
		i=RANDOM(PTRCNT);
		if (ptr[i]) { sfree(ptr[i]); ptr[i]=NULL; }	
		else {
			if (RANDOM(10)) ptr[i]=smalloc(TESTMINSIZE,0);
			else ptr[i]=smalloc(TESTMAXSIZE,1);
		}
		if (n%1000==0) {
			if (kptr[keep]) sfree(kptr[keep]);
			kptr[keep]=smalloc(TESTMINSIZE,2);
			keep++;
			if (keep>=KEEPPTRCNT) keep=0;
		}
		if (n%100000==0) minfo();
	}
        minfo();
	
        for (i=0; i<PTRCNT; i++) {
                if (ptr[i]) sfree(ptr[i]);
	}
	minfo();
	
	for (i=0; i<KEEPPTRCNT; i++) {
                if (kptr[i]) sfree(kptr[i]);
	}
	minfo();
}*/
