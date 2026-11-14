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

#include "Mui_cheats.h"

#include "Mui_selftest.h"
#include "actors.h"
#include "cheat.h"
#include "exult_constants.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "items.h"
#include "shapeid.h"
#include "ucmachine.h"
#include "version.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Msg(unsigned id) {
			return get_text_msg(id - unsigned(msg_file_start));
		}

		static auto Cheats() {
			return Msg(0xC14);
		}

		static auto Close() {
			return Msg(0xC07);
		}

		static auto Section(int i) {
			// The Troubleshooting sections' labels follow at 0xD40.
			return i < 7 ? Msg(0xCC6 + unsigned(i)) : Msg(0xD40 + unsigned(i - 7));
		}

		static auto Warning() {
			return Msg(0xCCD);
		}

		// Player toggles: label, help pairs from 0xCCE.
		static auto ToggleLabel(int i) {
			return Msg(0xCCE + 2 * unsigned(i));
		}

		static auto ToggleHelp(int i) {
			return Msg(0xCCF + 2 * unsigned(i));
		}
	};

	// Strings of the classic cheat screen reused here.
	namespace Classic {
		auto Running() {
			return Strings::Msg(0x8F4);
		}

		auto CurrentTime() {
			return Strings::Msg(0x8F5);
		}

		auto Day() {
			return Strings::Msg(0x8F6);
		}

		auto Coordinates() {
			return Strings::Msg(0x8F7);
		}

		auto North() {
			return Strings::Msg(0x8F8);
		}

		auto South() {
			return Strings::Msg(0x8F9);
		}

		auto Map() {
			return Strings::Msg(0x8FA);
		}

		auto West() {
			return Strings::Msg(0x8FB);
		}

		auto East() {
			return Strings::Msg(0x8FC);
		}

		auto Hex() {
			return Strings::Msg(0x8FD);
		}

		auto Dec() {
			return Strings::Msg(0x8FE);
		}

		auto am() {
			return Strings::Msg(0x659);
		}

		auto pm() {
			return Strings::Msg(0x65A);
		}

		auto ClockSet() {
			return Strings::Msg(0x91C);
		}

		auto Unnamed() {
			return Strings::Msg(0xA3F);
		}

		auto NpcNotOnMap() {
			return Strings::Msg(0xA53);
		}
	}    // namespace Classic

	// Player toggles in the order of their strings.
	enum Player_toggle {
		god = 0,
		wizard,
		infravision,
		hack_mover,
		eggs,
		paperdolls,
		pickpocket,
		grab
	};

	const char* const section_keys[]
			= {"player",  "world",        "npcs",        "teleport",     "flags",          "usecode",
			   "mapedit", "doctor_flags", "doctor_npcs", "doctor_items", "doctor_checkup", "doctor_safety"};
	constexpr int num_sections = int(std::size(section_keys));

	// Remembered for the session.
	int last_section = 0;

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	// A number typed as decimal or 0x hex, else -1.
	long parse_number(const std::string& s) {
		if (s.empty()) {
			return -1;
		}
		char*      end = nullptr;
		const long v   = std::strtol(s.c_str(), &end, 0);
		return end && *end == 0 && v >= 0 ? v : -1;
	}

	bool paperdolls_available() {
		Shape_manager* sman = Shape_manager::get_instance();
		return (Game::get_game_type() == BLACK_GATE || Game::get_game_type() == EXULT_DEVEL_GAME) && sman->can_use_paperdolls();
	}

	std::string format_time(Game_clock* clk) {
		char buf[128];
		snprintf(
				buf, sizeof(buf), "%s%i:%02i %s  %s%i", Classic::CurrentTime(), ((clk->get_hour() + 11) % 12) + 1,
				clk->get_minute(), clk->get_hour() < 12 ? Classic::am() : Classic::pm(), Classic::Day(), clk->get_day());
		return buf;
	}

	// A wrapped help line under a label.
	class Help_text : public Mui_label {
	public:
		explicit Help_text(std::string text_) : Mui_label(std::move(text_), Mui_font_role::small, Mui_colour::text_dim, true) {}
	};
}    // namespace

Mui_cheat_window::Mui_cheat_window() : Mui_window(Strings::Cheats()), cscreen(cheat.get_cheat_screen()) {
	tab_index = std::clamp(last_section, 0, num_sections - 1);
	read_world();
}

Mui_cheat_window::~Mui_cheat_window() {
	last_section = tab_index;
}

void Mui_cheat_window::select_section(const std::string& key) {
	for (int i = 0; i < num_sections; i++) {
		if (key == section_keys[i]) {
			tab_index = i;
			message.clear();
			rebuild();
			return;
		}
	}
}

