#include <stdio.h>
#include <alloc.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>
#include <process.h>
#include <errno.h>
#include <dsound.h>
#include <math.h>
#include "main.h"
#include "dd.h"

/* NOTE: we could speed up sound a lot by caching and re-using buffers containing recently played sounds */

extern int enable_sound;

LPDIRECTSOUND ds;

char *sound_error(int nr)
{
	static char buf[256];
	char *err;
	switch(nr) {
		case DSERR_INVALIDCALL:			err="DSERR_INVALIDCALL"; break;
		case DSERR_PRIOLEVELNEEDED: 		err="DSERR_PRIOLEVELNEEDED"; break;
		case DSERR_UNSUPPORTED:			err="DSERR_UNSUPPORTED"; break;
		case DSERR_UNINITIALIZED:		err="DSERR_UNINITIALIZED"; break;
		case DSERR_OUTOFMEMORY:			err="DSERR_OUTOFMEMORY"; break;
		case DSERR_NOAGGREGATION:		err="DSERR_NOAGGREGATION"; break;
		case DSERR_INVALIDPARAM: 		err="DSERR_INVALIDPARAM"; break;
		case DSERR_BADFORMAT:			err="DSERR_BADFORMAT"; break;
		case DSERR_ALLOCATED:			err="DSERR_ALLOCATED"; break;
		default: 				err="unknown"; break;
	}

	sprintf(buf,"DDSOUND says: %d (%s)",nr,err);

	return buf;
}

void sound_caps(void)
{
	DSCAPS caps;
	HRESULT hr;

	bzero(&caps,sizeof(caps));
	caps.dwSize=sizeof(caps);

	hr=ds->lpVtbl->GetCaps(ds,&caps);
	if (hr!=DS_OK) { fail("GetCaps: %s",sound_error(hr)); return; }

	if (caps.dwFlags&DSCAPS_CERTIFIED) addline("DSCAPS_CERTIFIED");
	if (caps.dwFlags&DSCAPS_CONTINUOUSRATE) addline("DSCAPS_CONTINUOUSRATE");
	if (caps.dwFlags&DSCAPS_EMULDRIVER) addline("DSCAPS_EMULDRIVER");
	if (caps.dwFlags&DSCAPS_PRIMARY16BIT) addline("DSCAPS_PRIMARY16BIT");
	if (caps.dwFlags&DSCAPS_PRIMARY8BIT) addline("DSCAPS_PRIMARY8BIT");
	if (caps.dwFlags&DSCAPS_PRIMARYMONO) addline("DSCAPS_PRIMARYMONO");
	if (caps.dwFlags&DSCAPS_PRIMARYSTEREO) addline("DSCAPS_PRIMARYSTEREO");
	if (caps.dwFlags&DSCAPS_SECONDARY16BIT) addline("DSCAPS_SECONDARY16BIT");
	if (caps.dwFlags&DSCAPS_SECONDARY8BIT) addline("DSCAPS_SECONDARY8BIT");
	if (caps.dwFlags&DSCAPS_SECONDARYMONO) addline("DSCAPS_SECONDARYMONO");
	if (caps.dwFlags&DSCAPS_SECONDARYSTEREO) addline("DSCAPS_SECONDARYSTEREO");

	addline("dwMinSecondarySampleRate=%d",caps.dwMinSecondarySampleRate);
	addline("dwMaxSecondarySampleRate=%d",caps.dwMaxSecondarySampleRate);

	addline("dwMaxHwMixingStaticBuffers=%d",caps.dwMaxHwMixingStaticBuffers);
	addline("dwPlayCpuOverheadSwBuffers=%d",caps.dwPlayCpuOverheadSwBuffers);
	addline("dwTotalHwMemBytes=%d",caps.dwTotalHwMemBytes);
}


