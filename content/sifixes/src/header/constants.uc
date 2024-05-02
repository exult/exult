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
 *
 *	This header file defines general constants used throughout usecode, for
 *	Black Gate and Serpent Isle. Constants particular to a function can be
 *	found in the relevant codefile; constants particular to a game can be
 *	found in that game's header directory.
 *
 *	Author: Alun Bestor (exult@washboardabs.net)
 *	With modifications by Marzo Junior (marzojr@yahoo.com)
 *	Last modified: 2006-02-27
 */

/*
 *	Event types, compiled with help from Marzo
 *	The global <event> variable is set with one of these values to describe
 *	how the current function was called: whether by the player clicking on the
 *	object, or by a scripted event, or by egg trigger conditions, or just the
 *	item being onscreen, etc.
 *	Functions check the value of <event> in order to provide different responses
 *	to different events.
 *	Functions can also set <event> to an arbitrary value, in order to mimic a
 *	real event or just as a 'pseudo-argument' to functions called with the
 *	likes of UI_path_run_usecode.
 */

const int LOOP_ONCE = -1;

enum events {
	PROXIMITY		= 0,	// Object is on-screen or nearby
							// This is called repeatedly, with a random delay
							// between each call
	DOUBLECLICK		= 1,	// Object is double-clicked on
	SCRIPTED		= 2,	// Function is called from inside a script{} block
							// (very common)
	EGG				= 3,	// Object is an egg that just hatched (triggered by
							// egg activation conditions)
	WEAPON			= 4,	// Object was wielded and 'swung' in combat
							// This is mainly used with 'weapon-like' objects
							// - e.g. smokebombs and fishing rods - that have more
							// advanced 'attack' behaviour.

	READIED			= 5,	// Object was worn or readied in inventory - used by
							// items like the Ring of Invisibility
	UNREADIED		= 6,	// Object was taken off or put away in inventory

	DEATH			= 7,	// NPC has just been killed (SI-only)
	STARTED_TALKING	= 9,	// NPC starts conversation with you (has TALK
							// schedule and has reached the Avatar)
							// This is SI-only - BG uses event 1 for this,
							// both for conversations triggered by doubleclick
							// and by the TALK schedule

	//The following events are arbitrary programmer conventions:
	BG_PATH_SUCCESS	= 7,	// Set with calls to UI_path_run_usecode, to indicate
							// a successful pathfind to the target object
	BG_PATH_FAILURE	= 8,	// Set with calls to UI_set_path_failure, to indicate
							// an interrupted pathfind (e.g. when the player
							// moves the Avatar manually)
	PATH_SUCCESS_9	= 9,	// Set with calls to UI_path_run_usecode, to indicate
							// a successful pathfind to the target object
	PATH_SUCCESS	= 10,	// Set with calls to UI_path_run_usecode, to indicate
							// a successful pathfind to the target object
	PATH_FAILURE	= 11,	// Set with calls to UI_set_path_failure, to indicate
							// an interrupted pathfind (e.g. when the player
							// moves the Avatar manually)
	SI_PATH_SUCCESS	= 13,	// Set with calls to UI_path_run_usecode, to indicate
							// a successful pathfind to the target object
	SI_PATH_FAILURE	= 14	// Set with calls to UI_set_path_failure, to indicate
							// an interrupted pathfind (e.g. when the player
							// moves the Avatar manually)
};

/*
 *	Cardinal directions: returned by UI_get_direction. Use with face or step
 *	statements in script{} blocks. e.g:
 *	script item { step NORTH; step NORTH; face SOUTH; }
 *	takes two steps north, then turn to face south
 */
enum directions {
	NORTH		= 0,
	NORTHEAST	= 1,
	EAST		= 2,
	SOUTHEAST	= 3,
	SOUTH		= 4,
	SOUTHWEST	= 5,
	WEST		= 6,
	NORTHWEST	= 7
};

