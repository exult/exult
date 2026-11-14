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

#include "Mui_dialogue_test.h"

#include "Mui_dialogue.h"
#include "Mui_selftest.h"
#include "actors.h"
#include "conversation.h"
#include "gamewin.h"
#include "ucmachine.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {
	// The keys that choose answers 0, 1, 2 ... (Usecode_internal::get_user_choice_num).
	constexpr const char option_keys[] = "123456789abcdefghijklmnopqrstuvwxyz";

	Conversation* conversation() {
		Usecode_machine* uc = Game_window::get_instance()->get_usecode();
		return uc ? uc->get_conversation() : nullptr;
	}

	bool same_text(const std::string& a, const std::string& b) {
		if (a.size() != b.size()) {
			return false;
		}
		for (size_t i = 0; i < a.size(); i++) {
			if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
				return false;
			}
		}
		return true;
	}

	bool all_digits(const std::string& s) {
		if (s.empty()) {
			return false;
		}
		for (const char c : s) {
			if (!std::isdigit(static_cast<unsigned char>(c))) {
				return false;
			}
		}
		return true;
	}

	// NPC by number, else the first NPC whose name matches (any case).
	Actor* find_npc(const std::string& spec) {
		Game_window* gwin = Game_window::get_instance();
		if (all_digits(spec)) {
			return gwin->get_npc(std::atol(spec.c_str()));
		}
		for (int i = 0; i < gwin->get_num_npcs(); i++) {
			Actor* npc = gwin->get_npc(i);
			if (npc && same_text(npc->get_npc_name(), spec)) {
				return npc;
			}
		}
		return nullptr;
	}

	std::string join(const std::vector<std::string>& items) {
		if (items.empty()) {
			return "-";
		}
		std::string out;
		for (const auto& item : items) {
			if (!out.empty()) {
				out += '|';
			}
			out += item;
		}
		return out;
	}

	const char* face_state(const Conversation::Conv_debug::Slot& slot) {
		if (!slot.used) {
			return "none";
		}
		return slot.drawable ? "shown" : "faceless";
	}

	void log_conversation(const Conversation::Conv_debug& d) {
		std::cout << "MUI-TEST info conv style " << d.style << " wait " << d.wait << " active " << d.active << std::endl;
		for (int i = 0; i < 2; i++) {
			const auto& s = d.slots[i];
			std::cout << "MUI-TEST info conv face " << i << " " << face_state(s) << " shape " << s.shape << " frame " << s.frame
					  << " npc " << s.npc_num << std::endl;
		}
		std::cout << "MUI-TEST info conv page \"" << d.page << "\"" << std::endl;
		std::cout << "MUI-TEST info conv answers " << join(d.answers) << std::endl;
		std::cout << "MUI-TEST info conv chosen \"" << d.last_choice << "\"" << std::endl;
	}

	void talk(const std::string& rest) {
		Game_window*     gwin = Game_window::get_instance();
		Usecode_machine* uc   = gwin->get_usecode();
		Actor*           npc  = find_npc(rest);
		if (!npc) {
			Mui_selftest::report(false, "talk " + rest, "no such NPC");
			return;
		}
		if (uc->get_num_faces_on_screen() > 0) {
			Mui_selftest::report(false, "talk " + rest, "a conversation is open");
			return;
		}
		std::cout << "MUI-TEST info talk " << rest << " (npc " << npc->get_npc_num() << ", usecode 0x" << std::hex
				  << npc->get_usecode() << std::dec << ")" << std::endl;
		const int ret = uc->call_usecode(npc->get_usecode(), npc, Usecode_machine::double_click);
		std::cout << "MUI-TEST info talk " << rest << " returned " << ret << std::endl;
	}

	void call_usecode(const std::string& rest) {
		Game_window*       gwin = Game_window::get_instance();
		std::istringstream args(rest);
		std::string        fn;
		std::string        who;
		args >> fn >> who;
		char*      end = nullptr;
		const long id  = std::strtol(fn.c_str(), &end, 0);
		if (fn.empty() || !end || *end) {
			Mui_selftest::report(false, "call-usecode " + rest, "bad function number");
			return;
		}
		Game_object* item = gwin->get_main_actor();
		if (!who.empty()) {
			item = find_npc(who);
			if (!item) {
				Mui_selftest::report(false, "call-usecode " + rest, "no such NPC");
				return;
			}
		}
		const int ret = gwin->get_usecode()->call_usecode(int(id), item, Usecode_machine::double_click);
		std::cout << "MUI-TEST info call-usecode " << rest << " returned " << ret << std::endl;
	}

	void choose_key(const std::string& text, const Conversation::Conv_debug& d) {
		for (size_t i = 0; i < d.answers.size(); i++) {
			if (d.answers[i] != text) {
				continue;
			}
			if (i >= sizeof(option_keys) - 1) {
				Mui_selftest::report(false, "conv-choose-key " + text, "no key for answer " + std::to_string(i));
				return;
			}
			std::cout << "MUI-TEST info conv-choose-key " << text << " = " << option_keys[i] << std::endl;
			Mui_selftest::press_key(option_keys[i], 0);
			return;
		}
		Mui_selftest::report(false, "conv-choose-key " + text, join(d.answers));
	}

	// Click the middle of a part of the themed view (Mui_dialogue::part_rect).
	void click_part(Mui_dialogue* view, const std::string& part) {
		TileRect r;
		if (!view->part_rect(part, r)) {
			Mui_selftest::report(false, "conv-click " + part, "no such part");
			return;
		}
		Game_window*  gwin = Game_window::get_instance();
		int           sx;
		int           sy;
		float         wx;
		float         wy;
		SDL_Renderer* rend = SDL_GetRenderer(gwin->get_win()->get_screen_window());
		if (!gwin->get_win()->layer_to_screen(view->get_layer(), r.x + r.w / 2, r.y + r.h / 2, sx, sy) || !rend
			|| !SDL_RenderCoordinatesToWindow(rend, float(sx), float(sy), &wx, &wy)) {
			Mui_selftest::report(false, "conv-click " + part, "unmappable");
			return;
		}
		std::cout << "MUI-TEST info conv-click " << part << std::endl;
		Mui_selftest::click_window(wx, wy, SDL_BUTTON_LEFT);
	}

	// "@avatar" stands for the Avatar's name, "@npc:N" for the label of
	// NPC N (Actor::get_name), so scripts work in both games.
	std::string speaker_spec(const std::string& spec) {
		if (spec == "@avatar") {
			return Game_window::get_instance()->get_main_actor()->get_npc_name();
		}
		if (spec.compare(0, 5, "@npc:") == 0) {
			const Actor* npc = Game_window::get_instance()->get_npc(std::atol(spec.c_str() + 5));
			return npc ? npc->get_name() : std::string("(no such NPC)");
		}
		return spec;
	}

	// Push a wheel turn at the centre of the panel (dy > 0 = up).
	void wheel_panel(Mui_dialogue* view, int dy) {
		TileRect r;
		view->part_rect("panel", r);
		Game_window*  gwin = Game_window::get_instance();
		int           sx;
		int           sy;
		float         wx;
		float         wy;
		SDL_Renderer* rend = SDL_GetRenderer(gwin->get_win()->get_screen_window());
		if (!gwin->get_win()->layer_to_screen(view->get_layer(), r.x + r.w / 2, r.y + r.h / 2, sx, sy) || !rend
			|| !SDL_RenderCoordinatesToWindow(rend, float(sx), float(sy), &wx, &wy)) {
			Mui_selftest::report(false, "conv-wheel " + std::to_string(dy), "unmappable");
			return;
		}
		std::cout << "MUI-TEST info conv-wheel " << dy << std::endl;
		Mui_selftest::wheel_window(wx, wy, dy);
	}

	// expect-conv-history I|SPEAKER|SUBSTR: SPEAKER "-" = no label, "*" = any.
	void expect_history(const std::vector<Mui_dialogue::Entry>& hist, const std::string& rest) {
		const auto        b1   = rest.find('|');
		const auto        b2   = b1 == std::string::npos ? b1 : rest.find('|', b1 + 1);
		const int         i    = std::atoi(rest.substr(0, b1).c_str());
		const std::string spk  = b1 == std::string::npos ? "*" : rest.substr(b1 + 1, b2 - b1 - 1);
		const std::string want = b2 == std::string::npos ? std::string() : rest.substr(b2 + 1);
		if (i < 0 || i >= int(hist.size())) {
			Mui_selftest::report(false, "conv history " + rest, "(no entry, " + std::to_string(hist.size()) + " entries)");
			return;
		}
		const auto& e  = hist[size_t(i)];
		const bool  ok = (spk == "*" || (spk == "-" ? e.speaker.empty() : e.speaker == speaker_spec(spk)))
						&& e.text.find(want) != std::string::npos;
		Mui_selftest::report(
				ok, "conv history " + rest, "\"" + e.speaker + "\" " + (e.choice ? "chose " : "said ") + "\"" + e.text + "\"");
	}

	// Themed-only commands; false if cmd is not one of them.
	bool run_themed(const std::string& cmd, const std::string& rest, Mui_dialogue* view) {
		if (cmd != "expect-conv-speaker" && cmd != "expect-conv-page-log" && cmd != "expect-conv-pages-atleast"
			&& cmd != "conv-click" && cmd.compare(0, 19, "expect-conv-history") != 0 && cmd != "conv-wheel") {
			return false;
		}
		if (!view) {
			Mui_selftest::report(false, cmd + " " + rest, "classic conversation");
			return true;
		}
		const auto& log  = view->page_log();
		const auto& hist = view->get_history();
		if (cmd == "expect-conv-speaker") {
			const std::string got = view->speaker();
			Mui_selftest::report(got == speaker_spec(rest), "conv speaker " + rest, "\"" + got + "\"");
		} else if (cmd == "expect-conv-history-count") {
			Mui_selftest::report(
					int(hist.size()) == std::atoi(rest.c_str()), "conv history count " + rest, std::to_string(hist.size()));
		} else if (cmd == "expect-conv-history-atleast") {
			Mui_selftest::report(
					int(hist.size()) >= std::atoi(rest.c_str()), "conv history >= " + rest, std::to_string(hist.size()));
		} else if (cmd == "expect-conv-history-open") {
			// yes|no; "top", "bottom", "middle" and "scrolled" (not at the
			// top) also check the scroll position.
			const int   pos = view->history_scroll();
			const int   max = view->history_scroll_max();
			std::string got = view->history_open() ? "yes" : "no";
			bool        ok  = got == rest;
			if (view->history_open() && (rest == "top" || rest == "bottom" || rest == "middle" || rest == "scrolled")) {
				ok = (rest == "top" && pos <= 0) || (rest == "bottom" && pos >= max) || (rest == "middle" && pos > 0 && pos < max)
					 || (rest == "scrolled" && pos > 0);
			}
			if (view->history_open()) {
				got += " scroll " + std::to_string(pos) + "/" + std::to_string(max);
			}
			Mui_selftest::report(ok, "conv history open " + rest, got);
		} else if (cmd == "expect-conv-history") {
			expect_history(hist, rest);
		} else if (cmd == "conv-wheel") {
			wheel_panel(view, std::atoi(rest.c_str()));
		} else if (cmd == "expect-conv-page-log") {
			const auto        bar  = rest.find('|');
			const int         i    = std::atoi(rest.substr(0, bar).c_str());
			const std::string want = bar == std::string::npos ? std::string() : rest.substr(bar + 1);
			const std::string got  = i >= 0 && i < int(log.size()) ? log[size_t(i)] : std::string("(no page)");
			Mui_selftest::report(
					i >= 0 && i < int(log.size()) && got.find(want) != std::string::npos, "conv page-log " + rest,
					"\"" + got + "\"");
		} else if (cmd == "expect-conv-pages-atleast") {
			Mui_selftest::report(int(log.size()) >= std::atoi(rest.c_str()), "conv pages >= " + rest, std::to_string(log.size()));
		} else {
			click_part(view, rest);
		}
		return true;
	}
}    // namespace

