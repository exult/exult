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

//Shapes
//------

//Shapes used by NPCs (ones that will talk to you and have schedules). Note that there's a bit of confusion between this and monster_shapes since some NPCs have the monster flag (even though they are human).
//Also note that some of these are duplicated to make 'monster' versions - you can find those in monster_shapes below.
enum npc_shapes {
	//NPCs with unique appearances
	SHAPE_MALE_AVATAR	= 721,
	SHAPE_FEMALE_AVATAR	= 989,
	SHAPE_FERRYMAN		= 155,	//Doom Scythe is built into shape, unlike FERRYMAN2
	SHAPE_FERRYMAN2		= 952,	//Actually this version is a monster...not sure which actually got used
	SHAPE_ALAGNER		= 227,
	SHAPE_PAPA			= 228,
	SHAPE_MAMA			= 229,
	SHAPE_UNICORN		= 375,
	SHAPE_KISSME		= 382,
	SHAPE_FAIRY			= 382,
	SHAPE_BATLIN		= 403,
	SHAPE_BATLIN2		= 482,	//Not sure which is the Real Batlin and which is the clone

	SHAPE_IOLO			= 465,
	SHAPE_SHAMINO		= 487,
	SHAPE_DUPRE			= 488,
	SHAPE_SPARK			= 489,
	SHAPE_JAANA			= 490,

	SHAPE_LORD_BRITISH	= 466,
	SHAPE_SMITH			= 476,	//Smith the Horse
	SHAPE_FRANK			= 477,	//Frank the Fox
	SHAPE_SHERRY		= 478,	//Sherry the Mouse
	SHAPE_DRAXINUSOM	= 480,	//Sherry the Mouse

	SHAPE_HOOK			= 506,	//Hook is a monster, yet Forskis isn't. Huh.
	SHAPE_FORSKIS		= 805,
	SHAPE_ELIZABETH		= 881,
	SHAPE_ABRAHAM		= 882,

	SHAPE_EMP				= 479,	//There's also a monster emp, SHAPE_MONSTER_EMP. Could be this one is Saralek, and the monster one is everyone else
	SHAPE_TROLL				= 861, //Not to be confused with monster trolls, this one is used for the troll jailer and troll prisoner

	SHAPE_CYCLOPS			= 380,	//Friendly Cyclops, not monster cyclops (probably Iskender)
	SHAPE_WISP				= 534,

	//Gargoyle shapes
	SHAPE_GARGOYLE_FLYING	= 226,	//Never used AFAIK
	SHAPE_GARGOYLE_WINGED	= 274,
	SHAPE_GARGOYLE_WARRIOR	= 274,	//White straps across torso
	SHAPE_GARGOYLE_NOBLE	= 473,	//Circlet on head
	SHAPE_GARGOYLE_WORKER	= 475,	//No wings
	SHAPE_GARGOYLE_WINGLESS	= 475,

	//While these have the MONSTER flag, they are only used as NPCs AFAIK
	SHAPE_GHOST_FEMALE		= 299,
	SHAPE_GHOST_MALE		= 317,

	//General human shapes
	SHAPE_SAGE_MALE			= 318,
	SHAPE_SAGE_FEMALE		= 448,
	SHAPE_MAGE_MALE			= 445,
	SHAPE_MAGE_FEMALE		= 446,

	SHAPE_SHOPKEEPER_FEMALE	= 454,
	SHAPE_SHOPKEEPER_MALE	= 455,

	SHAPE_PEASANT_MALE		= 319,
	SHAPE_PEASANT_FEMALE	= 452,
	SHAPE_PEASANT_CHILD		= 471,
	SHAPE_PEASANT_CHILD2	= 965,	//Identical in appearance to CHILD

	SHAPE_WOUNDED_MAN		= 447,
	SHAPE_BEGGAR_CRUTCHES	= 449,
	SHAPE_BEGGAR_NOLEGS		= 450,

	SHAPE_NOBLE_MALE		= 451,
	SHAPE_NOBLE_FEMALE		= 456,
	SHAPE_NOBLE_CHILD		= 472,

	SHAPE_GYPSY_MALE		= 457,
	SHAPE_PIRATE			= 458,
	SHAPE_WENCH				= 459,
	SHAPE_RANGER_MALE		= 460,
	SHAPE_RANGER_FEMALE		= 461,