/*
 *	parts of the day (3-hour intervals). Returned by UI_part_of_day. This is
 *	usually only used by conversation scripts, as a way of narrowing down
 *	schedule-related behaviour.
 */
enum day_periods {
	// The period is equal to one-third of the current game hour.
	MIDNIGHT		= 0,	// 0-2
	EARLY			= 1,	// 3-5
	DAWN			= 2,	// 6-8
	MORNING			= 3,	// 9-11
	NOON			= 4,	// 12-14
	AFTERNOON		= 5,	// 15-17
	EVENING			= 6,	// 18-20
	NIGHT			= 7		// 21-23
};

/*
 *	Wildcards, used for specifying "any acceptable value for this parameter"
 *	to a measuring function. These are commonly used with object-related
 *	intrinsics like UI_get_cont_items, UI_count_objects, UI_remove_party_items.
 */
enum wildcards {
	SHAPE_ANY	= -359,
	QUALITY_ANY	= -359,
	FRAME_ANY	= -359
};

// tick multipliers, for use with UI_advance_time or script statements
enum times {
	MINUTE	= 25,
	HOUR	= 1500
};
const long DAY		= 36000;
/*
 *	Examples:
 *	UI_advance_time(30 * MINUTE); // advance time by 30 game minutes
 *	UI_advance_time(2 * HOUR); // advance time by two game hours
 *	script after MINUTE ticks {  ... } // schedule this script block to execute
 *	                       // after one game minute
 */

/*
 *	Item/NPC flags, stolen from the Exult LB-joins-party patch with some
 *	comments added.
 *	These can be retrieved and set using UI_get_item_flag(itemref, flag),
 *	UI_set_item_flag(itemref, flag) and UI_clear_item_flag(itemref, flag).
*/
enum item_flags {
	INVISIBLE			= 0,
	ASLEEP				= 1,
	CHARMED				= 2,
	CURSED				= 3,
	DEAD				= 4,
	IN_PARTY			= 6,
	PARALYZED			= 7,
	PARALYSED			= 7,		//British spelling
	POISONED			= 8,
	PROTECTION			= 9,
	ON_MOVING_BARGE		= 10,
	OKAY_TO_TAKE		= 11,		// The item does not belong to anyone, and won't
									// trigger stealing behaviour if you take it.
	MIGHT				= 12,		// Double strength, dext, intel.
	IMMUNITIES			= 13,		// Lots of immunities; can be gotten only.
	CANT_DIE			= 14,		// Test flag in monster_info.
	IN_ACTION			= 15,		// If the NPC is doing something. Set by "Dance" spell in BG.
	DONT_MOVE			= 16,		// SI-only. NPC can't move; "cutscene" mode.
	DONT_RENDER			= 16,		// BG-only. Like DONT_MOVE, but avatar also
									// completely invisible.
	TEMPORARY			= 18,		// Is temporary - this means that the item will
									// be deleted once the party gets beyond a certain
									// range from it (outside the superchunk?)
	ACTIVE_SAILOR		= 20,		// The barge's 'captain'. When getting the flag,
									// you will actually get the current captain.
	OKAY_TO_LAND		= 21,		// Used for flying-carpet.
	BG_DONT_MOVE		= 22,		// BG-only. NPC can't move; "cutscene" mode.
	SI_DONT_RENDER		= 22,		// SI-only. Like DONT_MOVE, but avatar also
									// completely invisible.
	IN_DUNGEON			= 23,		// If set, you won't be accused of stealing.
	IS_SOLID			= 24,		// Used by gangplank usecode to determine if an object is solid.
	CONFUSED			= 25,
	ACTIVE_BARGE		= 26,		// Object is a barge object moving, or on a barge
									// object that is moving. Set in usecode, and mostly
									// used for the SI 'NPC' ships such as the turtle.
	MET					= 28,		// Has the npc been met before - originally this
									// was SI-only, but Exult implements it for BG
									// too. This determines conversation behaviour,
									// and whether the NPC's real name or shape name
									// is displayed when they are single-clicked on.
									// BG originally used global flags for this, which
									// amounts to an extra 250-odd flags.
	SI_TOURNAMENT		= 29,		// Call usecode (eventid = 7) on death
	SI_ZOMBIE			= 30,		// Used for sick Neyobi, Cantra, post-Bane companions.

