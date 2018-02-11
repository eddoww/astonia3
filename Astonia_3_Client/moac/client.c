#include <winsock2.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#pragma hdrstop

#define ISCLIENT
#include "../lib/zlib.h"
#include "main.h"
#include "client.h"
#include "sound.h"

int display_gfx=0,display_time=0;

void bzero_client(int part);

int rec_bytes=0;
int sent_bytes=0;

// extern

extern int update_skltab;

// socket

int sock=-1;            // -1 is important -
int sockstate=0;
unsigned int socktime=0;
int socktimeout=0;
int change_area=0;
int kicked_out=0;
int target_port=5556;
int backup_server=0;
int developer_server=0;
unsigned int unique=0;
unsigned int usum=0;

unsigned int sock_server=0; //(192<<24)|(168<<16)|(42<<8)|(22<<0);
unsigned short sock_port=1080;
char sock_user[80]="astonia";
int sock_done=0;

#ifdef MR
int target_server=(192<<24)|(168<<16)|(0<<8)|(21<<0);
int update_server=(192<<24)|(168<<16)|(0<<8)|(21<<0);
int base_server=(192<<24)|(168<<16)|(0<<8)|(21<<0);
#endif
#ifdef DB
#define tmp (91<<24)+(250<<16)+(112<<8)+37;
int target_server=tmp;
int update_server=tmp;
int base_server=tmp;
#undef tmp
#endif
#ifdef US
int target_server=(212<<24)|(202<<16)|(240<<8)|(67<<0);
int update_server=(212<<24)|(202<<16)|(240<<8)|(66<<0);
int base_server=(212<<24)|(202<<16)|(240<<8)|(67<<0);
#endif
char username[40];
char password[16];
int zsinit;
struct z_stream_s zs;

#define Q_SIZE        16
struct queue
{
        unsigned char buf[16384];
        int size;
};

// ------------------------- client start --------------------------------

int tick;
int mirror=0,newmirror=0;
int lasttick;           // ticks in inbuf
int lastticksize;       // size inbuf must reach to get the last tick complete in the queue
int realtime;

struct queue queue[Q_SIZE];
int q_in,q_out,q_size;

int server_cycles;

int ticksize;
int inused;
int indone;
int login_done;
unsigned char inbuf[MAX_INBUF];

int outused;
unsigned char outbuf[MAX_OUTBUF];

int act;
int actx;
int acty;

unsigned int cflags;                // current item flags
unsigned int csprite;                // and sprite

int originx;
int originy;
struct map map[MAPDX*MAPDY];
struct map map2[MAPDX*MAPDY];

int value[2][V_MAX];
int item[INVENTORYSIZE];
int item_flags[INVENTORYSIZE];
int hp;
int mana;
int rage;
int endurance;
int lifeshield;
int experience;
int experience_used;
int mil_exp;
int gold;

struct player player[MAXCHARS];
//char name[MAXCHARS][40];

union ceffect ceffect[MAXEF];
unsigned char ueffect[MAXEF];

int con_type;
char con_name[80];
int con_cnt;
int container[CONTAINERSIZE];
int price[CONTAINERSIZE];
int itemprice[CONTAINERSIZE];
int cprice;

int lookinv[12];
int looksprite,lookc1,lookc2,lookc3;
char look_name[80];
char look_desc[1024];

char pent_str[7][80];

int pspeed=0;   // 0=normal   1=fast      2=stealth     - like the server
int pcombat=0;  // 0=balanced 1=offensive 2=defensive   - i dunno

int may_teleport[64+32];

// ------------------------- client end --------------------------------

// end of struct client

int sv_map01(unsigned char *buf, int *last,struct map *cmap)
{
        int p,c;

        if ((buf[0]&(16+32))==SV_MAPTHIS) {
                p=1;
                c=*last;
        } else if ((buf[0]&(16+32))==SV_MAPNEXT) {
                p=1;
                c=*last+1;

        } else if ((buf[0]&(16+32))==SV_MAPOFF) {
                p=2;
                c=*last+*(unsigned char*)(buf+1);
        } else {
                p=3;
                c=*(unsigned short*)(buf+1);
        }

        // printf("MAP01, last=%d, c=%d [%X]\n",last,c,buf[0]);

        if (c>MAPDX*MAPDY || c<0) { printf("sv_map01 illegal call with c=%d\n",c); fflush(stdout); exit(-1); }
        
        if (buf[0]&1) {
                cmap[c].ef[0]=*(unsigned int*)(buf+p); p+=4;
                //addline("map effect %d",map[c].ef[0]);
        }
        if (buf[0]&2) {
                cmap[c].ef[1]=*(unsigned int*)(buf+p); p+=4;
                //addline("map effect %d",map[c].ef[1]);
        }
        if (buf[0]&4) {
                cmap[c].ef[2]=*(unsigned int*)(buf+p); p+=4;
                //addline("map effect %d",map[c].ef[2]);
        }
        if (buf[0]&8) {
                cmap[c].ef[3]=*(unsigned int*)(buf+p); p+=4;
                //addline("map effect %d",map[c].ef[3]);
        }

        *last=c;

        return p;
}

int sv_map10(unsigned char *buf, int *last,struct map *cmap)
{
        int p,c;

        if ((buf[0]&(16+32))==SV_MAPTHIS) {
                p=1;
                c=*last;
        } else if ((buf[0]&(16+32))==SV_MAPNEXT) {
                p=1;
                c=*last+1;
        } else if ((buf[0]&(16+32))==SV_MAPOFF) {
                p=2;
                c=*last+*(unsigned char*)(buf+1);
        } else {
                p=3;
                c=*(unsigned short*)(buf+1);
        }

        // printf("MAP10, last=%d, c=%d [%X]\n",last,c,buf[0]);

        if (c>MAPDX*MAPDY || c<0) { printf("sv_map10 illegal call with c=%d\n",c); fflush(stdout); exit(-1); }

        if (buf[0]&1) {
                cmap[c].csprite=*(unsigned int*)(buf+p); p+=4;
                cmap[c].cn=*(unsigned short*)(buf+p); p+=2;
                //cmap[c].flags=*(unsigned char*)(buf+p); p+=1;
        }
        if (buf[0]&2) {
                cmap[c].action=*(unsigned char*)(buf+p); p++;
                cmap[c].duration=*(unsigned char*)(buf+p); p++;
                cmap[c].step=*(unsigned char*)(buf+p); p++;
        }
        if (buf[0]&4) {
                cmap[c].dir=*(unsigned char*)(buf+p); p++;
                cmap[c].health=*(unsigned char*)(buf+p); p++;
                cmap[c].mana=*(unsigned char*)(buf+p); p++;
                cmap[c].shield=*(unsigned char*)(buf+p); p++;
        }
        if (buf[0]&8) {
                cmap[c].csprite=0;
                cmap[c].cn=0;
                cmap[c].action=0;
                cmap[c].duration=0;
                cmap[c].step=0;
                cmap[c].dir=0;
                cmap[c].health=0;
        }

        *last=c;

        return p;
}