void Mui_cheat_window::adjust_theme(Mui_theme& t) const {
	// Sidebar sections and the red cheat accent in every theme.
	t.tabs_left                            = true;
	const uint8 red                        = t.colour(Mui_colour::cheat_accent);
	const auto  light                      = Mui_theme::nearest_index(0xffffff);
	t.idx[int(Mui_colour::accent)]         = red;
	t.idx[int(Mui_colour::accent_hover)]   = Mui_theme::blend_index(red, light, 1, 4);
	t.idx[int(Mui_colour::text_on_accent)] = t.colour(Mui_colour::text_on_danger);
	t.rgb[int(Mui_colour::accent)]         = t.rgb[int(Mui_colour::cheat_accent)];
}

std::vector<std::string> Mui_cheat_window::tab_labels() const {
	std::vector<std::string> out;
	for (int i = 0; i < num_sections; i++) {
		out.emplace_back(Strings::Section(i));
	}
	return out;
}

std::vector<std::string> Mui_cheat_window::tab_keys() const {
	return std::vector<std::string>(std::begin(section_keys), std::end(section_keys));
}

void Mui_cheat_window::on_tab(int index) {
	// Action results belong to the section they came from.
	message.clear();
	Mui_window::on_tab(index);
}

std::string Mui_cheat_window::default_help() const {
	return Strings::Msg(0xD1C);
}

void Mui_cheat_window::read_world() {
	Game_clock* clk = gwin->get_clock();
	day             = clk->get_day();
	hour            = clk->get_hour();
	minute          = clk->get_minute();
	if (Actor* av = gwin->get_main_actor()) {
		const Tile_coord t = av->get_tile();
		tele_x             = t.tx;
		tele_y             = t.ty;
		tele_z             = t.tz;
	}
	tele_map = gwin->get_map()->get_num();
}

void Mui_cheat_window::build_footer(Mui_box& footer) {
	auto* classic = footer.add<Mui_button>("cheats.classic", Strings::Msg(0xD1A), Mui_button::Style::secondary, [this] {
		run_classic(CheatScreen::Which::Root);
	});
	classic->help = Strings::Msg(0xD1B);
	auto* close   = footer.add<Mui_button>("cheats.close", Strings::Close(), Mui_button::Style::primary, [this] {
        post([this] {
            finish();
        });
    });
	close->help   = Strings::Msg(0xC1C);
}

/*
 *  Body.
 */

void Mui_cheat_window::build(Mui_box& body) {
	npc_list     = nullptr;
	npc_details  = nullptr;
	npc_location = nullptr;
	npc_teleport = nullptr;
	npc_tools    = nullptr;
	flag_list    = nullptr;
	flag_name    = nullptr;
	flag_toggle  = nullptr;
	flag_count   = nullptr;
	clear_doctor_widgets();
	building     = true;
	body.gap     = 8;
	build_warning(body);
	switch (tab_index) {
	case 1:
		build_world(body);
		break;
	case 2:
		build_npcs(body);
		break;
	case 3:
		build_teleport(body);
		break;
	case 4:
		build_flags(body);
		break;
	case 5:
		build_usecode(body);
		break;
	case 6:
		build_map_editor(body);
		break;
	case 7:
		build_doctor_flags(body);
		break;
	case 8:
		build_doctor_npcs(body);
		break;
	case 9:
		build_doctor_items(body);
		break;
	case 10:
		build_doctor_checkup(body);
		break;
	case 11:
		build_doctor_safety(body);
		break;
	default:
		build_player(body);
		break;
	}
	build_message(body);
	building = false;
}

void Mui_cheat_window::build_warning(Mui_box& body) {
	auto* strip   = body.add<Mui_box>(Mui_box::Dir::horizontal, "cheats.warning");
	strip->panel  = true;
	strip->bg     = Mui_colour::cheat_accent;
	strip->pad    = 8;
	auto* text    = strip->add<Mui_label>(Strings::Warning(), Mui_font_role::body, Mui_colour::text_on_danger, true);
	text->stretch = true;
	text->id      = "cheats.warning.text";
}

void Mui_cheat_window::build_message(Mui_box& body) {
	if (message.empty()) {
		return;
	}
	body.add<Mui_label>(message, Mui_font_role::body, Mui_colour::success, true)->id = "cheats.message";
}

namespace {
	// Label and help on the left, the control on the right.
	Mui_box* control_row(Mui_box& body, const std::string& label, const std::string& help) {
		auto* row     = body.add<Mui_box>(Mui_box::Dir::horizontal);
		auto* text    = row->add<Mui_box>(Mui_box::Dir::vertical);
		text->gap     = 2;
		text->stretch = true;
		text->add<Mui_label>(label);
		if (!help.empty()) {
			text->add<Help_text>(help);
		}
		return row;
	}
}    // namespace