	NO_SPELL_CASTING	= 31,
	POLYMORPH			= 32,		// Do not set this flag directly; use the
									// UI_set_polymorph intrinsic instead.
	TATTOOED			= 33,
	READ				= 34,		// Can read non-Latin alphabet scrolls, books, signs.
	ISPETRA				= 35,
	CAN_FLY				= 36,
	FREEZE				= 37,
	NAKED				= 38 		// Exult. Makes the avatar naked given its skin.
									// Other NPCs should use set_polymorph instead.
};

// Business activities (taken from the cheat screen)
enum schedules {
	IN_COMBAT		= 0,	// renamed to not conflict with COMBAT, the NPC
							// stat property.
	PACE_HORIZONTAL	= 1,	// Walk horizontally until you hit a wall, then
							// turn around. (Patrolling on the cheap.)
	PACE_VERTICAL	= 2,	// Same as above, but vertically.
	TALK			= 3,	// NPC runs to the Avatar to talk to them. When
							// they get within a certain distance of where the
							// Avatar was when this schedule was set, a
							// STARTED_TALKING event is triggered on the NPC.
							// At this point the schedule must be changed.
	DANCE			= 4,
	EAT				= 5,
	FARM			= 6,	// Waves farm implements around.
	TEND_SHOP		= 7,	// This is really just a more specific version of
							// LOITER, used for narrowing down schedule barks.
							// See BAKE, SEW and BLACKSMITH for more specific
							// examples of shop behaviour.
	MINE			= 8,
	MINER			= 8,
	HOUND			= 9,
	STANDTHERE		= 10,	// renamed to not conflict with STAND, the NPC
							// animation frame
	LOITER			= 11,	// Hangs around a certain point, within 10 units
							// or so
	WANDER			= 12,	// Roams nearby a certain point (as much as a
							// 320x200 screen away)
	BLACKSMITH		= 13,
	SLEEP			= 14,
	WAIT			= 15,	// Similar to STAND, except that they will never
							// leave the WAIT schedule until it is manually
							// changed: their preset schedule list is ignored.
	MAJOR_SIT		= 16,
	GRAZE			= 17,
	BAKE			= 18,
	SEW				= 19,
	SHY				= 20,	// Tries to keep out of the Avatar's way - will
							// half-heartedly flee until out of a certain range.
	LAB				= 21,
	THIEF			= 22,	// Approaches the party and will take gold from the Avatar's
							// backpack, then will bark "Greetings!" in SI.
	WAITER			= 23,
	SPECIAL			= 24,	// ??
	KID_GAMES		= 25,	// Tag! Thou art it! And so forth.
	TAG				= 25,
	EAT_AT_INN		= 26,	// same as Eat, only with different barks.
	DUEL			= 27,
	SPAR			= 27,
	PREACH			= 28,	// Broken in SI, Leon resets to Loiter after trying it.
	PATROL			= 29,	// This tells the AI to follow a particular set of
							// patrol waypoints, defined by path eggs.
	DESK_WORK		= 30,
	FOLLOW_AVATAR	= 31	// That most noble of pursuits. Like WAIT, this
							// completely overrides the NPC's schedule list.
};

/*
 *	NPC animation frames. Use these with UI_set_item_frame or (preferably) in
 *	script blocks, with 'actor frame'.
 *	e.g.: script AVATAR {  actor frame STAND; actor frame USE; actor frame
 *	SWING_1; actor_frame STAND; }
 *	Important note: use 'actor frame' with NPCs instead of 'frame', as 'actor
 *	frame' takes the NPC's current facing into account.
 */
