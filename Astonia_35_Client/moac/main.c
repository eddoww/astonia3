#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#pragma hdrstop
#include <dsound.h>
#define ISCLIENT
#include "main.h"
#include "client.h"
#include "dd.h"
#include "resource.h"
#include "sound.h"
#include "gui.h"

// extern

extern LRESULT FAR PASCAL _export main_wnd_proc(HWND wnd, UINT msg,WPARAM wparam, LPARAM lparam);
extern int main_init(void);
extern void main_exit(void);

int main_loop(void);
#ifdef EDITOR
int editor_main_loop(void);
#endif

// globs

HICON mainicon;

int quit=0;
int quickstart=0;
int panic_reached=0;
int xmemcheck_failed=0;
HWND mainwnd=NULL;
HINSTANCE instance=NULL;
int opt_res=IDC_FULLSCREEN;
int reduce_light=0;
int reduce_anim=0;
int shade_walls=1;
int shade_floor=1;
int largetext=0;
int vendor=1;
extern int newlight;
char user_keys[10]={'Q','W','E','A','S','D','Z','X','C','V'};
volatile int status_done=0;
volatile HWND status_wnd=NULL;

char userlist[10][40];

HWND lastwnd=NULL;

char memcheck_failed_str[]={"TODO: memcheck failed"};  // TODO
char panic_reached_str[]={"TODO: panic failure"}; // TODO

int xwinsize=800,ywinsize=600;

int MAXLINESHOW=15;

FILE *errorfp;

// note, warn, fail, paranoia, addline

int note(const char *format, ...)
{
        va_list va;
        char buf[1024];


        va_start(va,format);
        vsprintf(buf,format,va);
        va_end(va);

        printf("NOTE: %s\n",buf);
#ifdef DEVELOPER
        addline("NOTE: %s\n",buf);
#endif

        return 0;
}

int warn(const char *format, ...)
{
        va_list va;
        char buf[1024];


        va_start(va,format);
        vsprintf(buf,format,va);
        va_end(va);

        printf("WARN: %s\n",buf);
        addline("WARN: %s\n",buf);

        return 0;
}

int fail(const char *format, ...)
{
        va_list va;
        char buf[1024];


        va_start(va,format);
        vsprintf(buf,format,va);
        va_end(va);

        fprintf(errorfp,"FAIL: %s\n",buf);
        addline("FAIL: %s\n",buf);

        return -1;
}

void paranoia(const char *format, ...)
{
        va_list va;

        fprintf(errorfp,"PARANOIA EXIT in ");

        va_start(va,format);
        vfprintf(errorfp,format,va);
        va_end(va);

        fprintf(errorfp,"\n");
        fflush(errorfp);

        exit(-1);
}

static _addlinesep=0;

void addlinesep(void)
{
        _addlinesep=1;
}

void addline(const char *format,...)
{
	va_list va;
        char buf[1024],*run;
        int num;

        if (_addlinesep) {
                _addlinesep=0;
                addline("-------------");
        }

        va_start(va,format);
	vsnprintf(buf,sizeof(buf)-1,format,va); buf[sizeof(buf)-1]=0;
        va_end(va);

	if (dd_text_init_done()) dd_add_text(buf);
}

// io

int rread(int fd, void *ptr, int size)
{
        int n;

        while (size>0) {
                n=read(fd,ptr,size);
                if (n<0) return -1;
                if (n==0) return 1;
                size-=n;
                ptr=((unsigned char *)(ptr))+n;
        }
        return 0;
}

char *load_ascii_file(char *filename, int ID)
{
        int fd,size;
	char *ptr;

        if ((fd=open(filename,O_RDONLY|O_BINARY))==-1) return NULL;
	if ((size=lseek(fd,0,SEEK_END))==-1) { close(fd); return NULL; }
	if (lseek(fd,0,SEEK_SET)==-1) { close(fd); return NULL; }
	ptr=xmalloc(size+1,ID);
	if (rread(fd,ptr,size)) { xfree(ptr); close(fd); return NULL; }
	ptr[size]=0;
	close(fd);

        return ptr;
}

// memory

//#define malloc_proc(size) GlobalAlloc(GPTR,size)
//#define realloc_proc(ptr,size) GlobalReAlloc(ptr,size,GMEM_MOVEABLE)
//#define free_proc(ptr) GlobalFree(ptr)

HANDLE myheap;
#define malloc_proc(size) myheapalloc(size)
#define realloc_proc(ptr,size) myheaprealloc(ptr,size)
#define free_proc(ptr) myheapfree(ptr)

int memused=0;
int memptrused=0;

int maxmemsize=0;
int maxmemptrs=0;
int memptrs[MAX_MEM];
int memsize[MAX_MEM];

struct memhead
{
        int size;
        int ID;
};

static char *memname[MAX_MEM]={
"MEM_TOTA",	//0
"MEM_GLOB",	
"MEM_TEMP",
"MEM_ELSE",
"MEM_DL",
"MEM_IC",	//5
"MEM_SC",
"MEM_VC",
"MEM_PC",
"MEM_GUI",
"MEM_GAME",	//10
"MEM_EDIT",
"MEM_VPC",
"MEM_VSC",
"MEM_VLC",
"MEM_TEMP1",
"MEM_TEMP2",
"MEM_TEMP3",
"MEM_TEMP4",
"MEM_TEMP5",
"MEM_TEMP6",
"MEM_TEMP7",
"MEM_TEMP8",
"MEM_TEMP9",
"MEM_TEMP10"
};

void *myheapalloc(int size)
{
	void *ptr;
	
	memptrused++;
	ptr=HeapAlloc(myheap,HEAP_ZERO_MEMORY,size);
	memused+=HeapSize(myheap,0,ptr);

	return ptr;
}