int init_sound(HWND hwnd)
{
	//LPDIRECTSOUNDBUFFER pb=NULL;
	//PCMWAVEFORMAT pcmwf;
	//DSBUFFERDESC buffdesc;
        HRESULT hr;

	if (!enable_sound) return -1;

	hr=DirectSoundCreate(NULL,&ds,NULL);
	if (hr!=DS_OK) { enable_sound=0; fail("DirectSoundCreate: %s",sound_error(hr)); sound_caps(); return -1; }

	hr=ds->lpVtbl->SetCooperativeLevel(ds,hwnd,DSSCL_EXCLUSIVE);
	if (hr!=DS_OK) { enable_sound=0; fail("SetCooperativeLevel: %s",sound_error(hr)); sound_caps(); return -1; }

	/*memset(&pcmwf,0,sizeof(PCMWAVEFORMAT));
	pcmwf.wf.wFormatTag=WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels=2;
	pcmwf.wf.nSamplesPerSec=44100;
	pcmwf.wf.nBlockAlign=2;
	pcmwf.wf.nAvgBytesPerSec=pcmwf.wf.nSamplesPerSec*pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample=8;

	buffdesc.dwSize=sizeof(DSBUFFERDESC);
	buffdesc.dwFlags=DSBCAPS_PRIMARYBUFFER;	//|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN;
	buffdesc.dwReserved=0;
	buffdesc.dwBufferBytes=0;
	buffdesc.lpwfxFormat=NULL;

	hr=ds->lpVtbl->CreateSoundBuffer(ds,&buffdesc,&pb,NULL);
	if (hr!=DS_OK) { fail("CreateSoundBuffer"); enable_sound=0; return -1; }

	hr=pb->lpVtbl->SetFormat(pb,(LPCWAVEFORMATEX)&pcmwf);
	if (hr!=DS_OK) { fail("SetFormat"); enable_sound=0; return -1; }

	hr=pb->lpVtbl->Play(pb,0,0,DSBPLAY_LOOPING);
	if (hr!=DD_OK) { fail("Play: %d",hr); enable_sound=0; return -1; }  */

        return 0;
}

struct snd_pak
{
        unsigned int start;
	unsigned int size;
	unsigned int freq;
};

#ifdef DEVELOPER
static char *sfx_name[]={
	"..\\sfx\\null.wav",			//0
	"..\\sfx\\sdemonawaken.wav",		//1
        "..\\sfx\\door.wav",			//2
	"..\\sfx\\door2.wav",			//3
	"..\\sfx\\man_dead.wav",		//4
	"..\\sfx\\thunderrumble3.wav",		//5
	"..\\sfx\\explosion.wav",		//6
	"..\\sfx\\hit_body2.wav",		//7
	"..\\sfx\\miss.wav",			//8
	"..\\sfx\\man_hurt.wav",		//9
	"..\\sfx\\pigeon.wav",			//10
	"..\\sfx\\crow.wav",			//11
	"..\\sfx\\crow2.wav",			//12
	"..\\sfx\\laughingman6.wav",		//13
	"..\\sfx\\drip1.wav",			//14
	"..\\sfx\\drip2.wav",			//15
	"..\\sfx\\drip3.wav",			//16
	"..\\sfx\\howl1.wav",			//17
	"..\\sfx\\howl2.wav",			//18
	"..\\sfx\\bird1.wav",			//19
	"..\\sfx\\bird2.wav",			//20
	"..\\sfx\\bird3.wav",			//21
	"..\\sfx\\catmeow2.wav",		//22
	"..\\sfx\\cricket.wav",			//23
	"..\\sfx\\specht.wav",			//24
	"..\\sfx\\haeher.wav",			//25
	"..\\sfx\\owl1.wav",			//26
	"..\\sfx\\owl2.wav",			//27
	"..\\sfx\\owl3.wav",			//28
	"..\\sfx\\magic.wav",			//29
	"..\\sfx\\flash.wav",			//30	lightning strike
	"..\\sfx\\scarynote.wav",		//31	freeze
	"..\\sfx\\woman_hurt.wav",		//32
	"..\\sfx\\woman_dead.wav",		//33
	"..\\sfx\\parry1.wav",			//34
	"..\\sfx\\parry2.wav",			//35
	"..\\sfx\\dungeon_breath1.wav",		//36
	"..\\sfx\\dungeon_breath2.wav",		//37
	"..\\sfx\\pents_mood1.wav",		//38
	"..\\sfx\\pents_mood2.wav",		//39
	"..\\sfx\\pents_mood3.wav",		//40
	"..\\sfx\\ancient_activate.wav",	//41
	"..\\sfx\\pent_activate.wav",		//42
	"..\\sfx\\ancient_runout.wav",		//43

	"..\\sfx\\bubble1.wav",			//44
	"..\\sfx\\bubble2.wav",			//45
	"..\\sfx\\bubble3.wav",			//46
	"..\\sfx\\whale1.wav",			//47
	"..\\sfx\\whale2.wav",			//48
	"..\\sfx\\whale3.wav",			//49
	
	NULL
};

