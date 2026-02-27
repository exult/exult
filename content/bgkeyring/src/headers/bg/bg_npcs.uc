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

//Party members (you know em, you love em)
enum party_members {
	PARTY		= -357,	//Used by several intrinsics (e.g. UI_count_objects) that would otherwise take a single NPC
						//Not supported by several other intrinsics that you'd really like it to (e.g. UI_get_cont_items)
	AVATAR		= -356,
	IOLO		= -1,
	SPARK		= -2,
	SHAMINO		= -3,
	DUPRE		= -4,
	JAANA		= -5,
	SENTRI		= -7,
	JULIA		= -8,
	KATRINA		= -9,
	TSERAMED	= -10
};

//Trinsic residents
enum trinsic_npcs {
	PETRE		= -11,	//stablekeeper
	FINNIGAN	= -12,	//mayor
	GILBERTO	= -13,	//injured guard
	JOHNSON		= -14,	//guard on east gate
	KLOG		= -16,	//fellowship leader and husband of Ellen
	CHANTU		= -17,	//healer
	DELL		= -18,	//provisioner and grouch
	APOLLONIA	= -19,	//barmaid
	MARKUS		= -20,	//trainer
	GARGAN		= -21,	//shipwright
	CAROLINE	= -22,	//fellowship recruiter

	PAUL		= -233,	//Passion Play actor
	MERYL		= -234,	//Passion Play actress
	DUSTIN		= -235,	//Passion Play actor

	ELLEN		= -236	//wife of Klog
};

//Britain residents
enum britain_npcs {
	LORD_BRITISH	= -23,	//Castle British, ineffectual monarch
	NYSTUL		= -24,	//Castle British, royal sage
	CHUCKLES	= -25,	//Castle British, royal jester
	BATLIN		= -26,	//arch-villain
	RAYMUNDO	= -27,	//playwright
	JESSE		= -28,	//actor
	STUART		= -29,	//actor
	AMBER		= -30,	//actress and Shamino's main squeeze
	KRISTY		= -31,	//Castle British, creche child
	MAX			= -32,	//Castle British, creche child
	NICHOLAS	= -33,	//Castle British, creche child
	NANNA		= -34,	//Castle British, childcare professional and champion of the working class
	CSIL		= -35,	//healer and molecular biologist
	ZELLA		= -36,	//trainer
	LUCY		= -37,	//barmaid and owner of the Blue Boar
	GREG		= -38,	//provisioner
	NENO		= -39,	//musician
	JUDITH		= -40,	//musician and wife of Patterson
	CANDICE		= -41,	//museum curator and mistress of Patterson
	CYNTHIA		= -42,	//treasurer and wife of James
	PATTERSON	= -43,	//mayor and philanderer
	CARROCIO	= -44,	//puppetmaster and secret lover of Nell
	FIGG		= -45,	//orchard owner
	JAMES		= -46,	//innkeeper
	JEANETTE	= -47,	//barmaid at the Blue Boar and courter of Charles and Willy (make up your mind you silly woman)
	DENBY		= -49,	//trainer
	FRED		= -50,	//merchant at Farmer's Market and husband of Kelly
	KELLY		= -51,	//merchant at Farmer's Market and wife of Fred
	WILLY		= -52,	//baker and total knob
	GAYE		= -53,	//seamstress
	COOP		= -54,	//arms dealer at Iolo's Bows
	GRAYSON		= -55,	//arms dealer
	DIANE		= -56,	//stablekeeper
	CLINT		= -57,	//shipwright
	GORDON		= -58,	//fish and chipper
	SEAN		= -59,	//jeweller
	BROWNIE		= -60,	//farmer and mayoral incumbent
	MACK		= -61,	//farmer and Kilrathi witness
	SNAZ		= -62,	//beggar and irritant
	MILLIE		= -63,	//fellowship recruiter and sister of Thad