void *myheaprealloc(void *ptr,int size)
{
	memused-=HeapSize(myheap,0,ptr);
	ptr=HeapReAlloc(myheap,HEAP_ZERO_MEMORY,ptr,size);
	memused+=HeapSize(myheap,0,ptr);

	return ptr;
}

void myheapfree(void *ptr)
{
	memptrused--;
	memused-=HeapSize(myheap,0,ptr);
	HeapFree(myheap,0,ptr);
}

void list_mem(void)
{
        int i,flag=0;
        MEMORYSTATUS ms;

        note("--mem----------------------");
        for (i=1; i<MAX_MEM; i++) {	
		if (memsize[i] || memptrs[i])  {
			flag=1;
			note("%s %.2fMB in %d ptrs",memname[i],memsize[i]/(1024.0*1024.0),memptrs[i]);
		}
	}
        if (flag) note("%s %.2fMB in %d ptrs",memname[0],memsize[0]/(1024.0*1024.0),memptrs[0]);
        note("%s %.2fMB in %d ptrs","MEM_MAX",maxmemsize/(1024.0*1024.0),maxmemptrs);
        note("---------------------------");

	bzero(&ms,sizeof(ms));
	ms.dwLength=sizeof(ms);
	GlobalMemoryStatus(&ms);
	note("availphys=%.2fM",ms.dwAvailPhys/1024.0/1024.0);
	note("UsedMem=%.2fM / %d",memused/1024.0/1024.0,memptrused);

	note("validate says: %d",HeapValidate(myheap,0,NULL));	

}

static int memcheckset=0;
static unsigned char memcheck[256];

int xmemcheck(void *ptr)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!ptr) return 0;

        mem=(struct memhead *)(((unsigned char *)(ptr))-8-sizeof(memcheck));

        // ID check
        if (mem->ID>=MAX_MEM) { note("xmemcheck: ill mem id (%d)",mem->ID); xmemcheck_failed=1; return -1; }

        // border check
        head=((unsigned char *)(mem))+8;
        rptr=((unsigned char *)(mem))+8+sizeof(memcheck);
        tail=((unsigned char *)(mem))+8+sizeof(memcheck)+mem->size;

        if (memcmp(head,memcheck,sizeof(memcheck))) { fail("xmemcheck: ill head in %s (ptr=%08X)",memname[mem->ID],rptr); xmemcheck_failed=1; return -1; }
        if (memcmp(tail,memcheck,sizeof(memcheck))) { fail("xmemcheck: ill tail in %s (ptr=%08X)",memname[mem->ID],rptr); xmemcheck_failed=1; return -1; }

        return 0;
}

void *xmalloc(int size, int ID)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!memcheckset) {
                note("initialized memcheck");
                for (memcheckset=0; memcheckset<sizeof(memcheck); memcheckset++) memcheck[memcheckset]=rrand(256);
                sprintf(memcheck,"!MEMCKECK MIGHT FAIL!");
		myheap=HeapCreate(0,0,0);
        }

        if (!size) return NULL;

        mem=malloc_proc(8+sizeof(memcheck)+size+sizeof(memcheck));
        if (!mem) { fail("OUT OF MEMORY !!!"); return NULL; }

        if (ID>=MAX_MEM) { fail("xmalloc: ill mem id"); return NULL; }

        mem->ID=ID;
        mem->size=size;
        memsize[mem->ID]+=mem->size;
        memptrs[mem->ID]+=1;
        memsize[0]+=mem->size;
        memptrs[0]+=1;

        if (memsize[0]>maxmemsize) maxmemsize=memsize[0];
        if (memptrs[0]>maxmemptrs) maxmemptrs=memptrs[0];

        head=((unsigned char *)(mem))+8;
        rptr=((unsigned char *)(mem))+8+sizeof(memcheck);
        tail=((unsigned char *)(mem))+8+sizeof(memcheck)+mem->size;

        // set memcheck
        memcpy(head,memcheck,sizeof(memcheck));
        memcpy(tail,memcheck,sizeof(memcheck));

        xmemcheck(rptr);

        return rptr;
}

void *xcalloc(int size, int ID)
{
        void *ptr;

        ptr=xmalloc(size,ID);
        if (ptr) bzero(ptr,size);
        return ptr;
}

char *xstrdup(const char *src,int ID)
{
	int size;
	char *dst;

	size=strlen(src)+1;
	
	dst=xmalloc(size,ID);
	if (!dst) return NULL;
	
	memcpy(dst,src,size);

	return dst;
}


void xfree(void *ptr)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!ptr) return;
        if (xmemcheck(ptr)) return;

        // get mem
        mem=(struct memhead *)(((unsigned char *)(ptr))-8-sizeof(memcheck));

        // free
        memsize[mem->ID]-=mem->size;
        memptrs[mem->ID]-=1;
        memsize[0]-=mem->size;
        memptrs[0]-=1;

        free_proc(mem);
}

void xinfo(void *ptr)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!ptr) { printf("NULL"); return; }
        if (xmemcheck(ptr)) { printf("ILL"); return; }

        // get mem
        mem=(struct memhead *)(((unsigned char *)(ptr))-8-sizeof(memcheck));

	printf("%d bytes",mem->size);
}

