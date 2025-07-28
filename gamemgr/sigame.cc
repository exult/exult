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

#include "sigame.h"

#include "Audio.h"
#include "common_types.h"
#include "data/exult_flx.h"
#include "data/exult_si_flx.h"
#include "databuf.h"
#include "exult.h"
#include "files/utils.h"
#include "flic/playfli.h"
#include "font.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "mappatch.h"
#include "miscinf.h"
#include "palette.h"
#include "playscene.h"
#include "shapeid.h"
#include "span.h"
#include "touchui.h"
#include "txtscroll.h"

#include <array>
#include <cctype>

using std::cout;
using std::endl;
using std::make_unique;
using std::rand;
using std::unique_ptr;

SI_Game::SI_Game() {
	if (!read_game_xml()) {
		add_shape("gumps/check", 2);
		add_shape("gumps/fileio", 3);
		add_shape("gumps/fntext", 4);
		add_shape("gumps/loadbtn", 5);
		add_shape("gumps/savebtn", 6);
		add_shape("gumps/halo", 7);
		add_shape("gumps/disk", 19);
		add_shape("gumps/heart", 20);
		add_shape("gumps/statatts", 23);
		add_shape("gumps/musicbtn", 24);
		add_shape("gumps/speechbtn", 25);
		add_shape("gumps/soundbtn", 26);
		add_shape("gumps/spellbook", 38);
		add_shape("gumps/combat", 41);
		add_shape("gumps/statsdisplay", 42);
		add_shape("gumps/quitbtn", 50);
		add_shape("gumps/yesnobox", 51);
		add_shape("gumps/yesbtn", 52);
		add_shape("gumps/nobtn", 53);
		add_shape("gumps/book", 27);
		add_shape("gumps/scroll", 49);
		add_shape("gumps/combat_stats", 91);
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
		add_shape("gumps/chest", 18);
		add_shape("gumps/shipshold", 21);
		add_shape("gumps/drawer", 22);
		add_shape("gumps/woodsign", 44);
		add_shape("gumps/tombstone", 45);
		add_shape("gumps/goldsign", 46);
		add_shape("gumps/body", 48);
		add_shape("gumps/tree", 64);

		add_shape("gumps/cstats/1", 58);
		add_shape("gumps/cstats/2", 59);
		add_shape("gumps/cstats/3", 60);
		add_shape("gumps/cstats/4", 61);
		add_shape("gumps/cstats/5", 62);
		add_shape("gumps/cstats/6", 63);

		add_shape("sprites/map", 22);
		add_shape("sprites/cheatmap", EXULT_SI_FLX_SIMAP_SHP);

		add_shape("gumps/scroll_spells", 66);
		add_shape("gumps/spell_scroll", 65);
		add_shape("gumps/jawbone", 56);
		add_shape("gumps/tooth", 57);

		const char* exultflx = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
		const char* gameflx  = BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);

		add_resource("files/shapes/count", nullptr, 8);
		add_resource("files/shapes/0", SHAPES_VGA, 0);
		add_resource("files/shapes/1", FACES_VGA, 0);
		add_resource("files/shapes/2", GUMPS_VGA, 0);
		add_resource("files/shapes/3", SPRITES_VGA, 0);
		add_resource("files/shapes/4", MAINSHP_FLX, 0);
		add_resource("files/shapes/5", PAPERDOL, 0);
		add_resource("files/shapes/6", exultflx, 0);
		add_resource("files/shapes/7", FONTS_VGA, 0);

		add_resource("files/gameflx", gameflx, 0);

		add_resource(
				"config/defaultkeys", gameflx, EXULT_SI_FLX_DEFAULTKEYS_TXT);
		add_resource("config/bodies", gameflx, EXULT_SI_FLX_BODIES_TXT);
		add_resource(
				"config/paperdol_info", gameflx,
				EXULT_SI_FLX_PAPERDOL_INFO_TXT);
		add_resource("config/shape_info", gameflx, EXULT_SI_FLX_SHAPE_INFO_TXT);
		add_resource(
				"config/shape_files", gameflx, EXULT_SI_FLX_SHAPE_FILES_TXT);
		add_resource(
				"config/avatar_data", gameflx, EXULT_SI_FLX_AVATAR_DATA_TXT);
		add_resource("config/autonotes", gameflx, EXULT_SI_FLX_AUTONOTES_TXT);

		add_resource("palettes/count", nullptr, 14);
		add_resource("palettes/0", PALETTES_FLX, 0);
		add_resource("palettes/1", PALETTES_FLX, 1);
		add_resource("palettes/2", PALETTES_FLX, 2);
		add_resource("palettes/3", PALETTES_FLX, 3);
		add_resource("palettes/4", PALETTES_FLX, 4);
		add_resource("palettes/5", PALETTES_FLX, 5);
		add_resource("palettes/6", PALETTES_FLX, 6);
		add_resource("palettes/7", PALETTES_FLX, 7);
		add_resource("palettes/8", PALETTES_FLX, 8);
		add_resource("palettes/9", PALETTES_FLX, 9);
		add_resource("palettes/10", PALETTES_FLX, 10);
		add_resource("palettes/11", PALETTES_FLX, 11);
		add_resource("palettes/12", PALETTES_FLX, 12);
		add_resource("palettes/13", MAINSHP_FLX, 1);
		add_resource("palettes/14", MAINSHP_FLX, 26);

		add_resource("palettes/patch/0", PATCH_PALETTES, 0);
		add_resource("palettes/patch/1", PATCH_PALETTES, 1);
		add_resource("palettes/patch/2", PATCH_PALETTES, 2);
		add_resource("palettes/patch/3", PATCH_PALETTES, 3);
		add_resource("palettes/patch/4", PATCH_PALETTES, 4);
		add_resource("palettes/patch/5", PATCH_PALETTES, 5);
		add_resource("palettes/patch/6", PATCH_PALETTES, 6);
		add_resource("palettes/patch/7", PATCH_PALETTES, 7);
		add_resource("palettes/patch/8", PATCH_PALETTES, 8);
		add_resource("palettes/patch/9", PATCH_PALETTES, 9);
		add_resource("palettes/patch/10", PATCH_PALETTES, 10);
		add_resource("palettes/patch/11", PATCH_PALETTES, 11);
		add_resource("palettes/patch/12", PATCH_PALETTES, 12);
		add_resource("palettes/patch/13", PATCH_MAINSHP, 1);
		add_resource("palettes/patch/14", PATCH_MAINSHP, 26);

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
	fontManager.add_font("SIINTRO_FONT", INTRO_DAT, PATCH_INTRO, 14, 0);
	fontManager.add_font("SMALL_BLACK_FONT", FONTS_VGA, PATCH_FONTS, 2, 0);
	fontManager.add_font("TINY_BLACK_FONT", FONTS_VGA, PATCH_FONTS, 4, 0);
	fontManager.add_font(
			"EXULT_END_FONT", File_spec(EXULT_FLX, EXULT_FLX_ENDFONT_SHP),
			PATCH_ENDFONT, 0, -1);
	// TODO: Verify if these map patches make sense for SI Beta, and come up
	// with patches specific to it.
	if (GAME_SI && !is_si_beta()) {
		Map_patch_collection& mp = gwin->get_map_patches();
		// Egg by "PC pirate" in forest:
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(647, 1899, 0), 275, 7, 1)));
		// Carpets above roof in Monitor:
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(1035, 2572, 8), 483, 1, 0), true));
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(1034, 2571, 6), 483, 1, 0)));
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(1034, 2571, 5), 483, 1, 0), true));
		// Neyobi under a fur:
		mp.add(std::make_unique<Map_patch_modify>(
				Object_spec(Tile_coord(1012, 873, 0), 867, 13, 0),
				Object_spec(Tile_coord(1013, 873, 1), 867, 13, 0)));
		// Bread on the prep table in Moonshade
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(2381, 1896, 2), 377, 1, 0)));
		// Flour on Moonshade display table
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(2378, 1890, 2), 863, 16, 0)));
		// Dough on Moonshade display table
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(2369, 1896, 2), 863, 17, 0)));
		// Skullcrusher Mountains
		//    music instruments in wall
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(35, 1942, 0), 690, 0, 0)));
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(35, 1954, 0), 692, 0, 0)));
		// FIXME: eggs shouldn't spawn inside of walls
		//    egg spawning spiders in wall
		mp.add(std::make_unique<Map_patch_remove>(
				Object_spec(Tile_coord(60, 1937, 0), 275, 0, 0)));
	}
}