int sv_map11(unsigned char *buf, int *last,struct map *cmap)
{
        int p,c;
        int tmp32;

        if ((buf[0]&(16+32))==SV_MAPTHIS) {
                p=1;
                c=*last;
        } else if ((buf[0]&(16+32))==SV_MAPNEXT) {
                p=1;
                c=*last+1;
        } else if ((buf[0]&(16+32))==SV_MAPOFF) {
                p=2;
                c=*last+*(unsigned char*)(buf+1);
        } else {
                p=3;
                c=*(unsigned short*)(buf+1);
        }

        //if (cmap==map2) printf("MAP11, last=%d, c=%d [%X]\n",*last,c,buf[0]);

        if (c>MAPDX*MAPDY || c<0) { printf("sv_map11 illegal call with c=%d\n",c); fflush(stdout); exit(-1); }

        if (buf[0]&1) {
                tmp32=*(unsigned int*)(buf+p); p+=4;
                cmap[c].gsprite=(unsigned short int)(tmp32&0x0000FFFF);
                cmap[c].gsprite2=(unsigned short int)(tmp32>>16);
        }
        if (buf[0]&2) {
                tmp32=*(unsigned int*)(buf+p); p+=4;
                cmap[c].fsprite=(unsigned short int)(tmp32&0x0000FFFF);
                cmap[c].fsprite2=(unsigned short int)(tmp32>>16);
        }
        if (buf[0]&4) {
                cmap[c].isprite=*(unsigned int*)(buf+p); p+=4;
                //printf("got isprite %d\n",cmap[c].isprite);
                if (cmap[c].isprite&0x80000000) {
                        cmap[c].isprite&=~0x80000000;
                        cmap[c].ic1=*(unsigned short*)(buf+p); p+=2;
                        cmap[c].ic2=*(unsigned short*)(buf+p); p+=2;
                        cmap[c].ic3=*(unsigned short*)(buf+p); p+=2;
                } else {
                        cmap[c].ic1=0;
                        cmap[c].ic2=0;
                        cmap[c].ic3=0;
                }
        }
        if (buf[0]&8) {
                if (*(unsigned char*)(buf+p)) {
                        cmap[c].flags=*(unsigned short*)(buf+p); p+=2;
                } else {
                        cmap[c].flags=*(unsigned char*)(buf+p); p++;
                }
        }

        //if (cmap==map2) printf("light=%d,sprite=%d",cmap[c].flags&CMF_LIGHT,cmap[c].gsprite);

        *last=c;

        return p;
}

int svl_ping(char *buf)
{
        int t,diff;

        t=*(unsigned int *)(buf+1);
        diff=GetTickCount()-t;
        addline("RTT1: %.2fms",diff/1000.0);
        
        return 5;
}

int sv_ping(char *buf)
{
        int t,diff;

        t=*(unsigned int *)(buf+1);
        diff=GetTickCount()-t;
        addline("RTT2: %.2fms",diff/1000.0);
        
        return 5;
}


void sv_scroll_right(struct map *cmap)
{
        memmove(cmap,cmap+1,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-1));
}

void sv_scroll_left(struct map *cmap)
{
        memmove(cmap+1,cmap,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-1));
}

void sv_scroll_down(struct map *cmap)
{
        memmove(cmap,cmap+(DIST*2+1),sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)));
}

void sv_scroll_up(struct map *cmap)
{
        memmove(cmap+(DIST*2+1),cmap,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)));
}

void sv_scroll_leftup(struct map *cmap)
{
        memmove(cmap+(DIST*2+1)+1,cmap,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)-1));
}

void sv_scroll_leftdown(struct map *cmap)
{
        memmove(cmap,cmap+(DIST*2+1)-1,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)+1));
}

void sv_scroll_rightup(struct map *cmap)
{
        memmove(cmap+(DIST*2+1)-1,cmap,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)+1));
}

void sv_scroll_rightdown(struct map *cmap)
{
        memmove(cmap,cmap+(DIST*2+1)+1,sizeof(struct map)*((DIST*2+1)*(DIST*2+1)-(DIST*2+1)-1));
}

void sv_setval(unsigned char *buf,int nr)
{
        int n;

        n=buf[1];
        if (n<0 || n>=V_MAX) return;

        if (nr==0 && n==V_PROFESSION) ;
        else value[nr][n]=*(short*)(buf+2);

        update_skltab=1;
}

void sv_sethp(unsigned char *buf)
{
        hp=*(short*)(buf+1);
}

void sv_endurance(unsigned char *buf)
{
        endurance=*(short*)(buf+1);
}

void sv_lifeshield(unsigned char *buf)
{
        lifeshield=*(short*)(buf+1);
}

void sv_setmana(unsigned char *buf)
{
        mana=*(short*)(buf+1);
}

void sv_setrage(unsigned char *buf)
{
        rage=*(short*)(buf+1);
}

void sv_setitem(unsigned char *buf)
{
        int n;

        n=buf[1];
        if (n<0 || n>=INVENTORYSIZE) return;

        item[n]=*(unsigned int*)(buf+2);
        item_flags[n]=*(unsigned int*)(buf+6);
}

void sv_setorigin(unsigned char *buf)
{
        originx=*(unsigned short int *)(buf+1);
        originy=*(unsigned short int *)(buf+3);
}

void sv_settick(unsigned char *buf)
{
        tick=*(unsigned int *)(buf+1);

        //note("settick=%d",tick);
}

void sv_mirror(unsigned char *buf)
{
        mirror=newmirror=*(unsigned int *)(buf+1);

        //note("mirror=%d",mirror);
}

void sv_realtime(unsigned char *buf)
{
        realtime=*(unsigned int *)(buf+1);
}

void sv_speedmode(unsigned char *buf)
{
        pspeed=buf[1];
}

void sv_fightmode(unsigned char *buf)
{
        pcombat=buf[1];
}

void sv_setcitem(unsigned char *buf)
{
        int n;

        csprite=*(unsigned int *)(buf+1);
        cflags=*(unsigned int *)(buf+5);
}

void sv_act(unsigned char *buf)
{
        int n;
        extern int teleporter;

        act=*(unsigned short int *)(buf+1);
        actx=*(unsigned short int *)(buf+3);
        acty=*(unsigned short int *)(buf+5);

        if (act) teleporter=0;
}

int sv_text(unsigned char *buf)
{
        int len;
        char line[1024];
        extern char tutor_text[];
        extern int show_tutor;

        len=*(unsigned short*)(buf+1);
        if (len<1000) {
                memcpy(line,buf+3,len);
                line[len]=0;
                if (line[0]=='#') {
                        if (!isdigit(line[1])) {
                                strcpy(tutor_text,line+1);
                                show_tutor=1;
                        } else if (line[1]=='1') {
                                strcpy(look_name,line+2);
                        } else if (line[1]=='2') {
                                strcpy(look_desc,line+2);
                        } else if (line[1]=='3') {
                                strcpy(pent_str[0],line+2);
                                pent_str[1][0]=pent_str[2][0]=pent_str[3][0]=pent_str[4][0]=pent_str[5][0]=pent_str[6][0]=0;
                        } else if (line[1]=='4') {
                                strcpy(pent_str[1],line+2);
                                //pent_str[2][0]=pent_str[3][0]=pent_str[4][0]=pent_str[5][0]=pent_str[6][0]=0;
                        } else if (line[1]=='5') {
                                strcpy(pent_str[2],line+2);
                                //pent_str[3][0]=pent_str[4][0]=pent_str[5][0]=pent_str[6][0]=0;
                        } else if (line[1]=='6') {
                                strcpy(pent_str[3],line+2);
                                //pent_str[4][0]=pent_str[5][0]=pent_str[6][0]=0;
                        } else if (line[1]=='7') {
                                strcpy(pent_str[4],line+2);
                                //pent_str[5][0]=pent_str[6][0]=0;
                        } else if (line[1]=='8') {
                                strcpy(pent_str[5],line+2);
                                //pent_str[6][0]=0;
                        } else if (line[1]=='9') {
                                strcpy(pent_str[6],line+2);
                        }
                } else addline("%s",line);
        }

        return len+3;
}

int svl_text(unsigned char *buf)
{
        int len;

        len=*(unsigned short*)(buf+1);
        return len+3;
}

int sv_conname(unsigned char *buf)
{
        int len;

        len=*(unsigned char*)(buf+1);
        if (len<80) {
                memcpy(con_name,buf+2,len);
                con_name[len]=0;
        }

        return len+2;
}

int svl_conname(unsigned char *buf)
{
        int len;

        len=*(unsigned char*)(buf+1);

        return len+2;
}

