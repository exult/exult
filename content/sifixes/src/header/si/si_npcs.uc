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
 */

// When you are sent to Freedom, the NPC ID of Iolo, Shamino, Dupre and Boydon
// gets set to this value so the game knows they were in your party:
const int BOOTED_FOR_FREEDOM = 0x001E;
const int CURED_OF_INSANITY = 0x001D;

// Party members (you know 'em, you love 'em)
enum party_members {
	// Used by several intrinsics (e.g. UI_count_objects) that would otherwise
	// take a single NPC
	// Not supported by several other intrinsics that you'd really like it to
	// (e.g. UI_get_cont_items)
	PARTY = -357,
	// Permanent NPCs:
	// Britannians:
	AVATAR = -356,
	DUPRE = -1,
	SHAMINO = -2,
	IOLO = -3,
	GWENNO = -149, // Joins after Dupre becomes a pile of ashes

	// Serpent Isle:
	BOYDON = -34, // Created at Mad Mage Isle, remains until he dies

	// Temporary NPCs:
	// Both join in Temple of Tolerance, both die soon after:
	MORTEGRO = -26,
	SETHYS = -152,

	PETRA = -28, // Joins for Temple of Discipline, remains until kicked out

	SELINA = -44, // Joins for the Mint subquest, leaves after

	WILFRED = -45, // Joins after banes, bolts if hurt too badly

	// Joins in freedom (leaves after) and again after banes until you kill
	// the Death Knight
	STEFANO = -168

};

// Moonshade residents
enum moonshade_npcs {
	ANDRIO = -4, // Mageling
	SERV_01 = -5, // Melino and Columna's automaton
	SERV_02 = -6, // Gustacio's
	SERV_03 = -7, // Torrissio's
	SERV_04 = -8, // Ducio's baker
	SERV_05 = -9, // Ducio's blacksmith
	SERV_06 = -10, // MageLord's
	SERV_07 = -11, // Rotoluncia's
	SERV_08 = -12, // Flindo's
	BUCIA = -13, // Shopkeeper
	COLUMNA = -14,
	DUCIO = -15,
	EDRIN = -16,
	FEDABIBLIO = -17,
	FILBERCIO = -18,
	FRELI = -19, // Child
	FRIGIDAZZI = -20,
	GOBLIN_MESSENGER = -21, // Frigidazzi's servant
	GUSTACIO = -22,
	JULIA = -23, // Commanders of the Rangers
	MELINO = -24,
	AUTO_MESSENGER = -25,
	// MORTEGRO = -26, // Defined above
	MOSH = -27, // Rat woman
	// PETRA = -28, // Defined above
	POTHOS = -29,
	ROCCO = -30, // Innkeeper
	ROTOLUNCIA = -31,
	TOPO = -32,
	TORRISSIO = -33,
	SERV09 = -191, // Stefano's
	SERV10 = -192, // Mortegro's
	SERV11 = -193, // Seminarium's
	SERV12 = -194,  // Pothos' downstairs automaton
	MUSIC01 = -198, // Blue Boar musician
	MUSIC02 = -199, // Blue Boar musician
	MUSIC03 = -200, // Blue Boar musician
	SERV_SEW = -206, // Ducio's seamster
	AUTOGUARD = -216 // Pothos' patrolling automaton
};

// Mad Mage Isle residents
enum madmage_npcs {
	// BOYDON = -34, // Defined above
	ERSTAM = -35,
	VASEL = -36
};

// Sleeping Bull residents
enum sleepingbull_npcs {
	ALE = -37,
	ARGUS = -38,
	BYRIN = -39,
	HAWK = -40,
	DEVRA = -41,
	FLINDO = -42, // Visiting from Moonshade
	KANE = -43,
	// SELINA = -44, // Defined as one of Batlin's goons, below
	// WILFRED = -45, // Defined above
	ENSORCIO = -81 // Exile from Moonshade
};

// Fawn residents, including the Fellowship members
enum fawn_npcs {
	ALYSSAND = -46, // Fawn shopkeeper / seamstress
	DELIN = -47, // Fawn shopkeeper
	DELPHYNIA = -48, // Fawn horticulturalist
	GARTH = -49, // Fawn great captain
	JOTH = -50, // Fawn great captain
	VOLDIN = -51, // Fawn great captain
	JENDON = -52, // Fawn innkeeper
	JORVIN = -53, // Fawn security
	KYLISTA = -54, // Fawn priestess
	YELINDA = -55, // Fawn leader
	LEON = -56, // Fellowship leader
	KALEN = -57, // Fellowship pirate
	OLON = -58, // Fawn sailor
	RUGGS = -59, // Fellowship
	SCOTS = -60, // Fellowship mapmaker
	ZULITH = -61, // Fawn chancellor
	BUSTER = -260 // Fawn dog
};