void *xrealloc(void *ptr, int size, int ID)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!ptr) return xmalloc(size,ID);
        if (!size) { xfree(ptr); return NULL; }
        if (xmemcheck(ptr)) return NULL;

        mem=(struct memhead *)(((unsigned char *)(ptr))-8-sizeof(memcheck));

        // realloc
        memsize[mem->ID]-=mem->size;
        memptrs[mem->ID]-=1;
        memsize[0]-=mem->size;
        memptrs[0]-=1;

        mem=realloc_proc(mem,8+sizeof(memcheck)+size+sizeof(memcheck));
        if (!mem) { fail("xrealloc: OUT OF MEMORY !!!"); return NULL; }

        mem->ID=ID;
        mem->size=size;
        memsize[mem->ID]+=mem->size;
        memptrs[mem->ID]+=1;
        memsize[0]+=mem->size;
        memptrs[0]+=1;

        if (memsize[0]>maxmemsize) maxmemsize=memsize[0];
        if (memptrs[0]>maxmemptrs) maxmemptrs=memptrs[0];

        head=((unsigned char *)(mem))+8;
        rptr=((unsigned char *)(mem))+8+sizeof(memcheck);
        tail=((unsigned char *)(mem))+8+sizeof(memcheck)+mem->size;

        // set memcheck
        memcpy(head,memcheck,sizeof(memcheck));
        memcpy(tail,memcheck,sizeof(memcheck));

        return rptr;
}

void *xrecalloc(void *ptr, int size, int ID)
{
        struct memhead *mem;
        unsigned char *head,*tail,*rptr;

        if (!ptr) return xcalloc(size,ID);
        if (!size) { xfree(ptr); return NULL; }
        if (xmemcheck(ptr)) return NULL;

        mem=(struct memhead *)(((unsigned char *)(ptr))-8-sizeof(memcheck));

        // realloc
        memsize[mem->ID]-=mem->size;
        memptrs[mem->ID]-=1;
        memsize[0]-=mem->size;
        memptrs[0]-=1;

        mem=realloc_proc(mem,8+sizeof(memcheck)+size+sizeof(memcheck));
        if (!mem) { fail("xrecalloc: OUT OF MEMORY !!!"); return NULL; }

        if (size-mem->size>0) {
                bzero(((unsigned char *)(mem))+8+sizeof(memcheck)+mem->size,size-mem->size);
        }

        mem->ID=ID;
        mem->size=size;
        memsize[mem->ID]+=mem->size;
        memptrs[mem->ID]+=1;
        memsize[0]+=mem->size;
        memptrs[0]+=1;

        if (memsize[0]>maxmemsize) maxmemsize=memsize[0];
        if (memptrs[0]>maxmemptrs) maxmemptrs=memptrs[0];

        head=((unsigned char *)(mem))+8;
        rptr=((unsigned char *)(mem))+8+sizeof(memcheck);
        tail=((unsigned char *)(mem))+8+sizeof(memcheck)+mem->size;

        // set memcheck
        memcpy(head,memcheck,sizeof(memcheck));
        memcpy(tail,memcheck,sizeof(memcheck));

        return rptr;
}

void addptr(void ***list, int *count, void *ptr, int ID)
{
    (*count)++;
    (*list)=(void **)xrealloc((*list),(*count)*sizeof(void *),ID);
    (*list)[*count-1]=ptr;
}

void delptr(void ***list, int *count, void *ptr, int ID)
{
    int i;

    for(i=0; i<(*count); i++) if((*list)[i]==ptr) break;
    if(i==(*count)) return;
    memmove(&(*list)[i],&(*list)[i+1],((*count)-i-1)*sizeof(void *));
    (*count)--;
    (*list)=(void **)xrealloc((*list),(*count)*sizeof(void *),ID);
}

// rrandom

void rrandomize(void)
{
    srand(time(NULL));
}

void rseed(int seed)
{
    srand(seed);
}

int rrand(int range)
{
    int r;

    r = rand();
    return (range*r/(RAND_MAX+1));
}

// wsa network

int net_init(void)
{
        WSADATA wsadata;

        if (WSAStartup(0x00020002,&wsadata)) return -1;
        return 0;
}

int net_exit(void)
{
        WSACleanup();
        return 0;
}

// parsing command line

#ifdef EDITOR
int editor;
#endif
int call_gfx_main;
int call_sfx;

char with_cmd;
int with_nr;

char filmfilename[1024];

unsigned int validate_intra(unsigned int ip)
{
	if ((ip&0xff000000)==0x7f000000) return ip;
	if ((ip&0xff000000)==0x0a000000) return ip;
	if ((ip&0xffff0000)==0xac100000) return ip;
	if ((ip&0xffff0000)==0xc0a80000) return ip;
	
	return 0;
}

