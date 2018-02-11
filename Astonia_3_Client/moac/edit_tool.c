#include <windows.h>
#include <stdio.h>
#pragma hdrstop
#include "main.h"
#include "edit.h"

#ifdef EDITOR

int ifilecount=0;
IFILE **ifilelist=NULL;

int cfilecount=0;
CFILE **cfilelist=NULL;

int mfilecount=0;
MFILE **mfilelist=NULL;

/*
int nfilecount=0;
NFILE **nfilelist=NULL;
*/

int pfilecount=0;
PFILE **pfilelist=NULL;

char *parse_spaces(char *run)
{
        do {
                 while (*run==' ' || *run=='\t' || *run=='\r' || *run=='\n') run++;
                 if (*run=='#') while (*run && *run!='\n') run++;
        }
        while(*run=='#');

        return run;
}

char *parse_nsv(char *run, char *name, char *sep, char *value, int *ivalue)
{
        char *valuebeg=value;

        *ivalue=-1;

        run=parse_spaces(run);
        if (*run==0) { *sep=0; *name=0; *value=0; return run; }
        if (*run==';') { *sep=*run++; *name=0; *value=0; return run; }
        while (*run && *run!=' ' && *run!='\t' && *run!='\r' && *run!='\n' && *run!=':' && *run!=';' && *run!='=') *name++=*run++;

        run=parse_spaces(run);
        if (*run==0) { *sep=0; *name=0; *value=0; return run; }
        if (*run!=':' && *run!='=') { *sep=0; *name=0; *value=0; return run; }

        *sep=*run++;
        if (*sep==':') { *name=0; *value=0; return run; }

        run=parse_spaces(run);
        if (*run==0) { *sep=0; *name=0; *value=0; return run; }

        if (*run=='"') {
                run++;
                while (*run && *run!='"') *value++=*run++;
                run++;
        }
        else {
                while (*run && *run!=' ' && *run!='\t' && *run!='\r' && *run!='\n' && *run!=':' && *run!=';' && *run!='=') *value++=*run++;
                *value=0;
                *ivalue=atoi(valuebeg);
        }

        *name=0;
        *value=0;

        return run;
}

void find_itm_files(char *searchpath)
{
        WIN32_FIND_DATA data;
        HANDLE find;
        IFILE *ifile;
        char searchstring[1024];

        sprintf(searchstring,"%s*.itm",searchpath);

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
                do {
                        ifile=xcalloc(sizeof(IFILE),MEM_EDIT);
                        sprintf(ifile->filename,"%s%s",searchpath,data.cFileName);
                        addptr((void ***)&ifilelist,&ifilecount,ifile,MEM_EDIT);
                }
                while (FindNextFile(find,&data));

                FindClose(find);
        }
}

void find_chr_files(char *searchpath)
{
        WIN32_FIND_DATA data;
        HANDLE find;
        CFILE *cfile;
        char searchstring[1024];

        sprintf(searchstring,"%s*.chr",searchpath);

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
                do {
                        cfile=xcalloc(sizeof(CFILE),MEM_EDIT);
                        sprintf(cfile->filename,"%s%s",searchpath,data.cFileName);
                        addptr((void ***)&cfilelist,&cfilecount,cfile,MEM_EDIT);
                }
                while (FindNextFile(find,&data));

                FindClose(find);
        }
}

void find_map_files(char *searchpath)
{
        WIN32_FIND_DATA data;
        HANDLE find;
        MFILE *mfile;
        char searchstring[1024];

        sprintf(searchstring,"%s*.map",searchpath);

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
                do {
                        mfile=xcalloc(sizeof(MFILE),MEM_EDIT);
                        sprintf(mfile->filename,"%s%s",searchpath,data.cFileName);
                        addptr((void ***)&mfilelist,&mfilecount,mfile,MEM_EDIT);
                }
                while (FindNextFile(find,&data));

                FindClose(find);
        }
}