// Monitor residents
enum monitor_npcs {
	ANDRAL = -62, // Sculptor, Leopard
	CALADIN = -63, // Leader of the Bears
	CANTRA = -64, // Child, not yet a knight
	CELLIA = -65, // Furrier, Wolf
	G_SIMON = -66, // Simon the Goblin
	HARNNA = -67, // Farmer and Healer, Leopard
	KRAYG = -68, // Provisioner, Wolf
	MARSTEN = -69, // Leader of the Leopards
	LUCILLA = -70, // Waitress, Wolf
	LUTHER = -71, // Braggart, Bear
	LYDIA = -72, // Tattoos, Bear
	RENFRY = -73, // Crematorium, Wolf
	SHAZZANA = -74, // Trainer, Leopard
	SHMED = -75, // Knight's Test, Wolf
	SIMON = -76, // Simon in disguise, Bear
	SPEKTOR = -77, // Treasurer, Leopard
	STANDARR = -78, // Blacksmith, Leopard
	TEMPLAR = -79, // Goblin expert, Bear
	FLICKEN = -80, // West gate guard, Bear
	BRENDANN = -150, // Leader of the Wolves
	SMUDGE = -261 // Cat
};

// Automatons in the temple of discipline
enum temple_of_discipline {
	LORD_OF_DISCIPLINE = -82, // Failing automaton
	ACID_AUTOMATON = -83, // Another failing automaton
	GUARD1 = -84,
	GUARD3 = -86,
	GUARD5 = -88,
	GUARD6 = -89,
	GUARD7 = -90, // The dead one (can be brought back)
	GUARD8 = -91
};

// Furnace residents
enum furnace {
	AUTO1 = -85,
	AUTO2 = -87,
	AUTO3 = -92,
	AUTO4 = -93,
	ZHELKAS = -170,
	THIEF1 = -176,
	THIEF2 = -177,
	THIEF3 = -183,
	KEY_GUY = -259 // Troll, has key to Serpent Staff
};

// Automatons from temple of logic
enum temple_of_logic {
	MURDER1 = -94,
	MURDER2 = -95,
	MURDER3 = -96,
	MURDER4 = -97,
	MURDER5 = -98,
	MURDER6 = -99,
	MURDER7 = -184
};

// Residents from temple of ethicality
enum temple_of_ethicality {
	// The automaton which administers the test of ethicality
	ETH_AUTOMATON = -100,
	MAN_IN_FIRE = -151
};


// Goblin camp residents
enum goblin_camp {
	GOBL01 = -101,
	GOBL02 = -102,
	GOBL03 = -103,
	GOBL04 = -104,
	GOBL05 = -105,
	GOBL06 = -106,
	GOBL07 = -107,
	GOBL08 = -108,
	GOBL09 = -109,
	GOBL10 = -110,
	GOBL11 = -111,
	GOBL12 = -112,
	GOBL13 = -113,
	GOBL14 = -114,
	GOBL15 = -115,
	GOBL16 = -116,
	GOBL17 = -117,
	GOBL18 = -118,
	GOBLIN_KING = -265,
	CHUCK = -274,
	JOHNSON = -285 //Captive pikeman
};

// Fawn Tower residents, the five goblins you must kill to free the tower
enum fawn_tower {
	GOBL19 = -119,
	GOBL21 = -120,
	GOBL22 = -121,
	GOBL23 = -122,
	GOBL24 = -123
};

// Dream world denizens
enum dream_world {
	D_STEFANO = -124,
	RABINDRINATH = -181,
	SIRANUSH = -182,
	D_CANTRA = -201,
	D_BYRIN = -218,
	D_ENSORCIO = -219,
	D_FILBERTIO = -220,
	SMITHZHORSE = -277,
	LORD_BRITISH = -280
};

// Spinebreaker residents
enum spinebreaker {
	BUTLER = -125, // The hierophant's butler
	GUARD_11 = -126, // Greets you with Batlin's voice
	// BRUNT = -127, // Defined below
	// DEADEYE = -128, // Defined below
	PASSWORD = -129, // Asks for the password at entrance
	METAL_MAN = -202 // Asks about The Structure of Order
};

// Skullcrusher residents
enum skullcrusher {
	GWANI_01 = -140,
	GWANI_02 = -141,
	GWANI_03 = -142,
	GUARD18 = -186,
	GUARD19 = -187,
	GUARD20 = -188,
	GUARD21 = -189,
	GUARD17 = -190, // Western gate guard
	VASCULIO = -294
};

// Gwani Village residents
enum gwani_npcs {
	BAIYANDA = -143,
	BWUNDIAI = -144,
	MWAERNO = -145,
	MYAURI = -146,
	NEYOBI = -147, // Gwani child
	YENANI = -148,
	GILWOYAI = -153,
	KAPYUNDI = -154
};

// Residents from temple of tolerance
enum temple_of_tolerance {
	// SETHYS = -152, // Defined above
	YEARL = -267, // Snow leopard
	BRENDA = -268, // Naga
	MEELOSE = -273, // Wildman
	EMMIT = -279 // Ratman
};