void Mui_cheat_window::build_player(Mui_box& body) {
	body.add<Mui_section_header>(Strings::Msg(0xCE3));

	struct Item {
		Player_toggle which;
		const char*   id;
		bool          value;
		void (*toggle)();
	};

	Shape_manager*          sman  = Shape_manager::get_instance();
	const std::vector<Item> items = {
			{        god,         "cheats.god",										  cheat.in_god_mode(),
			 [] {
             cheat.toggle_god();
			 }},
			{     wizard,      "cheats.wizard",									   cheat.in_wizard_mode(),
			 [] {
             cheat.toggle_wizard();
			 }},
			{infravision, "cheats.infravision",                                       cheat.in_infravision(),
			 [] {
             cheat.toggle_infravision();
			 }},
			{ hack_mover,   "cheats.hackmover",                                        cheat.in_hack_mover(),
			 [] {
             cheat.toggle_hack_mover();
			 }},
			{       eggs,        "cheats.eggs",											 gwin->paint_eggs,
			 [] {
             cheat.toggle_eggs();
			 }},
			{ paperdolls,  "cheats.paperdolls", sman->can_use_paperdolls() && sman->are_paperdolls_enabled(),
			 [] {
             CheatScreen::toggle_paperdolls();
			 }},
			{ pickpocket,  "cheats.pickpocket",                                        cheat.in_pickpocket(),
			 [] {
             cheat.toggle_pickpocket();
			 }},
			{       grab,        "cheats.grab",									   cheat.grabbing_actor(),
			 [] {
             cheat.toggle_grab_actor();
			 }},
	};
	for (const auto& it : items) {
		if (it.which == paperdolls && !paperdolls_available()) {
			continue;
		}
		const std::string help = Strings::ToggleHelp(it.which);
		Mui_box*          row  = control_row(body, Strings::ToggleLabel(it.which), help);
		row->id                = std::string(it.id) + ".row";
		const std::string id   = it.id;
		auto              fn   = it.toggle;
		auto*             t    = row->add<Mui_toggle>(id, it.value, [this, fn, id](bool) {
            post([this, fn, id] {
                fn();
                message.clear();
                rebuild(id);
            });
        });
		t->help                = help;
	}
	body.add<Mui_section_header>(Strings::Msg(0xCDE));
	Mui_box* row = control_row(body, Strings::Msg(0xCDF), Strings::Msg(0xCE0));
	row->add<Mui_button>(
			   "cheats.heal", Strings::Msg(0xCDF), Mui_button::Style::secondary,
			   [this] {
				   post([this] {
					   cheat.heal_party();
					   message = std::string(Strings::Msg(0xD1D)) + " " + Strings::Msg(0xCDF);
					   rebuild("cheats.heal");
				   });
			   })
			->help
			= Strings::Msg(0xCE0);
	row = control_row(body, Strings::Msg(0xCE1), Strings::Msg(0xCE2));
	row->add<Mui_button>(
			   "cheats.levelup", Strings::Msg(0xCE1), Mui_button::Style::secondary,
			   [this] {
				   post([this] {
					   cheat.levelup_party();
					   message = std::string(Strings::Msg(0xD1D)) + " " + Strings::Msg(0xCE1);
					   rebuild("cheats.levelup");
				   });
			   })
			->help
			= Strings::Msg(0xCE2);
}

