/*
 *
 *  Copyright (C) 2006  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *	This source file contains the code for Zauriel's journal.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

void ZaurielJournal shape#(0x44E) () {
	var quality = get_item_quality();

	//This basically alternated book text with remarks from party members.
	if (event == DOUBLECLICK) {
		//Play book sound:
		UI_play_sound_effect2(SOUND_BOOK, item);
		//Start book mode for current item:
		book_mode();
		//Get journal's quality:
		quality = get_item_quality();
		//Display text based on book's quality:
		if (quality == 1) {
			UI_close_gumps();
			gflags[READ_ZAURIEL_JOURNAL] = true;
			say("~~ ~~Zauriel's Journal~~",
				"~~by Zauriel~~",
				"~~Part 2*Year 343",
				"~~April 7",
				"~~    My plans are coming close to fruition. The pieces are moving in the board, and soon the coup will come. With the powerful spells under my command, I have no doubt that Lord British will fall prey to my magics! I will be the Overlord of Britannia! Mwahahaha. MWAHAHAHAHA!!!*");
			say("April 8",
				"~~    YES! Another victory! I have now learned the secret art of shapeshifting from that FOOL Erethian! I can now become a dragon! YES! HAHA! Lord British is doomed! Mwahahaha!!!*April 9",
				"~~    My supply of reagents is growing steadily, as well as my army of undead creatures and golems. Soon, my associates and I will be able to crush the ENTIRE ARMY of Lord British and then I will crush him with my bare fists! Mwahaha!!*");
		} else if (quality == 2) {
			say("June 12",
				"~~    My head was extremely sore yesterday and, from what I can gather, for at least a month since. I haven't written any entries in my journal throughout this period, and I think it is a good time to start again. My memories of the last few years seem to be fading quickly, as if some sort of veil is standing before my eyes. I was able to puzzle together what had happened to me during this time, and most of which was not good.",
				"~~    I was apparently on a quest to overthrow the good Lord British, and had conspired with a set of... unsavory characters, to say the least. What had possessed me to make such an attempt at the life of a just leader? I know not, but it seems to have passed. I have arranged for a meeting with my former 'associates' tomorrow, and I intend to dissolve my partnership with them, as well as invalidate the plans I had laid. I don't expect that it will end well...*June 13",
				"~~    The meeting went much as I had foreseen. My former associates made the mistake of attacking me. It was unfortunate, but I had to kill them all. I suppose that it was inevitable, but I still wish they could be reasoned with. In any case, I found it odd that their spells went awry whenever they tried anything. They also seemed less... sane than they were before. I wonder why my spells were unaffected? Moreover, I wonder why I am not going insane, but seem to be growing less insane by the minute? Perhaps something is happening to Britannia. I may have to check that in the future...*");
			say("June 14",
				"~~    I have made my mind: I shall try to discover what is happening to Britannia, or more precisely, to the Ether. I will go to the Lycaeum first thing in the morning; if only I knew where the nearest moongate is located...*");
			say("August 28",
				"~~    I have finally found the moongate! I wonder how many people get lost here in Yew trying to find it?");
		} else if (quality == 3) {
			say("December 21",
				"~~    My time in the Lycaeum has been well spent. I have a much better idea of what may be happening in Britannia. My magical skills have also vastly improved in this period of time; it is a good thing that I shall no longer be dependent on the moongates for travel. It is time to go to the mainland and try to search for more answers.*");
			say("December 22",
				"~~    Back in the mainland, I have found lodging here with two mages. They live in a lone house to the north of the Shrine of Sacrifice.  They are married and have a newborn daughter, by the way.",
				"~~    They appear to be stable for the moment, but I am more concerned for their still unnamed daughter -- I can sense the sheer power she carries within, almost a vortex in the Ether. I also fear that her mind will be irreparably damaged by the Ether distortions... it is really sad that someone this young is doomed to insanity, but I dread her powers the most. I hope it is an unfounded fear...*");
			say("December 23",
				"~~    Alas, my fears were correct. I have barely escaped with my life from a devastating explosion caused by the baby! Her parents weren't so lucky, and neither was everything nearby -- all but the little girl were turned to dust.",
				"~~    The child is extremely dangerous -- but I cannot abandon her to her death. Maybe there is some way to shield her from the distorted Ether?*");
			say("December 26",
				"~~    I have decided upon a name for the baby: I shall call her Laurianna. It seems to fit her, somehow...*");
			say("Year 347",
				"~~March 12",
				"~~    I have discovered a marvelous material -- which people seem to call Blackrock nowadays. It has precisely the sort of capabilities that I need, being a very effective at blocking the Ether waves. Maybe I can construct some sort of helmet to protect my daughter, or maybe even control the destruction she causes.*");
			say("July 9",
				"~~    I have found an arrangement of Blackrock which nullifies the destruction caused by my daughter. But shaping the blackrock has proven to be beyond my capabilities...*");
			say("September 12",
				"~~    It appears that the damaged Ether is the source of my sanity. Recently, I repeatedly found myself wandering randomly, killing things for no reason at all... this behavior not appropriate for a dragon of my stature.",
				"~~    The longer I spend protected from the damaged Ether, the longer I stay in this strange state of mind... I wonder what might happen if I spend too much time protected from the Ether?*");
			say("Year 350",
				"~~June 20",
				"~~    I, the great Lord Zauriel, have attained some success in working with the Blackrock. It is still beyond my capabilities to craft an effective helmet from it, although I have uncovered a way to distill a potion from it that protects my daughter's damaged mind from the damaged Ether. Apparently, the potion is deadly poison to all save her, therefore I shall not even write it to my journal.*");
			say("Year 356",
				"~~February 18",
				"~~    Today is my birthday, and I have been graced with visions of what will come to pass. I put them forth here, up and until the moment of my death by the hands of the mighty Avatar -- who shall return to save Britannia from the mounting threat and will restore the Ether to normal in the process. It is perhaps poetic justice that I should perish by the hands of such a valiant mortal, due to all the transgressions I have made in the past and all the woe and destruction I have caused.",
				"~~    I, the great Zauriel, shall be slain by the hands of a most worthy opponent, the same one that slew Mondain, Minax and Exodus! My name will go down into history due to the fierce battle that will ensue, and I shall finally be able to cure my daughter! I must now only uncover the spells I shall have to weave to finally cure my hatchling from the insanity that afflicts her.*");
			say("Year 361",
				"~~August 31",
				"~~    Today I shall make the final preparations to cure my daughter. Many mighty spells I, Lord Zauriel, shall weave to take effect upon my demise, using my very essence to heal my daughter's damaged mind. And it could not have been in a better time, for the Avatar shall be returning to Britannia any moment now, even as I have foreseen. All I have to do now is to wait 'distracted' and 'weakened' until that inept mage attacks and kidnaps my beloved daughter, thus setting in motion the events that will result in my well deserved death.*");
		}

		//Allow for a remark by an NPC:
		script item call ZaurielJournal;
	} else if (event == SCRIPTED) {
		//Get a random party member:
		var npc = randomPartyMember();
		var npc_num = npc->get_npc_number();
		var npc_face;
		if ((npc_num == AVATAR) && UI_is_pc_female()) {
			//If it is avatar and female, set correct face:
			npc_face = 1;
		} else {
			npc_face = 0;
		}
		//Show the NPC's face
		npc->show_npc_face(npc_face);

		//Use the book's quality to determine the remark:
		if (quality == 1) {
			say("@There are pages and pages of this drivel. The man seems to be totally insane, even to the point of including maniacal laughter in writing...");
			say("@Maybe we should skip ahead a little bit...@");
		} else if (quality == 2) {
			var msg;
			if (npc_num == AVATAR) {
				msg = "Iolo, Shamino, Dupre and myself";
			} else if (npc_num == IOLO) {
				msg = "the Avatar, Shamino, Dupre and myself";
			} else if (npc_num == SHAMINO) {
				msg = "the Avatar, Iolo, Dupre and myself";
			} else if (npc_num == DUPRE) {
				msg = "the Avatar, Iolo, Shamino and myself";
			} else {
				msg = "the Avatar, Iolo, Shamino and Dupre";
			}
			say("@Zauriel also developed a strong interest in ", msg,
			    " while in the Lycaeum. He seems to be completely obsessed...@");
		} else if (quality == 3) {
			say("@Several of the next pages include detailed accounts of everything we did ever since thou didst return to Britannia, Avatar!");
			say("@It is unnerving to think that he knew all of this years before the events actually happened.");
			say("@But why, oh why, did he not write the things thou woudst do -after- he died?@");
			set_item_quality(1);
			return;
		}

		//Increase the journal's quality:
		set_item_quality(quality + 1);
		//Return to the function for more book text:
		script item call ZaurielJournal, DOUBLECLICK;
	}
}
