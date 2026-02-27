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
 *	This header file contains several more flags from Black Gate.
 *	It is mostly unsorted... although generally grouped by similarity.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//FoV flags:
enum fov_flags {
	TALKED_ABOUT_RUMBLE				= 0x02FE,
	BANISHED_EXODUS					= 0x030C,
	SCROLL_OF_INFINITY				= 0x030E,
	MET_ERETHIAN					= 0x0310,
	MET_ARCADION					= 0x0313,
	IMPROVED_BLADE					= 0x032D,
	BROKE_MIRROR					= 0x032F,
	COMMANDED_BOND					= 0x0330,
	REFUSED_HELP_ARCADION			= 0x0331,
	HELPING_ARCADION				= 0x0332,
	TALKED_ARCADION_WITH_GEM		= 0x0333,
	ACCEPTED_GEM_POWER				= 0x0333,
	ARCADION_SLAVE					= 0x0334,
	REFUSED_GEM_POWER				= 0x0335,
	FINISHED_BLADE_FORGING			= 0x0337,
	TALKED_ABOUT_BINDING_GEM		= 0x0338
	// Flags from the original which are unused in the mod:
	//BEGIN_BOND					= 0x0343,
	//FINISHED_BOND					= 0x0344
};

//Some Cove flags:
enum cove_flags {
	MET_JAANA						= 0x0018,
	ASKED_JAANA_NAME				= 0x00EF,
	KNOWS_COVE_GOSSIP				= 0x00E4,
	KNOWS_JAANA_IS_HEALER			= 0x0028,
	HEARD_ABOUT_NATASSIA			= 0x00E0,
	HAS_MIRANDAS_BILL				= 0x006A,
	MET_LORD_HEATHER				= 0x00EA,
	KNOWS_DEMARIA_KNOWS				= 0x00E3,
	LOCK_LAKE_BILL_SIGNED			= 0x00DE
};

enum serpent_hold_flags {
	KNOWS_LEIGH_IS_HEALER			= 0x027A,
	CAN_EXAMINE_STONE_CHIPS			= 0x0259,
	DOING_STATUE_QUEST				= 0x025E,
	EXAMINED_CHIPS					= 0x025F
};

enum terfin_flags {
	KNOWNS_ABOUT_CONFLICTS			= 0x0244,
	KNOWS_SILAMO_UNHAPPY			= 0x023D
};

//Some metaplot-related flags:
enum metaplot_flags {
	TALKED_TO_GYPSY					= 256,	//0x100;
	HAD_FIRST_WISP_TALK				= 336,	//0x150;
	WISP_OFFERED_TRADE				= 307,	//0x133;
	DELIVERED_NOTEBOOK_TO_WISPS		= 343,	//0x157;
	LEARNED_ABOUT_ALIGNMENT			= 295	//0x127
};

//Flags used in the Improved Orb of the Moons:
enum orb_flags {
	BROKE_SPHERE					= 4,	//0x0004
	LEFT_TRINSIC					= 87	//0x0057
};

//Some Yew flags, used in reimplementation of Perrin and Reyna
//for the end of the Keyring Quest:
enum yew_flags {
	MET_PERRIN						= 0x0145,
	KNOWS_REYNA_LOVES_ANIMALS		= 0x013B,
	MET_REYNA						= 0x0146,
	ASKED_REYNA_ABOUT_MOTHER		= 0x0128,
	KNOWS_REYNA_JOB					= 0x0163,
	REYNA_EMERGENCY					= 0x013A,
	GAVE_REYNA_FLOWERS				= 0x0139
};

const int MET_MARIAH				= 0x01FB;

//Set when the Magic Storm spell is cast:
const int MAGIC_STORM_SPELL			= 0x02ED;

//Prevents casting of spells because the Avatar is inside
//one of the generators:
const int INSIDE_GENERATOR			= 0x39;

//Seance flags; I have NO idea why they didn't go with just
//ONE flag instead...
enum seance_flags {
	SEANCE_CAINE					= 0x01B2,
	SEANCE_FERRYMAN					= 0x01B3,
	SEANCE_MARKHAM					= 0x01B4,
	SEANCE_HORANCE					= 0x01B5,
	SEANCE_TRENT					= 0x01B6,
	SEANCE_MORDRA					= 0x01B7,
	SEANCE_ROWENA					= 0x01B8,
	SEANCE_PAULETTE					= 0x01B9,
	SEANCE_QUENTON					= 0x01BA,
	SEANCE_FORSYTHE					= 0x01BB
};