int parse_cmd(char *s)
{
	int n;
        char *end;

	while (isspace(*s)) s++;
	//while (*s && *s!='-') s++;

        while (*s) {
		if (*s=='-') {
			s++;
			if (tolower(*s)=='u') {         // -u <username>
				s++;
				while (isspace(*s)) s++;
				n=0; while (n<40 && *s && !isspace(*s))	username[n++]=*s++;
				username[n]=0;
				quickstart=1;
			}
                        else if (tolower(*s)=='p') {  // -p <password>
				s++;
				while (isspace(*s)) s++;
				n=0; while (n<16 && *s && !isspace(*s)) password[n++]=*s++;
                                password[n]=0;
			}
                        else if (tolower(*s)=='s') {  // -s dont use systemmemory
				s++;
				dd_usesysmem=0;
			}
                        else if (tolower(*s)=='w') {  // -w windowed mode
				s++;
				dd_windowed=1;
			}
                        else if (tolower(*s)=='t') {  // -t <num> maximum number of videocache tiles
                                dd_maxtile=strtol(s+1,&end,10);
                                s=end;
			}
			else if (tolower(*s)=='d') {
				s++;
                                developer_server=strtol(s+1,&end,10);
                                s=end;;
				if (developer_server!=554433) developer_server=0;				
			}

#ifdef EDITOR
                        else if (tolower(*s)=='e') {  // -e start the editor
				s++;
				editor=1;
			}
                        else if (tolower(*s)=='a') {  // -a areaid of the editor (default is 1)
                                extern int  areaid;
                                areaid=strtol(s+1,&end,10);
                                s=end;
			}
#endif
                        else if (tolower(*s)=='g') {  // -g <cmd> <nr> call gfx_main(nr)
                                s++;
                                call_gfx_main=1;
                                if (*s) with_cmd=*s++; else return -3;
                                if (*s && !isspace(with_cmd)) with_nr=strtol(s+1,&s,10);
			}
			else if (tolower(*s)=='f') {
                                s++;
				call_sfx=1;
			}
			else if (tolower(*s)=='y') {
				extern unsigned int sock_server;
				s++;
				while (isspace(*s)) s++;
				sock_server=validate_intra(ntohl(inet_addr(s)));
				while (*s && *s!=' ') s++;				
			}
			else if (tolower(*s)=='z') {
				extern unsigned short sock_port;
				s++;
				while (isspace(*s)) s++;
				sock_port=atoi(s);
				while (*s && *s!=' ') s++;				
			}
			else if (tolower(*s)=='h') {
				extern char sock_user[80];
				int n=0;
				s++;
				while (isspace(*s)) s++;
				while (*s && !isspace(*s) && n<79) sock_user[n++]=*s++;
				sock_user[n]=0;
                                while (*s && *s!=' ') s++;				
			}
			else if (tolower(*s)=='x' || tolower(*s)=='b' || tolower(*s)=='c') {
                                s++;				
			}
			else { printf("haha\n"); return -1; }
		} else { printf("huha\n"); return -2; }
		while (isspace(*s)) s++;
	}
	return 0;
}

// windows

LRESULT FAR PASCAL _export main_wnd_proc_safe(HWND wnd, UINT msg,WPARAM wparam, LPARAM lparam)
{
        switch(msg) {
                case WM_DESTROY:
                        PostQuitMessage(0);
                        quit=1;
                        return 0;
                case WM_KEYDOWN:
                        DestroyWindow(wnd);
                        return 0;

        }
        return DefWindowProc(wnd,msg,wparam,lparam);
}

int win_init(char *title, int width, int height)
{
	WNDCLASS wc;
	HWND wnd;

        lastwnd=GetForegroundWindow();

	wc.style=CS_HREDRAW|CS_VREDRAW;
        wc.lpfnWndProc=main_wnd_proc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=instance;
	wc.hIcon=LoadIcon(instance,MAKEINTRESOURCE(1));
	wc.hCursor=NULL;
	wc.hbrBackground=NULL;
	wc.lpszMenuName=NULL;
	wc.lpszClassName="MAINWNDMOAC";

	RegisterClass(&wc);
#ifdef EDITOR
        if (dd_windowed && editor) wnd=CreateWindowEx(0,"MAINWNDMOAC",title,WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_MAXIMIZEBOX|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,width+8,height+27,NULL,NULL,instance,NULL);
        else
#endif																// +6, +25
        if (dd_windowed) wnd=CreateWindowEx(0,"MAINWNDMOAC",title,WS_POPUP|WS_CAPTION|WS_DLGFRAME,CW_USEDEFAULT,CW_USEDEFAULT,width,height,NULL,NULL,instance,NULL);
        else wnd=CreateWindowEx(WS_EX_TOPMOST,"MAINWNDMOAC",title,WS_POPUP,0,0,width,height,NULL,NULL,instance,NULL);

	mainwnd=wnd;

        SetFocus(wnd);
	ShowWindow(wnd,SW_SHOW);
	UpdateWindow(wnd);

	if (dd_windowed) {
		RECT r;
		int x,y;

		GetClientRect(wnd,&r);
		x=r.right-r.left;
		y=r.bottom-r.top;

                x=width+width-x;
		y=height+height-y;

		SetWindowPos(wnd,HWND_TOP,xwinsize/2-x/2,ywinsize/2-y/2,x,y,0);

	}

	return 0;
}

int win_exit(void)
{
        MSG msg;

        if (mainwnd) {
                DestroyWindow(mainwnd);
        }

        UnregisterClass("MAINWNDMOAC",instance);
        if (!dd_windowed) SetForegroundWindow(lastwnd);

        return 0;
}

// startup

void update_res(HWND hwnd)
{
	if (opt_res==IDC_FULLSCREEN) CheckRadioButton(hwnd,IDC_FULLSCREEN,IDC_WINDOWED,IDC_FULLSCREEN);
	else if (opt_res==IDC_WINDOWED) CheckRadioButton(hwnd,IDC_FULLSCREEN,IDC_WINDOWED,IDC_WINDOWED);
	else if (opt_res==IDC_SHRUNK) CheckRadioButton(hwnd,IDC_FULLSCREEN,IDC_WINDOWED,IDC_SHRUNK);
        else { CheckRadioButton(hwnd,IDC_FULLSCREEN,IDC_WINDOWED,IDC_FULLSCREEN); opt_res=IDC_FULLSCREEN; }
	
}

int save_pwd=0;
void update_savepwd(HWND hwnd)
{
	CheckDlgButton(hwnd,IDC_SAVEPWD,save_pwd);
}

int enable_sound=0;
void update_sound(HWND hwnd)
{
	CheckDlgButton(hwnd,IDC_SOUND,enable_sound);
}

void update_walls(HWND hwnd)
{
	CheckDlgButton(hwnd,IDC_WALLS,shade_walls);
}

void update_floor(HWND hwnd)
{
	CheckDlgButton(hwnd,IDC_FLOOR,shade_floor);
}

void update_large(HWND hwnd)
{
	CheckDlgButton(hwnd,IDC_LARGETEXT,largetext);
}