/*
void find_int_files(char *searchpath)
{
        WIN32_FIND_DATA data;
        HANDLE find;
        NFILE *nfile;
        char searchstring[1024];

        sprintf(searchstring,"%s*.int",searchpath);

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
                do {
                        nfile=xcalloc(sizeof(NFILE),MEM_EDIT);
                        sprintf(nfile->filename,"%s%s",searchpath,data.cFileName);
                        addptr((void ***)&nfilelist,&nfilecount,nfile,MEM_EDIT);
                }
                while (FindNextFile(find,&data));

                FindClose(find);
        }
}
*/

void find_pre_files(char *searchpath)
{
        WIN32_FIND_DATA data;
        HANDLE find;
        PFILE *pfile;
        char searchstring[1024];

        sprintf(searchstring,"%s*.pre",searchpath);

        if ((find=FindFirstFile(searchstring,&data))!=INVALID_HANDLE_VALUE) {
                do {
                        pfile=xcalloc(sizeof(PFILE),MEM_EDIT);
                        sprintf(pfile->filename,"%s%s",searchpath,data.cFileName);
                        addptr((void ***)&pfilelist,&pfilecount,pfile,MEM_EDIT);
                }
                while (FindNextFile(find,&data));

                FindClose(find);
        }
}

EITEM *find_item(char *uni)
{
        int f,i;

        for (f=0; f<ifilecount; f++) {
                for (i=0; i<ifilelist[f]->icount; i++) {
                        if (!strcmp(ifilelist[f]->ilist[i]->uni,uni)) return ifilelist[f]->ilist[i];
                }
        }

        return NULL;
}

ECHAR *find_char(char *uni)
{
        int f,i;

        for (f=0; f<cfilecount; f++) {
                for (i=0; i<cfilelist[f]->ccount; i++) {
                        if (!strcmp(cfilelist[f]->clist[i]->uni,uni)) return cfilelist[f]->clist[i];
                }
        }

        return NULL;
}

void load_itm_file(IFILE *ifile)
{
        char *all,*run;
        char name[1024],value[1024],sep;
        EITEM *item=NULL;
        int ivalue;

        run=all=load_ascii_file(ifile->filename,MEM_EDIT);
        if (!all) { fail("failed to open '%s'",ifile->filename); return; }

        note("loading \"%s\"",ifile->filename);

        ifile->loaded=1;

        while (*run) {
                run=parse_nsv(run,name,&sep,value,&ivalue);
		// note("got name=%s,sep=%c (%d),value=%s,ivalue=%d",name,sep,sep,value,ivalue);
                if (sep==':') {
                        if ((item=find_item(name))==NULL) {
                                item=xcalloc(sizeof(EITEM),MEM_EDIT);
                                item->ifile=ifile;
                                strncpy(item->uni,name,EMAXUNI-1);
                                note("created new item '%s'",item->uni);
                                addptr((void ***)&ifile->ilist,&ifile->icount,item,MEM_EDIT);
                        }
                        else {
                                note("found item '%s' in '%s'",item->uni,item->ifile->filename);
                        }
                }
                else if (sep==';') {
                        item=NULL;
                }
                else if (sep=='=' && item) {
                        if (!strcmp(name,"name")) strncpy(item->name,value,EMAXNAME-1);
                        else if (!strcmp(name,"description")) strncpy(item->desc,value,EMAXDESC-1);
                        else if (!strcmp(name,"sprite")) {
                                if (ivalue>100000) ivalue=ivalue-100000+64000;
                                item->sprite=ivalue;
                        }
                }
        }

        xfree(all);
}

void load_chr_file(CFILE *cfile)
{
        char *all,*run;
        char name[1024],value[1024],sep;
        ECHAR *chr=NULL;
        int ivalue;

        run=all=load_ascii_file(cfile->filename,MEM_EDIT);
        if (!all) { fail("failed to open '%s'",cfile->filename); return; }

        note("loading \"%s\"",cfile->filename);

        cfile->loaded=1;

        while (*run) {
                run=parse_nsv(run,name,&sep,value,&ivalue);
                if (sep==':') {
                        if ((chr=find_char(name))==NULL) {
                                chr=xcalloc(sizeof(ECHAR),MEM_EDIT);
                                chr->cfile=cfile;
                                strncpy(chr->uni,name,EMAXUNI-1);
                                // note("created new char '%s'",chr->uni);
                                addptr((void ***)&cfile->clist,&cfile->ccount,chr,MEM_EDIT);
                        }
                        else {
                                note("found char '%s' in '%s'",chr->uni,chr->cfile->filename);
                        }
                }
                else if (sep==';') {
                        chr=NULL;
                }
                else if (sep=='=' && chr) {
                         if (!strcmp(name,"name")) strncpy(chr->name,value,EMAXNAME-1);
                         else if (!strcmp(name,"description")) strncpy(chr->desc,value,EMAXDESC-1);
                         else if (!strcmp(name,"sprite")) chr->sprite=ivalue;
                }
        }

        xfree(all);
}