// Great Northern Forest residents
enum northernforest_npcs {
	BERYL = -155,
	DRAYGAN = -156,
	HURD = -157,
	IVOR = -158,
	MORGHRIM = -159,
	HOUND_OF_DOSKAR = -269,
	HAZARD = -275,
	SONAR = -276 // Timberwolf known as Windrunner
};

// Freedom residents
enum mountains_of_freedom {
	JAIL_01 = -160,
	JAIL_02 = -161,
	JAIL_03 = -162,
	LORTHONDO = -163,
	PRISON1 = -164,
	PRISON2 = -165,
	PRISON3 = -166,
	PRISON4 = -167,
	TELDRONO = -169,
	PIRATE_GUY = -196,
	SABRINA = -217,
	GARG_PRISONER = -266,
	WATSON = -282,
	DEATHMARE = -283
};

// Test of Purity residents
enum test_of_purity {
	SEX_01 = -171,
	SEX_02 = -172,
	SEX_03 = -173,
	SEX_04 = -174,
	SEX_05 = -175,
	CLONE_IOLO = -178, // Also appears in Dream World
	CLONE_SHAMINO = -179,
	CLONE_DUPRE = -180
};

// Shamino's castle
enum shamino_castle {
	HENCH1 = -205,
	HENCH2 = -203, // Starts at House of the Dead
	HENCH3 = -204 // Starts at House of the Dead
};

// Monk Isle residents
enum monkisle_npcs {
	KARNAX = -207,
	SILENT2 = -208,
	MIGGIM = -209,
	SILENT3 = -210,
	THOXA = -211,
	BRACCUS = -212,
	DRAXTA = -213,
	SILENT1 = -214,
	XENKA = -215
};

// Residents of Castle of the White Dragon
enum white_dragon_castle {
	// The quotes are from http://www.it-he.org
	ANTI_DUPRE = -263, // Haha! I am Dupre, bane of drunkenness!
	ANTI_IOLO = -262,  // I am Iolo! Bane of shooting-the-Avatar-through-the-
	                   // heart-with-the-triple-crossbow!
	ANTI_SHAM = -264   // Kneel before Shamino, bane of rapidly diminishing
	                   // consequence...
};

// Batlin's goons
enum batlingoons_npcs {
	// Batlin is actually a monster in this game, not a real person... err, NPC.
	// SELINA = -44, // Defined above
	BRUNT = -127, // Fighter
	DEADEYE = -128, // Pirate
	PALOS = -272 // Gargoyle
};

// All other NPCs
enum misc_npcs {
	GUARD12 = -130, // Automaton, starts at House of the Dead
	GUARD13 = -131, // Automaton, starts at House of the Dead
	GUARD14 = -132, // Automaton, starts at House of the Dead
	GUARD15 = -133, // Automaton, starts at House of the Dead
	GUARD16 = -134, // Automaton, starts at House of the Dead
	ZOMBIE01 = -135, // Gwani, starts at House of the Dead
	ZOMBIE02 = -136, // Gwani, starts at House of the Dead
	ZOMBIE03 = -137, // Gwani, starts at House of the Dead
	ZOMBIE04 = -138, // Gwani, starts at House of the Dead
	ZOMBIE05 = -139, // Gwani, starts at House of the Dead
	DED_AUTOMATON = -185, // Automaton, in the middle of ocean
	PERRY_STOKES = -195, // The software pirate
	IAUTO = -197, // Automaton, starts at House of the Dead
	DEATH_KNIGHT = -222, // Starts at the House of the Dead
	LOAF1 = -221, // Starts at the House of the Dead
	LOAF2 = -223, // Starts at the House of the Dead
	LOAF3 = -224, // Starts at the House of the Dead
	LOAF4 = -225, // Starts at the House of the Dead
	LOAF5 = -226, // Starts at the House of the Dead
	LOAF6 = -227, // Starts at the House of the Dead
	AUTO_2 = -228, // Automaton, starts at the House of the Dead
	AUTO_3 = -229, // Automaton, starts at the House of the Dead
	AUTO_4 = -230, // Automaton, starts at the House of the Dead
	LARRY = -231, // Automaton, starts at the House of the Dead
	STAN = -232, // Automaton, starts at the House of the Dead
	SUPER_GOBLIN = -256, // At the tree leading to Goblin Camp
	BILLY_CAIN = -257, // Goblin, SW of knight's test
	STEVE_POWERS = -258, // Goblin, SE of Sleeping Bull Inn, along north coast
	HENCH_MAN = -270, // Starts at the House of the Dead
	HENCHMAN = -271, // Starts at the House of the Dead
	ELSI = -278, // Cow, north of Mint
	CRUSTY = -281, // Clown, SW of Captain Stokes tower
	PIBB = -284, // Starts at the House of the Dead
	CINDY = -287, // Naked girl S of turtle-summoning bell
	DBEYER = -293, // Gargoyle, starts at the House of the Dead
	FITCH = -295, // Trapper, the one dying at the edge of the Ice Plains
	AUTOMATON_FACE = -298,
};
