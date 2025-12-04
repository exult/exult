/*
 *  fnames.h - Names of data files for Exult.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2025  The Exult Team
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

#ifndef FNAMES_H
#define FNAMES_H 1

// This will get prepended with different things at runtime
// depending on the OS
#define USER_CONFIGURATION_FILE "exult.cfg"

// without that define utils.cc errors with a "use of undeclared identifier
// EXULT_DATADIR"
#if (defined(MACOSX) || defined(SDL_PLATFORM_IOS)) && !defined(EXULT_DATADIR)
#	define EXULT_DATADIR "data"
#endif

/*
 *  Here are the files we use:
 */
#define GAMEDAT         "<GAMEDAT>/"
#define SHAPES_VGA      "<STATIC>/shapes.vga"
#define PATCH_SHAPES    "<PATCH>/shapes.vga"
#define FACES_VGA       "<STATIC>/faces.vga"
#define PATCH_FACES     "<PATCH>/faces.vga"
#define GUMPS_VGA       "<STATIC>/gumps.vga"
#define PATCH_GUMPS     "<PATCH>/gumps.vga"
#define FONTS_VGA       "<STATIC>/fonts.vga"
#define PATCH_FONTS     "<PATCH>/fonts.vga"
#define SPRITES_VGA     "<STATIC>/sprites.vga"
#define PATCH_SPRITES   "<PATCH>/sprites.vga"
#define MAINSHP_FLX     "<STATIC>/mainshp.flx"
#define U7MAINSHP_FLX   "<ULTIMA7_STATIC>/mainshp.flx"
#define PATCH_MAINSHP   "<PATCH>/mainshp.flx"
#define ENDSHAPE_FLX    "<STATIC>/endshape.flx"
#define PATCH_ENDSHAPE  "<PATCH>/endshape.flx"
#define SHPDIMS         "<STATIC>/shpdims.dat"
#define PATCH_SHPDIMS   "<PATCH>/shpdims.dat"
#define TFA             "<STATIC>/tfa.dat"
#define PATCH_TFA       "<PATCH>/tfa.dat"
#define WGTVOL          "<STATIC>/wgtvol.dat"
#define PATCH_WGTVOL    "<PATCH>/wgtvol.dat"
#define U7CHUNKS        "<STATIC>/u7chunks"
#define PATCH_U7CHUNKS  "<PATCH>/u7chunks"
#define U7MAP           "<STATIC>/u7map"
#define PATCH_U7MAP     "<PATCH>/u7map"
#define TEXT_FLX        "<STATIC>/text.flx"
#define PATCH_TEXT      "<PATCH>/text.flx"
#define PATCH_EXULTMSG  "<PATCH>/exultmsg.txt"
#define PATCH_EXULTMSG_FR "<PATCH>/exultmsg_fr.txt"
#define PATCH_EXULTMSG_DE "<PATCH>/exultmsg_de.txt"
#define PATCH_EXULTMSG_ES "<PATCH>/exultmsg_es.txt" 
#define U7IFIX "<STATIC>/u7ifix"
#define PATCH_U7IFIX    "<PATCH>/u7ifix"
#define U7IREG          "<GAMEDAT>/u7ireg"
#define MULTIMAP_DIR    "/map"
#define PALETTES_FLX    "<STATIC>/palettes.flx"
#define PATCH_PALETTES  "<PATCH>/palettes.flx"
#define INTRO_DAT       "<STATIC>/intro.dat"
#define PATCH_INTRO     "<PATCH>/intro.dat"
#define INTROPAL_DAT    "<STATIC>/intropal.dat"
#define PATCH_INTROPAL  "<PATCH>/intropal.dat"
#define U7NBUF_DAT      "<GAMEDAT>/u7nbuf.dat"
#define NPC_DAT         "<GAMEDAT>/npc.dat"
#define MONSNPCS        "<GAMEDAT>/monsnpcs.dat"
#define USEDAT          "<GAMEDAT>/usecode.dat"
#define USEVARS         "<GAMEDAT>/usecode.var"
#define FLAGINIT        "<GAMEDAT>/flaginit"
#define GWINDAT         "<GAMEDAT>/gamewin.dat"
#define GSCHEDULE       "<GAMEDAT>/schedule.dat"
#define SCHEDULE_DAT    "<STATIC>/schedule.dat"
#define SHPDIMS_DAT     "<STATIC>/shpdims.dat"
#define INITGAME        "<STATIC>/initgame.dat"
#define PATCH_INITGAME  "<PATCH>/initgame.dat"
#define USECODE         "<STATIC>/usecode"
#define PATCH_USECODE   "<PATCH>/usecode"
#define POINTERS        "<STATIC>/pointers.shp"
#define PATCH_POINTERS  "<PATCH>/pointers.shp"
#define MAINMUS         "<STATIC>/mt32mus.dat"
#define MAINMUS_AD      "<STATIC>/adlibmus.dat"
#define INTROMUS        "<STATIC>/intrordm.dat"
#define INTROMUS_AD     "<STATIC>/introadm.dat"
#define XMIDI_AD        "<STATIC>/xmidi.ad"
#define XMIDI_MT        "<STATIC>/xmidi.mt"
#define U7SPEECH        "<STATIC>/u7speech.spc"
#define SISPEECH        "<STATIC>/sispeech.spc"
#define PATCH_U7SPEECH  "<PATCH>/u7speech.spc"
#define PATCH_SISPEECH  "<PATCH>/sispeech.spc"
#define XFORMTBL        "<STATIC>/xform.tbl"
#define PATCH_XFORMS    "<PATCH>/xform.tbl"
#define BLENDS          "<STATIC>/blends.dat"
#define PATCH_BLENDS    "<PATCH>/blends.dat"
#define MONSTERS        "<STATIC>/monsters.dat"
#define PATCH_MONSTERS  "<PATCH>/monsters.dat"
#define EQUIP           "<STATIC>/equip.dat"
#define PATCH_EQUIP     "<PATCH>/equip.dat"
#define READY           "<STATIC>/ready.dat"
#define PATCH_READY     "<PATCH>/ready.dat"
#define WIHH            "<STATIC>/wihh.dat"
#define PATCH_WIHH      "<PATCH>/wihh.dat"
#define IDENTITY        "<GAMEDAT>/identity"
#define ENDGAME         "<STATIC>/endgame.dat"
#define PATCH_ENDGAME   "<PATCH>/endgame.dat"
#define ENDSCORE_XMI    "<STATIC>/endscore.xmi"
#define PATCH_ENDSCORE  "<PATCH>/endscore.xmi"
#define MIDITMPFILE     "u7midi"
#define MIDISFXFILE     "u7sfx"
#define SAVENAME        "<SAVEGAME>/exult%02d%s.sav"
#define SAVENAME2       "<SAVEGAME>/exult*%s.sav"
#define INTROSND        "<STATIC>/introsnd.dat"
#define PATCH_INTROSND  "<PATCH>/introsnd.dat"
#define PATCH_ARMOR     "<PATCH>/armor.dat"
#define ARMOR           "<STATIC>/armor.dat"
#define WEAPONS         "<STATIC>/weapons.dat"
#define PATCH_WEAPONS   "<PATCH>/weapons.dat"
#define AMMO            "<STATIC>/ammo.dat"
#define PATCH_AMMO      "<PATCH>/ammo.dat"
#define PAPERDOL        "<STATIC>/paperdol.vga"
#define PATCH_PAPERDOL  "<PATCH>/paperdol.vga"
#define OCCLUDE         "<STATIC>/occlude.dat"
#define PATCH_OCCLUDE   "<PATCH>/occlude.dat"
#define CONTAINER       "<STATIC>/container.dat"
#define PATCH_CONTAINER "<PATCH>/container.dat"
#define PATCH_ENDFONT   "<PATCH>/endfont.shp"

