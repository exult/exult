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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "bggame.h"

#include "Audio.h"
#include "AudioMixer.h"
#include "data/bg/introsfx_mt32_flx.h"
#include "data/exult_bg_flx.h"
#include "data/exult_flx.h"
#include "databuf.h"
#include "exult.h"
#include "exult_constants.h"
#include "files/utils.h"
#include "flic/playfli.h"
#include "fnames.h"
#include "font.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "imagewin/ArbScaler.h"
#include "imagewin/imagewin.h"
#include "items.h"
#include "mappatch.h"
#include "miscinf.h"
#include "modmgr.h"
#include "palette.h"
#include "playscene.h"
#include "shapeid.h"
#include "touchui.h"
#include "txtscroll.h"

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using std::rand;
using std::strchr;
using std::unique_ptr;

enum {
	ultima_text_shp  = 0x0D,
	butterfly_shp    = 0x0E,
	lord_british_shp = 0x11,
	trees_shp        = 0x12,

	guardian_mouth_shp    = 0x1E,
	guardian_forehead_shp = 0x1F,
	guardian_eyes_shp     = 0x20
};

enum {
	bird_song_midi = 0,
	home_song_midi = 1,
	guardian_midi  = 2,
	menu_midi      = 3,
	credits_midi   = 4,
	quotes_midi    = 5
};