#define SHDRSIZE	46

void analyze_hdr(unsigned char *ptr,int *pfreq,int *plen)
{
	/*note("chunk ID %4.4s",(ptr+0));
	note("chunk size %d",*(unsigned int*)(ptr+4));
	note("chunk format %d",*(unsigned int*)(ptr+8));

	note("sub chunk 1 ID %4.4s",(ptr+12));
	note("sub chunk 1 size %d",*(unsigned int*)(ptr+16));

	note("sub chunk 2 ID %4.4s",(ptr+38));
	note("sub chunk 2 size %d",*(unsigned int*)(ptr+42));*/

	if (plen) *plen=*(unsigned int*)(ptr+42);
	if (pfreq) *pfreq=*(unsigned int*)(ptr+24);
}

void make_sound_pak(void)
{
	struct snd_pak *sp=NULL;
	unsigned char *data=NULL;
	unsigned char hdr[256];
	int maxsp=0,handle,size=0,tmp,n,freq,len;

	while (sfx_name[maxsp]) {
		sp=xrealloc(sp,sizeof(struct snd_pak)*(maxsp+1),MEM_TEMP);
		handle=open(sfx_name[maxsp],O_RDONLY|O_BINARY);
		if (handle==-1) {
			fail("file %s not found",sfx_name[maxsp]);
			exit(1);
		}
		read(handle,hdr,SHDRSIZE); analyze_hdr(hdr,&freq,&len); if (len<0 || len>1024*1024) {
			fail("wav format unrecogniced: len=%d",len);
			exit(1);
		}
                lseek(handle,SHDRSIZE,SEEK_SET);

		data=xrealloc(data,size+len,MEM_TEMP);
		read(handle,data+size,len); close(handle);
		sp[maxsp].start=size; sp[maxsp].size=len; sp[maxsp].freq=freq; size+=len;
		maxsp++;
	}

	for (n=0; n<maxsp; n++) {
		sp[n].start+=sizeof(struct snd_pak)*maxsp;
		note("sp[%d].start=%d, sp[%d].size=%d, sp[%d].freq=%d",n,sp[n].start,n,sp[n].size,n,sp[n].freq);
	}

	handle=open("sound.pak",O_WRONLY|O_BINARY|O_TRUNC|O_CREAT,0666);
	write(handle,sp,sizeof(struct snd_pak)*maxsp);
	write(handle,data,size);
	close(handle);

	note("%d sounds, %d bytes header, %dK content, %dK total",
	     maxsp,maxsp*sizeof(struct snd_pak),size>>10,(size+maxsp*sizeof(struct snd_pak))>>10);
}
#endif

struct sbuf
{
	LPDIRECTSOUNDBUFFER sb;	
	int nr;
	struct sbuf *next;
};