int sv_exit(unsigned char *buf)
{
        int len;
        char line[1024];

        len=*(unsigned char*)(buf+1);
        if (len<=200) {
                memcpy(line,buf+2,len);
                line[len]=0;
                addline("Server demands exit: %s",line);
        }
        kicked_out=1;

        return len+2;
}

int svl_exit(unsigned char *buf)
{
        int len;

        len=*(unsigned char*)(buf+1);
        return len+2;
}

int sv_name(unsigned char *buf)
{
        int len,cn;

        len=buf[12];
        cn=*(unsigned short*)(buf+1);

        if (cn<1 || cn>=MAXCHARS) addline("illegal cn %d in sv_name",cn);
        else {
                memcpy(player[cn].name,buf+13,len);
                player[cn].name[len]=0;

                player[cn].level=*(unsigned char*)(buf+3);
                player[cn].c1=*(unsigned short*)(buf+4);
                player[cn].c2=*(unsigned short*)(buf+6);
                player[cn].c3=*(unsigned short*)(buf+8);
                player[cn].clan=*(unsigned char*)(buf+10);
                player[cn].pk_status=*(unsigned char*)(buf+11);
        }

        return len+13;
}

int svl_name(unsigned char *buf)
{
        int len;

        len=buf[12];
        
        return len+13;
}

int find_ceffect(int fn)
{
        int n;

        for (n=0; n<MAXEF; n++) {
                if (ueffect[n] && ceffect[n].generic.nr==fn) {
                        return n;
                }
        }
        return -1;
}

int is_char_ceffect(int type)
{
        switch(type) {
                case 1:         return 1;
                case 2:         return 0;
                case 3:         return 1;
                case 4:         return 0;
                case 5:         return 1;
                case 6:         return 0;
                case 7:         return 0;
                case 8:         return 1;
                case 9:         return 1;
                case 10:        return 1;
                case 11:        return 1;
                case 12:        return 1;
                case 13:        return 0;
                case 14:        return 1;
                case 15:        return 0;
                case 16:        return 0;
                case 17:        return 0;
                case 22:        return 1;
                case 23:        return 1;

        }
        return 0;
}

int find_cn_ceffect(int cn,int skip)
{
        int n;

        for (n=0; n<MAXEF; n++) {
                if (ueffect[n] && is_char_ceffect(ceffect[n].generic.type) && ceffect[n].flash.cn==cn) {
                        if (skip) { skip--; continue; }
                        return n;
                }
        }
        return -1;
}

int sv_ceffect(unsigned char *buf)
{
        int nr,type,len=0; //,fn,arg;

        nr=buf[1];
        type=((struct cef_generic *)(buf+2))->type;

        //fn=((struct cef_generic *)(buf+2))->nr;        
        //arg=((struct cef_explode *)(buf+2))->age;        

        switch(type) {
                case 1:                len=sizeof(struct cef_shield); break;
                case 2:                len=sizeof(struct cef_ball); break;
                case 3:                len=sizeof(struct cef_strike); break;
                case 4:                len=sizeof(struct cef_fireball); break;
                case 5:                len=sizeof(struct cef_flash); break;

                case 7:                len=sizeof(struct cef_explode); break;
                case 8:                len=sizeof(struct cef_warcry); break;
                case 9:                len=sizeof(struct cef_bless); break;
                case 10:        len=sizeof(struct cef_heal); break;
                case 11:        len=sizeof(struct cef_freeze); break;
                case 12:        len=sizeof(struct cef_burn); break;
                case 13:        len=sizeof(struct cef_mist); break;
                case 14:        len=sizeof(struct cef_potion); break;
                case 15:        len=sizeof(struct cef_earthrain); break;
                case 16:        len=sizeof(struct cef_earthmud); break;
                case 17:        len=sizeof(struct cef_edemonball); break;
                case 18:        len=sizeof(struct cef_curse); break;
                case 19:        len=sizeof(struct cef_cap); break;
                case 20:        len=sizeof(struct cef_lag); break;
                case 21:        len=sizeof(struct cef_pulse); break;
                case 22:        len=sizeof(struct cef_pulseback); break;
                case 23:        len=sizeof(struct cef_firering); break;
                case 24:        len=sizeof(struct cef_bubble); break;


                default:        note("unknown effect %d",type); break;
        }

        if (nr<0 || nr>=MAXEF) { printf("sv_ceffect: invalid nr %d\n",nr); fflush(stdout); exit(-1); }

        memcpy(ceffect+nr,buf+2,len);

        //if (type==24) addline("slot %d: ceffect type %d (%d)",nr,type,ceffect[nr].bubble.yoff);

        return len+2;
}

void sv_ueffect(unsigned char *buf)
{
        int n,i,b;

        for (n=0; n<MAXEF; n++) {
                i=n/8;
                b=1<<(n&7);
                if (buf[i+1]&b) ueffect[n]=1;
                else ueffect[n]=0;                
        }
        //addline("used: %d %d %d %d",ueffect[0],ueffect[1],ueffect[2],ueffect[3]);
}

int svl_ceffect(unsigned char *buf)
{
        int nr,type,len=0;

        nr=buf[1];
        type=((struct cef_generic *)(buf+2))->type;

        switch(type) {
                case 1:                len=sizeof(struct cef_shield); break;
                case 2:                len=sizeof(struct cef_ball); break;
                case 3:                len=sizeof(struct cef_strike); break;
                case 4:                len=sizeof(struct cef_fireball); break;
                case 5:                len=sizeof(struct cef_flash); break;

                case 7:                len=sizeof(struct cef_explode); break;
                case 8:                len=sizeof(struct cef_warcry); break;
                case 9:                len=sizeof(struct cef_bless); break;
                case 10:        len=sizeof(struct cef_heal); break;
                case 11:        len=sizeof(struct cef_freeze); break;
                case 12:        len=sizeof(struct cef_burn); break;
                case 13:        len=sizeof(struct cef_mist); break;
                case 14:        len=sizeof(struct cef_potion); break;
                case 15:        len=sizeof(struct cef_earthrain); break;
                case 16:        len=sizeof(struct cef_earthmud); break;
                case 17:        len=sizeof(struct cef_edemonball); break;
                case 18:        len=sizeof(struct cef_curse); break;
                case 19:        len=sizeof(struct cef_cap); break;
                case 20:        len=sizeof(struct cef_lag); break;
                case 21:        len=sizeof(struct cef_pulse); break;
                case 22:        len=sizeof(struct cef_pulseback); break;
                case 23:        len=sizeof(struct cef_firering); break;
                case 24:        len=sizeof(struct cef_bubble); break;


                default:        note("unknown effect %d",type); break;

        }

        if (nr<0 || nr>=MAXEF) { printf("svl_ceffect: invalid nr %d\n",nr); fflush(stdout); exit(-1); }

        return len+2;
}

void sv_container(unsigned char *buf)
{
        int nr;

        nr=buf[1];
        if (nr<0 || nr>=CONTAINERSIZE) { printf("illegal nr %d in sv_container!",nr);  fflush(stdout); exit(-1); }
        
        container[nr]=*(unsigned int*)(buf+2);        
}

void sv_price(unsigned char *buf)
{
        int nr;

        nr=buf[1];
        if (nr<0 || nr>=CONTAINERSIZE) { printf("illegal nr %d in sv_price!",nr);  fflush(stdout); exit(-1); }
        
        price[nr]=*(unsigned int*)(buf+2);        
}

void sv_itemprice(unsigned char *buf)
{
        int nr;

        nr=buf[1];
        if (nr<0 || nr>=CONTAINERSIZE) { printf("illegal nr %d in sv_itemprice!",nr);  fflush(stdout); exit(-1); }
        
        itemprice[nr]=*(unsigned int*)(buf+2);        
}

void sv_cprice(unsigned char *buf)
{
        cprice=*(unsigned int*)(buf+1);        
}

