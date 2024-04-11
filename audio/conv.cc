/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#include "conv.h"

// sfx with ??? are converted to sfx #135 so you can tell
// it's wrong. Some I suspect to be something so it's not set 135
const int bgconv[] = {
		12,     // Bow Twang		0
		80,     // Missile ??		1
		9,      // Blade			2
		11,     // Blunt			3
		125,    // Hit				4
		61,     // Graze			5
		92,     // Rotating			6
		40,     // Explos #1		7
		41,     // Explos #2		8
		42,     // Explos #3		9
		127,    // Whip pta			10
		71,     // Thunder			11
		44,     // Fireball			12
		65,     // Torches			13
		94,     // Gumps!!!!!		14
		56,     // Gavel			15
		121,    // Treadle			16
		117,    // Clock tick		17
		118,    // Clock tock		18
		16,     // Chime			19
		45,     // Fire 1			20
		46,     // Fire 2			21
		47,     // Fire 3			22
		28,     // Bell Ding		23
		30,     // Bell Dong		24
		72,     // Log Saw			25
		78,     // Mill Stone		26
		68,     // Key				27
		70,     // Lever			28
		135,    // Roulette			29
		32,     // Creeeeaack		30
		31,     // Creeeeaack		31
		89,     // Portcullis		32
		88,     // Portcullis close	33
		35,     // Drawbridge		34
		34,     // Drawbridge		35
		135,    // Fuse  ???		36
		95,     // Shadoobie		37
		99,     // Splash			38
		126,    // W. Anchor		39
		37,     // D. Anchor		40
		18,     // Creeeeaack		41
		17,     // Creeeeaack		42
		2,      // gumpster			43
		1,      // gumpster			44
		49,     // Forge			45
		33,     // Douse			46
		7,      // Bellows			47
		50,     // Fountain			48
		109,    // Surf's up		49
		107,    // Stream			50
		133,    // Waterfall		51
		129,    // Wind    ???		52
		135,    // Rainman  ???		53
		114,    // Swamp 1			54
		110,    // Swamp 2			55
		111,    // Swamp 3			56
		112,    // Swamp 4			57
		113,    // Swamp 5			58
		132,    // Waterwheel		59
		39,     // Eruption ???		60
		22,     // Crickets			61
		116,    // Thunder			62
		128,    // Whirlpool		63
		64,     // Heal				64
		20,     // Spell			65
		67,     // Spell			66
		130,    // Wizard   ???		67
		57,     // General			68
		48,     // Fizzle			69
		84,     // New Spell		70
		82,     // MPdrain			71
		83,     // MPgain			72
		134,    // Footstep L		73
		134,    // Footstep R		74
		108,    // Success			75
		43,     // Failure			76
		55,     // Moongate			77
		54,     // Moongate B		78
		26,     // Entity Hum		79
		101,    // Entity Hum		80
		115,    // Entity Hum		81
		96,     // Shreik			82
		135,    // Slap      ???	83
		135,    // Oooffff   ???	84
		15,     // Bird    ???		85
		10,     // Blocked !!		86
		52,     // Furl				87
		124,    // Unfurl			88
		135,    // MISSING			89
		36,     // Drink ???		90
		38,     // Eat   ???		91
		135,    // Whip ptb			92
		135,    // Doorslam			93
		135,    // Portcullis		94
		135,    // Drawbridge		95
		135,    // Closed			96
		100,    // SpinnWheel		97
		79,     // Minning   ???	98
		59,     // Minning   ???	99
		93,     // Shutters			100
		135,    // 1armbandit ???	101
		73,     // Loom				102
		103,    // Stalags			103
		75,     // MagicWeap		104
		86,     // Poison			105
		65,     // Ignite			106
		62,     // Yo yo LA ???		107
		131,    // Wind Spell		108
		90,     // Protect			109
		91,     // PoisonSpel ???	110
		66,     // IgniteSpel		111
		21,     // CradleRock		112
		5,      // Beeezzzzz		113
		74,     // Machines			114
		255,    // Static - not used in SI	115
		136     // Tick Tock		116
};

// A -1 means no equivalent.
const int bgconvsong[] = {
		-1,    // 0
		-1,    // 1
		-1,    // 2
		70,    // 3
		68,    // 4
		69,    // 5
		67,    // 6
		66,    // 7
		-1,    // 8
		0,     // 9
		1,     // 10
		2,     // 11
		3,     // 12
		4,     // 13
		5,     // 14
		6,     // 15
		7,     // 16
		8,     // 17
		9,     // 18
		10,    // 19
		11,    // 20
		12,    // 21
		13,    // 22
		14,    // 23
		15,    // 24
		16,    // 25
		17,    // 26
		18,    // 27
		19,    // 28
		20,    // 29
		21,    // 30
		22,    // 31
		23,    // 32
		24,    // 33
		25,    // 34
		26,    // 35
		27,    // 36
		28,    // 37
		29,    // 38
		-1,    // 39
		30,    // 40
		31,    // 41
		32,    // 42
		33,    // 43
		34,    // 44
		35,    // 45
		36,    // 46
		37,    // 47
		38,    // 48
		39,    // 49
		40,    // 50
		41,    // 51
		42,    // 52
		43,    // 53
		-1,    // 54
		44,    // 55
		45,    // 56
		46,    // 57
		47,    // 58
		48,    // 59
		-1,    // 60
		-1,    // 61
		-1,    // 62
		-1,    // 63
		-1,    // 64
		-1,    // 65
		-1,    // 66
		-1,    // 67
		-1,    // 68
		-1,    // 69
		-1     // 70
};
