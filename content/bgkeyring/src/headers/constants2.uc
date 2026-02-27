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
 *	This header file contains several more constants used throughout usecode.
 *	Specifically, it has constants for several sprite animations and sounds, as
 *	well as some faces.
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

//Sprite effects
enum sprite_effects {
	ANIMATION_BIG_BLAST			= 1,
	ANIMATION_BIG_CLOUDS		= 2,
	ANIMATION_CLOUDS			= 3,
	ANIMATION_MEDIUM_BLAST		= 4,
	ANIMATION_SMALL_BLAST		= 5,
	ANIMATION_TELEPORT			= 7,
	ANIMATION_DEATH_VORTEX		= 8,
	ANIMATION_POOF				= 9,
	ANIMATION_RED_DUST			= 11,
	ANIMATION_FIREWORKS			= 12,
	ANIMATION_GREEN_BUBBLES		= 13,
	ANIMATION_OOZING_BLOB		= 14,
	ANIMATION_CIRCLE_BARRIER	= 15,
	ANIMATION_SPARKLES			= 16,
	ANIMATION_LIGHTNING			= 17,
	ANIMATION_BLUE_BEADS		= 18,
	ANIMATION_BURST_ARROW		= 19,
	ANIMATION_PURPLE_BUBBLES	= 21,
	ANIMATION_SWORD_STRIKE		= 23,
	ANIMATION_MUSIC				= 24
};

//SFX
enum sound_effects {
	SOUND_BLAST					= 8,	// Fireball spell sound effect.
	SOUND_BIG_BLAST				= 9,	// Explosion spell sound effect.
	SOUND_MOONGATE				= 11,	// Moongate raising sound effect.
	SOUND_BOOK					= 14,	// Played when the book gump displays.
	SOUND_KEY					= 27,	// Sound made by keys when you use them.
	SOUND_GLASS_SHATTER			= 37,	// Glass breaking sound effect.
	SOUND_HIT					= 62,	// Something getting hit in combat.
	SOUND_TELEPORT				= 72	// Teleportation sound effect.
};

//Some faces:
enum faces {
	DRACOTHRAXUS_FACE			= -293,
	BLACK_SWORD_FACE				= -292,
	ARCADION_GEM_FACE			= -291,
	ARCADION_MIRROR_FACE			= -290,
	ERETHIAN_FACE				= -286
};