void sv_gold(unsigned char *buf)
{
        gold=*(unsigned int*)(buf+1);
}

void sv_concnt(unsigned char *buf)
{
        int nr;

        nr=buf[1];
        if (nr<0 || nr>CONTAINERSIZE) { printf("illegal nr %d in sv_contcnt!",nr);  fflush(stdout); exit(-1); }

        con_cnt=nr;
}

void sv_contype(unsigned char *buf)
{
        con_type=buf[1];
}

void sv_exp(unsigned char *buf)
{
        experience=*(unsigned long*)(buf+1);        
        update_skltab=1;
}

void sv_exp_used(unsigned char *buf)
{
        experience_used=*(unsigned long*)(buf+1);
        update_skltab=1;
}

void sv_mil_exp(unsigned char *buf)
{
        mil_exp=*(unsigned long*)(buf+1);
}

void sv_cycles(unsigned char *buf)
{
        extern int server_cycles;
        int c;

        c=*(unsigned long*)(buf+1);

        server_cycles=server_cycles*0.99+c*0.01;
}

void sv_lookinv(unsigned char *buf)
{
        int n;
        extern int show_look;

        looksprite=*(unsigned int*)(buf+1);
        lookc1=*(unsigned int*)(buf+5);
        lookc2=*(unsigned int*)(buf+9);
        lookc3=*(unsigned int*)(buf+13);
        for (n=0; n<12; n++) {
                lookinv[n]=*(unsigned int*)(buf+17+n*4);
        }
        show_look=1;
}

void sv_server_old(unsigned char *buf)
{
        //note("got change server");
        change_area=1;
        if (!developer_server) target_server=*(unsigned int*)(buf+1);
        target_port=*(unsigned short*)(buf+5);

        if (backup_server==0) {
                switch(target_server) {
                        case (212<<24)|(202<<16)|(240<<8)|(67<<0):        target_server=(195<<24)|(90<<16)|(31<<8)|(34<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(68<<0):        target_server=(195<<24)|(90<<16)|(31<<8)|(35<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(69<<0):        target_server=(195<<24)|(90<<16)|(31<<8)|(36<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(70<<0):        target_server=(195<<24)|(90<<16)|(31<<8)|(37<<0); break;
                }
        } else if (backup_server==1) {
                switch(target_server) {
                        case (212<<24)|(202<<16)|(240<<8)|(67<<0):        target_server=(195<<24)|(50<<16)|(166<<8)|(1<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(68<<0):        target_server=(195<<24)|(50<<16)|(166<<8)|(2<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(69<<0):        target_server=(195<<24)|(50<<16)|(166<<8)|(3<<0); break;
                        case (212<<24)|(202<<16)|(240<<8)|(70<<0):        target_server=(195<<24)|(50<<16)|(166<<8)|(4<<0); break;
                }
        }
}

void sv_server(unsigned char *buf)
{
        //note("got change server");
        change_area=1;
        target_port=*(unsigned short*)(buf+5);

#ifndef DEVELOPER
        if (!developer_server) target_server=base_server+((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0)));
        else target_server=base_server;
#endif
        note("serverID=%d, would translate to: %u.%u.%u.%u",
             ((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0))),
             ((base_server+((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0))))>>24)&255,
             ((base_server+((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0))))>>16)&255,
             ((base_server+((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0))))>>8)&255,
             ((base_server+((*(unsigned int*)(buf+1))-((212<<24)|(202<<16)|(240<<8)|(67<<0))))>>0)&255);
}

void sv_logindone(void)
{
        login_done=1;
        bzero_client(1);
        //note("got login done");
}

void sv_special(unsigned char *buf)
{
        unsigned int type,opt1,opt2;

        type=*(unsigned int*)(buf+1);
        opt1=*(unsigned int*)(buf+5);
        opt2=*(unsigned int*)(buf+9);

        //note("get special %d %d",type,opt1,opt2);
        switch(type) {
                case 0:                display_gfx=opt1; display_time=tick; break;
#ifdef DOSOUND
                default:        if (type>0 && type<1000) play_pak_sound(type,opt1,opt2);
                                break;
#endif
        }
}

void sv_teleport(unsigned char *buf)
{
        int n,i,b;
        extern int teleporter;

        for (n=0; n<64+32; n++) {
                i=n/8;
                b=1<<(n&7);
                if (buf[i+1]&b) may_teleport[n]=1;
                else may_teleport[n]=0;
                //addline("%d: %d (%d,%X)",n,may_teleport[n],i,b);
        }
        teleporter=1;
        newmirror=mirror;
        //note("reset newmirror to %d",newmirror);
}

void sv_prof(unsigned char *buf)
{
        int n,cnt=0;

        for (n=0; n<P_MAX; n++) {
                cnt+=(value[1][n+V_PROFBASE]=buf[n+1]);
        }
        value[0][V_PROFESSION]=cnt;

        update_skltab=1;
}

struct quest quest[MAXQUEST];
struct shrine_ppd shrine;

void sv_questlog(unsigned char *buf)
{
        int size;

        size=sizeof(struct quest)*MAXQUEST;

        memcpy(quest,buf+1,size);
        memcpy(&shrine,buf+1+size,sizeof(struct shrine_ppd));
}