enum npc_frames {
	STAND		= 0,
	WALK_1		= 1,
	WALK_2		= 2,

	USE			= 3,	//general use motion

	SWING_1		= 4,	//start of one-handed swing, arm up over shoulder
	SWING_2		= 5,	//middle of one-handed swing, arm out to the side
	SWING_3 	= 6,	//end of one-handed swing, arm out to the front

	SWING_2H_1 	= 7,	//start of 2-handed swing, arms up over shoulder
	SWING_2H_2 	= 8,	//middle of 2-handed swing, arms out to the side
	SWING_2H_3	= 9,	//end of 2-handed swing, arms out to the front

	SIT			= 10,	//sitting down
	LEAN		= 11,	//leaning down
	KNEEL		= 12,	//kneeling on one knee
	LIE			= 13,	//lying down
	CAST_1		= 14,	//both arms high in the air (casting motion)
	CAST_2		= 15	//both arms stretched out (casting motion)
};

// North/South/East/West frame offsets for the NPC frames. Only really necessary
// if you're using UI_set_item_frame or 'frame'
enum frame_offsets {
	NORTH_FRAMESET	= 0,
	SOUTH_FRAMESET	= 16,
	WEST_FRAMESET	= 32,
	EAST_FRAMESET	= 48
};

// Ready slots for Black Gate: use with <UI_is_readied> or <UI_get_readied>.
// These are the same whether paperdolls are on or off.
enum inv_slots_bg {
	BG_BACKPACK		=  0,
	BG_WEAPON_HAND	=  1,
	BG_SHIELD_HAND	=  2,
	BG_OFF_HAND		=  2,
	BG_BELT			=  3,
	BG_NECK			=  4,
	BG_TORSO		=  5,
	BG_LEFT_RING	=  6,
	BG_RIGHT_RING	=  7,
	BG_QUIVER		=  8,
	BG_HEAD			=  9,
	BG_LEGS			= 10,
	BG_FEET			= 11,
	BG_USECODE		= 12,	// Exult-only; ported from SI
	BG_CLOAK		= 13,	// Exult-only; ported from SI
	BG_GLOVES		= 14,	// Exult-only; ported from SI
	BG_TRIPLE_BOLTS	= 15,
	BG_EARS			= 16,	// Exult-only; ported from SI
	BG_BACK_SHIELD	= 17,	// Exult-only; ported from SI
	BG_BACK_2H		= 19,	// Exult-only; ported from SI
	BG_BOTH_HANDS	= 20,
	BG_LRGLOVES		= 21,	// Uses both ring slots
	BG_AMULET		= 22	// Exult-only; ported from SI
};

// Ready slots for Serpent Isle: use with <UI_is_readied> or <UI_get_readied>.
// These are the same whether paperdolls are on or off.
enum inv_slots_si {
	SI_SHIELD_HAND	=  0,
	SI_OFF_HAND		=  0,
	SI_WEAPON_HAND	=  1,
	SI_CLOAK		=  2,
	SI_AMULET		=  3,
	SI_HEAD			=  4,
	SI_GLOVES		=  5,
	SI_USECODE		=  6,
	SI_RIGHT_RING	=  7,
	SI_LEFT_RING	=  8,
	SI_EARS			=  9,
	SI_QUIVER		= 10,
	SI_BELT			= 11,
	SI_TORSO		= 12,
	SI_FEET			= 13,
	SI_LEGS			= 14,
	SI_BACKPACK		= 15,
	SI_BACK_SHIELD	= 16,
	SI_BACK_2H		= 17,
	SI_TRIPLE_BOLTS	= 18,	// Exult-only; ported from BG
	SI_BOTH_HANDS	= 20,
	SI_LRGLOVES		= 21	// Exult-only; ported from BG ; uses both ring slots
};