BG_Game::BG_Game() : shapes(ENDSHAPE_FLX, -1, PATCH_ENDSHAPE) {
	if (!read_game_xml()) {
		add_shape("gumps/check", 2);
		add_shape("gumps/fileio", 3);
		add_shape("gumps/fntext", 4);
		add_shape("gumps/loadbtn", 5);
		add_shape("gumps/savebtn", 6);
		add_shape("gumps/halo", 7);
		add_shape("gumps/disk", 24);
		add_shape("gumps/heart", 25);
		add_shape("gumps/statatts", 28);
		add_shape("gumps/musicbtn", 29);
		add_shape("gumps/speechbtn", 30);
		add_shape("gumps/soundbtn", 31);
		add_shape("gumps/spellbook", 43);
		add_shape("gumps/statsdisplay", 47);
		add_shape("gumps/combat", 46);
		add_shape("gumps/quitbtn", 56);
		add_shape("gumps/yesnobox", 69);
		add_shape("gumps/yesbtn", 70);
		add_shape("gumps/nobtn", 71);
		add_shape("gumps/book", 32);
		add_shape("gumps/scroll", 55);
		add_shape("gumps/combatmode", 12);
		add_shape("gumps/slider", 14);
		add_shape("gumps/slider_diamond", 15);
		add_shape("gumps/slider_right", 16);
		add_shape("gumps/slider_left", 17);

		add_shape("gumps/box", 0);
		add_shape("gumps/crate", 1);
		add_shape("gumps/barrel", 8);
		add_shape("gumps/bag", 9);
		add_shape("gumps/backpack", 10);
		add_shape("gumps/basket", 11);
		add_shape("gumps/chest", 22);
		add_shape("gumps/shipshold", 26);
		add_shape("gumps/drawer", 27);
		add_shape("gumps/woodsign", 49);
		add_shape("gumps/tombstone", 50);
		add_shape("gumps/goldsign", 51);
		add_shape("gumps/body", 53);

		add_shape("gumps/scroll_spells", 0xfff);
		add_shape("gumps/spell_scroll", 0xfff);
		add_shape("gumps/jawbone", 0xfff);
		add_shape("gumps/tooth", 0xfff);

		add_shape("sprites/map", 22);
		add_shape("sprites/cheatmap", EXULT_BG_FLX_BGMAP_SHP);

		const char* exultflx = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
		const char* gameflx  = BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX);

		add_resource("files/shapes/count", nullptr, 9);
		add_resource("files/shapes/0", SHAPES_VGA, 0);
		add_resource("files/shapes/1", FACES_VGA, 0);
		add_resource("files/shapes/2", GUMPS_VGA, 0);
		add_resource("files/shapes/3", SPRITES_VGA, 0);
		add_resource("files/shapes/4", MAINSHP_FLX, 0);
		add_resource("files/shapes/5", ENDSHAPE_FLX, 0);
		add_resource("files/shapes/6", FONTS_VGA, 0);
		add_resource("files/shapes/7", exultflx, 0);
		add_resource("files/shapes/8", gameflx, 0);

		add_resource("files/gameflx", gameflx, 0);

		add_resource(
				"files/paperdolvga", gameflx, EXULT_BG_FLX_BG_PAPERDOL_VGA);
		add_resource("files/mrfacesvga", gameflx, EXULT_BG_FLX_BG_MR_FACES_VGA);
		add_resource(
				"config/defaultkeys", gameflx, EXULT_BG_FLX_DEFAULTKEYS_TXT);
		add_resource("config/bodies", gameflx, EXULT_BG_FLX_BODIES_TXT);
		add_resource(
				"config/paperdol_info", gameflx,
				EXULT_BG_FLX_PAPERDOL_INFO_TXT);
		add_resource("config/shape_info", gameflx, EXULT_BG_FLX_SHAPE_INFO_TXT);
		add_resource(
				"config/shape_files", gameflx, EXULT_BG_FLX_SHAPE_FILES_TXT);
		add_resource(
				"config/avatar_data", gameflx, EXULT_BG_FLX_AVATAR_DATA_TXT);
		add_resource("config/autonotes", gameflx, EXULT_BG_FLX_AUTONOTES_TXT);
		add_resource("files/intro_hand", gameflx, EXULT_BG_FLX_INTRO_HAND_SHP);

		add_resource("palettes/count", nullptr, 18);
		add_resource("palettes/0", PALETTES_FLX, 0);
		add_resource("palettes/1", PALETTES_FLX, 1);
		add_resource("palettes/2", PALETTES_FLX, 2);
		add_resource("palettes/3", PALETTES_FLX, 3);
		add_resource("palettes/4", PALETTES_FLX, 4);
		add_resource("palettes/5", PALETTES_FLX, 5);
		add_resource("palettes/6", PALETTES_FLX, 6);
		add_resource("palettes/7", PALETTES_FLX, 7);
		add_resource("palettes/8", PALETTES_FLX, 8);
		add_resource("palettes/9", PALETTES_FLX, 10);
		add_resource("palettes/10", PALETTES_FLX, 11);
		add_resource("palettes/11", PALETTES_FLX, 12);
		add_resource("palettes/12", INTROPAL_DAT, 0);
		add_resource("palettes/13", INTROPAL_DAT, 1);
		add_resource("palettes/14", INTROPAL_DAT, 2);
		add_resource("palettes/15", INTROPAL_DAT, 3);
		add_resource("palettes/16", INTROPAL_DAT, 4);
		add_resource("palettes/17", INTROPAL_DAT, 5);

		add_resource("palettes/patch/0", PATCH_PALETTES, 0);
		add_resource("palettes/patch/1", PATCH_PALETTES, 1);
		add_resource("palettes/patch/2", PATCH_PALETTES, 2);
		add_resource("palettes/patch/3", PATCH_PALETTES, 3);
		add_resource("palettes/patch/4", PATCH_PALETTES, 4);
		add_resource("palettes/patch/5", PATCH_PALETTES, 5);
		add_resource("palettes/patch/6", PATCH_PALETTES, 6);
		add_resource("palettes/patch/7", PATCH_PALETTES, 7);
		add_resource("palettes/patch/8", PATCH_PALETTES, 8);
		add_resource("palettes/patch/9", PATCH_PALETTES, 10);
		add_resource("palettes/patch/10", PATCH_PALETTES, 11);
		add_resource("palettes/patch/11", PATCH_PALETTES, 12);
		add_resource("palettes/patch/12", PATCH_INTROPAL, 0);
		add_resource("palettes/patch/13", PATCH_INTROPAL, 1);
		add_resource("palettes/patch/14", PATCH_INTROPAL, 2);
		add_resource("palettes/patch/15", PATCH_INTROPAL, 3);
		add_resource("palettes/patch/16", PATCH_INTROPAL, 4);
		add_resource("palettes/patch/17", PATCH_INTROPAL, 5);

		add_resource("xforms/count", nullptr, 20);
		add_resource("xforms/0", XFORMTBL, 0);
		add_resource("xforms/1", XFORMTBL, 1);
		add_resource("xforms/2", XFORMTBL, 2);
		add_resource("xforms/3", XFORMTBL, 3);
		add_resource("xforms/4", XFORMTBL, 4);
		add_resource("xforms/5", XFORMTBL, 5);
		add_resource("xforms/6", XFORMTBL, 6);
		add_resource("xforms/7", XFORMTBL, 7);
		add_resource("xforms/8", XFORMTBL, 8);
		add_resource("xforms/9", XFORMTBL, 9);
		add_resource("xforms/10", XFORMTBL, 10);
		add_resource("xforms/11", XFORMTBL, 11);
		add_resource("xforms/12", XFORMTBL, 12);
		add_resource("xforms/13", XFORMTBL, 13);
		add_resource("xforms/14", XFORMTBL, 14);
		add_resource("xforms/15", XFORMTBL, 15);
		add_resource("xforms/16", XFORMTBL, 16);
		add_resource("xforms/17", XFORMTBL, 17);
		add_resource("xforms/18", XFORMTBL, 18);
		add_resource("xforms/19", XFORMTBL, 19);
	}
	fontManager.add_font("MENU_FONT", MAINSHP_FLX, PATCH_MAINSHP, 9, 1);
	fontManager.add_font("END2_FONT", ENDGAME, PATCH_ENDGAME, 4, -1);
	fontManager.add_font("END3_FONT", ENDGAME, PATCH_ENDGAME, 5, -2);
	fontManager.add_font("NORMAL_FONT", FONTS_VGA, PATCH_FONTS, 0, -1);
	fontManager.add_font("SMALL_BLACK_FONT", FONTS_VGA, PATCH_FONTS, 2, 0);
	fontManager.add_font("TINY_BLACK_FONT", FONTS_VGA, PATCH_FONTS, 4, 0);
	fontManager.add_font("GUARDIAN_FONT", MAINSHP_FLX, PATCH_MAINSHP, 3, -2);
	fontManager.add_font(
			"EXULT_END_FONT", File_spec(EXULT_FLX, EXULT_FLX_ENDFONT_SHP),
			PATCH_ENDFONT, 0, -1);
	auto& mp = gwin->get_map_patches();
	// Sawdust in Iolo's hut is at lift 2, should be 0
	// FIXME - the original had some way to deal with this
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(481, 599, 2), 224, 7, 0),
			Object_spec(Tile_coord(480, 598, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(482, 601, 2), 224, 7, 0),
			Object_spec(Tile_coord(481, 600, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(482, 600, 2), 224, 7, 0),
			Object_spec(Tile_coord(481, 599, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(481, 602, 2), 224, 7, 0),
			Object_spec(Tile_coord(480, 601, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(479, 599, 2), 224, 7, 0),
			Object_spec(Tile_coord(478, 598, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(477, 597, 2), 224, 7, 0),
			Object_spec(Tile_coord(476, 596, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(476, 597, 2), 224, 7, 0),
			Object_spec(Tile_coord(475, 596, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(472, 595, 2), 224, 7, 0),
			Object_spec(Tile_coord(471, 594, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(473, 598, 2), 224, 7, 0),
			Object_spec(Tile_coord(472, 597, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(472, 600, 2), 224, 7, 0),
			Object_spec(Tile_coord(471, 599, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(470, 597, 2), 224, 7, 0),
			Object_spec(Tile_coord(469, 596, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(469, 597, 2), 224, 7, 0),
			Object_spec(Tile_coord(468, 596, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(467, 599, 2), 224, 7, 0),
			Object_spec(Tile_coord(466, 598, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(468, 600, 2), 224, 7, 0),
			Object_spec(Tile_coord(467, 599, 0), 224, 7, 0)));
	mp.add(std::make_unique<Map_patch_modify>(
			Object_spec(Tile_coord(467, 601, 2), 224, 7, 0),
			Object_spec(Tile_coord(466, 600, 0), 224, 7, 0)));
	// pure BG shape 874 only has an empty frame. For FoV's Test of Truth
	// it was changed for creating a dungeon chasm while obviously forgetting
	// that frame #0 was used in two occassions:
	//       hole in Despise's rebel base to invisible stairway
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(681, 1192, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(682, 1192, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(681, 1195, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(682, 1195, 5), 874, 0, 0)));
	//       stairways hole in Shame's 2nd floor
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(807, 951, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(811, 951, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(807, 955, 5), 874, 0, 0)));
	mp.add(std::make_unique<Map_patch_remove>(
			Object_spec(Tile_coord(811, 955, 5), 874, 0, 0)));
}

#define WAITDELAY(x)                    \
	do {                                \
		switch (wait_delay(x)) {        \
		case 1:                         \
			throw UserBreakException(); \
			break;                      \
		case 2:                         \
			throw UserSkipException();  \
			break;                      \
		}                               \
	} while (false)

#define WAITDELAYCYCLE1(x)                     \
	do {                                       \
		switch (wait_delay((x), 16, 78, 50)) { \
		case 1:                                \
			throw UserBreakException();        \
			break;                             \
		case 2:                                \
			throw UserSkipException();         \
			break;                             \
		}                                      \
	} while (false)

#define WAITDELAYCYCLE2(x)                 \
	do {                                   \
		switch (wait_delay((x), 250, 5)) { \
		case 1:                            \
			throw UserBreakException();    \
			break;                         \
		case 2:                            \
			throw UserSkipException();     \
			break;                         \
		}                                  \
	} while (false)

#define WAITDELAYCYCLE3(x)                  \
	do {                                    \
		switch (wait_delay((x), 240, 15)) { \
		case 1:                             \
			throw UserBreakException();     \
			break;                          \
		case 2:                             \
			throw UserSkipException();      \
			break;                          \
		}                                   \
	} while (false)

#define WAITDELAYCYCLE4(x)                     \
	do {                                       \
		switch (wait_delay((x), 16, 78, 15)) { \
		case 1:                                \
			throw UserBreakException();        \
			break;                             \
		case 2:                                \
			throw UserSkipException();         \
			break;                             \
		}                                      \
	} while (false)

#define WAITDELAYCYCLE5(x)                      \
	do {                                        \
		switch (wait_delay((x), 16, -78, 50)) { \
		case 1:                                 \
			throw UserBreakException();         \
			break;                              \
		case 2:                                 \
			throw UserSkipException();          \
			break;                              \
		}                                       \
	} while (false)

#define WAITDELAYCYCLE6(x)                      \
	do {                                        \
		switch (wait_delay((x), 16, -78, 15)) { \
		case 1:                                 \
			throw UserBreakException();         \
			break;                              \
		case 2:                                 \
			throw UserSkipException();          \
			break;                              \
		}                                       \
	} while (false)

void BG_Game::play_intro() {
	Audio* audio = Audio::get_ptr();
	audio->stop_music();
	if (scene_available("intro")) {
		// Play the custom intro scene
		play_scene("intro");
		return;
	}
	MyMidiPlayer* midi = audio->get_midi();
	if (midi) {
		midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);
	}

	gwin->clear_screen(true);

	// TODO: check/fix other resolutions

	try {
		/********************************************************************
		 Lord British Presents
		********************************************************************/

		scene_lord_british();

		/********************************************************************
		 Ultima VII logo w/Trees
		********************************************************************/

		scene_butterfly();

		/********************************************************************
		 Enter guardian
		********************************************************************/

		scene_guardian();

		/********************************************************************
		 PC screen
		********************************************************************/

		scene_desk();

		/********************************************************************
		 The Moongate
		********************************************************************/

		scene_moongate();
	} catch (const UserBreakException& /*x*/) {
	}

	// Fade out the palette...
	pal->fade_out(c_fade_out_time);

	// ... and clean the screen.
	gwin->clear_screen(true);

	// Stop all audio output
	audio->cancel_streams();
}

File_spec BG_Game::get_sfx_subflex() {
	if (Audio::have_sblaster_sfx(BLACK_GATE)) {
		return File_spec{
				game->get_resource("files/gameflx").str,
				EXULT_BG_FLX_INTROSFX_SB_FLX};
	}
	// TODO: MIDI SFX
	// if (audio->have_midi_sfx()) {
	// 	return File_spec{game->get_resource("files/gameflx").str,
	// EXULT_BG_FLX_INTROSFX_MTIDI_FLX};
	// }
	return File_spec{
			game->get_resource("files/gameflx").str,
			EXULT_BG_FLX_INTROSFX_MT32_FLX};
}

void BG_Game::scene_lord_british() {
	/*
	 *  Enhancements to lip-syncing contributed by
	 *  Eric Wasylishen, Jun. 19, 2006.
	 */

	try {
		WAITDELAY(1500);    // - give a little space between exult title music
		//     and LB presents screen

		// Lord British presents...  (sh. 0x11)
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 3);
		sman->paint_shape(topx, topy, shapes.get_shape(lord_british_shp, 0));
		gwin->get_win()->FillGuardband();
		pal->fade_in(c_fade_in_time);
		if (1 == wait_delay(2000)) {
			throw UserBreakException();
		}
		pal->fade_out(c_fade_out_time);
	} catch (const UserSkipException& /*x*/) {
	}
	gwin->clear_screen(true);
}

void BG_Game::scene_butterfly() {
	constexpr static const int frame_duration
			= 23;    // - used to be 16.. too fast.
	constexpr static const int frame_count = 3;

	constexpr static const std::array butterfly_x{
			6,   18,  30,  41,  52,  62,  70,  78,  86,  95,  104, 113,
			122, 132, 139, 146, 151, 155, 157, 158, 157, 155, 151, 146,
			139, 132, 124, 116, 108, 102, 96,  93,  93,  93,  95,  99,
			109, 111, 118, 125, 132, 140, 148, 157, 164, 171, 178, 184,
			190, 196, 203, 211, 219, 228, 237, 246, 254, 259, 262, 264,
			265, 265, 263, 260, 256, 251, 245, 239, 232, 226, 219, 212,
			208, 206, 206, 209, 212, 216, 220, 224, 227, 234, 231, 232,
			233, 233, 233, 233, 234, 236, 239, 243, 247, 250, 258, 265};

	constexpr static const std::array butterfly_y{
			155, 153, 151, 150, 149, 148, 148, 148, 148, 149, 150, 150,
			150, 149, 147, 142, 137, 131, 125, 118, 110, 103, 98,  94,
			92,  91,  91,  91,  92,  95,  99,  104, 110, 117, 123, 127,
			131, 134, 135, 135, 135, 135, 135, 134, 132, 129, 127, 123,
			119, 115, 112, 109, 104, 102, 101, 102, 109, 109, 114, 119,
			125, 131, 138, 144, 149, 152, 156, 158, 159, 159, 158, 155,
			150, 144, 137, 130, 124, 118, 112, 105, 99,  93,  86,  80,
			73,  66,  59,  53,  47,  42,  38,  35,  32,  29,  26,  25};
	static_assert(butterfly_x.size() == butterfly_y.size());
	constexpr static const size_t butterfly_num_coords = butterfly_x.size();

	constexpr static const std::array butterfly_end_frames{3, 4, 3, 4,
														   3, 2, 1, 0};
	constexpr static const std::array butterfly_end_delay{167, 416, 250, 416,
														  416, 416, 416, 333};

	int frame;

	auto DrawButterfly = [&](int x, int y, int frame, int delay,
							 Image_buffer* backup, Shape_frame* butterfly) {
		// Draw butterfly
		win->get(
				backup, topx + x - butterfly->get_xleft(),
				topy + y - butterfly->get_yabove());
		sman->paint_shape(
				topx + x, topy + y, shapes.get_shape(butterfly_shp, frame));
		win->ShowFillGuardBand();
		WAITDELAY(delay);
		win->put(
				backup, topx + x - butterfly->get_xleft(),
				topy + y - butterfly->get_yabove());
	};

	int  dir           = 0;
	auto ButterflyFlap = [&]() {
		if ((rand() % 5) < 4) {
			if (frame == 3) {
				dir = -1;
			} else if (frame == 0) {
				dir = +1;
			}
			frame += dir;
		}
	};

	try {
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 4);

		// Load the butterfly shape
		Shape_frame* butterfly(shapes.get_shape(butterfly_shp, 0));
		const unique_ptr<Image_buffer> backup(win->create_buffer(
				butterfly->get_width(), butterfly->get_height()));

		// Start playing the birdsongs while still faded out
		Audio::get_ptr()->start_music(
				bird_song_midi, false, MyMidiPlayer::Force_None, INTROMUS);

		// trees with "Ultima VII" on top of 'em
		sman->paint_shape(topx, topy, shapes.get_shape(trees_shp, 0));
		sman->paint_shape(
				topx + 160, topy + 50, shapes.get_shape(ultima_text_shp, 0));
		win->FillGuardband();
		// Keep it dark for some more time, playing the music
		WAITDELAY(4500);    //  - was WAITDELAY(3500);

		// Finally fade in
		pal->fade_in(c_fade_in_time);

		WAITDELAY(4000);

		win->ShowFillGuardBand();

		WAITDELAY(7100);

		//
		// Move the butterfly along its path
		//
		frame        = 0;
		Sint32 delay = frame_duration;
		for (size_t i = 0; i < butterfly_num_coords - 1; ++i) {
			for (int j = 0; j < frame_count; ++j) {
				Sint32    ticks = SDL_GetTicks();
				const int x     = butterfly_x[i]
							  + j * (butterfly_x[i + 1] - butterfly_x[i])
										/ frame_count;
				const int y = butterfly_y[i]
							  + j * (butterfly_y[i + 1] - butterfly_y[i])
										/ frame_count;
				DrawButterfly(x, y, frame, delay, backup.get(), butterfly);

				// Flap the wings; but not always, so that the butterfly
				// "glides" from time to time
				ButterflyFlap();

				// Calculate the difference between the time we wanted to spent
				// and the time we actually spent; then adjust 'delay'
				// accordingly
				ticks = SDL_GetTicks() - ticks;
				delay = (delay + (2 * frame_duration - ticks)) / 2;

				// ... but maybe we also have to skip frames..
				if (delay < 0) {
					// Calculate how many frames we should skip
					int       frames_to_skip = (-delay) / frame_duration + 1;
					const int new_index = i * frame_count + j + frames_to_skip;
					i                   = new_index / frame_count;
					j                   = new_index % frame_count;

					// Did we skip over the end?
					if (i >= butterfly_num_coords - 1) {
						break;
					}

					while (frames_to_skip--) {
						ButterflyFlap();
					}

					delay = 0;
				}
			}
		}

		// Finally, let it flutter a bit on the end spot
		for (int i = 0; i < 8; i++) {
			DrawButterfly(
					butterfly_x[butterfly_num_coords - 1],
					butterfly_y[butterfly_num_coords - 1],
					butterfly_end_frames[i], butterfly_end_delay[i],
					backup.get(), butterfly);
		}

		WAITDELAY(1000);

		// Wait till the music finished playing
		while (Audio::get_ptr()->is_track_playing(bird_song_midi)) {
			WAITDELAY(20);
		}
	} catch (const UserSkipException& /*x*/) {
	}
}

#define FLASH_SHAPE1(x, y, shape, frame, delay)                              \
	do {                                                                     \
		sman->paint_shape((x), (y), shapes.get_shape((shape), (frame)));     \
		win->ShowFillGuardBand();                                            \
		WAITDELAYCYCLE1((delay));                                            \
		win->put(backup.get(), (x) - s->get_xleft(), (y) - s->get_yabove()); \
	} while (0)

#define FLASH_SHAPE2(x, y, shape, frame, delay)                              \
	do {                                                                     \
		sman->paint_shape((x), (y), shapes.get_shape((shape), (frame)));     \
		win->ShowFillGuardBand();                                            \
		WAITDELAYCYCLE4((delay));                                            \
		win->put(backup.get(), (x) - s->get_xleft(), (y) - s->get_yabove()); \
	} while (0)

class LipSynchReader {
	std::unique_ptr<IBufferDataView> data;

public:
	LipSynchReader()
			: data(std::make_unique<IExultDataSource>(
					  MAINSHP_FLX, PATCH_MAINSHP, 0x0F)) {}

	LipSynchReader(const char* pp, int len)
			: data(std::make_unique<IBufferDataView>(pp, len)) {}

	bool have_events() const {
		return data->getAvail() > 0;
	}

	void get_event(int& time, int& code) {
		if (have_events()) {
			const int curr_time = data->read2();
			time                = curr_time * 1000.0 / 60.0;
			code                = data->read1();
		} else {
			time = 0;
			code = 0;
		}
	}

	void translate_code(int code, int& mouth, int& eyes, int& lasteyes) {
		// Lookup table for translating lipsynch data. The table is indexed by
		// the current frame-1, then by read lipsynch data-1.
		constexpr static const int eye_frame_LUT[6][9] = {
				//   1, 2, 3, 4, 5, 6, 7, 8, 9		// Last eye frame (or the
				//   one before, if it was 10)
				{1, 2, 3, 1, 2, 3, 1, 2,3                                        }, // 1: Change eyebrows to angry
				{4, 5, 6, 4, 5, 6, 4, 5, 6}, // 2: Change eyebrows to neutral
				{7, 8, 9, 7, 8, 9, 7, 8, 9}, // 3: Change eyebrows to raised
				{1, 1, 1, 4, 4, 4, 7, 7, 7}, // 4: Change eyes to fully open
				{2, 2, 2, 5, 5, 5, 8, 8, 8}, // 5: Change eyes to half-open
				{3, 3, 3, 6, 6, 6, 9, 9,
				 9						}, // 6: Change eyes to fully closed
		};
		// Set based on code read
		if (code >= 8 && code < 23) {
			// This changes mouth frame
			mouth = code - 8;
		} else if (code == 7) {
			// This sets eyes to looking down
			// Don't update last eye frame
			eyes = 10;
		} else if (code > 0 && code < 7) {
			// This changes eyes and eyebrows
			eyes     = eye_frame_LUT[code - 1][lasteyes - 1];
			lasteyes = eyes;
		}
	}
};

// Simple RAII wrapper for SDL_DestroySurface
class SDL_SurfaceOwner {
	SDL_Surface* surf;

public:
	SDL_SurfaceOwner(Image_buffer* src, SDL_Surface* draw) {
		const SDL_PixelFormatDetails* draw_format
				= SDL_GetPixelFormatDetails(draw->format);
		surf = SDL_CreateSurfaceFrom(
				src->get_width(), src->get_height(),
				SDL_GetPixelFormatForMasks(
						draw_format->bits_per_pixel, draw_format->Rmask,
						draw_format->Gmask, draw_format->Bmask,
						draw_format->Amask),
				src->get_bits(), src->get_line_width());
	}

	~SDL_SurfaceOwner() noexcept {
		SDL_DestroySurface(surf);
	}

	SDL_Surface* get() noexcept {
		return surf;
	}
};

// Simple RAII wrapper for managing window clip
class WinClip {
	Image_window8* window;

public:
	WinClip(Image_window8* win, int cx, int cy, int wid, int hgt)
			: window(win) {
		window->set_clip(cx, cy, wid, hgt);
	}

	~WinClip() noexcept {
		window->clear_clip();
	}
};

// Simple RAII wrapper for stopping speech
struct SpeechManager {
	SpeechManager(const char* fname, const char* fpatch, bool wait) {
		if (Audio::get_ptr()->is_audio_enabled()
			&& Audio::get_ptr()->is_speech_enabled()) {
			Audio::get_ptr()->playSpeechfile(fname, fpatch, wait);
		}
	}

	~SpeechManager() noexcept {
		Audio::get_ptr()->stop_speech();
	}
};

void BG_Game::scene_guardian() {
	constexpr static const int Eyes_Dist     = 12;
	constexpr static const int Forehead_Dist = 49;

	constexpr static const std::array text_times{
			250,   1700,  5250,  8200,  10550, 12900, 16000, 19950,
			22700, 27200, 32400, 36400, 39650, 42400, 44000};

	constexpr static const size_t text_num_frames = text_times.size();

	constexpr static const char surfacing_data[]{
			0x03, 0x00, 0x02,    // Eyebrows -> neutral
			0x03, 0x00, 0x05,    // Eyes -> half-open
			0x15, 0x00, 0x04,    // Eyes -> fully open
			0x33, 0x00, 0x07,    // Eyes -> Look down
			0x54, 0x00, 0x04,    // Eyes -> fully open
			0x77, 0x00, 0x00};

	try {
		Uint32          ticks;
		const File_spec sfxfile = get_sfx_subflex();
		{
			// create buffer containing a blue 'plasma' screen
			const unique_ptr<Image_buffer> plasma(win->create_buffer(
					win->get_full_width(), win->get_full_height()));
			gwin->plasma(
					win->get_full_width(), win->get_full_height(),
					win->get_start_x(), win->get_start_y(), 16, 16 + 76);
			win->get(plasma.get(), win->get_start_x(), win->get_start_y());

			pal->load(INTROPAL_DAT, PATCH_INTROPAL, 2);
			pal->set_color(
					1, 0, 0, 0);    // UGLY hack... set font background to black
			pal->apply();

			// play static SFX
			Audio::get_ptr()->play_sound_effect(
					sfxfile, INTROSFX_MT32_FLX_INTRO_MT_STATIC1_WAV);

			//
			// Show some "static" alternating with the blue plasma
			//
			// TODO: Is this the right kind of static? Dosbox shows a mostly
			// black
			//      with an ocassional white pixel static - but is this what it
			//      was really like?

			ticks = SDL_GetTicks();
			while (true) {
				win->get_ibuf()->fill_static(0, 7, 15);
				win->ShowFillGuardBand();
				WAITDELAYCYCLE1(2);
				if (SDL_GetTicks() > ticks + 400) {    // 400)
					break;
				}
			}

			Audio::get_ptr()->play_sound_effect(
					sfxfile, INTROSFX_MT32_FLX_INTRO_MT_STATIC2_WAV);

			win->put(plasma.get(), win->get_start_x(), win->get_start_y());
			win->ShowFillGuardBand();
			WAITDELAYCYCLE1(200);

			ticks = SDL_GetTicks();
			while (true) {
				win->get_ibuf()->fill_static(0, 7, 15);
				win->ShowFillGuardBand();
				WAITDELAYCYCLE1(2);
				if (SDL_GetTicks() > ticks + 200) {
					break;
				}
			}

			Audio::get_ptr()->play_sound_effect(
					sfxfile, INTROSFX_MT32_FLX_INTRO_MT_STATIC3_WAV);

			win->put(plasma.get(), win->get_start_x(), win->get_start_y());
			win->ShowFillGuardBand();
			WAITDELAYCYCLE1(200);

			ticks = SDL_GetTicks();
			while (true) {
				win->get_ibuf()->fill_static(0, 7, 15);
				win->ShowFillGuardBand();
				WAITDELAYCYCLE1(2);
				if (SDL_GetTicks() > ticks + 100) {
					break;
				}
			}

			win->put(plasma.get(), win->get_start_x(), win->get_start_y());
			win->ShowFillGuardBand();
		}

		//
		// Start background music
		//
		Audio::get_ptr()->start_music(
				guardian_midi, false, MyMidiPlayer::Force_None, INTROMUS);

		WAITDELAYCYCLE1(3800);

		//
		// First 'popup' (sh. 0x21)
		//

		Audio::get_ptr()->play_sound_effect(
				sfxfile, INTROSFX_MT32_FLX_INTRO_MT_GUARDIAN1_WAV);

		{
			Shape_frame*                   s = shapes.get_shape(0x21, 0);
			const unique_ptr<Image_buffer> backup(
					win->create_buffer(s->get_width(), s->get_height()));
			win->get(
					backup.get(), centerx - 53 - s->get_xleft(),
					centery - 68 - s->get_yabove());
			for (int i = 8; i >= -8; i--) {
				FLASH_SHAPE2(centerx - 53, centery - 68, 0x21, 1 + abs(i), 80);
			}
		}
		WAITDELAYCYCLE1(2000);

		//
		// Second 'popup' (sh. 0x22)
		//
		Audio::get_ptr()->play_sound_effect(
				sfxfile, INTROSFX_MT32_FLX_INTRO_MT_GUARDIAN2_WAV);

		{
			Shape_frame*                   s = shapes.get_shape(0x22, 0);
			const unique_ptr<Image_buffer> backup(
					win->create_buffer(s->get_width(), s->get_height()));
			win->get(
					backup.get(), centerx - s->get_xleft(),
					centery - 45 - s->get_yabove());
			for (int i = 9; i >= -9; i--) {
				FLASH_SHAPE2(centerx, centery - 45, 0x22, 9 - abs(i), 80);
			}
		}
		WAITDELAYCYCLE1(2000);

		//
		// Successful 'popup' (sh. 0x23)
		//
		Audio::get_ptr()->play_sound_effect(
				sfxfile, INTROSFX_MT32_FLX_INTRO_MT_GUARDIAN3_WAV);

		{
			Shape_frame*                   s = shapes.get_shape(0x23, 0);
			const unique_ptr<Image_buffer> backup(
					win->create_buffer(s->get_width(), s->get_height()));
			const unique_ptr<Image_buffer> cbackup(
					win->create_buffer(s->get_width(), s->get_height()));

			win->get(
					cbackup.get(), centerx - s->get_xleft(),
					centery - s->get_yabove());
			sman->paint_shape(
					centerx, centery, s);    // frame 0 is static background
			win->get(
					backup.get(), centerx - s->get_xleft(),
					centery - s->get_yabove());
			for (int i = 1; i < 16; i++) {
				FLASH_SHAPE2(centerx, centery, 0x23, i, 70);
			}

			sman->paint_shape(centerx, centery, shapes.get_shape(0x23, 15));
			win->ShowFillGuardBand();

			WAITDELAYCYCLE1(500);    // - show his face for half a second
			// before he opens his eyes.

			win->put(
					cbackup.get(), centerx - s->get_xleft(),
					centery - s->get_yabove());
		}
		//
		// Actual appearance
		//

		// mouth
		{
			Shape_frame* s = shapes.get_shape(guardian_mouth_shp, 0);
			unique_ptr<Image_buffer> backup(
					win->create_buffer(s->get_width(), s->get_height()));
			const unique_ptr<Image_buffer> cbackup(
					win->create_buffer(s->get_width(), s->get_height()));
			win->get(
					cbackup.get(), centerx - s->get_xleft(),
					centery - s->get_yabove());
			sman->paint_shape(centerx, centery, s);    // frame 0 is background
			win->get(
					backup.get(), centerx - s->get_xleft(),
					centery - s->get_yabove());
			// eyes
			Shape_frame* s2 = shapes.get_shape(guardian_eyes_shp, 0);
			unique_ptr<Image_buffer> backup2(
					win->create_buffer(s2->get_width(), s2->get_height()));
			const unique_ptr<Image_buffer> cbackup2(
					win->create_buffer(s2->get_width(), s2->get_height()));
			win->get(
					cbackup2.get(), centerx - s2->get_xleft(),
					centery - Eyes_Dist - s2->get_yabove());
			sman->paint_shape(
					centerx, centery - Eyes_Dist,
					s2);    // frame 0 is background
			win->get(
					backup2.get(), centerx - s2->get_xleft(),
					centery - Eyes_Dist - s2->get_yabove());
			// forehead
			Shape_frame* s3 = shapes.get_shape(guardian_forehead_shp, 0);
			const unique_ptr<Image_buffer> cbackup3(
					win->create_buffer(s3->get_width(), s3->get_height()));
			win->get(
					cbackup3.get(), centerx - s3->get_xleft(),
					centery - Forehead_Dist - s3->get_yabove());
			sman->paint_shape(
					centerx, centery - Forehead_Dist,
					s3);    // forehead isn't animated

			// prepare Guardian speech
			{
				std::shared_ptr<Font> font
						= fontManager.get_font("GUARDIAN_FONT");
				const U7multiobject textobj(MAINSHP_FLX, PATCH_MAINSHP, 0x0D);
				size_t              txt_len;
				auto                txt = textobj.retrieve(
                        txt_len,
                        true);    // Must null terminate
				char* txt_ptr;
				char* txt_end;
				char* next_txt;
				next_txt = txt_ptr = reinterpret_cast<char*>(txt.get());

				const int txt_height = font->get_text_height();
				int       txt_ypos   = gwin->get_height() - txt_height - 16;

				// backup text area
				unique_ptr<Image_buffer> backup3(
						win->create_buffer(win->get_full_width(), txt_height));
				win->get(backup3.get(), win->get_start_x(), txt_ypos);

				// Lipsynching
				int            eye_frame      = 3;
				int            last_eye_frame = 3;
				int            mouth_frame    = 1;
				int            text_index     = -1;
				int            next_time;
				int            next_code;
				LipSynchReader lipsync;
				LipSynchReader surfacing(
						surfacing_data, sizeof(surfacing_data));

				int           time  = 0;
				unsigned long start = SDL_GetTicks();

				auto       audio     = Audio::get_ptr();
				const bool speech    = audio && audio->is_speech_enabled();
				const bool subtitles = !speech || audio->is_speech_with_subs();

				auto AdvanceTextPointer = [&]() {
					txt_ptr = next_txt;
					txt_end = strchr(txt_ptr, '\r');
					if (txt_end) {
						*txt_end = '\0';
						next_txt = txt_end + 2;
					} else {
						// can't find next, so disable subtitles
						text_index = text_num_frames;
					}
				};
				auto EraseAndDraw = [&](Image_buffer* backup, Shape_frame* fra,
										int shnum, int frnum, int dist) {
					win->put(
							backup, centerx - fra->get_xleft(),
							centery - dist - fra->get_yabove());
					sman->paint_shape(
							centerx, centery - dist,
							shapes.get_shape(shnum, frnum));
				};
				auto DrawSpeech = [&]() {
					// Erase text
					win->put(backup3.get(), win->get_start_x(), txt_ypos);
					// Erase and redraw eyes
					EraseAndDraw(
							backup2.get(), s2, guardian_eyes_shp, eye_frame,
							Eyes_Dist);
					// Erase and redraw mouth
					EraseAndDraw(
							backup.get(), s, guardian_mouth_shp, mouth_frame,
							0);
					if (text_index > 0
						&& static_cast<unsigned>(text_index)
								   < text_num_frames) {
						// Draw text
						font->center_text(
								win->get_ib8(), centerx, txt_ypos, txt_ptr);
					}
				};

				// First event needs to be read here
				surfacing.get_event(next_time, next_code);
				DrawSpeech();
				// before speech
				while (time < 2000) {
					if (surfacing.have_events() && time >= next_time) {
						surfacing.translate_code(
								next_code, mouth_frame, eye_frame,
								last_eye_frame);
						surfacing.get_event(next_time, next_code);
						DrawSpeech();
					}

					win->ShowFillGuardBand();
					WAITDELAYCYCLE5(15);
					win->ShowFillGuardBand();
					time = (SDL_GetTicks() - start);
				}

				const SpeechManager mngr(INTROSND, PATCH_INTROSND, false);
				time  = 0;
				start = SDL_GetTicks();

				// First event needs to be read here
				lipsync.get_event(next_time, next_code);
				if (subtitles) {
					text_index = 0;
				} else {
					text_index = text_num_frames;    // Disable subtitles
				}
				// start speech
				// Loop till speech has stopped or there is nolipsync data
				// previously was time < 47537 but this caused long translations
				// of speech to cut early
				while ((speech && audio->is_speech_playing())
					   || (!speech && lipsync.have_events())) {
					bool need_redraw = false;
					if (next_code && lipsync.have_events()
						&& time >= next_time) {
						lipsync.translate_code(
								next_code, mouth_frame, eye_frame,
								last_eye_frame);
						lipsync.get_event(next_time, next_code);
						need_redraw = true;
					}

					if (static_cast<unsigned>(text_index) < text_num_frames
						&& time >= text_times[text_index]) {
						text_index++;
						AdvanceTextPointer();
						need_redraw = true;
					}
					if (need_redraw) {
						DrawSpeech();
					}
					win->ShowFillGuardBand();
					WAITDELAYCYCLE6(15);
					win->ShowFillGuardBand();
					time = (SDL_GetTicks() - start);
				}

				win->ShowFillGuardBand();
				WAITDELAYCYCLE6(1000);
				win->ShowFillGuardBand();

				win->put(backup3.get(), 0, txt_ypos);
				win->put(
						cbackup.get(), centerx - s->get_xleft(),
						centery - s->get_yabove());
				win->put(
						cbackup2.get(), centerx - s2->get_xleft(),
						centery - Eyes_Dist - s2->get_yabove());
				win->put(
						cbackup3.get(), centerx - s3->get_xleft(),
						centery - Forehead_Dist - s3->get_yabove());
			}

			// G. disappears again (sp. 0x23 again)
			Audio::get_ptr()->play_sound_effect(
					sfxfile, INTROSFX_MT32_FLX_INTRO_MT_GUARDIAN4_WAV);

			{
				Shape_frame*                   s = shapes.get_shape(0x23, 0);
				const unique_ptr<Image_buffer> backup(
						win->create_buffer(s->get_width(), s->get_height()));
				const unique_ptr<Image_buffer> cbackup(
						win->create_buffer(s->get_width(), s->get_height()));
				win->get(
						cbackup.get(), centerx - s->get_xleft(),
						centery - s->get_yabove());
				sman->paint_shape(
						centerx, centery, s);    // frame 0 is background
				win->get(
						backup.get(), centerx - s->get_xleft(),
						centery - s->get_yabove());
				for (int i = 15; i > 0; i--) {
					FLASH_SHAPE2(centerx, centery, 0x23, i, 70);
				}
				win->put(
						cbackup.get(), centerx - s->get_xleft(),
						centery - s->get_yabove());
			}

			win->ShowFillGuardBand();
		}
		WAITDELAYCYCLE1(1200);

		Audio::get_ptr()->stop_music();

		//
		// More static
		//
		Audio::get_ptr()->play_sound_effect(
				sfxfile, INTROSFX_MT32_FLX_INTRO_MT_OUTSTATIC_WAV);

		ticks = SDL_GetTicks();
		while (true) {
			win->get_ibuf()->fill_static(0, 7, 15);
			win->ShowFillGuardBand();
			WAITDELAYCYCLE1(2);
			if (SDL_GetTicks() > ticks + 400) {
				break;
			}
		}

		gwin->clear_screen(true);

		//
		// White dot
		//
		Audio::get_ptr()->play_sound_effect(
				sfxfile, INTROSFX_MT32_FLX_INTRO_MT_OUTNOISE_WAV);

		Shape_frame*                   s = shapes.get_shape(0x14, 0);
		const unique_ptr<Image_buffer> backup(
				win->create_buffer(s->get_width() + 2, s->get_height() + 2));
		win->get(backup.get(), centerx - 1, centery - 1);

		ticks = SDL_GetTicks();
		while (true) {
			const int x = centerx + rand() % 3 - 1;
			const int y = centery + rand() % 3 - 1;
			FLASH_SHAPE1(x, y, 0x14, 0, 0);
			WAITDELAYCYCLE1(2);
			if (SDL_GetTicks() - ticks > 800) {
				break;
			}
		}
	} catch (const UserSkipException& /*x*/) {
	}
}

namespace {    // anonymous

	enum HandlerScriptOps {
		eNOP,             // Does nothing except redraw static if needed
		eHAND_HIT,        // Move hand to frame 3, and redraw static if needed
		eHAND_RECOIL,     // Decrement hand frame, and redraw static if needed
		eBLACK_SCREEN,    // Draw black screen
		eSHOW_STATIC,     // Draw static
		eFLASH_FAKE_TITLE,    // Draw fake title screen for 1 frame, then revert
							  // to black screen
		eSHOW_FAKE_TITLE,     // Draw fake title screen permanently
	};

	constexpr const std::array HandlerScript{
			eHAND_HIT,         eBLACK_SCREEN, eSHOW_FAKE_TITLE,  eSHOW_STATIC,
			eHAND_RECOIL,      eNOP,          eHAND_RECOIL,      eNOP,
			eHAND_RECOIL,      eBLACK_SCREEN, eHAND_HIT,         eNOP,
			eSHOW_FAKE_TITLE,  eSHOW_STATIC,  eHAND_RECOIL,      eNOP,
			eHAND_RECOIL,      eNOP,          eHAND_RECOIL,      eBLACK_SCREEN,
			eHAND_HIT,         eNOP,          eFLASH_FAKE_TITLE, eHAND_RECOIL,
			eFLASH_FAKE_TITLE, eHAND_RECOIL,  eFLASH_FAKE_TITLE, eHAND_RECOIL,
			eSHOW_FAKE_TITLE};

	class Hand_Handler {
	private:
		Image_window8*                win;
		Shape_manager*                sman;
		std::unique_ptr<Shape_file>   handshp;
		std::unique_ptr<Image_buffer> handBackup;
		std::unique_ptr<Image_buffer> staticScreen;
		Shape_frame*                  screenShape;
		Shape_frame*                  handFrame;
		size_t                        scriptPosition;
		int                           centerx, centery;
		int                           handFrNum;
		HandlerScriptOps              currBackground;
		bool                          playedStaticSFX;

	public:
		Hand_Handler(
				BG_Game* game, Vga_file& shapes, Image_window8* _win,
				Shape_manager* _sman, int _centerx, int _centery)
				: win(_win), sman(_sman),
				  staticScreen(win->create_buffer(160, 99)),
				  screenShape(shapes.get_shape(0x1D, 0)), handFrame(nullptr),
				  scriptPosition(0), centerx(_centerx), centery(_centery),
				  handFrNum(-1), currBackground(eBLACK_SCREEN),
				  playedStaticSFX(false) {
			const str_int_pair& resource
					= game->get_resource("files/intro_hand");
			const U7object  shpobj(resource.str, resource.num);
			std::size_t     len;
			auto            handBuffer = shpobj.retrieve(len);
			IBufferDataView ds(handBuffer, len);
			handshp = std::make_unique<Shape_file>(&ds);
		}

		// Returns true to keep going.
		bool draw_frame();

		void backup_shape_bkgnd(
				Shape_frame* fra, std::unique_ptr<Image_buffer>& backup) {
			backup = win->create_buffer(fra->get_width(), fra->get_height());

			win->get(
					backup.get(), centerx - 156 - fra->get_xleft(),
					centery + 78 - fra->get_yabove());
		}

		void restore_shape_bkgnd(
				Shape_frame* fra, std::unique_ptr<Image_buffer>& backup) {
			win->put(
					backup.get(), centerx - 156 - fra->get_xleft(),
					centery + 78 - fra->get_yabove());
			backup.reset();
		}
	};

	bool Hand_Handler::draw_frame() {
		if (scriptPosition >= HandlerScript.size()) {
			return false;
		}
		const HandlerScriptOps currOp   = HandlerScript[scriptPosition];
		bool                   drawHand = false;
		// Do hand first
		switch (currOp) {
		case eHAND_HIT:
			// TODO: SFX for hand hitting monitor
			drawHand  = true;
			handFrNum = 3;
			break;
		case eHAND_RECOIL:
			if (handFrNum > 0) {
				handFrNum--;
				drawHand = true;
			}
			break;
		default:
			break;
		}
		if (drawHand) {
			if (handFrame && handBackup) {
				restore_shape_bkgnd(handFrame, handBackup);
			}
			handFrame = handshp->get_frame(handFrNum);
			if (handFrame) {
				backup_shape_bkgnd(handFrame, handBackup);
				sman->paint_shape(centerx - 167, centery + 78, handFrame);
			}
		}
		// Lets now handle backgrounds.
		switch (currOp) {
		case eHAND_HIT:
		case eHAND_RECOIL:
			// Special case for hand opcodes: redraw static
			// if it is the current background.
			if (currBackground != eSHOW_STATIC) {
				break;
			}
			[[fallthrough]];
		case eSHOW_STATIC:
		case eNOP:
			if (currOp == eSHOW_STATIC) {
				currBackground = currOp;
				if (!playedStaticSFX) {
					playedStaticSFX = true;
					// play static SFX
					const File_spec sfxfile = BG_Game::get_sfx_subflex();
					Audio::get_ptr()->play_sound_effect(
							sfxfile,
							INTROSFX_MT32_FLX_INTRO_MT_MONITORSLAP_WAV);
				}
			}
			staticScreen->fill_static(0, 7, 15);
			win->put(
					staticScreen.get(),
					centerx + 12 - screenShape->get_width() / 2,
					centery - 22 - screenShape->get_height() / 2);
			win->ShowFillGuardBand();
			drawHand = false;
			break;
		case eFLASH_FAKE_TITLE:
		case eSHOW_FAKE_TITLE:
			sman->paint_shape(centerx + 12, centery - 22, screenShape);
			win->ShowFillGuardBand();
			drawHand = false;
			if (currOp == eSHOW_FAKE_TITLE) {
				currBackground = currOp;
				break;
			}
			[[fallthrough]];
		case eBLACK_SCREEN:
			win->fill8(
					0, screenShape->get_width(), screenShape->get_height(),
					centerx + 12 - screenShape->get_width() / 2,
					centery - 22 - screenShape->get_height() / 2);
			if (currOp == eBLACK_SCREEN) {
				currBackground = currOp;
				win->ShowFillGuardBand();
				drawHand = false;
			}
			break;
		default:    // Just in case
			return false;
		}

		if (drawHand) {
			win->ShowFillGuardBand();
		}

		scriptPosition++;
		return scriptPosition < HandlerScript.size();
	}
}    // End of anonymous namespace

void BG_Game::scene_desk() {
	try {
		Audio::get_ptr()->start_music(
				home_song_midi, false, MyMidiPlayer::Force_None, INTROMUS);

		gwin->clear_screen();
		// Clip it to 320x200 region
		const WinClip clip(win, centerx - 160, centery - 100, 320, 200);

		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 1);
		pal->apply();

		// draw monitor (sh. 0x07, 0x08, 0x09, 0x0A: various parts of monitor)
		sman->paint_shape(centerx, centery, shapes.get_shape(0x07, 0));
		sman->paint_shape(centerx, centery, shapes.get_shape(0x09, 0));
		sman->paint_shape(centerx, centery, shapes.get_shape(0x08, 0));
		sman->paint_shape(centerx, centery, shapes.get_shape(0x0A, 0));

		// Zoom out from zoomed in screen
		{
			unique_ptr<Image_buffer> unzoomed(win->create_buffer(320, 200));
			win->get(
					unzoomed.get(), 0 + (win->get_game_width() - 320) / 2,
					0 + (win->get_game_height() - 200) / 2);
			const unique_ptr<Image_buffer> zoomed(win->create_buffer(320, 200));

			const Image_window::ScalerInfo& scaler
					= Image_window::Scalers[Image_window::point];

			SDL_Surface*     draw_surface = win->get_draw_surface();
			SDL_SurfaceOwner unzoomed_surf(unzoomed.get(), draw_surface);
			SDL_SurfaceOwner zoomed_surf(zoomed.get(), draw_surface);

			Shape_frame*             dotshp = shapes.get_shape(0x14, 0);
			unique_ptr<Image_buffer> dot(win->create_buffer(
					dotshp->get_width(), dotshp->get_height()));
			{
				const unique_ptr<Image_buffer> backup(win->create_buffer(
						dot->get_width(), dot->get_height()));
				win->get(backup.get(), centerx + 12, centery - 22);
				sman->paint_shape(centerx + 12, centery - 22, dotshp);
				win->get(dot.get(), centerx + 12, centery - 22);
				win->put(backup.get(), centerx + 12, centery - 22);
			}
			const unique_ptr<Image_buffer> backup(win->create_buffer(
					dot->get_width() + 2, dot->get_height() + 2));
			unzoomed->get(backup.get(), centerx + 12, centery - 22);

			const int zx = 88;
			const int zy = 22;
			const int zw = 166;
			const int zh = 112;

			uint32 next_ticks = SDL_GetTicks() + 10;
			for (int i = 0; i < 40; i++) {
				const int sw = zw + (320 - zw) * i / 40;
				const int sh = zh + (200 - zh) * i / 40;
				const int sx = zx + (0 - zx) * i / 40;
				const int sy = zy + (0 - zy) * i / 40;

				// frame drop?
				if (next_ticks > SDL_GetTicks()) {
					unzoomed->put(backup.get(), centerx + 12, centery - 22);
					unzoomed->put(
							dot.get(), centerx + rand() % 3 - 1 + 12,
							centery + rand() % 3 - 1 - 22);
					scaler.arb->Scale(
							unzoomed_surf.get(), sx, sy, sw, sh,
							zoomed_surf.get(), 0, 0, 320, 200, true);
					win->put(
							zoomed.get(), 0 + (win->get_game_width() - 320) / 2,
							0 + (win->get_game_height() - 200) / 2);
					win->ShowFillGuardBand();
					int delta = next_ticks - SDL_GetTicks();
					if (delta < 0) {
						delta = 0;
					}
					WAITDELAY(delta);
				}
				next_ticks += 10;
			}

			win->put(
					unzoomed.get(), 0 + (win->get_game_width() - 320) / 2,
					0 + (win->get_game_height() - 200) / 2);
			win->ShowFillGuardBand();
		}

		{
			// draw arm hitting pc
			Hand_Handler hand(this, shapes, win, sman, centerx, centery);
			while (hand.draw_frame()) {
				WAITDELAY(60);
			}

			WAITDELAY(1300);
		}

		// "Something is obviously amiss"
		sman->paint_shape(centerx, centery + 50, shapes.get_shape(0x15, 0));
		win->ShowFillGuardBand();
		WAITDELAY(3000);

		// TODO: misaligned?

		// scroll right (sh. 0x06: map to the right of the monitor)
		for (int i = 0; i <= 194; i += 2) {    // was += 4
			sman->paint_shape(centerx - i, centery, shapes.get_shape(0x07, 0));
			sman->paint_shape(centerx - i, centery, shapes.get_shape(0x09, 0));
			sman->paint_shape(centerx - i, centery, shapes.get_shape(0x08, 0));
			sman->paint_shape(centerx - i, centery, shapes.get_shape(0x0A, 0));
			sman->paint_shape(
					centerx - i + 12, centery - 22, shapes.get_shape(0x1D, 0));
			sman->paint_shape(topx + 320 - i, topy, shapes.get_shape(0x06, 0));

			if (i > 75 && i < 194) {
				// "It has been a long time..."
				sman->paint_shape(
						centerx, centery + 50, shapes.get_shape(0x16, 0));
			}
			win->ShowFillGuardBand();
			WAITDELAY(110);    // was 30
		}

		WAITDELAY(1500);
		// scroll down (sh. 0x0B: mouse + orb of moons, below map)
		for (int i = 0; i <= 50; i += 2) {
			sman->paint_shape(
					centerx - 194, centery - i, shapes.get_shape(0x07, 0));
			sman->paint_shape(
					centerx - 194, centery - i, shapes.get_shape(0x09, 0));
			sman->paint_shape(
					centerx - 194, centery - i, shapes.get_shape(0x08, 0));
			sman->paint_shape(
					centerx - 194, centery - i, shapes.get_shape(0x0A, 0));
			sman->paint_shape(
					centerx - 194 + 12, centery - 22 - i,
					shapes.get_shape(0x1D, 0));
			sman->paint_shape(
					topx + 319 - 194, topy - i, shapes.get_shape(0x06, 0));
			sman->paint_shape(
					topx + 319, topy + 199 - i, shapes.get_shape(0x0B, 0));
			// "The mystical Orb beckons you"
			sman->paint_shape(centerx, topy, shapes.get_shape(0x17, 0));
			win->ShowFillGuardBand();
			WAITDELAYCYCLE2(110);
		}

		sman->paint_shape(
				centerx - 194, centery - 50, shapes.get_shape(0x07, 0));
		sman->paint_shape(
				centerx - 194, centery - 50, shapes.get_shape(0x09, 0));
		sman->paint_shape(
				centerx - 194, centery - 50, shapes.get_shape(0x08, 0));
		sman->paint_shape(
				centerx - 194, centery - 50, shapes.get_shape(0x0A, 0));
		sman->paint_shape(
				centerx - 182, centery - 72, shapes.get_shape(0x1D, 0));
		sman->paint_shape(
				topx + 319 - 194, topy - 50, shapes.get_shape(0x06, 0));
		sman->paint_shape(topx + 319, topy + 149, shapes.get_shape(0x0B, 0));
		// "It has opened gateways to Britannia in the past"
		sman->paint_shape(centerx, topy, shapes.get_shape(0x18, 0));
		win->ShowFillGuardBand();

		WAITDELAYCYCLE2(3200);
		pal->fade_out(100);
		gwin->clear_screen(true);
	} catch (const UserSkipException& /*x*/) {
	}
}

void BG_Game::scene_moongate() {
	// sh. 0x02, 0x03, 0x04, 0x05: various parts of moongate
	// sh. 0x00, 0x01: parting trees before clearing
	gwin->clear_screen(false);
	pal->load(INTROPAL_DAT, PATCH_INTROPAL, 5);
	pal->apply();

	// "Behind your house is the circle of stones"
	sman->paint_shape(centerx, centery + 50, shapes.get_shape(0x19, 0));
	pal->fade_in(c_fade_in_time);

	const File_spec sfxfile = get_sfx_subflex();
	Audio::get_ptr()->play_sound_effect(
			sfxfile, INTROSFX_MT32_FLX_INTRO_MT_MOONGATE_WAV);

	// TODO: fade in screen while text is onscreen

	WAITDELAY(4000);

	// Bushes move out of the way
	for (int i = 50; i >= -170; i -= 2) {    // was for(i=120;i>=-170;i-=6)
		sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x02, 0));
		sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x03, 0));
		sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x04, 0));
		sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x05, 0));

		// TODO: if you watch the original closely, the bushes are scaled up in
		// size slightly as they move out.
		sman->paint_shape(
				centerx + i, topy - ((i - 60) / 4), shapes.get_shape(0x00, 0));
		sman->paint_shape(
				centerx - i, topy - ((i - 60) / 4), shapes.get_shape(0x01, 0));

		if ((40 > i) && (i > -100)) {
			// "Why is a moongate already there?"
			sman->paint_shape(centerx, centery + 50, shapes.get_shape(0x1A, 0));
		} else if (i <= -100) {
			// "You have but one path to the answer"
			sman->paint_shape(centerx, centery + 50, shapes.get_shape(0x1C, 0));
		}

		win->ShowFillGuardBand();
		WAITDELAYCYCLE3(80);
	}

	// Wait till the music finished playing
	while (Audio::get_ptr()->is_track_playing(home_song_midi)) {
		WAITDELAYCYCLE3(50);
	}

	// zoom (run) into moongate
	sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x02, 0));
	sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x03, 0));
	sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x04, 0));
	sman->paint_shape(centerx + 1, centery + 1, shapes.get_shape(0x05, 0));

	const unique_ptr<Image_buffer> unzoomed(win->create_buffer(320, 200));
	win->get(
			unzoomed.get(), 0 + (win->get_game_width() - 320) / 2,
			0 + (win->get_game_height() - 200) / 2);
	const unique_ptr<Image_buffer> zoomed(win->create_buffer(320, 200));

	const Image_window::ScalerInfo& scaler
			= Image_window::Scalers[Image_window::point];

	SDL_Surface* draw_surface = win->get_draw_surface();

	SDL_SurfaceOwner unzoomed_surf(unzoomed.get(), draw_surface);
	SDL_SurfaceOwner zoomed_surf(zoomed.get(), draw_surface);

	const int zx = 151;
	const int zy = 81;
	const int zw = 5;
	const int zh = 4;

	Audio::get_ptr()->play_sound_effect(
			sfxfile, INTROSFX_MT32_FLX_INTRO_MT_SHOT_WAV);

	uint32 next_ticks = SDL_GetTicks() + 10;
	for (int i = 159; i >= 0; i--) {
		const int sw = zw + (320 - zw) * i / 160;
		const int sh = zh + (200 - zh) * i / 160;
		const int sx = zx + (0 - zx) * i / 160;
		const int sy = zy + (0 - zy) * i / 160;

		// frame drop?
		if (next_ticks > SDL_GetTicks()) {
			scaler.arb->Scale(
					unzoomed_surf.get(), sx, sy, sw, sh, zoomed_surf.get(), 0,
					0, 320, 200, true);
			win->put(
					zoomed.get(), 0 + (win->get_game_width() - 320) / 2,
					0 + (win->get_game_height() - 200) / 2);
			win->ShowFillGuardBand();
			int delta = next_ticks - SDL_GetTicks();
			if (delta < 0) {
				delta = 0;
			}
			WAITDELAYCYCLE3(delta);
		}
		next_ticks += 5;
	}
}

