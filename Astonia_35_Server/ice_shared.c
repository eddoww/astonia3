/*

$Id: ice_shared.c,v 1.1 2005/09/24 09:48:53 ssim Exp $

$Log: ice_shared.c,v $
Revision 1.1  2005/09/24 09:48:53  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:14  sam
Added RCS tags


*/

void itemspawn(int in,int cn)
{
        int in2;

	if (!cn) return;	// always make sure its not an automatic call if you don't handle it

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}

        // get item to spawn
        switch(it[in].drdata[0]) {
		case 0:		in2=create_item("melting_key"); break;
		case 1:		in2=create_item("ice_boots1"); break;
		case 2:		in2=create_item("ice_cape1"); break;
		case 3:		in2=create_item("ice_belt1"); break;
		case 4:		in2=create_item("ice_ring1"); break;
		case 5:		in2=create_item("ice_amulet1"); break;
		case 6:		in2=create_item("melting_key2"); break;

		case 7:		in2=create_item("ice_boots2"); break;
		case 8:		in2=create_item("ice_cape2"); break;
		case 9:		in2=create_item("ice_belt2"); break;
		case 10:	in2=create_item("ice_ring2"); break;
		case 11:	in2=create_item("ice_amulet2"); break;

		case 12:	in2=create_item("ice_boots3"); break;
		case 13:	in2=create_item("ice_cape3"); break;
		case 14:	in2=create_item("ice_belt3"); break;
		case 15:	in2=create_item("ice_ring3"); break;
		case 16:	in2=create_item("ice_amulet3"); break;
		case 17:	in2=create_item("palace_bomb"); break;
		case 18:	in2=create_item("palace_cap"); break;

		default:	
				log_char(cn,LOG_SYSTEM,0,"Congratulations, %s, you have just discovered bug #4244B-%d-%d, please report it to the authorities!",ch[cn].name,it[in].x,it[in].y);
				return;
	}

        if (!in2) {
		log_char(cn,LOG_SYSTEM,0,"Congratulations, %s, you have just discovered bug #4244C-%d-%d, please report it to the authorities!",ch[cn].name,it[in].x,it[in].y);
		return;			
	}

	if (!can_carry(cn,in2,0)) {
		destroy_item(in2);
		return;
	}

	if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from ice itemspawn");
	ch[cn].citem=in2;
	ch[cn].flags|=CF_ITEMS;
	it[in2].carried=cn;

        log_char(cn,LOG_SYSTEM,0,"You got a %s.",it[in2].name);
}

void warmfire(int in,int cn)
{
	int in2,fn,n;

	if (!cn) return;

	if (ch[cn].citem) {
		log_char(cn,LOG_SYSTEM,0,"Please empty your 'hand' (mouse cursor) first.");
		return;
	}
	if (!it[in].drdata[0]) {
		in2=create_item("ice_scroll");
		if (in2) {
			if (ch[cn].flags&CF_PLAYER) dlog(cn,in2,"took from warmfire");
			ch[cn].citem=in2;
			it[in2].carried=cn;
			ch[cn].flags|=CF_ITEMS;
			it[in2].drdata[0]=ch[cn].x;
			it[in2].drdata[1]=ch[cn].y;
	
			log_char(cn,LOG_SYSTEM,0,"Next to the fire, you find an ancient scroll. It seems to be a scroll of teleport which will take you back here.");
		}
	}

	for (n=12; n<30; n++) {
		if ((in2=ch[cn].item[n]) && it[in2].driver==IDR_CURSE) {
			destroy_item(in2);
			ch[cn].item[n]=0;
			break;
		}
	}
	if (n==30) {
		log_char(cn,LOG_SYSTEM,0,"You warm your hands on the fire.");
		return;
	}
	for (n=0; n<4; n++) {
		if ((fn=ch[cn].ef[n]) && ef[fn].type==EF_CURSE) {
			remove_effect_char(fn);
			free_effect(fn);
		}
	}
	update_char(cn);
	log_char(cn,LOG_SYSTEM,0,"You move close to the heat of the fire, and you feel the demon's cold leave you.");
}

void backtofire(int in,int cn)
{
	if (!cn) return;
	if (!it[in].carried) return;	// can only use if item is carried
	
	if (teleport_char_driver(cn,it[in].drdata[0],it[in].drdata[1])) {
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it was used");
		remove_item_char(in);
		destroy_item(in);
		log_char(cn,LOG_SYSTEM,0,"The scroll vanished.");
	}
}

void meltingkey(int in,int cn)
{
	int sprite;

        if (cn) return;
	if (!it[in].carried) return;	// can only use if item is carried
	
	it[in].drdata[1]++;
	if (it[in].drdata[1]>=it[in].drdata[0]) {
		if (it[in].carried) log_char(it[in].carried,LOG_SYSTEM,0,"Your %s melted away.",it[in].name);
		if (ch[cn].flags&CF_PLAYER) dlog(cn,in,"dropped because it melted");
		remove_item(in);
		destroy_item(in);
		return;
	}
	sprite=50494+it[in].drdata[1]*5/it[in].drdata[0];

	if (it[in].sprite!=sprite) {
		it[in].sprite=sprite;
		if (it[in].carried) {
			ch[it[in].carried].flags|=CF_ITEMS;
			if (sprite==50495) {
				log_char(it[in].carried,LOG_SYSTEM,0,"Your %s starts to melt.",it[in].name);
			}
		}
	}
			
	call_item(it[in].driver,in,0,ticker+TICKS*10);
}

void freakdoor(int in,int cn)
{
	int me,you,in2;

	if (!cn) return;
	
	// avoid endless recursion
	if (it[in].drdata[9]) return;

	me=it[in].drdata[8];
	if (it[in].drdata[14]) me=0;	// one-way freaks
	
	if (me) {
		you=*(unsigned int*)(it[in].drdata+10);
	
		if (!you) {
			for (in2=1; in2<MAXITEM; in2++) {
				if (!it[in2].flags) continue;
				if (it[in2].driver!=IDR_FREAKDOOR) continue;
				if (in2==in) continue;
				if (it[in2].drdata[15]) continue;	// no-target freak
				if (it[in2].drdata[8]==me) break;
			}
			if (in2==MAXITEM) {
				elog("PANIC: freakdoor %d at %d,%d: partner not found",me,it[in].x,it[in].y);
				return;
			}
			you=*(unsigned int*)(it[in].drdata+10)=in2;
		}
	} else you=in;

	//log_char(cn,LOG_SYSTEM,0,"Door %d (%d/%d)",me,in,you);

	if (it[in].x!=ch[cn].x || it[in].y!=ch[cn].y) {
		item_driver(IDR_DOOR,in,cn);
		// open the other door if it is closed and our door was opened
		if (you!=in && it[in].drdata[0] && !it[you].drdata[0]) item_driver(IDR_DOOR,you,cn);
	} else if (in!=you) {
		int dx,dy,px,py;

		// open the other door if it is still closed
		if (!it[you].drdata[0]) item_driver(IDR_DOOR,you,cn);

		if (player_driver_get_move(cn,&px,&py)) {
			dx=px-ch[cn].x;
			dy=py-ch[cn].y;
		} else dx=dy=0;

		it[you].drdata[9]=1;	// set flag: no 2nd jump
		teleport_char_driver(cn,it[you].x,it[you].y);
		it[you].drdata[9]=0;

		if (dx || dy) {
			player_driver_fake_move(cn,ch[cn].x+dx,ch[cn].y+dy);
		}
	}
}

