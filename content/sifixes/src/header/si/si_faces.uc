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

/*	This lists the names of the NPCs for each face in faces.vga.
 *	Faces under 255 match the NPC number.
 *	Faces above 255 do not match the NPC number in the non-modded game.
 *
 *	2012-12-13 Knight Captain
 *
 *  2023-03-15 Added to SIfixes, includes Rhona's artwork for SmithzHorse.
 */

enum face_names {
	FACE_AVATAR = 0,					// 6 frames
	FACE_DUPRE = 1,
	FACE_SHAMINO = 2,
	FACE_IOLO = 3,
	FACE_ANDRIO = 4,
	// 5-12 unused
	FACE_BUCIA = 13,
	FACE_COLUMNA = 14,
	FACE_DUCIO = 15,
	FACE_EDRIN = 16,
	FACE_FEDABIBLIO = 17,
	FACE_FILBERCIO = 18,				// 2 frames, same image
	FACE_FRELI = 19,
	FACE_FRIGIDAZZI = 20,				// 3 frames, 2 same with clothes, 3rd naked
	// 21 unused
	FACE_GUSTACIO = 22,
	FACE_JULIA = 23,
	FACE_MELINO = 24,
	// 25 unused
	FACE_MORTEGRO = 26,
	FACE_MOSH = 27,
	FACE_PETRA = 28,
	FACE_POTHOS = 29,
	FACE_ROCCO = 30,
	FACE_ROTOLUNCIA = 31,				// 2 frames, same image
	FACE_TOPO = 32,
	FACE_TORRISSIO = 33,
	FACE_BOYDON = 34,
	FACE_ERSTAM = 35,
	FACE_VASEL = 36,
	// 37 unused
	FACE_ARGUS = 38,
	FACE_BYRIN = 39,
	FACE_HAWK = 40,
	FACE_DEVRA = 41,
	FACE_FLINDO = 42,
	FACE_KANE = 43,
	FACE_SELINA = 44,
	FACE_WILFRED = 45,
	FACE_ALYSSAND = 46,
	FACE_DELIN = 47,
	FACE_DELPHYNIA = 48,
	FACE_GARTH = 49,
	FACE_JOTH = 50,
	FACE_VOLDIN = 51,
	FACE_JENDON = 52,
	FACE_JORVIN = 53,
	FACE_KYLISTA = 54,
	FACE_YELINDA = 55,
	FACE_LEON = 56,
	FACE_KALEN = 57,
	FACE_OLON = 58,
	FACE_RUGGS = 59,
	FACE_SCOTS = 60,
	FACE_ZULITH = 61,
	FACE_ANDRAL = 62,
	FACE_CALADIN = 63,
	FACE_CANTRA = 64,
	FACE_CELLIA = 65,
	FACE_G_SIMON = 66,
	FACE_HARNNA = 67,
	FACE_KRAYG = 68,
	FACE_MARSTEN = 69,
	FACE_LUCILLA = 70,
	FACE_LUTHER = 71,
	FACE_LYDIA = 72,
	FACE_RENFRY = 73,
	FACE_SHAZZANA = 74,
	FACE_SHMED = 75,
	FACE_SIMON = 76,
	FACE_SPEKTOR = 77,
	FACE_STANDARR = 78,
	FACE_TEMPLAR = 79,
	FACE_FLICKEN = 80,
	FACE_ENSORCIO = 81,
	// 82-93 unused
	FACE_MURDER1 = 94,
	FACE_MURDER2 = 95,
	FACE_MURDER3 = 96,
	FACE_MURDER4 = 97,
	FACE_MURDER5 = 98,
	FACE_MURDER6 = 99,
	// 100-126 unused
	FACE_BRUNT = 127,
	FACE_DEADEYE = 128,
	// 129-142 unused
	FACE_BAIYANDA = 143,
	FACE_BWUNDIAI = 144,
	FACE_MWAERNO = 145,
	FACE_MYAURI = 146,
	FACE_NEYOBI = 147,
	FACE_YENANI = 148,
	FACE_GWENNO = 149,
	FACE_BRENDANN = 150,
	// 151 unused
	FACE_SETHYS = 152,
	FACE_GILWOYAI = 153,
	FACE_KAPYUNDI = 154,
	FACE_BERYL = 155,
	FACE_DRAYGAN = 156,
	// 157-158 unused
	FACE_MORGHRIM = 159,
	// 160-167 unused
	FACE_STEFANO = 168,
	// 169 unused
	FACE_ZHELKAS = 170,
	// 171-177 unused
	FACE_CLONE_IOLO = 178,
	FACE_CLONE_SHAMINO = 179,
	FACE_CLONE_DUPRE = 180,
	FACE_RABINDRINATH = 181,
	FACE_SIRANUSH = 182,
	// 183 unused
	FACE_MURDER7 = 184,
	// 185-206 unused
	FACE_KARNAX = 207,
	// 208 unused
	FACE_MIGGIM = 209,
	//210 unused
	FACE_THOXA = 211,
	FACE_BRACCUS = 212,
	FACE_DRAXTA = 213,
	// 214 unused
	FACE_XENKA = 215,
	// 216-255 unused
	// Most face numbers used below do not match with NPC numbers.
	FACE_ARCADION = 256,
	FACE_BLACK_SWORD = 257,				// 4 frames, only first used in game
	FACE_SHRIASH = 258,
	FACE_WISP1 = 259,					// Unused in game
	FACE_WISP2 = 260,					// Unused in game
	FACE_ORACLE = 261,
	FACE_CATMAN = 262,
	// 263 unused
	FACE_VASCULIO = 264,
	// 265 unused
	FACE_YELINDA_SKINLESS = 266,
	FACE_SSITHNOS = 267,
	FACE_ANTI_SHAMINO1 = 268,
	FACE_FAWN_GUARD = 269,
	FACE_GOBLIN_KING = 270,
	FACE_PIKEMAN_BEAR = 271,
	FACE_SKYRISE = 272,
	FACE_PALOS = 273,
	FACE_RANGER = 274,
	FACE_PIKEMAN_WOLF = 275,
	FACE_LORD_BRITISH = 276,
	FACE_GHOST = 277,
	FACE_PIKEMAN_LEOPARD = 278,
	FACE_HOUND_OF_DOSKAR = 279,
	FACE_HAZARD = 280,
	FACE_ERNESTO = 281,
	FACE_ANTI_DUPRE1 = 282,
	// 283 unused
	FACE_CHAOS_HIEROPHANT = 284,
	// 285 unused
	FACE_BEATRIX = 286,
	FACE_BATLIN = 287,
	FACE_ANTI_IOLO1 = 288,
	FACE_FITCH = 289,
	FACE_ANTI_SHAMINO = 290,
	FACE_ANTI_DUPRE = 291,
	FACE_ANTI_IOLO = 292,
	FACE_CHAOS_SERPENT = 293,
	FACE_ORDER_SERPENT = 294,
	FACE_GREAT_EARTH_SERPENT = 295,
	FACE_GUARDIAN = 296,
	FACE_WILDMAN = 297,					// Unused in the original games
	FACE_AUTOMATON = 298,
	FACE_AVATAR_TATTOO = 299,			// 6 frames
	FACE_GES_TRANSLUCENT = 300,
	FACE_BOYDON_HEAD = 301,
	FACE_ROTOLUNCIA_SCROLL = 302,
	// 303-304 unused
	FACE_SHAL = 305,
	FACE_ISSTANAR = 306,
	FACE_ELISSA = 307,
	FACE_SUROK = 308,
	FACE_TSANDAR = 309,
	FACE_DRAXINAR = 310,
	FACE_YUREL = 311,
	FACE_RIEYA = 312,
	FACE_SOLARIA = 313,
	FACE_DRUSILLA = 314,
	FACE_SMITHZHORSE = 317
};

enum face_frames {
	FRAME_COLUMNA_YOUNG = 0,
	FRAME_COLUMNA_OLD = 1,
	FRAME_FRIGIDAZZI_NAKED = 2,
	FRAME_GHOST_BALD = 0,
	FRAME_GHOST_HAIR = 1,
	FRAME_GHOST_SMIRK = 2,
	FRAME_BATLIN_WITH_MEDALLION = 0,
	FRAME_BATLIN_WITHOUT_MEDALLION = 1
};