	GEOFFREY	= -64,	//Castle British, captain of the guard and useless ex-follower
	WISLEM		= -65,	//Castle British, council member and gargoyle representative
	SHERRY		= -66,	//Castle British, mouse
	BOOTS		= -67,	//Castle British, cook and wife of Bennie
	BENNIE		= -68,	//Castle British, head servant and husband of Boots
	WESTON		= -69,	//Castle British, prisoner and husband of Alina
	MIRANDA		= -70,	//Castle British, council leader
	INWISLOKLEM	= -71,	//Castle British, council member and gargoyle representative
	NELL		= -72,	//Castle British, servant, sister of Charles, and secret lover of at least Carrocio and Lord British (slut)
	CHARLES		= -73,	//Castle British, servant, brother of Nell and courter of Jeanette

	KESSLER		= -237	//apothecary
};

//Cove residents
enum cove_npcs {
	RUDYOM			= -74,	//mad mage
	NASTASSIA		= -75,	//caretaker of shrine of Compassion and feeble love-interest
	RAYBURT			= -76,	//trainer and Pamela's main squeeze
	LORD_HEATHER	= -77,	//mayor and Jaana's main squeeze
	PAMELA			= -78,	//inkeeper and Rayburt's main squeeze
	ZINAIDA			= -79,	//tavernkeeper and de Maria's main squeeze
	DE_MARIA		= -80,	//tavernkeeper and Zinaida's main squeeze
	REGAL			= -271	//Rayburt's dog (not sure why he gets an NPC number, it's not like he does anything - maybe for schedules?)
};

//Minoc residents
enum minoc_npcs {
	ELYNOR		= -81,	//fellowship leader and secret lover of Gregor
	GREGOR		= -82,	//mine owner and secret lover of Elynor
	MARGARETA	= -83,	//gypsy, wife of Jergi and fortune-teller
	SASHA		= -84,	//clockmaker
	GLADSTONE	= -85,	//glassblower
	XANTHIA		= -86,	//candlemaker
	ZORN		= -87,	//blacksmith
	SEARA		= -88,	//artisan
	KARL		= -89,	//alienated woodsman and enemy of Owen
	OWEN		= -90,	//shipwright and tosser
	BURNSIDE	= -91,	//mayor
	RUTHERFORD	= -92,	//barkeep
	WILLIAM		= -93,	//sawmill operator
	KARENNA		= -94,	//trainer
	JAKHER		= -95,	//trainer
	JERGI		= -96,	//gypsy and husband of Margareta

	MIKOS		= -97,	//mine foreman
	FODUS		= -99,	//miner and drug addict
	OWINGS		= -239,	//miner and moron
	MALLOY		= -243	//miner and moron
};

//Yew/great forest residents
enum yew_npcs {
	TRELLEK		= -6,	//emp (Fun fact: his low ID is because he was originally meant to join the party)
	SARALEK		= -98,	//emp
	TAVENOR		= -100,	//emp
	SALAMON		= -101,	//emp
	NICODEMUS	= -102,	//mad mage
	THAD		= -103,	//highwayman and brother of Millie
	BRADMAN		= -104,	//trainer
	SIR_JEFF	= -105,	//judge
	TIERY		= -106,	//gravedigger and creep
	REYNA		= -108,	//healer
	GHARL		= -111,	//troll prisoner
	D_REL		= -112,	//pirate and prisoner
	SMITH		= -113,	//horse and hintbook
	AIMI		= -114,	//monk, gardener and crap painter
	PENNI		= -115,	//trainer and wife of Addom
	BEN			= -116,	//woodcutter
	GOTH		= -117,	//prison warden
	PERRIN		= -238,	//scholar and trainer
	TAYLOR		= -242,	//monk
	KREG		= -245,	//fugitive and false monk

	PAPA		= -241,	//Bee Caves, nudist
	MAMA		= -255,	//Bee Caves, nudist

	XORINIA		= -256	//will-o-wisp
};

//Jhelom residents
enum jhelom_npcs {
	DE_SNEL		= -119,	//trainer
	JOSEPH		= -120,	//mayor
	KLIFTIN		= -121,	//armourer
	OPHELIA		= -122,	//barmaid
	DAPHNE		= -123,	//barmaid
	SPRELLIC	= -124,	//tavern owner and wimp
	VOKES		= -125,	//big dumb fighter
	SYRIA		= -126,	//fighter
	TIMMONS		= -127	//fighter
};