static void save_unique(void)
{
        HKEY hk;

        if (RegCreateKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Notepad",&hk)!=ERROR_SUCCESS) return;

        unique=unique^0xfe2abc82;
        usum=  unique^0x3e5fba04;

        RegSetValueEx(hk,"fInput1",0,REG_DWORD,(void*)&unique,4);
        RegSetValueEx(hk,"fInput2",0,REG_DWORD,(void*)&usum,4);
}

static void load_unique(void)
{
        HKEY hk;
        int size=4,type;

        if (RegCreateKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Notepad",&hk)!=ERROR_SUCCESS) return;

        RegQueryValueEx(hk,"fInput1",0,(void*)&type,(void*)&unique,(void*)&size);
        RegQueryValueEx(hk,"fInput2",0,(void*)&type,(void*)&usum,(void*)&size);
        
        if ((unique^0x3e5fba04)!=usum) unique=usum=0;
        else unique=unique^0xfe2abc82;
}

void sv_unique(unsigned char *buf)
{
        if (unique!=*(unsigned int*)(buf+1)) {
                unique=*(unsigned int*)(buf+1);
                save_unique();
        }
        //printf("set unique to %d\n",unique);
}

void process(unsigned char *buf,int size)
{
        int len=0,panic=0,last=-1;

        //printf("process called\n");

        while (size>0 && panic++<20000) {
                //printf("pro: buf=%p, size=%d, *=%d\n",buf,size,buf[0]);
                     if ((buf[0]&(64+128))==SV_MAP01) len=sv_map01(buf,&last,map);  // ANKH
                else if ((buf[0]&(64+128))==SV_MAP10) len=sv_map10(buf,&last,map);  // ANKH
                else if ((buf[0]&(64+128))==SV_MAP11) len=sv_map11(buf,&last,map);  // ANKH
                else
                        switch (buf[0]) {
                                case SV_SCROLL_UP:              sv_scroll_up(map); len=1; break;
                                case SV_SCROLL_DOWN:            sv_scroll_down(map); len=1; break;
                                case SV_SCROLL_LEFT:            sv_scroll_left(map); len=1; break;
                                case SV_SCROLL_RIGHT:           sv_scroll_right(map); len=1; break;
                                case SV_SCROLL_LEFTUP:          sv_scroll_leftup(map); len=1; break;
                                case SV_SCROLL_LEFTDOWN:        sv_scroll_leftdown(map); len=1; break;
                                case SV_SCROLL_RIGHTUP:         sv_scroll_rightup(map); len=1; break;
                                case SV_SCROLL_RIGHTDOWN:       sv_scroll_rightdown(map); len=1; break;

                                case SV_SETVAL0:                sv_setval(buf,0); len=4; break;
                                case SV_SETVAL1:                sv_setval(buf,1); len=4; break;

                                case SV_SETHP:                  sv_sethp(buf); len=3; break;
                                case SV_SETMANA:                sv_setmana(buf); len=3; break;
                                case SV_SETRAGE:                sv_setrage(buf); len=3; break;
                                case SV_ENDURANCE:                sv_endurance(buf); len=3; break;
                                case SV_LIFESHIELD:                sv_lifeshield(buf); len=3; break;

                                case SV_SETITEM:                sv_setitem(buf); len=10; break;

                                case SV_SETORIGIN:              sv_setorigin(buf); len=5; break;
                                case SV_SETTICK:                sv_settick(buf); len=5; break;
                                case SV_SETCITEM:               sv_setcitem(buf); len=9; break;

                                case SV_ACT:                    sv_act(buf); len=7; break;
                                case SV_EXIT:                        len=sv_exit(buf); break;
                                case SV_TEXT:                   len=sv_text(buf); break;
                                

                                case SV_NAME:                        len=sv_name(buf); break;

                                case SV_CONTAINER:                sv_container(buf); len=6; break;
                                case SV_PRICE:                        sv_price(buf); len=6; break;
                                case SV_CPRICE:                        sv_cprice(buf); len=5; break;
                                case SV_CONCNT:                        sv_concnt(buf); len=2; break;
                                case SV_ITEMPRICE:                sv_itemprice(buf); len=6; break;
                                case SV_CONTYPE:                sv_contype(buf); len=2; break;
                                case SV_CONNAME:                len=sv_conname(buf); break;

                                case SV_GOLD:                        sv_gold(buf); len=5; break;

                                case SV_EXP:                         sv_exp(buf); len=5; break;
                                case SV_EXP_USED:                sv_exp_used(buf); len=5; break;
                                case SV_MIL_EXP:                 sv_mil_exp(buf); len=5; break;
                                case SV_LOOKINV:                sv_lookinv(buf); len=17+12*4; break;
                                case SV_CYCLES:                        sv_cycles(buf); len=5; break;
                                case SV_CEFFECT:                len=sv_ceffect(buf); break;
                                case SV_UEFFECT:                sv_ueffect(buf); len=9; break;

                                case SV_SERVER:                        sv_server(buf); len=7; break;

                                case SV_REALTIME:               sv_realtime(buf); len=5; break;

                                case SV_SPEEDMODE:                sv_speedmode(buf); len=2; break;
                                case SV_FIGHTMODE:                sv_fightmode(buf); len=2; break;
                                case SV_LOGINDONE:                sv_logindone(); len=1; break;
                                case SV_SPECIAL:                sv_special(buf); len=13; break;
                                case SV_TELEPORT:                sv_teleport(buf); len=13; break;

                                case SV_MIRROR:                 sv_mirror(buf); len=5; break;
                                case SV_PROF:                        sv_prof(buf); len=21; break;
                                case SV_PING:                        len=sv_ping(buf); break;
                                case SV_UNIQUE:                        sv_unique(buf); len=5; break;
                                case SV_QUESTLOG:                sv_questlog(buf); len=101+sizeof(struct shrine_ppd); break;

                                default:                        return;// endwin(); printf("size=%d, len=%d",size,len); exit(1);
                        }

                size-=len; buf+=len;
        }

        if (size) {
                printf("PANIC! size=%d",size); exit(1);
        }
}

void prefetch(unsigned char *buf,int size)
{
        int len=0,panic=0,last=-1;

        while (size>0 && panic++<20000) {
                //printf("pre: buf=%p, size=%d, *=%d [%d %d %d %d %d %d %d %d ]\n",buf,size,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8]);
                     if ((buf[0]&(64+128))==SV_MAP01) len=sv_map01(buf,&last,map2);  // ANKH
                else if ((buf[0]&(64+128))==SV_MAP10) len=sv_map10(buf,&last,map2);  // ANKH
                else if ((buf[0]&(64+128))==SV_MAP11) len=sv_map11(buf,&last,map2);  // ANKH
                else
                        switch (buf[0]) {
                                case SV_SCROLL_UP:              sv_scroll_up(map2); len=1; break;
                                case SV_SCROLL_DOWN:            sv_scroll_down(map2); len=1; break;
                                case SV_SCROLL_LEFT:            sv_scroll_left(map2); len=1; break;
                                case SV_SCROLL_RIGHT:           sv_scroll_right(map2); len=1; break;
                                case SV_SCROLL_LEFTUP:          sv_scroll_leftup(map2); len=1; break;
                                case SV_SCROLL_LEFTDOWN:        sv_scroll_leftdown(map2); len=1; break;
                                case SV_SCROLL_RIGHTUP:         sv_scroll_rightup(map2); len=1; break;
                                case SV_SCROLL_RIGHTDOWN:       sv_scroll_rightdown(map2); len=1; break;

                                case SV_SETVAL0:                len=4; break;
                                case SV_SETVAL1:                len=4; break;

                                case SV_SETHP:                  len=3; break;
                                case SV_SETMANA:                len=3; break;
                                case SV_SETRAGE:                len=3; break;
                                case SV_ENDURANCE:                len=3; break;
                                case SV_LIFESHIELD:                len=3; break;

                                case SV_SETITEM:                len=10; break;

                                case SV_SETORIGIN:              len=5; break;
                                case SV_SETTICK:                len=5; break;
                                case SV_SETCITEM:               len=9; break;

                                case SV_ACT:                    len=7; break;

                                case SV_TEXT:                   len=svl_text(buf); break;
                                case SV_EXIT:                   len=svl_exit(buf); break;

                                case SV_NAME:                        len=svl_name(buf); break;

                                case SV_CONTAINER:                len=6; break;
                                case SV_PRICE:                        len=6; break;
                                case SV_CPRICE:                        len=5; break;
                                case SV_CONCNT:                        len=2; break;
                                case SV_ITEMPRICE:                len=6; break;
                                case SV_CONTYPE:                len=2; break;
                                case SV_CONNAME:                len=svl_conname(buf); break;

                                case SV_MIRROR:                len=5; break;


                                case SV_GOLD:                        len=5; break;

                                case SV_EXP:                         len=5; break;
                                case SV_EXP_USED:                len=5; break;
                                case SV_MIL_EXP:                len=5; break;
                                case SV_LOOKINV:                len=17+12*4; break;
                                case SV_CYCLES:                        len=5; break;
                                case SV_CEFFECT:                len=svl_ceffect(buf); break;
                                case SV_UEFFECT:                len=9; break;

                                case SV_SERVER:                        len=7; break;
                                case SV_REALTIME:               len=5; break;
                                case SV_SPEEDMODE:                len=2; break;
                                case SV_FIGHTMODE:                len=2; break;
                                case SV_LOGINDONE:                len=1; break;
                                case SV_SPECIAL:                len=13; break;
                                case SV_TELEPORT:                len=13; break;
                                case SV_PROF:                        len=21; break;
                                case SV_PING:                        len=svl_ping(buf); break;
                                case SV_UNIQUE:                        len=5; break;
                                case SV_QUESTLOG:                len=101+sizeof(struct shrine_ppd); break;

                                default:                        note("got illegal command %d",buf[0]); exit(1);
                        }

                size-=len; buf+=len;
        }

        if (size) {
                printf("2 PANIC! size=%d",size); exit(1);
        }
}

void client_send(void *buf, int len)
{
        if (len>MAX_OUTBUF-outused) return;

        memcpy(outbuf+outused,buf,len);
        outused+=len;

        /*int n,tot=0;

        while(tot<len) {
                n=send(sock,((char *)(buf))+tot,len-tot,0);
                if (n<=0) {
                        addline("connection lost during write");
                        closesocket(sock);
                        sock=-1;
                        return;
                }
                tot+=n;
        }*/
}

void cmd_move(int x, int y)
{
        char buf[16];

        buf[0]=CL_MOVE;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);
}

void cmd_ping(void)
{
        char buf[16];

        buf[0]=CL_PING;
        *(unsigned int *)(buf+1)=GetTickCount();
        client_send(buf,5);
}

void cmd_swap(int with)
{
        char buf[16];

        buf[0]=CL_SWAP;
        buf[1]=with;
        client_send(buf,2);
}

void cmd_fastsell(int with)
{
        char buf[16];

        buf[0]=CL_FASTSELL;
        buf[1]=with;
        client_send(buf,2);
}

void cmd_use_inv(int with)
{
        char buf[16];

        buf[0]=CL_USE_INV;
        buf[1]=with;
        client_send(buf,2);
}

void cmd_take(int x, int y)
{
        char buf[16];

        buf[0]=CL_TAKE;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);                
}