void Mui_cheat_window::build_world(Mui_box& body) {
	Game_clock* clk = gwin->get_clock();
	body.add<Mui_section_header>(Strings::Section(1));
	body.add<Mui_label>(format_time(clk), Mui_font_role::heading)->id = "cheats.now";
	auto* fields                                                      = body.add<Mui_box>(Mui_box::Dir::horizontal);
	fields->gap                                                       = 12;
	auto field = [&](const char* id, unsigned label, int minv, int maxv, int& var) {
		auto* col = fields->add<Mui_box>(Mui_box::Dir::vertical);
		col->gap  = 2;
		col->add<Mui_label>(Strings::Msg(label), Mui_font_role::small, Mui_colour::text_dim);
		auto* f = col->add<Mui_number_field>(id, minv, maxv, 1, std::clamp(var, minv, maxv), [&var](int v) {
			var = v;
		});
		f->help = Strings::Msg(0xCE9);
	};
	field("cheats.day", 0xCE5, 0, 99999, day);
	field("cheats.hour", 0xCE6, 0, 23, hour);
	field("cheats.minute", 0xCE7, 0, 59, minute);
	auto* setcol = fields->add<Mui_box>(Mui_box::Dir::vertical);
	setcol->gap  = 2;
	setcol->add<Mui_label>(" ", Mui_font_role::small);
	auto* set = setcol->add<Mui_button>("cheats.settime", Strings::Msg(0xCE8), Mui_button::Style::primary, [this] {
		post([this] {
			// As the classic Set Time command.
			Game_clock* c = gwin->get_clock();
			c->reset();
			c->set_day(day);
			c->set_hour(hour);
			c->set_minute(minute);
			message = Classic::ClockSet();
			rebuild("cheats.settime");
		});
	});
	set->help = Strings::Msg(0xCE9);
	body.add<Help_text>(set->help);
	// Time rate.
	Mui_box* row  = control_row(body, Strings::Msg(0xCEA), Strings::Msg(0xCEB));
	auto*    rate = row->add<Mui_slider>("cheats.timerate", 1, 20, 1, std::clamp(clk->get_time_rate(), 1, 20), [](int v) {
        Game_window::get_instance()->get_clock()->set_time_rate(v);
    });
	rate->help    = Strings::Msg(0xCEB);
	row           = control_row(body, Strings::Msg(0xCEC), Strings::Msg(0xCED));
	auto* fake    = row->add<Mui_button>("cheats.fakeperiod", Strings::Msg(0xCEC), Mui_button::Style::secondary, [this] {
        post([this] {
            cheat.fake_time_period();
            read_world();
            message.clear();
            rebuild("cheats.fakeperiod");
        });
    });
	fake->help    = Strings::Msg(0xCED);
	// Position, as on the classic cheat screen.
	body.add<Mui_section_header>(Strings::Msg(0xCEE));
	char             buf[512];
	const int        curmap = gwin->get_map()->get_num();
	const Tile_coord t      = gwin->get_main_actor()->get_tile();
	const int        longi  = ((t.tx - 0x3A5) / 10);
	const int        lati   = ((t.ty - 0x46E) / 10);
	snprintf(
			buf, sizeof(buf), "%s%d %s %d %s, %s%d", Classic::Coordinates(), abs(lati),
			(lati < 0 ? Classic::North() : Classic::South()), abs(longi), (longi < 0 ? Classic::West() : Classic::East()),
			Classic::Map(), curmap);
	body.add<Mui_label>(buf)->id = "cheats.position";
	snprintf(buf, sizeof(buf), "%s(%04x, %04x, %02x)", Classic::Hex(), t.tx, t.ty, t.tz);
	body.add<Mui_label>(buf, Mui_font_role::mono)->id = "cheats.hex";
	snprintf(buf, sizeof(buf), "%s(%04i, %04i, %02i)", Classic::Dec(), t.tx, t.ty, t.tz);
	body.add<Mui_label>(buf, Mui_font_role::mono)->id = "cheats.dec";
	// Game and build.
	body.add<Mui_section_header>(Strings::Msg(0xD20));
	std::string title = Game::get_menustring();
	for (size_t pos; (pos = title.find('\n')) != std::string::npos;) {
		title.replace(pos, 1, ": ");
	}
	body.add<Mui_label>(std::string(Classic::Running()) + "\"" + title + "\"", Mui_font_role::small, Mui_colour::text_dim)->id
			= "cheats.game";
	body.add<Mui_label>(
			std::string("Exult Version " VERSION " Rev: ") + std::string(VersionGetGitRevision(true)), Mui_font_role::small,
			Mui_colour::text_dim);
}

/*
 *  NPCs.
 */

void Mui_cheat_window::filter_npcs() {
	npc_rows.clear();
	const std::string needle = lower(npc_filter);
	const long        number = parse_number(npc_filter);
	const int         n      = gwin->get_num_npcs();
	for (int i = 0; i < n; i++) {
		Actor* a = gwin->get_npc(i);
		if (!a) {
			continue;
		}
		if (!needle.empty() && i != number && lower(a->get_npc_name()).find(needle) == std::string::npos
			&& std::to_string(i).rfind(needle, 0) != 0) {
			continue;
		}
		npc_rows.push_back(i);
	}
	// Exact number first.
	if (number >= 0) {
		auto it = std::find(npc_rows.begin(), npc_rows.end(), int(number));
		if (it != npc_rows.end()) {
			std::rotate(npc_rows.begin(), it, it + 1);
		}
	}
	if (std::find(npc_rows.begin(), npc_rows.end(), npc_selected) == npc_rows.end()) {
		npc_selected = npc_rows.empty() ? -1 : npc_rows.front();
	}
	if (number >= 0 && !npc_rows.empty() && npc_rows.front() == number) {
		npc_selected = int(number);
	}
}

