/**
 ** Gamewin.cc - X-windows Ultima7 map browser.
 **
 ** Written: 7/22/98 - JSF
 **/

/*
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "gamewin.h"

#include "Astar.h"
#include "Audio.h"
#include "AudioMixer.h"
#include "Configuration.h"
#include "Face_stats.h"
#include "Flex.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "ItemMenu_gump.h"
#include "Notebook_gump.h"
#include "ShortcutBar_gump.h"
#include "actions.h"
#include "animate.h"
#include "audio/midi_drivers/XMidiFile.h"
#include "barge.h"
#include "cheat.h"
#include "chunks.h"
#include "chunkter.h"
#include "combat.h"
#include "combat_opts.h"
#include "dir.h"
#include "drag.h"
#include "effects.h"
#include "egg.h"
#include "exult.h"
#include "files/U7file.h"
#include "flic/playfli.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamerend.h"
#include "items.h"
#include "keyactions.h"
#include "keys.h"
#include "mappatch.h"
#include "monsters.h"
#include "monstinf.h"
#include "mouse.h"
#include "naturallight.h"
#include "npcnear.h"
#include "objiter.h"
#include "palette.h"
#include "party.h"
#include "paths.h"
#include "schedule.h"
#include "spellbook.h"
#include "touchui.h"
#include "ucmachine.h"
#include "ucsched.h" /* Only used to flush objects. */
#include "usefuns.h"
#include "utils.h"
#include "version.h"
#include "virstone.h"

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>

#ifdef USE_EXULTSTUDIO
#	include "servemsg.h"
#	include "server.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::istream;
using std::make_unique;
using std::ofstream;
using std::rand;
using std::srand;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {

	class Strings {
	public:
		static auto DoesNotExist() {
			return get_text_msg(0x750 - msg_file_start);
		}

		static auto IsNotOnTheMap() {
			return get_text_msg(0x751 - msg_file_start);
		}

		static auto SearchingForShape() {
			return get_text_msg(0x752 - msg_file_start);
		}

		static auto NotFound() {
			return get_text_msg(0x753 - msg_file_start);
		}

		static auto SavingGame() {
			return get_text_msg(0x754 - msg_file_start);
		}

		static auto Oink() {
			return get_text_msg(0x6F0 - msg_file_start);
		}
	};

}    // namespace

// THE game window:
Game_window* Game_window::game_window = nullptr;

/*
 *  Provide chirping birds.
 */
class Background_noise : public Time_sensitive {
	int          repeats;       // Repeats in quick succession.
	int          last_sound;    // # of last sound played.
	Game_window* gwin;

	enum Song_states {
		Invalid,
		Outside,
		Dungeon,
		Nighttime,
		RainStorm,
		SnowStorm
	} laststate;    // Last state for SFX music tracks,

	// 1 outside, 2 dungeon, 3 nighttime, 4 rainstorm,
	// 5 snowstorm, 6 for danger nearby
public:
	Background_noise(Game_window* gw) : repeats(0), last_sound(-1), gwin(gw), laststate(Invalid) {
		gwin->get_tqueue()->add(5000, this);
	}

	~Background_noise() override {
		gwin->get_tqueue()->remove(this);
	}

	void handle_event(unsigned long curtime, uintptr udata) override;

	static bool is_combat_music(int num) {
		// Lumping music 16 as if it were a combat music in order to simplify
		// the check.
		return (num >= Audio::game_music(9) && num <= Audio::game_music(12))
			   || (num >= Audio::game_music(15) && num <= Audio::game_music(18));
	}
};

/*
 *  Play background sound.
 */

void Background_noise::handle_event(unsigned long curtime, uintptr udata) {
#ifndef COLOURLESS_REALLY_HATES_THE_BG_SFX

	unsigned long delay = 8000;
	Song_states   currentstate;
	const int     bghour    = gwin->get_clock()->get_hour();
	const int     weather   = gwin->get_effects()->get_weather();
	const bool    nighttime = bghour < 6 || bghour > 20;
	Combat_schedule::danger_music();
	if (gwin->is_in_dungeon()) {
		currentstate = Dungeon;
	} else if (weather == 2) {
		currentstate = RainStorm;
	} else if (weather == 1) {
		currentstate = SnowStorm;
	} else if (nighttime) {
		currentstate = Nighttime;
	} else {
		currentstate = Outside;
	}

	MyMidiPlayer* player = Audio::get_ptr()->get_midi();
	// The background sfx tracks only play for Digital Music, MT32emu,
	// MT32/FakeMT32 FMOpl is not sounding acceptable even though the original
	// used it.
	const bool play_bg_tracks = player && (player->get_ogg_enabled() || player->is_mt32());

	if (player) {
		delay                = 1000;    // Quickly get back to this function check
		const int curr_track = player->get_current_track();
		if ((curr_track == -1 || laststate != currentstate) && Audio::get_ptr()->is_music_enabled()) {
			// Conditions: not playing music, playing a background music
			if (curr_track == -1 || gwin->is_background_track(curr_track)
				|| ((currentstate == Dungeon) && !is_combat_music(curr_track))) {
				// Not already playing music
				int tracknum = 255;

				// Get the relevant track number.
				if (gwin->is_in_dungeon()) {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(52);
					}
					laststate = Dungeon;
				} else if (weather == 1) {    // Snowstorm
					if (play_bg_tracks) {
						tracknum = Audio::game_music(5);
					}
					laststate = SnowStorm;
				} else if (weather == 2) {    // Rainstorm
					if (play_bg_tracks) {
						tracknum = Audio::game_music(4);
					}
					laststate = RainStorm;
				} else if (nighttime) {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(7);
					}
					laststate = Nighttime;
				} else {
					if (play_bg_tracks) {
						tracknum = Audio::game_music(6);
					}
					laststate = Outside;
				}
				Audio::get_ptr()->start_music(tracknum, true);
			}
		}
	}

	// Tests to see if background sfx tracks are playing,
	// possible when the game has been restored
	// and the Audio option was changed from OGG/MT32 to something else
	if (player && !play_bg_tracks
		&& ((player->get_current_track() >= 4 && player->get_current_track() <= 7) || player->get_current_track() == 52)) {
		player->stop_music();
	}
	Main_actor* ava = gwin->get_main_actor();
	// Testing. When outside play birds during daytime or crickets at night
	if (ava && !gwin->is_main_actor_inside() && currentstate != Dungeon) {
		int                        sound     = -1;    // SFX #.
		static const unsigned char bgnight[] = {61, 61, 255};
		static const unsigned char bgday[]   = {82, 85, 85};
		if (repeats > 0) {    // Repeating?
			sound = last_sound;
		} else {
			if (nighttime) {
				sound = bgnight[rand() % sizeof(bgnight)];
				// only play daytime sfx when no music track is playing
			} else if (!play_bg_tracks && (!player || player->get_current_track() == -1)) {
				sound = bgday[rand() % sizeof(bgday)];
			}
			last_sound = sound;
		}
		if (sound >= 0 && sound != 255) {
			Audio::get_ptr()->play_sound_effect(Audio::game_sfx(sound), AUDIO_MAX_VOLUME - 64);
			repeats++;    // Count it.
			if (rand() % (repeats + 1) == 0) {
				// Repeat.
				delay = 500 + rand() % 1000;
			} else {
				delay   = 4000 + rand() % 3000;
				repeats = 0;
			}
		} else {
			repeats = 0;
		}
	}

	gwin->get_tqueue()->add(curtime + delay, this, udata);
#endif
}

/*
 *  Create game window.
 */

Game_window::Game_window(
		int width, int height, bool fullscreen, int gwidth, int gheight, int scale, int scaler, Image_window::FillMode fillmode,
		unsigned int fillsclr    // Window dimensions.
		)
		: dragging(nullptr), effects(new Effects_manager(this)), map(new Game_map(0)), render(new Game_render),
		  gump_man(new Gump_manager), party_man(new Party_manager), win(nullptr), npc_prox(new Npc_proximity_handler(this)),
		  pal(nullptr), tqueue(new Time_queue()), background_noise(new Background_noise(this)), usecode(nullptr), combat(false),
		  focus(true), ice_dungeon(false), painted(false), ambient_light(false), infravision_active(false), skip_above_actor(31),
		  in_dungeon(0), num_npcs1(0), std_delay(c_std_delay), time_stopped(0), special_light(0), theft_warnings(0), theft_cx(255),
		  theft_cy(255), moving_barge(nullptr), main_actor(nullptr), camera_actor(nullptr), npcs(0), bodies(0), scrolltx(0),
		  scrollty(0), dirty(0, 0, 0, 0), save_names{}, mouse3rd(false), fastmouse(false), double_click_closes_gumps(false),
		  text_bg(false), step_tile_delta(8), allow_right_pathfind(2), scroll_with_mouse(false), alternate_drop(false),
		  allow_autonotes(false), allow_enhancements(false), in_exult_menu(false), extended_intro(false), load_palette_timer(0),
		  plasma_start_color(0), plasma_cycle_range(0), skip_lift(255), paint_eggs(false), paint_egg_areas(0), armageddon(false),
		  walk_in_formation(false), debug(0), blits(0), scrolltx_l(0), scrollty_l(0), scrolltx_lp(0), scrollty_lp(0),
		  scrolltx_lo(0), scrollty_lo(0), avposx_ld(0), avposy_ld(0), lerping_enabled(0) {
	game_window = this;    // Set static ->.
	clock       = new Game_clock(tqueue);
	shape_man   = new Shape_manager();    // Create the single instance.
	maps.push_back(map);                  // Map #0.
	// Create window.
	win = new Image_window8(width, height, gwidth, gheight, scale, fullscreen, scaler, fillmode, fillsclr);
	win->set_title("Exult Ultima VII Engine");
	pal = new Palette();
	Game_singletons::init(this);    // Everything but 'usecode' exists.
	Shape_frame::set_to_render(win->get_ib8());

	string str;
	config->value("config/gameplay/textbackground", text_bg, -1);
	config->value("config/gameplay/mouse3rd", str, "no");
	if (str == "yes") {
		mouse3rd = true;
	}
	config->set("config/gameplay/mouse3rd", str, false);
	config->value("config/gameplay/fastmouse", str, "no");
	if (str == "yes") {
		fastmouse = true;
	}
	config->set("config/gameplay/fastmouse", str, false);
	config->value("config/gameplay/double_click_closes_gumps", str, "no");
	if (str == "yes") {
		double_click_closes_gumps = true;
	}
	config->set("config/gameplay/double_click_closes_gumps", str, false);
	config->value("config/gameplay/combat/difficulty", Combat::difficulty, 0);
	config->set("config/gameplay/combat/difficulty", Combat::difficulty, false);
	config->value("config/gameplay/combat/charmDifficulty", str, "normal");
	Combat::charmed_more_difficult = (str == "hard");
	config->set("config/gameplay/combat/charmDifficulty", str, false);
	config->value("config/gameplay/combat/mode", str, "original");
	if (str == "keypause") {
		Combat::mode = Combat::keypause;
	} else {
		Combat::mode = Combat::original;
	}
	config->set("config/gameplay/combat/mode", str, false);
	config->value("config/gameplay/combat/show_hits", str, "no");
	Combat::show_hits = (str == "yes");
	config->set("config/gameplay/combat/show_hits", str, false);
	config->value("config/audio/disablepause", str, "no");
	config->set("config/audio/disablepause", str, false);

	config->value("config/gameplay/step_tile_delta", step_tile_delta, 8);
	if (step_tile_delta < 1) {
		step_tile_delta = 1;
	}
	config->set("config/gameplay/step_tile_delta", step_tile_delta, false);

	config->value("config/gameplay/allow_right_pathfind", str, "double");
	if (str == "no") {
		allow_right_pathfind = 0;
	} else if (str == "single") {
		allow_right_pathfind = 1;
	}
	config->set("config/gameplay/allow_right_pathfind", str, false);

	// New 'formation' walking?
	config->value("config/gameplay/formation", str, "yes");
	// Assume "yes" on anything but "no".
	walk_in_formation = str != "no";
	config->set("config/gameplay/formation", walk_in_formation ? "yes" : "no", false);

	config->value("config/gameplay/smooth_scrolling", lerping_enabled, 0);
	config->set("config/gameplay/smooth_scrolling", lerping_enabled, false);
	config->value("config/gameplay/alternate_drop", str, "no");
	alternate_drop = str == "yes";
	config->set("config/gameplay/alternate_drop", alternate_drop ? "yes" : "no", false);
	config->value("config/gameplay/allow_autonotes", str, "no");
	allow_autonotes = str == "yes";
	config->set("config/gameplay/allow_autonotes", allow_autonotes ? "yes" : "no", false);
	config->value("config/gameplay/enhancements", str, "yes");
	allow_enhancements = str == "yes";
	config->set("config/gameplay/enhancements", allow_enhancements ? "yes" : "no", false);
	Shape_info::set_allow_enhancements(allow_enhancements);
	config->value("config/gameplay/natural_light", str, "no");
	natural_light = str != "no";
	config->set("config/gameplay/natural_light", natural_light ? "yes" : "no", false);
#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
	const string default_scroll_with_mouse = "no";
	const string default_item_menu         = "yes";
	const string default_dpad_location     = "right";
	const string default_touch_pathfind    = "yes";
	const string default_shortcutbar       = "translucent";
	const string default_outline           = "black";
#else
	const string default_scroll_with_mouse = "yes";
	const string default_item_menu         = "no";
	const string default_dpad_location     = "no";
	const string default_touch_pathfind    = "no";
	const string default_shortcutbar       = "no";
	const string default_outline           = "no";
#endif

	// scroll with mouse
	config->value("config/gameplay/scroll_with_mouse", str, default_scroll_with_mouse);
	scroll_with_mouse = str == "yes";
	config->set("config/gameplay/scroll_with_mouse", scroll_with_mouse ? "yes" : "no", false);

	// Item menu
	config->value("config/touch/item_menu", str, default_item_menu);
	item_menu = str == "yes";
	config->set("config/touch/item_menu", item_menu ? "yes" : "no", false);

	// DPad location
	config->value("config/touch/dpad_location", str, default_dpad_location);
	if (str == "no") {
		dpad_location = 0;
	} else if (str == "left") {
		dpad_location = 1;
	} else {
		str           = "right";
		dpad_location = 2;
	}
	config->set("config/touch/dpad_location", str, false);

	// Touch pathfind
	config->value("config/touch/touch_pathfind", str, default_touch_pathfind);
	touch_pathfind = str == "yes";
	config->set("config/touch/touch_pathfind", touch_pathfind ? "yes" : "no", false);

	// Shortcut bar
	config->value("config/shortcutbar/use_shortcutbar", str, default_shortcutbar);
	if (str == "no") {
		use_shortcutbar = 0;
	} else if (str == "yes") {
		use_shortcutbar = 2;
	} else {
		str             = "translucent";
		use_shortcutbar = 1;
	}
	config->set("config/shortcutbar/use_shortcutbar", str, false);

	config->value("config/shortcutbar/use_outline_color", str, default_outline);
	if (str == "no") {
		outline_color = NPIXCOLORS;
	} else if (str == "green") {
		outline_color = POISON_PIXEL;
	} else if (str == "white") {
		outline_color = PROTECT_PIXEL;
	} else if (str == "yellow") {
		outline_color = CURSED_PIXEL;
	} else if (str == "blue") {
		outline_color = CHARMED_PIXEL;
	} else if (str == "red") {
		outline_color = HIT_PIXEL;
	} else if (str == "purple") {
		outline_color = PARALYZE_PIXEL;
	} else {
		str           = "black";
		outline_color = BLACK_PIXEL;
	}
	config->set("config/shortcutbar/use_outline_color", str, false);

	config->value("config/shortcutbar/hide_missing_items", str, "yes");
	sb_hide_missing = str != "no";
	config->set("config/shortcutbar/hide_missing_items", sb_hide_missing ? "yes" : "no", false);
	config->write_back();

	// default to SI extended intro
	config->value("config/gameplay/extended_intro", str, "yes");
	extended_intro = str == "yes";
	config->set("config/gameplay/extended_intro", extended_intro ? "yes" : "no", false);
}

/*
 *  Blank out screen.
 */
void Game_window::clear_screen(bool update) {
	win->BeginPaintIntoGuardBand(nullptr, nullptr, nullptr, nullptr);
	win->fill8(0, win->get_full_width(), win->get_full_height(), win->get_start_x(), win->get_start_y());

	win->EndPaintIntoGuardBand();
	// update screen
	if (update) {
		show(true);
	}
}

/*
 *  Deleting game window.
 */

Game_window::~Game_window() {
	gump_man->close_all_gumps(true);
	clear_world(false);    // Delete all objects, chunks.
	delete shape_man;
	delete gump_man;
	delete party_man;
	delete background_noise;
	delete tqueue;
	tqueue = nullptr;
	// Free objects that may own layers living in 'win' before the
	// window itself (the drag state and text effects each own layers).
	delete dragging;
	dragging = nullptr;
	delete effects;
	effects = nullptr;
	delete win;
	delete pal;
	for (auto* map : maps) {
		if (map) {
			delete map;
		}
	}
	delete usecode;
	delete clock;
	delete npc_prox;
	delete render;
	// The single instance is gone; clear the static so get_instance() returns
	// null (e.g. Mouse's destructor checks this before touching the window).
	game_window = nullptr;
}

/*
 *  Redirect all shape/text drawing to a buffer (e.g. an overlay layer),
 *  returning the previous target. Both the window's active buffer and the
 *  static shape render target are switched so every drawing path follows.
 */

Image_buffer8* Game_window::push_render_target(Image_buffer8* buf) {
	Image_buffer8* prev = win->set_render_buffer(buf);
	Shape_frame::set_to_render(buf);
	return prev;
}

void Game_window::pop_render_target(Image_buffer8* prev) {
	win->set_render_buffer(prev);
	Shape_frame::set_to_render(prev);
}

