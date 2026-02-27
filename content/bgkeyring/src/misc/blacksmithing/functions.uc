/*
 *
 *  Copyright (C) 2006  Alun Bestor/The Exult Team
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
 *	Author: Marzo Junior (reorganizing/updating code by Alun Bestor)
 *	Last Modified: 2006-03-19
 */

//Swordblank was doubleclicked and used on anvil - place it on top of the anvil
void useSwordOnAnvil object#() () {
	var anvil = AVATAR->find_nearest(SHAPE_ANVIL, 5);
	if (anvil) {
		placeOnTarget(item, anvil, 0, 0, 1);
	}
}

//returns true if <swordblank> is correctly positioned on <anvil>, false otherwise
var onAnvil (var swordblank, var anvil) {
	struct<Position> swordblank_pos = swordblank->get_object_position();
	struct<Position> anvil_pos = anvil->get_object_position();

	return (swordblank_pos.x == anvil_pos.x &&
			swordblank_pos.y == anvil_pos.y &&
			swordblank_pos.z == anvil_pos.z + 1);
}

//returns true if the specified blank is the Black Sword blank, false otherwise
var isBlackSword (var swordblank) {
	return (swordblank->get_item_frame() >= 8);
}

//Set the base cool frame of the swordblank (this depends on how much it has
//been worked; it is governed by 3 different quality states, see
//swordblank_qualities enum)
void setCooledFrame object#() () {
	var target_frame;
	var quality = get_item_quality();

	if (isBlackSword(item)) {
		if (gflags[FINISHED_BLADE_FORGING]) {
			target_frame = 15;
		} else if (gflags[IMPROVED_BLADE]) {
			target_frame = 14;
		} else {
			target_frame = 13;
		}
	} else {
		if (quality == SWORDBLANK_READY) {
			target_frame = 7;
		} else if (quality >= SWORDBLANK_IMPROVED) {
			target_frame = 6;
		} else {
			target_frame = 0;
		}
	}
	set_item_frame(target_frame);
}

//This function slowly cools down a hot swordblank to its cool state
//More complex than you might think it needs to be, because it's
//self-calling and there's special frame-selection when a sword
//reaches its coolest state.
void coolSwordBlank object#() () {
	var current_frame;
	var is_blacksword;
	var hot_frames;
	var warm_frames;
	var cool_frames;
	var min_frame;
	var target_frame;
	var quality;

	current_frame = get_item_frame();
	is_blacksword = isBlackSword(item);

	if (is_blacksword) {
		hot_frames = [10, 11, 12];
		warm_frames = [8, 9];
		cool_frames = [13, 14, 15];
		min_frame = 8;
	} else {
		hot_frames = [2, 3, 4, 5];
		warm_frames = [1];
		cool_frames = [0, 6, 7];
		min_frame = 1;
	}

	//We're on the last heated frame - cool the sword down completely
	//to the appropriate base frame
	if (current_frame == min_frame) {
		item->setCooledFrame();

		//If the swordblank was ready to be quenched but was left to
		//cool naturally, it will warp and revert to a lower quality
		if (!is_blacksword) {
			quality = get_item_quality();
			if (quality == SWORDBLANK_QUENCH) {
				set_item_quality(quality - 2);
				avatarSay("The blank has warped as it cooled down -- it will need more heating and work to straighten the blade again.~Once the blade is at its sharpest, it must be quenched quickly in a water trough to strengthen and harden the blade.");
			}
		}
	} else if (current_frame in hot_frames || current_frame in warm_frames) {
		//Sword still has more cooling to do, decrement the frame and call this
		//function again
		script item {
			previous frame;
			wait SWORDBLANK_COOL_SPEED;
			call coolSwordBlank;
		}
	}
}

//The initial function to start the swordblank cooling down. This should be
//called *whenever the swordblank is used in a script block*, since that
//script would otherwise override the cooling-down script and the blank
//would stay hot forever.
void startCooling object#() () {
	script item after SWORDBLANK_COOL_SPEED ticks call coolSwordBlank;
}

/*
 *	This function controls the heating animation of the swordblank, when
 *	it is heated on a firepit.
 */