#define GSCRNSHOT   "<GAMEDAT>/scrnshot.shp"
#define GSAVEINFO   "<GAMEDAT>/saveinfo.dat"
#define GEXULTVER   "<GAMEDAT>/exult.ver"
#define GNEWGAMEVER "<GAMEDAT>/newgame.ver"
#define KEYRINGDAT  "<GAMEDAT>/keyring.dat"
#define NOTEBOOKXML "<GAMEDAT>/notebook.xml"

#define TEXTMSGS       "<STATIC>/textmsg.txt"
#define PATCH_TEXTMSGS "<PATCH>/textmsg.txt"
#define PATCH_MINIMAPS "<PATCH>/minimaps.vga"

#define R_SINTRO "<STATIC>/r_sintro.xmi"
#define A_SINTRO "<STATIC>/a_sintro.xmi"
#define R_SEND   "<STATIC>/r_send.xmi"
#define A_SEND   "<STATIC>/a_send.xmi"

#define U7VOICE_FLX  "<STATIC>/u7voice.flx"
#define MAINMENU_TIM "<STATIC>/mainmenu.tim"
#define INTRO_TIM    "<STATIC>/u7intro.tim"

#define EXULT_FLX     "<DATA>/exult.flx"
#define EXULT_BG_FLX  "<DATA>/exult_bg.flx"
#define EXULT_SI_FLX  "<DATA>/exult_si.flx"
#define EXULT_GAM_FLX "<DATA>/exult_%s.flx"

