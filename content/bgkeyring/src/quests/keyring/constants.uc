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
 *	This header file contains the relevant constants used throughout usecode for
 *	the Keyring Quest.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Event levels for the keyring cutscenes:
const int EVENT_TELEPORT				= 15;	//0x00F
const int CLEAR_FLAGS					= 16;

//NPC IDs used for the Mage and his goons and for Joneleth. These
//IDs are set when they are created, and are used to differentiate
//them from ordinary monsters in usecode.
const int ID_MAGE_OR_GOON				= 31;
const int ID_JONELETH					= 31;

//Global indicator of progress for the Keyring Quest:
enum Keyring_quest_levels {
	NOT_STARTED							= 0,
	QUEST_ACCEPTED						= 1,
	NO_ONE_THERE						= 2,
	TOLD_ABOUT_GEM						= 3,
	PLAYER_HAS_GEM						= 4,
	GEM_USED							= 5,
	PLAYER_KILLED_MAGE					= 6,
	POTION_WAS_USED						= 7,
	LAURIANNA_IS_CURED					= 8,
	LAURIANNA_MOVED_TO_YEW				= 9
};

//The quest timer; it is not used for now:
const int LAURIANNA_TIMER				= 15;

//Relevant faces:
const int KEYRING_ENEMY					= -294;
const int LAUNDO_FACE					= 0;
const int JONELETH_FACE					= 1;
