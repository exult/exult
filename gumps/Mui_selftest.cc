/*
Copyright (C) 2026 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Mui_selftest.h"

#include "Configuration.h"
#include "Gamemenu_gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Mui_journey_log.h"
#include "Mui_router.h"
#include "Mui_window.h"
#include "Newfile_gump.h"
#include "Yesno_gump.h"
#include "cheat.h"
#include "exult.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "keyactions.h"
#include "listfiles.h"
#include "mouse.h"
#include "ucmachine.h"
#include "vgafile.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

#if defined(__GNUG__)
#	include <cxxabi.h>
#endif

// Save game compression level (exult.cc).
extern int save_compression;

namespace {
	struct State {
		bool                     init    = false;
		bool                     enabled = false;
		bool                     done    = false;
		std::vector<std::string> lines;
		size_t                   pc        = 0;
		uint32                   next_time = 0;
		int                      pass      = 0;
		int                      fail      = 0;
		bool                     log_open  = false;    // Log the next gump seen.
		std::string              last_opened;
		Mui_window*              window         = nullptr;
		bool                     mod_set        = false;
		bool                     restored       = false;    // Last Journey Log loaded a game.
		bool                     escape_classic = false;    // Escape the next text-mode cheat menu.
		std::string              last_number;               // Result of prompt-number.
		std::string              last_answer;               // Result of ask.
	};

	State& state() {
		static State st;
		if (st.init) {
			return st;
		}
		st.init            = true;
		const char* script = std::getenv("EXULT_MUI_TEST");
		if (!script || !*script) {
			return st;
		}
		std::ifstream in(script);
		if (!in) {
			std::cout << "MUI-TEST FAIL script " << script << " got unreadable" << std::endl;
			st.fail++;
			st.enabled = true;
			return st;
		}
		std::string line;
		while (std::getline(in, line)) {
			const auto b = line.find_first_not_of(" \t\r");
			if (b == std::string::npos || line[b] == '#') {
				continue;
			}
			const auto e = line.find_last_not_of(" \t\r");
			line         = line.substr(b, e - b + 1);
			// Expand ${NAME} from the environment.
			for (size_t pos = line.find("${"); pos != std::string::npos; pos = line.find("${", pos)) {
				const size_t end = line.find('}', pos);
				if (end == std::string::npos) {
					break;
				}
				const char*       val = std::getenv(line.substr(pos + 2, end - pos - 2).c_str());
				const std::string rep = val ? val : "";
				line.replace(pos, end - pos + 1, rep);
				pos += rep.size();
			}
			st.lines.push_back(line);
		}
		st.enabled        = true;
		int         secs  = 90;
		const char* t_env = std::getenv("EXULT_MUI_TEST_TIMEOUT");
		if (t_env && std::atoi(t_env) > 0) {
			secs = std::atoi(t_env);
		}
		std::thread([secs] {
			std::this_thread::sleep_for(std::chrono::seconds(secs));
			std::cout << "MUI-TEST FAIL watchdog got timeout after " << secs << " s" << std::endl;
			std::cout.flush();
			std::_Exit(124);
		}).detach();
		std::cout << "MUI-TEST info script " << script << " (" << st.lines.size() << " commands)" << std::endl;
		return st;
	}

	void expect(bool ok, const std::string& what, const std::string& got) {
		State& st = state();
		if (ok) {
			st.pass++;
			std::cout << "MUI-TEST PASS " << what << std::endl;
		} else {
			st.fail++;
			std::cout << "MUI-TEST FAIL " << what << " got " << got << std::endl;
		}
	}

	void finish_script() {
		State& st = state();
		if (st.done) {
			return;
		}
		st.done = true;
		std::cout << "MUI-TEST DONE " << st.pass << " pass " << st.fail << " fail" << std::endl;
	}

	std::string class_name(const void* obj, const std::type_info& ti) {
		static_cast<void>(obj);
		std::string name = ti.name();
#if defined(__GNUG__)
		int   status = 0;
		char* dem    = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
		if (status == 0 && dem) {
			name = dem;
		}
		std::free(dem);
#endif
		return name;
	}

	SDL_Window* screen_window() {
		return Game_window::get_instance()->get_win()->get_screen_window();
	}

	// Layer pixel -> SDL window coordinates.
	bool layer_to_window(Mui_window* w, int lx, int ly, float& wx, float& wy) {
		int sx;
		int sy;
		if (!Game_window::get_instance()->get_win()->layer_to_screen(w->get_layer(), lx, ly, sx, sy)) {
			return false;
		}
		SDL_Renderer* r = SDL_GetRenderer(screen_window());
		return r && SDL_RenderCoordinatesToWindow(r, float(sx), float(sy), &wx, &wy);
	}

	void push_click(float wx, float wy, Uint8 button) {
		const SDL_WindowID id = SDL_GetWindowID(screen_window());
		SDL_Event          e{};
		e.type             = SDL_EVENT_MOUSE_MOTION;
		e.motion.windowID  = id;
		e.motion.x         = wx;
		e.motion.y         = wy;
		e.motion.timestamp = SDL_GetTicksNS();
		SDL_PushEvent(&e);
		e                  = SDL_Event{};
		e.type             = SDL_EVENT_MOUSE_BUTTON_DOWN;
		e.button.windowID  = id;
		e.button.button    = button;
		e.button.down      = true;
		e.button.clicks    = 1;
		e.button.x         = wx;
		e.button.y         = wy;
		e.button.timestamp = SDL_GetTicksNS();
		SDL_PushEvent(&e);
		e.type        = SDL_EVENT_MOUSE_BUTTON_UP;
		e.button.down = false;
		SDL_PushEvent(&e);
	}

	bool parse_key(const std::string& spec, SDL_Keycode& key, SDL_Keymod& mod) {
		mod              = SDL_KMOD_NONE;
		std::string name = spec;
		for (;;) {
			const auto plus = name.find('+');
			if (plus == std::string::npos || plus == 0) {
				break;
			}
			const std::string m = name.substr(0, plus);
			if (m == "shift") {
				mod = SDL_Keymod(mod | SDL_KMOD_LSHIFT);
			} else if (m == "ctrl") {
				mod = SDL_Keymod(mod | SDL_KMOD_LCTRL);
			} else if (m == "alt") {
				mod = SDL_Keymod(mod | SDL_KMOD_LALT);
			} else {
				break;
			}
			name = name.substr(plus + 1);
		}
		key = SDL_GetKeyFromName(name.c_str());
		return key != SDLK_UNKNOWN;
	}

	void push_key(SDL_Keycode key, SDL_Keymod mod) {
		SDL_Event e{};
		e.type          = SDL_EVENT_KEY_DOWN;
		e.key.windowID  = SDL_GetWindowID(screen_window());
		e.key.key       = key;
		e.key.scancode  = SDL_GetScancodeFromKey(key, nullptr);
		e.key.mod       = mod;
		e.key.down      = true;
		e.key.timestamp = SDL_GetTicksNS();
		SDL_PushEvent(&e);
		e.type     = SDL_EVENT_KEY_UP;
		e.key.down = false;
		SDL_PushEvent(&e);
	}

	int count_saves() {
		char mask[256];
		snprintf(mask, sizeof(mask), SAVENAME2, GAME_BG ? "bg" : GAME_SI ? "si" : "dev");
		FileList files;
		U7ListFiles(mask, files, true);
		return int(files.size());
	}

	bool cheat_flag(const std::string& which, bool& value) {
		if (which == "god") {
			value = cheat.in_god_mode();
		} else if (which == "wizard") {
			value = cheat.in_wizard_mode();
		} else if (which == "infravision") {
			value = cheat.in_infravision();
		} else if (which == "hackmover") {
			value = cheat.in_hack_mover();
		} else if (which == "eggs") {
			value = Game_window::get_instance()->paint_eggs;
		} else if (which == "pickpocket") {
			value = cheat.in_pickpocket();
		} else if (which == "grab") {
			value = cheat.grabbing_actor();
		} else if (which == "mapeditor") {
			value = cheat.in_map_editor();
		} else if (which == "tilegrid") {
			value = cheat.show_tile_grid();
		} else {
			return false;
		}
		return true;
	}

	void open_target(const std::string& what, const std::string& arg) {
		State& st   = state();
		st.log_open = true;
		auto* gman  = Game_window::get_instance()->get_gump_man();
		if (what == "menu") {
			ActionMenuGump(nullptr);
		} else if (what == "journey") {
			bool restored = false;
			if (Mui::open_journey_log(&restored)) {
				st.restored = restored;
				std::cout << "MUI-TEST info journey closed, restored " << (restored ? "yes" : "no") << std::endl;
			} else {
				ActionFileGump(nullptr);
			}
		} else if (what == "options") {
			if (!Mui::open_options(arg.empty() ? nullptr : arg.c_str())) {
				Gamemenu_gump menu;
				gman->do_modal_gump(&menu, Mouse::hand);
			}
		} else if (what == "cheats" || what == "classic-cheats") {
			// The classic cheat screen has its own loop the driver cannot
			// reach: queue Escape so it closes straight away.
			if (what == "classic-cheats" || !Mui::themed()) {
				push_key(SDLK_ESCAPE, SDL_KMOD_NONE);
				st.log_open    = false;
				st.last_opened = "CheatScreen";
				std::cout << "MUI-TEST info opened CheatScreen" << std::endl;
			}
			cheat.cheat_screen();
		} else if (what == "classic-menu") {
			Gamemenu_gump menu;
			gman->do_modal_gump(&menu, Mouse::hand);
		} else if (what == "classic-file") {
			auto* g = new Newfile_gump();
			gman->do_modal_gump(g, Mouse::hand);
			delete g;
		} else {
			st.log_open = false;
			expect(false, "open " + what, "unknown target");
		}
		st.log_open = false;
	}

	void run_command(Mui_window* win, Modal_gump* gump) {
		State& st = state();
		if (st.mod_set) {
			SDL_SetModState(SDL_KMOD_NONE);
			st.mod_set = false;
		}
		const std::string  line = st.lines[st.pc++];
		std::istringstream ss(line);
		std::string        cmd;
		ss >> cmd;
		std::string rest;
		std::getline(ss, rest);
		const auto b = rest.find_first_not_of(' ');
		rest         = b == std::string::npos ? std::string() : rest.substr(b);
		std::istringstream args(rest);
		std::cout << "MUI-TEST info > " << line << std::endl;
		Game_window* gwin = Game_window::get_instance();
		if (cmd == "wait") {
			int ms = 0;
			args >> ms;
			st.next_time = SDL_GetTicks() + uint32(std::max(0, ms));
		} else if (cmd == "style") {
			if (rest == "none") {
				config->remove("config/gameplay/menu_style", true);
			} else if (win) {
				win->switch_theme(rest);
			} else {
				config->set("config/gameplay/menu_style", rest, true);
			}
		} else if (cmd == "display") {
			int w = 0;
			int h = 0;
			args >> w >> h;
			Image_window* iw = gwin->get_win();
			gwin->resized(
					unsigned(w), unsigned(h), false, unsigned(iw->get_game_width()), unsigned(iw->get_game_height()),
					unsigned(iw->get_scale_factor()), unsigned(iw->get_scaler()), iw->get_fill_mode(),
					unsigned(iw->get_fill_scaler()));
			gwin->set_all_dirty();
		} else if (cmd == "open") {
			std::string what;
			std::string arg;
			args >> what >> arg;
			open_target(what, arg);
		} else if (cmd == "click") {
			TileRect r;
			if (!win || !win->widget_rect(rest, r)) {
				expect(false, "click " + rest, win ? "no such widget" : "no themed window");
			} else {
				win->force_paint();    // Layout must be current.
				win->widget_rect(rest, r);
				float wx;
				float wy;
				if (layer_to_window(win, r.x + r.w / 2, r.y + r.h / 2, wx, wy)) {
					push_click(wx, wy, SDL_BUTTON_LEFT);
				} else {
					expect(false, "click " + rest, "unmappable");
				}
			}
		} else if (cmd == "rclick") {
			// Right-click at a layer point (outside the window closes it).
			int x = 0;
			int y = 0;
			args >> x >> y;
			float wx;
			float wy;
			if (win && layer_to_window(win, x, y, wx, wy)) {
				push_click(wx, wy, SDL_BUTTON_RIGHT);
			}
		} else if (cmd == "key") {
			SDL_Keycode key;
			SDL_Keymod  mod;
			if (parse_key(rest, key, mod)) {
				if (mod != SDL_KMOD_NONE) {
					SDL_SetModState(mod);
					st.mod_set = true;
				}
				push_key(key, mod);
			} else {
				expect(false, "key " + rest, "unknown key");
			}
		} else if (cmd == "type") {
			// "..." keeps leading and trailing spaces.
			std::string text = rest;
			if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
				text = text.substr(1, text.size() - 2);
			}
			if (win) {
				win->text_input(text.c_str());
			}
		} else if (cmd == "select") {
			std::string id;
			int         idx = 0;
			args >> id >> idx;
			Mui_widget* w = win ? win->find(id) : nullptr;
			if (!w || !w->select_index(idx)) {
				expect(false, "select " + rest, "no such widget");
			}
		} else if (cmd == "tab") {
			TileRect r;
			if (win && win->widget_rect("mui.tabs." + rest, r)) {
				float wx;
				float wy;
				if (layer_to_window(win, r.x + r.w / 2, r.y + r.h / 2, wx, wy)) {
					push_click(wx, wy, SDL_BUTTON_LEFT);
				}
			} else {
				expect(false, "tab " + rest, "no such tab");
			}
		} else if (cmd == "shot") {
			if (win) {
				win->force_paint();
			} else {
				gwin->set_all_dirty();
				gwin->paint();
			}
			gwin->show(true);
			int           w   = 0;
			int           h   = 0;
			SDL_IOStream* dst = SDL_IOFromFile(rest.c_str(), "wb");
			const bool    ok  = dst && gwin->get_win()->screenshot_display(dst, w, h);
			std::cout << "MUI-TEST info shot " << rest << " " << w << "x" << h << std::endl;
			if (!ok) {
				expect(false, "shot " + rest, "write error");
			}
		} else if (cmd == "expect-config") {
			std::string key;
			std::string want;
			args >> key;
			std::getline(args, want);
			const auto wb = want.find_first_not_of(' ');
			want          = wb == std::string::npos ? std::string() : want.substr(wb);
			std::string got;
			if (config->key_exists(key)) {
				config->value(key, got, "");
			} else {
				got = "<absent>";
			}
			expect(got == want, "config " + key + " = " + want, got);
		} else if (cmd == "expect-widget") {
			std::string id;
			std::string want;
			args >> id;
			std::getline(args, want);
			const auto wb = want.find_first_not_of(' ');
			want          = wb == std::string::npos ? std::string() : want.substr(wb);
			Mui_widget* w = win ? win->find(id) : nullptr;
			expect(w && w->value_text() == want, "widget " + id + " = " + want, w ? w->value_text() : "<missing>");
		} else if (cmd == "expect-game-area") {
			// Game area in effect: W H.
			Image_window*     iw  = gwin->get_win();
			const std::string got = std::to_string(iw->get_game_width()) + " " + std::to_string(iw->get_game_height());
			expect(got == rest, "game area " + rest, got);
		} else if (cmd == "expect-no-widget") {
			Mui_widget* w = win ? win->find(rest) : nullptr;
			expect(win && !w, "no widget " + rest, w ? "present" : "no themed window");
		} else if (cmd == "expect-widget-atleast") {
			// The widget's value starts with a number of at least N.
			std::string id;
			int         want = 0;
			args >> id >> want;
			Mui_widget*       w   = win ? win->find(id) : nullptr;
			const std::string got = w ? w->value_text() : "<missing>";
			expect(w && std::atoi(got.c_str()) >= want, "widget " + id + " >= " + std::to_string(want), got);
		} else if (cmd == "expect-style") {
			// Theme of the themed window on top.
			const std::string got = win ? win->theme().name : "<no window>";
			expect(got == rest, "style " + rest, got);
		} else if (cmd == "expect-scale") {
			// UI scale of the window on top, in hundredths.
			int want = 0;
			args >> want;
			const int got = win ? int(win->get_scale() * 100 + 0.5f) : -1;
			expect(got == want, "scale " + std::to_string(want), std::to_string(got));
		} else if (cmd == "expect-frame") {
			// Size of the window on top in layer pixels: W H.
			const std::string got
					= win ? std::to_string(win->get_frame_rect().w) + " " + std::to_string(win->get_frame_rect().h) : "<no window>";
			std::cout << "MUI-TEST info frame " << got << std::endl;
			if (!rest.empty()) {
				expect(got == rest, "frame " + rest, got);
			}
		} else if (cmd == "prompt-number") {
			// The number prompt of drag and usecode: MIN MAX STEP DEFAULT.
			int minv = 0;
			int maxv = 0;
			int stp  = 1;
			int def  = 0;
			args >> minv >> maxv >> stp >> def;
			bool      escaped = false;
			const int got     = gwin->get_gump_man()->prompt_for_number(minv, maxv, stp, def, nullptr, &escaped);
			st.last_number    = std::to_string(got) + (escaped ? " escaped" : "");
			std::cout << "MUI-TEST info number " << st.last_number << std::endl;
		} else if (cmd == "expect-number") {
			expect(st.last_number == rest, "number " + rest, st.last_number);
		} else if (cmd == "ask") {
			// A classic Yes/No question (Yesno_gump::ask).
			st.last_answer = Yesno_gump::ask(rest.c_str()) ? "yes" : "no";
			std::cout << "MUI-TEST info answer " << st.last_answer << std::endl;
		} else if (cmd == "expect-answer") {
			expect(st.last_answer == rest, "answer " + rest, st.last_answer);
		} else if (cmd == "expect-focus") {
			Mui_widget* f = win ? win->get_focus() : nullptr;
			expect(f && f->id == rest, "focus " + rest, f ? f->id : "<none>");
		} else if (cmd == "expect-open") {
			expect(st.last_opened.find(rest) != std::string::npos, "opened " + rest, st.last_opened);
		} else if (cmd == "expect-window") {
			const bool want = rest != "none";
			expect(want == (win != nullptr), "window " + rest, win ? "open" : "none");
		} else if (cmd == "expect-cheat") {
			std::string which;
			std::string onoff;
			args >> which >> onoff;
			bool value = false;
			if (!cheat_flag(which, value)) {
				expect(false, "cheat " + rest, "unknown cheat");
			} else {
				expect(value == (onoff == "on"), "cheat " + rest, value ? "on" : "off");
			}
		} else if (cmd == "expect-time") {
			int d = -1;
			int h = -1;
			int m = -1;
			args >> d >> h >> m;
			Game_clock*       clk = gwin->get_clock();
			const std::string got = std::to_string(clk->get_day()) + " " + std::to_string(clk->get_hour()) + " "
									+ std::to_string(clk->get_minute());
			expect(clk->get_day() == d && clk->get_hour() == h && clk->get_minute() == m, "time " + rest, got);
		} else if (cmd == "expect-flag") {
			int         num = 0;
			std::string onoff;
			args >> num >> onoff;
			const bool value = gwin->get_usecode()->get_global_flag_bool(num);
			expect(value == (onoff == "on"), "global flag " + rest, value ? "on" : "off");
		} else if (cmd == "escape-classic") {
			st.escape_classic = true;
		} else if (cmd == "save-game") {
			int         num = 0;
			std::string name;
			args >> num;
			std::getline(args, name);
			const auto nb = name.find_first_not_of(' ');
			name          = nb == std::string::npos ? std::string() : name.substr(nb);
			gwin->write();
			gwin->save_gamedat(num, name.c_str());
		} else if (cmd == "save-compression") {
			// 0 = Flex saves, 1-2 = zip (when built with zip support).
			args >> save_compression;
		} else if (cmd == "expect-saves") {
			int want = 0;
			args >> want;
			const int got = count_saves();
			expect(got == want, "saves " + std::to_string(want), std::to_string(got));
			if (auto* log = dynamic_cast<Mui_journey_window*>(win)) {
				expect(log->saves_count() == want, "journey saves " + std::to_string(want), std::to_string(log->saves_count()));
			}
		} else if (cmd == "expect-visible") {
			int want = 0;
			args >> want;
			auto* log = dynamic_cast<Mui_journey_window*>(win);
			expect(log && log->visible_count() == want, "journey visible " + std::to_string(want),
				   log ? std::to_string(log->visible_count()) : "no Journey Log");
		} else if (cmd == "expect-card-name") {
			int         index = 0;
			std::string want;
			args >> index;
			std::getline(args, want);
			const auto wb         = want.find_first_not_of(' ');
			want                  = wb == std::string::npos ? std::string() : want.substr(wb);
			auto*             log = dynamic_cast<Mui_journey_window*>(win);
			const std::string got = log ? log->visible_name(index) : "no Journey Log";
			expect(got == want, "journey card " + std::to_string(index) + " = " + want, got);
		} else if (cmd == "select-card") {
			auto* log = dynamic_cast<Mui_journey_window*>(win);
			if (!log || !log->select_by_name(rest)) {
				expect(false, "select-card " + rest, log ? "no such card" : "no Journey Log");
			}
		} else if (cmd == "expect-save-name") {
			// Name stored in a savegame file, read as Newfile_gump does.
			int         num = 0;
			std::string want;
			args >> num;
			std::getline(args, want);
			const auto wb                       = want.find_first_not_of(' ');
			want                                = wb == std::string::npos ? std::string() : want.substr(wb);
			char*                       name    = nullptr;
			SaveGame_Details*           details = nullptr;
			SaveGame_Party*             party   = nullptr;
			std::unique_ptr<Shape_file> shot;
			bool                        readable = false;
			try {
				readable = gwin->get_saveinfo(num, name, shot, details, party);
			} catch (const exult_exception&) {
				readable = false;
			}
			const std::string got = name ? std::string(name) : "<none>";
			delete[] name;
			delete details;
			delete[] party;
			expect(readable && details != nullptr && got == want, "save " + std::to_string(num) + " name = " + want,
				   got + (readable ? "" : " (unreadable)"));
		} else if (cmd == "restore-game") {
			// The calls of Newfile_gump::load().
			int num = 0;
			args >> num;
			gwin->restore_gamedat(num);
			gwin->read();
			std::cout << "MUI-TEST info restored save " << num << std::endl;
		} else if (cmd == "expect-restored") {
			const bool want = rest == "yes";
			expect(st.restored == want, "journey restored " + rest, st.restored ? "yes" : "no");
		} else if (cmd == "close") {
			if (win) {
				win->finish();
			} else if (gump) {
				gump->close();
			} else {
				expect(false, "close", "nothing open");
			}
		} else if (cmd == "quit") {
			finish_script();
			quitting_time = QUIT_TIME_YES;
			if (win) {
				win->finish();
			} else if (gump) {
				gump->close();
			}
		} else {
			expect(false, "command " + line, "unknown command");
		}
		if (st.pc >= st.lines.size() && cmd != "open") {
			finish_script();
		}
	}

	void step(Mui_window* win, Modal_gump* gump) {
		State& st = state();
		if (!st.enabled || st.done) {
			return;
		}
		if (gump && st.log_open) {
			st.log_open    = false;
			st.last_opened = win ? class_name(win, typeid(*win)) : class_name(gump, typeid(*gump));
			std::cout << "MUI-TEST info opened " << st.last_opened << std::endl;
		}
		if (SDL_GetTicks() < st.next_time) {
			return;
		}
		if (st.pc >= st.lines.size()) {
			finish_script();
			return;
		}
		run_command(win, gump);
	}
}    // namespace

bool Mui_selftest::active() {
	return state().enabled;
}

void Mui_selftest::tick(uint32 ticks) {
	static_cast<void>(ticks);
	State& st = state();
	if (!st.enabled) {
		return;
	}
	Gump_manager::modal_frame_hook = &Mui_selftest::on_modal_frame;
	step(nullptr, nullptr);
	if (st.done && quitting_time == QUIT_TIME_NO) {
		quitting_time = QUIT_TIME_YES;
	}
}

void Mui_selftest::on_window_frame(Mui_window* win) {
	if (!state().enabled) {
		return;
	}
	state().window = win;
	step(win, win);
}

void Mui_selftest::on_modal_frame(Modal_gump* gump) {
	// Themed windows step from their own run().
	if (!state().enabled || dynamic_cast<Mui_window*>(gump) != nullptr) {
		return;
	}
	step(nullptr, gump);
}

void Mui_selftest::on_classic_screen(const char* what) {
	State& st = state();
	if (!st.enabled) {
		return;
	}
	st.log_open    = false;
	st.last_opened = std::string("CheatScreen ") + what;
	std::cout << "MUI-TEST info opened " << st.last_opened << std::endl;
	if (st.escape_classic) {
		st.escape_classic = false;
		push_key(SDLK_ESCAPE, SDL_KMOD_NONE);
	}
}

void Mui_selftest::forget(Mui_window* win) {
	if (state().window == win) {
		state().window = nullptr;
	}
}