/*
 *  Abort.
 */

void Game_window::abort(const char* msg, ...) {
	std::va_list ap;
	va_start(ap, msg);
	char buf[512];
	vsnprintf(buf, sizeof(buf), msg, ap);    // Format the message.
	cerr << "Exult (fatal): " << buf << endl;
	delete this;
	throw quit_exception(-1);
}

bool Game_window::is_background_track(int num) const {    // ripped out of Background_noise
	// Have to do it this way because of SI.
	return num == Audio::game_music(4) || num == Audio::game_music(5) || num == Audio::game_music(6) || num == Audio::game_music(7)
		   || num == Audio::game_music(8) || num == Audio::game_music(52);
}

void Game_window::init_files(bool cycle) {
	const uint32                      menu_mouse_only = 1u << static_cast<int>(Image_window::UiLayerMousePointer);
	Game_window::Scoped_ui_layer_mask load_layers(this, cycle ? 0u : menu_mouse_only);
	// Display red plasma during load...
	if (cycle) {
		setup_load_palette();
	}

	usecode = Usecode_machine::create();
	Game_singletons::init(this);    // Everything should exist here.

	cycle_load_palette();
	shape_man->load();    // All the .vga files!
	cycle_load_palette();

	const unsigned long timer = SDL_GetTicks();
	srand(timer);    // Use time to seed rand. generator.
	// Force clock to start.
	tqueue->add(timer, clock, this);
	// Go to starting chunk
	scrolltx_lp = scrolltx_l = scrolltx = game->get_start_tile_x();
	scrollty_lp = scrollty_l = scrollty = game->get_start_tile_y();
	scrolltx_lo = scrollty_lo = 0;
	avposx_ld = avposy_ld = 0;

	// initialize keybinder
	delete keybinder;
	keybinder = new KeyBinder();

	std::string d;
	std::string keyfilename;
	d = "config/disk/game/" + Game::get_gametitle() + "/keys";
	config->value(d.c_str(), keyfilename, "(default)");
	if (keyfilename == "(default)") {
		config->set(d.c_str(), keyfilename, true);
		keybinder->LoadDefaults();
	} else {
		try {
			keybinder->LoadFromFile(keyfilename.c_str());
		} catch (file_open_exception&) {
			cerr << "Key mappings file '" << keyfilename << "' not found, falling back to default mappings." << endl;
			keybinder->LoadDefaults();
		}
	}
	keybinder->LoadFromPatch();
	cycle_load_palette();

	int fps;    // Init. animation speed.
	config->value("config/video/fps", fps, 5);
	if (fps <= 0) {
		fps = 5;
	}
	std_delay = 1000 / fps;    // Convert to msecs. between frames.
}

/*
 *  Read any map. (This is for "multimap" games, not U7.)
 */

Game_map* Game_window::get_map(int num    // Should be > 0.
) {
	if (num >= static_cast<int>(maps.size())) {
		maps.resize(num + 1);
	}
	if (maps[num] == nullptr) {
		auto* newmap = new Game_map(num);
		maps[num]    = newmap;
		maps[num]->init();
	}
	return maps[num];
}

/*
 *  Set current map to given #.
 */

void Game_window::set_map(int num) {
	map = get_map(num);
	if (!map) {
		abort("Map %d doesn't exist", num);
	}
	Game_singletons::gmap = map;
}

/*
 *  Get map patch list.
 */
Map_patch_collection& Game_window::get_map_patches() {
	return map->get_map_patches();
}

/*
 *  Set/unset barge mode.
 */

void Game_window::set_moving_barge(Barge_object* b) {
	if (b && b != moving_barge) {
		b->gather();    // Will 'gather' on next move.
		if (!b->contains(main_actor)) {
			b->set_to_gather();
		}
		landing_barge = nullptr;    // Boarding cancels any pending landing glide.
	} else if (!b && moving_barge) {
		moving_barge->done();    // No longer 'barging'.
		// Smooth scroll: prevent jerkiness on offboarding, especially the flying carpet.
		scrolltx_lp = scrolltx_l = scrolltx;
		scrollty_lp = scrollty_l = scrollty;
		landing_barge            = moving_barge;
	}
	moving_barge = b;
}

/*
 *  Is character moving?
 */

bool Game_window::is_moving() const {
	return moving_barge ? moving_barge->is_moving() : main_actor->is_moving();
}

/*
 *  Are we in dont_move mode?
 */

bool Game_window::main_actor_dont_move() const {
	return !cheat.in_map_editor() && main_actor != nullptr &&    // Not if map-editing.
		   ((main_actor->get_flag(Obj_flags::dont_move)) || (main_actor->get_flag(Obj_flags::dont_render)));
}

/*
 *  Are we in dont_move mode?
 */

bool Game_window::main_actor_can_act() const {
	return main_actor->can_act();
}

bool Game_window::main_actor_can_act_charmed() const {
	return main_actor->can_act_charmed();
}

bool Game_window::in_infravision() const {
	return infravision_active || cheat.in_infravision();
}

/*
 *  Add time for a light spell.
 */

void Game_window::add_special_light(int units    // Light=500, GreatLight=5000.
) {
	if (!special_light) {    // Nothing in effect now?
		special_light = clock->get_total_minutes();
		clock->set_palette();
	}
	special_light += units / 20;    // Figure ending time.
}

/*
 *  Set 'stop time' value.
 */

void Game_window::set_time_stopped(long delay    // Delay in ticks (1/1000
												 // secs.), -1 to stop
												 // indefinitely, or 0 to end.
) {
	if (delay == -1) {
		time_stopped = -1;
	} else if (!delay) {
		time_stopped = 0;
	} else {
		const long new_expire = Game::get_ticks() + delay;
		if (new_expire > time_stopped) {    // Set expiration time.
			time_stopped = new_expire;
		}
	}
}

/*
 *  Return delay to expiration (or 3000 if indefinite).
 */

long Game_window::check_time_stopped() {
	if (time_stopped == -1) {
		return 3000;
	}
	const long delay = time_stopped - Game::get_ticks();
	if (delay > 0) {
		return delay;
	}
	time_stopped = 0;    // Done.
	return 0;
}

/*
 *  Toggle combat mode.
 */

void Game_window::toggle_combat() {
	combat = !combat;
	// Change party member's schedules.
	int       newsched = combat ? Schedule::combat : Schedule::follow_avatar;
	const int cnt      = party_man->get_count();
	for (int i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		if (!person) {
			continue;
		}
		if (!person->can_act_charmed()) {
			newsched = Schedule::combat;
		}
		const int sched = person->get_schedule_type();
		if (sched != newsched && sched != Schedule::wait && sched != Schedule::loiter) {
			person->set_schedule_type(newsched);
		}
	}
	if (!main_actor_can_act_charmed()) {
		newsched = Schedule::combat;
	}
	if (main_actor->get_schedule_type() != newsched) {
		main_actor->set_schedule_type(newsched);
	}
	if (combat) {    // Get rid of flee modes.
		main_actor->ready_best_weapon();
		set_moving_barge(nullptr);    // And get out of barge mode.
		Actor*    all[9];
		const int cnt = get_party(all, 1);
		for (int i = 0; i < cnt; i++) {
			// Did Usecode set to flee?
			Actor* act = all[i];
			if (act->get_attack_mode() == Actor::flee && !act->did_user_set_attack()) {
				act->set_attack_mode(Actor::nearest);
			}
			// And avoid attacking party members,
			//  in case of Usecode bug.
			const Game_object* targ = act->get_target();
			if (targ && targ->get_flag(Obj_flags::in_party)) {
				act->set_target(nullptr);
			}
			if (touchui != nullptr && Combat::mode != Combat::original) {
				touchui->showPauseControls();
			}
		}
	} else {                 // Ending combat.
		Combat::resume();    // Make sure not still paused.
		if (touchui != nullptr && Combat::mode != Combat::original) {
			touchui->hidePauseControls();
		}
	}
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
}

/*
 *  Add an NPC.
 */

void Game_window::add_npc(
		Actor* npc,
		int    num    // Number. Has to match npc->num.
) {
	assert(num == npc->get_npc_num());
	assert(num <= static_cast<int>(npcs.size()));
	if (num == static_cast<int>(npcs.size())) {    // Add at end.
		npcs.push_back(std::static_pointer_cast<Actor>(npc->shared_from_this()));
	} else {
		// Better be unused.
		assert(!npcs[num] || npcs[num]->is_unused());
		npcs[num] = std::static_pointer_cast<Actor>(npc->shared_from_this());
	}
}

/*  Get desired NPD.
 */
Actor* Game_window::get_npc(long npc_num) const {
	if (npc_num < 0 || npc_num >= static_cast<int>(npcs.size())) {
		return nullptr;
	} else {
		return npcs[npc_num].get();
	}
}

/*
 *  Show desired NPC.
 */

void Game_window::locate_npc(int npc_num) {
	std::ostringstream msg;
	Actor*             npc = get_npc(npc_num);
	if (!npc) {
		msg << "NPC " << npc_num << Strings::DoesNotExist();
		effects->center_text(msg.str().c_str());
	} else if (npc->is_pos_invalid()) {
		msg << "NPC " << npc_num << Strings::IsNotOnTheMap();
		effects->center_text(msg.str().c_str());
	} else {
		// ++++WHAT IF on a different map???
		const Tile_coord pos = npc->get_tile();
		center_view(pos);
		cheat.clear_selected();
		cheat.append_selected(npc);
		msg << "NPC " << npc_num << ": '" << npc->get_npc_name() << "'.";
		const int above = pos.tz + npc->get_info().get_3d_height() - 1;
		if (skip_above_actor > above) {
			set_above_main_actor(above);
		}
		effects->center_text(msg.str().c_str());
	}
}

/*
 *  Find first unused NPC #.
 */

int Game_window::get_unused_npc() {
	int cnt = npcs.size();    // Get # in list.
	int i   = 0;
	for (; i < cnt; i++) {
		if (i >= 356 && i <= 359) {
			continue;    // Never return these.
		}
		if (!npcs[i] || npcs[i]->is_unused()) {
			return i;
		}
	}
	if (i >= 356 && i <= 359) {
		// Special, 'reserved' cases.
		i = 360;
		do {
			npcs.push_back(std::make_shared<Npc_actor>("Reserved", 0));
			auto& npc = npcs[cnt];
			npc->set_schedule_type(Schedule::wait);
			npc->set_unused(true);
		} while (++cnt < 360);
	}
	return i;    // First free one is past the end.
}

/*
 *  Resize event occurred.
 */

void Game_window::resized(
		unsigned int neww, unsigned int newh, bool newfs, unsigned int newgw, unsigned int newgh, unsigned int newsc,
		unsigned int newsclr, Image_window::FillMode newfill, unsigned int newfillsclr) {
	win->resized(neww, newh, newfs, newgw, newgh, newsc, newsclr, newfill, newfillsclr);
	pal->apply(false);
	Shape_frame::set_to_render(win->get_ib8());
	if (!main_actor) {    // In case we're before start.
		return;
	}
	center_view(main_actor->get_tile());
	paint();
	// Do the following only if in game (not for menus)
	if (!gump_man->gump_mode()) {
		char msg[80];
		snprintf(msg, sizeof(msg), "%ux%ux%u", neww, newh, newsc);
		effects->center_text(msg);
	}
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	Face_stats::UpdateButtons();
}

/*
 *  Clear out world's contents. Should be used during a 'restore'.
 */

void Game_window::clear_world(bool restoremapedit) {
	const bool edit = cheat.in_map_editor();
	cheat.set_map_editor(false);
	Combat::resume();
	tqueue->clear();    // Remove all entries.
	clear_dirty();
	Usecode_script::clear();    // Clear out all scheduled usecode.
	// Most NPCs were deleted when the map is cleared; we have to deal with some
	// stragglers.
	for (auto& npc : npcs) {
		if (npc && npc->is_unused()) {
			npc.reset();
		}
	}
	for (auto* map : maps) {
		if (map) {
			map->clear();
		}
	}
	try {
		set_map(0);    // Back to main map.
	} catch (const quit_exception& /*f*/) {
		// Lets not allow this to go up.
	}
	Monster_actor::delete_all();    // To be safe, del. any still around.
	Notebook_gump::clear();
	main_actor   = nullptr;
	camera_actor = nullptr;
	num_npcs1    = 0;
	theft_cx = theft_cy = -1;
	combat              = false;
	npcs.resize(0);    // NPC's already deleted above.
	bodies.resize(0);
	moving_barge       = nullptr;    // Get out of barge mode.
	landing_barge      = nullptr;
	special_light      = 0;        // Clear out light spells.
	ambient_light      = false;    // And ambient lighting.
	infravision_active = false;
	effects->remove_all_effects(false);
	Schedule_change::clear();
	cheat.set_map_editor(edit && restoremapedit);
}

/*
 *  Look throughout the map for a given shape. The search starts at
 *  the first currently-selected shape, if possible.
 *
 *  Output: true if found, else 0.
 */

bool Game_window::locate_shape(
		int  shapenum,    // Desired shape.
		bool upwards,     // If true, search upwards.
		int  frnum,       // Frame.
		int  qual         // Quality/quantity.
) {
	// Get (first) selected object.
	const std::vector<Game_object_shared>& sel   = cheat.get_selected();
	Game_object*                           start = !sel.empty() ? (sel[0]).get() : nullptr;
	std::ostringstream                     msg;
	msg << Strings::SearchingForShape() << shapenum;
	effects->center_text(msg.str().c_str());
	paint();
	show();
	Game_object* obj = map->locate_shape(shapenum, upwards, start, frnum, qual);
	if (!obj) {
		effects->center_text(Strings::NotFound());
		return false;    // Not found.
	}
	effects->remove_text_effects();
	cheat.clear_selected();    // Select obj.
	cheat.append_selected(obj);
	//++++++++Got to show it.
	Game_object* owner = obj->get_outermost();    //+++++TESTING
	if (owner != obj) {
		cheat.append_selected(owner);
	}
	center_view(owner->get_tile());
	return true;
}

/*
 *  Set location to ExultStudio.
 */

inline void Send_location(Game_window* gwin) {
#ifdef USE_EXULTSTUDIO
	if (client_socket >= 0 &&    // Talking to ExultStudio?
		cheat.in_map_editor()) {
		unsigned char  data[50];
		unsigned char* ptr = &data[0];
		little_endian::Write4(ptr, gwin->get_scrolltx());
		little_endian::Write4(ptr, gwin->get_scrollty());
		little_endian::Write4(ptr, gwin->get_width() / c_tilesize);
		little_endian::Write4(ptr, gwin->get_height() / c_tilesize);
		little_endian::Write4(ptr, gwin->get_win()->get_scale_factor());
		Exult_server::Send_data(client_socket, Exult_server::view_pos, &data[0], ptr - data);
	}
#else
	ignore_unused_variable_warning(gwin);
#endif
}

/*
 *  Send current location to ExultStudio.
 */

void Game_window::send_location() {
	Send_location(this);
}

bool Game_window::rotatecolours() {
	Uint32        ticks       = SDL_GetTicks();
	static Uint32 last_rotate = 0;
	// Rotate less often if scaling and
	//   not paletized.
	const int rot_speed = 100 << (win->fast_palette_rotate() ? 0 : 1);

	if (ticks > last_rotate + rot_speed) {
		// (Blits in simulated 8-bit mode.)
		win->rotate_colors(0xfc, 3, 0);
		win->rotate_colors(0xf8, 4, 0);
		win->rotate_colors(0xf4, 4, 0);
		win->rotate_colors(0xf0, 4, 0);
		win->rotate_colors(0xe8, 8, 0);
		win->rotate_colors(0xe0, 8, 1);
		while (ticks > last_rotate + rot_speed) {
			last_rotate += rot_speed;
		}
		// Non palettized needs explicit blit.
		if (!win->is_palettized()) {
			set_painted();
		}
		return true;
	}
	return false;
}

/*
 *  Set the scroll position to a given tile.
 */

void Game_window::set_scrolls(int newscrolltx, int newscrollty) {
	scrolltx = newscrolltx;
	scrollty = newscrollty;
	// Set scroll box.
	// Let's try 2x2 tiles.
	scroll_bounds.w = scroll_bounds.h = 2;
	scroll_bounds.x                   = scrolltx + (get_width() / c_tilesize - scroll_bounds.w) / 2;
	// OFFSET HERE
	scroll_bounds.y = scrollty + ((get_height()) / c_tilesize - scroll_bounds.h) / 2;

	Barge_object* old_active_barge = moving_barge;
	map->read_map_data();    // This pulls in objects.
	// Found active barge?
	if (!old_active_barge && moving_barge) {
		// Do it right.
		Barge_object* b = moving_barge;
		moving_barge    = nullptr;
		set_moving_barge(b);
	}
	// Set where to skip rendering.
	const int  cx    = camera_actor->get_cx();
	const int  cy    = camera_actor->get_cy();
	Map_chunk* nlist = map->get_chunk(cx, cy);
	nlist->setup_cache();
	const int tx = camera_actor->get_tx();
	const int ty = camera_actor->get_ty();
	set_above_main_actor(nlist->is_roof(tx, ty, camera_actor->get_lift()));
	set_in_dungeon(nlist->has_dungeon() ? nlist->is_dungeon(tx, ty) : 0);
	set_ice_dungeon(nlist->is_ice_dungeon(tx, ty));
	send_location();    // Tell ExultStudio.
}

/*
 *  Set the scroll position so that a given tile is centered. (Used by
 *  center_view.)
 */

void Game_window::set_scrolls(Tile_coord cent    // Want center here.
) {
	// Figure in tiles.
	// OFFSET HERE
	const int tw = get_width() / c_tilesize;
	const int th = (get_height()) / c_tilesize;
	set_scrolls(DECR_TILE(cent.tx, tw / 2), DECR_TILE(cent.ty, th / 2));
}