void cmd_look_map(int x, int y)
{
        char buf[16];

        buf[0]=CL_LOOK_MAP;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);
}

void cmd_look_item(int x, int y)
{
        char buf[16];

        buf[0]=CL_LOOK_ITEM;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);
}

void cmd_look_inv(int pos)
{
        char buf[16];

        buf[0]=CL_LOOK_INV;
        *(unsigned char *)(buf+1)=pos;        
        client_send(buf,2);
}

void cmd_look_char(int cn)
{
        char buf[16];

        buf[0]=CL_LOOK_CHAR;
        *(unsigned short *)(buf+1)=cn;        
        client_send(buf,3);
}

void cmd_use(int x, int y)
{
        char buf[16];

        buf[0]=CL_USE;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);
}

void cmd_drop(int x, int y)
{
        char buf[16];

        buf[0]=CL_DROP;
        *(unsigned short int *)(buf+1)=x;
        *(unsigned short int *)(buf+3)=y;
        client_send(buf,5);        
}

void cmd_speed(int mode)
{
        char buf[16];

        buf[0]=CL_SPEED;
        buf[1]=mode;
        client_send(buf,2);
}

void cmd_combat(int mode)
{
        char buf[16];

        buf[0]=CL_FIGHTMODE;
        buf[1]=mode;
        client_send(buf,2);
}

void cmd_teleport(int nr)
{
        char buf[64];

        if (nr>100) {        // ouch
                extern int newmirror;

                newmirror=nr-100;
                //note("set newmirror to %d",newmirror);
                return;
        }

        buf[0]=CL_TELEPORT;
        buf[1]=nr;
        buf[2]=newmirror;
        client_send(buf,3);
}

void cmd_stop(void)
{
        char buf[16];

        buf[0]=CL_STOP;        
        client_send(buf,1);
}

void cmd_kill(int cn)
{
        char buf[16];

        buf[0]=CL_KILL;
        *(unsigned short int *)(buf+1)=cn;
        client_send(buf,3);
}

void cmd_give(int cn)
{
        char buf[16];

        buf[0]=CL_GIVE;
        *(unsigned short int *)(buf+1)=cn;
        client_send(buf,3);
}

void cmd_some_spell(int spell, int x, int y, int chr)
{
        char buf[16],len;

        switch (spell) {
                case CL_BLESS:
                case CL_HEAL:
                        buf[0]=spell;
                        *(unsigned short *)(buf+1)=chr;
                        len=3;
                        break;

                case CL_FIREBALL:
                case CL_BALL:
                        buf[0]=spell;
                        if (x) {
                                *(unsigned short *)(buf+1)=x;
                                *(unsigned short *)(buf+3)=y;
                        } else {
                                *(unsigned short *)(buf+1)=0;
                                *(unsigned short *)(buf+3)=chr;
                        }
                        
                        len=5;
                        break;
                
                case CL_MAGICSHIELD:
                case CL_FLASH:
                case CL_WARCRY:
                case CL_FREEZE:
                case CL_PULSE:
                        buf[0]=spell;
                        len=1;
                        break;
                default:        
                        addline("WARNING: unknown spell %d\n",spell);
                        return;
        }

        //addline("sending spell %d, len %d, [%d,%d]",(int)buf[0],(int)len,(int)(*(unsigned short*)(buf+1)),(int)(*(unsigned short*)(buf+3)));

        client_send(buf,len);
}

void cmd_raise(int vn)
{
        char buf[16];

        buf[0]=CL_RAISE;
        *(unsigned short int *)(buf+1)=vn;
        client_send(buf,3);
}

void cmd_take_gold(int vn)
{
        char buf[16];

        buf[0]=CL_TAKE_GOLD;
        *(unsigned int *)(buf+1)=vn;
        client_send(buf,5);        
}

void cmd_drop_gold(void)
{
        char buf[16];

        buf[0]=CL_DROP_GOLD;
        client_send(buf,1);
}

void cmd_junk_item(void)
{
        char buf[16];

        buf[0]=CL_JUNK_ITEM;
        client_send(buf,1);
}

void cmd_text(char *text)
{
        char buf[512];
        int len;

        // MR

        if (!text) return;

        buf[0]=CL_TEXT;

        for (len=0; text[len] && text[len]!='°' && len<254; len++) buf[len+2]=text[len];

        buf[2+len]=0;
        buf[1]=len+1;

        client_send(buf,len+3);
}

void cmd_log(char *text)
{
        char buf[512];
        int len;

        // MR

        if (!text) return;

        buf[0]=CL_LOG;

        for (len=0; text[len] && len<254; len++) buf[len+2]=text[len];

        buf[2+len]=0;
        buf[1]=len+1;

        client_send(buf,len+3);
}

void cmd_con(int pos)
{
        char buf[16];

        buf[0]=CL_CONTAINER;
        buf[1]=pos;
        client_send(buf,2);        
}

void cmd_con_fast(int pos)
{
        char buf[16];

        buf[0]=CL_CONTAINER_FAST;
        buf[1]=pos;
        client_send(buf,2);        
}

void cmd_look_con(int pos)
{
        char buf[16];

        buf[0]=CL_LOOK_CONTAINER;
        buf[1]=pos;
        client_send(buf,2);        
}

void cmd_getquestlog(void)
{
        char buf[16];

        buf[0]=CL_GETQUESTLOG;
        client_send(buf,1);
}

void cmd_reopen_quest(int nr)
{
        char buf[16];

        buf[0]=CL_REOPENQUEST;
        buf[1]=nr;
        client_send(buf,2);
}

void bzero_client(int part)
{
        extern int show_look;
        if (part==0) {
                //tick=0;
                lasttick=0;
                lastticksize=0;

                bzero(queue,sizeof(queue));
                q_in=q_out=q_size=0;

                server_cycles=0;

                zsinit=0;
                bzero(&zs,sizeof(zs));

                ticksize=0;
                inused=0;
                indone=0;
                login_done=0;
                sock_done=0;
                bzero(inbuf,sizeof(inbuf));

                outused=0;
                bzero(outbuf,sizeof(outbuf));                
        }

        if (part==1) {
                //tick=0;

                act=0;
                actx=0;
                acty=0;

                cflags=0;
                csprite=0;

                originx=0;
                originy=0;
                bzero(map,sizeof(map));
                bzero(map2,sizeof(map2));

                bzero(value,sizeof(value));
                bzero(item,sizeof(item));
                bzero(item_flags,sizeof(item_flags));
                hp=0;
                mana=0;
                endurance=0;
                lifeshield=0;
                experience=0;
                experience_used=0;
                mil_exp=0;
                gold=0;

                bzero(player,sizeof(player));

                bzero(ceffect,sizeof(ceffect));
                bzero(ueffect,sizeof(ueffect));

                con_cnt=0;
                bzero(container,sizeof(container));
                bzero(price,sizeof(price));
                bzero(itemprice,sizeof(itemprice));
                cprice=0;

                bzero(lookinv,sizeof(lookinv));
                show_look=0;

                pspeed=0;
                pcombat=0;
                pent_str[0][0]=pent_str[1][0]=pent_str[2][0]=pent_str[3][0]=pent_str[4][0]=pent_str[5][0]=pent_str[6][0]=0;

                bzero(may_teleport,sizeof(may_teleport));
        }
}

