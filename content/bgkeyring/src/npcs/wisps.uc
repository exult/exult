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
 *	This source file contains usecode for the Wisps, for compatibility
 *	with the Improved Orb of the Moons.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

const int QUALITY_ALAGNER_NOTEBOOK		= 2;	//0x002
const int WISP_FACE						= -256;
void Wisp object#(0x500) () {
	var party;
	var wisp_sched;
	var party_has_notebook;

	if (event == PROXIMITY) {
		return;
	}

	party = UI_get_party_list();
	wisp_sched = UI_get_schedule_type(get_npc_object());

	if (!(wisp_sched == TALK)) {
		WISP_FACE.say("The wisp does not respond.*");
		return;
	}

	//Standard options:
	add(["name", "job", "bye"]);

	//If the party has Alagner's notebook, add the option to talk about it:
	if (contHasItemCount(PARTY, 1, SHAPE_BOOK, QUALITY_ALAGNER_NOTEBOOK, FRAME_ANY)) {
		add("notebook");
	}

	//The party has seen the gypsy and knows about the Timelord:
	if (gflags[TALKED_TO_GYPSY]) {
		add("Time Lord");
	}

	if(!gflags[HAD_FIRST_WISP_TALK]) {
		WISP_FACE.say("A ball of light approaches you.",
			"~~@'You' are not the entity known as 'Trellek'. 'You' call out in the manner of the species called 'emps'. 'Xorinia' was expecting the entity 'Trellek'.");
		say("@But that is not of importance. From the information 'I' have, the local manifestation before 'me' is the entity known as 'Avatar'.");
		say("The Wisp glows brightly a second or two.",
			"~~@'Xorinia' wishes to exchange information with the human entity.@");
		gflags[HAD_FIRST_WISP_TALK] = true;
		giveExperience(500);
	} else {
		WISP_FACE.say("@Once again a local manifestation addresses the Xorinite dimension.@");
	}


	converse(0) {
		case "name" (remove):
			say("@Highest probabilities indicate that a manifestation from the Xorinite dimension will be called 'Wisp' by the entities known as 'humans'. I am also called 'Xorinia' by other manifestations of the Xorinite dimension.@");
			add("Wisp");

		case "Wisp" (remove):
			say("@This label has been implemented by human entities to name manifestations from the Xorinite dimension since the time when this dimension was discovered by Xorinite manifestations. Another common name is 'Will-o-the-wisp'.",
				"~~ @The preceding sample of information was provided without charge. Usually there is a fee for information.@");

		case "information" (remove):
			say("@The Undrian Council seeks information regarding a certain entity by the name of 'Alagner'. 'You' have access to this information. 'I' have information regarding a certain entity which 'you' are seeking. The Undrian Council proposes a trade.@");
			add(["Undrian Council", "Alagner", "trade"]);

		case "Undrian Council" (remove):
			say("@The Council represents what 'your' language defines as 'government'.@");

		case "job":
			say("@'Xorinia' is a conduit for information between different planes and dimensions. 'Xorinia' also catalogs information which is necessary for growth of the Xorinite community. 'You' have information which may be valuable to 'me'. 'I' also have information that 'you' want.@");
			add("information");

		case "Alagner" (remove):
			say("@The Undrian Council has information that there is a human entity in 'your' dimension that has been called 'the wisest man in Britannia.' This entity is known as 'Alagner' and lives in 'your' colony of 'New Magincia'. 'Alagner' has what the entity calls a 'notebook'. The 'notebook' is a collection of information.@");

		case "trade" (remove):
			say("@'I' want to absorb the information in Alagner's 'notebook'. If 'you' bring the 'notebook' here, the Undrian Council will release information useful to 'you'. Do 'you' agree to the trade?@");
			gflags[WISP_OFFERED_TRADE] = true;

			if (askYesNo()) {
				say("@'Xorinia' recognizes 'your' usefulness. 'I' shall be here. Human entities will call 'my' activity 'waiting'.@");
			} else {
				say("@'Xorinia' recognizes 'your' hostility. 'I' shall be here should 'you' reflect upon 'your' decision and decide to change it.@*");
				set_schedule_type(SHY);
				return;
			}

		case "Time Lord" (remove):
			if (!gflags[WISP_OFFERED_TRADE]) {
				say("@The entity known as 'Time Lord' requests an audience with 'you'. Before 'I' can give 'you' more information about this, 'I' must propose a trade.@");
				add("information");
			} else {
				say("@The entity known as 'Time Lord' is a being from the space/time dimension. The Xorinite Dimension has been communicating with 'Time Lord' for what 'humans' call 'centuries'.@");
			}

		case "notebook" (remove):
			say("@The human entity is welcomed by 'Xorinia'. 'You' have brought the item 'notebook'. 'I' shall now absorb the information contained therein.@",
				"~~The Wisp glows brightly for a few seconds. The notebook remains in your possession.",
				"~~@'I' have completed my absorption of the information. 'You' may now return the item 'notebook' to the entity 'Alagner'.",
				"~~@And now for the exchange of information and delivery of a message.@");
			gflags[DELIVERED_NOTEBOOK_TO_WISPS] = true;
			giveExperience(700);
			add(["exchange", "message"]);

		case "message" (remove):
			say("@'Xorinia' must deliver a message to 'you'. The entity known as 'Time Lord' requests 'your' audience. 'Time Lord' is trapped at the plane known as the Shrine of Spirituality. 'You' can reach 'him' by using 'your' object 'Orb of the Moons' in the location directly to 'your' 'far' 'southwest'.");
			gflags[ORB_FIXED_TIMELORD] = true;
			add("Time Lord");

		case "exchange" (remove):
			say("@Now for the information 'you' seek. 'This' dimension known as 'Britannia' is under attack by an entity called 'The Guardian'.",
				"~~@'The Guardian' lives in another dimension. 'Xorinia' sometimes trades information with this entity. Do 'you' want to know more about 'The Guardian'?@");
			if(askYesNo()) {
				say("@'Xorinia' has digested information about 'The Guardian' and can state the following facts:",
					"~~@'The Guardian' possesses qualities which human entities label 'vain', 'greedy', 'egocentric', and 'malevolent'. 'The Guardian' thrives on power and domination. 'The Guardian' takes 'pleasure' from conquering other worlds. His sensory organs are now focused on 'this' dimension known as 'Britannia'.",
					"~~@'The Guardian' is attempting to enter 'this' dimension by means of an item human entities call a 'Moongate'. This 'Moongate' is not a 'red' color or 'blue' color 'Moongate', which 'Xorinia' knows is the standard form of this item. 'The Guardian' is building a 'Moongate' of the color 'black'.@");
				add("Black Gate");
			} else {
				say("@'Xorinia' always responds to free information. Transaction complete.@*");
			}

		case "Black Gate" (remove):
			say("@The 'Black Gate' will be fully functional when the phenomenon known as 'Astronomical Alignment' next occurs.",
				"~~ @Although 'Xorinia' does not normally seek to influence actions of other manifestations, 'Xorinia' warns 'you' that if 'The Guardian' enters 'this' dimension, it will be the end of the dimension known as 'Britannia'. 'The Guardian' is powerful in 'his' own dimension. In 'your' dimension, 'he' will be unstoppable.",
				"~~@The Undrian Council sincerely hopes this information is useful. Transaction complete.@*");
			gflags[LEARNED_ABOUT_ALIGNMENT] = true;

		case "bye":
			say("@'Xorinia' always welcomes the exchange of information. Farewell.@*");
			set_schedule_type(SHY);
			return;
	}
}