void Mui_cheat_window::show_npc() {
	Actor* a = npc_selected >= 0 ? gwin->get_npc(npc_selected) : nullptr;
	if (npc_list) {
		std::vector<std::string> rows;
		int                      sel = -1;
		for (size_t i = 0; i < npc_rows.size(); i++) {
			Actor* r = gwin->get_npc(npc_rows[i]);
			char   buf[160];
			snprintf(
					buf, sizeof(buf), "%4d  %s  (%s %d)", npc_rows[i], r ? r->get_npc_name().c_str() : "", Strings::Msg(0xCF8),
					r ? r->get_shapenum() : 0);
			rows.emplace_back(buf);
			if (npc_rows[i] == npc_selected) {
				sel = int(i);
			}
		}
		npc_list->rows = std::move(rows);
		npc_list->set_selected(sel, false);
		if (sel < 0) {
			npc_list->scroll = 0;
		}
	}
	if (npc_details) {
		npc_details->text = a ? std::to_string(npc_selected) + "  " + a->get_npc_name() : Strings::Msg(0xCF1);
	}
	if (npc_location) {
		std::string text;
		if (a) {
			char buf[200];
			if (a->get_chunk()) {
				const Tile_coord t = a->get_tile();
				snprintf(
						buf, sizeof(buf), "%s: %d, %d, %d  %s%d   %s: %d", Strings::Msg(0xCF6), t.tx, t.ty, t.tz, Classic::Map(),
						a->get_map_num(), Strings::Msg(0xCF7), a->get_property(Actor::health));
			} else {
				snprintf(buf, sizeof(buf), "%s   %s: %d", Strings::Msg(0xCF5), Strings::Msg(0xCF7), a->get_property(Actor::health));
			}
			text = buf;
		}
		npc_location->text = text;
	}
	if (npc_teleport) {
		npc_teleport->enabled = a && a->get_chunk();
	}
	if (npc_tools) {
		npc_tools->enabled = a != nullptr;
	}
	invalidate_all();
}

void Mui_cheat_window::build_npcs(Mui_box& body) {
	filter_npcs();
	auto* search        = body.add<Mui_text_field>("cheats.npc.search", npc_filter, Strings::Msg(0xCEF));
	search->search_icon = true;
	search->help        = Strings::Msg(0xCEF);
	search->on_change   = [this](const std::string& s) {
        npc_filter = s;
        filter_npcs();
        show_npc();
	};
	npc_list               = body.add<Mui_list>("cheats.npc.list", std::vector<std::string>{}, -1);
	npc_list->visible_rows = 8;
	npc_list->row_ref      = 30;
	npc_list->help         = Strings::Msg(0xCF0);
	npc_list->on_select    = [this](int i) {
        if (i >= 0 && i < int(npc_rows.size())) {
            npc_selected = npc_rows[size_t(i)];
            post([this] {
                show_npc();
            });
        }
	};
	npc_list->on_activate = [this](int i) {
		if (i >= 0 && i < int(npc_rows.size())) {
			npc_selected = npc_rows[size_t(i)];
			if (Actor* a = gwin->get_npc(npc_selected)) {
				run_classic(CheatScreen::Which::NPC, a);
			}
		}
	};
	npc_details        = body.add<Mui_label>("", Mui_font_role::heading);
	npc_details->id    = "cheats.npc.details";
	npc_location       = body.add<Mui_label>("", Mui_font_role::body, Mui_colour::text_dim);
	npc_location->id   = "cheats.npc.location";
	auto* actions      = body.add<Mui_box>(Mui_box::Dir::horizontal);
	npc_teleport       = actions->add<Mui_button>("cheats.npc.teleport", Strings::Msg(0xCF2), Mui_button::Style::secondary, [this] {
        post([this] {
            // As the NPC tool's Teleport to NPC.
            Actor* a = npc_selected >= 0 ? gwin->get_npc(npc_selected) : nullptr;
            if (!a) {
                return;
            }
            if (!a->get_chunk()) {
                message = Classic::NpcNotOnMap();
            } else {
                gwin->teleport_party(a->get_tile(), false, a->get_map_num());
                read_world();
                message.clear();
            }
            rebuild("cheats.npc.teleport");
        });
    });
	npc_teleport->help = Strings::Msg(0xCF2);
	npc_tools          = actions->add<Mui_button>("cheats.npc.tools", Strings::Msg(0xCF3), Mui_button::Style::primary, [this] {
        if (Actor* a = npc_selected >= 0 ? gwin->get_npc(npc_selected) : nullptr) {
            run_classic(CheatScreen::Which::NPC, a);
        }
    });
	npc_tools->help    = Strings::Msg(0xCF4);
	body.add<Help_text>(npc_tools->help);
	show_npc();
}

/*
 *  Teleport.
 */