void load_map_file(MFILE *mfile)
{
        char *all,*run,*end;
        char name[1024],value[1024],sep;
        int mn=0,x,y,i;
        int ivalue;

        mfile->width=256;
        mfile->height=256;
        mfile->field=xcalloc(mfile->width*mfile->height*sizeof(MFIELD),MEM_EDIT);

        run=all=load_ascii_file(mfile->filename,MEM_EDIT);
        if (!all) { fail("failed to open '%s'",mfile->filename); return; }

        note("loading \"%s\"",mfile->filename);

        mfile->loaded=1;
        mfile->modified=0;

        while (*run) {
                run=parse_nsv(run,name,&sep,value,&ivalue);
                if (sep=='=') {
                        if (!strcmp(name,"field")) {
                                x=atoi(value);
                                if (strchr(value,',')) y=atoi(strchr(value,',')+1);
                                if (x>=0 && y>=0 && x<mfile->width && y<mfile->height) mn=x+y*mfile->width;
                        }
                        else if (!strcmp(name,"gsprite")) {
                                // if (ivalue>100000) ivalue=ivalue-100000+64000;
                                mfile->field[mn].sprite[0]=ivalue&0x0000FFFF;
                                mfile->field[mn].sprite[1]=(ivalue&0xFFFF0000)>>16;

                        }
                        else if (!strcmp(name,"fsprite")) {
                                // if (ivalue>100000) ivalue=ivalue-100000+64000;
                                mfile->field[mn].sprite[2]=ivalue&0x0000FFFF;
                                mfile->field[mn].sprite[3]=(ivalue&0xFFFF0000)>>16;
                        }
                        else if (!strcmp(name,"it")) mfile->field[mn].itm=find_item(value);
                        else if (!strcmp(name,"ch")) mfile->field[mn].chr=find_char(value);
                        else if (!strcmp(name,"flag")) {
                                for (i=0; i<MAX_EFLAG; i++) if (!strcmp(value,eflags[i].name)) mfile->field[mn].flags|=eflags[i].mask;
                        }
                }
        }
	
	/*
	// sewer duplication
	for (x=0; x<256; x++) {
		for (y=0; y<256; y++) {
			int src,lvl,n,dst;
			char buf[256],c;

			if (x>=0 && y>=0 && x<134 && y<119) continue;
			if (x>=134 && y>=0 && x<256 && y<134) continue;
			if (x>=119 && y>=119 && x<=134 && y<=134) continue;

			src=(255-x)+(255-y)*256;
			dst=x+y*256;

			mfile->field[dst]=mfile->field[src];
			
			if (mfile->field[dst].chr && strstr(mfile->field[dst].chr->uni,"ratling")) {
				lvl=atoi(mfile->field[dst].chr->uni+7);
				sprintf(buf,"ratling%d",lvl+8);
				mfile->field[dst].chr=find_char(buf);
			}
			if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"crt_mushroom")) {
				lvl=atoi(mfile->field[dst].itm->uni+12);
				sprintf(buf,"crt_mushroom%d",lvl+4);
				mfile->field[dst].itm=find_item(buf);
			}
			if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"sewer_crate")) {
				lvl=atoi(mfile->field[dst].itm->uni+11);
				c=mfile->field[dst].itm->uni[12];
				sprintf(buf,"sewer_crate%d%c",lvl+2,c);
				mfile->field[dst].itm=find_item(buf);
				note(buf);
			}
			for (n=0; n<4; n++) {
				if (mfile->field[dst].sprite[n]==22522) mfile->field[dst].sprite[n]=22524;
				else if (mfile->field[dst].sprite[n]==22524) mfile->field[dst].sprite[n]=22522;
				else if (mfile->field[dst].sprite[n]==22523) mfile->field[dst].sprite[n]=22525;
				else if (mfile->field[dst].sprite[n]==22525) mfile->field[dst].sprite[n]=22523;
				else if (mfile->field[dst].sprite[n]==22526) mfile->field[dst].sprite[n]=22527;
				else if (mfile->field[dst].sprite[n]==22527) mfile->field[dst].sprite[n]=22526;
				else if (mfile->field[dst].sprite[n]==22529) mfile->field[dst].sprite[n]=22528;
				else if (mfile->field[dst].sprite[n]==22528) mfile->field[dst].sprite[n]=22529;			
			}			
		}
	}*/

        /*// pents turn
	for (x=0; x<128; x++) {
		for (y=0; y<256; y++) {
			int src,dst;
			MFIELD field;

                        src=(255-x)+(y)*256;
			dst=x+y*256;

			field=mfile->field[dst];
			mfile->field[dst]=mfile->field[src];
			mfile->field[src]=field;
		}
	}*/

	/*// pent bugfix
	for (x=0; x<256; x++) {
		for (y=0; y<256; y++) {
			int dst;

                        if (y<128) continue;

			dst=x+y*256;
			
			
                        if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl8a")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl9a");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl8b")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl9b");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl8c")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl9c");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl9a")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl10a");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl9b")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl10b");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl9c")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl10c");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl10a")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl11a");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl10b")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl11b");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl10c")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl11c");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl11a")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl12a");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl11b")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl12b");
			} else if (mfile->field[dst].itm && strstr(mfile->field[dst].itm->uni,"fire_pentlvl11c")) {
                                mfile->field[dst].itm=find_item("fire_pentlvl12c");
			}
		}
	}*/

        xfree(all);
}