Shape_frame* BG_Game::get_menu_shape() {
	return menushapes.get_shape(0x2, 0);
}

void BG_Game::top_menu() {
	Audio::get_ptr()->start_music(
			menu_midi, true, MyMidiPlayer::Force_None, INTROMUS);
	sman->paint_shape(topx, topy, get_menu_shape());
	pal->load(INTROPAL_DAT, PATCH_INTROPAL, 6);
	pal->fade_in(60);
}

void BG_Game::show_journey_failed() {
	pal->fade_out(50);
	gwin->clear_screen(true);
	sman->paint_shape(topx, topy, get_menu_shape());
	journey_failed_text();
}

class ExVoiceBuffer {
private:
	const char* file;
	const char* patch;
	int         index;
	bool        played = false;

public:
	bool play_it();

	ExVoiceBuffer(const char* f, const char* p, int i)
			: file(f), patch(p), index(i) {}

	bool can_play() const {
		return file || patch;
	}
};

bool ExVoiceBuffer::play_it() {
	size_t              size;
	const U7multiobject voc(file, patch, index);
	auto                buffer = voc.retrieve(size);
	uint8*              buf    = buffer.get();
	if (!memcmp(buf, "voc", sizeof("voc") - 1)) {
		// IFF junk.
		buf += 8;
		size -= 8;
	}
	Audio::get_ptr()->copy_and_play_speech(buf, size, false);
	played = true;

	return false;
}