	SHAPE_FIGHTER_MALE		= 462,
	SHAPE_FIGHTER_FEMALE	= 463,
	SHAPE_PALADIN			= 464,

	SHAPE_JESTER				= 467,
	SHAPE_HIGHWAYMAN			= 485,	//Only used once, AFAIK
	SHAPE_ENTERTAINER_MALE		= 468,
	SHAPE_ENTERTAINER_FEMALE	= 469,

	SHAPE_TOWNSMAN			= 265,
	SHAPE_BLACKSMITH		= 304,
	SHAPE_GUARD				= 720,	//Yes, the actual real-McCoy guard
	SHAPE_BARKEEP			= 957,	//Looks identical to SHOPKEEPER_MALE, just different name

	SHAPE_BABY				= 730,	//This is just an object, basically. Kinda creepy.
	SHAPE_TODDLER			= 864
};

//Shapes used by 'monsters' - i.e. combatants and humans without schedules
enum monster_shapes {
	//Unique monsters
	SHAPE_ETHEREAL_MONSTER	= 230,
	SHAPE_HYDRA				= 381,
	SHAPE_STONE_HARPIE		= 753,
	SHAPE_GOLEM				= 1015,

	//Human 'monsters'
	SHAPE_MONSTER_MAGE		= 154,
	SHAPE_MONSTER_PALADIN	= 247,
	SHAPE_MONSTER_FIGHTER	= 259,
	SHAPE_MONSTER_GUARD		= 394,
	SHAPE_MONSTER_GUARD2	= 806,
	SHAPE_MONSTER_GUARD3	= 946,	//Certainly laying it on with the duplicate guards.
									//(Presumably they each have different appearances and abilities.)
	SHAPE_MONSTER_PIRATE	= 401,
	SHAPE_MONSTER_EMP		= 784,	//Not sure why this is a monster?
	SHAPE_MONSTER_GARGOYLE	= 883,	//Of the Wingless variety
	SHAPE_FELLOWSHIP_MALE	= 884,	//Found on Isle of the Avatar
	SHAPE_FELLOWSHIP_FEMALE	= 929,	//Found on Isle of the Avatar

	SHAPE_GHOST				= 299,	//Your generic white-sheet jobbie

	//Both of these are monsters - one has a crown, the other is more tatty. Not sure which is Horance.
	SHAPE_LICHE				= 354,
	SHAPE_LICHE2			= 519,

	SHAPE_ACID_SLUG			= 491,
	SHAPE_ALLIGATOR			= 492,
	SHAPE_BAT				= 493,
	SHAPE_CORPSER			= 499,

	SHAPE_MONSTER_CYCLOPS	= 501,	//Not to be confused with SHAPE_CYCLOPS, which is the friendly NPC version

	SHAPE_DRAGON			= 504,
	SHAPE_DRAKE				= 505,

	SHAPE_GAZER				= 511,
	SHAPE_GREMLIN			= 513,
	SHAPE_HEADLESS			= 514,

	SHAPE_REAPER			= 524,
	SHAPE_SEA_SERPENT		= 525,

	SHAPE_SKELETON			= 528,
	SHAPE_SLIME				= 529,

	SHAPE_HARPIE			= 532,
	SHAPE_MONSTER_TROLL		= 533,	//There's also an NPC troll, SHAPE_TROLL

	SHAPE_TENTACLES			= 536,
	SHAPE_MONGBAT			= 661
};

//Shapes used by 'normal' animals (i.e., real-life ones, whether friendly or hostile)
enum animal_shapes {
	SHAPE_BEE		= 494,
	SHAPE_CAT		= 495,
	SHAPE_DOG		= 496,
	SHAPE_CHICKEN	= 498,
	SHAPE_COW		= 500,
	SHAPE_DEER		= 502,
	SHAPE_FISH		= 509,	//live fish in the water, not dead ones on your plate (those are frames of SHAPE_FOOD, see food_frames below)
	SHAPE_FOX		= 510,	//Not to be confused with Frank the Fox, who is SHAPE_FRANK
	SHAPE_FLY		= 517,
	SHAPE_MOUSE		= 521,	//Not to be confused with Sherry the Mouse, who is SHAPE_SHERRY
	SHAPE_RAT		= 523,
	SHAPE_SNAKE		= 530,
	SHAPE_WOLF		= 537,
	SHAPE_SCORPION	= 706,
	SHAPE_BIRD		= 716,
	SHAPE_HORSE		= 727,		//Not to be confused with Smith the Horse, who is SHAPE_SMITH
	SHAPE_DRAFT_HORSE	= 796,	//These hardly count as an animal, poor things
	SHAPE_RABBIT	= 811,
	SHAPE_SPIDER	= 865,
	SHAPE_SHEEP		= 970
};