/*
 *  Center view around a given tile. This is called during a 'restore'
 *  to init. stuff as well.
 */

void Game_window::center_view(const Tile_coord& t) {
	set_scrolls(t);
	set_all_dirty();
}

/*
 *  Set actor to center view around.
 */

void Game_window::set_camera_actor(Actor* a) {
	if (a == main_actor &&    // Setting back to main actor?
		camera_actor &&       // Change in chunk?
		(camera_actor->get_cx() != main_actor->get_cx() || camera_actor->get_cy() != main_actor->get_cy())) {
		// Cache out temp. objects.
		emulate_cache(camera_actor->get_chunk(), main_actor->get_chunk());
	}
	camera_actor       = a;
	const Tile_coord t = a->get_tile();
	set_scrolls(t);    // Set scrolling around position,
	// and read in map there.
	set_all_dirty();
}

/*
 *  Scroll if necessary.
 *
 *  Output: 1 if scrolled (screen updated).
 */

bool Game_window::scroll_if_needed(Tile_coord t) {
	if (suppress_barge_recenter) {
		// Change lift of a barge without recentering the view,
		// afterwards the next move recenters.
		return false;
	}
	bool scrolled = false;
	// 1 lift = 1/2 tile.
	const int tx = DECR_TILE(t.tx, t.tz / 2);
	const int ty = DECR_TILE(t.ty, t.tz / 2);
	if (Tile_coord::gte(DECR_TILE(scroll_bounds.x), tx)) {
		view_left();
		scrolled = true;
	} else if (Tile_coord::gte(tx, (scroll_bounds.x + scroll_bounds.w) % c_num_tiles)) {
		view_right();
		scrolled = true;
	}
	if (Tile_coord::gte(DECR_TILE(scroll_bounds.y), ty)) {
		view_up();
		scrolled = true;
	} else if (Tile_coord::gte(ty, (scroll_bounds.y + scroll_bounds.h) % c_num_tiles)) {
		view_down();
		scrolled = true;
	}
	return scrolled;
}

/*
 *  Change lift of a barge without recentering the view.
 */

void Game_window::barge_lift_move(Game_object* obj, const Tile_coord& t) {
	if (!obj->as_barge()) {
		obj->move(t);
		return;
	}
	suppress_barge_recenter = true;
	obj->move(t);
	suppress_barge_recenter = false;
}

/*
 *  Show the absolute game location, where each coordinate is of the
 *  8x8 tiles clicked on.
 */

void Game_window::show_game_location(
		int x, int y    // Point on screen.
) {
	x = get_scrolltx() + x / c_tilesize;
	y = get_scrollty() + y / c_tilesize;
	cout << "Game location is (" << x << ", " << y << ")" << endl;
}

/*
 *  Get screen area used by object.
 */

TileRect Game_window::get_shape_rect(const Game_object* obj) const {
	if (!obj->get_chunk()) {    // Not on map?
		Gump* gump = gump_man->find_gump(obj);
		if (gump) {
			return gump->get_shape_rect(obj);
		} else {
			return TileRect(0, 0, 0, 0);
		}
	}
	Shape_frame* s = obj->get_shape();
	if (!s) {
		// This is probably fatal.
#ifdef DEBUG
		std::cerr << "DEATH! get_shape() returned a nullptr pointer: " << __FILE__ << ":" << __LINE__ << std::endl;
		std::cerr << "Betcha it's a little doggie." << std::endl;
#endif
		return TileRect(0, 0, 0, 0);
	}
	Tile_coord t      = obj->get_tile();    // Get tile coords.
	const int  lftpix = (c_tilesize * t.tz) / 2;
	t.tx += 1 - get_scrolltx();
	t.ty += 1 - get_scrollty();
	// Watch for wrapping.
	if (t.tx < -c_num_tiles / 2) {
		t.tx += c_num_tiles;
	}
	if (t.ty < -c_num_tiles / 2) {
		t.ty += c_num_tiles;
	}
	return get_shape_rect(s, t.tx * c_tilesize - 1 - lftpix, t.ty * c_tilesize - 1 - lftpix);
}

/*
 *  Get screen location of given tile.
 */

inline void Get_shape_location(Tile_coord t, int scrolltx, int scrollty, int& x, int& y) {
	const int lft = 4 * t.tz;
	t.tx += 1 - scrolltx;
	t.ty += 1 - scrollty;
	// Watch for wrapping.
	if (t.tx < -c_num_tiles / 2) {
		t.tx += c_num_tiles;
	}
	if (t.ty < -c_num_tiles / 2) {
		t.ty += c_num_tiles;
	}
	x = t.tx * c_tilesize - 1 - lft;
	y = t.ty * c_tilesize - 1 - lft;
}

/*
 *  Get screen loc. of object which MUST be on the map (no owner).
 */

void Game_window::get_shape_location(const Game_object* obj, int& x, int& y) {
	const Tile_coord t = obj->get_tile();
	Get_shape_location(t, scrolltx, scrollty, x, y);
	bool apply_avpos = false;
	// Smooth scroll barges but only if the Avatar is on board,
	// otherwise the barge could be jerking during automatic boarding.
	if (moving_barge && moving_barge->contains(get_main_actor())) {
		if (moving_barge->is_grouped_member(obj)) {
			apply_avpos = true;
		}
	} else {
		// Smooth scroll the avatar
		const Actor* actor = obj->as_actor();
		if (obj == get_camera_actor()) {
			apply_avpos = true;
		} else if (actor && actor->is_in_party() && lerping_enabled) {
			// Apply the same lerping offset to party members
			apply_avpos = true;
		} else if (landing_barge && lerping_enabled && landing_barge->is_grouped_member(obj)) {
			// Keep party and barge in sync during landing.
			apply_avpos = true;
		}
	}
	if (apply_avpos) {
		x += avposx_ld;
		y += avposy_ld;
	}
	x -= scrolltx_lo;
	y -= scrollty_lo;
}

void Game_window::get_shape_location(const Tile_coord& t, int& x, int& y) {
	Get_shape_location(t, scrolltx, scrollty, x, y);
	x -= scrolltx_lo;
	y -= scrollty_lo;
}

/*
 *  Rebuild the spatial-light overlay layers from the per-frame light_renders
 *  list (populated by the renderer for every visible, unblocked light source).
 *
 *  Each brightness tier (0 = candle, 1 = single light, 2 = many lights) gets one
 *  game-area overlay layer: a copy of the just-painted (dark, time-of-day) world
 *  image, drawn through that tier's brighter fixed UI palette and masked by a
 *  per-pixel radial alpha (coverage) that fades smoothly from full at the source
 *  to zero at the radius edge. Composited over the dark base world this lights
 *  only the area under each source, leaving the global palette untouched.
 */

/*
 *  Prepare the global roof-pixel mask for a world render. Only active when a
 *  night/dawn/dusk palette is in effect (spatial lights only brighten then);
 *  at full day it stays inactive and costs nothing. The mask mirrors the
 *  world ibuf so build_light_layers can test roof coverage per pixel.
 */

void Game_window::begin_roof_mask() {
	roof_light_mask_active = false;
	if (!natural_light) {
		return;
	}
	NaturalLight::Flood_cache_frame_begin();
	const int palnum = pal ? pal->get_palette_number() : PALETTE_DAY;
	if (palnum == PALETTE_DAY) {
		return;    // No spatial lighting at full day: no roof mask needed.
	}
	Image_buffer8* ib = win ? win->get_ib8() : nullptr;
	if (!ib) {
		return;
	}
	const int bw = static_cast<int>(ib->get_width());
	const int bh = static_cast<int>(ib->get_height());
	if (!roof_light_mask || static_cast<int>(roof_light_mask->get_width()) != bw
		|| static_cast<int>(roof_light_mask->get_height()) != bh) {
		roof_light_mask = std::make_unique<Image_buffer8>(bw, bh);
	}
	roof_light_mask->fill8(0);    // 0 = not a roof pixel.
	roof_light_mask_active = true;
}

/*
 *  Update the global roof mask as one object is painted.  Objects paint
 *  back-to-front, so the topmost shape at each pixel wins: a roof shape -- or
 *  anything standing at roof level (lift >= 5), which is just as far above an
 *  interior light -- sets its pixels (kept dark under lights), and any ground
 *  shape painted over them clears them (e.g. a tree overlapping a roof stays
 *  lit). The final mask therefore marks exactly the pixels where the visible
 *  shape sits at or above the roof.
 */

void Game_window::update_roof_mask(Game_object* obj, int sx, int sy) {
	if (!roof_light_mask_active || !roof_light_mask || !obj) {
		return;
	}
	Shape_frame* frame = obj->get_shape();
	if (!frame || !frame->is_rle()) {
		return;
	}
	// Only pixels a light splat actually samples matter: skip any object whose
	// sprite touches none of last frame's splat rectangles (build_light_layers
	// records them, grown by a margin).  With no lights in view the whole mask
	// paint is skipped.  A newly appearing light or a view jump self-corrects
	// on the next frame.
	{
		const TileRect box(sx - frame->get_xleft(), sy - frame->get_yabove(), frame->get_width(), frame->get_height());
		bool           touched = false;
		for (const TileRect& r : light_mask_rects) {
			if (box.intersects(r)) {
				touched = true;
				break;
			}
		}
		if (!touched) {
			return;
		}
	}
	// Mask values: 255 = roof, 128 + storey = tall / upper-storey shape,
	// 0 = clear.  How each light kind treats them is decided in
	// Splat_radial_light's pixel loop.
	static const Xform_palette roof_set = [] {
		Xform_palette x;
		for (int i = 0; i < 256; ++i) {
			x.colors[i] = 255;
		}
		return x;
	}();
	static const std::array<Xform_palette, 4> roof_tall = [] {
		std::array<Xform_palette, 4> xs{};
		for (int s = 0; s < 4; ++s) {
			for (int i = 0; i < 256; ++i) {
				xs[s].colors[i] = static_cast<unsigned char>(128 + s);
			}
		}
		return xs;
	}();
	static const Xform_palette roof_clear = [] {
		Xform_palette x;
		for (int i = 0; i < 256; ++i) {
			x.colors[i] = 0;
		}
		return x;
	}();
	// An object standing ON a roof must stay as dark as the roof under it.
	// "Roof level" is the render-skip lift while inside (NOT a hardcoded z 5:
	// a tall room's visible upper wall segments are walls, not roofs) and the
	// classic z 5 while outside.  Inside, an is_roof() shape only counts when
	// ABOVE the Avatar: some interiors use roof shapes as flooring.  Ground
	// objects merely drawn in front of a roof still clear their pixels.
	const bool inside = is_main_actor_inside();
	// True when nothing is drawn above this object's top -- it stands under
	// open sky, so a spill / exterior light may treat it as a whole unit.
	auto open_sky_above = [&](int top) {
		const Tile_coord t     = obj->get_tile();
		Map_chunk* const chunk = map->get_chunk_safely(t.tx / c_tiles_per_chunk, t.ty / c_tiles_per_chunk);
		return chunk != nullptr && chunk->get_lowest_blocked(top, t.tx % c_tiles_per_chunk, t.ty % c_tiles_per_chunk) < 0;
	};
	bool roof_like;
	bool tall_exterior = false;
	// Storey a tall-exterior mark belongs to (mask value 128 + storey): a
	// spill lights it only from the same or a higher storey.
	auto storey_of = [](int lift) {
		const int s = lift / 5;
		return s < 0 ? 0 : (s > 3 ? 3 : s);
	};
	int tall_storey = 0;
	if (obj->as_actor() != nullptr) {
		// Actors are never marked as a real roof (255).  On an open-sky deck
		// mark 128 like the deck objects around them (dark under the room
		// below, lit whole by spills / exterior / carried light); at ground
		// level or under an interior roof stay clear.
		const int top         = obj->get_lift() + obj->get_info().get_3d_height();
		const int floor_level = inside ? get_render_skip_lift() : 5;
		if (obj->get_lift() >= floor_level && open_sky_above(top)) {
			roof_like     = true;
			tall_exterior = true;
		} else if (inside && obj->get_lift() >= 5) {
			// An actor on an upper storey below the render skip: mark
			// 128 + storey like the walls and furniture up there.
			roof_like     = true;
			tall_exterior = true;
			tall_storey   = storey_of(obj->get_lift());
		} else {
			roof_like = false;
		}
	} else if (obj->get_info().is_floor()) {
		// A floor slab used as the storey below's ceiling: mark its flat TOP
		// 128 + storey (not 255) so a deck-level spill still lights it while
		// a ground window's glow is storey-gated off.  Inside, the slab's
		// front-facing thickness strip (bottom / right 4px-per-z) is left as
		// painted -- marking it would draw a dark seam along the adjoining
		// wall.  Outside, the whole sprite first gets a plain 128 face mark,
		// or the room's veto light shows as a bright beam over the strip.
		const int zs         = obj->get_info().get_3d_height();
		const int top_storey = storey_of(obj->get_lift() + zs);
		const int strip      = 4 * zs;
		// Only an ELEVATED slab (top reaching z 5+) is a storey's ceiling /
		// deck.  Ground-level flooring (a stone floor, a paved yard) is just
		// floor: clear its pixels so the room's light brushes it normally.
		if (obj->get_lift() + zs < 5) {
			frame->paint_rle_transformed(roof_light_mask.get(), sx, sy, roof_clear);
			return;
		}
		if (!inside) {
			frame->paint_rle_transformed(roof_light_mask.get(), sx, sy, roof_tall[0]);
		}
		Image_buffer::ClipRectSave clipsave(roof_light_mask.get());
		const TileRect             top_rect(
                sx - frame->get_xleft(), sy - frame->get_yabove(), frame->get_width() - strip, frame->get_height() - strip);
		const TileRect r = top_rect.intersect(clipsave.Rect());
		if (r.w > 0 && r.h > 0) {
			roof_light_mask->set_clip(r.x, r.y, r.w, r.h);
			frame->paint_rle_transformed(roof_light_mask.get(), sx, sy, roof_tall[top_storey]);
		}
		return;
	} else if (inside) {
		const int  av_lift       = main_actor ? main_actor->get_lift() : 0;
		const bool is_roof_shape = obj->get_info().is_roof() && obj->get_lift() > av_lift;
		roof_like                = is_roof_shape || obj->get_lift() >= get_render_skip_lift();
		const int top            = obj->get_lift() + obj->get_info().get_3d_height();
		if (roof_like && !is_roof_shape && open_sky_above(top)) {
			// An object standing on an open-sky deck (castle battlements):
			// plain 128, so a spill from any storey lights its face (only the
			// flat slab TOP is storey-gated, see the is_floor branch).
			tall_exterior = true;
		} else if (!roof_like && top > get_render_skip_lift()) {
			// A grounded shape rising above the room's ceiling can only be an
			// exterior one (an outside tree); confirm open sky above its top
			// so nothing indoors is caught here.
			if (open_sky_above(top)) {
				tall_exterior = true;
			} else if (obj->get_lift() >= 5) {
				// Blocked above and starting at an upper storey: a
				// second-storey wall segment or upstairs furniture.
				tall_exterior = true;
				tall_storey   = storey_of(obj->get_lift());
			}
		} else if (!roof_like && obj->get_lift() >= 5) {
			// Upper-storey walls / furnishings below the render skip: mark
			// 128 + storey so z-blind lights from lower floors stay off them
			// while their own storey's lights still reach them.
			tall_exterior = true;
			tall_storey   = storey_of(obj->get_lift());
		}
	} else {
		const bool is_roof_shape = obj->get_info().is_roof();
		roof_like                = is_roof_shape || obj->get_lift() >= 5;
		const int top            = obj->get_lift() + obj->get_info().get_3d_height();
		if (roof_like && !is_roof_shape && open_sky_above(top)) {
			// Object on an open-sky deck: plain 128, as in the inside branch.
			tall_exterior = true;
		} else if (roof_like && !is_roof_shape) {
			// A second storey's wall / furnishing seen from outside: mark
			// 128 + storey so only its own storey's lights and spills reach it.
			tall_exterior = true;
			tall_storey   = storey_of(obj->get_lift());
		} else if (!roof_like && top >= 5) {
			// A grounded shape tall enough to reach roof level (tree,
			// lamppost): under open sky mark it as a whole unit so masked
			// lights don't cut the canopy into lit and dark patches; under
			// cover leave the mask as the shapes beneath painted it.
			if (open_sky_above(top)) {
				tall_exterior = true;
			} else {
				return;
			}
		}
	}
	frame->paint_rle_transformed(
			roof_light_mask.get(), sx, sy, tall_exterior ? roof_tall[tall_storey] : (roof_like ? roof_set : roof_clear));
}

/*
 *  Destroy the natural-light overlay layers and drop any pending
 *  per-frame light state.
 */
void Game_window::destroy_light_layers() {
	for (int& handle : light_layer_handles) {
		if (handle >= 0) {
			win->destroy_layer(handle);
			handle = -1;
		}
	}
	light_layer_w      = -1;
	light_layer_h      = -1;
	light_layer_palnum = -2;
	light_renders.clear();
	light_mask_rects.clear();
	light_scratch_cov.clear();
	for (int t = 0; t < 3; ++t) {
		light_tier_cov[t].clear();
		light_tier_rects[t].clear();
		light_tier_mask_rects[t].clear();
		light_tier_bounds[t].clear();
		light_tier_anchor_x[t] = 0;
		light_tier_anchor_y[t] = 0;
		light_tier_sig[t]      = 0;
		light_tier_mov_cov[t].clear();
		light_tier_mov_bounds[t].clear();
		light_tier_mov_sig[t] = 0;
	}
}