struct sbuf *trash=NULL;
int sbuf_cnt=0;

#define SILLY_FREQ	22050

static int orig_freq,orig_size;
int silly_sound=0;

static void convert_pak(struct snd_pak *sp)
{
	orig_freq=sp->freq;
	orig_size=sp->size;

	sp->size=(double)sp->size*SILLY_FREQ/sp->freq-ceil((double)SILLY_FREQ/sp->freq);
	sp->freq=SILLY_FREQ;
}

/*static void expensive_convert_data(int handle,unsigned char *dst,int size)
{
	int n,val;
	unsigned char *src;
	double pos,high,low;

	src=xmalloc(orig_size,MEM_TEMP);
	read(handle,src,orig_size);

	for (n=0; n<size; n++) {
		pos=(double)n*orig_freq/SILLY_FREQ;
		
		high=pos-floor(pos);
		low=1-high;
		
		dst[n]=src[ceil(pos)]*high+src[floor(pos)]*low;
		//if (n%1000==0) note("pos=%.2f high=%.2f low=%.2f src_high=%d, src_low=%d, res=%d",pos,high,low,src[ceil(pos)],src[floor(pos)],dst[n]);
	}

	free(src);
}*/

static void convert_data(int handle,unsigned char *dst,int size)
{
	int n;
	unsigned char *src;

	src=xmalloc(orig_size,MEM_TEMP);
	read(handle,src,orig_size);

	for (n=0; n<size; n++) {
                dst[n]=src[n*orig_freq/SILLY_FREQ];		
	}

	free(src);
}

void play_pak_sound(int nr,int vol,int p)
{
	LPDIRECTSOUNDBUFFER sb;
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC buffdesc;
        HRESULT hr;
	int handle;
	struct snd_pak sp;
	void *ptr1,*ptr2;
	unsigned long size1,size2;
	struct sbuf *sbuf;

	if (!enable_sound) return;
	if (sbuf_cnt>15) return;

	// force volume and pan to sane values
	if (vol>0) vol=0;
	if (vol<-9999) vol=-9999;

	if (p>9999) p=9999;
	if (p<-9999) p=-9999;

	// read size and frequency
	handle=open("sound.pak",O_RDONLY|O_BINARY);
	lseek(handle,nr*sizeof(sp),SEEK_SET);
	read(handle,&sp,sizeof(sp));

	if (silly_sound) convert_pak(&sp);

	// create wave header with frequency from pak
	memset(&pcmwf,0,sizeof(PCMWAVEFORMAT));
	pcmwf.wf.wFormatTag=WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels=1;
	pcmwf.wf.nSamplesPerSec=sp.freq;
	pcmwf.wf.nBlockAlign=1;
	pcmwf.wf.nAvgBytesPerSec=pcmwf.wf.nSamplesPerSec*pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample=8;

	// create buffer description with size from pak
	memset(&buffdesc,0,sizeof(buffdesc));
	buffdesc.dwSize=sizeof(buffdesc);
	buffdesc.dwFlags=DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME;	//|DSBCAPS_GLOBALFOCUS;
	buffdesc.dwBufferBytes=sp.size;
	buffdesc.lpwfxFormat=(LPWAVEFORMATEX)&pcmwf;

	// create sound buffer with freq/size
        hr=ds->lpVtbl->CreateSoundBuffer(ds,&buffdesc,&sb,NULL);
	if (hr!=DS_OK) { fail("CreateSoundBuffer: %s",sound_error(hr)); sound_caps(); close(handle); enable_sound=0; return; }	

	// get write pointer to sound buffer
	hr=sb->lpVtbl->Lock(sb,0,sp.size,&ptr1,&size1,&ptr2,&size2,0);
	if (hr!=DD_OK) { fail("Lock: %s",sound_error(hr)); sound_caps(); close(handle); enable_sound=0; return; }

	// fill sound buffer with data
	lseek(handle,sp.start,SEEK_SET);
	if (silly_sound) {
		convert_data(handle,ptr1,size1);
	} else {			
		read(handle,ptr1,size1);		
	}
	close(handle);

	//note("playing sample %d, size %d, freq %d",nr,sp.size,sp.freq);

	// and play it
	hr=sb->lpVtbl->Play(sb,0,0,0);
	if (hr!=DD_OK) { fail("Play: %s",sound_error(hr)); sound_caps(); enable_sound=0; return; }
	hr=sb->lpVtbl->SetVolume(sb,vol);
	if (hr!=DD_OK) { fail("SetVolume: %s",sound_error(hr)); sound_caps(); enable_sound=0; return; }
	hr=sb->lpVtbl->SetPan(sb,p);
	if (hr!=DD_OK) { fail("SetPan: %s",sound_error(hr)); sound_caps(); enable_sound=0; return; }

	// add sound buffer to list, to allow later deletion
	sbuf=xcalloc(sizeof(struct sbuf),MEM_TEMP);
        sbuf->sb=sb;
	sbuf->nr=nr;
	
	sbuf->next=trash;
        trash=sbuf;
	sbuf_cnt++;

	//note("req to play sound %d (%d,%d)",nr,vol,p);
	if (sbuf_cnt>10) note("sbuf_cnt=%d",sbuf_cnt);	
}