void heatSwordBlank object#() () {
	var current_frame;
	var is_blacksword;
	var min_frame;
	var max_frame;
	var target_frame;

	//Hot enough to hammer
	var hot_frames;
	//Heated, but not hot enough
	var warm_frames;
	//Cooled down completely
	var cool_frames;

	current_frame = get_item_frame();
	is_blacksword = isBlackSword(item);

	//Frames for the Black Sword
	if (is_blacksword) {
		hot_frames	= [10, 11, 12];
		warm_frames = [8, 9];
		cool_frames = [13, 14, 15];

		//The minimum heated frame to cool down to
		min_frame =	8;
		//The maximum heated frame to heat up to
		max_frame = 12;
	} else {
		//Frames for the regular swordblank
		hot_frames	= [2, 3, 4, 5];
		warm_frames = [1];
		cool_frames = [0, 6, 7];

		//The minimum heated frame to cool down to
		min_frame =	1;
		//The maximum heated frame to heat up to
		max_frame = 5;
	}

	if (current_frame in cool_frames) {
		target_frame = min_frame;
	} else {
		target_frame = current_frame + 1;
		if (target_frame > max_frame) {
			target_frame = max_frame;
		}
	}

	//Now, set the swordblank to the target frame then start it
	//cooling down again.
	script item {
		wait 2;
		frame target_frame;
		call startCooling;
	}
}

//This function is called when the swordblank is worked upon the
//anvil with a hammer - it raises (or lowers) the quality of the
//swordblank. The only place this function is called from is
//useHammerOnSwordblank.
void temperSword object#() () {
	//Find the nearest swordblank to the Avatar
	//(we can't provide swordblank as the <item>, because this would require a
	//script block - and we don't want to override the cooling-down script
	//animation.)
	var swordblank = AVATAR->find_nearest(SHAPE_SWORDBLANK, 3);
	if (!swordblank) {
		return;
	}

	var current_frame = swordblank->get_item_frame();
	var rand = UI_get_random(100);

	var quality = swordblank->get_item_quality();

	//The Black Sword works slightly different from other swords
	if (isBlackSword(swordblank)) {
		if (gflags[FINISHED_BLADE_FORGING]) {
			//Sword is as good as it gets
			avatarSay("The blade has been worked as well as it can be. It will take some form of magic to make this sword blank into a usable weapon.");
		} else if (gflags[IMPROVED_BLADE]) {
			//Sword has been improved
			if (rand > 66) {
				gflags[FINISHED_BLADE_FORGING] = true;
				avatarSay("You feel that you've done the best job that you can, but the sword doesn't feel quite right. It's much too heavy and cumbersome to wield as a weapon.");
			} else if (rand < 20) {
				//Whoops! There goes your hard work!
				gflags[IMPROVED_BLADE] = false;
				avatarSay("That last blow was perhaps a bit too hard, It'll take a while to hammer out the flaws.");
			}
		} else if (rand > 66) {
			//Sword is at its shittiest state
			gflags[IMPROVED_BLADE] = true;
			avatarSay("After a short while you notice that the edge has definitely improved.");
		}
	} else {
		//The regular swordblanks:
		if (quality == SWORDBLANK_READY) {
			//Swordblank is already as good as it gets
			avatarSay("This blade is already sharp and true, and ready to be given to a weaponsmith for the finishing touch - the hilt and pommel.");
			return;
		} else if (quality == SWORDBLANK_QUENCH) {
			//Swordblank needs quenching and cannot be improved any more
			avatarSay("You have put as fine an edge on this blade as you can. It must now be quenched in water to harden the blade before the blank cools completely.");
			return;
		} else {
			//Swordblank can be worked, adjust its quality
			var strength	= AVATAR->get_npc_prop(STRENGTH);
			var dexterity	= AVATAR->get_npc_prop(DEXTERITY);

			var fail_chance		= BASE_TEMPER_FAIL_CHANCE - (dexterity / 2);
			var succeed_chance	= BASE_TEMPER_SUCCEED_CHANCE + (strength / 2);

			//Small chance (based on dexterity) that you fuck it up and
			//mess up the blade
			if (quality > 0 && rand < fail_chance) {
				avatarBark(randomIndex(["@Argh!@", "@Oops!@"]));

				//Give some party feedback
				if (UI_get_random(2) == 1) {
					randomPartyBark(randomIndex(["@Not so hard!@", "@Careful!@"]));
				}

				//Lower the quality
				swordblank->set_item_quality(quality - 1);
			} else if (rand < succeed_chance) {
				//otherwise, there's a bigger chance (based on strength)
				//that you improve the quality of the blade
				//if this bumped the quality up to the next notch,
				//notify the player they're making progress
				if (quality + 1 == SWORDBLANK_IMPROVED) {
					if (isNearby(MENION)) {
						MENION.say("Menion comes over to check on your progress. @Ah, thine blade is coming along nicely! A little more sweat and tears and thou shalt have a fine sword!@");
						MENION.hide();
					} else {
						avatarSay("After a short while you notice that the edge has definitely improved.");
					}
				} else if (UI_get_random(2) == 1) {
					//Otherwise, give some party feedback
					randomPartyBark(randomIndex(["@Looks good!@", "@Keep it up!@", "@Put thine arm into it!@"]));
				}
				swordblank->set_item_quality(quality + 1);
			}
		}
	}
}