const int SHAPE_KEY				= 641;
const int SHAPE_GOLD			= 644;	//Filthy lucre
const int SHAPE_SCROLL			= 797;
const int SHAPE_EGG				= 275;
const int SHAPE_FISHING_ROD		= 662;	//fishing rod (quel surprise)

const int SHAPE_RUNE			= 877;
const int SHAPE_ORB				= 785;	//Orb of the Moons

const int SHAPE_PRISM			= 981;	//The Guardian's three prisms

const int SHAPE_FOOD			= 377;
const int SHAPE_BOTTLE			= 616;
const int SHAPE_REAGENT			= 842;
const int SHAPE_KITCHEN_ITEM	= 863;
const int SHAPE_BUCKET			= 810;
const int SHAPE_POWDER_KEG		= 704;

const int SHAPE_DOUGH			= 658;
const int SHAPE_HEARTH			= 831;

const int SHAPE_WHEAT			= 677;
const int SHAPE_MILLSTONE		= 711;

const int SHAPE_WELL			= 740;
const int SHAPE_WELLBASE		= 470;
const int SHAPE_ROCK			= 331;

const int SHAPE_WORKTABLE_VERTICAL		= 1003;
const int SHAPE_WORKTABLE_HORIZONTAL	= 1018;

const int SHAPE_EMPTY_CRADLE	= 992;
const int SHAPE_FULL_CRADLE		= 987;

const int SHAPE_SPILL = 912;

enum light_sources {
	SHAPE_LIGHTSOURCE		= 336,
	SHAPE_SCONCE			= 481,
	SHAPE_TORCH				= 595,
	SHAPE_LIGHTSOURCE_LIT	= 338,
	SHAPE_SCONCE_LIT		= 435,
	SHAPE_TORCH_LIT			= 701,
	SHAPE_CAMPFIRE			= 825
	//SHAPE_FIREPIT lives in forging_shapes below
};

enum cloth_shapes {
	SHAPE_THREAD	= 654,
	SHAPE_LOOM		= 261,
	SHAPE_CLOTH		= 851,
	SHAPE_BANDAGE	= 827,
	SHAPE_TOP		= 249,
	SHAPE_PANTS		= 738,
	SHAPE_TOY		= 742,
	SHAPE_CLOAK		= 285,
	SHAPE_COSTUME	= 838,
	SHAPE_HOOD		= 444,
	SHAPE_SHEARS	= 698,
	SHAPE_WOOL		= 653
};

//Door shapes
enum door_shapes {
	SHAPE_DOOR_HORIZONTAL		= 270,
	SHAPE_DOOR_VERTICAL			= 376,
	SHAPE_DOOR2_HORIZONTAL		= 432,
	SHAPE_DOOR2_VERTICAL		= 433,

	SHAPE_ABBEY_DOOR_LEFT_HORIZONTAL	= 246,
	SHAPE_ABBEY_DOOR_RIGHT_HORIZONTAL	= 225,

	SHAPE_ABBEY_DOOR_LEFT_VERTICAL	= 250,
	SHAPE_ABBEY_DOOR_RIGHT_VERICAL	= 392	//Note: This is actually a copy of ABBEY_DOOR_LEFT_VERTICAL. Maybe they screwed up but never used them anyway.
};

//Chest shapes
enum chest_shapes {
	SHAPE_CHEST			= 800,
	SHAPE_LOCKED_CHEST	= 522
};

//Shapes used in mining
enum mining_shapes {
	SHAPE_MINING_MACHINE	= 410,
	SHAPE_CONVEYOR_BELT		= 411,
	SHAPE_BLACKROCK			= 914,
	SHAPE_LEAD_ORE			= 915,
	SHAPE_IRON_ORE			= 916,
	SHAPE_STONE_CHIPS		= 815
};