// sound garbage collection: remove all sound buffer which have finished playing
void sound_gc(void)
{
	int panic=0;
	struct sbuf *sbuf,*prev,*next;
	DWORD status;
	HRESULT hr;

	if (!enable_sound) return;

	for (sbuf=trash,prev=NULL; sbuf && panic++<99; sbuf=next) {
		next=sbuf->next;
                hr=sbuf->sb->lpVtbl->GetStatus(sbuf->sb,&status);
		if (hr!=DS_OK || (status&DSBSTATUS_PLAYING)==0) {
			sbuf->sb->lpVtbl->Release(sbuf->sb);
			if (prev) prev->next=sbuf->next;
			else trash=sbuf->next;
			xfree(sbuf);
			sbuf_cnt--;
			//note("released buffer %p (%d)",sbuf,sbuf_cnt);
		} else {
			//note("not released buffer %p (%d)",sbuf,sbuf_cnt);
			prev=sbuf;
		}
	}
}

void sound_display(void)
{
	int panic=0,y=70;
	struct sbuf *sbuf;

	if (!enable_sound) return;

	for (sbuf=trash; sbuf && panic++<99; sbuf=sbuf->next) {
		dd_drawtext_fmt(5,y,0x7fff,DD_SMALL|DD_FRAME,"snd: %3d",sbuf->nr); y+=10;		
	}
}