std::vector<unsigned int> BG_Game::get_congratulations_messages() {
	return {congrats + 0, congrats + 1, congrats + 2,
			congrats + 3, congrats + 4, congrats + 5,
			congrats + 6, congrats + 7, congrats + 8};
}

void BG_Game::end_game(bool success, bool within_game) {
	waitforspeech();
	Audio* audio = Audio::get_ptr();
	if (scene_available("endgame")) {
		audio->stop_music();
		// Play the custom endgame scene
		play_scene("endgame");
		return;
	}
	std::shared_ptr<Font> font = fontManager.get_font("MENU_FONT");

	if (!success) {
		TextScroller text(MAINSHP_FLX, 0x15, font, nullptr);
		gwin->clear_screen();
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 0);
		for (sint32 i = 0; i < text.get_count(); i++) {
			text.show_line(gwin, topx, topx + 320, topy + 20 + i * 12, i);
		}

		pal->fade_in(c_fade_in_time);
		wait_delay(10000);
		pal->fade_out(c_fade_out_time);

		gwin->clear_screen();
		font->center_text(
				ibuf, centerx, centery - 10, get_text_msg(end_of_ultima7));
		pal->fade_in(c_fade_in_time);
		wait_delay(4000);
		pal->fade_out(c_fade_out_time);

		gwin->clear_screen();
		font->center_text(
				ibuf, centerx, centery - 10, get_text_msg(end_of_britannia));
		pal->fade_in(c_fade_in_time);
		wait_delay(4000);
		pal->fade_out(c_fade_out_time);
		gwin->clear_screen(true);
		return;
	}

	audio->stop_music();
	MyMidiPlayer* midi = audio->get_midi();
	if (midi) {
		midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_ENDGAME);
	}

	const bool speech = Audio::get_ptr()->is_audio_enabled()
						&& Audio::get_ptr()->is_speech_enabled();
	const bool subtitles = !speech || Audio::get_ptr()->is_speech_with_subs();

	// Clear screen
	gwin->clear_screen(true);

	ExVoiceBuffer speech1(ENDGAME, PATCH_ENDGAME, 7);
	ExVoiceBuffer speech2(ENDGAME, PATCH_ENDGAME, 8);
	ExVoiceBuffer speech3(ENDGAME, PATCH_ENDGAME, 9);

	/* There seems to be something wrong with the shapes. Needs investigating
	U7multiobject shapes(ENDGAME, PATCH_ENDGAME, 10);
	shapes.retrieve("endgame.shp");
	Shape_file sf("endgame.shp");
	int x = get_width()/2-160;
	int y = get_height()/2-100;
	cout << "Shape in Endgame.dat has " << sf.get_num_frames() << endl;
	*/

	// Fli Buffers
	playfli fli1(ENDGAME, PATCH_ENDGAME, 0);
	playfli fli2(ENDGAME, PATCH_ENDGAME, 1);
	playfli fli3(ENDGAME, PATCH_ENDGAME, 2);

	fli1.play(win, 0, 0, 0);

	// Start endgame music.
	Audio::get_ptr()->start_music(ENDSCORE_XMI, 1, false);

	try {
		unsigned int next = 0;
		for (unsigned int i = 0; i < 240; i++) {
			next = fli1.play(win, 0, 1, next);
			if (wait_delay(0)) {
				throw UserSkipException();
			}
		}

		for (unsigned int i = 1; i < 150; i++) {
			next = fli1.play(win, i, i + 1, next);
			if (wait_delay(0)) {
				throw UserSkipException();
			}
		}

		if (speech) {
			speech1.play_it();
		}
		std::shared_ptr<Font> endfont2 = fontManager.get_font("END2_FONT");
		std::shared_ptr<Font> endfont3 = fontManager.get_font("END3_FONT");
		std::shared_ptr<Font> normal   = fontManager.get_font("NORMAL_FONT");

		{
			const char* message = get_text_msg(you_cannot_do_that);
			const int   height  = topy + 200 - endfont2->get_text_height() * 2;
			const int   width
					= (gwin->get_width() - endfont2->get_text_width(message))
					  / 2;

			for (unsigned int i = 150; i < 204; i++) {
				next = fli1.play(win, i, i, next);
				if (subtitles) {
					endfont2->draw_text(ibuf, width, height, message);
				}
				win->ShowFillGuardBand();
				if (wait_delay(0, 0, 1)) {
					throw UserSkipException();
				}
			}
		}

		// Set new music
		Audio::get_ptr()->start_music(ENDSCORE_XMI, 2, false);

		// Set speech

		if (speech) {
			speech2.play_it();
		}

		{
			const char* message = get_text_msg(damn_avatar);
			const int   height  = topy + 200 - endfont2->get_text_height() * 2;
			const int   width
					= (gwin->get_width() - endfont2->get_text_width(message))
					  / 2;

			for (unsigned int i = 0; i < 100; i++) {
				next = fli2.play(win, i, i, next);
				if (subtitles) {
					endfont2->draw_text(ibuf, width, height, message);
				}
				win->ShowFillGuardBand();
				if (wait_delay(0, 0, 1)) {
					throw UserSkipException();
				}
			}
		}

		Palette* pal = fli2.get_palette();
		next         = SDL_GetTicks();
		for (const unsigned int i = 1000 + next; next < i; next += 10) {
			// Speed related frame skipping detection
			const bool skip_frame
					= Game_window::get_instance()->get_frame_skipping()
					  && SDL_GetTicks() >= next;
			while (SDL_GetTicks() < next)
				;
			if (!skip_frame) {
				pal->set_brightness((i - next) / 10);
				pal->apply();
			}
			if (wait_delay(0, 0, 1)) {
				throw UserSkipException();
			}
		}

		pal->set_brightness(80);    // Set readable brightness
		// Text message 1

		// Paint backgound black
		win->fill8(0);

		// Paint text
		{
			const char* message = get_text_msg(blackgate_destroyed);
			const int   height
					= (gwin->get_height() - normal->get_text_height()) / 2;
			const int width
					= (gwin->get_width() - normal->get_text_width(message)) / 2;

			normal->draw_text(ibuf, width, height, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for 3 seconds
		for (unsigned int i = 0; i < 30; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade(50, 0, 0);

		// Now the second text message

		// Paint backgound black
		win->fill8(0);

		// Paint text
		{
			const char* message = get_text_msg(guardian_has_stopped);
			const int   height
					= (gwin->get_height() - normal->get_text_height()) / 2;
			const int width
					= (gwin->get_width() - normal->get_text_width(message)) / 2;
			normal->draw_text(ibuf, width, height, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for approx 3 seonds
		for (unsigned int i = 0; i < 30; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade(50, 0, 0);

		fli3.play(win, 0, 0, next);
		pal  = fli3.get_palette();
		next = SDL_GetTicks();
		for (const unsigned int i = 1000 + next; next < i; next += 10) {
			// Speed related frame skipping detection
			const bool skip_frame
					= Game_window::get_instance()->get_frame_skipping()
					  && SDL_GetTicks() >= next;
			while (SDL_GetTicks() < next)
				;
			if (!skip_frame) {
				pal->set_brightness(100 - (i - next) / 10);
				pal->apply();
			}
			if (wait_delay(0, 0, 1)) {
				throw UserSkipException();
			}
		}

		if (speech) {
			speech3.play_it();
		}

		playfli::fliinfo finfo;
		fli3.info(&finfo);

		int m;
		int starty = (gwin->get_height() - endfont3->get_text_height() * 8) / 2;

		next = SDL_GetTicks();
		for (const unsigned int i = next + 28000; i > next;) {
			for (unsigned int j = 0; j < static_cast<unsigned>(finfo.frames);
				 j++) {
				next = fli3.play(win, j, j, next);
				if (subtitles) {
					for (m = 0; m < 8; m++) {
						endfont3->center_text(
								ibuf, centerx,
								starty + endfont3->get_text_height() * m,
								get_text_msg(txt_screen0 + m));
					}
				}
				win->ShowFillGuardBand();
				if (wait_delay(10, 0, 1)) {
					throw UserSkipException();
				}
			}
		}

		next = SDL_GetTicks();
		for (const unsigned int i = 1000 + next; next < i; next += 10) {
			// Speed related frame skipping detection
			const bool skip_frame
					= Game_window::get_instance()->get_frame_skipping()
					  && SDL_GetTicks() >= next;
			while (SDL_GetTicks() < next)
				;
			if (!skip_frame) {
				pal->set_brightness((i - next) / 10);
				pal->apply();
			}
			if (wait_delay(0, 0, 1)) {
				throw UserSkipException();
			}
		}

		// Text Screen 1
		pal->set_brightness(80);    // Set readable brightness

		// Paint backgound black
		win->fill8(0);

		// Because of the German version we have to fit 11 lines of height 20
		// into a screen of 200 pixels so starty has needs to be a tiny bit in
		// the negative but not -10
		starty = (gwin->get_height() - normal->get_text_height() * 11) / 2.5;

		for (unsigned int i = 0; i < 11; i++) {
			const char* message = get_text_msg(txt_screen1 + i);
			normal->draw_text(
					ibuf, centerx - normal->get_text_width(message) / 2,
					starty + normal->get_text_height() * i, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for 20 seconds (only 10 at the moment)
		for (unsigned int i = 0; i < 100; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade(50, 0, 0);

		if (wait_delay(10)) {
			throw UserSkipException();
		}

		// Text Screen 2

		// Paint backgound black
		win->fill8(0);

		starty = (gwin->get_height() - normal->get_text_height() * 9) / 2;

		for (unsigned int i = 0; i < 9; i++) {
			const char* message = get_text_msg(txt_screen2 + i);
			normal->draw_text(
					ibuf, centerx - normal->get_text_width(message) / 2,
					starty + normal->get_text_height() * i, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for 20 seonds (only 8 at the moment)
		for (unsigned int i = 0; i < 80; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade(50, 0, 0);

		if (wait_delay(10)) {
			throw UserSkipException();
		}

		// Text Screen 3

		// Paint backgound black
		win->fill8(0);

		starty = (gwin->get_height() - normal->get_text_height() * 8) / 2;

		for (unsigned int i = 0; i < 8; i++) {
			const char* message = get_text_msg(txt_screen3 + i);
			normal->draw_text(
					ibuf, centerx - normal->get_text_width(message) / 2,
					starty + normal->get_text_height() * i, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for 20 seonds (only 8 at the moment)
		for (unsigned int i = 0; i < 80; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade(50, 0, 0);

		if (wait_delay(10)) {
			throw UserSkipException();
		}

		// Text Screen 4

		// Paint backgound black
		win->fill8(0);

		starty = (gwin->get_height() - normal->get_text_height() * 5) / 2;

		for (unsigned int i = 0; i < 5; i++) {
			const char* message = get_text_msg(txt_screen4 + i);
			normal->draw_text(
					ibuf, centerx - normal->get_text_width(message) / 2,
					starty + normal->get_text_height() * i, message);
		}

		// Fade in for 1 sec (50 cycles)
		pal->fade(50, 1, 0);

		// Display text for 10 seonds (only 5 at the moment)
		for (unsigned int i = 0; i < 50; i++) {
			if (wait_delay(100)) {
				throw UserSkipException();
			}
		}

		// Fade out for 1 sec (50 cycles)
		pal->fade_out(50);

		// Congratulations screen
		// show only when finishing a game and not when viewed from menu
		if (within_game) {
			show_congratulations(pal);
		}
	} catch (const UserSkipException& /*x*/) {
		pal->set_brightness(80);    // Set readable brightness
		win->fill8(0);
		win->ShowFillGuardBand();
	}

	if (midi) {
		midi->stop_music();
		midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GAME);
	}

	audio->stop_music();

	gwin->clear_screen(true);
}

void BG_Game::show_quotes() {
	if (scene_available("quotes")) {
		Audio::get_ptr()->stop_music();
		// Play the custom quotes scene
		play_scene("quotes");
		return;
	}
	Audio::get_ptr()->start_music(
			quotes_midi, false, MyMidiPlayer::Force_None, INTROMUS);
	TextScroller quotes(
			MAINSHP_FLX, 0x10, fontManager.get_font("MENU_FONT"),
			menushapes.extract_shape(0x14));
	quotes.run(gwin);
}

void BG_Game::show_credits() {
	if (scene_available("credits")) {
		Audio::get_ptr()->stop_music();
		// Play the custom credits scene
		play_scene("credits");
		U7open_out("<SAVEGAME>/quotes.flg");
		return;
	}
	pal->load(INTROPAL_DAT, PATCH_INTROPAL, 6);
	Audio::get_ptr()->start_music(
			credits_midi, false, MyMidiPlayer::Force_None, INTROMUS);
	TextScroller credits(
			MAINSHP_FLX, 0x0E, fontManager.get_font("MENU_FONT"),
			menushapes.extract_shape(0x14));
	if (credits.run(gwin)) {    // Watched through the entire sequence?
		U7open_out("<SAVEGAME>/quotes.flg");
	}
}

bool BG_Game::new_game(Vga_file& shapes) {
	const int             menuy = topy + 110;
	std::shared_ptr<Font> font  = fontManager.get_font("MENU_FONT");

	Vga_file faces_vga;
	// Need to know if SI is installed
	const bool si_installed
			= (gamemanager->is_si_installed() || gamemanager->is_ss_installed())
			  && U7exists("<SERPENT_STATIC>/shapes.vga");

	// Multiracial faces.
	const str_int_pair& resource = get_resource("files/mrfacesvga");
	// List of files to load.
	std::vector<std::pair<std::string, int>> source{
			{   FACES_VGA,           -1},
			{resource.str, resource.num},
			{ PATCH_FACES,           -1},
	};
	faces_vga.load(source);

	const int max_name_len = 16;
	char      npc_name[max_name_len + 1];
	char      disp_name[max_name_len + 2];
	npc_name[0] = 0;

	int       selected    = 0;
	const int num_choices = 4;
	bool      editing     = true;
	bool      redraw      = true;
	bool      ok          = true;

	// Skin info
	Avatar_default_skin* defskin  = Shapeinfo_lookup::GetDefaultAvSkin();
	Skin_data*           skindata = Shapeinfo_lookup::GetSkinInfoSafe(
            defskin->default_skin, defskin->default_female, si_installed);

	Palette* pal = gwin->get_pal();
	// This should work because the palette in exult_bg.flx is
	// a single-file object.
	pal->load(
			File_spec(INTROPAL_DAT, 6),
			File_spec(
					get_resource("files/gameflx").str,
					EXULT_BG_FLX_U7MENUPAL_PAL),
			File_spec(PATCH_INTROPAL, 6), 0);

	// Create palette translation table. Maybe make them static?
	std::array<unsigned char, 256> transto;
	{
		Palette oldpal;
		oldpal.load(INTROPAL_DAT, PATCH_INTROPAL, 6);
		oldpal.create_palette_map(pal, transto.data());
	}
	gwin->clear_screen(true);
	pal->apply(true);
	if (Mouse::mouse()
		&& !Mouse::use_touch_input) {            // If not primarily touch input
		Mouse::mouse()->show(transto.data());    // Attempt to make the mouse
												 // visible initially
	}
	SDL_Window* window = gwin->get_win()->get_screen_window();
#if !defined(SDL_PLATFORM_IOS) && !defined(ANDROID)
	if (!SDL_TextInputActive(window)) {
		SDL_StartTextInput(window);
	}
#endif
	do {
		Delay();
		if (redraw) {
			if (Mouse::mouse()) {
				Mouse::mouse()->hide();
			}
			gwin->clear_screen();
			sman->paint_shape(
					topx, topy, shapes.get_shape(0x2, 0), false,
					transto.data());
			sman->paint_shape(
					topx + 10, menuy + 10, shapes.get_shape(0xC, selected == 0),
					false, transto.data());
			Shape_frame* sex_shape = shapes.get_shape(0xA, selected == 1);
			sman->paint_shape(
					topx + 10, menuy + 25, sex_shape, false, transto.data());
			int sex_width = sex_shape->get_width() + 10;
			if (sex_width > 35) {
				sex_width += 25;
			} else {
				sex_width = 60;
			}

			sman->paint_shape(
					topx + sex_width, menuy + 25,
					shapes.get_shape(0xB, skindata->is_female), false,
					transto.data());

			Shape_frame* portrait = faces_vga.get_shape(
					skindata->face_shape, skindata->face_frame);
			sman->paint_shape(topx + 290, menuy + 61, portrait);

			sman->paint_shape(
					topx + 10, topy + 180, shapes.get_shape(0x8, selected == 2),
					false, transto.data());
			sman->paint_shape(
					centerx + 10, topy + 180,
					shapes.get_shape(0x7, selected == 3), false,
					transto.data());
			if (selected == 0) {
				snprintf(disp_name, max_name_len + 2, "%s_", npc_name);
			} else {
				snprintf(disp_name, max_name_len + 2, "%s", npc_name);
			}
			font->draw_text(
					ibuf, topx + 60, menuy + 10, disp_name, transto.data());
			if (Mouse::mouse()) {
				Mouse::mouse()->show(transto.data());
			}
			gwin->get_win()->ShowFillGuardBand();
			redraw = false;
		} else if (Mouse::mouse() && Mouse::mouse()->is_onscreen()) {
			gwin->get_win()->ShowFillGuardBand();
		}
		SDL_Renderer* renderer
				= SDL_GetRenderer(gwin->get_win()->get_screen_window());
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			Uint16 keysym_unicode = 0;
			bool   isTextInput    = false;
			SDL_ConvertEventToRenderCoordinates(renderer, &event);

			if (event.type == SDL_EVENT_MOUSE_MOTION) {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(
						event.motion.x, event.motion.y, gwin->get_fastmouse(),
						mx, my);
				Mouse::mouse()->hide();
				Mouse::mouse()->move(mx, my);
				Mouse::mouse_update = true;
				Mouse::mouse()->show(transto.data());
			}

			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
				|| event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				const SDL_Rect rectName   = {topx + 10, menuy + 10, 130, 16};
				const SDL_Rect rectSex    = {topx + 10, menuy + 25, 130, 16};
				const SDL_Rect rectOnward = {topx + 10, topy + 180, 130, 16};
				const SDL_Rect rectReturn = {centerx + 10, topy + 180, 130, 16};
				SDL_Point      point;
				gwin->get_win()->screen_to_game(
						event.button.x, event.button.y, gwin->get_fastmouse(),
						point.x, point.y);
				if (SDL_GetRectEnclosingPoints(&point, 1, &rectName, nullptr)) {
					if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
						selected = 0;
					} else if (selected == 0 && touchui != nullptr) {
						touchui->promptForName(npc_name);
					}
					redraw = true;
				} else if (SDL_GetRectEnclosingPoints(
								   &point, 1, &rectSex, nullptr)) {
					if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
						selected = 1;
					} else if (selected == 1) {
						skindata = Shapeinfo_lookup::GetNextSelSkin(
								skindata, si_installed, true);
					}
					redraw = true;
				} else if (SDL_GetRectEnclosingPoints(
								   &point, 1, &rectOnward, nullptr)) {
					if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
						selected = 2;
					} else if (selected == 2) {
						editing = false;
						ok      = true;
					}
					redraw = true;
				} else if (SDL_GetRectEnclosingPoints(
								   &point, 1, &rectReturn, nullptr)) {
					if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
						selected = 3;
					} else if (selected == 3) {
						editing = false;
						ok      = false;
					}
					redraw = true;
				}
			} else if (event.type == TouchUI::eventType) {
				if (event.user.code == TouchUI::EVENT_CODE_TEXT_INPUT) {
					if (selected == 0 && event.user.data1 != nullptr) {
						strncpy(npc_name, static_cast<char*>(event.user.data1),
								max_name_len);
						npc_name[max_name_len] = '\0';
						free(event.user.data1);
						redraw = true;
					}
				}
			} else if (event.type == SDL_EVENT_TEXT_INPUT) {
				isTextInput    = true;
				keysym_unicode = event.text.text[0];
				event.type     = SDL_EVENT_KEY_DOWN;
				event.key.key  = SDLK_UNKNOWN;
			}
			if (event.type == SDL_EVENT_KEY_DOWN) {
				redraw = true;
				switch (event.key.key) {
				case SDLK_SPACE:
					if (selected == 0) {
						const int len = strlen(npc_name);
						if (len < max_name_len) {
							npc_name[len]     = ' ';
							npc_name[len + 1] = 0;
						}
					} else if (selected == 1) {
						skindata = Shapeinfo_lookup::GetNextSelSkin(
								skindata, si_installed, true);
					} else if (selected == 2) {
						editing = false;
						ok      = true;
					} else if (selected == 3) {
						editing = ok = false;
					}
					break;
				case SDLK_LEFT:
					if (selected == 1) {
						skindata = Shapeinfo_lookup::GetPrevSelSkin(
								skindata, si_installed, true);
					}
					break;
				case SDLK_RIGHT:
					if (selected == 1) {
						skindata = Shapeinfo_lookup::GetNextSelSkin(
								skindata, si_installed, true);
					}
					break;
				case SDLK_ESCAPE:
					editing = false;
					ok      = false;
					break;
				case SDLK_TAB:
				case SDLK_DOWN:
					++selected;
					if (selected == num_choices) {
						selected = 0;
					}
					break;
				case SDLK_UP:
					--selected;
					if (selected < 0) {
						selected = num_choices - 1;
					}
					break;
				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					if (selected < 2) {
						++selected;
					} else if (selected == 2) {
						editing = false;
						ok      = true;
					} else {
						editing = ok = false;
					}
					break;
				case SDLK_BACKSPACE:
					if (selected == 0 && strlen(npc_name) > 0) {
						npc_name[strlen(npc_name) - 1] = 0;
					}
					break;
				default: {
					if ((isTextInput && selected == 0)
						|| (!isTextInput && keysym_unicode > +'~'
							&& selected == 0)) {
						const int len = strlen(npc_name);
						char      chr = 0;
						if ((keysym_unicode & 0xFF80) == 0) {
							chr = keysym_unicode & 0x7F;
						}

						if (chr >= ' ' && len < max_name_len) {
							npc_name[len]     = chr;
							npc_name[len + 1] = 0;
						}
					} else {
						redraw = false;
					}
				} break;
				}
			}
		}
	} while (editing);
	if (SDL_TextInputActive(window)) {
		SDL_StopTextInput(window);
	}
	// Hide mouse on way out to clear the mouse's dirty box
	if (Mouse::mouse()) {
		Mouse::mouse()->hide();
	}
	gwin->clear_screen();

	if (ok) {
#ifdef DEBUG
		std::cout << "Skin is: " << skindata->skin_id
				  << " Sex is: " << skindata->is_female << std::endl;
#endif
		set_avskin(skindata->skin_id);
		set_avname(npc_name);
		set_avsex(skindata->is_female);
		pal->fade_out(c_fade_out_time);
		gwin->clear_screen(true);
		// Immediately restore the palette after clearing the screen incase
		// init_gamedat errors and displays a yesno_gump
		pal->fade_in(0);
		ok = gwin->init_gamedat(true);
	} else {
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 6);
		sman->paint_shape(topx, topy, shapes.get_shape(0x2, 0));
		pal->apply();
	}
	return ok;
}
