/*
 *  $Id: book.c,v 1.2 2007/06/28 12:10:52 devel Exp $
 *
 * $Log: book.c,v $
 * Revision 1.2  2007/06/28 12:10:52  devel
 * changed superior zombie names
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "log.h"
#include "notify.h"
#include "direction.h"
#include "do.h"
#include "path.h"
#include "error.h"
#include "drdata.h"
#include "see.h"
#include "death.h"
#include "talk.h"
#include "effect.h"
#include "database.h"
#include "map.h"
#include "create.h"
#include "container.h"
#include "drvlib.h"
#include "tool.h"
#include "spell.h"
#include "effect.h"
#include "light.h"
#include "date.h"
#include "los.h"
#include "command.h"
#include "player.h"
#include "book.h"

// library helper functions needed for init
int ch_driver(int nr,int cn,int ret,int lastact);	// character driver (decides next action)
int it_driver(int nr,int in,int cn);					// item driver (special cases for use)
int ch_died_driver(int nr,int cn,int co);				// called when a character dies

// EXPORTED - character/item driver
int driver(int type,int nr,int obj,int ret,int lastact)
{
	switch(type) {
		case 0:		return ch_driver(nr,obj,ret,lastact);
		case 1: 	return it_driver(nr,obj,ret);
		case 2:		return ch_died_driver(nr,obj,ret);
		default: 	return 0;
	}
}

void book_driver(int in,int cn)
{
	char buf[80];

	if (!cn) return;

	switch(it[in].drdata[0]) {
		case BOOK_LOISAN1:
			log_char(cn,LOG_SYSTEM,0,"The magical properties of these skulls are astonishing. They are the artifacts the various shrines of the ancients accept, they can also be used to animate skeletons.");
			log_char(cn,LOG_SYSTEM,0,"After I told Moakin about them he used some magical stones to enhance the skulls and he created a small army of skeletons. Too bad his hunger for power made him go away without sharing his secrets with me.");
			log_char(cn,LOG_SYSTEM,0,"I wonder what became of him, and his puppet, Dorugin. But I digress. Now that Moakin has left, I will have to find out how to control the undead created with these skulls.");
			log_char(cn,LOG_SYSTEM,0,"My experiments have been successful in raising skeletons and zombies. A single plain skull can be used to create about a dozen of them. I wonder what one of the rare silver skulls would do.");
			log_char(cn,LOG_SYSTEM,0,"But I still have to control my creations. How Moakin managed to do that escapes me. I have tried various potions on those fools in Cameron to understand how to control a mind, but to no avail.");
			log_char(cn,LOG_SYSTEM,0,"It seems alchemy is worthless when it comes to control. I will have to resort to magical jewels. But those are hard to find. Maybe one of the shrines will produce some?");
                        return;
		
		case BOOK_LOISAN2:
			log_char(cn,LOG_SYSTEM,0,"Healing Potions. Mana Potions. Torches. Small magical effects. Plain skulls are worthless. Must try the silver ones.");
			log_char(cn,LOG_SYSTEM,0,"Ahh. These zombies are dangerous. But they shall not stop me, Loisan. No luck with silver skulls either.");
			log_char(cn,LOG_SYSTEM,0,"Must try to get golden ones. But the danger...");
			log_char(cn,LOG_SYSTEM,0,"I am dying. So close. Oh, how cruel.");
                        return;
		
		case BOOK_SUPERIOR1:
			log_char(cn,LOG_SYSTEM,0,"Day 122, year 48, morning by outside time. Personal diary of Ioslan of the Cerasa.");
			log_char(cn,LOG_SYSTEM,0,"We had to retreat further into the tunnels. The enemy is sending a new type of monster. Our creatures fight valiantly, but they cannot withstand them for long. We will have to flee, or we will perish.");
			log_char(cn,LOG_SYSTEM,0,"Armenicon has created more powerful creatures, but they fail to recognize us. Therefore, Armenicon added keywords to them, which will stun them for a short time, allowing us to flee from them.");
			log_char(cn,LOG_SYSTEM,0,"Once they are released, we will leave this part of the tunnel system and hope our enemy will invade and die. Today it is my turn to sneak through the tunnels and collect the skulls of our creatures.");
			log_char(cn,LOG_SYSTEM,0,"The enemy still has not learned the value of them. I just hope I will survive to flee with my kin.");
                        return;

		case BOOK_SUPERIOR2:
			log_char(cn,LOG_SYSTEM,0,"Specimen 33. Prototype 4. Keyword: Kazimah.");
			log_char(cn,LOG_SYSTEM,0,"I will send this creature to guard the huge cavern. We cannot prevent the enemy from taking our storage room, but we can make him pay dearly for it.");
                        return;

		case BOOK_SUPERIOR3:
			log_char(cn,LOG_SYSTEM,0,"Specimen 35. Prototype 4. Keyword: Ergatoth.");
			log_char(cn,LOG_SYSTEM,0,"Another guard for our storage room.");

			return;

		case BOOK_SUPERIOR4:
			log_char(cn,LOG_SYSTEM,0,"Day 122, year 48, evening by outside time. Personal diary of Armenicon of the Cerasa.");
			log_char(cn,LOG_SYSTEM,0,"Ioslan has not returned. We cannot tell if he managed to recharge the spawners or not. We must flee immediately. The enemy will attack very soon.");
                        return;

		case BOOK_SUPERIOR5:
			log_char(cn,LOG_SYSTEM,0,"Specimen 34. Prototype 4. Keyword: Norganoth.");
			log_char(cn,LOG_SYSTEM,0,"Good. Prototype 4 is very difficult to create, but extremely powerful. This creature is to guard the storage room.");
                        return;

		case BOOK_SUPERIOR6:
			log_char(cn,LOG_SYSTEM,0,"Specimen 36. Prototype 4. Keyword: Larkanoth.");
			log_char(cn,LOG_SYSTEM,0,"The last prototype 4 for the storage room. These creatures are deadly.");
                        return;

		case BOOK_VAMPIRE1:
			log_char(cn,LOG_SYSTEM,0,"There are two kinds of vampires. One is known under varying names, such as 'Vampire', 'Lesser Vampire', 'Dracul' or 'Necrifah'.");
                        log_char(cn,LOG_SYSTEM,0,"Of the other kind, only a few sources report. They are called 'Vampire Lords' or 'Methusalah'.");
			log_char(cn,LOG_SYSTEM,0,"Killing a Lesser Vampire is as simple as penetrating it with a sword, or frying it with magic. They possess the abilities of the human they were once, but not much more.");
			log_char(cn,LOG_SYSTEM,0,"But killing a Vampire Lord on the other hand is very difficult, since each of them only has one weakness. Discovering that weakness is of utmost importance.");
			log_char(cn,LOG_SYSTEM,0,"Even if the weakness is known, it will still be a hard battle, as Vampire Lords are extremely old and powerful.");
			return;

		case BOOK_VAMPIRE2:
			log_char(cn,LOG_SYSTEM,0,"In a vision, I saw a sun shine in the darkness, and I saw fear in the eyes of the Lord.");
			log_char(cn,LOG_SYSTEM,0,"But then the sun was shattered, and parts of it fell into the dark. The Lord took them, and hid them in His lair.");
			log_char(cn,LOG_SYSTEM,0,"Then I saw Him leave His crypt, and come for me.");
			return;

		case BOOK_VAMPIRE3:
			log_char(cn,LOG_SYSTEM,0,"One among many, one pointing sideways, part you shall find there. Cross I shall be with thee, shouldst thou fail.");
			return;

		case BOOK_VAMPIRE4:
			log_char(cn,LOG_SYSTEM,0,"'And,' said the wise, 'If ye are burning, my pupil, what shall ye do?'");
			log_char(cn,LOG_SYSTEM,0,"'Extinguish the flames, master?'");
			return;

		case BOOK_VAMPIRE5:
			log_char(cn,LOG_SYSTEM,0,"Take heed, and go no further! This way leads to the Vampire Lord!");
			log_char(cn,LOG_SYSTEM,0,"It is said that one strike with the right dagger will kill the Lord. But alas, many have tried, but no one found the right dagger.");
			return;

		case BOOK_DEMON1:
			demonspeak(cn,0,buf);
			log_char(cn,LOG_SYSTEM,0,"I have seen in written in fiery letters upon the sky: Those who have the knowledge can invoke protection against demonic might by uttering the words: '%s'",buf);
			return;
		case BOOK_DEMON2:
			demonspeak(cn,1,buf);
			log_char(cn,LOG_SYSTEM,0,"Those who need better protection against earth demons, those who have the knowledge, use these words: '%s'",buf);
			return;
		case BOOK_DEMON3:
			demonspeak(cn,2,buf);
			log_char(cn,LOG_SYSTEM,0,"'%s' will give thee even better protection.",buf);
			return;
		case BOOK_DEMON4:
			demonspeak(cn,3,buf);
			log_char(cn,LOG_SYSTEM,0,"'%s' will give thee even better protection.",buf);
			return;
		case BOOK_DEMON5:
			demonspeak(cn,4,buf);
			log_char(cn,LOG_SYSTEM,0,"'%s' will give thee even better protection.",buf);
			return;
		
		case SIGN_EDEMON1:
			if (ch[cn].value[1][V_DEMON]<1) {
				log_char(cn,LOG_SYSTEM,0,"It's written in strange letters you cannot read.");
				return;
			}
			if (ch[cn].value[1][V_DEMON]<2) {
				log_char(cn,LOG_SYSTEM,0,"You recognice some of the letters used in this sign from your studies of the ancient knowledge, but you cannot tell what the sign means.");
				return;
			}
                        log_char(cn,LOG_SYSTEM,0,"Defense Systems Control Room");
                        return;
		
		case SIGN_EDEMON2:
			if (ch[cn].value[1][V_DEMON]<1) {
				log_char(cn,LOG_SYSTEM,0,"It's written in strange letters you cannot read.");
				return;
			}
			if (ch[cn].value[1][V_DEMON]<2) {
				log_char(cn,LOG_SYSTEM,0,"You recognice some of the letters used in this sign from your studies of the ancient knowledge, but you cannot tell what the sign means.");
				return;
			}
                        log_char(cn,LOG_SYSTEM,0,"Research Laboratorium");
			log_char(cn,LOG_SYSTEM,0,"Caution, live demons!");
			return;
		
		case BOOK_EDEMON1:
			log_char(cn,LOG_SYSTEM,0,"Day 91, year 97, evening by outside time. Personal diary of Avaisor of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"The struggle seems hopeless now. We're trapped in these caverns by our own defense systems. We can no longer control them as the key was lost when Daoslan was slain by demons in the southern part of the natural caverns.");
			log_char(cn,LOG_SYSTEM,0,"Our desperate attempts to raise demons for our defense have failed so far. Some of the research labs had to be closed since the demons in them could no longer be controlled.");
                        return;

		case BOOK_EDEMON2:
			log_char(cn,LOG_SYSTEM,0,"Day 58, year 97, memo on the state of War by Seraios of the Isara");
			log_char(cn,LOG_SYSTEM,0,"Only one adversary remains after the glorious defeat of Keriaos. But it is a dangerous one. Islena has persuaded four of our enemies to join forces with her, and she will gather all her allies in the north to form an army capable of destroying us.");
			log_char(cn,LOG_SYSTEM,0,"We must make our move first and attack before she is ready. I advise that we attack Islena's headquarter with...");
			log_char(cn,LOG_SYSTEM,0,"(The remaining pages are burned.)");
                        return;

		case BOOK_EDEMON3:
			log_char(cn,LOG_SYSTEM,0,"Day 84, year 97, evening by outside time. Personal diary of Delasar of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"I have established two outposts beyond our defense line to the north. They will allow me to study the demons as they are attacking our defense systems. I might be able to find other means to protect us this way.");
			log_char(cn,LOG_SYSTEM,0,"Going there is dangerous, and I might not make it back with my knowledge. I will keep other diaries there, so that my clan will be able to use my findings even after my death.");
			log_char(cn,LOG_SYSTEM,0,"I have asked Avaisor to turn off the defense systems in an hour so that I can reach my outpost. Fate, let me survive!");
			player_special(cn,0,50287,0);
                        return;

		case BOOK_EDEMON4:
			log_char(cn,LOG_SYSTEM,0,"Day 55, year 97, evening by outside time. Personal diary of Isranor of the Cerasa.");
			log_char(cn,LOG_SYSTEM,0,"Our glorious leader, Carisar, has joined forces with Islena of the Ilasner. Our talks with our direct enemy the Isara have failed. Too much blood was shed already and neither they nor we could overcome the hatred. But still, I was impressed by Ishtar, their leader.");
			log_char(cn,LOG_SYSTEM,0,"In spite of our alliance with the Ilasner, our position here is quite hopeless. The Isara will soon be forced to attack with all their might. Our defenses cannot withstand them for long. We will abandon this position within the next few days.");
			log_char(cn,LOG_SYSTEM,0,"Fortunately, we made good progress with our demonic research project. We will not suffer much from the demons that escaped during the early stages of our research. And we can hope that they will delay the Isara's pursuit.");
			log_char(cn,LOG_SYSTEM,0,"We will open the demon-gate before we leave. The steady flood of demons coming from it will give us the time we need and hinder the Isara.");
			player_special(cn,0,50305,0);
                        return;

		case BOOK_IDEMON1:
			log_char(cn,LOG_SYSTEM,0,"Day 155, year 103. Personal diary of Kamaleon of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"In our pursuit of Islena's forces we have finally reached one of her former settlements. The long march north, first through that fiery maze full of lava and unmanned yet dangerous defense stations and now through these icy caverns has tired us beyond measure.");
			log_char(cn,LOG_SYSTEM,0,"So many friends lost, so many deaths. And yet we must press on after only a few days rest, lest we give Islena time to counter-attack and crush us while we are defenseless. I wonder how far this pursuit will take us. We have come so far north in these caverns, we must be below the sea already.");
                        log_char(cn,LOG_SYSTEM,0,"But we will not stop, it would mean death. Mind, be tranquil, this will end.");
                        return;

		case BOOK_IDEMON2:
			log_char(cn,LOG_SYSTEM,0,"Day 158, year 103. Personal diary of Ileanor, Lieutenant of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"The three days rest we have given our men are all the time we can spare. Not all wounds are healed, and the men are still tired, but delaying further would leave us open to a counter-attack. I wonder what the Ilasner are up to. It is not like them to give up this much ground without any resistance.");
			log_char(cn,LOG_SYSTEM,0,"Tomorrow at dawn, well, tomorrow when we wake up, we will move on. We still have some wood to build fires to break the ice demon's spell, and the morale is as well as can be expected under these circumstances. I am greatly worried, though. We haven't seen the surface for years, and all the explosions we heard a few weeks ago mean the war is raging there as savage as it is here.");
                        return;

		case BOOK_IDEMON3:
			log_char(cn,LOG_SYSTEM,0,"Day 145, year 103. Personal diary of Cari-Maar of the Ilasner.");
			log_char(cn,LOG_SYSTEM,0,"Today we were ordered to retreat. The defense stations and the fire demons have delayed our pursuers long enough. Rumor has it that Islena and the main force have established a defensive position further to the north.");
			log_char(cn,LOG_SYSTEM,0,"But our scouts report that those cursed Isara have managed to bring half their forces through alive. We are vastly outnumbered. We can only hope that the fortified positions will give us enough advantage to make up for our lack in numbers.");
			return;

		case BOOK_SWAMP:
			log_char(cn,LOG_SYSTEM,0,"Contrary to my original belief, the swamp beasts possess no intelligence. The buildings they inhabitate must have been built by a now extinct people. I assume that the three stone circles have been built by the same people.");
			log_char(cn,LOG_SYSTEM,0,"Some pages later: I have discovered old drawings, showing humans fighting against swamp beasts. In the first pictures the humans flee from a huge beast. A bit further down, one of the drawings shows a human warrior standing in the center of a stone circle, holding a weapon in his hand. Strangely, it shows the sun being exactly below the warrior and the ground. The warrior seems to be waiting, and looking at the sun. In the next drawing, he is still standing in the stone circle, but now he is killing a small swamp beast. His weapon seems to be glowing.");
			return;

		case BOOK_PALACE1:
			log_char(cn,LOG_SYSTEM,0,"Day 172, year 103. Personal diary of Cari-Maar of the Ilasner.");
			log_char(cn,LOG_SYSTEM,0,"Today we finished raising the demon lord for the trap we've built for the Isara. Let them come now, they are doomed.");
			return;
		
		case BOOK_PALACE2:
			log_char(cn,LOG_SYSTEM,0,"Day 175, year 103. Personal diary of Ileanor, Lieutenant of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"Dead. All dead. Only Ishtar and I survived the storm of demon lords the Ilasner raised. We could flee, but we are locked into these rooms. The demon lords cannot enter, but they have begun to invoke the icy cold. We will freeze to death.");
                        log_char(cn,LOG_SYSTEM,0,"Day 177, year 103. Personal diary of Ileanor, Lieutenant of the Isara.");
			log_char(cn,LOG_SYSTEM,0,"The cold is creeping into my bones. Ishtar has kept us alive so far, but now he is exhausted and cannot sustain the heating spell. I think the whole palace is frozen.");
			log_char(cn,LOG_SYSTEM,0,"Why, oh why did we have to fight this war? The world was so beautiful, and so were we. But now, all that remains is blood and tears. If anyone survives this folly, let our fate teach you not to repeat our mistakes!");
			return;
		
		case BOOK_PALACE3:
			log_char(cn,LOG_SYSTEM,0,"Day 175, year 103. Personal diary of Islena.");
			log_char(cn,LOG_SYSTEM,0,"The cursed Isara are caught in our trap. Now they will die, all of them will die.");
			log_char(cn,LOG_SYSTEM,0,"Day 176, year 103. Personal diary of Islena.");
			log_char(cn,LOG_SYSTEM,0,"It seems some of them got away. The demon lords are out of control and trying to freeze them to death. I can feel the cold even here, in my rooms.");
			log_char(cn,LOG_SYSTEM,0,"Day 177, year 103. Personal diary of Islena.");
			log_char(cn,LOG_SYSTEM,0,"The cold is slowly killing all of us. All attempts to control the demon lords have failed. Now all of us must die. But I shall die happily if I can take Ishtar with me into the cold.");
			return;

		case BOOK_RUNES1:
			log_char(cn,LOG_SYSTEM,0,"Personal Diary of Korzam, Magical Advisor of Scarcewind.");
			log_char(cn,LOG_SYSTEM,0,"°c1The line above has been nearly scratched out, and replaced by:");
			log_char(cn,LOG_SYSTEM,0,"Personal Diary of Korzam, Governor of Exkordon.");
			log_char(cn,LOG_SYSTEM,0,"Scarcewind, the fool, is still loyal to Aston. He does not understand that the only way for our city to prosper is to cut our ties to that rotten empire. What good is an advisor, if no one listens to him?");
			log_char(cn,LOG_SYSTEM,0,"To get my mind on other things, I have gone north, into the barren lands below the mountains, hunting rumors. It is said that huge towers are build on those plains, and in those mountains. Towers built by powerful wizards of the old age. Whoever started these rumors has his history wrong, that is for sure. There was no old age. Before us were the ancients. They destroyed each other, and the world, in their foolish war. After them came we, and Ishtar and his notions of godhood and the empire.");
			log_char(cn,LOG_SYSTEM,0,"But if these towers are really there, and if they are as magical as the rumors say, who built them? Who else but the ancients! There was no one else who could have built them. And if the ancients are the makers, those towers are old and must have survived the destructions of the war. I want to see what kind of magic can make buildings survive what has shattered the earth.");
			log_char(cn,LOG_SYSTEM,0,"°c1You skip several pages containing a description of the voyage to the towers.");
			log_char(cn,LOG_SYSTEM,0,"I have forced my way into one of the towers. Magical they are, for sure, and guarded by the living dead. Fighting my way inside nearly exhausted me, and all I could do was grab some parchments and a small bag and flee, before those undead came back in greater numbers.");
			log_char(cn,LOG_SYSTEM,0,"The book is written in the language of the ancients. Unfortunately, I can barely understand some words. The bag contained polished pieces of bone, each bearing a rune. I will return to Exkordon now, and study them at my leisure.");
			log_char(cn,LOG_SYSTEM,0,"I found some pictures in the book, showing how to arrange the runes. I wonder what will happen...");
			log_char(cn,LOG_SYSTEM,0,"°c1You notice a change in the writing. It is the same hand, but the letters are bigger, and more forcefully written.");
			log_char(cn,LOG_SYSTEM,0,"That does it. Scarewind is a weak fool. I shall kill him, and take Exkordons fate into my own hands.");
			log_char(cn,LOG_SYSTEM,0,"Easy, almost too easy it was. I am now Governor of Exkordon. Scarcewind died like the fool he was in life. 'How can you do that? Why? I trusted you!' What a fool. I invited him into my house, told him about an important discovery I made. He came, and left his guards outside. And so he died. When his guards came looking for him, I lured them into my cellars, and disposed of them. They are no match for the ancient's magic.");
			log_char(cn,LOG_SYSTEM,0,"°c1Here, the writing changes back to the style used in the beginning.");
			log_char(cn,LOG_SYSTEM,0,"What have I done? What came over me? And why are the dead rising, and walking my halls? They are dead! Dead! I killed them!");
			return;

		case BOOK_RUNES2:
			log_char(cn,LOG_SYSTEM,0,"Once leads on, twice is rewarding, three times is dangerous.");
			return;

		case BOOK_RUNES3:
			log_char(cn,LOG_SYSTEM,0,"Two Berkano flanking an Ansuz will give thee Endurance.");
			return;

		case BOOK_RUNES4:
			log_char(cn,LOG_SYSTEM,0,"Berkano, Dagaz, Ansuz is healthy.");
			return;

		case BOOK_RUNES5:
			log_char(cn,LOG_SYSTEM,0,"Ansuz and Dagaz twice - good for Mages.");
			return;

		case BOOK_RUNES6:
			log_char(cn,LOG_SYSTEM,0,"Ansuz, Ehwaz, Dagaz - better defense for the Warrior.");
			return;

		case BOOK_RUNES7:
			log_char(cn,LOG_SYSTEM,0,"Ehwaz twice followed by Berkano - better defense for the Mage.");
			return;

		case BOOK_RUNES8:
			log_char(cn,LOG_SYSTEM,0,"Berkano, Ehwaz, Ansuz will decrease magic damage.");
			return;
		
		case BOOK_BONES1:
			log_char(cn,LOG_SYSTEM,0,"Day 12, year 45. Personal diary of Sluiran of the Caremar.");
			log_char(cn,LOG_SYSTEM,0,"The battles raging outside are closer to our hiding place. We must find some means to defend ourselves. I have started to study the forbidden art of necromancy, based on the rune magic. The undead shall fight where the living cannot.");
			log_char(cn,LOG_SYSTEM,0,"°c1You skip some pages.");
			log_char(cn,LOG_SYSTEM,0,"Day 37, year 47. Personal diary of Sluiran of the Caremar.");
			log_char(cn,LOG_SYSTEM,0,"The towers have fallen, but the undead have held our halls against the first wave of attackers. I have many, many bodies for my work now. More and more undead shall defend us. We might survive, after all.");
			return;
		
		case BOOK_BONES2:
			log_char(cn,LOG_SYSTEM,0,"Day 213, year 61. Personal diary of Sluiran of the Caremar.");
			log_char(cn,LOG_SYSTEM,0,"We have been attacked by demons again, and we are running out of dead bodies to raise in our defense. We can no longer reach those in the outer halls. It will not take long before they take our last defenses. But they shall not gain any profit by this. I shall cast a spell that will raise all dead in these halls over and over again. So we will continue the fight, even after we are dead.");
			return;

		case BOOK_EVILKIR:
			log_char(cn,LOG_SYSTEM,0,"My dear Sarkilar,");
			log_char(cn,LOG_SYSTEM,0,"thine shall be the land from rotten Exkordon to the icy shores Valkyries. It is ripe, ready for thee to take it. The magic of the Kir should give thee sufficient strength. Take as many of the young monks as thou canst, and cloud their mind, as I taught thee. Once thy force is strong enough, take the land which is promised thee.");
			log_char(cn,LOG_SYSTEM,0,"Islena");
			return;

		case BOOK_LAB2_DIARY:
                        log_char(cn,LOG_SYSTEM,0,"The pages are badly burned. You can only read: All those heros who tried to kill my brother died through his hands. To keep these young hotheads away, I summoned a demon to guard the entrance and ordered him to let no one pass but me. He is a bit short-sighted, but...");
                        log_char(cn,LOG_SYSTEM,0,"My brother must be killed, or the horror will never stop. He is my brother, but he must die for his misdeeds...");
                        log_char(cn,LOG_SYSTEM,0,"The last fight with the undeads was hard. But even though I am bleeding from many wounds, today is the day I will kill my brother. I will take the amulet and go into the family vault and face him now!");
                        return;
                case BOOK_LAB2_DIARY_PAGE:
			log_char(cn,LOG_SYSTEM,0,"Most of the page is burned, but you can read: To prevent holy water from hurting him, and his minions, my brother created a anti-magic zone which dispells all holy effects and all magic. But I have found a way to break this spell. I created an amulet to hold the counter-spell...");
                        return;

		case BOOK_SHRIKE:
			log_char(cn,LOG_SYSTEM,0,"My wounds are too much to bear and I fear that I will not survive. I have found none of the parts of the Talisman of the Moon, nor the location of the Moon Pool in which to enchant it. I have failed to find a way to lift the curse off my old friend, and I am sorry.");
                        return;
	}
}


int ch_driver(int nr,int cn,int ret,int lastact)
{
	switch(nr) {
                default:	return 0;
	}
}

int it_driver(int nr,int in,int cn)
{
	switch(nr) {
		case IDR_BOOK:		book_driver(in,cn); return 1;
		
		default:	return 0;
	}
}

int ch_died_driver(int nr,int cn,int co)
{
	switch(nr) {
                default:	return 0;
	}
}












