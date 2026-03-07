/*
 *
 *  Copyright (C) 2026  The Exult Team
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

/*
 *  One of the requests for SI Fixes is to have a hint to call the Hound to
 *  track Batlin by letting him sniff Batlin's medallion. Players may miss this
 *  step because it's already established that Batlin and his henchmen have
 *  cold weather gear. The game requires sniffing the medallion to go forward.
 *  This will cause Shamino to speak up only if the Hound has not already been
 *  called for this tracking.
 *
 *  2026-03-06 Knight Captain
 */

// "Deprecated; use Usecode C script blocks instead."
// But there is little documentation of those script blocks.
const byte no_halt = (byte)0x23;
const byte call_usecode = (byte)0x55;

void eggShaminoSeesHisCastle object#(0x6F7) () // 1783
{
	var action;
	var batlins_medallion;

	if ((event == EGG) && UI_get_item_flag(SHAMINO, IN_PARTY))
	{
		if (!gflags[SHAMINO_SAW_HIS_CASTLE])
		{
			delayedBark(SHAMINO, "@I know this place!@", 2);
			action = UI_delayed_execute_usecode_array(AVATAR, [no_halt, call_usecode, 1783], 12);
		}
	}
	
	if ((event == EGG) && UI_get_item_flag(CANTRA, SI_ZOMBIE) && UI_get_item_flag(SHAMINO, IN_PARTY))
	{
		if (!gflags[HOUND_TRACKED_BATLIN] && !gflags[HOUND_HINT_BATLIN])
		{
			delayedBark(SHAMINO, "@Avatar...@", 3);
			action = UI_delayed_execute_usecode_array(AVATAR, [no_halt, call_usecode, 1783], 13);
			// Remove the egg. Not required thanks to the above check.
			// UI_remove_item(item);
		}
	}

	if (event == SCRIPTED)
	{
		if (!gflags[SHAMINO_SAW_HIS_CASTLE])
		{
			UI_clear_item_say(SHAMINO);
			UI_show_npc_face0(FACE_SHAMINO, 0);
			say("@I never thought to see this again!@");
			say("@Avatar, this castle thou dost see before thee was once mine home. This was once my kingdom -- And once, so long ago that mine heart alone remembers, within these walls dwelt the fairest lady who ever lived... Oh, my poor Beatrix! Left alone so very long ago...@");
			say("@But such remorse doth do us no good now... She is long since dust, I fear, and our peril now is real. If thou wishest, I can draw thee a map of mine home, as once I knew it.@");

			if (askYesNo())
			{
				say("@I do not know what changes were made after I left, Avatar. But perhaps this will help.@");
				action = giveItemsToPartyMember(AVATAR, 1, SHAPE_MAP, QUALITY_ANY, FRAME_SHAMINOS_MAP, false, true);
			}
			else
			{
				say("@'Tis just as well... I do not know what changes were made after I left. It probably would not help.@");
			}

			say("@I seem to recall that there was a secret entrance to the west...@");
			say("@But that was a long time ago. I doubt that the two great trees near the illusionary mountain entrance are still there. The secret entrance would be most difficult to find should its landmarks be gone.@");
			say("@'Tis a pity that so much hath changed without my being here...@");
			
			// Set the flag so you don't get the first reaction twice.
			gflags[SHAMINO_SAW_HIS_CASTLE] = true;
			UI_error_message("Shamino saw his castle for the first time in years.");
			// In the case that the player comes in the secret entrance without ever approproaching the front gate,
			// we will also show the Batlin medallion hint after this, so the hint is not missed.
		}
		else if (UI_get_item_flag(CANTRA, SI_ZOMBIE))
		{
			UI_error_message("Cantra is a zombie.");
			if (hasItemCount(PARTY, 1, SHAPE_AMULET, QUALITY_ANY, FRAME_BATLINS_MEDALLION))
			{
				UI_error_message("The Party has Batlin's medallion in their possession.");
				UI_clear_item_say(SHAMINO);
				UI_show_npc_face0(FACE_SHAMINO, 0);
				say("@Now that we are outside of the walls, calling the Hound again may help us track down Batlin.@");
			}
			else // No medallion.
			{
				UI_error_message("The Party does NOT have Batlin's medallion in their possession.");
				UI_clear_item_say(SHAMINO);
				UI_show_npc_face0(FACE_SHAMINO, 0);
				say("@Did any of us pick up Batlin's medallion? With it, the Hound might help us track down Batlin.@");
			}
			// Set the flag so you don't get the hint twice.
			gflags[HOUND_HINT_BATLIN] = true;
		}
	}

	else {
		return; }
}