//Shapes used in weaponsmithing (e.g. Forge of Virtue)
enum forging_shapes {
	SHAPE_SWORDBLANK		= 668,
	SHAPE_ANVIL				= 991,
	SHAPE_TROUGH_VERTICAL	= 719,
	SHAPE_TROUGH_HORIZONTAL	= 741,
	SHAPE_FIREPIT			= 739
};

enum moongate_shapes {
	SHAPE_ORB_MOONGATE_HORIZONTAL	= 779,
	SHAPE_ORB_MOONGATE_VERTICAL		= 157,
	SHAPE_STANDING_RED_MOONGATE		= 776,
	SHAPE_STANDING_BLUE_MOONGATE	= 777
};

enum weapon_shapes {
	SHAPE_HAMMER		= 623,
	SHAPE_CUSTOM_SWORD	= 635,
	SHAPE_BLACK_SWORD	= 707
};

//Frames
//------

//these are frames of SHAPE_TOP
const int FRAME_SHIRT	= 0;
const int FRAME_DRESS1	= 1;
const int FRAME_DRESS2	= 2;

const int FRAME_CUBE			= 1;	//the cube prism

const int FRAME_RUNE_HONOR		= 6;

enum kitchen_item_frames {
	FRAME_FLOURSACK_OPEN	= 0,
	FRAME_PITCHER			= 2,

	FRAME_ROLLINGPIN	= 8,
	FRAME_ROLLINGPIN_2	= 9,
	FRAME_FLOURSACK		= 13,
	FRAME_FLOURSACK_2	= 14,
	FRAME_CHURN			= 15
};

enum food_frames {
	FRAME_BREAD			= 0,
	FRAME_ROLLS			= 1,
	FRAME_BAGUETTE		= 2,
	FRAME_BREAD_LONG	= 2,	//for you Francophobes (or should I call it "FRAME_BREAD_FREEDOM"?)
	FRAME_FRUITCAKE		= 3,
	FRAME_CAKE			= 4,
	FRAME_PIE			= 5,
	FRAME_PASTRY		= 6,

	FRAME_SAUSAGES		= 7,
	FRAME_MUTTON		= 8,
	FRAME_BEEF			= 9,
	FRAME_CHICKEN		= 10,
	FRAME_HAM			= 11,
	FRAME_TROUT			= 12,
	FRAME_FLOUNDER		= 13,
	FRAME_VENISON		= 14,
	FRAME_JERKY			= 15,

	FRAME_APPLE			= 16,
	FRAME_BANANA		= 17,
	FRAME_CARROTS		= 18,
	FRAME_CARROT		= 18,
	FRAME_GRAPES		= 19,
	FRAME_PUMPKIN1		= 20,
	FRAME_PUMPKIN_LARGE	= 20,
	FRAME_PUMPKIN2		= 21,
	FRAME_PUMPKIN_SMALL	= 21,
	FRAME_LEEK			= 22,

	FRAME_RIBS			= 23,
	FRAME_EGG			= 24,

	FRAME_BUTTER		= 25,
	FRAME_CHEESE1		= 26,
	FRAME_CHEESE_WHEEL	= 26,
	FRAME_CHEESE2		= 27,
	FRAME_CHEESE_WEDGE	= 27,
	FRAME_CHEESE3		= 28,
	FRAME_CHEESE_GREEN	= 28,

	FRAME_POTATO			= 29,
	FRAME_FISH_AND_CHIPS	= 30,
	FRAME_SILVERLEAF		= 31
};

enum bottle_frames {
	FRAME_MILK				= 7
};

enum dough_frames {
	//used in baking.uc
	FRAME_FLOUR			= 0,
	FRAME_DOUGH_FLAT	= 1,	//this will now create a pastry when cooked in the oven
	FRAME_DOUGH_BALL	= 2
};

enum bucket_frames {
	FRAME_BUCKET_EMPTY	= 0,
	FRAME_BUCKET_WATER	= 1,
	FRAME_BUCKET_BLOOD	= 2,
	FRAME_BUCKET_WINE	= 3,
	FRAME_BUCKET_BEER	= 4,
	FRAME_BUCKET_BEER2	= 5,
	FRAME_WATERINGCAN	= 6
};