#define BUNDLE_EXULT_FLX     "<BUNDLE>/exult.flx"
#define BUNDLE_EXULT_BG_FLX  "<BUNDLE>/exult_bg.flx"
#define BUNDLE_EXULT_SI_FLX  "<BUNDLE>/exult_si.flx"
#define BUNDLE_EXULT_GAM_FLX "<BUNDLE>/exult_%s.flx"

#define AUTONOTES       "autonotes.txt"
#define PATCH_AUTONOTES "<PATCH>/autonotes.txt"

#define PATCH_KEYS "<PATCH>/patchkeys.txt"

#define GUMP_INFO       "<STATIC>/gump_info.txt"
#define PATCH_GUMP_INFO "<PATCH>/gump_info.txt"

#define EXULT_SERVER "<GAMEDAT>/exultserver"

#define NUM_FONTS (20)

// U7 game names in "exult.cfg":
#define CFG_BG_NAME    "blackgate"
#define CFG_BG_DE_NAME "schwarzepforte"
#define CFG_BG_ES_NAME "puertanegra"
#define CFG_BG_FR_NAME "portenoire"
#define CFG_FOV_NAME   "forgeofvirtue"
#define CFG_SI_NAME    "serpentisle"
#define CFG_SI_ES_NAME "islaserpiente"
#define CFG_SS_NAME    "silverseed"
#define CFG_SIB_NAME   "serpentbeta"
#define CFG_DEMO_NAME  "demo"

// U7 game titles in "exult.cfg":
#define CFG_BG_TITLE    "ULTIMA VII\nTHE BLACK GATE"
#define CFG_BG_DE_TITLE "ULTIMA VII\nDIE SCHWARZE PFORTE"
#define CFG_BG_ES_TITLE "ULTIMA VII\nLA PUERTA NEGRA"
#define CFG_BG_FR_TITLE "ULTIMA VII\nLA PORTE NOIRE"
#define CFG_FOV_TITLE   "ULTIMA VII\nTHE FORGE OF VIRTUE"
#define CFG_SI_TITLE    "ULTIMA VII PART 2\nSERPENT ISLE"
#define CFG_SI_ES_TITLE "ULTIMA VII PART 2\nLA ISLA SERPIENTE"
#define CFG_SS_TITLE    "ULTIMA VII PART 2\nTHE SILVER SEED"
#define CFG_SIB_TITLE   "ULTIMA VII PART 2\nSERPENT ISLE BETA"
#define CFG_DEMO_TITLE  "EXULT\nDEMO GAME"

// Exult SFX Packages:
#define SFX_ROLAND_BG  "sqsfxbg.flx"
#define SFX_ROLAND_SI  "sqsfxsi.flx"
#define SFX_BLASTER_BG "jmsfx.flx"
#define SFX_BLASTER_SI "jmsisfx.flx"
#define SFX_MIDIFILE   "midisfx.flx"

#endif