void save_map_file(MFILE *mfile)
{
        FILE *file;
        MFIELD *field;
        int x,y,i;

        file=fopen(mfile->filename,"w+b");
        if (!file) { addline("could not open output file: %s",strerror(errno)); return; }

        mfile->modified=0;

        for (y=0; y<mfile->height; y++) {
                for (x=0; x<mfile->width; x++) {
                        field=&mfile->field[x+y*mfile->width];
                        if (field->flags==0 && field->sprite[0]==0 && field->sprite[1]==0 && field->sprite[2]==0 && field->sprite[3]==0 && field->itm==NULL && field->chr==NULL) continue;

                        fprintf(file,"field=\"%d,%d\"\n",x,y);
                        if (field->sprite[0] || field->sprite[1]) fprintf(file,"gsprite=%d\n",field->sprite[0]|(field->sprite[1]<<16));
                        if (field->sprite[2] || field->sprite[3]) fprintf(file,"fsprite=%d\n",field->sprite[2]|(field->sprite[3]<<16));
                        if (field->itm) fprintf(file,"it=%s\n",field->itm->uni);
                        if (field->chr) fprintf(file,"ch=%s\n",field->chr->uni);
                        for (i=0; i<MAX_EFLAG; i++) if (field->flags&eflags[i].mask) fprintf(file,"flag=%s\n",eflags[i].name);
                        /*
                        if (field->flags&MF_MOVEBLOCK) fprintf(file,"flag=MF_MOVEBLOCK\n");
                        if (field->flags&MF_SIGHTBLOCK) fprintf(file,"flag=MF_SIGHTBLOCK\n");
                        if (field->flags&MF_INDOORS) fprintf(file,"flag=MF_INDOORS\n");
                        if (field->flags&MF_RESTAREA) fprintf(file,"flag=MF_RESTAREA\n");
			if (field->flags&MF_SOUNDBLOCK) fprintf(file,"flag=MF_SOUNDBLOCK\n");
			if (field->flags&MF_SHOUTBLOCK) fprintf(file,"flag=MF_SHOUTBLOCK\n");
                        */
                        fprintf(file,"\n");
                }
        }

        fclose(file);
}

void free_map_file(MFILE *mfile)
{
        mfile->loaded=0;
        xfree(mfile->field);
}