/*void play_pak_sound_trans(int nr,int vol,int p,int trans)
{
	LPDIRECTSOUNDBUFFER sb;
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC buffdesc;
        HRESULT hr;
	int handle;
	struct snd_pak sp;
	void *ptr1,*ptr2;
	unsigned long size1,size2;
	struct sbuf *sbuf;

	if (!enable_sound) return;
	if (sbuf_cnt>15) return;

	// force volume and pan to sane values
	if (vol>0) vol=0;
	if (vol<-9999) vol=-9999;

	if (p>9999) p=9999;
	if (p<-9999) p=-9999;

	// read size and frequency
	handle=open("sound.pak",O_RDONLY|O_BINARY);
	lseek(handle,nr*sizeof(sp),SEEK_SET);
	read(handle,&sp,sizeof(sp));

	// create wave header with frequency from pak
	memset(&pcmwf,0,sizeof(PCMWAVEFORMAT));
	pcmwf.wf.wFormatTag=WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels=1;
	pcmwf.wf.nSamplesPerSec=(sp.freq*trans)/1000;
	pcmwf.wf.nBlockAlign=1;
	pcmwf.wf.nAvgBytesPerSec=pcmwf.wf.nSamplesPerSec*pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample=8;

	// create buffer description with size from pak
	buffdesc.dwSize=sizeof(DSBUFFERDESC);
	buffdesc.dwFlags=DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|DSBCAPS_GLOBALFOCUS;
	buffdesc.dwReserved=0;
	buffdesc.dwBufferBytes=sp.size;
	buffdesc.lpwfxFormat=(LPWAVEFORMATEX)&pcmwf;

	// create sound buffer with freq/size
        hr=ds->lpVtbl->CreateSoundBuffer(ds,&buffdesc,&sb,NULL);
	if (hr!=DS_OK) { fail("CreateSoundBuffer 3"); close(handle); return; }	

	// get write pointer to sound buffer
	hr=sb->lpVtbl->Lock(sb,0,sp.size,&ptr1,&size1,&ptr2,&size2,0);
	if (hr!=DD_OK) { fail("Lock"); return; }

	// fill sound buffer with data
	lseek(handle,sp.start,SEEK_SET);
	read(handle,ptr1,size1);
	close(handle);

	// and play it
	hr=sb->lpVtbl->Play(sb,0,0,0);
	if (hr!=DD_OK) { fail("Play"); return; }
	hr=sb->lpVtbl->SetVolume(sb,vol);
	if (hr!=DD_OK) { fail("SetVolume"); return; }
	hr=sb->lpVtbl->SetPan(sb,p);
	if (hr!=DD_OK) { fail("SetPan"); return; }

	// add sound buffer to list, to allow later deletion
	sbuf=xcalloc(sizeof(struct sbuf),MEM_TEMP);
        sbuf->sb=sb;
	
	sbuf->next=trash;
        trash=sbuf;
	sbuf_cnt++;

	//note("req to play sound %d (%d,%d)",nr,vol,p);
	if (sbuf_cnt>10) note("sbuf_cnt=%d",sbuf_cnt);	
}*/

#define MOODNEXT	(1000/4)

void sound_mood(void)
{
        static int next=0,step=0;
	int diff;

	if (!step) next=GetTickCount();
	diff=GetTickCount()-next;
	if (diff<0) return;
	if (diff>40) {
		//note("sound slow: %d",diff);
		next=GetTickCount();
	}
	
        /*switch(step%32) {
		case 0:		play_pak_sound_trans(30,400,0,1000); break;
		case 2:		play_pak_sound_trans(30,400,0,1000); break;
		case 4:		play_pak_sound_trans(30,400,0,1000); break;
		case 6:		play_pak_sound_trans(30,400,0,1000); break;
		case 7:		play_pak_sound_trans(30,400,0,1000-83); break;
		case 8:		play_pak_sound_trans(30,400,0,1000); break;
		case 10:	play_pak_sound_trans(30,400,0,1000); break;
		case 12:	play_pak_sound_trans(30,400,0,1000); break;
		case 14:	play_pak_sound_trans(30,400,0,1000); break;
		case 15:	play_pak_sound_trans(30,400,0,1000-83); break;
		case 16:	play_pak_sound_trans(30,400,0,1000); break;
		case 18:	play_pak_sound_trans(30,400,0,1000); break;
		case 20:	play_pak_sound_trans(30,400,0,1000); break;
		case 22:	play_pak_sound_trans(30,400,0,1000); break;
		case 23:	play_pak_sound_trans(30,400,0,1000-83); break;
		case 24:	play_pak_sound_trans(30,400,0,1000-83-76); break;
		case 26:	play_pak_sound_trans(30,400,0,1000-83-76); break;
		case 28:	play_pak_sound_trans(30,400,0,1000-83-76); break;
		case 30:	play_pak_sound_trans(30,400,0,1000-83-76); break;
		case 31:	play_pak_sound_trans(30,400,0,1000-83); break;
	}*/

	next+=MOODNEXT;
	step++;
}