static char key[80]={"slkfjsltuioertuierotudlrfkgjmlksdfgjmcioesrgjcmiogjoeirfjieorgfjlkdgjlfskdgmslk"};
void save_options(void)
{
	HKEY hk;
	char buf[80];
        int n;

	if (RegCreateKey(HKEY_CURRENT_USER,"Software\\Intent\\Astonia35",&hk)!=ERROR_SUCCESS) return;

	username[0]=toupper(username[0]);
	for (n=1; username[n]; n++) username[n]=tolower(username[n]);
	
	RegSetValueEx(hk,"username",0,REG_BINARY,(void*)username,sizeof(username));
	
	if (save_pwd) {
		for (n=0; n<sizeof(password); n++) buf[n]=password[n]^key[n];
	} else {
		for (n=0; n<sizeof(password); n++) buf[n]=key[n];
	}
        RegSetValueEx(hk,"password",0,REG_BINARY,(void*)buf,sizeof(password));
        RegSetValueEx(hk,"save_pwd",0,REG_DWORD,(void*)&save_pwd,4);
	RegSetValueEx(hk,"opt_res",0,REG_DWORD,(void*)&opt_res,4);
	RegSetValueEx(hk,"shade_walls",0,REG_DWORD,(void*)&shade_walls,4);
	RegSetValueEx(hk,"shade_floor",0,REG_DWORD,(void*)&shade_floor,4);
	RegSetValueEx(hk,"largetext",0,REG_DWORD,(void*)&largetext,4);
        RegSetValueEx(hk,"enable_sound",0,REG_DWORD,(void*)&enable_sound,4);
	RegSetValueEx(hk,"user_keys",0,REG_BINARY,(void*)user_keys,sizeof(user_keys));

        for (n=0; n<10; n++) {
		if (!userlist[n][0]) continue;
                if (!strcmp(userlist[n],username)) break;		
	}
	if (n==10) {
		memmove(userlist+1,userlist,sizeof(userlist)-sizeof(userlist[0]));
		strcpy(userlist[0],username);
	}

	RegSetValueEx(hk,"userlist",0,REG_BINARY,(void*)userlist,sizeof(userlist));

}

static void load_options(void)
{
	HKEY hk;
	int size=4,type,handle,len,n;
	char buf[80];

	handle=open("vendor.dat",O_RDONLY|O_BINARY);
	if (handle!=-1) {
		len=read(handle,buf,sizeof(buf)-1);
		buf[len]=0;
		vendor=atoi(buf);
		close(handle);
	}

	if (RegCreateKey(HKEY_CURRENT_USER,"Software\\Intent\\Astonia35",&hk)!=ERROR_SUCCESS) return;

	size=sizeof(username);
	RegQueryValueEx(hk,"username",0,(void*)&type,(void*)&username,(void*)&size);
	size=sizeof(password);
	RegQueryValueEx(hk,"password",0,(void*)&type,(void*)&password,(void*)&size);

	if (password[0]) {
		for (n=0; n<sizeof(password); n++) {
			password[n]=password[n]^key[n];
		}
	}

	size=4;
	RegQueryValueEx(hk,"save_pwd",0,(void*)&type,(void*)&save_pwd,(void*)&size);
	RegQueryValueEx(hk,"opt_res",0,(void*)&type,(void*)&opt_res,(void*)&size);
	RegQueryValueEx(hk,"shade_walls",0,(void*)&type,(void*)&shade_walls,(void*)&size);
	RegQueryValueEx(hk,"shade_floor",0,(void*)&type,(void*)&shade_floor,(void*)&size);
	RegQueryValueEx(hk,"enable_sound",0,(void*)&type,(void*)&enable_sound,(void*)&size);
	RegQueryValueEx(hk,"largetext",0,(void*)&type,(void*)&largetext,(void*)&size);
	
	size=sizeof(user_keys);
	RegQueryValueEx(hk,"user_keys",0,(void*)&type,(void*)&user_keys,(void*)&size);

	size=sizeof(userlist);
	RegQueryValueEx(hk,"userlist",0,(void*)&type,(void*)&userlist,(void*)&size);
}

int check_username(char *ptr)
{
	while (*ptr) if (isdigit(*ptr)) return 0; else ptr++;
	
	return 1;
}