/*
int open_client(char *username, char *password)
{
        struct sockaddr_in addr;
        unsigned long one=1;
        struct hostent *he;
        char tmp[80];

        if (sock!=-1) return fail("client was already open!");

        // init
        // bzero(&__struct_client_start__+1,&__struct_client_end__-&__struct_client_start__-1);
        bzero_client();

        // connect
        //serveraddr=htonl((192<<24)|(168<<16)|(42<<8)|(1<<0));
        //serveraddr=htonl((64<<24)|(23<<16)|(60<<8)|(52<<0));
        //serveraddr=htonl((192<<24)|(168<<16)|(42<<8)|(26<<0));

        note("open client (%d)",GetTickCount());

        if ((sock=socket(PF_INET,SOCK_STREAM,0))==INVALID_SOCKET) { sock=-1; return -1; }

        addline("connecting to %d.%d.%d.%d:%d",(target_server>>24)&0xff,(target_server>>16)&0xff,(target_server>>8)&0xff,(target_server>>0)&0xff,target_port);

        addr.sin_family=AF_INET;
        addr.sin_port=htons(target_port);
        addr.sin_addr.s_addr=htonl(target_server);
        if ((connect(sock,(struct sockaddr*)&addr,sizeof(addr)))) {
                if (WSAGetLastError()!=WSAEWOULDBLOCK) {
                        note("connect failed %d\n",WSAGetLastError());
                        closesocket(sock);
                        sock=-1;
                        return -1;
                }
                else note("connect would block");
        }

        // set client values
        if (inflateInit(&zs)) { closesocket(sock); sock=-1; return -1; }
        if (inflateInit(&zs2)) { closesocket(sock); sock=-1; return -1; }
        zsinit=1;

        // send name
        bzero(tmp,sizeof(tmp));
        strcpy(tmp,username);
        // send(sock,tmp,40,0);
        client_send(tmp,40);

        // send password
        bzero(tmp,sizeof(tmp));
        strcpy(tmp,password);
        // send(sock,tmp,16,0);
        client_send(tmp,16);

        // set client to non-blocking mode
        if (ioctlsocket(sock,FIONBIO,&one)==-1) {
                closesocket(sock);
                sock=-1;
                printf("ioctlsocket(non-blocking) failed\n",WSAGetLastError());
                return -1;
        }

        change_area=0;

        return 0;
}
*/

int close_client(void)
{
        if (sock!=-1) { closesocket(sock); sock=-1; }
        if (zsinit) { inflateEnd(&zs); zsinit=0; }

        sockstate=0;
        socktime=0;

        bzero_client(0);
        bzero_client(1);

        return 0;
}

#define MAXPASSWORD        16
void decrypt(char *name,char *password)
{
        int i;
        static char secret[4][MAXPASSWORD]={
                "\000cgf\000de8etzdf\000dx",
                "jrfa\000v7d\000drt\000edm",
                "t6zh\000dlr\000fu4dms\000",
                "jkdm\000u7z5g\000j77\000g"
                };

        for (i=0; i<MAXPASSWORD; i++) {
                password[i]=password[i]^secret[name[1]%4][i]^name[i%3];
        }
}

void send_info(int sock)
{
        struct sockaddr_in addr;
        int len;
        char buf[80];

        len=sizeof(addr);
        getsockname(sock,(struct sockaddr*)&addr,&len);
        *(unsigned int*)(buf+0)=addr.sin_addr.s_addr;
        
        len=sizeof(addr);
        getpeername(sock,(struct sockaddr*)&addr,&len);
        *(unsigned int*)(buf+4)=addr.sin_addr.s_addr;

        load_unique();

        *(unsigned int*)(buf+8)=unique;

        send(sock,buf,12,0);
}

