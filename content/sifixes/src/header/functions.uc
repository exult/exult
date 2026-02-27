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
 */

//returns a random entry from an array
var randomIndex(var array) {
	var size = UI_get_array_size(array);
	var rand = UI_get_random(size);
	return array[rand];
}

void freeze object#() () {
	set_item_flag(DONT_MOVE);
}

void unfreeze object#() () {
	clear_item_flag(DONT_MOVE);
}

void addShaminoToParty () {
	SHAMINO->add_to_party();
	gflags[SHAMINO_HAS_BELONGINGS] = true;
	SHAMINO->set_npc_id(0);

	script getPathEgg(5, 4) after 10 ticks {
		nohalt;
		call startSerpentSpeechViaRing;
	}
	delayedBark(SHAMINO, "@Welcome back!@", 0);
	abort;
}

// Unlike the other two companions, Iolo didn't have one for himself:
void askIoloBelongings () {
	if (IOLO->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY)) {
		say("@I am carrying many items, some of which may be of use to thee. Wouldst thou care to have these?@");
		if (askYesNo()) {
			say("@Here they are.@");
			gflags[IOLO_HAS_BELONGINGS] = false;

			var iolo_items = IOLO->get_cont_items(SHAPE_ANY, QUALITY_ANY, FRAME_ANY);
			var obj;
			var index;
			var max;
			var give_return;
			var cumulative_result = [false, 0];

			for (obj in iolo_items with index to max) {
				if (!((obj->get_item_shape() == SHAPE_PLAINRING) &&
				    (obj->get_item_frame() == 2))) {
					give_return = giveItemsToPartyMember(AVATAR, obj->get_item_quantity(0), obj->get_item_shape(), obj->get_item_quality(), obj->get_item_frame(), obj->get_item_flag(TEMPORARY), false);
					if (cumulative_result[1] == 0) {
						cumulative_result[1] = give_return[1];
					}

					cumulative_result[2] = (cumulative_result[2] + give_return[getArraySize(give_return)]);
					obj->remove_item();
				}
			}

			if (cumulative_result[1] != 0) {
				say("@Thy friends will have to help carry these things.@");
			}

			if (cumulative_result[2] > 0) {
				give_return = cumulative_result[2];
				if (give_return > 1) {
					say("@Since thou canst not carry these remaining ",
					give_return, " items, I will place them at thy feet.@");
				} else {
					say("@Since thou dost not have enough room for this last item, I will place it at thy feet.@");
				}
			}
		} else {
			gflags[IOLO_HAS_BELONGINGS] = true;
			say("@If thou changest thy mind, thou hast but to return and ask again.@");
		}
	}
}

/*
 *  The following function has been created from usecode found in the
 *	Exult CVS snapshot. I include it here only for convenience; I have
 *	edited it to fit the conventions used in the rest of the mod.
 *
 *	The original code was written by Jeff Freedman (aka "DrCode").
 *
 */
const int QUALITY_LOGIC = 13;
const int QUALITY_ETHICALITY = 14;
const int QUALITY_DISCIPLINE = 15;

void CureCantra () {
	if (event == SI_PATH_SUCCESS) {
		UI_close_gumps();
		// Now empty.
		set_item_frame(0);
		// Just for safety
		set_item_quality(0);

		CANTRA->obj_sprite_effect(ANIMATION_TELEPORT, 0, 0, 0, 0, 0, 0);
		// No longer crazy.
		CANTRA->clear_item_flag(SI_ZOMBIE);
		CANTRA->set_schedule_type(TALK);
		// We've done it.
		gflags[CURED_CANTRA] = true;
	} else if (event == SI_PATH_FAILURE) {
		CANTRA->set_schedule_type(WANDER);
	}
}

void CureCompanion () {
	var npcnum;
	var bucket_quality = get_item_quality();

	// Through the bucket's quality, determine which companion
	// is being cured:
	if (bucket_quality == QUALITY_LOGIC) {
		npcnum = IOLO;
	} else if (bucket_quality == QUALITY_ETHICALITY) {
		npcnum = SHAMINO;
	} else if (bucket_quality == QUALITY_DISCIPLINE) {
		npcnum = DUPRE;
	}

	if (event == SI_PATH_SUCCESS) {
		UI_close_gumps();
		// Now empty.
		set_item_frame(0);
		// Just for safety
		set_item_quality(0);

		npcnum->obj_sprite_effect(ANIMATION_TELEPORT, 0, 0, 0, 0, 0, 0);
		npcnum->clear_item_flag(SI_ZOMBIE);

		// This is for the new conversation, and only happens once per companion:
		npcnum->set_npc_id(CURED_OF_INSANITY);

		if (!npcnum->get_item_flag(IN_PARTY)) {
			npcnum->set_schedule_type(TALK);
		}
	} else if (event == SI_PATH_FAILURE) {
		npcnum->set_schedule_type(WANDER);
	}
}

void firesnakeExplode () {
	var obj;
	var index;
	var max;
	var damage;
	var nearbyobjs = find_nearby(SHAPE_ANY, 8, 0);
	var party = UI_get_party_list();
	struct<Position> pos = get_object_position();
	var vertoff = (pos.z + 1) / 2;
	UI_sprite_effect(ANIMATION_BIG_BLAST, (pos.x + vertoff), (pos.y + vertoff), 0, 0, 0, -1);
	UI_play_sound_effect(42);

	for (obj in nearbyobjs with index to max) {
		// Party safety:
		if (!(obj in party)) {
			damage = UI_die_roll(10, 20);
			if (obj->is_npc()) {
				script obj hit damage, MAGIC_DAMAGE;
				if (obj->get_alignment() != 2) {
					obj->set_alignment(2);
				}
				obj->set_schedule_type(IN_COMBAT);
			} else {
				script obj hit damage, MAGIC_DAMAGE;
			}
		}
	}
}

void dropAllItems (var npc, var pos) {
	// Here, we drop only objects directly held by the character:
	var spots = [
		SI_OFF_HAND, SI_WEAPON_HAND, SI_CLOAK, SI_AMULET, SI_HEAD,
		SI_GLOVES, SI_RIGHT_RING, SI_LEFT_RING, SI_EARS, SI_QUIVER,
		SI_BELT, SI_TORSO, SI_FEET, SI_LEGS, SI_BACKPACK,
		SI_BACK_SHIELD, SI_BACK_2H
	];

	for (spot in spots) {
		var obj = npc->get_readied(spot);
		if (obj->set_last_created()) {
			UI_update_last_created(pos);
		}
	}
}

void wraperXenkaReturns object#() () {
	xenkaReturns();
}