//New Magincia residents
enum new_magincia_npcs {
	RUSSELL		= -129,	//shipwright
	BORIS		= -130,	//tavernkeeper and husband of Magenta
	MAGENTA		= -131,	//mayor and wife of Boris
	HENRY		= -132,	//courter of Constance and pathetic sap
	CONSTANCE	= -133,	//water-bearer and silly cow
	ROBIN		= -134,	//professional gambler and greaseball
	BATTLES		= -135,	//thug for Robin
	LEAVELL		= -136,	//thug for Robin
	SAM			= -137,	//flower man and total hippie
	ALAGNER		= -246	//scholar and plot device
};

//Skara Brae residents
enum skara_brae_npcs {
	MARKHAM		= -140,	//barkeep
	HORANCE		= -141,	//lich
	TRENT		= -142,	//blacksmith and husband of Rowena
	MORDRA		= -143,	//witch
	ROWENA		= -144,	//ensorcelled servant of Horance and wife of Trent
	PAULETTE	= -145,	//barmaid
	QUENTON		= -146,	//persistent murder victim from Ultima VI
	FORSYTHE	= -147,	//mayor and coward
	CAINE		= -247	//alchemist and The Tortured One
};

//Moonglow residents
enum moonglow_npcs {
	CARLYN		= -118,	//seamstress
	PENUMBRA	= -150,	//sleeping seeress
	ZELDA		= -152,	//Lycaeum, advisor and courter of Brion
	MARIAH		= -153,	//Lycaeum, mad mage and former companion
	CUBOLT		= -155,	//farmer
	BALAYNA		= -156,	//soon-to-be-murdered fellowship assistant
	TOLEMAC		= -157,	//farmer
	MORZ		= -158,	//f-f-f-farmer
	JILLIAN		= -159,	//Lycaeum, scholar, trainer and husband of Effrem
	EFFREM		= -160,	//househusband of Jillian
	CHAD		= -161,	//trainer
	ELAD		= -162,	//healer
	PHEARCY		= -163,	//tavernkeeper
	ADDOM		= -164,	//explorer and husband of Penni
	FRANK		= -165,	//Lycaeum, smartarse fox
	BRION		= -248,	//Lycaeum, astronomer
	NELSON		= -249,	//Lycaeum, curator
	RANKIN		= -250	//fellowship leader and murderer of Balayna
};

//Paws residents
enum paws_npcs {
	THURSTON	= -166,	//miller and courter of Polly
	FERIDWYN	= -167,	//fellowship shelter manager, father of Garritt and husband of Brita
	BRITA		= -168,	//snotty narrowminded bitch, mother of Garritt and wife of Feridwyn
	ALINA		= -169,	//useless poor person and wife of Weston
	MERRICK		= -170,	//sycophant and Fellowship shill
	GARRITT		= -171,	//drug abuser and insufferable offspring of Feridwyn and Brita
	MORFIN		= -172,	//slaughterhouse owner and drug dealer
	BEVERLEA	= -173,	//curio shop owner and blind old bat
	KOMOR		= -174,	//embittered beggar with one leg
	FENN		= -175,	//embittered cart-bound beggar with no legs
	ANDREW		= -176,	//dairy owner
	CAMILLE		= -177,	//farmer, widow and mother of Tobias
	TOBIAS		= -178,	//obnoxious son of Camille
	POLLY		= -179	//tavernkeeper and courter of Thurston
};

//Terfin residents
enum terfin_npcs {
	DRAXINUSOM	= -180,	//king of the Gargoyles
	INFORLEM	= -181,	//trainer and arms dealer
	INMANILEM	= -182,	//healer
	TEREGUS		= -183,	//keeper of altars
	RUNEB		= -184,	//fellowship thug
	QUAN		= -185,	//fellowship leader
	QUAEVEN		= -186,	//Rec Center master and fellowship follower
	SILAMO		= -187,	//gardener
	SARPLING	= -188,	//jeweller and magic provisioner
	FORBRAK		= -189,	//tavernkeeper
	BETRA		= -190	//provisioner
};

