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

//These qualities control the appearance and readiness of the swordblank,
//and are set by temperSword() when the sword is worked upon the anvil.
//The numbers essentially mean the number of hits it takes to reach that
//state, except that there is a high chance that the quality will go up
//and a low chance it will go down with each hit.
enum swordblank_qualities {
	SWORDBLANK_READY	= 9,	//At this quality, the swordblank is hardened by
								//quenching and is ready to be completed by Menion
	SWORDBLANK_QUENCH	= 8,	//At this quality, the swordblank is ready to be
								//quenched and cannot be hammered any more
								//(If the sword is allowed to cool without quenching,
								//it will reduce to below this quality again)
	SWORDBLANK_IMPROVED	= 4		//At and above this quality, the swordblank has an
								//improved appearance
};

//These control the chance that the swordblank's quality is improved/reduced
//with every blow of the hammer.
enum swordblank_chances {
	//The base % chance a hit will mar the blade (final fail chance = BASE_TEMPER_FAIL_CHANCE - (dexterity / 2)
	BASE_TEMPER_FAIL_CHANCE = 20,
	//The base % chance a hit will improve the blade (final succeed chance = BASE_TEMPER_SUCCEED_CHANCE + (strength / 2)
	BASE_TEMPER_SUCCEED_CHANCE = 60
};

//How fast the swordblank cools down (one frame every n ticks)
const int SWORDBLANK_COOL_SPEED = 25;

//The sound of quenching a sword in a water trough
const int SOUND_QUENCH_SWORD = 46;

const int SOUND_HAMMER_SWORD = 45;

const int SOUND_BELLOWS = 47;		//Fwooosh
const int NUM_BELLOWS_PUMPS = 2;	//The number of times to pump the bellows

//The number of ticks it takes the firepit to cool down one notch
//(increased from 15 to 20)
const int FIREPIT_COOL_SPEED = 20;
//The sound made when the flames roar higher and higher and HIGHER
const int SOUND_FIREPIT = 47;

//The quality of the instruction scroll Menion gives you
const int SWORDSMITHING_INSTRUCTIONS = 13;
//How much Menion charges for training
const int MENION_TRAINING_PRICE = 45;
//The XP reward for completing a custom sword
const int SWORDSMITHING_XP = 25;