/*
 *	NPC properties (mostly ability scores)
 *	These can be retrieved and set using UI_get_npc_property(npc, property) and
 *	UI_set_npc_property(npc, property, value) respectively.
 *	Note however that UI_set_npc_property will actually *add* the value to the
 *	original property, not set it to that value. Which means you will need to
 *	calculate a relative positive/negative adjustment to set it to a target
 *	value.
 */
enum npc_properties {
	STRENGTH		= 0,
	DEXTERITY		= 1,
	INTELLIGENCE	= 2,
	HEALTH			= 3,
	COMBAT			= 4,
	MANA			= 5,
	MAX_MANA		= 6,
	TRAINING		= 7,
	EXPERIENCE		= 8,
	FOODLEVEL		= 9,
	SEX_FLAG		= 10,	// 1 (nonzero) if female, 0 if male.
	MISSILE_WEAPON	= 11	// Cannot be set; returns 1 if wearing a missile
							// or (good) thrown weapon, 0 otherwise.
};


// NPC attack behaviours. Retrieve and set using UI_set_attack_mode(npc, mode)
// and UI_get_attack_mode(npc).
enum npc_attack_modes {
	NEAREST		= 0,
	WEAKEST		= 1,
	STRONGEST	= 2,
	BERSERK		= 3,
	PROTECT		= 4,
	DEFEND		= 5,
	FLANK		= 6,
	FLEE		= 7,
	RANDOM		= 8,
	MANUAL		= 9
};

/*
 *	Failure cursor constants for use with UI_flash_mouse. Note that these do not
 *	correspond to frame numbers in pointers.uc, but to some internal mapping.
 *	(the "BLOCKED" cursor seems to be unavailable through this method.)
 */
enum cursors {
	CURSOR_X			= 1,	//Default "no you can't do that" X cursor
	CURSOR_OUT_OF_RANGE = 2,
	CURSOR_OUT_OF_AMMO	= 3,
	CURSOR_TOO_HEAVY	= 4,
	CURSOR_WONT_FIT		= 5
};

enum egg_states {
	CACHED_IN = 0,			// Activated when chunk read in?
	PARTY_NEAR = 1,
	AVATAR_NEAR = 2,		// Avatar steps into area.
	AVATAR_FAR = 3,			// Avatar steps outside area.
	AVATAR_FOOTPAD = 4,		// Avatar must step on it.
	PARTY_FOOTPAD = 5,
	SOMETHING_ON = 6,		// Something placed on/near it.
	EXTERNAL_CRITERIA = 7	// Appears on Isle of Avatar.  Guessing
};

//NPC attitudes toward the avatar
enum alignments {
	NEUTRAL			= 0,
	GOOD			= 1,
	EVIL			= 2,
	CHAOTIC			= 3
};

enum weather_types {
	CLEAR_WEATHER	= 0,
	SNOWING			= 1,	//Unsure; in Exult, works the same as RAIN, below and seems identical to RAIN in the originals
	RAIN			= 2,
	SPARKLE			= 3,	//Prevents casting of spells in BG (and maybe SI too?)
	FOG				= 4,	//Seems to work like RAIN in the originals
	OVERCAST		= 5,	//Seems to clear weather in the originals
	CLOUDY			= 6
};

//Mask values used (e.g.) in find_nearby intrinsic; these are flags, which mean
//that they can be added together
enum item_masks {
	MASK_NONE				= 0x0,
	MASK_NPC				= 0x04,		//Only find NPCs, alive or dead
	MASK_NPC2				= 0x08,		//Only find living NPCs
	MASK_EGG				= 0x10,		//Also for barges
	MASK_INVISIBLE			= 0x20,
	MASK_PARTY_INVISIBLE	= 0x40,
	MASK_TRANSLUCENT		= 0x80,
	MASK_ALL_UNSEEN			= 0xB0		//MASK_EGG+MASK_INVISIBLE+MASK_TRANSLUCENT
};