bool Mui_dialogue_test::run(const std::string& cmd, const std::string& rest) {
	if (cmd == "talk") {
		talk(rest);
		return true;
	}
	if (cmd == "call-usecode") {
		call_usecode(rest);
		return true;
	}
	if (cmd.compare(0, 5, "conv-") != 0 && cmd.compare(0, 12, "expect-conv-") != 0) {
		return false;
	}
	Conversation* conv = conversation();
	if (!conv) {
		Mui_selftest::report(false, cmd + " " + rest, "no conversation");
		return true;
	}
	if (run_themed(cmd, rest, conv->get_view())) {
		return true;
	}
	Conversation::Conv_debug d;
	conv->describe(d);
	if (cmd == "conv-dump") {
		const bool ok = conv->dump_layers(rest.c_str());
		std::cout << "MUI-TEST info conv-dump " << rest << std::endl;
		if (!ok) {
			Mui_selftest::report(false, "conv-dump " + rest, "write error");
		}
	} else if (cmd == "conv-log") {
		log_conversation(d);
		if (const Mui_dialogue* view = conv->get_view()) {
			std::cout << "MUI-TEST info conv speaker \"" << view->speaker() << "\" pages " << view->page_log().size() << std::endl;
			for (size_t i = 0; i < view->page_log().size(); i++) {
				std::cout << "MUI-TEST info conv page-log " << i << " \"" << view->page_log()[i] << "\"" << std::endl;
			}
			for (size_t i = 0; i < view->get_history().size(); i++) {
				const auto& e = view->get_history()[i];
				std::cout << "MUI-TEST info conv history " << i << " \"" << e.speaker << "\" " << (e.choice ? "chose" : "said")
						  << " \"" << e.text << "\"" << std::endl;
			}
		}
	} else if (cmd == "conv-choose-key") {
		choose_key(rest, d);
	} else if (cmd == "expect-conv-style") {
		Mui_selftest::report(d.style == rest, "conv style " + rest, d.style);
	} else if (cmd == "expect-conv-wait") {
		Mui_selftest::report(d.wait == rest, "conv wait " + rest, d.wait);
	} else if (cmd == "expect-conv-faces") {
		const int got = Game_window::get_instance()->get_usecode()->get_num_faces_on_screen();
		Mui_selftest::report(got == std::atoi(rest.c_str()), "conv faces " + rest, std::to_string(got));
	} else if (cmd == "expect-conv-face") {
		std::istringstream args(rest);
		int                slot = -1;
		std::string        want;
		args >> slot >> want;
		const std::string got = slot >= 0 && slot < 2 ? face_state(d.slots[slot]) : "bad slot";
		Mui_selftest::report(got == want, "conv face " + rest, got);
	} else if (cmd == "expect-conv-page") {
		Mui_selftest::report(d.page.find(rest) != std::string::npos, "conv page has " + rest, "\"" + d.page + "\"");
	} else if (cmd == "expect-conv-answers") {
		const std::string got = join(d.answers);
		Mui_selftest::report(got == rest, "conv answers " + rest, got);
	} else if (cmd == "expect-conv-chosen") {
		Mui_selftest::report(d.last_choice == rest, "conv chosen " + rest, "\"" + d.last_choice + "\"");
	} else {
		Mui_selftest::report(false, "command " + cmd + " " + rest, "unknown conversation command");
	}
	return true;
}