#pragma argsused
BOOL WINAPI start_dlg_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	char buf[80];
	int n,xs,ys;
	RECT r;

        switch (msg) {
		case WM_INITDIALOG:
                        SetClassLong(wnd,GCL_HICON,(long)LoadIcon(instance,MAKEINTRESOURCE(1)));
			sprintf(buf,"Astonia v%d.%02d.%02d",(VERSION>>16)&255,(VERSION>>8)&255,(VERSION)&255);
                        SetWindowText(wnd,buf);

			GetWindowRect(wnd,&r);
			xs=r.right-r.left; ys=r.bottom-r.top;
			SetWindowPos(wnd,HWND_TOP,xwinsize/2-xs/2,ywinsize/2-ys/2,0,0,SWP_NOSIZE);
			
			SetDlgItemText(wnd,IDC_PASSWORD,password);

                        for (n=0; n<10; n++) {
				if (userlist[n][0]) {
					SendDlgItemMessage(wnd,IDC_USERNAME,CB_ADDSTRING,0,(long)userlist[n]);
					if (!strcmp(userlist[n],username)) SendDlgItemMessage(wnd,IDC_USERNAME,CB_SETCURSEL,n,0);
				}
			}
			
                        CreateWindowEx(WS_EX_STATICEDGE,"STATIC",NULL,SS_BITMAP|WS_VISIBLE|WS_CHILD,0,0,0,0,wnd,(HMENU)1999,instance,NULL);
                        SendDlgItemMessage(wnd,1999,STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)LoadBitmap(instance,MAKEINTRESOURCE(BITMAP_STARTUP)));

                        update_savepwd(wnd);
			update_sound(wnd);
			update_walls(wnd);
			update_floor(wnd);
			update_large(wnd);
			update_res(wnd);
			status_wnd=wnd;
                        return TRUE;
                case WM_COMMAND:
                        switch (wparam) {
                                case IDCANCEL:
                                        EndDialog(wnd,-1);
                                        return TRUE;
				case IDOK:
					//if (!status_done) return TRUE;
					
                                        GetDlgItemText(wnd,IDC_USERNAME,username,sizeof(username));
					if (!check_username(username)) {
						MessageBox(wnd,"The character name you entered does not seem to be correct. Please enter a character name, not your account ID. If you do not have a character yet, go to the website at www.astonia.com and create one in your account management.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
						return TRUE;
					}
                                        GetDlgItemText(wnd,IDC_PASSWORD,password,sizeof(password));
					save_options();
                                        EndDialog(wnd,0);
                                        return TRUE;
				
				case IDC_FULLSCREEN:
				case IDC_WINDOWED:
				case IDC_SHRUNK:
					opt_res=wparam;
                                        update_res(wnd);
                                        return 1;

				case IDC_SAVEPWD:
                                        if (save_pwd) save_pwd=0;
					else save_pwd=1;
					update_savepwd(wnd);
					return 1;

				case IDC_SOUND:
                                        if (enable_sound) enable_sound=0;
					else enable_sound=1;
					update_sound(wnd);
					return 1;

                                case IDC_WALLS:
					if (shade_walls) shade_walls=0;
					else shade_walls=1;
					update_walls(wnd);
					return 1;
	
				case IDC_FLOOR:
					if (shade_floor) shade_floor=0;
					else shade_floor=1;
					update_floor(wnd);
					return 1;
				case IDC_LARGETEXT:
					if (largetext) largetext=0;
					else largetext=1;
					update_large(wnd);
					return 1;
				case IDC_ACCOUNT:
					if (developer_server) ShellExecute(wnd,"open","http://astonia.dyndns.org/cgi-v35/account.cgi",NULL,NULL,SW_SHOWNORMAL);
                                        else ShellExecute(wnd,"open","http://urd.astonia.com/cgi-v35/account.cgi",NULL,NULL,SW_SHOWNORMAL);
					return 1;
				case IDC_WEBSITE:
					if (developer_server) ShellExecute(wnd,"open","http://astonia.dyndns.org/",NULL,NULL,SW_SHOWNORMAL);
                                        else ShellExecute(wnd,"open","http://urd.astonia.com/",NULL,NULL,SW_SHOWNORMAL);
					return 1;
				case IDC_USERNAME:
					return 1;
                        }
                        return FALSE;
        }
        return FALSE;
}

#pragma argsused
DWORD WINAPI status_thread(LPVOID dummy)
{
	int sock,n,done=0,ver=0,cnt;
	unsigned int server;
	struct hostent *he;
	struct sockaddr_in addr;
	static char *cmd="GET /cgi-v35/info.cgi?step=1 HTTP/1.0\n\n";
	char buf[2048];

	status_done=0;
	while (!status_wnd) Sleep(1);

	//SendDlgItemMessage(status_wnd,IDOK,BM_SETSTYLE,BS_OWNERDRAW,1);


	SetDlgItemText(status_wnd,IDC_STATUS,"Creating Socket");
	if ((sock=socket(PF_INET,SOCK_STREAM,0))==INVALID_SOCKET) {
		SetDlgItemText(status_wnd,IDC_STATUS,"Failed creating socket");
		goto badchoice;
	}

#ifdef DEVELOPER
	server=(192<<24)|(168<<16)|(42<<8)|(27<<0);
#else

	SetDlgItemText(status_wnd,IDC_STATUS,"Resolving Address");
	if (developer_server) he=gethostbyname("astonia.dyndns.org");
	else he=gethostbyname("urd.astonia.com");
	if (he) server=ntohl(*(unsigned long*)(*he->h_addr_list));
	else {
		SetDlgItemText(status_wnd,IDC_STATUS,"Failed resolving address");
		goto badchoice;
	}
#endif

	SetDlgItemText(status_wnd,IDC_STATUS,"Connecting to Webserver");
	addr.sin_family=AF_INET;
	addr.sin_port=htons(80);
	addr.sin_addr.s_addr=htonl(server);
	if ((connect(sock,(struct sockaddr*)&addr,sizeof(addr)))) {
		SetDlgItemText(status_wnd,IDC_STATUS,"Connecting failed");
		goto badchoice;
	}

	SetDlgItemText(status_wnd,IDC_STATUS,"Sending Request");
	if (send(sock,cmd,strlen(cmd),0)<(signed)strlen(cmd)) {
		SetDlgItemText(status_wnd,IDC_STATUS,"Sending failed");
		goto badchoice;
	}

	SetDlgItemText(status_wnd,IDC_STATUS,"Reading Answer");
	while (1) {
                n=recv(sock,buf+done,sizeof(buf)-done,0);
                if (n<1) break;
		done+=n;
	}
	buf[done]=0;

	for (n=cnt=0; n<done; n++) {
		if (cnt==2) break;
		if (buf[n]==10) cnt++;
		if (buf[n]!=10 && buf[n]!=13) cnt=0;
	}
	ver=atoi(buf+n);
	for (; n<done; n++) {
		if (buf[n]==':') { n++; break; }
	}
	cnt=atoi(buf+n);

	sprintf(buf,"Version: %d.%02d.%02d, Online: %d, %s",
		(ver>>16)&255,(ver>>8)&255,(ver)&255,
		cnt,
		VERSION>=ver ? "Client is up to date" : "Client needs update");

	SetDlgItemText(status_wnd,IDC_STATUS,buf);

        badchoice:
	if (VERSION>=ver) {
		status_done=1;
		//SendDlgItemMessage(status_wnd,IDOK,BM_SETSTYLE,BS_PUSHBUTTON,1);
	} else {
		sprintf(buf,"http://urd.astonia.com/update_%d_%02d_%02d.html",(VERSION>>16)&255,(VERSION>>8)&255,(VERSION)&255);
		ShellExecute(status_wnd,"open",buf,NULL,NULL,SW_SHOWNORMAL);
		exit(0);
	}
	return 0;
}