//This is called when the avatar is in front of the anvil and ready to
//use their hammer on the swordblank.
//We can get to this function by:
//	1.	Doubleclicking a wielded hammer and clicking on a swordblank on top
//		of an anvil
//	2.	Doubleclicking a swordblank that is on top of an anvil while wielding
//		a hammer
//	3.	Doubleclicking an anvil that has a swordblank on top of it while
//		wielding a hammer
//<item> is the swordblank in question.

void useHammerOnSwordblank () {

	//Make sure they're still wielding the hammer! This should always
	//be the case, though; allow Julia's hammer to be used too
	if (!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_HAMMER, QUALITY_ANY) &&
			!AVATAR->is_readied(BG_WEAPON_HAND, SHAPE_JULIAS_HAMMER, QUALITY_ANY)) {
		return;
	}
	var current_frame = get_item_frame();

	//Hot enough to hammer
	var hot_frames;
	//Heated, but not hot enough
	var warm_frames;
	//Cooled down completely
	var cool_frames;
	if (isBlackSword(item)) {
		hot_frames = [10, 11, 12];
		warm_frames = [8, 9];
		cool_frames = [13, 14, 15];
	} else {
		hot_frames = [2, 3, 4, 5];
		warm_frames = [1];
		cool_frames = [0, 6, 7];
	}

	if (current_frame in cool_frames) {
		avatarBark("@The sword is not heated.@");
	} else if (current_frame in warm_frames) {
		avatarBark("@The sword is too cool.@");
	} else if (current_frame in hot_frames) {
		//Ok, the blade is hot enough, bash it good
		//Swing that hammer good
		script AVATAR {
			face directionFromAvatar(item);
			actor frame ready;
			actor frame raise_1h;
			actor frame strike_1h;
			sfx SOUND_HAMMER_SWORD;
			//Set the effect that the bashing is having on the swordblank
			call temperSword;
			actor frame ready;
			actor frame standing;
		}
	}
}

//Swordblank was doubleclicked and used on firepit - place it in the center
void useSwordOnFirepit object#() () {
	var firepit = AVATAR->find_nearest(SHAPE_FIREPIT, 5);
	if (firepit) {
		// Now, place it on the firepit
		placeOnTarget(item, firepit, 0, -1, 2);
	}
}

//Swordblank was used on a trough of water (item is swordblank)
void useSwordOnTrough object#() () {
	var trough;
	struct<Position> swordblank_pos;
	var offsetx = 0;
	var offsety = 0;

	//Find the nearest horizontal/vertical trough we can
	trough = AVATAR->find_nearest(SHAPE_TROUGH_HORIZONTAL, 5);
	if (!trough) {
		trough = AVATAR->find_nearest(SHAPE_TROUGH_VERTICAL, 5);
	}
	if (!trough) {
		return;
	}

	//Todo: work out horizontal trough offsets
	var placed = false;
	if (trough->get_item_shape() == SHAPE_TROUGH_VERTICAL) {
		offsetx = 1;
		offsety = -1;
		placed = true;
		struct<Position> pos = trough->get_object_position();
		if (!isBlackSword(item)) {
			pos.x += 1;
			pos.y -= 2;
		} else {
			pos.x += 1;
			pos.y -= 1;
		}
		pos.z += 2;
		item->set_last_created();
		if (UI_update_last_created(pos)) {
			UI_play_sound_effect(0x49);
		}
	} else {
		placed = placeOnTarget(item, trough, offsetx, offsety, 2);
	}

	if (placed) {
		swordblank_pos = get_object_position();
		UI_sprite_effect(ANIMATION_POOF, (swordblank_pos.x - 3), (swordblank_pos.y - 3), 0, 0, 0, -1);
		UI_play_sound_effect(SOUND_QUENCH_SWORD);

		if (isBlackSword(item)) {
			item->setCooledFrame();
		} else {
			if (get_item_quality() == SWORDBLANK_QUENCH) {
				set_item_quality(SWORDBLANK_READY);
				AVATAR.say("The water hisses and bubbles as you sink the red-hot swordblank into it.",
					"~When you draw it out, the cold blade feels lighter in your hand and has taken on an deadly sheen.");
			}
			item->setCooledFrame();
		}

		//Empty the trough
		script trough repeat 2 {
			previous frame;
		};
	}
}