void Game_window::build_light_layers() {
	static const Image_window::UiLayerKind kinds[3]
			= {Image_window::UiLayerLightCandle, Image_window::UiLayerLightSingle, Image_window::UiLayerLightMany};
	// Brighter tiers sit on top (greater z) so they win where lights overlap;
	// all stay below conversation (z 0) and gump layers.
	static const int zvals[3] = {-1002, -1001, -1000};

	const int W = get_width();
	const int H = get_height();

	// The splat rectangles recorded below feed NEXT frame's roof-mask clip
	// (update_roof_mask); rebuild them from scratch each frame.
	light_mask_rects.clear();

	// If the game area changed size (video settings change), the existing
	// layers are the wrong size: their coverage would be dropped, leaving a
	// full-opaque brightened layer covering the whole screen. Recreate them.
	if (W != light_layer_w || H != light_layer_h) {
		for (int t = 0; t < 3; ++t) {
			if (light_layer_handles[t] >= 0) {
				destroy_layer(light_layer_handles[t]);
				light_layer_handles[t] = -1;
			}
		}
		light_layer_w = W;
		light_layer_h = H;
	}

	// Re-assert each light layer's config every build: some video/UI changes
	// (toggling infravision, changing a UI layer palette, etc.) reset the
	// per-kind configs, which would otherwise drop the brighter fixed palette.
	// Use the game's own scaler / fill scaler so the lit copy is filtered like
	// the world; placement is exact via get_game_area_dest (game_to_screen).
	static const int modes[3]
			= {Image_window::UiPaletteCandle, Image_window::UiPaletteSingleLight, Image_window::UiPaletteManyLights};
	const int gscaler = win->get_scaler();
	const int gfill   = win->get_fill_scaler();
	for (int t = 0; t < 3; ++t) {
		win->set_ui_layer_config(kinds[t], 0, 0, gscaler, Image_window::Fill, gfill);
		win->set_ui_layer_palette(kinds[t], modes[t]);
	}

	// No spatial lighting at full day or mid palette-transition: the fixed UI
	// palettes are disabled then, so just hide any existing layers.
	const int palnum = pal ? pal->get_palette_number() : PALETTE_DAY;
	// Refill the (brightened) fixed palettes only when the palette number
	// actually changes. Doing it every frame would overwrite the per-frame
	// colour-cycling rotation that rotate_colors applies to these overrides,
	// freezing animated flames under a light. Palette::apply also refills them
	// on every real palette change, keeping them in phase with the live palette.
	if (pal && palnum != light_layer_palnum) {
		pal->update_ui_layer_palettes();
		light_layer_palnum = palnum;
	}
	const bool lights_active = palnum != PALETTE_DAY && !light_renders.empty();

	// A tier only brightens if its fixed palette is actually lighter than the
	// current world palette; at dawn/dusk the world may already be brighter
	// than (say) candlelight, in which case that tier would darken the scene.
	static const int tier_palnum[3] = {PALETTE_CANDLE, PALETTE_SINGLE_LIGHT, PALETTE_MANY_LIGHTS};
	const int        world_lum      = pal ? pal->get_luminance() : 0;

	// Source pixels = the world image just painted (world + effects only; gumps
	// live in their own layers and must not be brightened).
	Image_buffer*        src    = win->get_ibuf();
	const unsigned char* srcpix = src ? src->get_bits() : nullptr;
	const int            src_lw = src ? static_cast<int>(src->get_line_width()) : 0;

	// Global roof-pixel mask: any pixel a drawn roof covers stays dark under
	// every light so an interior light never lights up the roof over it.
	const unsigned char* roofpix = (roof_light_mask_active && roof_light_mask) ? roof_light_mask->get_bits() : nullptr;
	const int            roof_lw = roofpix ? static_cast<int>(roof_light_mask->get_line_width()) : 0;

	// INSIDE, the room mask applies to every light (the roof mask on top so
	// nothing spills onto a still-drawn roof).  OUTSIDE, it still applies to a
	// light itself under a roof (lr.mask_roof) so its walls contain it, while
	// an exterior light stays unmasked and brightens nearby house roofs.
	const bool inside = is_main_actor_inside();

	// Per-tier signature of the STATIC (world-pinned) lights shaping this
	// frame's coverage masks.  Positions are hashed RELATIVE to the tier's
	// first static light, so a pure view scroll keeps the signature: the
	// cached mask is then reused as-is (no scroll) or translated by the
	// scroll delta.  Moving lights (the carried torch and its spills) never
	// enter the signature -- they are splatted fresh on top each frame.  The
	// room-grid and roof-mask CONTENT is not hashed; a 250ms reuse cap bounds
	// staleness from those, matching the light caches' TTL.
	uint64_t tier_sig[3];
	// Moving lights get their own signature.  Scroll is mixed in ONLY for
	// GATED moving lights (room-field lattices are world-pinned, so any
	// scroll moves the field under the dome); an ungated moving light -- the
	// torch outdoors -- is a pure screen-stationary dome, so its overlay
	// survives walking.  Grid content changes are caught by the flood
	// generation below; the stamp cap is only a roof-content safety net.
	uint64_t mov_sig[3];
	int      sx0[3]        = {0, 0, 0};
	int      sy0[3]        = {0, 0, 0};
	bool     has_static[3] = {false, false, false};
	{
		for (const auto& lr : light_renders) {
			if (lr.moving || lr.tier < 0 || lr.tier > 2 || has_static[lr.tier]) {
				continue;
			}
			has_static[lr.tier] = true;
			sx0[lr.tier]        = lr.sx;
			sy0[lr.tier]        = lr.sy;
		}
		auto mix = [](uint64_t h, int64_t v) {
			h ^= static_cast<uint64_t>(v);
			return h * 1099511628211ULL;
		};
		uint64_t base = mix(mix(mix(mix(14695981039346656037ULL, W), H), inside), roofpix != nullptr);
		// Hidden-room gating (do_splat) depends on the render cut-away level.
		base = mix(base, skip_above_actor);
		// Any change to a cached room grid (a door opened/closed) rebuilds
		// every tier's coverage immediately.
		base        = mix(base, static_cast<int64_t>(NaturalLight::Flood_content_generation()));
		tier_sig[0] = tier_sig[1] = tier_sig[2] = base;
		mov_sig[0] = mov_sig[1] = mov_sig[2] = base;
		for (const auto& lr : light_renders) {
			if (lr.tier < 0 || lr.tier > 2) {
				continue;
			}
			uint64_t h = lr.moving ? mov_sig[lr.tier] : tier_sig[lr.tier];
			if (lr.moving) {
				h                = mix(h, lr.sx);
				h                = mix(h, lr.sy);
				const bool gated = (inside || lr.mask_roof || lr.is_spill) && !lr.lit.empty();
				if (gated) {
					h = mix(mix(mix(mix(h, scrolltx), scrollty), scrolltx_lo), scrollty_lo);
				}
			} else {
				h = mix(h, lr.sx - sx0[lr.tier]);
				h = mix(h, lr.sy - sy0[lr.tier]);
			}
			h = mix(h, lr.radius);
			h = mix(h, lr.elevation);
			h = mix(h, lr.rt);
			h = mix(h, lr.ltx);
			h = mix(h, lr.lty);
			h = mix(h, lr.ltz);
			h = mix(h, lr.mask_roof);
			h = mix(h, lr.dist_bias);
			h = mix(h, lr.is_spill);
			h = mix(h, lr.spill_percent);
			h = mix(h, lr.spill_floor);
			h = mix(h, static_cast<int64_t>(lr.lit.size()));
			if (lr.moving) {
				mov_sig[lr.tier] = h;
			} else {
				tier_sig[lr.tier] = h;
			}
		}
	}

	for (int t = 0; t < 3; ++t) {
		bool any = false;
		for (const auto& lr : light_renders) {
			if (lr.tier == t) {
				any = true;
				break;
			}
		}
		// Skip the tier if its palette is no brighter than the world palette.
		const int  tier_lum = pal ? pal->get_reference_luminance(tier_palnum[t]) : -1;
		const bool brighter = tier_lum > world_lum;
		if (!lights_active || !any || !srcpix || !brighter) {
			if (light_layer_handles[t] >= 0) {
				layer_set_visible(light_layer_handles[t], false);
			}
			// While hidden, the roof mask goes unstamped (light_mask_rects
			// empty) and the view scrolls under the retained coverage.  The
			// sig would still MATCH on re-entry (it hashes light positions
			// relative to each other), reusing/translating that garbage for
			// up to a second -- so force a fresh rebuild + mask resample.
			light_tier_sig[t] = 0;
			continue;
		}
		// Ensure the (game-area sized) layer exists.
		int handle = light_layer_handles[t];
		if (handle < 0) {
			handle                 = create_layer(W, H, 255, 0, zvals[t]);
			light_layer_handles[t] = handle;
		}
		layer_set_opaque(handle, true);
		layer_set_ui_kind(handle, kinds[t]);

		Image_buffer8* dst = get_layer_ibuf(handle);
		if (!dst) {
			continue;
		}
		unsigned char* dstpix = dst->get_bits();
		const int      dst_lw = static_cast<int>(dst->get_line_width());

		// This tier's radial-alpha (coverage) mask holds the STATIC
		// (world-pinned) lights only.  It persists between frames: while the
		// static signature holds, a still view reuses it as-is and a scrolled
		// view translates it by the scroll delta, re-splatting only the
		// lights exposed at a screen edge; anything else rebuilds it.
		std::vector<unsigned char>& covbuf = light_tier_cov[t];
		if (covbuf.size() != static_cast<size_t>(W) * H) {
			covbuf.assign(static_cast<size_t>(W) * H, 0);
			light_tier_rects[t].clear();
			light_tier_mask_rects[t].clear();
			light_tier_bounds[t].clear();
			light_tier_sig[t] = 0;
		}
		unsigned char* cov    = covbuf.data();
		const uint64_t now_ms = SDL_GetTicks();

		// Splat one light into `covp` (and the layer's pixels); returns the
		// splat's unclamped screen bounds (the same union Splat_radial_light
		// uses: the free dome around the centre plus the field's lattice
		// around the room anchor).  An optional clip window restricts the
		// pixels written (scroll-vacated strip patching).
		auto do_splat
				= [&](const Light_render_info& lr, unsigned char* covp, int cx0 = 0, int cy0 = 0, int cx1 = -1, int cy1 = -1) {
					  // Masking policy: see the `inside` comment above; the per-pixel
					  // roof / tall / storey semantics live in Splat_radial_light.
					  const bool mask_roof = roofpix && lr.mask_roof;
					  // The dome (intensity + falloff) emits from the flame up on the
					  // sprite, not the tile foot get_shape_location returns, so raise
					  // only its centre up-and-left by the sprite's height (plus the
					  // foot-corner residual).  A SPILL's centre stays at the opening's
					  // outside tile -- its elevation only shapes the continued falloff
					  // -- so it is not shifted.
					  const int foot_bias = c_tilesize / 4;
					  const int emit      = (lr.is_spill ? 0 : lr.elevation) + foot_bias;
					  const int csx       = lr.sx - emit;
					  const int csy       = lr.sy - emit;
					  // Gated lights render as a propagated field from the room-fill
					  // grid (see Splat_radial_light).  The screen reference pinning
					  // the grid is the foot position of the light's own tile at the
					  // TOP of the room's walls (walls render 4px per z up-left of
					  // their tile) -- a fixed property of the room, so the field
					  // stays pinned to the walls as the source moves.
					  const unsigned char* grid    = nullptr;
					  int                  grid_rt = 0;
					  int                  grid_fx = 0;
					  int                  grid_fy = 0;
					  // Veto lights: the storey their room roof reaches; spills: the
					  // bubble's render storey (gating rules in Splat_radial_light).
					  int light_top_storey = 0;
					  // The field's lattice anchor level (locates the shaft band of
					  // an elevated light's floor holes in Splat_radial_light).
					  int field_anchor_z = 0;
					  // A light whose room is not part of this render must not
					  // splat: a veto light on a cut-away storey (viewer below
					  // it) would paint only its shaft band onto the room below;
					  // a roofed spill whose ceiling IS drawn (viewer above its
					  // storey) is hidden and would paint only its z-shifted
					  // wall-ring seam over the slab sides / exterior ground.
					  bool hidden_room = lr.mask_roof && lr.ltz >= skip_above_actor;
					  if ((inside || lr.mask_roof || lr.is_spill) && !lr.lit.empty()) {
						  grid    = lr.lit.data();
						  grid_rt = lr.rt;
						  // Anchor at the wall top whenever the light has a real
						  // ceiling (a dungeon's rock ceiling counts even though
						  // mask_roof is false).  Only a light with none -- an exterior
						  // lamp gated while the Avatar is inside, a spill on open
						  // ground -- anchors at its own storey floor: the floor+5
						  // fallback would slide its field 4px per z up-left off it.
						  bool      ceiling  = false;
						  const int roof_z   = NaturalLight::Light_room_roof_z(map, lr.ltx, lr.lty, lr.ltz, &ceiling);
						  const int anchor_z = (lr.mask_roof || ceiling) ? roof_z : (lr.ltz / 5) * 5;
						  field_anchor_z     = anchor_z;
						  if (lr.is_spill && ceiling && roof_z < skip_above_actor) {
							  hidden_room = true;
						  }
						  if (lr.mask_roof) {
							  light_top_storey = (anchor_z + 4) / 5;
						  } else if (lr.is_spill) {
							  light_top_storey = lr.ltz / 5;
						  }
						  const int wtx = ((lr.ltx % c_num_tiles) + c_num_tiles) % c_num_tiles;
						  const int wty = ((lr.lty % c_num_tiles) + c_num_tiles) % c_num_tiles;
						  get_shape_location(Tile_coord(wtx, wty, anchor_z), grid_fx, grid_fy);
						  // Empirically the wall-top anchor lands 1px up-left of the
						  // visible interior; nudge the reference back.
						  grid_fx += 1;
						  grid_fy += 1;
					  }
					  if (!hidden_room) {
						  NaturalLight::Splat_radial_light(
								  covp, dstpix, srcpix, W, H, dst_lw, src_lw, csx, csy, lr.radius, lr.elevation, lr.dist_bias,
								  lr.spill_percent, roofpix, roof_lw, mask_roof, lr.is_spill, lr.spill_floor, light_top_storey,
								  lr.ltz / 5, field_anchor_z, grid, grid_rt, grid_fx, grid_fy, cx0, cy0, cx1, cy1);
					  }
					  int bx0 = csx - lr.radius;
					  int bx1 = csx + lr.radius;
					  int by0 = csy - lr.radius;
					  int by1 = csy + lr.radius;
					  if (grid != nullptr) {
						  bx0 = std::min(bx0, grid_fx - lr.radius - c_tilesize);
						  bx1 = std::max(bx1, grid_fx + lr.radius + c_tilesize);
						  by0 = std::min(by0, grid_fy - lr.radius - c_tilesize);
						  by1 = std::max(by1, grid_fy + lr.radius + c_tilesize);
					  }
					  return TileRect(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
				  };

		// Derive both rect lists from the per-light unclamped bounds:
		// unclamped + margin -> next frame's roof-mask clips; clamped ->
		// the coverage rows holding non-zero alpha.
		auto rebuild_rects = [&]() {
			light_tier_rects[t].clear();
			light_tier_mask_rects[t].clear();
			for (const TileRect& b : light_tier_bounds[t]) {
				TileRect clip = b;
				light_tier_mask_rects[t].push_back(clip.enlarge(24));
				const int bx0 = std::max(b.x, 0);
				const int by0 = std::max(b.y, 0);
				const int bx1 = std::min(b.x + b.w - 1, W - 1);
				const int by1 = std::min(b.y + b.h - 1, H - 1);
				if (bx0 <= bx1 && by0 <= by1) {
					light_tier_rects[t].emplace_back(bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1);
				}
			}
		};

		// The world animates beneath a kept mask: refresh the copied world
		// pixels over the static coverage rows.
		auto refresh_dst = [&]() {
			for (const TileRect& r : light_tier_rects[t]) {
				for (int y = r.y; y < r.y + r.h; ++y) {
					std::memcpy(
							dstpix + static_cast<size_t>(y) * dst_lw + r.x, srcpix + static_cast<size_t>(y) * src_lw + r.x,
							static_cast<size_t>(r.w));
				}
			}
		};

		const bool sig_ok = light_tier_sig[t] != 0 && tier_sig[t] == light_tier_sig[t] && now_ms - light_tier_stamp[t] < 1000
							&& !light_tier_resample[t];
		int dx = 0;
		int dy = 0;
		if (sig_ok && has_static[t]) {
			dx = sx0[t] - light_tier_anchor_x[t];
			dy = sy0[t] - light_tier_anchor_y[t];
		}
		const bool reuse      = sig_ok && dx == 0 && dy == 0;
		const bool translate  = sig_ok && !reuse && std::abs(dx) < W && std::abs(dy) < H;
		bool       has_moving = false;
		{
			if (reuse) {
				// Mask unchanged and screen-aligned: just refresh the copied
				// world pixels and keep last pass's rects.
				refresh_dst();
			} else if (translate) {
				// Same static set, pure view scroll: slide the cached mask by
				// the scroll delta and zero the vacated strips.
				const int copy_w    = W - std::abs(dx);
				const int dst_x     = std::max(dx, 0);
				const int src_x     = std::max(-dx, 0);
				auto      shift_row = [&](int y) {
                    unsigned char*       drow = cov + static_cast<size_t>(y) * W;
                    const unsigned char* srow = cov + static_cast<size_t>(y - dy) * W;
                    std::memmove(drow + dst_x, srow + src_x, static_cast<size_t>(copy_w));
                    if (dx > 0) {
                        std::memset(drow, 0, static_cast<size_t>(dst_x));
                    } else if (dx < 0) {
                        std::memset(drow + copy_w, 0, static_cast<size_t>(W - copy_w));
                    }
				};
				if (dy >= 0) {
					for (int y = H - 1; y >= dy; --y) {
						shift_row(y);
					}
					for (int y = 0; y < dy; ++y) {
						std::memset(cov + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
					}
				} else {
					for (int y = 0; y < H + dy; ++y) {
						shift_row(y);
					}
					for (int y = H + dy; y < H; ++y) {
						std::memset(cov + static_cast<size_t>(y) * W, 0, static_cast<size_t>(W));
					}
				}
				for (TileRect& b : light_tier_bounds[t]) {
					b.x += dx;
					b.y += dy;
				}
				rebuild_rects();
				refresh_dst();

				// Patch only the vacated strips: the translated interior is
				// already correct, so each affected light is re-splatted
				// CLIPPED to the strip(s) its bounds reach into -- a few
				// pixel columns/rows, not its whole area.
				struct Strip {
					int x0, y0, x1, y1;
				};

				Strip strips[2];
				int   nstrips = 0;
				if (dx > 0) {
					strips[nstrips++] = {0, 0, dx - 1, H - 1};
				} else if (dx < 0) {
					strips[nstrips++] = {W + dx, 0, W - 1, H - 1};
				}
				if (dy > 0) {
					strips[nstrips++] = {0, 0, W - 1, dy - 1};
				} else if (dy < 0) {
					strips[nstrips++] = {0, H + dy, W - 1, H - 1};
				}
				size_t bi = 0;
				for (const auto& lr : light_renders) {
					if (lr.tier != t || lr.moving || lr.radius <= 0) {
						continue;
					}
					if (bi >= light_tier_bounds[t].size()) {
						break;
					}
					const TileRect& b = light_tier_bounds[t][bi++];
					for (int s = 0; s < nstrips; ++s) {
						const Strip& st = strips[s];
						if (b.x > st.x1 || b.y > st.y1 || b.x + b.w - 1 < st.x0 || b.y + b.h - 1 < st.y0) {
							continue;
						}
						do_splat(lr, cov, st.x0, st.y0, st.x1, st.y1);
					}
				}
				light_tier_anchor_x[t] = sx0[t];
				light_tier_anchor_y[t] = sy0[t];
			} else {
				// Full rebuild: restore the all-zero coverage invariant over
				// last pass's rows first, then splat every static light.
				for (const TileRect& r : light_tier_rects[t]) {
					for (int y = r.y; y < r.y + r.h; ++y) {
						std::memset(cov + static_cast<size_t>(y) * W + r.x, 0, static_cast<size_t>(r.w));
					}
				}
				const bool content_changed = light_tier_sig[t] != tier_sig[t];
				light_tier_bounds[t].clear();
				for (const auto& lr : light_renders) {
					if (lr.tier != t || lr.moving || lr.radius <= 0) {
						continue;
					}
					light_tier_bounds[t].push_back(do_splat(lr, cov));
				}
				rebuild_rects();
				light_tier_sig[t]      = tier_sig[t];
				light_tier_stamp[t]    = now_ms;
				light_tier_anchor_x[t] = sx0[t];
				light_tier_anchor_y[t] = sy0[t];
				if (content_changed) {
					// The static set changed (a light placed, removed or newly
					// scrolled into view): the roof mask this frame was stamped
					// only inside LAST frame's light rects, so the rebuild above
					// baked stale mask under the new coverage.  Repaint
					// everything next frame (deferred: paint_dirty clears the
					// dirty rect right after this call) and rebuild once more to
					// resample the then-complete mask.
					light_tier_resample[t] = true;
					light_repaint_pending  = true;
				} else {
					light_tier_resample[t] = false;
				}
			}
			// This tier's roof-mask clips for next frame.
			light_mask_rects.insert(light_mask_rects.end(), light_tier_mask_rects[t].begin(), light_tier_mask_rects[t].end());
			// Moving lights (the carried torch and its spills) never touch
			// the static cache: they live in their own cached overlay, reused
			// while the Avatar stands still and rebuilt while walking, then
			// max-combined over a scratch copy of the static mask.
			for (const auto& lr : light_renders) {
				if (lr.tier == t && lr.moving && lr.radius > 0) {
					has_moving = true;
					break;
				}
			}
			if (has_moving) {
				const size_t                sz     = static_cast<size_t>(W) * H;
				std::vector<unsigned char>& mov    = light_tier_mov_cov[t];
				const bool                  mov_ok = light_tier_mov_sig[t] != 0 && mov_sig[t] == light_tier_mov_sig[t]
									&& now_ms - light_tier_mov_stamp[t] < 1000 && mov.size() == sz;
				if (!mov_ok) {
					if (mov.size() != sz) {
						mov.assign(sz, 0);
					} else {
						for (const TileRect& b : light_tier_mov_bounds[t]) {
							const int bx0 = std::max(b.x, 0);
							const int by0 = std::max(b.y, 0);
							const int bx1 = std::min(b.x + b.w - 1, W - 1);
							const int by1 = std::min(b.y + b.h - 1, H - 1);
							for (int y = by0; y <= by1; ++y) {
								std::memset(mov.data() + static_cast<size_t>(y) * W + bx0, 0, static_cast<size_t>(bx1 - bx0 + 1));
							}
						}
					}
					light_tier_mov_bounds[t].clear();
					for (const auto& lr : light_renders) {
						if (lr.tier != t || !lr.moving || lr.radius <= 0) {
							continue;
						}
						light_tier_mov_bounds[t].push_back(do_splat(lr, mov.data()));
					}
					light_tier_mov_sig[t]   = mov_sig[t];
					light_tier_mov_stamp[t] = now_ms;
				}
				// Composite into the scratch copy and refresh the world pixels
				// beneath the moving coverage (its splat-time dst copies go
				// stale as the world animates).
				light_scratch_cov.resize(sz);
				std::memcpy(light_scratch_cov.data(), cov, sz);
				for (const TileRect& b : light_tier_mov_bounds[t]) {
					TileRect clipr = b;
					light_mask_rects.push_back(clipr.enlarge(24));
					const int bx0 = std::max(b.x, 0);
					const int by0 = std::max(b.y, 0);
					const int bx1 = std::min(b.x + b.w - 1, W - 1);
					const int by1 = std::min(b.y + b.h - 1, H - 1);
					if (bx0 > bx1 || by0 > by1) {
						continue;
					}
					for (int y = by0; y <= by1; ++y) {
						std::memcpy(
								dstpix + static_cast<size_t>(y) * dst_lw + bx0, srcpix + static_cast<size_t>(y) * src_lw + bx0,
								static_cast<size_t>(bx1 - bx0 + 1));
						unsigned char*       srow = light_scratch_cov.data() + static_cast<size_t>(y) * W;
						const unsigned char* mrow = mov.data() + static_cast<size_t>(y) * W;
						for (int x = bx0; x <= bx1; ++x) {
							if (mrow[x] > srow[x]) {
								srow[x] = mrow[x];
							}
						}
					}
				}
			}
		}
		{
			layer_set_coverage(handle, has_moving ? light_scratch_cov.data() : cov, W, H);
			// Align the overlay with the world's on-screen rectangle.
			int gx0 = 0;
			int gy0 = 0;
			int gdw = W;
			int gdh = H;
			win->get_game_area_dest(gx0, gy0, gdw, gdh);
			layer_set_dest(handle, gx0, gy0, gdw, gdh);
			layer_set_visible(handle, true);
			layer_set_dirty(handle);
		}
	}
}

/*
 *  Put the actor(s) in the world.
 */

void Game_window::init_actors() {
	if (main_actor) {    // Already done?
		Game::clear_avname();
		Game::clear_avsex();
		Game::clear_avskin();
		return;
	}
	read_npcs();    // Read in all U7 NPC's.
	int avsched = combat ? Schedule::combat : Schedule::follow_avatar;
	if (!main_actor_can_act_charmed()) {
		avsched = Schedule::combat;
	}
	main_actor->set_schedule_type(avsched);

	// Was a name, sex or skincolor set in Game
	// this bascially detects
	bool changed = false;

	if (Game::get_avsex() == 0 || Game::get_avsex() == 1 || Game::get_avname()
		|| (Game::get_avskin() >= 0 && Game::get_avskin() <= 2)) {
		changed = true;
	}

	Game::clear_avname();
	Game::clear_avsex();
	Game::clear_avskin();

	// Update gamedat if there was a change
	if (changed) {
		schedule_npcs(6, false);
		write_npcs();
	}
}

// In gamemgr/modmgr.cc because it is also needed by ES.
string get_game_identity(const char* savename, const string& title);

/*
 *  Create initial 'gamedat' directory if needed
 *
 */

bool Game_window::init_gamedat(bool create) {
	// Create gamedat files 1st time.
	if (create) {
		cout << "Creating 'gamedat' files." << endl;
		if (is_system_path_defined("<PATCH>") && U7exists(PATCH_INITGAME)) {
			restore_gamedat(PATCH_INITGAME);
		} else {
			// Flag that we're reading U7 file.
			Game::set_new_game();
			restore_gamedat(INITGAME);
		}
		// Editing, and no IDENTITY?
		if (Game::is_editing() && !U7exists(IDENTITY)) {
			auto pOut = U7open_out(IDENTITY);
			if (!pOut) {
				return false;
			}
			auto& out = *pOut;
			out << Game::get_gametitle() << endl;
		}

		// log version of exult that was used to start this game
		auto out = U7open_out(GNEWGAMEVER);
		if (out) {
			getVersionInfo(*out);
		}
	}
	//++++Maybe just test for IDENTITY+++:
	else if ((U7exists(U7NBUF_DAT) || !U7exists(NPC_DAT)) && !Game::is_editing()) {
		return false;
	} else {
		auto pIdentity_file = U7open_in(IDENTITY);
		if (!pIdentity_file) {
			return false;
		}
		auto& identity_file = *pIdentity_file;
		char  gamedat_identity[256];
		identity_file.read(gamedat_identity, 256);
		char* ptr = gamedat_identity;
		for (; (*ptr != 0x1a && *ptr != 0x0d && *ptr != 0x0a); ptr++)
			;
		*ptr = 0;
		cout << "Gamedat identity " << gamedat_identity << endl;
		const string static_identity = get_game_identity(INITGAME, Game::get_gametitle());
		if (static_identity != gamedat_identity) {
			return false;
		}
		// scroll coords.
	}
	read_save_names();    // Read in saved-game names.
	return true;
}

/*
 *  Save game by writing out to the 'gamedat' directory.
 *
 *  Output: 0 if error, already reported.
 */

void Game_window::write(bool nopaint) {
	// Lets just show a nice message on screen first

	const int width       = get_width();
	const int centre_x    = width / 2;
	const int height      = get_height();
	const int centre_y    = height / 2;
	const int text_height = shape_man->get_text_height(0);
	const int text_width  = shape_man->get_text_width(0, Strings::SavingGame());

	if (!nopaint) {
		win->fill_translucent8(0, width, height, 0, 0, shape_man->get_xform(8));
		shape_man->paint_text(0, Strings::SavingGame(), centre_x - text_width / 2, centre_y - text_height);
		show(true);
	}
	for (auto* map : maps) {
		if (map) {
			map->write_ireg();    // Write ireg files.
		}
	}
	write_npcs();              // Write out npc.dat.
	usecode->write();          // Usecode.dat (party, global flags).
	Notebook_gump::write();    // Write out journal.
	write_gwin();              // Write our data.
	write_saveinfo(!nopaint);
}

/*
 *  Restore game by reading in 'gamedat'.
 *
 *  Output: 0 if error, already reported.
 */

void Game_window::read() {
	Game_window::Scoped_ui_layer_mask load_layers(this, 0);
	Audio::get_ptr()->cancel_streams();
	// Display red plasma during load...
	setup_load_palette();

	clear_world(true);    // Wipe clean.

	// Re-add background noise to queue after clear_world() removed it
	if (background_noise) {
		tqueue->add(Game::get_ticks() + 5000, background_noise);
	}

	read_gwin();    // Read our data.
	// DON'T do anything that might paint()
	// before calling read_npcs!!
	setup_game(cheat.in_map_editor());    // Read NPC's, usecode.
	Mouse::mouse()->set_speed_cursor();
}

/*
 *  Write data for the game.
 *
 *  Output: 0 if error.
 */

void Game_window::write_gwin() {
	OFileDataSource gout(GWINDAT);
	// Start with scroll coords (in tiles).
	gout.write2(get_scrolltx());
	gout.write2(get_scrollty());
	// Write clock.
	gout.write2(clock->get_day());
	gout.write2(clock->get_hour());
	gout.write2(clock->get_minute());
	gout.write4(special_light);    // Write spell expiration minute.
	MyMidiPlayer* player = Audio::get_ptr()->get_midi();
	if (player) {
		gout.write4(static_cast<uint32>(player->get_current_track()));
		gout.write4(static_cast<uint32>(player->is_repeating()) | player->get_egg_count() << 16);
	} else {
		gout.write4(static_cast<uint32>(-1));
		gout.write4(0);
	}
	gout.write1(armageddon ? 1 : 0);
	gout.write1(ambient_light ? 1 : 0);
	gout.write1(combat ? 1 : 0);
	gout.write1(infravision_active ? 1 : 0);
	if (!gout.good()) {
		throw file_write_exception(GWINDAT);
	}
}

/*
 *  Read data for the game.
 *
 *  Output: 0 if error.
 */

void Game_window::read_gwin() {
	if (!clock->in_queue()) {    // Be sure clock is running.
		tqueue->add(Game::get_ticks(), clock, this);
	}

	IFileDataSource gin(GWINDAT);
	if (!gin.good()) {
		return;
	}

	// Start with scroll coords (in tiles).
	scrolltx_lp = scrolltx_l = scrolltx = gin.read2();
	scrollty_lp = scrollty_l = scrollty = gin.read2();
	scrolltx_lo = scrollty_lo = 0;
	avposx_ld = avposy_ld = 0;
	// Read clock.
	clock->reset();
	clock->set_day(gin.read2());
	clock->set_hour(gin.read2());
	clock->set_minute(gin.read2());
	if (!gin.good()) {    // Next ones were added recently.
		throw file_read_exception(GWINDAT);
	}
	special_light = gin.read4();
	armageddon    = false;    // Old saves may not have this yet.

	if (!gin.good()) {
		special_light = 0;
		return;
	}

	const int track_num = gin.read4();
	const int repeat    = gin.read4();
	if (!gin.good()) {
		Audio::get_ptr()->stop_music();
		return;
	}
	MyMidiPlayer* midi = Audio::get_ptr()->get_midi();
	if (!is_background_track(track_num) || (midi && (midi->get_ogg_enabled() || midi->is_mt32()))) {
		Audio::get_ptr()->start_music(track_num, repeat & 0x1);
		if (midi) {
			midi->set_egg_count(static_cast<uint16>(repeat >> 16));
		}
	}
	armageddon = gin.read1() == 1;
	if (!gin.good()) {
		armageddon = false;
	}

	ambient_light = gin.read1() == 1;
	if (!gin.good()) {
		ambient_light = false;
	}

	combat = gin.read1() == 1;
	if (!gin.good()) {
		combat = false;
	}

	infravision_active = gin.read1() == 1;
	if (!gin.good()) {
		infravision_active = false;
	}
}

/*
 *  Was any map modified?
 */

bool Game_window::was_map_modified() {
	if (Game_map::was_chunk_terrain_modified()) {
		return true;
	}
	for (auto* map : maps) {
		if (map && map->was_map_modified()) {
			return true;
		}
	}
	return false;
}

/*
 *  Write out map data (IFIXxx, U7CHUNKS, U7MAP) to static, and also
 *  save 'gamedat' to <PATCH>/initgame.dat.
 *
 *  Note: This is for map-editing.
 *
 *  Output: Errors are reported.
 */

void Game_window::write_map() {
	for (auto* map : maps) {
		if (map && map->was_map_modified()) {
			map->write_static();    // Write ifix, map files.
		}
	}
	write();    // Write out to 'gamedat' too.
	save_gamedat(PATCH_INITGAME, "Saved map");
}

/*
 *  Reinitialize game from map.
 */

void Game_window::read_map() {
	init_gamedat(true);    // Unpack 'initgame.dat'.
	read();                // This does the whole restore.
}

/*
 *  Reload (patched) usecode.
 */

void Game_window::reload_usecode() {
	// Get custom usecode functions.
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_USECODE)) {
		auto pFile = U7open_in(PATCH_USECODE);
		if (!pFile) {
			return;
		}
		auto& file = *pFile;
		usecode->read_usecode(file, true);
	}
}

