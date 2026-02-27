/*
 *
 *  Copyright (C) 2006  Alun Bestor
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
 *	This header file defines constants for shapes and frames in Black Gate's
 *	SHAPES.VGA. Fill them in as you go along! It is best to use enums to group
 *	shapes thematically by type and frames by shape, rather than attempting to
 *	order them numerically.
 *
 *	Author: Alun Bestor (exult@washboardabs.net)
 *	Last Modified: 2006-03-19
*/

//Who the player has met. BG used global flags for this because the MET item flag didn't originally exist.
enum met_flags {
	MET_LORD_BRITISH	= 0x98,
	MET_FINNIGAN		= 0x4C,
	MET_NYSTUL			= 0x99,
	MET_CHUCKLES		= 0x9A,
	MET_CANDICE			= 0xAA,
	MET_WILLY			= 0xB5,
	MET_GORDON			= 0xBB,
	MET_CSIL			= 0xA4,

	MET_JULIA			= 0x1B,
	MET_GLADSTONE		= 0x110,

	MET_CAMILLE			= 0x22A,

	MET_MENION			= 0x269,
	MET_HORFFE			= 0x26E,
	MET_TORY			= 0x271,

	MET_ELAD			= 0x204,

	MET_LEIGH			= 0x272,

	MET_INMANILEM		= 0x247,

	MET_CHANTU			= 0x52
};

//Flags used by the starting murder investigation quest
enum trinsic_murder_flags {
	GOT_CHRISTOPHERS_KEY		= 0x3C,
	GOT_TRINSIC_PASSWORD		= 0x3D,
	NEEDS_TRINSIC_PASSWORD		= 0x42,
	UNLOCKED_CHRISTOPHERS_CHEST	= 0x3E,

	KNOWS_ABOUT_CHRISTOPERS_ARGUMENT	= 0x3F,
	REFUSED_MURDER_INVESTIGATION		= 0x59,
	EXPECTED_TO_LOOK_IN_STABLES			= 0x5A,
	REPORTED_CHRISTOPHERS_KEY			= 0x48,
	CAN_GIVE_MURDER_REPORT				= 0x5B,
	STARTED_MURDER_REPORT				= 0x5D,
	LEARNED_ABOUT_CROWN_JEWEL			= 0x40,
	LEARNED_ABOUT_HOOK					= 0x43,
	FINISHED_MURDER_INVESTIGATION		= 0x44,
	WAITING_FOR_INVESTIGATION_PAYMENT	= 0x45
};

//Broken the tetrahedron generator that screws up magic. This affects most conversations with wizards.
const int BROKE_TETRAHEDRON		= 0x03;
const int BROKE_CUBE			= 0x04;
const int ORB_FIXED_TIMELORD	= 0x134;

//Used the armageddon spell
const int CAST_ARMAGEDDON = 0x1E;


//Flags used in conversation with Lord British
enum lb_conversation_flags {
	WESTON_FREED					= 0xCC,
	AGREED_TO_FREE_WESTON			= 0xCD,
	GOT_ORB							= 0xDD,
	ASKED_LB_ABOUT_MAGIC			= 0x66,
	LEARNED_ABOUT_BRITAIN_MURDER	= 0xD1, //allows you to ask Patterson about it
	ASKED_LB_ABOUT_HEAL				= 0xD3,
	LEARNED_ABOUT_BLACKROCK			= 0x65,	//allows you to ask Rudyom about it?
	ASKED_LB_ABOUT_GUARDIAN			= 0xD4,	//stops you ever asking him again (yes, plaster over those plot-holes)
	LB_REWARDED_FOR_FV				= 0x30D	//Received Lord British's reward for completing the Forge of Virtue
};

//Flags used in conversation with Julia
enum julia_conversation_flags {
	ASKED_JULIA_TO_LEAVE		= 0x101,
	JULIA_IN_PARTY				= 0x108,
	ASKED_JULIA_ABOUT_SPARK		= 0x121
};

//Flags used in conversation with Tseramed
enum tseramed_conversation_flags {
	TSERAMED_THINKS_MAGE		= 0x15E
};

//Flags used in Minoc conversations to do with the sawmill murder
enum minoc_murder_flags {
	LEARNED_ABOUT_MINOC_MURDER	= 0x11F
};

//Flags used by the Owen's Monument quest
enum owen_monument_flags {
	COMPLETED_OWENS_QUEST		= 0xF7,
	LEARNED_ABOUT_PLANS			= 0x10B,
	OWENS_PLANS_ARE_UNSAFE		= 0xFD
};

//Batlin has buggered off (after you talk to him with the cube in your possession)
const int BATLIN_MISSING = 0xDA;

//Heard that Patterson is having an affair with Candice
const int HEARD_ABOUT_PATTERSONS_AFFAIR = 0x80;

//Heard about the Inner Voice
const int HEARD_ABOUT_VOICE = 0x8C;

const int JEANETTE_LOVES_WILLY = 0x85;	//heard about Jeanette's crush on Willy the Baker
const int HIRED_BY_WILLY = 0xCB;		//hired by Willy to bake bread

//Miscellaneous flags for NPCs in Paws
enum paws_flags {
	HEARD_ABOUT_PAWS_THEFT	= 0x212,	//Heard from an NPC about the theft
	TOBIAS_ACCUSED			= 0x213,	//Feridwyn accused Tobias of the theft
	SOLVED_PAWS_THEFT		= 0x218,	//Found Garritt was the thief
	HEARD_CAMILLES_DEFENSE	= 0x234,	//Camille has run out to say that her son is innocent of the theft (prevents conversation recurring)
	GOT_CAMILLES_WHEAT		 = 0x21A,	//Given the wheat by Camille (prevents her offering it again)
	DELIVERED_CAMILLES_WHEAT = 0x21D	//Delivered the wheat to Thurston (so you don't get paid for it twice)
};

enum serpents_hold_flags {
	STARTED_HOLD_INVESTIGATION = 0x25E,		//Got the mission from John-Paul to find who defaced the statue
	FINISHED_HOLD_INVESTIGATION = 0x261,	//Finished the statue investigation properly, pinning Sir Pendaran as the culprit
	HEARD_ABOUT_RIKY	= 0x277,			//Heard that Tory's baby son was kidnapped by harpies
	RESCUED_RIKY		= 0x278				//Player has rescued Riky (prevents conversation from coming up again)
};