//NPC animation frames, WITH rotation bit. Use these with UI_set_item_frame_rot
//or (preferably) in script blocks, with 'frame'.
//e.g.: script AVATAR {  frame STAND_WEST; frame USE_NORTH; frame SWING_1_SOUTH; frame STAND_EAST; }
enum npc_rot_frames {
	STAND_NORTH			= 0x0000,
	WALK_1_NORTH		= 0x0001,
	WALK_2_NORTH		= 0x0002,
	USE_NORTH			= 0x0003,
	SWING_1_NORTH		= 0x0004,
	SWING_2_NORTH		= 0x0005,
	SWING_3_NORTH		= 0x0006,
	SWING_2H_1_NORTH	= 0x0007,
	SWING_2H_2_NORTH	= 0x0008,
	SWING_2H_3_NORTH	= 0x0009,
	SIT_NORTH			= 0x000A,
	LEAN_NORTH			= 0x000B,
	KNEEL_NORTH			= 0x000C,
	LIE_NORTH			= 0x000D,
	CAST_1_NORTH		= 0x000E,
	CAST_2_NORTH		= 0x000F,
	STAND_SOUTH			= 0x0010,
	WALK_1_SOUTH		= 0x0011,
	WALK_2_SOUTH		= 0x0012,
	USE_SOUTH			= 0x0013,
	SWING_1_SOUTH		= 0x0014,
	SWING_2_SOUTH		= 0x0015,
	SWING_3_SOUTH		= 0x0016,
	SWING_2H_1_SOUTH	= 0x0017,
	SWING_2H_2_SOUTH	= 0x0018,
	SWING_2H_3_SOUTH	= 0x0019,
	SIT_SOUTH			= 0x001A,
	LEAN_SOUTH			= 0x001B,
	KNEEL_SOUTH			= 0x001C,
	LIE_SOUTH			= 0x001D,
	CAST_1_SOUTH		= 0x001E,
	CAST_2_SOUTH		= 0x001F,
	STAND_WEST			= 0x0020,
	WALK_1_WEST			= 0x0021,
	WALK_2_WEST			= 0x0022,
	USE_WEST			= 0x0023,
	SWING_1_WEST		= 0x0024,
	SWING_2_WEST		= 0x0025,
	SWING_3_WEST		= 0x0026,
	SWING_2H_1_WEST		= 0x0027,
	SWING_2H_2_WEST		= 0x0028,
	SWING_2H_3_WEST		= 0x0029,
	SIT_WEST			= 0x002A,
	LEAN_WEST			= 0x002B,
	KNEEL_WEST			= 0x002C,
	LIE_WEST			= 0x002D,
	CAST_1_WEST			= 0x002E,
	CAST_2_WEST			= 0x002F,
	STAND_EAST			= 0x0030,
	WALK_1_EAST			= 0x0031,
	WALK_2_EAST			= 0x0032,
	USE_EAST			= 0x0033,
	SWING_1_EAST		= 0x0034,
	SWING_2_EAST		= 0x0035,
	SWING_3_EAST		= 0x0036,
	SWING_2H_1_EAST		= 0x0037,
	SWING_2H_2_EAST		= 0x0038,
	SWING_2H_3_EAST		= 0x0039,
	SIT_EAST			= 0x003A,
	LEAN_EAST			= 0x003B,
	KNEEL_EAST			= 0x003C,
	LIE_EAST			= 0x003D,
	CAST_1_EAST			= 0x003E,
	CAST_2_EAST			= 0x003F
};

enum damage_types {
	NORMAL_DAMAGE		= 0,
	FIRE_DAMAGE			= 1,
	MAGIC_DAMAGE		= 2,
	LIGHTNING_DAMAGE	= 3,
	POISON_DAMAGE		= 3,
	STARVATION_DAMAGE	= 3,
	FREEZING_DAMAGE		= 3,
	ETHEREAL_DAMAGE		= 4,
	SONIC_DAMAGE		= 5
};