/*
 *  Shift view by one tile.
 */

void Game_window::view_right() {
	const int w = get_width();
	const int h = get_height();
	// Get current rightmost chunk.
	const int old_rcx = ((scrolltx + (w - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	scrolltx          = INCR_TILE(scrolltx);
	scroll_bounds.x   = INCR_TILE(scroll_bounds.x);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	// Shift image to left.
	win->copy(c_tilesize, 0, w - c_tilesize, h, 0, 0);
	// Paint 1 column to right.
	paint(w - c_tilesize, 0, c_tilesize, h);
	dirty.x -= c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_rcx = ((scrolltx + (w - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	if (new_rcx != old_rcx) {
		Send_location(this);
	}
}

void Game_window::view_left() {
	const int old_lcx = (scrolltx / c_tiles_per_chunk) % c_num_chunks;
	// Want to wrap.
	scrolltx        = DECR_TILE(scrolltx);
	scroll_bounds.x = DECR_TILE(scroll_bounds.x);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	win->copy(0, 0, get_width() - c_tilesize, get_height(), c_tilesize, 0);
	const int h = get_height();
	paint(0, 0, c_tilesize, h);
	dirty.x += c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_lcx = (scrolltx / c_tiles_per_chunk) % c_num_chunks;
	if (new_lcx != old_lcx) {
		Send_location(this);
	}
}

void Game_window::view_down() {
	const int w = get_width();
	const int h = get_height();
	// Get current bottomost chunk.
	const int old_bcy = ((scrollty + (h - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	scrollty          = INCR_TILE(scrollty);
	scroll_bounds.y   = INCR_TILE(scroll_bounds.y);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	win->copy(0, c_tilesize, w, h - c_tilesize, 0, 0);
	paint(0, h - c_tilesize, w, c_tilesize);
	dirty.y -= c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_bcy = ((scrollty + (h - 1) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	if (new_bcy != old_bcy) {
		Send_location(this);
	}
}

void Game_window::view_up() {
	const int old_tcy = (scrollty / c_tiles_per_chunk) % c_num_chunks;
	// Want to wrap.
	scrollty        = DECR_TILE(scrollty);
	scroll_bounds.y = DECR_TILE(scroll_bounds.y);
	if (gump_man->showing_gumps()) {    // Gump on screen?
		paint();
		return;
	}
	map->read_map_data();    // Be sure objects are present.
	const int w = get_width();
	win->copy(0, 0, w, get_height() - c_tilesize, 0, c_tilesize);
	paint(0, 0, w, c_tilesize);
	dirty.y += c_tilesize;    // Shift dirty rect.
	dirty = clip_to_win(dirty);
	// New chunk?
	const int new_tcy = (scrollty / c_tiles_per_chunk) % c_num_chunks;
	if (new_tcy != old_tcy) {
		Send_location(this);
	}
}

/*
 *  Get gump being dragged.
 */

Gump* Game_window::get_dragging_gump() {
	return dragging ? dragging->gump : nullptr;
}

/*
 *  Alternative start actor function
 *  Placed in an alternative function to prevent breaking barges
 */
void Game_window::start_actor_alt(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	// Avatar can move, don't sync the barge anymore.
	landing_barge = nullptr;
	int  ax;
	int  ay;
	bool blocked[8];
	get_shape_location(main_actor, ax, ay);

	Tile_coord start = main_actor->get_tile();
	int        dir;
	const bool checkdrop = (main_actor->get_type_flags() & MOVE_LEVITATE) == 0;

	for (dir = 0; dir < 8; dir++) {
		Tile_coord dest = start.get_neighbor(dir);
		blocked[dir]    = main_actor->is_blocked(dest, &start, main_actor->get_type_flags());
		if (checkdrop && abs(start.tz - dest.tz) > 1) {
			blocked[dir] = true;
		}
	}

	dir = Get_direction_NoWrap(ay - winy, winx - ax);

	if (blocked[dir] && !blocked[(dir + 1) % 8]) {
		dir = (dir + 1) % 8;
	} else if (blocked[dir] && !blocked[(dir + 7) % 8]) {
		dir = (dir + 7) % 8;
	} else if (blocked[dir]) {
		Game_object* block = main_actor->is_moving() ? nullptr : main_actor->find_blocking(start.get_neighbor(dir), dir);
		// We already know the blocking object isn't the avatar, so don't
		// double check it here.
		if (!block || !block->move_aside(main_actor, dir)) {
			stop_actor();
			if (main_actor->get_lift() % 5) {    // Up on something?
				// See if we're stuck in the air.
				const int savetz = start.tz;
				if (!Map_chunk::is_blocked(start, 1, MOVE_WALK, 100) && start.tz < savetz) {
					main_actor->move(start.tx, start.ty, start.tz);
				}
			}
			return;
		}
	}

	const int delta = step_tile_delta * c_tilesize;    // Bigger # here avoids jerkiness,
	// but causes probs. with followers.
	switch (dir) {
	case north:
		// cout << "NORTH" << endl;
		ay -= delta;
		break;

	case northeast:
		// cout << "NORTH EAST" << endl;
		ay -= delta;
		ax += delta;
		break;

	case east:
		// cout << "EAST" << endl;
		ax += delta;
		break;

	case southeast:
		// cout << "SOUTH EAST" << endl;
		ay += delta;
		ax += delta;
		break;

	case south:
		// cout << "SOUTH" << endl;
		ay += delta;
		break;

	case southwest:
		// cout << "SOUTH WEST" << endl;
		ay += delta;
		ax -= delta;
		break;

	case west:
		// cout << "WEST" << endl;
		ax -= delta;
		break;

	case northwest:
		// cout << "NORTH WEST" << endl;
		ay -= delta;
		ax -= delta;
		break;
	}

	const int lift       = main_actor->get_lift();
	const int liftpixels = 4 * lift;    // Figure abs. tile.
	int       tx         = get_scrolltx() + (ax + liftpixels) / c_tilesize;
	int       ty         = get_scrollty() + (ay + liftpixels) / c_tilesize;
	// Wrap:
	tx = (tx + c_num_tiles) % c_num_tiles;
	ty = (ty + c_num_tiles) % c_num_tiles;
	main_actor->walk_to_tile(tx, ty, lift, speed, 0);
	if (walk_in_formation && main_actor->get_action()) {
		//++++++In this case, may need to set schedules back to
		// follow_avatar after, say, sitting.++++++++++
		main_actor->get_action()->set_get_party(true);
	} else {    // "Traditional" Exult walk:-)
		main_actor->get_followers();
	}
}

/*
 *  Start the actor.
 */

void Game_window::start_actor(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	if (main_actor->Actor::get_flag(Obj_flags::asleep)) {
		return;    // Zzzzz....
	}
	if (!cheat.in_map_editor() && (main_actor->in_usecode_control() || main_actor->get_flag(Obj_flags::paralyzed))) {
		return;
	}
	if (gump_man->gump_mode() && !gump_man->gumps_dont_pause_game()) {
		return;
	}
	//	teleported = 0;
	if (moving_barge) {
		// Want to move center there.
		const int lift       = main_actor->get_lift();
		const int liftpixels = 4 * lift;    // Figure abs. tile.
		int       tx         = get_scrolltx() + (winx + liftpixels) / c_tilesize;
		int       ty         = get_scrollty() + (winy + liftpixels) / c_tilesize;
		// Wrap:
		tx                     = (tx + c_num_tiles) % c_num_tiles;
		ty                     = (ty + c_num_tiles) % c_num_tiles;
		const Tile_coord atile = moving_barge->get_center();
		const Tile_coord btile = moving_barge->get_tile();
		// Go faster than walking.
		moving_barge->travel_to_tile(Tile_coord(tx + btile.tx - atile.tx, ty + btile.ty - atile.ty, btile.tz), speed / 2);
	} else {
		/*
		main_actor->walk_to_tile(tx, ty, lift, speed, 0);
		main_actor->get_followers();
		*/
		// Set schedule.
		const int sched = main_actor->get_schedule_type();
		if (sched != Schedule::follow_avatar && sched != Schedule::combat && !main_actor->get_flag(Obj_flags::asleep)) {
			main_actor->set_schedule_type(Schedule::follow_avatar);
			// If the avatar was *not* in combat or follow avatar schedules,
			// then we need to reset the schedules of party members when the
			// Avatar moves.
			const int cnt = party_man->get_count();
			for (int i = 0; i < cnt; i++) {
				Actor* npc = get_npc(party_man->get_member(i));
				// Not sure how much of this is needed, but in any case...
				if (npc == nullptr || npc->get_flag(Obj_flags::asleep) || npc->is_dead()) {
					continue;
				}
				npc->set_schedule_type(Schedule::follow_avatar);
			}
		}
		// Going to use the alternative function for this at the moment
		start_actor_alt(winx, winy, speed);
	}
}

/*
 *  Find path to where user double-right-clicked.
 */

void Game_window::start_actor_along_path(
		int winx, int winy,    // Mouse position to aim for.
		int speed              // Msecs. between frames.
) {
	if (main_actor->Actor::get_flag(Obj_flags::asleep) || main_actor->Actor::get_flag(Obj_flags::paralyzed)
		|| main_actor->get_schedule_type() == Schedule::sleep || moving_barge) {    // For now, don't do barges.
		return;                                                                     // Zzzzz....
	}
	// Animation in progress?
	if (!cheat.in_map_editor() && main_actor->in_usecode_control()) {
		return;
	}
	//	teleported = 0;
	const int        lift       = main_actor->get_lift();
	const int        liftpixels = 4 * lift;    // Figure abs. tile.
	const Tile_coord dest(
			get_scrolltx() + (winx + liftpixels) / c_tilesize, get_scrollty() + (winy + liftpixels) / c_tilesize, lift);
	if (!main_actor->walk_path_to_tile(dest, speed)) {
		cout << "Couldn't find path for Avatar." << endl;
		if (touch_pathfind) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
		}
	} else {
		if (touch_pathfind) {
			get_effects()->add_effect(std::make_unique<Sprites_effect>(18, dest, 0, 0, 0, 0));
		}
		main_actor->get_followers();
	}
}

/*
 *  Stop the actor.
 */

void Game_window::stop_actor() {
	if (moving_barge) {
		moving_barge->stop();
	} else {
		main_actor->stop();    // Stop and set resting state.
		if (!gump_man->gump_mode() || gump_man->gumps_dont_pause_game()) {
			//+++++++The following line is for testing:
			//			if (!walk_in_formation)
			main_actor->get_followers();
		}
	}
}

/*
 *  Teleport the party.
 */

void Game_window::teleport_party(
		const Tile_coord& t,            // Where to go.
		bool              skip_eggs,    // Don't activate eggs at dest.
		int               newmap,       // New map #, or -1 for same map.
		bool              no_status_check) {
	const Tile_coord oldpos = main_actor->get_tile();
	main_actor->set_action(nullptr);    // Definitely need this, or you may
	//   step back to where you came from.
	moving_barge  = nullptr;    // Calling 'done()' could be risky...
	landing_barge = nullptr;
	int       i;
	const int cnt = party_man->get_count();
	if (!skip_eggs) {
		main_actor->get_chunk()->unhatch_eggs(main_actor, t.tx, t.ty, t.tz, oldpos.tx, oldpos.ty);
	}
	if (newmap != -1) {
		set_map(newmap);
	}
	main_actor->move(t.tx, t.ty, t.tz, newmap);    // Move Avatar.
	// Fixes a rare crash when moving between maps and teleporting:
	newmap = main_actor->get_map_num();
	center_view(t);    // Bring pos. into view, and insure all
	clock->reset_palette();
	//   objs. exist.
	for (i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		if (person && !person->is_dead() && person->get_schedule_type() != Schedule::wait
			&& (person->can_act() || no_status_check)) {
			person->set_action(nullptr);
			const Tile_coord t1 = Map_chunk::find_spot(t, 8, person->get_shapenum(), person->get_framenum(), 1);
			if (t1.tx != -1) {
				person->move(t1, newmap);
				person->change_frame(person->get_dir_framenum(Actor::standing));
			}
		}
	}
	main_actor->get_followers();
	if (!skip_eggs) {    // Check all eggs around new spot.
		Map_chunk::try_all_eggs(main_actor, t.tx, t.ty, t.tz, oldpos.tx, oldpos.ty);
	}
	//	teleported = 1;
}

/*
 *  Get party members.
 *
 *  Output: Number returned in 'list'.
 */

int Game_window::get_party(
		Actor** a_list,       // Room for 9.
		int     avatar_too    // 1 to include Avatar too.
) {
	int n = 0;
	if (avatar_too && main_actor) {
		a_list[n++] = main_actor;
	}
	const int cnt = party_man->get_count();
	for (int i = 0; i < cnt; i++) {
		const int party_member = party_man->get_member(i);
		Actor*    person       = get_npc(party_member);
		if (person) {
			a_list[n++] = person;
		}
	}
	return n;    // Return # actually stored.
}

/*
 *  Find a given shaped item amongst the party, and 'activate' it.  This
 *  is used, for example, by the 'f' command to feed.
 *
 *  Output: True if the item was found, else false.
 */

bool Game_window::activate_item(
		int shnum,    // Desired shape.
		int frnum,    // Desired frame
		int qual      // Desired quality
) {
	Actor*    party[9];    // Get party.
	const int cnt = get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		Actor*       person = party[i];
		Game_object* obj    = person->find_item(shnum, qual, frnum);
		if (obj) {
			obj->activate();
			return true;
		}
	}
	// Special case: Archwizard mode spellbook - create a temporary one with
	// all spells.
	if (shnum == 761 && cheat.in_wizard_mode()) {
		static unsigned char all_spells[9]    = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		static auto          wizard_spellbook = std::make_unique<Spellbook_object>(761, 0, 0, 0, 0, all_spells, 255);
		wizard_spellbook->activate();
		return true;
	}
	return false;
}

/*
 *  Find the top object that can be selected, dragged, or activated.
 *  The one returned is the 'highest'.
 *
 *  Output: ->object, or null if none.
 */

Game_object* Game_window::find_object(
		int x, int y    // Pos. on screen.
) {
#ifdef DEBUG
	cout << "Clicked at tile (" << get_scrolltx() + x / c_tilesize << ", " << get_scrollty() + y / c_tilesize << ")" << endl;
#endif
	const int not_above = get_render_skip_lift();
	// Figure chunk #'s.
	const int start_cx = ((scrolltx + x / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	const int start_cy = ((scrollty + y / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	// Check 1 chunk down & right too.
	const int stop_cx = (2 + (scrolltx + (x + 4 * not_above) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;
	const int stop_cy = (2 + (scrollty + (y + 4 * not_above) / c_tilesize) / c_tiles_per_chunk) % c_num_chunks;

	Game_object* best  = nullptr;    // Find 'best' one.
	bool         trans = true;       // Try to avoid 'transparent' objs.
	// Go through them.
	for (int cy = start_cy; cy != stop_cy; cy = INCR_CHUNK(cy)) {
		for (int cx = start_cx; cx != stop_cx; cx = INCR_CHUNK(cx)) {
			Map_chunk* olist = map->get_chunk(cx, cy);
			if (!olist) {
				continue;
			}
			Object_iterator next(olist->get_objects());
			Game_object*    obj;
			while ((obj = next.get_next()) != nullptr) {
				if (obj->get_lift() >= not_above || !get_shape_rect(obj).has_world_point(x, y) || !obj->is_findable()) {
					continue;
				}
				// Check the shape itself.
				Shape_frame* s = obj->get_shape();
				int          ox;
				int          oy;
				get_shape_location(obj, ox, oy);
				if (!s->has_point(x - ox, y - oy)) {
					continue;
				}
				// Fixes key under rock in BG at [915, 2434, 0]; need to
				// know if there are side effects.
				if (!best || best->lt(*obj) != 0 || trans) {
					bool ftrans = obj->get_info().is_transparent();
					// TODO: Fix this properly, instead of with an ugly hack.
					// This fixes clicking the Y shapes instead of the Y
					// depression in SI. This has the effect of also making
					// the Y depressions not draggable.
					if (GAME_SI && obj->get_shapenum() == 0xd1 && obj->get_framenum() == 17) {
						ftrans = true;    // Pretend it is transparent
					}
					if (!ftrans || trans) {
						best  = obj;
						trans = ftrans;
					}
				}
			}
		}
	}
	return best;
}

void Game_window::find_nearby_objects(Game_object_map_xy& mobjxy, int x, int y, Gump* gump) {
	// Find object at each pixel
	for (int iy = y - 10; iy < (y + 10); iy++) {
		for (int ix = x - 10; ix < (x + 10); ix++) {
			Game_object* iobj;
			if (gump) {
				iobj = gump->find_object(ix, iy);
			} else {
				iobj = find_object(ix, iy);
			}

			if (iobj) {
				const Shape_info& info = iobj->get_info();
				auto              cl   = info.get_shape_class();
				// Filter list a bit.
				if (cl == Shape_info::unusable || cl == Shape_info::building) {
					continue;
				}
				if ((cl == Shape_info::hatchable || cl == Shape_info::barge || info.is_barge_part()) && !cheat.in_map_editor()) {
					continue;
				}
				if (info.is_transparent()) {
					continue;
				}
				mobjxy[iobj] = Position2d{ix, iy};
			}
		}
	}
}

static inline string Get_object_name(const Game_object* obj) {
	if (obj == Game_window::get_instance()->get_main_actor()) {
		if (GAME_BG) {
			return get_misc_name(0x42);
		} else if (GAME_SI) {
			return get_misc_name(0x4e);
		} else {
			return obj->get_name();
		}
	}
	return obj->get_name();
}

/*
 *  Show the name of the item the mouse is clicked on.
 */

void Game_window::show_items(
		int x, int y,    // Coords. in window.
		bool ctrl        // Control key is pressed.
) {
	// Look for obj. in open gump.
	Gump* gump = gump_man->find_gump(x, y);
	int   gx;
	int   gy;
	gump_man->map_game_to_gump(gump, x, y, gx, gy);
	Game_object* obj;    // What we find.
	bool         found_in_gump = false;
	if (gump) {
		obj           = gump->find_object(gx, gy);
		found_in_gump = (obj != nullptr);
		if (!obj) {
			obj = gump->get_cont_or_actor(gx, gy);
		}
	} else {    // Search rest of world.
		obj = find_object(x, y);
	}

	if (item_menu) {
		Game_object_map_xy mobjxy;
		find_nearby_objects(mobjxy, x, y, gump);
		if (!mobjxy.empty() && Notebook_gump::get_instance() == nullptr) {
			// Make sure menu is visible on the screen
			Itemmenu_gump itemgump(&mobjxy, x, y);
			Game_window::get_instance()->get_gump_man()->do_modal_gump(&itemgump, Mouse::hand, itemgump.get_outline_painter());
			obj = nullptr;
		}
	}

	// Map-editing?
	if (obj && cheat.in_map_editor()) {
		if (ctrl) {    // Control?  Toggle.
			cheat.toggle_selected(obj);
		} else {
			// In normal mode, sel. just this one.
			cheat.clear_selected();
			if (cheat.get_edit_mode() == Cheat::move) {
				cheat.append_selected(obj);
			}
		}
	} else {    // All other cases:  unselect.
		cheat.clear_selected();
	}

	// Do we have an NPC?
	Actor* npc = obj ? obj->as_actor() : nullptr;
	if (npc && cheat.number_npcs() && (npc->get_npc_num() > 0 || npc == main_actor)) {
		char              str[64];
		const std::string namestr = Get_object_name(obj);
		snprintf(str, sizeof(str), "(%i) %s", npc->get_npc_num(), namestr.c_str());
		effects->add_text(str, obj);
	} else if (obj) {
		// Show name.  If clicking empty space in a gump with a custom name,
		// use that instead of the container object's name.
		std::string namestr;
		const char* gname = (gump && !found_in_gump) ? gump->get_click_name() : nullptr;
		if (gname) {
			namestr = gname;
		} else {
			namestr = Get_object_name(obj);
		}
		if (Game_window::get_instance()->failed_copy_protection() && (npc == main_actor || !npc)) {    // Avatar and items
			namestr = Strings::Oink();
		}
		// Combat and an NPC?
		if (in_combat() && Combat::mode != Combat::original && npc) {
			char buf[128];
			snprintf(buf, sizeof(buf), " (%d)", npc->get_property(Actor::health));
			namestr += buf;
		}
		effects->add_text(namestr.c_str(), obj);
	} else if (cheat.in_map_editor() && skip_lift > 0) {
		// Show flat, but not when editing ter.
		const ShapeID id = get_flat(x, y);
		char          str[20];
		snprintf(str, sizeof(str), "Flat %d:%d", id.get_shapenum(), id.get_framenum());
		effects->add_text(str, x, y);
	}
	// If it's an actor and we want to grab the actor, grab it.
	if (npc && cheat.grabbing_actor() && (npc->get_npc_num() || npc == main_actor)) {
		cheat.set_grabbed_actor(npc);
	}

#ifdef DEBUG
	int shnum;
	if (obj) {
		shnum                   = obj->get_shapenum();
		const int         frnum = obj->get_framenum();
		const Shape_info& info  = obj->get_info();
		cout << "Object " << shnum << ':' << frnum << " has 3d tiles (x, y, z): " << info.get_3d_xtiles(frnum) << ", "
			 << info.get_3d_ytiles(frnum) << ", " << info.get_3d_height();
		Actor* npc = obj->as_actor();
		if (npc) {
			cout << ", sched = " << npc->get_schedule_type() << ", real align = " << npc->get_alignment()
				 << ", eff. align = " << npc->get_effective_alignment() << ", npcnum = " << npc->get_npc_num();
		}
		cout << endl;
		if (obj->get_chunk()) {
			const Tile_coord t = obj->get_tile();
			cout << "tx = " << t.tx << ", ty = " << t.ty << ", tz = " << t.tz << ", ";
		}
		cout << "quality = " << obj->get_quality() << ", okay_to_take = " << obj->get_flag(Obj_flags::okay_to_take)
			 << ", flag0x1d = " << obj->get_flag(0x1d) << ", hp = " << obj->get_obj_hp() << ", weight = " << obj->get_weight()
			 << ", volume = " << obj->get_volume() << endl;
		cout << "obj = " << obj << endl;
		if (obj->get_flag(Obj_flags::asleep)) {
			cout << "ASLEEP" << endl;
		}
		if (obj->is_egg()) {    // Show egg info. here.
			obj->as_egg()->print_debug();
		}
	} else {    // Obj==0
		const ShapeID id = get_flat(x, y);
		shnum            = id.get_shapenum();
		cout << "Clicked on flat shape " << shnum << ':' << id.get_framenum() << endl;

#	ifdef CHUNK_OBJ_DUMP
		Map_chunk* chunk = map->get_chunk_safely(x / c_tiles_per_chunk, y / c_tiles_per_chunk);
		if (chunk) {
			Object_iterator it(chunk->get_objects());
			Game_object*    each;
			cout << "Chunk Contents: " << endl;
			while ((each = it.get_next()) != nullptr) {
				cout << "    " << each->get_name() << ":" << each->get_shapenum() << ":" << each->get_framenum() << endl;
			}
		}
#	endif
		if (id.is_invalid()) {
			return;
		}
	}
	const Shape_info& info = ShapeID::get_info(shnum);
	cout << "TFA[1][0-6]= " << (info.get_tfa(1) & 127) << endl;
	cout << "TFA[0][0-1]= " << (info.get_tfa(0) & 3) << endl;
	cout << "TFA[0][3-4]= " << ((info.get_tfa(0) >> 3) & 3) << endl;
	if (info.is_animated()) {
		cout << "Object is ANIMATED" << endl;
	}
	if (info.has_translucency()) {
		cout << "Object has TRANSLUCENCY" << endl;
	}
	if (info.is_transparent()) {
		cout << "Object is TRANSPARENT" << endl;
	}
	if (info.is_light_source()) {
		cout << "Object is LIGHT_SOURCE" << endl;
	}
	if (info.is_door()) {
		cout << "Object is a DOOR" << endl;
	}
	if (info.is_solid()) {
		cout << "Object is SOLID" << endl;
	}
#endif
}

/*
 *  Handle right click when combat is paused.  The user can right-click
 *  on a party member, then on an enemy to attack.
 */

void Game_window::paused_combat_select(
		int x, int y    // Coords in window.
) {
	Gump* gump = gump_man->find_gump(x, y);
	if (gump) {
		return;    // Ignore if clicked on gump.
	}
	Game_object* obj = find_object(x, y);
	Actor*       npc = obj ? obj->as_actor() : nullptr;
	if (!npc || !npc->is_in_party() || npc->get_flag(Obj_flags::asleep) || npc->is_dead() || npc->get_flag(Obj_flags::paralyzed)
		|| !npc->can_act_charmed()) {
		return;    // Want an active party member.
	}
	npc->paint_outline(PROTECT_PIXEL);
	show(true);    // Flash white outline.
	SDL_Delay(100);
	npc->add_dirty();
	paint_dirty();
	show();
	// Pick a spot.
	if (!Get_click(x, y, Mouse::greenselect, nullptr, true)) {
		return;
	}
	obj = find_object(x, y);    // Find it.
	if (!obj) {                 // Nothing?  Walk there.
		// Needs work if lift > 0.
		const int        lift       = npc->get_lift();
		const int        liftpixels = 4 * lift;
		const Tile_coord dest(scrolltx + (x + liftpixels) / c_tilesize, scrollty + (y + liftpixels) / c_tilesize, lift);
		// Aim within 1 tile.
		if (!npc->walk_path_to_tile(dest, std_delay, 0, 1)) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
		} else {    // Make sure he's in combat mode.
			npc->set_target(nullptr, true);
		}
		return;
	}
	Actor* target = obj->as_actor();
	// Don't attack party or body.
	if ((target && target->is_in_party()) || obj->get_info().is_body_shape()) {
		Mouse::mouse()->flash_shape(Mouse::redx);
		return;
	}
	npc->set_target(obj, true);
	obj->paint_outline(HIT_PIXEL);    // Flash red outline.
	show(true);
	SDL_Delay(100);
	add_dirty(obj);
	paint_dirty();
	show();
	if (npc->get_schedule_type() != Schedule::combat) {
		npc->set_schedule_type(Schedule::combat);
	}
}

/*
 *  Get the 'flat' that a screen point is in.
 */

ShapeID Game_window::get_flat(
		int x, int y    // Window point.
) {
	int       tx     = (get_scrolltx() + x / c_tilesize) % c_num_tiles;
	int       ty     = (get_scrollty() + y / c_tilesize) % c_num_tiles;
	const int cx     = tx / c_tiles_per_chunk;
	const int cy     = ty / c_tiles_per_chunk;
	tx               = tx % c_tiles_per_chunk;
	ty               = ty % c_tiles_per_chunk;
	Map_chunk* chunk = map->get_chunk(cx, cy);
	ShapeID    id    = chunk->get_flat(tx, ty);
	return id;
}

/*
 *  Handle a double-click.
 */

void Game_window::double_clicked(
		int x, int y    // Coords in window.
) {
	// Animation in progress?
	if (main_actor_dont_move()) {
		return;
	}
	// Nothing going on?
	// Look for obj. in open gump.
	Game_object* obj            = nullptr;
	const bool   gump           = gump_man->double_clicked(x, y, obj);
	const bool   avatar_can_act = main_actor_can_act();

	// If gump manager didn't handle it, we search the world for an object
	if (!gump) {
		obj = find_object(x, y);
		if (!avatar_can_act && obj && obj->as_actor() && obj->as_actor() == main_actor->as_actor()) {
			ActionFileGump(nullptr);
			return;
		}
		// Check path, except if an NPC, sign, or if editing.
		if (obj && !obj->as_actor() && !cheat.in_hack_mover() &&
			//! Is_sign(obj->get_shapenum()) &&
			!Fast_pathfinder_client::is_grabable(main_actor, obj)) {
			Mouse::mouse()->flash_shape(Mouse::blocked);
			return;
		}
	}
	if (!obj || !avatar_can_act) {
		return;    // Nothing found or avatar disabled.
	}
	if (combat && !gump &&    // In combat?
		!Combat::is_paused() && main_actor_can_act_charmed() && (!gump_man->gump_mode() || gump_man->gumps_dont_pause_game())) {
		Actor*                        npc  = obj->as_actor();
		const Shape_info&             info = obj->get_info();
		const Shape_info::Shape_class cls  = info.get_shape_class();
		// But don't attack party members.
		if ((!npc || !npc->is_in_party()) &&
			// Or bodies.
			!obj->get_info().is_body_shape()) {
			// And don't attack unlocked doors
			if ((info.is_door() && obj->get_framenum() % 4 < 2) ||
				// or unlocked containers
				(cls == Shape_info::container && !info.is_container_locked())) {
				obj->activate();
				return;
			} else {
				// In combat mode.
				// Want everyone to be in combat.
				combat = false;
				main_actor->set_target(obj);
				toggle_combat();
				return;
			}
		}
	}
	effects->remove_text_effects();    // Remove text msgs. from screen.
#ifdef DEBUG
	cout << "Object name is " << obj->get_name() << endl;
#endif
	usecode->init_conversation();
	// Check if CTRL is pressed to open basic properties instead
	const bool ctrl_pressed = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
	obj->activate(ctrl_pressed ? 2 : 1);
	npc_prox->wait(4);    // Delay "barking" for 4 secs.
}

/*
 *  Add an NPC to the 'nearby' list.
 */

void Game_window::add_nearby_npc(Npc_actor* npc) {
	if (!npc->is_nearby()) {
		npc->set_nearby();
		npc_prox->add(Game::get_ticks(), npc);
	}
}

/*
 *  Remove an NPC from the nearby list.
 */

void Game_window::remove_nearby_npc(Npc_actor* npc) {
	if (npc->is_nearby()) {
		npc_prox->remove(npc);
	}
}

/*
 *  Add all nearby NPC's to the given list.
 */

void Game_window::get_nearby_npcs(Actor_vector& a_list) const {
	npc_prox->get_all(a_list);
}

/*
 *  Tell all npc's to update their schedules at a new 3-hour period.
 */

void Game_window::schedule_npcs(
		int  hour,    // 24 hour
		bool repaint) {
	// Go through npc's, skipping Avatar.
	for (auto& npc : npcs) {
		if (!npc) {
			continue;
		}
		// Don't want companions leaving.
		if (npc->get_schedule_type() != Schedule::wait
			&& (npc->get_schedule_type() != Schedule::combat || npc->get_target() == nullptr)) {
			npc->update_schedule(hour / 3, hour % 3 == 0 ? -1 : 0);
		}
	}

	if (repaint) {
		paint();    // Repaint all.
	}
}

/*
 *  Tell all npc's to restore some of their HP's and/or mana on the hour.
 */

void Game_window::mend_npcs() {
	// Go through npc's.
	for (auto& npc : npcs) {
		if (npc) {
			npc->mend_wounds(true);
		}
	}
}

/*
 *  Get guard shape.
 */

int Game_window::get_guard_shape() {
	// Base on avatar's position.
	const Tile_coord pos = Game_window::get_instance()->main_actor->get_tile();
	if (!GAME_SI) {    // Default (BG).
		return 0x3b2;
	}
	// Moonshade?
	if (pos.tx >= 2054 && pos.ty >= 1698 && pos.tx < 2590 && pos.ty < 2387) {
		return 0x103;    // Ranger.
	}
	// Fawn?
	if (pos.tx >= 895 && pos.ty >= 1604 && pos.tx < 1173 && pos.ty < 1960) {
		return 0x17d;    // Fawn guard.
	}
	if (pos.tx >= 670 && pos.ty >= 2430 && pos.tx < 1135 && pos.ty < 2800) {
		return 0xe4;    // Pikeman.
	}
	return -1;    // No local guard.
}

/*
 *  Find a witness to the Avatar's thievery.
 *
 *  Output: ->witness, or nullptr.
 *      closest_npc = closest one that's nearby.
 */

Actor* Game_window::find_witness(
		Actor*& closest_npc,    // Closest one returned.
		int     align           // Desired alignment.
) {
	Actor_vector nearby;    // See if someone is nearby.
	main_actor->find_nearby_actors(nearby, c_any_shapenum, 12, 0x28);
	closest_npc                    = nullptr;    // Look for closest NPC.
	int       closest_dist         = 5000;
	Actor*    witness              = nullptr;    // And closest facing us.
	int       closest_witness_dist = 5000;
	const int gshape               = get_guard_shape();
	for (auto& npc : nearby) {
		// Want non-party intelligent and not disabled NPCs only.
		if (npc->is_in_party() || !npc->is_sentient() || !npc->can_act()) {
			continue;
		}
		// Witnesses must either match desired alignment or they must be
		// local guards and the alignment must be neutral. This makes guards
		// assist neutral and chaotic NPCs.
		if (npc->get_effective_alignment() != align || (npc->get_shapenum() == gshape && align != Actor::neutral)) {
			continue;
		}
		const int dist = npc->distance(main_actor);
		if (dist >= closest_witness_dist || !Fast_pathfinder_client::is_grabable(npc, main_actor)) {
			continue;
		}
		// Looking toward Avatar?
		auto      sched   = static_cast<Schedule::Schedule_types>(npc->get_schedule_type());
		const int dir     = npc->get_direction(main_actor);
		const int facing  = npc->get_dir_facing();
		const int dirdiff = (dir - facing + 8) % 8;
		if (dirdiff < 3 || dirdiff > 5 || sched == Schedule::hound) {
			// Yes.
			witness              = npc;
			closest_witness_dist = dist;
		} else if (dist < closest_dist) {
			closest_npc  = npc;
			closest_dist = dist;
		}
	}
	return witness;
}

/*
 *  Handle theft.
 */

void Game_window::theft() {
	// See if in a new location.
	const int cx = main_actor->get_cx();
	const int cy = main_actor->get_cy();
	if (cx != theft_cx || cy != theft_cy) {
		theft_cx       = cx;
		theft_cy       = cy;
		theft_warnings = 0;
	}
	Actor* closest_npc;
	Actor* witness = find_witness(closest_npc, Actor::neutral);
	if (!witness) {
		if (closest_npc && rand() % 2) {
			if (closest_npc->is_goblin()) {
				closest_npc->say(goblin_heard_something);
			} else if (closest_npc->can_speak()) {
				closest_npc->say(heard_something);
			}
		}
		return;    // Didn't get caught.
	}
	const bool avainvisible = main_actor->get_flag(Obj_flags::invisible);
	if (avainvisible && !witness->can_see_invisible()) {
		if (witness->is_goblin()) {
			witness->say(goblin_invis_theft);
		} else if (witness->can_speak()) {
			witness->say(first_invis_theft, last_invis_theft);
		}
		return;    // Didn't get caught because was invisible.
	}
	const int dir = witness->get_direction(main_actor);
	// Face avatar.
	witness->change_frame(witness->get_dir_framenum(dir, Actor::standing));
	// If not in combat, change to hound schedule.
	auto sched = static_cast<Schedule::Schedule_types>(witness->get_schedule_type());
	if (sched != Schedule::combat && sched != Schedule::hound) {
		witness->set_schedule_type(Schedule::hound);
	}
	theft_warnings++;
	if (theft_warnings < 2 + rand() % 3) {
		// Just a warning this time.
		if (witness->is_goblin()) {
			witness->say(goblin_theft);
		} else if (witness->can_speak()) {
			witness->say(first_theft, last_theft);
		}
		return;
	}
	gump_man->close_all_gumps();    // Get gumps off screen.
	call_guards(witness, true);
}

/*
 *  Create a guard to arrest the Avatar.
 */

void Game_window::call_guards(
		Actor* witness,    // ->witness, or nullptr to find one.
		bool   theft       // called from Game_window::theft
) {
	Actor* closest;
	if (armageddon || in_dungeon) {
		return;
	}
	const int gshape = get_guard_shape();
	const int align  = witness ? witness->get_effective_alignment() : Actor::neutral;
	if (witness || (witness = find_witness(closest, align)) != nullptr) {
		if (witness->is_goblin()) {
			if (gshape < 0) {
				witness->say(goblin_need_help);
			} else if (theft) {
				witness->say(goblin_call_guards_theft);
			} else {
				witness->say(first_goblin_call_guards, last_goblin_call_guards);
			}
		} else if (witness->can_speak()) {
			if (gshape < 0) {
				witness->say(first_need_help, last_need_help);
			} else if (theft) {
				witness->say(first_call_guards_theft, last_call_guards_theft);
			} else {
				witness->say(first_call_guards, last_call_guards);
			}
		}
	}
	if (gshape < 0) {    // No local guards; lets forward to attack_avatar.
		attack_avatar(0, align);
		return;
	}

	const Tile_coord dest = Map_chunk::find_spot(main_actor->get_tile(), 5, gshape, 0, 1);
	if (dest.tx != -1) {
		// Show guard running up.
		// Create it off-screen.
		const int        numguards = 1 + rand() % 3;
		const Tile_coord offscreen(main_actor->get_tile() + Tile_coord(128, 128, 0));
		// Start in combat if avatar is fighting.
		// FIXME: Disabled for now, as causes guards to attack
		// avatar if you break glass, when they should arrest
		// instead.
		const Schedule::Schedule_types sched = /*combat ? Schedule::combat
											  :*/
				Schedule::arrest_avatar;
		for (int i = 0; i < numguards; i++) {
			const Game_object_shared new_guard = Monster_actor::create(gshape, offscreen, sched, Actor::chaotic);
			auto*                    guard     = static_cast<Monster_actor*>(new_guard.get());
			add_nearby_npc(guard);
			guard->approach_another(main_actor);
		}
		// Guaranteed way to do it.
		MyMidiPlayer* player = Audio::get_ptr()->get_midi();
		if (player && !Background_noise::is_combat_music(player->get_current_track())) {
			Audio::get_ptr()->start_music(Audio::game_music(10), true);
		}
	}
}

/*
 *  Makes guards stop trying to arrest the avatar. Based on what it seems to
 *  do in SI, as well as what usecode seems to want to do.
 */

void Game_window::stop_arresting() {
	if (armageddon || in_dungeon) {
		return;
	}
	const int gshape = get_guard_shape();
	if (gshape < 0) {    // No one to calm down.
		return;
	}

	Actor_vector nearby;    // See if someone is nearby.
	main_actor->find_nearby_actors(nearby, gshape, 20, 0x28);
	for (auto& npc : nearby) {
		if (!npc->is_in_party() && npc->get_schedule_type() == Schedule::arrest_avatar) {
			npc->set_schedule_type(Schedule::wander);
			// Prevent guard from becoming hostile.
			npc->set_alignment(Actor::neutral);
		}
	}
}

/*
 *  Have nearby residents attack the Avatar.
 */

void Game_window::attack_avatar(
		int create_guards,    // # of extra guards to create.
		int align) {
	if (armageddon) {
		return;
	}
	const int gshape = get_guard_shape();
	if (gshape >= 0 && !in_dungeon) {
		while (create_guards--) {
			// Create it off-screen.
			const Game_object_shared new_guard = Monster_actor::create(
					gshape, main_actor->get_tile() + Tile_coord(128, 128, 0), Schedule::combat, Actor::chaotic);
			auto* guard = static_cast<Monster_actor*>(new_guard.get());
			add_nearby_npc(guard);
			guard->set_target(main_actor, true);
			guard->approach_another(main_actor);
		}
	}

	int          numhelpers = 0;
	const int    maxhelpers = 3;
	Actor_vector nearby;    // See if someone is nearby.
	main_actor->find_nearby_actors(nearby, c_any_shapenum, 20, 0x28);
	for (auto& npc : nearby) {
		if (npc->can_act() && !npc->is_in_party() && npc->is_sentient()
			&& ((npc->get_shapenum() == gshape && !in_dungeon) || align == npc->get_effective_alignment()) &&
			// Only if can get there.
			Fast_pathfinder_client::is_grabable(npc, main_actor)) {
			npc->set_target(main_actor, true);
			if (++numhelpers >= maxhelpers) {
				break;
			}
		}
	}
	// Guaranteed way to do it.
	MyMidiPlayer* player = Audio::get_ptr()->get_midi();
	if (player && !Background_noise::is_combat_music(player->get_current_track())) {
		Audio::get_ptr()->start_music(Audio::game_music(10), true);
	}
}

/*
 *  Gain/lose focus.
 */

void Game_window::get_focus() {
	cout << "Game resumed" << endl;
	Audio::get_ptr()->resume_audio();
	focus = true;
	tqueue->resume(Game::get_ticks());
}

void Game_window::lose_focus() {
	if (!focus) {
		return;    // Fixes SDL bug.
	}
	cout << "Game paused" << endl;

	string str;
	config->value("config/audio/disablepause", str, "no");
	if (str == "no") {
		Audio::get_ptr()->pause_audio();
	}

#if defined(SDL_PLATFORM_IOS) || defined(ANDROID)
	// Quick saving to make sure no game progress gets lost
	// when the app goes into background.
	write();
#endif

	focus = false;
	tqueue->pause(Game::get_ticks());
}

/*
 *  Prepare for game
 */

void Game_window::setup_game(bool map_editing) {
	// Map 0 always exists.
	get_map(0)->init();
	if (is_system_path_defined("<PATCH>")) {
		// There is a patch dir; search for other maps.
		for (int i = 1; i <= 0xFF; i++) {
			char fname[128];
			if (U7exists(Get_mapped_name(PATCH_U7MAP, i, fname))) {
				get_map(i)->init();
			}
		}
	}
	// Init. current 'tick'.
	Game::set_ticks(SDL_GetTicks());
	Face_stats::RemoveGump();    // it tries to update when reading actors so
								 // delete it until finished loading
	init_actors();               // Set up actors if not already done.
	// This also sets up initial
	// schedules and positions.

	cycle_load_palette();

	Notebook_gump::initialize();    // Read in journal.

	// read autonotes
	std::string d;
	std::string autonotesfilename;
	d = "config/disk/game/" + Game::get_gametitle() + "/autonotes";
	config->value(d.c_str(), autonotesfilename, "(default)");
	if (autonotesfilename == "(default)") {
		config->set(d.c_str(), autonotesfilename, true);
		Notebook_gump::read_auto_text();
	} else {
		try {
			Notebook_gump::read_auto_text_file(autonotesfilename.c_str());
		} catch (file_open_exception&) {
			cerr << "Autonotes file '" << autonotesfilename << "' not found, falling back to default autonotes." << endl;
			Notebook_gump::read_auto_text();
		}
	}

	usecode->read();    // Read the usecode flags
	cycle_load_palette();

	if (GAME_BG && !map_editing) {
		string yn;    // Override from config. file.
		// Skip intro. scene?
		config->value("config/gameplay/skip_intro", yn, "no");
		if (yn == "yes") {
			usecode->set_global_flag(Usecode_machine::did_first_scene, true);
		}

		// Should Avatar be visible?
		if (usecode->get_global_flag_bool(Usecode_machine::did_first_scene)) {
			main_actor->clear_flag(Obj_flags::bg_dont_render);
		} else {
			main_actor->set_flag(Obj_flags::bg_dont_render);
		}
	}

	cycle_load_palette();

	// Fade out & clear screen before palette change
	pal->fade_out(c_fade_out_time);
	clear_screen(true);
	load_palette_timer = 0;

	// note: we had to stop the plasma here already, because init_readied
	// and activate_eggs may update the screen through usecode functions
	// (Helm of Light, for example)
	Actor*    party[9];
	const int cnt = get_party(party, 1);    // Get entire party.
	for (int i = 0; i < cnt; i++) {         // Init. rings.
		party[i]->init_readied();
	}
	// Ensure time is stopped if map-editing.
	time_stopped = map_editing ? -1 : 0;
	//+++++The below wasn't prev. done by ::read(), so maybe it should be
	//+++++controlled by a 'first-time' flag.
	// Want to activate first egg.
	Map_chunk* olist = main_actor->get_chunk();
	olist->setup_cache();

	const Tile_coord t = main_actor->get_tile();
	// Do them immediately.
	if (!map_editing) {    // (Unless map-editing.)
		olist->activate_eggs(main_actor, t.tx, t.ty, t.tz, -1, -1, true);
	}
	// Force entire repaint.
	set_all_dirty();
	painted = true;                     // Main loop uses this.
	gump_man->close_all_gumps(true);    // Kill gumps.
	Face_stats::load_config(config);
	if (using_shortcutbar()) {
		g_shortcutBar = new ShortcutBar_gump(0, 0);
	}
	if (touchui != nullptr) {
		touchui->showButtonControls();
		touchui->showGameControls();
	}
	// During the first scene do not show the UI elements,
	// unless the Avatar is free to move (possibly in mods).
	if ((GAME_BG && !usecode->get_global_flag_bool(Usecode_machine::did_first_scene))
		|| (GAME_SI && !usecode->get_global_flag_bool(Usecode_machine::si_did_first_scene))
		|| (main_actor_dont_move() && !main_actor_can_act() && !main_actor_can_act_charmed())) {
		if (touchui != nullptr) {
			touchui->hideGameControls();
		}
		if (Face_stats::Visible()) {
			Face_stats::HideGump();
		}
		if (ShortcutBar_gump::Visible()) {
			ShortcutBar_gump::HideGump();
		}
	}

	// Set palette for time-of-day.
	clock->reset_palette();
	pal->fade(6, 1, -1);    // Fade back in.
}

void Game_window::plasma(int w, int h, int x, int y, int startc, int endc) {
	win->BeginPaintIntoGuardBand(&x, &y, &w, &h);
	Image_buffer8* ibuf = get_win()->get_ib8();

	ibuf->fill8(startc, w, h, x, y);

	for (int i = 0; i < w * h; i++) {
		const Uint8 pc = startc + rand() % (endc - startc + 1);
		const int   px = x + rand() % w;
		const int   py = y + rand() % h;

		for (int j = 0; j < 6; j++) {
			const int px2 = px + rand() % 17 - 8;
			const int py2 = py + rand() % 17 - 8;
			ibuf->fill8(pc, 3, 1, px2 - 1, py2);
			ibuf->fill8(pc, 1, 3, px2, py2 - 1);
		}
	}
	win->EndPaintIntoGuardBand();
	painted = true;
}

/*
 *  Chunk caching emulation: swap out chunks which are now at least
 *  3 chunks away.
 */
void Game_window::emulate_cache(Map_chunk* olist, Map_chunk* nlist) {
	if (!olist) {
		return;    // Seems like there's nothing to do.
	}
	// Cancel weather from eggs that are
	// far away.
	effects->remove_weather_effects(120);
	const int newx = nlist->get_cx();
	const int newy = nlist->get_cy();
	const int oldx = olist->get_cx();
	const int oldy = olist->get_cy();
	Game_map* omap = olist->get_map();
	Game_map* nmap = nlist->get_map();
	// Cancel scripts 4 chunks from this.
	Usecode_script::purge(Tile_coord(newx * c_tiles_per_chunk, newy * c_tiles_per_chunk, 0), 4 * c_tiles_per_chunk);
	int nearby[5][5];    // Chunks within 3.
	int x;
	int y;
	// Figure old range.
	const int old_minx = c_num_chunks + oldx - 2;
	const int old_maxx = c_num_chunks + oldx + 2;
	const int old_miny = c_num_chunks + oldy - 2;
	const int old_maxy = c_num_chunks + oldy + 2;
	if (nmap == omap) {    // Same map?
		// Set to 0
		memset(nearby, 0, sizeof(nearby));
		// Figure new range.
		const int new_minx = c_num_chunks + newx - 2;
		const int new_maxx = c_num_chunks + newx + 2;
		const int new_miny = c_num_chunks + newy - 2;
		const int new_maxy = c_num_chunks + newy + 2;
		// Now we write what we are now near
		for (y = new_miny; y <= new_maxy; y++) {
			if (y > old_maxy) {
				break;    // Beyond the end.
			}
			const int dy = y - old_miny;
			if (dy < 0) {
				continue;
			}
			assert(dy < 5);
			for (x = new_minx; x <= new_maxx; x++) {
				if (x > old_maxx) {
					break;
				}
				const int dx = x - old_minx;
				if (dx >= 0) {
					assert(dx < 5);
					nearby[dx][dy] = 1;
				}
			}
		}
	} else {    // New map, so cache out all of old.
		memset(nearby, 0, sizeof(nearby));
	}

	// Swap out chunks no longer nearby (0).
	Game_object_vector removes;
	for (y = 0; y < 5; y++) {
		for (x = 0; x < 5; x++) {
			if (nearby[x][y] != 0) {
				continue;
			}
			Map_chunk* list = omap->get_chunk_safely((old_minx + x) % c_num_chunks, (old_miny + y) % c_num_chunks);
			if (!list) {
				continue;
			}
			Object_iterator it(list->get_objects());
			Game_object*    each;
			while ((each = it.get_next()) != nullptr) {
				Egg_object* egg = each->as_egg();
				if (egg != nullptr && egg->reset()) {
					continue;
				} else if (each->get_flag(Obj_flags::is_temporary)) {
					removes.push_back(each);
				}
			}
		}
	}
	for (auto* remove : removes) {
#ifdef DEBUG
		const Tile_coord t = remove->get_tile();
		cout << "Culling object: " << remove->get_name() << '(' << remove << ")@" << t.tx << "," << t.ty << "," << t.tz << endl;
#endif
		remove->delete_contents();    // first delete item's contents
		remove->remove_this(nullptr);
	}

	if (omap == nmap) {
		omap->cache_out(newx, newy);
	} else {                        // Going to new map?
		omap->cache_out(-1, -1);    // Cache out whole of old map.
	}
}

// Tests to see if a move goes out of range of the actors superchunk
bool Game_window::emulate_is_move_allowed(int tx, int ty) {
	const int ax = camera_actor->get_cx() / c_chunks_per_schunk;
	const int ay = camera_actor->get_cy() / c_chunks_per_schunk;
	tx /= c_tiles_per_schunk;
	ty /= c_tiles_per_schunk;

	int difx = ax - tx;
	int dify = ay - ty;

	if (difx < 0) {
		difx = -difx;
	}
	if (dify < 0) {
		dify = -dify;
	}

	// Is it within 1 superchunk range?
	return (!difx || difx == 1 || difx == c_num_schunks || difx == c_num_schunks - 1)
		   && (!dify || dify == 1 || dify == c_num_schunks || dify == c_num_schunks - 1);
}

// create mini-screenshot (96x60) for use in savegames
unique_ptr<Shape_file> Game_window::create_mini_screenshot() {
	set_all_dirty();
	render->paint_map(0, 0, get_width(), get_height());

	unique_ptr<unsigned char[]> img(win->mini_screenshot());
	unique_ptr<Shape_file>      sh;
	if (img) {
		sh = make_unique<Shape_file>(make_unique<Shape_frame>(std::move(img), 96, 60, 0, 0, true));
	}

	set_all_dirty();
	paint();
	return sh;
}

#define BG_PLASMA_START_COLOR 128
#define BG_PLASMA_CYCLE_RANGE 80

#define SI_PLASMA_START_COLOR 16
#define SI_PLASMA_CYCLE_RANGE 96

void Game_window::setup_load_palette() {
	if (load_palette_timer != 0) {
		return;
	}

	if (GAME_BG) {
		plasma_start_color = BG_PLASMA_START_COLOR;
		plasma_cycle_range = BG_PLASMA_CYCLE_RANGE;
	} else {    // Default: if (GAME_SI)
		plasma_start_color = SI_PLASMA_START_COLOR;
		plasma_cycle_range = SI_PLASMA_CYCLE_RANGE;
	}

	// Put up the plasma to the screen
	plasma(win->get_full_width(), win->get_full_height(), win->get_start_x(), win->get_start_y(), plasma_start_color,
		   plasma_start_color + plasma_cycle_range - 1);

	// Load the palette
	if (GAME_BG) {
		pal->load(INTROPAL_DAT, PATCH_INTROPAL, 2);
	} else if (GAME_SI) {
		pal->load(MAINSHP_FLX, PATCH_MAINSHP, 1);
	}

	pal->apply();
	load_palette_timer = SDL_GetTicks();
}

void Game_window::cycle_load_palette() {
	if (load_palette_timer == 0) {
		return;
	}
	const uint32 ticks = SDL_GetTicks();
	if (ticks > load_palette_timer + 75) {
		for (int i = 0; i < 4; ++i) {
			get_win()->rotate_colors(plasma_start_color, plasma_cycle_range, 1);
		}
		show(true);

		// We query the timer here again, as the blit can take easily 50 ms and
		// more depending on the chosen scaler and the overall system speed
		load_palette_timer = SDL_GetTicks();
	}
}

bool Game_window::is_hostile_nearby() const {
	/* If there is a hostile NPC nearby, the avatar isn't allowed to
	 * move very fast
	 * Note that the range at which this occurs in the original is
	 * less than the "potential target" range- that is, if I go into
	 * combat mode, even when I'm allowed to run at full speed,
	 * I'll sometime charge off to kill someone "too far away"
	 * to affect a speed limit.
	 * I don't know whether this is taken into account by
	 * get_nearby_npcs, but on the other hand, its a negligible point.
	 */
	Actor_vector nearby;
	get_nearby_npcs(nearby);

	bool nearby_hostile = false;
	for (auto& actor : nearby) {
		if (!actor->is_dead() && actor->get_schedule() && actor->get_effective_alignment() >= Actor::evil
			&& ((actor->get_schedule_type() == Schedule::combat
				 && static_cast<Combat_schedule*>(actor->get_schedule())->has_started_battle())
				|| actor->get_schedule_type() == Schedule::arrest_avatar)) {
			/* TODO- I think invisibles still trigger the
			 * slowdown, verify this. */
			nearby_hostile = true;
			break; /* No need to bother checking the rest :P */
		}
	}
	return nearby_hostile;
}

bool Game_window::failed_copy_protection() {
	const bool confused    = main_actor->get_flag(Obj_flags::confused);
	const bool failureFlag = usecode->get_global_flag_bool(Usecode_machine::failed_copy_protect);
	return (GAME_SI && confused) || (GAME_BG && failureFlag);
}

void Game_window::got_bad_feeling(int odds) {
	if (!(GAME_BG || GAME_SI)) {
		return;
	}
	if ((rand() % odds) == 0) {
		const char* badfeeling = get_misc_name(GAME_BG ? 0x44 : 0x50);
		effects->remove_text_effect(main_actor);
		effects->add_text(badfeeling, main_actor);
	}
}

void Game_window::set_shortcutbar(uint8 s) {
	if (use_shortcutbar == s) {
		return;
	}
	if (using_shortcutbar() && s > 0) {
		use_shortcutbar = s;
		return;
	}
	use_shortcutbar = s;
	if (!is_in_exult_menu()) {
		if (using_shortcutbar()) {
			g_shortcutBar = new ShortcutBar_gump(0, 0);
		} else {
			gump_man->close_gump(g_shortcutBar);
			g_shortcutBar = nullptr;
		}
	}
}