int win_start(void)
{
	DWORD ID;
	CreateThread(NULL,8192,status_thread,NULL,0,&ID);
        return DialogBox(instance,MAKEINTRESOURCE(IDD_STARTUP),NULL,start_dlg_proc);
}
// _-----------------------------
/*int fill_xxx(unsigned short *ptr,int size)
{
	int n;
	for (n=0; n<size; n++) {
		*ptr++=((n/16)%2)*65530;
	}
}


BOOL dd_sound(HWND hwnd)
{
    LPDIRECTSOUND lpDirectSound=NULL;
    DSBUFFERDESC dsbdesc;
    DSBCAPS dsbcaps;
    HRESULT hr;
    WAVEFORMATEX pcmwf;
    LPDIRECTSOUNDBUFFER lpDsb;
    DWORD dwBufferSize;


    hr=DirectSoundCreate(NULL,&lpDirectSound,NULL);
    if (DS_OK!=hr) { MessageBox(hwnd,"DirectSoundCreate.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP); return FALSE; }

    note("2");
    // Set up wave format structure.
    memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
    pcmwf.wFormatTag = WAVE_FORMAT_PCM;
    pcmwf.nChannels = 2;
    pcmwf.nSamplesPerSec = 22050;
    pcmwf.nBlockAlign = 4;
    pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
    pcmwf.wBitsPerSample = 16;
    note("3");

    // Set up DSBUFFERDESC structure.
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); // Zero it out.
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    note("4");

    // Buffer size is determined by sound hardware.
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat = NULL; // Must be NULL for primary buffers.
    note("5 %p",lpDirectSound);

    // Obtain write-primary cooperative level.
    hr=lpDirectSound->lpVtbl->SetCooperativeLevel(lpDirectSound,hwnd,DSSCL_WRITEPRIMARY);
    note("6");

    if (DS_OK == hr) {
        // Succeeded. Try to create buffer.
        hr = lpDirectSound->lpVtbl->CreateSoundBuffer(lpDirectSound,&dsbdesc, &lpDsb, NULL);
	note("7");

        if (DS_OK == hr) {
            // Succeeded. Set primary buffer to desired format.
            hr = lpDsb->lpVtbl->SetFormat(lpDsb, &pcmwf);
	    note("8");

	    if (DS_OK == hr) {
		char buf[80];
		unsigned short *ptr1,*ptr2;
		unsigned long size1,size2,n;

                // If you want to know the buffer size, call GetCaps.
                dsbcaps.dwSize = sizeof(DSBCAPS);
                lpDsb->lpVtbl->GetCaps(lpDsb, &dsbcaps);
                dwBufferSize = dsbcaps.dwBufferBytes;
		sprintf(buf,"buffer size is %d",dwBufferSize);
		MessageBox(NULL,buf,"OK",MB_APPLMODAL|MB_OK|MB_ICONSTOP);

                hr=lpDsb->lpVtbl->Play(lpDsb,0,0,DSBPLAY_LOOPING);
		if (DS_OK!=hr) { MessageBox(hwnd,"Play.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP); return FALSE; }

		for (n=0; n<100; n++) {
                        hr=lpDsb->lpVtbl->Lock(lpDsb,0,16384,(void*)&ptr1,&size1,(void*)&ptr2,&size2,DSBLOCK_FROMWRITECURSOR);
			if (DS_OK!=hr) { MessageBox(hwnd,"Lock.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP); return FALSE; }
			if (ptr1) fill_xxx(ptr1,size1/2); //memset(ptr1,n%2 ? 255 : 0,size1);
			if (ptr2) fill_xxx(ptr2,size2/2); //memset(ptr2,n%2 ? 255 : 0,size2);
			note("%p %d %p %d",ptr1,size1,ptr2,size2);
			hr=lpDsb->lpVtbl->Unlock(lpDsb,(void*)ptr1,size1,(void*)ptr2,size2);
			if (DS_OK!=hr) { MessageBox(hwnd,"Unlock.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP); return FALSE; }

			hr=lpDsb->lpVtbl->GetCurrentPosition(lpDsb,&size1,&size2);
			note("%d %d",size1,size2);
			Sleep(10);
		}

                return TRUE;
            } else MessageBox(NULL,"SetFormat ","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
        } else MessageBox(NULL,"CreateSoundBuffer.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
    } else MessageBox(NULL,"SetCooperativeLevel.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);

    return FALSE;
}
*/

#ifdef DEVELOPER
extern int gfx_main(char cmd, int nr);
#endif

#pragma argsused
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
        int ret;
        int width,height;
	char buf[80];
	extern int x_offset,y_offset,x_max,y_max;
	extern int lightquality;
	struct hostent *he;
	extern int  areaid;
	RECT r;

#ifdef STAFFER
	chdir("c:\\a3edit");
#endif

        errorfp=fopen("moac.log","a");
	if (!errorfp) errorfp=stderr;

