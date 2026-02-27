/*
 *
 *	Copyright 2023 The Exult Team
 *
 *	This program is free software: you can redistribute it and/or modify it under the terms
 *	of the GNU General Public License as published by the Free Software Foundation,
 *	either version 2 of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *	without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along with this program.
 *	If not, see <http://www.gnu.org/licenses/>.
 */

/*	Effects from sprites.vga and renamed from ANIMATION to SPRITE,
 *	per the naming convention used in the rest of SIfixes and this mod.
 *	Sprites 0-26 are the same as in BG.
 *
 *	2017-01-12 Split out from si_shapes.uc by Knight Captain
 *	2017-01-20 Put in linear order since there's little benefit to groups.
 *	2023-03-15 Added full list to support adding a face to SmithzHorse.
 */

enum sprites {
	// Used for several effects:
	SPRITE_0 = 0,
	SPRITE_BIG_EXPLOSION = 1,
	SPRITE_OVERHEAD_CLOUDS = 2,
	SPRITE_TRAP_CLOUDS3 = 3,
	SPRITE_MEDIUM_EXPLOSION = 4,
	SPRITE_SMALL_EXPLOSION = 5,
	SPRITE_TRAP_CLOUDS6 = 6,
	// With blue circles:
	SPRITE_TELEPORT_HERE = 7,
	SPRITE_DEATH_VORTEX = 8,
	// The poof of "it didn't work"
	SPRITE_FAIL_GRAY = 9,
	// No sprite 10.
	// Where is this used?
	SPRITE_RED_SPLASH = 11,
	SPRITE_FIREWORKS = 12,
	SPRITE_GREEN_CIRCLES = 13,
	// No sprite 14.
	// No sprite 15.
	// Guessing here, cannot recall seeing it used:
	SPRITE_CATNIP = 16,
	SPRITE_WHITE_LIGHTNING = 17,
	SPRITE_BLUE_BURST = 18,
	SPRITE_BURST_ARROW = 19,
	// No sprite 20, was only in Beta.
	SPRITE_PURPLE_CIRCLES = 21,
	SPRITE_MAP_SERPENT_ISLE = 22,
	SPRITE_SWORDSTRIKE = 23,
	SPRITE_MUSIC_NOTES = 24,
	// No sprite 25.
	// With red circles:
	SPRITE_TELEPORT_AWAY = 26,
	// With the piece that goes southeast:
	SPRITE_Q_EXPLOSION = 27,
	// No sprite 28.
	// No sprite 29.
	SPRITE_RING_EXPLOSION = 30,
	// Used in the battle between Thoxa and Karnax, elsewhere?
	SPRITE_THOXA_EXPLOSION = 31,
	// Looks similar to a fire bolt:
	SPRITE_RED_SPIRAL = 32,
	SPRITE_SPINNING_SPARKS = 33,
	// Unused?
	SPRITE_POISON = 34,
	// 35 is in the Beta but messed up in the release.
	// Incomplete parts of other sprites:
	SPRITE_ORPHAN = 35,
	SPRITE_BLUE_MUMMY = 36,
	// No sprite 37.
	// No sprite 38.
	SPRITE_SERPENT_HEAD = 39,
	SPRITE_BLUE_LIGHTNING = 40,
	SPRITE_RED_LIGHTNING = 41,
	SPRITE_YELLOW_LIGHTNING = 42,
	SPRITE_GREEN_LIGHTNING = 43,
	// No sprite 44.
	// No sprite 45.
	SPRITE_TURTLE_BUBBLES = 46,
	SPRITE_BLINK = 47,
	SPRITE_SERPENT_PEDESTAL = 48,
	// No sprite 49.
	SPRITE_FRIGIDAZZI_STRIPS = 50,
	SPRITE_GOLD_CIRCLES = 51,
	SPRITE_MAP_ABANDONED_OUTPOST = 52,
	SPRITE_VISION_OF_PETRA = 53,
	// Used when first using the Amulet of Balance:
	SPRITE_TELEPORT_CONCENTRIC = 54,
	// Used on Sunrise Isle?
	SPRITE_GREEN_YELLOW_RED = 55,
	// Can rename this!
	SPRITE_YELLOW_SIX = 56,
	SPRITE_MAP_DARK_PATH = 57,
	SPRITE_MAP_SILVERPATES_TREASURE = 58,
	SPRITE_MAP_HAWKS_TREASURE = 59,
	SPRITE_MAP_SHAMINOS_CASTLE = 60,
	SPRITE_VASCULIOS_COFFIN = 61
};