//residents of the U.S.S. Serpent's Hold
enum serpents_hold_npcs {
	MENION		= -192, //trainer and weaponsmith
	PENDARAN	= -193, //knight, saboteur and husband of Jehanne
	JEHANNE		= -194, //provisioner and wife of Pendaran
	JOHN_PAUL	= -195, //lord and leader
	RICHTER		= -196, //armourer and fellowship member
	HORFFE		= -197, //gargoyle and Captain of the Guard
	JORDAN		= -198, //blind arms dealer for Iolo's South who can't even recognise his own boss
	DENTON		= -199, //bartender, logician, and particularly lame Star Trek reference
	TORY		= -200, //empath and unconvincingly distraught mother of baby Riky, stolen by dingos (er, harpies)
	LEIGH		= -201	//healer
};

//Vesper residents
enum vesper_npcs {
	CADOR		= -203,	//mine overseer
	MARA		= -204,	//miner
	ZAKSAM		= -205,	//trainer
	ELDROTH		= -206,	//provisioner
	YONGI		= -207,	//tavernkeeper
	BLORN		= -208,	//criminal
	AUSTON		= -209,	//mayor
	LIANA		= -210,	//mayor's clerk and total bigot
	LAP_LEM		= -211,	//gargoyle miner
	YVELLA		= -212,	//mother of Catherine and wife of Cador
	CATHERINE	= -213,	//daughter of Yvella and Cador, student of For-Lem
	FOR_LEM		= -214,	//gargoyle handyman and teacher of Catherine
	ANSIKART	= -215,	//gargoyle tavernkeeper
	WIS_SUR		= -216,	//gargoyle magic provisioner
	ANMANIVAS	= -217,	//gargoyle ex-miner
	FORANAMO	= -218,	//gargoyle ex-miner
	AURVIDLEM	= -219	//provisioner
};

//Buccaneer's Den residents
enum buccaneers_den_npcs {
	SULLIVAN	= -220,	//fellowship prisoner and conman
	WENCH		= -221,	//whore
	GLENNO		= -222,	//manager of the Baths
	MARTINE		= -223,	//whore
	ROBERTO		= -224,	//gigolo
	SINTAG		= -225,	//House of Games guard
	BLACKTOOTH	= -226,	//ex-pirate
	MOLE		= -227,	//ex-pirate
	LUCKY		= -228,	//trainer
	BUDO		= -229,	//provisioner
	GORDY		= -230,	//House of Games owner
	MANDY		= -231,	//innkeeper
	SMITHY		= -232,	//House of Games croupier
	DANAG		= -251,	//fellowship leader

	GROD		= -154,	//troll and fellowship torturer
	ANTON		= -240	//fellowship prisoner
};

//Miscellaneous NPCs (dungeons and other minor locations)
enum misc_npcs {
	MARTINGO	= -191, //Sultan of Spektran

	IRIALE		= -128,	//Fellowship Retreat, guardian of the Cube
	IAN			= -202,	//Fellowship Retreat, director
	GORN		= -138,	//Fellowship Retreat, recurring cameo from long-forgotten Origin game

	WAYNE		= -109,	//Dungeon Despise, lost monk
	GAROK		= -110,	//Dungeon Despise, mad mage and tax fugitive

	EIKO		= -15,	//Dagger Isle, vengeful half-sister of Amanda
	AMANDA		= -48,	//Dagger Isle, vengeful half-sister of Eiko
	ISKANDER	= -107,	//Dagger Isle, cyclops and target of Eiko and Amanda's vengeance

	HYDRA		= -149,	//Ambrosia, three-headed hydra, guardian of meteor
	KISSME		= -151,	//Ambrosia, horny faerie

	CAIRBRE		= -244,	//Dungeon Destard, friend of Cosmo
	KALLIBRUS	= -252,	//Dungeon Destard, friend of Cosmo
	COSMO		= -253,	//Dungeon Destard, virgin
	LASHER		= -254,	//Dungeon Destard, unicorn and virgin-detector

	HOOK		= -291,	//Isle of the Avatar, arch-villain
	FORSKIS		= -298,	//Isle of the Avatar, arch-villain
	ABRAHAM		= -299,	//Isle of the Avatar, arch-villain
	ELIZABETH	= -300	//Isle of the Avatar, arch-villain
};