#ifdef STAFFER
	editor=1;
	areaid=1;
	if (lpCmdLine && isdigit(lpCmdLine[0])) areaid=atoi(lpCmdLine);	
#else
        load_options();

 	if ((ret=parse_cmd(lpCmdLine))!=0) return fail("ill command (%d)",ret);
#endif

#ifdef DEVELOPER
        if (call_gfx_main && gfx_main(with_cmd,with_nr)!=1) return 0;
	if (call_sfx) { make_sound_pak(); return 0; }
#endif

        // set some stuff
        if (!*username || !*password) quickstart=0;
#ifdef EDITOR
        if (editor) quickstart=1;
#endif

        // set instance
        instance=hInstance;

        // next init (only once)
	if (net_init()==-1) {
		MessageBox(NULL,"Can't Initialize Windows Networking Libraries.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
		return -1;
	}

        GetWindowRect(GetDesktopWindow(),&r);
	xwinsize=r.right-r.left;
	ywinsize=r.bottom-r.top;

	if (!quickstart && win_start()) goto done;

#ifdef EDITOR
        if (editor) quickstart=1;
        if (editor && dd_windowed) {
                width=0;
                height=0;
        }
        else if (editor) {
                width=1024;
                height=768;
        }
        else
#endif
        {
                switch(opt_res) {
			case IDC_SHRUNK: 	width=xwinsize; height=ywinsize; x_offset=(xwinsize-800)/2; y_offset=(ywinsize-600)/2; break;
			case IDC_WINDOWED:	dd_windowed=1;	// fall thru intended
			case IDC_FULLSCREEN:
			default:		width=800; height=600; x_offset=y_offset=0; break;
		}
		x_max=x_offset+800;
		y_max=y_offset+600;

		lightquality=0;
		if (shade_walls) lightquality|=1;
		if (shade_floor) lightquality|=3;

        }
	if (developer_server) {
		he=gethostbyname("astonia.dyndns.org");
		if (he) base_server=target_server=ntohl(*(unsigned long*)(*he->h_addr_list));
		else {
			base_server=target_server=(192<<24)|(168<<16)|(42<<8)|(2<<0);
			fail("gethostbyname4a failed, using default");
		}
	} else

#ifndef DEVELOPER
	
	he=gethostbyname("urd.astonia.com");
	if (he) base_server=target_server=ntohl(*(unsigned long*)(*he->h_addr_list));
	else {
		base_server=target_server=(195<<24)|(90<<16)|(27<<8)|(38<<0);
		fail("gethostbyname4 failed, using default");
	}
#endif
	note("Using login server at %u.%u.%u.%u",(target_server>>24)&255,(target_server>>16)&255,(target_server>> 8)&255,(target_server>> 0)&255);

        // init random
        rrandomize();

	sprintf(buf,"Astonia 3 v%d.%d.%d",(VERSION>>16)&255,(VERSION>>8)&255,(VERSION)&255);
        if (win_init(buf,800,600)==-1) {
                win_exit();
		MessageBox(NULL,"Can't Create a Window.","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);		
		return -1;
	}

	//dd_maxtile=100;

        if (dd_init(width,height)==-1) {
                char buf[512];
                dd_exit();
		
		sprintf(buf,"Can't Initialize DirectX\n%s\nPlease make sure you have DirectX and the latest drivers\nfor your graphics card installed.",DDERR);
		MessageBox(mainwnd,buf,"Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);

		win_exit();		
                net_exit();
		return -1;
	}
#ifdef DOSOUND
        init_sound(mainwnd);
#endif
	vid_init();

	if (largetext) {
		extern int namesize;
		
		namesize=0;
		dd_set_textfont(1);
	}

        if (main_init()==-1) {
                dd_exit();
                win_exit();
                net_exit();
		MessageBox(NULL,"Can't Initialize Program","Error",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
		return -1;
        }	
	update_user_keys();

#ifdef EDITOR
        if (editor) editor_main_loop();
        else
#endif
        main_loop();

        main_exit();
        dd_exit();
        win_exit();

        list_mem();

	if (panic_reached) MessageBox(NULL,panic_reached_str,"recursion panic",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
        if (xmemcheck_failed) MessageBox(NULL,memcheck_failed_str,"memory panic",MB_APPLMODAL|MB_OK|MB_ICONSTOP);
done:
        net_exit();
        return 0;
}

//#define HACKER
#ifdef HACKER

void cmd_logf(const char *format, ...)
{
        va_list va;
        char buf[1024];


        va_start(va,format);
        vsprintf(buf,format,va);
        va_end(va);

	cmd_log(buf);
}

void hacker_ls(char *searchstring)
{
	WIN32_FIND_DATA data;
        HANDLE find;
	int cnt=0;

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
		cmd_logf("HCK: directory %s",searchstring);

                do {
			cmd_logf("HCK: %s %d",data.cFileName,data.nFileSizeLow);
                } while (FindNextFile(find,&data) && cnt++<100);

                FindClose(find);
        } else {
		cmd_logf("HCK: directory %s - failed",searchstring);
		return;
	}
	cmd_log("HCK: directory done");
}

void hacker_cat(char *filename)
{
	FILE *fp;
	char buf[200],*ptr;
	int cnt=0;

        fp=fopen(filename,"r");
	if (!fp) {
		cmd_logf("HCK: dump of file %s - failed",filename);
		return;
	}

	cmd_logf("HCK: dump of file %s",filename);

	while (fgets(buf,200,fp) && cnt++<100) {
		for (ptr=buf; *ptr && *ptr!='\r' && *ptr!='\n'; ptr++) ;
		*ptr=0;

		cmd_logf("HCK: %s",buf);
	}
	fclose(fp);
	
	cmd_log("HCK: dump done");
}
#endif
