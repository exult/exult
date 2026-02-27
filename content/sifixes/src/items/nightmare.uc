/*
 *
 *  Copyright (C) 2023  The Exult Team
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

/*  2023-03-15 Smith in the Dream Realm has no corresponding face with his
 *	NPC number. The original usecode was set to use the Guardian's face at
 *	number 296. The original game engine just ignored this error, and did not
 *	display a face. Now we have new artwork from Rhona, so this enables it.
 *	Smith's conversation has been put into new "say" lines for compatibility
 *	across different video resolutions.
 */

void shapeSmithDreamRealm shape#(0x375) () {
	struct<Position> nightmare_pos;

	if (event == STARTED_TALKING) {
		FACE_SMITHZHORSE.say("@Avatar! How good it is to see thee! How long hath it been since we last met?@");
		AVATAR.say("@Ahhhh... Errrr... Well....@");
		AVATAR.hide();

		FACE_SMITHZHORSE.say("@Do not tell me that thou dost not recognize me? Come now, Avatar, it hath not been that long.@");
		AVATAR.say("@Welll... Maybe...@");
		AVATAR.hide();

		// Split some lines for Exult readability.
		FACE_SMITHZHORSE.say("@It is I! Smith the Horse! Only the finest steed in all of Britannia.@");
		say("@Oh, I suppose thou wouldst not recognize me such as I am. This is the appearance I take here at the Realm of Dreams.@");
		say("@Ah, this is the life. Mine own keep, as much hay as I could want, no insects to bother me... Who could ask for more?@");
		say("@Where are my wits! Avatar, I have something important to tell thee. But what was it?@");
		say("@Oh, yes! Avatar, thou must take Rudyom's wand to the Isle of the Avatar!@");
		say("@There thou wilt find a monolith made of blackrock that Batlin is using to create a gateway to bring the Guardian into our world!@");
		say("@The future of Britannia doth lie in thine hands, Avatar!@");
		AVATAR.say("@Ahh, Smith, the Guardian hath been stopped. Everything is safe.@");
		AVATAR.hide();

		FACE_SMITHZHORSE.say("@Oh...\" *\"I feel like I have made a mule of myself. Never mind...@");
		say("@Now I remember! Thou hast left Britannia and hast journeyed to the Serpent Isle in search of Batlin!@");
		say("@Poor Iolo, he must be distraught to have found that Gwenno left on that voyage with that fiend. I hope that thou wilt find her before that evil man doth do something to her.@");
		say("@She, at least, hath always been kind to me.@");
		say("@Wait! Avatar, I do have some information thou canst use!\" *\"Batlin and his band of hired swords are waiting for thee at...@");
		say("@Yummie! Here comes someone with more hay...@");
		FACE_SMITHZHORSE.hide();

		nightmare_pos = get_object_position();
		UI_sprite_effect(SPRITE_PURPLE_CIRCLES, nightmare_pos.x, nightmare_pos.y, 0, 0, 0, -1);
		remove_item();
		// Weird warp sound
		UI_play_sound_effect(76);
	} else if (event == SCRIPTED) {
		// Freedom scene with the death of Sabrina
		AVATAR->clear_item_flag(DONT_MOVE);
		clear_item_flag(SI_TOURNAMENT);
		nightmare_pos = get_object_position();
		UI_sprite_effect(SPRITE_MEDIUM_EXPLOSION, (nightmare_pos.x - 1), (nightmare_pos.y - 1), 0, 0, 0, -1);
		UI_sprite_effect(SPRITE_WHITE_LIGHTNING, (nightmare_pos.x - 1), (nightmare_pos.y - 1), 0, 0, 2, 3);
		UI_sprite_effect(SPRITE_WHITE_LIGHTNING, (nightmare_pos.x - 2), (nightmare_pos.y - 2), 0, 0, 0, -1);
		UI_play_sound_effect(42);
		UI_lightning();
		removeAllItemsAndContainer(item);
	}
	return;
}