void Mui_cheat_window::build_teleport(Mui_box& body) {
	body.add<Mui_section_header>(Strings::Section(3));
	auto* fields = body.add<Mui_box>(Mui_box::Dir::horizontal);
	fields->gap  = 12;
	auto field   = [&](const char* id, unsigned label, int minv, int maxv, int& var) {
        auto* col = fields->add<Mui_box>(Mui_box::Dir::vertical);
        col->gap  = 2;
        col->add<Mui_label>(Strings::Msg(label), Mui_font_role::small, Mui_colour::text_dim);
        col->add<Mui_number_field>(
                   id, minv, maxv, 1, std::clamp(var, minv, maxv),
                   [&var](int v) {
                       var = v;
                   })
                ->help
                = Strings::Msg(0xCFE);
	};
	// Ranges as the classic tile coordinates command.
	field("cheats.tele.x", 0xCF9, 0, c_num_tiles, tele_x);
	field("cheats.tele.y", 0xCFA, 0, c_num_tiles, tele_y);
	field("cheats.tele.z", 0xCFB, 0, 255, tele_z);
	auto* gocol = fields->add<Mui_box>(Mui_box::Dir::vertical);
	gocol->gap  = 2;
	gocol->add<Mui_label>(" ", Mui_font_role::small);
	auto* go = gocol->add<Mui_button>("cheats.tele.go", Strings::Section(3), Mui_button::Style::primary, [this] {
		post([this] {
			gwin->teleport_party(Tile_coord(tele_x, tele_y, tele_z));
			read_world();
			message.clear();
			rebuild("cheats.tele.go");
		});
	});
	go->help = Strings::Msg(0xCFE);
	body.add<Help_text>(go->help);
	const int highest = cscreen ? cscreen->get_highest_map() : 0;
	if (highest != 0) {
		Mui_box* row = control_row(body, Strings::Msg(0xCFC), Strings::Msg(0xCFF));
		row->add<Mui_number_field>(
				   "cheats.tele.map", 0, highest, 1, std::clamp(tele_map, 0, highest),
				   [this](int v) {
					   tele_map = v;
				   })
				->help
				= Strings::Msg(0xCFF);
		row->add<Mui_button>(
				   "cheats.tele.mapgo", Strings::Msg(0xCFD), Mui_button::Style::secondary,
				   [this] {
					   post([this] {
						   // As the classic Map Number command.
						   gwin->teleport_party(gwin->get_main_actor()->get_tile(), true, tele_map);
						   read_world();
						   message.clear();
						   rebuild("cheats.tele.mapgo");
					   });
				   })
				->help
				= Strings::Msg(0xCFF);
	}
	body.add<Mui_section_header>(Strings::Msg(0xD1F));
	Mui_box* row = control_row(body, Strings::Msg(0xD02), Strings::Msg(0xD03));
	row->add<Mui_button>(
			   "cheats.tele.mapview", Strings::Msg(0xD02), Mui_button::Style::secondary,
			   [this] {
				   post([this] {
					   run_hidden([] {
						   cheat.map_teleport();
					   });
					   read_world();
					   rebuild("cheats.tele.mapview");
				   });
			   })
			->help
			= Strings::Msg(0xD03);
	row = control_row(body, Strings::Msg(0xD00), Strings::Msg(0xD01));
	row->add<Mui_button>(
			   "cheats.tele.more", Strings::Msg(0xD00), Mui_button::Style::secondary,
			   [this] {
				   run_classic(CheatScreen::Which::Teleport);
			   })
			->help
			= Strings::Msg(0xD01);
}

/*
 *  Global flags.
 */

std::string Mui_cheat_window::flag_label(unsigned num) const {
	const auto&      names = cscreen->get_global_flag_names();
	std::string_view name
			= num < names.size() && !names[num].empty() ? std::string_view(names[num]) : std::string_view(Classic::Unnamed());
	return std::to_string(num) + "  " + std::string(name);
}

void Mui_cheat_window::filter_flags() {
	flag_rows.clear();
	const auto&       names  = cscreen->get_global_flag_names();
	const std::string needle = lower(flag_filter);
	const long        number = parse_number(flag_filter);
	if (number >= 0 && size_t(number) <= Usecode_machine::last_gflag) {
		flag_rows.push_back(unsigned(number));
	}
	// The same range as the classic global flag editor.
	for (size_t i = 0; i <= Usecode_machine::last_gflag; i++) {
		if (long(i) == number) {
			continue;
		}
		if (!needle.empty()) {
			const bool by_name = i < names.size() && lower(names[i]).find(needle) != std::string::npos;
			if (!by_name && (number < 0 || std::to_string(i).rfind(flag_filter, 0) != 0)) {
				continue;
			}
		}
		flag_rows.push_back(unsigned(i));
	}
	if (std::find(flag_rows.begin(), flag_rows.end(), unsigned(std::max(0, flag_selected))) == flag_rows.end()
		|| flag_selected < 0) {
		flag_selected = flag_rows.empty() ? -1 : int(flag_rows.front());
	}
	if (number >= 0 && !flag_rows.empty() && long(flag_rows.front()) == number) {
		flag_selected = int(number);
	}
}