int poll_network(void)
{
        int n,nn;
        extern int vendor;

        // something fatal failed (sockstate will somewhen tell you what)
        if (sockstate<0) {
                return -1;
        }

        // create nonblocking socket
        if (sockstate==0 && !kicked_out) {

                struct sockaddr_in addr;
                unsigned long one=1;

                if (GetTickCount()<socktime) return 0;

                //note("create nonblocking socket");

                // reset socket
                if (sock!=-1) { closesocket(sock); sock=-1; }
                if (zsinit) { inflateEnd(&zs); zsinit=0; }
                change_area=0;
                kicked_out=0;

                // reset client
                bzero_client(0);

                if (!socktimeout) {
                        socktimeout=time(NULL);
                }

                // create socket
                if ((sock=socket(PF_INET,SOCK_STREAM,0))==INVALID_SOCKET) {
                        fail("creating socket failed (%d)",WSAGetLastError());
                        sock=-1;
                        sockstate=-1;   // fail - no retry
                        return -1;
                }

                // set to nonblocking
                if (ioctlsocket(sock,FIONBIO,&one)==-1) {
                        fail("ioctlsocket(non-blocking) failed\n",WSAGetLastError());
                        sockstate=-2;   // fail - no retry
                        return -1;
                }

                // connect to server
                if (sock_server) {
                        addr.sin_family=AF_INET;
                        addr.sin_port=htons(sock_port);
                        addr.sin_addr.s_addr=htonl(sock_server);
                        note("trying sock server at %d.%d.%d.%d:%d as %s. will connect to %d.%d.%d.%d:%d",
                                (sock_server>>24)&255,(sock_server>>16)&255,(sock_server>>8)&255,(sock_server>>0)&255,sock_port,sock_user,
                                (target_server>>24)&255,(target_server>>16)&255,(target_server>>8)&255,(target_server>>0)&255,target_port);
                        if ((connect(sock,(struct sockaddr*)&addr,sizeof(addr)))) {
                                if (WSAGetLastError()!=WSAEWOULDBLOCK) {
                                        fail("connect failed (%d)\n",WSAGetLastError());
                                        sockstate=-3;   // fail - no retry
                                        return -1;
                                }
                        }
                } else {
                        addr.sin_family=AF_INET;
                        addr.sin_port=htons(target_port);
                        addr.sin_addr.s_addr=htonl(target_server);
                        note("trying %d.%d.%d.%d",(target_server>>24)&255,(target_server>>16)&255,(target_server>>8)&255,(target_server>>0)&255);
                        if ((connect(sock,(struct sockaddr*)&addr,sizeof(addr)))) {
                                if (WSAGetLastError()!=WSAEWOULDBLOCK) {
                                        fail("connect failed (%d)\n",WSAGetLastError());
                                        sockstate=-3;   // fail - no retry
                                        return -1;
                                }
                        }
                }

                // statechange
                //note("got socket");
                sockstate=1;
                // return 0;
        }

        // wait until connect is ok
        if (sockstate==1) {

                struct fd_set outset,errset;
                struct timeval timeout;

                if (GetTickCount()<socktime) return 0;

                //note("check if connected");

                FD_ZERO(&outset);
                FD_ZERO(&errset);
                FD_SET((unsigned int)sock,&outset);
                FD_SET((unsigned int)sock,&errset);

                timeout.tv_sec=0;
                timeout.tv_usec=50;
                n=select(sock+1,NULL,&outset,&errset,&timeout);
                if (n==0) {
                        // timed out
                        //note("select timed out");
                        socktime=GetTickCount()+50;
                        return 0;
                }

                if (FD_ISSET(sock,&errset)) {
                        note("select connect failed (%d)",WSAGetLastError());
                        sockstate=0;
                        socktime=GetTickCount()+5000;
                        return -1;
                }

                if (!FD_ISSET(sock,&outset)) {
                        note("can we see this (select without timeout and none set) ?");
                        sockstate=-4;   // fail - no retry
                        return -1;
                }

                // statechange
                //note("connected");
                sockstate=2;
                // return 0;
        }

        // connected - send password and initialize compression buffers
        if (sockstate==2) {
                char tmp[256];

                //note("initialize some basics");

                // initialize compression
                if (inflateInit(&zs)) {
                        note("zsinit failed");
                        sockstate=-5;   // fail - no retry
                        return -1;
                }
                zsinit=1;

                if (sock_server) {
                        tmp[0]=4;
                        tmp[1]=1;
                        *(unsigned short*)(tmp+2)=htons(target_port);
                        *(unsigned int*)(tmp+4)=htonl(target_server);
                        strcpy(tmp+8,sock_user);
                        send(sock,tmp,strlen(sock_user)+8+1,0);
                }

                // send name
                bzero(tmp,sizeof(tmp));
                strcpy(tmp,username);
                send(sock,tmp,40,0);
                //client_send(tmp,40);

                // send password
                bzero(tmp,sizeof(tmp));
                strcpy(tmp,password);
                decrypt(username,tmp);
                send(sock,tmp,16,0);
                //client_send(tmp,16);

                *(unsigned int*)(tmp)=vendor;
                send(sock,tmp,4,0);
                send_info(sock);

                // statechange
                //note("done");
                sockstate=3;
        }

        if (sock_server && sock_done<8) {
                static unsigned char sock_msg[8];
                n=recv(sock,sock_msg+sock_done,8-sock_done,0);
                if (n<=0) {
                        if (WSAGetLastError()!=WSAEWOULDBLOCK) {
                                addline("connection lost during read (%d)\n",WSAGetLastError());
                                sockstate=0;
                                socktimeout=time(NULL);
                                return -1;
                        }
                        return 0;
                }
                sock_done+=n;
                
                //note("%d: %02X%02X%02X%02X%02X%02X%02X%02X",sock_done,sock_msg[0],sock_msg[1],sock_msg[2],sock_msg[3],sock_msg[4],sock_msg[5],sock_msg[6],sock_msg[7]);
                return 0;
        }

        //note("past sock_server");

        // here we go ...
        if (change_area) {
                sockstate=0;
                socktimeout=time(NULL);
                // change_area=0;
                return 0;
        }

        if (kicked_out) {
                //note("kicked out");
                sockstate=-6;   // fail - no retry
                // kicked_out=0;
                close_client();
                return -1;
        }

        //note("sockstate=%d, login_done=%d",sockstate,login_done);

        // check if we have one tick, so we can reset the map and move to state 4 !!! note that this state has no return statement, so we still read and write)
        if (sockstate==3) {
                if (login_done) { //if (lasttick>1) {
                        // statechange
                        //note("go ahead (left at tick=%d)",tick);
                        //bzero_client(1);
                        sockstate=4;
                }
        }

        // send
        if (outused && sockstate==4) {
                //note("send called, outused=%d, sockstate=%d",outused,sockstate);
                n=send(sock,outbuf,outused,0);

                if (n<=0) {
                        addline("connection lost during write (%d)\n",WSAGetLastError());
                        sockstate=0;
                        socktimeout=time(NULL);
                        return -1;
                }

                memmove(outbuf,outbuf+n,outused-n);
                outused-=n;
                sent_bytes+=n;
        }

        // recv
        n=recv(sock,(char *)inbuf+inused,MAX_INBUF-inused,0);
        if (n<=0) {
                if (WSAGetLastError()!=WSAEWOULDBLOCK) {
                        addline("connection lost during read (%d)\n",WSAGetLastError());
                        sockstate=0;
                        socktimeout=time(NULL);
                        return -1;
                }
                return 0;
        }
        inused+=n;
        rec_bytes+=n;

        //note("got %d: %d %d",n,inbuf[0],inbuf[1]);

        // count ticks
        while (1) {
                if (inused>=lastticksize+1 && *(inbuf+lastticksize)&0x40) {
                        lastticksize+=1+(*(inbuf+lastticksize)&0x3F);
                } else if (inused>=lastticksize+2) {
                        lastticksize+=2+(ntohs(*(unsigned short *)(inbuf+lastticksize))&0x3FFF);
                } else break;
                
                lasttick++;
        }

        return 0;
}

void auto_tick(struct map *cmap)
{
        int x,y,mn;
        
        // automatically tick map
        for (y=0; y<MAPDY; y++) {
                for (x=0; x<MAPDX; x++) {

                        mn=mapmn(x,y);
                        if (!(cmap[mn].csprite)) continue;
                        //if (cmap[mn].flags&CMF_STUNNED) continue;

                        cmap[mn].step++;
                        if (cmap[mn].step<cmap[mn].duration) continue;
                        cmap[mn].step=0;
                }
        }
}

int next_tick(void)
{
        int ticksize;
        int size,ret;

        // no room for next tick, leave it in in-queue
        if (q_size==Q_SIZE) return 0;

        // do we have a new tick
        if (inused>=1 && (*(inbuf)&0x40)) {
                ticksize=1+(*(inbuf)&0x3F);
                if (inused<ticksize) return 0;
                indone=1;
        }
        else if (inused>=2 && !(*(inbuf)&0x40)) {
                ticksize=2+(ntohs(*(unsigned short *)(inbuf))&0x3FFF);
                if (inused<ticksize) return 0;
                indone=2;
        }
        else {
                return 0;
        }

        // decompress
        if (*inbuf&0x80) {

                zs.next_in=inbuf+indone;
                zs.avail_in=ticksize-indone;

                zs.next_out=queue[q_in].buf;        //obuf;
                zs.avail_out=sizeof(queue[q_in].buf); //sizeof(obuf);

                ret=inflate(&zs,Z_SYNC_FLUSH);
                if (ret!=Z_OK) {
                        printf("Compression error %d\n",ret);
                        quit=1;
                        return 0;
                }

                if (zs.avail_in) { printf("HELP (%d)\n",zs.avail_in); return 0; }

                size=sizeof(queue[q_in].buf)-zs.avail_out;        //sizeof(obuf)
        }
        else {
                size=ticksize-indone;
                memcpy(queue[q_in].buf,inbuf+indone,size);
        }
        queue[q_in].size=size;
        
        auto_tick(map2);
        prefetch(queue[q_in].buf,queue[q_in].size);

        q_in=(q_in+1)%Q_SIZE;
        q_size++;

        // remove tick from inbuf
        if (inused-ticksize>=0) memmove(inbuf,inbuf+ticksize,inused-ticksize); else note("kuckuck!");
        inused=inused-ticksize;

        // adjust some values
        lasttick--;
        lastticksize-=ticksize;

        return 1;
}

int do_tick(void)
{
        // process tick
        if (q_size>0) {
                
                auto_tick(map);
                process(queue[q_out].buf,queue[q_out].size);
                q_out=(q_out+1)%Q_SIZE;
                q_size--;

                // increase tick
                tick++;
                if (tick%TICKS==0) realtime++;

                return 1;
        }

        return 0;
}

//----------

void cl_client_info(struct client_info *ci)
{
        char buf[256];

        buf[0]=CL_CLIENTINFO;
        memcpy(buf+1,ci,sizeof(struct client_info));
        client_send(buf,sizeof(struct client_info)+1);
}

void cl_ticker(void)
{
        char buf[256];

        buf[0]=CL_TICKER;
        *(unsigned int*)(buf+1)=tick;
        client_send(buf,5);
}

// X exp yield level Y
int exp2level(int val)
{
        if (val<1) return 1;
        
        return max(1,(int)(sqrt(sqrt(val))));
}

// to reach level X you need Y exp
int level2exp(int level)
{
        return pow(level,4);
}