// Little weighted random function for lightning on the castle
static int get_frame() {
	const int num = rand() % 9;

	if (num >= 8) {
		return 2;
	} else if (num >= 6) {
		return 1;
	}
	return 0;
}

void SI_Game::play_intro() {
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

	std::shared_ptr<Font> sifont = fontManager.get_font("SIINTRO_FONT");

	const bool speech = audio->is_audio_enabled() && audio->is_speech_enabled();
	const bool subtitles = !speech || Audio::get_ptr()->is_speech_with_subs();
	bool       extended_intro = gwin->get_extended_intro();

	auto select_fli = [&extended_intro](int orig_id, int ext_id) {
		if (extended_intro) {
			return playfli(game->get_resource("files/gameflx").str, ext_id);
		}
		return playfli(INTRO_DAT, PATCH_INTRO, orig_id);
	};
	constexpr static const std::array original_intro_counts{
			20, 20,  20, 50, 10,  75, 20, 20, 37, 55, 73, 220, 290, 50,
			81, 200, 20, 61, 320, 20, 20, 61, 61, 61, 20, 300, 20};
	constexpr static const std::array extended_intro_counts{
			20, 20,  20, 25, 10,  25, 20, 20,  37,  55, 73, 220, 290, 50,
			81, 200, 20, 61, 320, 20, 10, 115, 115, 91, 20, 300, 20};
	static_assert(
			original_intro_counts.size() == extended_intro_counts.size(),
			"Missing array count in one of the arrays");
	tcb::span selected_intro
			= extended_intro ? extended_intro_counts : original_intro_counts;
	const auto* selected_intro_counts = selected_intro.begin();

	gwin->clear_screen(true);

	// Lord British presents...
	try {
		playfli fli0(INTRO_DAT, PATCH_INTRO, 0);
		fli0.info();

		int next  = 0;
		int count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			next = fli0.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			wait_delay(0, 0, 1);
		}

		next = fli0.play(win, 0, 0, next, 100);
		win->ShowFillGuardBand();

		if (wait_delay(3000, 0, 1)) {
			throw UserBreakException();
		}

		count = *selected_intro_counts++;
		for (int j = count; j; j--) {
			next = fli0.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			wait_delay(0, 0, 1);
		}

		if (wait_delay(0, 0, 1)) {
			throw UserBreakException();
		}

		gwin->clear_screen(true);

		// Castle Outside

		// Start Music
		if (extended_intro) {
			const auto* midi_driver = audio->get_midi();
			if (midi_driver->get_ogg_enabled()) {
				audio->start_music(
						EXULT_SI_FLX_EXT_INTRO_SI01_OGG, false,
						MyMidiPlayer::Force_None, EXULT_SI_FLX);
			} else if (midi_driver->is_mt32()) {
				audio->start_music(
						EXULT_SI_FLX_EXT_INTRO_R_XMI, false,
						MyMidiPlayer::Force_None, EXULT_SI_FLX);
			} else {
				audio->start_music(
						EXULT_SI_FLX_EXT_INTRO_A_XMI, false,
						MyMidiPlayer::Force_None, EXULT_SI_FLX);
			}
		} else {
			audio->start_music(R_SINTRO, 0, false);
		}

		size_t                      size;
		unique_ptr<unsigned char[]> buffer;
		// Thunder, note we use the buffer again later so it's not freed here
		// this is not speech so removing this check
		// if (speech)
		{
			const U7multiobject voc_thunder(INTRO_DAT, PATCH_INTRO, 15);
			buffer = voc_thunder.retrieve(size);
			audio->copy_and_play_sfx(buffer.get() + 8, size - 8, false);
		}

		playfli fli1 = select_fli(1, EXULT_SI_FLX_EXT_INTRO_CASTLE_FLC);
		fli1.info();

		fli1.play(win, 0, 1, 0, 0);

		next     = SDL_GetTicks();
		int prev = -1;
		int num;

		count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			num = get_frame();
			if (prev != num) {
				for (int i = 0; i < num + 1; i++) {
					fli1.play(win, i, i, next);
				}
			}

			prev = num;
			next += 75;
			win->ShowFillGuardBand();
			if (wait_delay(1, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			num = get_frame();
			if (!extended_intro) {
				if (prev != num) {
					for (int i = 0; i < num + 1; i++) {
						fli1.play(win, i, i, next);
					}
				}
			} else {
				next = fli1.play(win, j, j, next) + 30;
			}

			if (jive) {
				sifont->center_text(
						ibuf, centerx, centery + 50, get_text_msg(dick_castle));
			} else {
				sifont->center_text(
						ibuf, centerx, centery + 50, get_text_msg(lord_castle));
			}

			prev = num;
			next += 75;
			win->ShowFillGuardBand();
			if (wait_delay(1, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			num = get_frame();
			if (prev != num) {
				for (int i = 0; i < num + 1; i++) {
					fli1.play(win, i, i, next);
				}
			}

			// Thunder again, we free the buffer here
			if (/* speech &&*/ j == 5) {
				audio->copy_and_play_sfx(buffer.get() + 8, size - 8, false);
				buffer.reset();
			}

			prev = num;
			next += 75;
			win->ShowFillGuardBand();
			if (wait_delay(1, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			num = get_frame();
			if (!extended_intro) {
				if (prev != num) {
					for (int i = 0; i < num + 1; i++) {
						fli1.play(win, i, i, next);
					}
				}
			} else {
				next = fli1.play(win, j, j, next) + 30;
			}

			for (int i = 0; i < 3; i++) {
				sifont->center_text(
						ibuf, centerx, centery + 50 + 15 * i,
						get_text_msg(bg_fellow + i));
			}

			prev = num;
			next += 75;
			win->ShowFillGuardBand();
			if (wait_delay(1, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int j = count; j; j--) {
			next = fli1.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		if (wait_delay(0)) {
			throw UserBreakException();
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Guard walks in
		playfli fli2 = select_fli(2, EXULT_SI_FLX_EXT_INTRO_SANCTUM1_FLC);
		fli2.info();

		count = *selected_intro_counts++;
		for (int j = 0; j < count; j++) {
			next = fli2.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Guard walks in
		int j;
		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli2.play(win, j, j, next);
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Guard walks in
		if (speech && !jive) {
			const U7multiobject voc_my_leige(INTRO_DAT, PATCH_INTRO, 16);
			auto                buffer = voc_my_leige.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		count = *selected_intro_counts++;
		for (; j < count; j++) {
			next = fli2.play(win, j, j, next);

			if (jive) {
				sifont->draw_text(
						ibuf, centerx + 30, centery + 87,
						get_text_msg(yo_homes));
			} else if (subtitles) {
				sifont->draw_text(
						ibuf, centerx + 30, centery + 87,
						get_text_msg(my_leige));
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		next = fli2.play(win, j, j, next);
		win->ShowFillGuardBand();
		wait_delay(0, 0, 1);

		const char* const all_we[2]
				= {get_text_msg(all_we0), get_text_msg(all_we0 + 1)};

		if (speech && !jive) {
			const U7multiobject voc_all_we(INTRO_DAT, PATCH_INTRO, 17);
			auto                buffer = voc_all_we.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		count = *selected_intro_counts++;
		for (; j < count; j++) {
			next = fli2.play(win, j, j, next);

			if (subtitles || jive) {
				sifont->draw_text(
						ibuf, centerx + 150 - sifont->get_text_width(all_we[0]),
						centery + 74, all_we[0]);
				sifont->draw_text(
						ibuf, centerx + 160 - sifont->get_text_width(all_we[1]),
						centery + 87, all_we[1]);
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int i = 0; i < count; i++) {
			if (wait_delay(10)) {
				throw UserBreakException();
			}
		}

		fli2.play(win, j, j, next);

		if (subtitles || jive) {
			const char* const and_a[2]
					= {get_text_msg(and_a0), get_text_msg(and_a0 + 1)};
			sifont->draw_text(
					ibuf, centerx + 150 - sifont->get_text_width(and_a[0]),
					centery + 74, and_a[0]);
			sifont->draw_text(
					ibuf, centerx + 150 - sifont->get_text_width(and_a[1]),
					centery + 87, and_a[1]);
		}

		win->ShowFillGuardBand();
		j++;

		count = *selected_intro_counts++;
		for (int i = 0; i < count; i++) {
			if (wait_delay(10, 0, 1)) {
				throw UserBreakException();
			}
		}

		fli2.play(win, j, j);
		j++;

		count = *selected_intro_counts++;
		for (int i = 0; i < count; i++) {
			if (wait_delay(10, 0, 1)) {
				throw UserBreakException();
			}
		}

		if (speech && !jive) {
			const U7multiobject voc_indeed(INTRO_DAT, PATCH_INTRO, 18);
			auto                buffer = voc_indeed.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		next = fli2.play(win, j, j);
		j++;

		count = *selected_intro_counts++;
		for (; j < count; j++) {
			next = fli2.play(win, j, j, next);

			if (jive) {
				sifont->draw_text(
						ibuf, topx + 40, centery + 74, get_text_msg(iree));
			} else if (subtitles) {
				sifont->draw_text(
						ibuf, topx + 40, centery + 74, get_text_msg(indeed));
				sifont->draw_text(
						ibuf, topx + 40, centery + 87,
						get_text_msg(indeed + 1));
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (int i = 0; i < count; i++) {
			if (wait_delay(10)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Scroll opens
		playfli fli3 = select_fli(3, EXULT_SI_FLX_EXT_INTRO_SANCTUM2_FLC);
		fli3.info();

		next = 0;

		// Scroll opens
		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli3.play(win, j, j, next) + 20;
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// 'Stand Back'
		if (speech && !jive) {
			const U7multiobject voc_stand_back(INTRO_DAT, PATCH_INTRO, 19);
			auto                buffer = voc_stand_back.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		count = *selected_intro_counts++;
		for (; j < count; j++) {
			next = fli3.play(win, j, j, next) + 20;

			if (jive) {
				sifont->draw_text(
						ibuf, topx + 70, centery + 60, get_text_msg(jump_back));
			} else if (subtitles) {
				sifont->draw_text(
						ibuf, topx + 70, centery + 60,
						get_text_msg(stand_back));
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Big G speaks
		playfli fli4(INTRO_DAT, PATCH_INTRO, 4);
		fli4.info();

		IExultDataSource gshape_ds(INTRO_DAT, PATCH_INTRO, 30);
		char             name[9] = {0};
		gshape_ds.read(name, 8);
		Shape_frame* sf;

		const Shape_file gshape(&gshape_ds);

		cout << "Shape '" << name << "' in intro.dat has "
			 << gshape.get_num_frames() << endl;

		if (speech && !jive) {
			const U7multiobject voc_big_g(INTRO_DAT, PATCH_INTRO, 20);
			auto                buffer = voc_big_g.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		next = 0;

		// Batlin...

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli4.play(win, j % 40, j % 40, next);

			if (j < 300) {
				sf = gshape.get_frame((j / 2) % 7);
			} else {
				sf = gshape.get_frame(0);
			}

			if (sf) {
				sman->paint_shape(centerx - 36, centery, sf);
			}

			if (j < 100 && jive) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(batlin2));
				sifont->center_text(
						ibuf, centerx, centery + 87, get_text_msg(batlin2 + 1));
			} else if (j < 200 && jive) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(you_must));
				sifont->center_text(
						ibuf, centerx, centery + 87,
						get_text_msg(you_must + 1));
			} else if (j < 300 && jive) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(soon_i));
				sifont->center_text(
						ibuf, centerx, centery + 87, get_text_msg(soon_i + 1));
			} else if (j < 100 && (subtitles)) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(batlin));
				sifont->center_text(
						ibuf, centerx, centery + 87, get_text_msg(batlin + 1));
			} else if (j < 200 && (subtitles)) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(you_shall));
				sifont->center_text(
						ibuf, centerx, centery + 87,
						get_text_msg(you_shall + 1));
			} else if (j < 300 && (subtitles)) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(there_i));
				sifont->center_text(
						ibuf, centerx, centery + 87, get_text_msg(there_i + 1));
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}
		sf = gshape.get_frame(0);

		count = *selected_intro_counts++;
		for (j = count; j; j--) {
			next = fli4.play(win, 0, 0, next, j * 5);
			if (sf) {
				sman->paint_shape(centerx - 36, centery, sf);
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Tis LBs's Worst fear
		playfli fli5 = select_fli(5, EXULT_SI_FLX_EXT_INTRO_SHIP1_FLC);
		fli5.info();

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli5.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		if (speech && !jive) {
			const U7multiobject voc_tis_my(INTRO_DAT, PATCH_INTRO, 21);
			auto                buffer = voc_tis_my.retrieve(size);
			audio->copy_and_play_speech(buffer.get() + 8, size - 8, false);
		}

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli5.play(win, j, j, next) + 30;

			if (j < 20 && (subtitles || jive)) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(tis_my));
			} else if (j > 22 && (subtitles || jive)) {
				sifont->center_text(
						ibuf, centerx, centery + 74, get_text_msg(tis_my + 1));
				sifont->center_text(
						ibuf, centerx, centery + 87, get_text_msg(tis_my + 2));
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Boat 1
		playfli fli6 = select_fli(6, EXULT_SI_FLX_EXT_INTRO_SHIP2_FLC);
		fli6.info();

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli6.play(win, j, j, next) + 30;
			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Boat 2
		playfli fli7 = select_fli(7, EXULT_SI_FLX_EXT_INTRO_PIL1_FLC);
		fli7.info();

		const char* zot = "Zot!";

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli7.play(win, j, j, next) + 30;

			if (j > 55 && jive) {
				sifont->center_text(ibuf, centerx, centery + 74, zot);
			}

			win->ShowFillGuardBand();
			if (wait_delay(0, 0, 1)) {
				throw UserBreakException();
			}
		}

		// Do this! Prevents palette corruption
		gwin->clear_screen(true);

		// Ultima VII Part 2
		playfli fli8(INTRO_DAT, PATCH_INTRO, 8);
		fli8.info();

		count = *selected_intro_counts++;
		for (j = 0; j < count; j++) {
			next = fli8.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			wait_delay(0, 0, 1);
		}

		next = fli8.play(win, 0, 0, next, 100);
		win->ShowFillGuardBand();
		wait_delay(0, 0, 1);

		count = *selected_intro_counts++;
		for (int i = 0; i < count; i++) {
			if (wait_delay(10)) {
				throw UserBreakException();
			}
		}

		count = *selected_intro_counts++;
		for (j = count; j; j--) {
			next = fli8.play(win, 0, 0, next, j * 5);
			win->ShowFillGuardBand();
			wait_delay(0, 0, 1);
		}
	} catch (const UserBreakException& /*x*/) {
	}

	// Fade out the palette...
	// pal->fade_out(c_fade_out_time);
	// this doesn't work right ATM since the FLIC player has its own palette
	// handling

	// ... and clean the screen.
	gwin->clear_screen(true);

	// Stop all audio output
	audio->cancel_streams();
}

Shape_frame* SI_Game::get_menu_shape() {
	return menushapes.get_shape(0x2, 0);
}

void SI_Game::top_menu() {
	Audio::get_ptr()->start_music(
			28, true, MyMidiPlayer::Force_None, MAINSHP_FLX);
	sman->paint_shape(topx, topy, get_menu_shape());
	pal->load(MAINSHP_FLX, PATCH_MAINSHP, 26);
	pal->fade_in(60);
}

void SI_Game::show_journey_failed() {
	pal->fade_out(50);
	gwin->clear_screen(true);
	sman->paint_shape(topx, topy, get_menu_shape());
	journey_failed_text();
}

/*
 *  ExCineLite
 */

// ExCineEvent
struct ExCineEvent {
	uint32      time;    // Time to start, In MS
	const char* file;
	const char* patch;
	int         index;

	virtual bool play_it(Image_window* win, uint32 time)
			= 0;    // Return true if screen updated

	bool can_play() {
		return file != nullptr;
	}

	ExCineEvent(uint32 t, const char* f, const char* p, int i)
			: time(t), file(f), patch(p), index(i) {}

	virtual ~ExCineEvent() noexcept = default;
};

//
// ExCineFlic
//

struct ExCineFlic : public ExCineEvent {
private:
	int  start;     // First frame to play
	int  count;     // Number of frames
	bool repeat;    // Repeat?
	int  cur;     // Frame currently being displayed (note, it's not the actual
				  // frame)
	int speed;    // Speed of playback (ms per frame)

	// Data info
	std::unique_ptr<playfli> player;

public:
	bool play_it(Image_window* win, uint32 t) override;

	void load_flic();
	void free_flic();

	void fade_out(int cycles);

	ExCineFlic(
			uint32 time, const char* file, const char* patch, int i, int s,
			int c, bool r, int spd)
			: ExCineEvent(time, file, patch, i), start(s), count(c), repeat(r),
			  cur(-1), speed(spd), player(nullptr) {}

	ExCineFlic(uint32 time)
			: ExCineEvent(time, nullptr, nullptr, 0), start(0), count(0),
			  repeat(false), cur(0), speed(0), player(nullptr) {}
};

void ExCineFlic::load_flic() {
	free_flic();
	COUT("Loading " << file << ":" << index);
	if (patch) {
		player = make_unique<playfli>(file, patch, index);
	} else {
		player = make_unique<playfli>(file, index);
	}
	player->info();
}

void ExCineFlic::free_flic() {
	COUT("Freeing " << file << ":" << index);
	player.reset();
}

bool ExCineFlic::play_it(Image_window* win, uint32 t) {
	if (t < time) {
		return false;
	}

	if (cur + 1 < count || repeat) {
		// Only advance frame if we can
		const uint32 time_next = time + (cur + 1) * speed;
		if (time_next <= t) {
			cur++;
			// The actual frame number
			const int actual = start + (cur % count);
			player->play(win, actual, actual, 0);
			return true;
		}
	}
	player->put_buffer(win);
	return false;
}

void ExCineFlic::fade_out(int cycles) {
	if (player) {
		player->get_palette()->fade_out(cycles);
	}
}

//
// ExCineVoc
//

struct ExCineVoc : public ExCineEvent {
private:
	bool played;

public:
	bool play_it(Image_window* win, uint32 t) override;

	ExCineVoc(uint32 time, const char* file, const char* patch, int index)
			: ExCineEvent(time, file, patch, index), played(false) {}
};

bool ExCineVoc::play_it(Image_window* win, uint32 t) {
	ignore_unused_variable_warning(win, t);
	size_t              size;
	const U7multiobject voc(file, patch, index);
	auto                buffer = voc.retrieve(size);
	uint8*              buf    = buffer.get();
	if (!memcmp(buf, "win", sizeof("win") - 1)) {
		// IFF junk.
		buf += 8;
		size -= 8;
	}
	Audio::get_ptr()->copy_and_play_speech(buf, size, false);
	played = true;

	return false;
}

// ExSubEvent
struct ExSubEvent {
	uint32                time;    // Time to start, In MS
	const int             first_sub;
	const int             num_subs;
	std::shared_ptr<Font> sub_font;

	ExSubEvent(
			uint32 t, const int first, const int cnt, std::shared_ptr<Font> fnt)
			: time(t), first_sub(first), num_subs(cnt), sub_font(fnt) {}

	void show_sub(Image_buffer8* ibuf, int centerx, int centery) {
		int suby;
		switch (num_subs) {
		case 1:
			suby = centery + 87;
			break;
		case 2:
			suby = centery + 74;
			break;
		default:
			suby = centery + 61;
			break;
		}
		for (int ii = first_sub; ii < first_sub + num_subs; ii++, suby += 13) {
			sub_font->draw_text(
					ibuf,
					centerx - sub_font->get_text_width(get_text_msg(ii)) / 2,
					suby, get_text_msg(ii));
		}
	}
};

std::vector<unsigned int> SI_Game::get_congratulations_messages() {
	return {congrats_si + 0, congrats_si + 1, congrats_si + 2,
			congrats_si + 3, congrats_si + 4, congrats_si + 5,
			congrats_si + 6, congrats_si + 7, congrats_si + 8};
}

//
// Serpent Isle Endgame
//
void SI_Game::end_game(bool success, bool within_game) {
	ignore_unused_variable_warning(success);
	waitforspeech();
	Audio* audio = Audio::get_ptr();
	audio->stop_music();
	if (scene_available("endgame")) {
		// Play the custom endgame scene
		play_scene("endgame");
		return;
	}
	MyMidiPlayer* midi = audio->get_midi();
	if (midi) {
		midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_ENDGAME);
	}

	std::shared_ptr<Font> sifont = fontManager.get_font("SIINTRO_FONT");

	const bool speech = audio->is_audio_enabled() && audio->is_speech_enabled();
	const bool subtitles = !speech || Audio::get_ptr()->is_speech_with_subs();

	gwin->clear_screen(true);

	/* Endgame General Timings (in ms)

		  0 - Avatar floating right
	   6350 - Begin Serpent Swirling
	  10850 - Serpent Comes on screen
	  14643 - Balanced, and "there we are done"
	  17281 - "balance is restored"
	  21300 - "serpent isle"
	  22900 - "briatnnia"
	  24300 - "your earth"
	  26000 - "the entire universe"
	  28600 - "all are saved"
	  31600 - Avatar floating right, "worry not about your friend dupre"
	  35100 - "he is one with us"
	  37000 - "and content"
	  39800 - "good bye avatar"
	  42040 - "we thank you"
	  48550 - "well well well avatar"
	  48900 - Avatar floating left
	  51750 - "You have managed to thawt me one again"
	  55500 - "by restoring balance..."
	  62500 - floating far, "but now here you are"
	  64500 - "poised at the edge of eternity"
	  67000 - "where would you go?"
	  70250 - floating left, "back to britannia?"
	  72159 - "to earth?"
	  74750 - "perhaps you would join me.."
	  75500 - big g's hand
	  78000 - "we do have a score to settle"

	Flic Index
	9 = Avfloat
	10 = snake1
	11 = snake2
	12 = avfar
	13 = xavgrab

	Frame count : 61
	Width :       320
	Height :      200
	Depth :       8
	Speed :       5

	Frame count : 156
	Width :       320
	Height :      200
	Depth :       8
	Speed :       8

	Frame count : 4
	Width :       320
	Height :      200
	Depth :       8
	Speed :       10

	Frame count : 61
	Width :       320
	Height :      200
	Depth :       8
	Speed :       5

	Frame count : 121
	Width :       320
	Height :      200
	Depth :       8
	Speed :       5

	Sound Index
	22 = "there we are done, balance is restored"
	23 = "serpent isle...."
	24 = "goodbye avatar..."
	25 = "well well well avatar..."
	26 = "by restoring balance..."
	27 = "but now here you are..."
	28 = "back to britannia..."
	29 = "perhaps you..."

		  0 - Repeat 9
	   6350 - Play 10
	  14643 - Repeat 10, Play 22
	  21300 - Play 23
	  31600 - Repeat 9
	  39800 - Repeat 11, Play 24
	  48550 - Play 25
	  48900 - Repeat 13
	  55500 - Play 26
	  62500 - Repeat 12, play 27
	  70250 - Play 13, play 28
	  72159 - "to earth?"
	  74750 - play 29
	  75500 - big g's hand
	  78000 - "we do have a score to settle"
	*/

	// Flic List
	std::array flics{
			ExCineFlic(0, INTRO_DAT, PATCH_INTRO, 9, 0, 61, true, 75),
			ExCineFlic(6350, INTRO_DAT, PATCH_INTRO, 10, 0, 156, false, 95),
			ExCineFlic(21170, INTRO_DAT, PATCH_INTRO, 9, 0, 61, true, 75),
			ExCineFlic(39800, INTRO_DAT, PATCH_INTRO, 11, 0, 4, true, 75),
			ExCineFlic(48900, INTRO_DAT, PATCH_INTRO, 13, 0, 61, true, 75),
			ExCineFlic(62500, INTRO_DAT, PATCH_INTRO, 12, 0, 61, true, 75),
			ExCineFlic(70250, INTRO_DAT, PATCH_INTRO, 13, 0, 121, false, 75),
			ExCineFlic(82300)};
	const int   last_flic = flics.size() - 1;
	int         cur_flic  = -1;
	ExCineFlic* flic      = nullptr;
	ExCineFlic* pal_flic  = nullptr;

	// Voc List
	std::array vocs{
			ExCineVoc(14700, INTRO_DAT, PATCH_INTRO, 22),
			ExCineVoc(21300, INTRO_DAT, PATCH_INTRO, 23),
			ExCineVoc(39800, INTRO_DAT, PATCH_INTRO, 24),
			ExCineVoc(47700, INTRO_DAT, PATCH_INTRO, 25),
			ExCineVoc(55400, INTRO_DAT, PATCH_INTRO, 26),
			ExCineVoc(62500, INTRO_DAT, PATCH_INTRO, 27),
			ExCineVoc(70250, INTRO_DAT, PATCH_INTRO, 28),
			ExCineVoc(74750, INTRO_DAT, PATCH_INTRO, 29)};

	const int last_voc = vocs.size() - 1;
	int       cur_voc  = -1;

	// Subtitle times
	std::array subs{
			ExSubEvent(
					14643, balance, 2,
					sifont),    // "There, we are done.\nBalance is restored"
			ExSubEvent(
					21300, si_earth, 2,
					sifont),    // "Serpent Isle, Britannia, your Earth,\nthe
								// entire universe, all are saved."
			ExSubEvent(
					31600, dupre, 2,
					sifont),    // "Worry not about your friend Dupre.\nHe is
								// one with us, and content."
			ExSubEvent(
					39800, goodbye, 2,
					sifont),    // "Goodbye, Avatar.\nWe thank you."
			ExSubEvent(
					48900, well_well, 2,
					sifont),    // "Well well well, Avatar. You\nhave managed to
								// thwart me once again."
			ExSubEvent(
					55500, balance2, 3,
					sifont),    // "By restoring balance where\nonce chaos
								// reigned, you have\nsaved your accursed
								// world."
			ExSubEvent(
					62500, poised, 3,
					sifont),    // "But now here you are, poised\nat the edge of
								// Eternity.\nWhere would you go?"
			ExSubEvent(
					70250, brit_earth, 2,
					sifont),    // "Back to Britannia?\nTo Earth?"
			ExSubEvent(
					74750, pagan, 3,
					sifont),    // "Perhaps you would join me in\nanother world
								// alltogether?\nWe do have a score to settle!"
	};

	const int last_sub = subs.size() - 1;
	int       cur_sub  = -1;

	// Start the music
	audio->start_music(R_SEND, 0, false);

	const int start_time   = SDL_GetTicks();
	bool      showing_subs = false;

	while (true) {
		const uint32 time = SDL_GetTicks() - start_time;

		// Need to go to the next flic?
		if (cur_flic < last_flic && flics[cur_flic + 1].time <= time) {
			const bool next_play = flics[cur_flic + 1].can_play();
			// Can play the new one, don't need the old one anymore
			if (next_play) {
				// Free it
				if (flic) {
					flic->free_flic();
				}

				// Free the palette too, if we need to
				if (pal_flic && pal_flic != flic) {
					pal_flic->free_flic();
				}

				pal_flic = nullptr;
			}
			// Set palette to prev if required
			else if (flic && flic->can_play()) {
				pal_flic = flic;
			}
			// Previous flic didn't have a palette, so free it anyway
			else if (flic) {
				flic->free_flic();
			}

			cur_flic++;
			flic = std::addressof(flics[cur_flic]);

			if (next_play) {
				// Clear the screen to prevent palette corruption
				gwin->clear_screen(true);

				// Load the flic, and set pal_flic
				(pal_flic = flic)->load_flic();
			} else {
				COUT("Teminator ");
			}
			COUT("Flic at time: " << flic->time);
		}

		// Need to go to the next voc?
		if (cur_voc < last_voc && vocs[cur_voc + 1].time <= time) {
			cur_voc++;
			ExCineVoc& voc = vocs[cur_voc];

			// Just play it!
			voc.play_it(nullptr, time);
			// else COUT("Teminator ");
			COUT("voc at time: " << voc.time);
		}

		// Need to go to the next subtitle?
		if (cur_sub < last_sub && subs[cur_sub + 1].time <= time) {
			cur_sub++;
			if (subtitles) {
				showing_subs = true;
				COUT("Subtitle at time: " << subs[cur_sub].time);
			}
		}

		// We've finished
		if (cur_flic == last_flic && cur_voc == last_voc
			&& cur_sub == last_sub) {
			// Do a fade out
			if (pal_flic && pal_flic->can_play()) {
				pal_flic->fade_out(100);
			}

			COUT("Finished!" << std::endl);
			break;
		}

		// Play the flic if possible

		bool updated = false;

		if (flic && flic->can_play()) {
			updated = flic->play_it(win, time);
		}

		// Need to go to the next subtitle?
		if ((subtitles) && showing_subs && cur_sub <= last_sub) {
			updated = true;
			subs[cur_sub].show_sub(ibuf, centerx, centery);
		}

		if (updated) {
			win->ShowFillGuardBand();
		}

		if (wait_delay(0, 0, 1)) {
			// Do a quick fade out
			if (pal_flic && pal_flic->can_play()) {
				pal_flic->fade_out(20);
			}
			break;
		}
	}

	try {
		// Fade out for 1 sec (50 cycles)
		pal->set_brightness(80);    // Set readable brightness
		win->fill8(0);
		win->ShowFillGuardBand();

		// Congratulations screen
		// show only when finishing a game and not when viewed from menu
		if (within_game) {
			show_congratulations(pal);
		}
	} catch (const UserSkipException& /*x*/) {
	}

	gwin->clear_screen(true);

	// Stop all sounds
	audio->cancel_streams();
	audio->stop_music();
}

void SI_Game::show_quotes() {
	if (scene_available("quotes")) {
		Audio::get_ptr()->stop_music();
		// Play the custom quotes scene
		play_scene("quotes");
		return;
	}
	Audio::get_ptr()->start_music(
			32, false, MyMidiPlayer::Force_None, MAINSHP_FLX);
	TextScroller quotes(
			MAINSHP_FLX, 0x10, fontManager.get_font("MENU_FONT"),
			menushapes.extract_shape(0x14));
	quotes.run(gwin);
}

void SI_Game::show_credits() {
	if (scene_available("credits")) {
		Audio::get_ptr()->stop_music();
		// Play the custom credits scene
		play_scene("credits");
		U7open_out("<SAVEGAME>/quotes.flg");
		return;
	}
	pal->load(MAINSHP_FLX, PATCH_MAINSHP, 26);
	Audio::get_ptr()->start_music(
			30, false, MyMidiPlayer::Force_None, MAINSHP_FLX);
	TextScroller credits(
			MAINSHP_FLX, 0x0E, fontManager.get_font("MENU_FONT"),
			menushapes.extract_shape(0x14));
	if (credits.run(gwin)) {    // Watched through the entire sequence?
		U7open_out("<SAVEGAME>/quotes.flg");
	}
}

bool SI_Game::new_game(Vga_file& shapes) {
	const int             menuy = topy + 110;
	std::shared_ptr<Font> font  = fontManager.get_font("MENU_FONT");

	if (Mouse::mouse()
		&& !Mouse::use_touch_input) {    // If not primarily touch input
		Mouse::mouse()
				->show();    // Attempt to make the mouse visible initially
	}
	Vga_file faces_vga;
	faces_vga.load(FACES_VGA, PATCH_FACES);

	const int max_len = 16;
	char      npc_name[max_len + 1];
	char      disp_name[max_len + 2];
	npc_name[0] = 0;

	int       selected    = 0;
	const int num_choices = 4;
	bool      editing     = true;
	bool      redraw      = true;
	bool      ok          = true;

	// Skin info
	Avatar_default_skin* defskin  = Shapeinfo_lookup::GetDefaultAvSkin();
	Skin_data*           skindata = Shapeinfo_lookup::GetSkinInfoSafe(
            defskin->default_skin, defskin->default_female, true);
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
			sman->paint_shape(topx, topy, shapes.get_shape(0x2, 0));
			sman->paint_shape(
					topx + 10, menuy + 10,
					shapes.get_shape(0xC, selected == 0));
			sman->paint_shape(
					topx + 10, menuy + 25,
					shapes.get_shape(0x19, selected == 1));

			Shape_frame* portrait = faces_vga.get_shape(
					skindata->face_shape, skindata->face_frame);
			sman->paint_shape(topx + 300, menuy + 50, portrait);

			sman->paint_shape(
					topx + 10, topy + 180,
					shapes.get_shape(0x8, selected == 2));
			sman->paint_shape(
					centerx + 10, topy + 180,
					shapes.get_shape(0x7, selected == 3));
			if (selected == 0) {
				snprintf(disp_name, max_len + 2, "%s_", npc_name);
			} else {
				snprintf(disp_name, max_len + 2, "%s", npc_name);
			}
			font->draw_text(ibuf, topx + 60, menuy + 10, disp_name);
			if (Mouse::mouse()) {
				Mouse::mouse()->show();
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

			if (event.type == SDL_EVENT_MOUSE_MOTION && Mouse::mouse()) {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(
						event.motion.x, event.motion.y, gwin->get_fastmouse(),
						mx, my);
				Mouse::mouse()->hide();
				Mouse::mouse()->move(mx, my);
				Mouse::mouse_update = true;
				Mouse::mouse()->show();
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
								skindata, true, true);
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
								max_len);
						npc_name[max_len] = '\0';
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
						if (len < max_len) {
							npc_name[len]     = ' ';
							npc_name[len + 1] = 0;
						}
					} else if (selected == 1) {
						skindata = Shapeinfo_lookup::GetNextSelSkin(
								skindata, true, true);
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
								skindata, true, true);
					}
					break;
				case SDLK_RIGHT:
					if (selected == 1) {
						skindata = Shapeinfo_lookup::GetNextSelSkin(
								skindata, true, true);
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

						if (chr >= ' ' && len < max_len) {
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
		sman->paint_shape(topx, topy, shapes.get_shape(0x2, 0));
	}
	return ok;
}