void mfile_inc_undo(MFILE *mfile)
{
        if (!mfile) return;
        if (mfile->uend==0) mfile->uid=0;
        else mfile->uid=mfile->mundo[mfile->uend-1].uid+1;
}

void mfile_set_undo(MFILE *mfile, int sel)
{
        int x,y;

        if (!mfile || sel==-1) return;

        x=sel%mfile->width;
        y=sel/mfile->width;
        if (x<0 || y<0 || x>=mfile->width || y>=mfile->height) return;

        if (mfile->uend==mfile->umax) mfile->mundo=xrealloc(mfile->mundo,(mfile->umax+=64*64)*sizeof(*mfile->mundo),MEM_EDIT);

        mfile->mundo[mfile->uend].uid=mfile->uid;
        mfile->mundo[mfile->uend].x=x;
        mfile->mundo[mfile->uend].y=y;
        memcpy(&mfile->mundo[mfile->uend].field,&mfile->field[sel],sizeof(MFIELD));

        mfile->uend++;
}

void mfile_use_undo(MFILE *mfile)
{
        int uid,sel;

        if (!mfile) return;
        if (mfile->uend==0) return;

        uid=mfile->mundo[mfile->uend-1].uid;

        while (mfile->uend && mfile->mundo[mfile->uend-1].uid==uid) {
                if (mfile->mundo[mfile->uend-1].x>=mfile->width || mfile->mundo[mfile->uend-1].y>=mfile->height) continue;
                sel=mfile->mundo[mfile->uend-1].x+mfile->mundo[mfile->uend-1].y*mfile->width;
                memcpy(&mfile->field[sel],&mfile->mundo[mfile->uend-1].field,sizeof(MFIELD));
                mfile->uend--;
        }
}

int get_itype(char *str)
{
        if (!str) return 0;

        if (!strcmp(str,"GROUND")) return ITYPE_GROUND;
        if (!strcmp(str,"LINEWALL")) return ITYPE_LINEWALL;
        if (!strcmp(str,"CHANCE")) return ITYPE_CHANCE;
        if (!strcmp(str,"PRESET")) return ITYPE_PRESET;

        return 0;
}

char *get_iname(int type)
{
        switch (type) {
                case ITYPE_GROUND:      return "GROUND";
                case ITYPE_LINEWALL:    return "LINEWALL";
                case ITYPE_CHANCE:      return "CHANCE";
                case ITYPE_PRESET:      return "PRESET";
        }

        return "";
}

// intelligence

