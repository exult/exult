/*	Copyright (C) 2023  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// Makes the Hound indicate the target direction. If the first element
// of target is -1, this means the player is in the right area (that is,
// "We must be close!").
// Always aborts.
extern void HoundPointDirection 0x83B (var target);

void HoundOfDoskar shape#(0x36A) () {
	if (event == DEATH) {
		clear_item_say();
		item_say(randomIndex(["@Yipe, yipe, yipe!@", "@Aoaaoouuu!@"]));
		set_attack_mode(FLEE);
	} else if (event == DOUBLECLICK) {
		AVATAR->item_say("@Come here, boy!@");
		HOUND_OF_DOSKAR->makePartyFaceNPC();
		script item after 2 ticks {
			nohalt;
			say "@Woof!@";
		}
		HOUND_OF_DOSKAR->set_schedule_type(TALK);
	} else if (event == SCRIPTED) {
		if (item == getPathEgg(5, 1)) {
			var found_hound = AVATAR->find_nearby(SHAPE_HOUND_OF_DOSKAR, 40, 0);
			if (found_hound) {
				found_hound->set_schedule_type(HOUND);
			} else {
				abort;
			}
		}

		struct<ObjPos> target = UI_click_on_item();
		var target_shape = target->get_item_shape();
		var target_frame = target->get_item_frame();
		var target_quality = target->get_item_quality();
		UI_play_music(18, getPathEgg(5, 1));

		if (target_shape == SHAPE_AMULET && target_frame == FRAME_DIAMOND_NECKLACE && gflags[KNOW_YELINDAS_DIAMOND_NECKLACE] && !gflags[RETURNED_COMB_OF_BEAUTY]) {
			HoundPointDirection([1561, 1465, 0]);
		} else if (target_shape == SHAPE_WOODEN_SWORD && gflags[WILL_FIND_CANTRA] && !gflags[BATLIN_FLED_SHAMINOS_KEEP]) {
			// This is a plot-critical flag
			gflags[SHAMINOS_KEEP_BARRIERS_GONE] = true;
			if (getAvatarLocationID() == SHAMINOS_CASTLE) {
				HoundPointDirection([-1, 0, 0]);
			} else {
				HoundPointDirection([1890, 999, 0]);
			}
		} else if (target_shape == SHAPE_AMULET && target_frame == FRAME_BATLINS_MEDALLION && gflags[BATLIN_FLED_SHAMINOS_KEEP] && !gflags[BANES_RELEASED]) {
			// Plot critical, will prevent entry to Skullcrusher if not set.
			gflags[HOUND_TRACKED_BATLIN] = true;
			if (getAvatarLocationID() == SPINEBREAKER_MOUNTAINS) {
				HoundPointDirection([-1, 0, 0]);
			} else {
				HoundPointDirection([1896, 739, 0]);
			}
		} else if (gflags[BANES_RELEASED] &&
		         ((target_shape == SHAPE_AMULET && target_frame == FRAME_WHITE_DIAMOND_NECKLACE && !gflags[INSANITY_BANE_DEAD]) ||
		          (target_shape == SHAPE_LUTE && target_frame == FRAME_IOLOS_LUTE && !gflags[INSANITY_BANE_DEAD]) ||
		          (target_shape == SHAPE_DUPRE_SHIELD && !gflags[WANTONNESS_BANE_DEAD]) ||
		          (target_shape == SHAPE_BOOK && target_quality == 63 && !gflags[ANARCHY_BANE_DEAD]))) {
			if (getAvatarLocationID() == WHITE_DRAGON_CASTLE) {
				HoundPointDirection([-1, 0, 0]);
			} else {
				HoundPointDirection([1801, 1458, 0]);
			}
		}
		script item {
			nohalt;
			say "@Woof woof?@";
		}
		var random_party = partyUtters(1, 0, 0, false);
		random_party->clear_item_say();
		delayedBark(random_party, "@The hound looks confused.@", 4);
		abort;
	} else if (event == STARTED_TALKING) {
		HOUND_OF_DOSKAR->set_schedule_type(HOUND);
		FACE_HOUND_OF_DOSKAR.show_npc_face0(0);
		say("@Woof!@");
		add(["trick", "track", "attack", "bye"]);

		converse (0) {
			case "track":
				script item {
					nohalt;
					call HoundOfDoskar;
				}
				abort;

			case "trick":
				set_schedule_type(DANCE);
				script item {
					nohalt;
					say "@Ruff ruff ruff!@";
				}
				abort;

			case "attack" (remove):
				struct<ObjPos> target = UI_click_on_item();
				var barks = ["@Woof.@", "@Woof?@", "@Woof!!@", "@Woof.@"];
				if (!target || target->get_alignment() == GOOD || !target->is_npc()) {
					say(barks[GOOD]);
				} else {
					say(barks[target->get_alignment()]);
					set_schedule_type(IN_COMBAT);
					set_opponent(target);
					set_oppressor(target);
					abort;
				}
			case "bye" (remove):
				script item {
					nohalt;
					say "@Woof! Woof!@";
				}
				UI_remove_npc_face0();
				abort;
		}
	} else if (event == SI_PATH_SUCCESS) {
		set_schedule_type(HOUND);
		script item {
			nohalt;
			say "@Ruff!@";
		}
	} else if (event == SI_PATH_FAILURE) {
		set_schedule_type(HOUND);
	} else if (event == PROXIMITY) {
		clear_item_say();
		item_say(randomIndex(["@Woof!@", "@Bark!@"]));
	}
}