void Mui_cheat_window::show_flag() {
	Usecode_machine* uc = gwin->get_usecode();
	if (flag_list) {
		std::vector<std::string> rows;
		rows.reserve(flag_rows.size());
		int sel = -1;
		for (size_t i = 0; i < flag_rows.size(); i++) {
			rows.push_back(flag_label(flag_rows[i]));
			if (int(flag_rows[i]) == flag_selected) {
				sel = int(i);
			}
		}
		flag_list->rows = std::move(rows);
		flag_list->set_selected(sel, false);
		if (sel < 0) {
			flag_list->scroll = 0;
		}
	}
	if (flag_count) {
		flag_count->text = flag_rows.empty() ? Strings::Msg(0xD05) : std::to_string(flag_rows.size()) + " " + Strings::Msg(0xD0B);
	}
	if (flag_name) {
		flag_name->text = flag_selected >= 0 ? flag_label(unsigned(flag_selected)) : std::string();
	}
	if (flag_toggle) {
		flag_toggle->value   = flag_selected >= 0 && uc->get_global_flag_bool(flag_selected);
		flag_toggle->enabled = flag_selected >= 0;
	}
	invalidate_all();
}

void Mui_cheat_window::toggle_flag(unsigned num) {
	// As the classic global flag editor's Toggle Flag.
	Usecode_machine* uc = gwin->get_usecode();
	uc->set_global_flag(int(num), !uc->get_global_flag_bool(int(num)));
}

void Mui_cheat_window::build_flags(Mui_box& body) {
	filter_flags();
	auto* bar           = body.add<Mui_box>(Mui_box::Dir::horizontal);
	auto* search        = bar->add<Mui_text_field>("cheats.flag.search", flag_filter, Strings::Msg(0xD04));
	search->search_icon = true;
	search->stretch     = true;
	search->help        = Strings::Msg(0xD04);
	search->on_change   = [this](const std::string& s) {
        flag_filter = s;
        filter_flags();
        show_flag();
	};
	flag_count              = bar->add<Mui_label>("", Mui_font_role::small, Mui_colour::text_dim);
	flag_count->id          = "cheats.flag.count";
	flag_count->fill_w      = false;
	flag_list               = body.add<Mui_list>("cheats.flag.list", std::vector<std::string>{}, -1);
	flag_list->visible_rows = 8;
	flag_list->row_ref      = 30;
	flag_list->help         = Strings::Msg(0xD0A);
	flag_list->paint_row    = [this](Mui_painter& p, int i, const TileRect& r, bool sel) {
        if (i < 0 || i >= int(flag_rows.size())) {
            return;
        }
        const unsigned num  = flag_rows[size_t(i)];
        const uint8    fill = sel ? p.col(Mui_colour::accent) : flag_list->surface(p);
        const uint8    fg   = p.col(sel ? Mui_colour::text_on_accent : Mui_colour::text);
        TileRect       tr   = r;
        tr.x += p.px(12);
        tr.w -= p.px(24);
        p.text_in_idx(flag_list->rows[size_t(i)], tr, Mui_font_role::body, fg, fill, Mui_align::left);
        if (gwin->get_usecode()->get_global_flag_bool(int(num))) {
            p.text_in_idx(
                    Strings::Msg(0xD06), tr, Mui_font_role::small, sel ? fg : p.col(Mui_colour::success), fill, Mui_align::right);
        }
	};
	flag_list->on_select = [this](int i) {
		if (i >= 0 && i < int(flag_rows.size())) {
			flag_selected = int(flag_rows[size_t(i)]);
			post([this] {
				show_flag();
			});
		}
	};
	flag_list->on_activate = [this](int i) {
		if (i >= 0 && i < int(flag_rows.size())) {
			flag_selected = int(flag_rows[size_t(i)]);
			post([this] {
				toggle_flag(unsigned(flag_selected));
				show_flag();
			});
		}
	};
	auto* row          = body.add<Mui_box>(Mui_box::Dir::horizontal);
	flag_name          = row->add<Mui_label>("", Mui_font_role::heading);
	flag_name->id      = "cheats.flag.name";
	flag_name->stretch = true;
	flag_toggle        = row->add<Mui_toggle>("cheats.flag.toggle", false, [this](bool) {
        post([this] {
            if (flag_selected >= 0) {
                toggle_flag(unsigned(flag_selected));
            }
            show_flag();
        });
    });
	flag_toggle->help  = Strings::Msg(0xD0A);
	auto* adv          = row->add<Mui_button>("cheats.flag.advanced", Strings::Msg(0xD08), Mui_button::Style::secondary, [this] {
        run_classic(CheatScreen::Which::GlobalFlag, nullptr, unsigned(std::max(0, flag_selected)));
    });
	adv->help          = Strings::Msg(0xD09);
	body.add<Help_text>(adv->help);
	show_flag();
}