void load_pre_file(PFILE *pfile)
{
        char *all,*run;
        char name[1024],value[1024],sep;
        ITYPE *nint=NULL;
        int ivalue;
        int reset=0,spritecnt,probcnt,spritemax,i;

        run=all=load_ascii_file(pfile->filename,MEM_EDIT);
        if (!all) {
                fail("failed to open '%s'",pfile->filename); return;
        }

        note("loading \"%s\"",pfile->filename);

        pfile->loaded=1;

        while (*run) {
                run=parse_nsv(run,name,&sep,value,&ivalue);
                if (sep==':') {
                        nint=xcalloc(sizeof(ITYPE),MEM_EDIT);
                        nint->generic.type=get_itype(name);
                        addptr((void ***)&pfile->plist,&pfile->pcount,nint,MEM_EDIT);
                        reset=1;
                }
                else if (sep==';') {
                        nint=NULL;
                        reset=1;
                }
                else if (sep=='=' && nint) {
                        if (!strcmp(name,"name")) {
                                strncpy(nint->generic.name,value,EMAXNAME-1);
                        }
                        else if (!strcmp(name,"affect")) nint->generic.affect=ivalue;
                        else if (!strcmp(name,"setflag")) {
                                for (i=0; i<MAX_EFLAG; i++) {
                                        if (!strcmp(value,eflags[i].name)) {
                                                nint->generic.emode[EMODE_FLAG+i]=1;
                                                nint->generic.flags|=eflags[i].mask;
                                        }
                                }
                        }
                        else if (!strcmp(name,"clrflag")) {
                                for (i=0; i<MAX_EFLAG; i++) {
                                        if (!strcmp(value,eflags[i].name)) {
                                                nint->generic.emode[EMODE_FLAG+i]=1;
                                        }
                                }
                        }
                        else {
                                switch (nint->generic.type) {
                                        case ITYPE_GROUND:
                                                if (!strcmp(name,"gridx")) nint->ground.gridx=ivalue;
                                                else if (!strcmp(name,"gridy")) nint->ground.gridy=ivalue;
                                                else if (!strcmp(name,"sprite")) {
                                                        if (spritemax==0) nint->ground.sprite=xrecalloc(nint->ground.sprite,(spritemax=nint->ground.gridx*nint->ground.gridy)*sizeof(int),MEM_EDIT);
                                                        if (spritecnt>=spritemax) paranoia("load_int_file(): spritecount exeeds limit (%s,%s)",pfile->filename,nint->generic.name);
                                                        nint->ground.sprite[spritecnt++]=ivalue;
                                                }
                                                break;
                                        case ITYPE_LINEWALL:
                                                if (!strcmp(name,"len")) nint->linewall.len=ivalue;
                                                else if (!strcmp(name,"sprite")) {
                                                        if (spritemax==0) nint->linewall.sprite=xrecalloc(nint->linewall.sprite,(spritemax=nint->linewall.len)*sizeof(int),MEM_EDIT);
                                                        if (spritecnt>=spritemax) paranoia("load_int_file(): spritecount exeeds limit (%s,%s)",pfile->filename,nint->generic.name);
                                                        nint->linewall.sprite[spritecnt++]=ivalue;
                                                }
                                                break;
                                        case ITYPE_CHANCE:
                                                if (!strcmp(name,"len")) nint->chance.len=ivalue;
                                                else if (!strcmp(name,"sprite") || !strcmp(name,"prob")) {
                                                        if (spritemax==0) {
                                                                nint->chance.sprite=xrecalloc(nint->chance.sprite,(spritemax=nint->chance.len)*sizeof(int),MEM_EDIT);
                                                                nint->chance.prob=xrecalloc(nint->chance.prob,(spritemax+1)*sizeof(int),MEM_EDIT);
                                                                for (i=0; i<nint->chance.len; i++) {
                                                                        nint->chance.prob[i]=100;
                                                                        nint->chance.probsum+=100;
                                                                }
                                                        }
                                                        if (!strcmp(name,"sprite")) {
                                                                if (spritecnt>=spritemax) paranoia("load_int_file(): spritecount exeeds limit (%s,%s)",pfile->filename,nint->generic.name);
                                                                nint->chance.sprite[spritecnt++]=ivalue;
                                                        } else {
                                                                if (probcnt>=spritemax) paranoia("load_int_file(): probcount exeeds limit (%s,%s)",pfile->filename,nint->generic.name);
                                                                nint->chance.probsum-=nint->chance.prob[probcnt];
                                                                nint->chance.prob[probcnt]=ivalue;
                                                                nint->chance.probsum+=nint->chance.prob[probcnt];
                                                                probcnt++;
                                                        }
                                                }
                                                break;
					case ITYPE_PRESET:
						nint->preset.field.flags=nint->generic.flags;
                                                if (!strcmp(name,"gsprite")) nint->preset.field.sprite[0]=ivalue;
                                                else if (!strcmp(name,"gsprite2")) nint->preset.field.sprite[1]=ivalue;
                                                else if (!strcmp(name,"fsprite")) nint->preset.field.sprite[2]=ivalue;
                                                else if (!strcmp(name,"fsprite2")) nint->preset.field.sprite[3]=ivalue;
                                                else if (!strcmp(name,"it")) nint->preset.field.itm=find_item(value);
                                                else if (!strcmp(name,"ch")) nint->preset.field.chr=find_char(value);
                                                break;
                                }

                                // emodes
                                if (!strcmp(name,"gsprite"))            nint->generic.emode[EMODE_SPRITE+0]=1;
                                else if (!strcmp(name,"gsprite2"))      nint->generic.emode[EMODE_SPRITE+1]=1;
                                else if (!strcmp(name,"fsprite"))       nint->generic.emode[EMODE_SPRITE+2]=1;
                                else if (!strcmp(name,"fsprite2"))      nint->generic.emode[EMODE_SPRITE+3]=1;
                                else if (!strcmp(name,"it"))            nint->generic.emode[EMODE_ITM]=1;
                                else if (!strcmp(name,"ch"))            nint->generic.emode[EMODE_CHR]=1;

                        }
                }

                if (reset) {
                        spritemax=spritecnt=probcnt=0;
                        reset=0;
                }
        }

        xfree(all);
}

void free_pre_files(void)
{
        int f,i;
        ITYPE *nint;

        for (f=0; f<pfilecount; f++) {
                // note("del %s",nfilelist[f]->filename);
                for (i=0; i<pfilelist[f]->pcount; i++) {
                        nint=pfilelist[f]->plist[i];
                        // note("del nint %s (%d)",nint->generic.name,nint->generic.type);
                        switch (nint->generic.type) {
                                case ITYPE_GROUND:
                                        xfree(nint->ground.sprite);
                                        break;
                                case ITYPE_LINEWALL:
                                        xfree(nint->linewall.sprite);
                                        break;
                                case ITYPE_CHANCE:
                                        xfree(nint->chance.sprite);
                                        break;
                        }
                        xfree(nint);
                }
                xfree(pfilelist[f]->plist);
                xfree(pfilelist[f]);
        }
        xfree(pfilelist);
        pfilelist=NULL;
        pfilecount=0;
}

// preset

/*

void load_pre_file(PFILE *pfile)
{
        char *all,*run;
        char name[1024],value[1024],sep;
        int ivalue,i;
        PRESET *preset=NULL;
        int preset_max=0;

        run=all=load_ascii_file(pfile->filename,MEM_EDIT);
        if (!all) { fail("failed to open '%s'",pfile->filename); return; }

        note("loading \"%s\"",pfile->filename);

        pfile->loaded=1;

        while (*run) {
                run=parse_nsv(run,name,&sep,value,&ivalue);
                if (sep==':') {
                        // PRESET:
                        if (pfile->pcount==preset_max) pfile->preset_list=xrecalloc(pfile->preset_list,(preset_max+=128)*sizeof(PRESET),MEM_EDIT);
                        pfile->pcount++;
                        preset=&pfile->preset_list[pfile->pcount];
                }
                else if (sep=='=' && preset) {
                         if (!strcmp(name,"name")) {
                                 strncpy(preset->name,value,EMAXNAME-1);
                                 // note("preset[%d]=%s",pfile->pcount,preset->name);
                         }
                         else if (!strcmp(name,"setflag")) {
                                 for (i=0; i<MAX_EFLAG; i++) if (!strcmp(value,eflags[i].name)) {
                                         preset->emode[EMODE_FLAG+i]=1;
                                         preset->field.flags|=eflags[i].mask;
                                 }
                         }
                         else if (!strcmp(name,"clrflag")) {
                                 for (i=0; i<MAX_EFLAG; i++) if (!strcmp(value,eflags[i].name)) {
                                         preset->emode[EMODE_FLAG+i]=1;
                                 }
                         }
                         else if (!strcmp(name,"gsprite")) {
                                preset->emode[EMODE_SPRITE+0]=1;
                                preset->field.sprite[0]=ivalue;
                         }
                         else if (!strcmp(name,"gsprite2")) {
                                preset->emode[EMODE_SPRITE+1]=1;
                                preset->field.sprite[1]=ivalue;
                         }
                         else if (!strcmp(name,"fsprite")) {
                                preset->emode[EMODE_SPRITE+2]=1;
                                preset->field.sprite[2]=ivalue;
                         }
                         else if (!strcmp(name,"fsprite2")) {
                                preset->emode[EMODE_SPRITE+3]=1;
                                preset->field.sprite[3]=ivalue;
                         }
                }
        }

        xfree(all);

        xrealloc(pfile->preset_list,pfile->pcount*sizeof(PRESET),MEM_EDIT);
}

void free_pre_files(void)
{
        int i;
        for (i=0; i<pfilecount; i++) {
                xfree(pfilelist[i]->preset_list);
                xfree(pfilelist[i]);
        }
        xfree(pfilelist);
        pfilelist=NULL;
        pfilecount=0;
}

*/

#endif