/*
 *  Usecode and map editor.
 */

void Mui_cheat_window::build_usecode(Mui_box& body) {
	body.add<Mui_section_header>(Strings::Section(5));
	body.add<Mui_label>(Strings::Msg(0xD0C), Mui_font_role::body, Mui_colour::text, true);
	Mui_box* row = control_row(body, Strings::Msg(0xD0D), Strings::Msg(0xD07));
	row->add<Mui_button>(
			   "cheats.usecode.open", Strings::Msg(0xD0D), Mui_button::Style::primary,
			   [this] {
				   run_classic(CheatScreen::Which::Usecode);
			   })
			->help
			= Strings::Msg(0xD07);
	row = control_row(body, Strings::Msg(0xD0E), Strings::Msg(0xD0F));
	row->add<Mui_button>(
			   "cheats.usecode.reload", Strings::Msg(0xD0E), Mui_button::Style::secondary,
			   [this] {
				   post([this] {
					   // As the usecode menu's Reload Usecode.
					   gwin->get_usecode()->read_usecode();
					   message = std::string(Strings::Msg(0xD1D)) + " " + Strings::Msg(0xD0E);
					   rebuild("cheats.usecode.reload");
				   });
			   })
			->help
			= Strings::Msg(0xD0F);
}

void Mui_cheat_window::build_map_editor(Mui_box& body) {
	body.add<Mui_section_header>(Strings::Section(6));

	struct Item {
		const char* id;
		unsigned    label;
		bool        value;
		void (*toggle)();
	};

	const Item toggles[] = {
			{"cheats.mapeditor", 0xD10,  cheat.in_map_editor(),
			 [] {
             cheat.toggle_map_editor();
			 }},
			{ "cheats.tilegrid", 0xD12, cheat.show_tile_grid(),
			 [] {
             cheat.toggle_tile_grid();
			 }},
	};
	for (const auto& it : toggles) {
		const std::string help = Strings::Msg(it.label + 1);
		Mui_box*          row  = control_row(body, Strings::Msg(it.label), help);
		const std::string id   = it.id;
		auto              fn   = it.toggle;
		auto*             t    = row->add<Mui_toggle>(id, it.value, [this, fn, id](bool) {
            post([this, fn, id] {
                fn();
                message.clear();
                rebuild(id);
            });
        });
		t->help                = help;
		// The tile grid shows in map editor mode only.
		if (it.label == 0xD12 && !cheat.in_map_editor()) {
			t->enabled = false;
		}
	}
	body.add<Mui_section_header>(Strings::Msg(0xD1F));

	struct Tool {
		const char* id;
		unsigned    label;
		void (*fn)();
		bool hidden;    // Has its own loop.
	};

	const Tool tools[] = {
			{  "cheats.browser", 0xD14,
			 [] {
             cheat.shape_browser();
			 },  true},
			{   "cheats.sounds", 0xD16,
			 [] {
             cheat.sound_tester();
			 },  true},
			{"cheats.lastshape", 0xD18,
			 [] {
             cheat.create_last_shape();
			 }, false},
	};
	for (const auto& it : tools) {
		const std::string help = Strings::Msg(it.label + 1);
		Mui_box*          row  = control_row(body, Strings::Msg(it.label), help);
		const std::string id   = it.id;
		auto              fn   = it.fn;
		const bool        hide = it.hidden;
		row->add<Mui_button>(
				   id, Strings::Msg(it.label), Mui_button::Style::secondary,
				   [this, fn, id, hide] {
					   post([this, fn, id, hide] {
						   if (hide) {
							   run_hidden([fn] {
								   fn();
							   });
						   } else {
							   fn();
						   }
						   rebuild(id);
					   });
				   })
				->help
				= help;
	}
}

/*
 *  Text-mode menus and tools with their own loops.
 */

void Mui_cheat_window::run_classic(CheatScreen::Which which, Actor* actor, unsigned flag) {
	if (!cscreen) {
		return;
	}
	post([this, which, actor, flag] {
		static const char* const names[] = {"root", "npc", "usecode", "teleport", "flag"};
		run_hidden([this, which, actor, flag] {
			Mui_selftest::on_classic_screen(names[int(which)]);
			cscreen->run_menu_screen(which, actor, flag);
		});
		// Values may have changed there.
		read_world();
		message.clear();
		rebuild();
	});
}

void Mui_cheat_window::invalidate_all() {
	// The window lays out after build(); on the first build its frame
	// widgets do not exist yet.
	if (building) {
		return;
	}
	relayout();
	set_dirty();
}
