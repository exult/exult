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

/*
 *  The game doctor sections of the themed cheat window: Named flags, NPC
 *  browser, Item finder, Check-up and Safety & report. The work is done
 *  by Game_doctor (gamedoctor.h); every change asks first and goes through
 *  Game_doctor::apply_change(), which saves a backup and keeps the undo
 *  history.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Mui_cheats.h"
#include "actors.h"
#include "exult_constants.h"
#include "gamedoctor.h"
#include "gamewin.h"
#include "items.h"

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <utility>

namespace {
	auto Msg(unsigned id) {
		return get_text_msg(id - unsigned(msg_file_start));
	}

	// A message with its %s markers replaced by the arguments in order.
	std::string Fmt(unsigned id, const std::string& a, const std::string& b = {}) {
		std::string out = Msg(id);
		for (const std::string* arg : {&a, &b}) {
			const size_t pos = out.find("%s");
			if (pos == std::string::npos) {
				break;
			}
			out.replace(pos, 2, *arg);
		}
		return out;
	}

	std::string tile_text(const Tile_coord& t, int map) {
		char buf[96];
		snprintf(buf, sizeof(buf), "%d,%d,%d %s %d", t.tx, t.ty, t.tz, Msg(0xD74), map);
		return buf;
	}

	std::string join(const std::vector<std::string>& parts, std::string_view sep) {
		std::string out;
		for (const auto& part : parts) {
			if (!out.empty()) {
				out += sep;
			}
			out += part;
		}
		return out;
	}

	// A count with the singular or plural noun.
	std::string count_text(size_t n, unsigned singular, unsigned plural) {
		return std::to_string(n) + " " + Msg(n == 1 ? singular : plural);
	}

	// Number fields of the item finder: -1 means any.
	int to_query(int v, int any) {
		return v < 0 ? any : v;
	}

	class Help_text : public Mui_label {
	public:
		explicit Help_text(std::string text_) : Mui_label(std::move(text_), Mui_font_role::small, Mui_colour::text_dim, true) {}
	};

	Mui_list* add_list(Mui_box& body, const char* id, int rows, unsigned help) {
		auto* list         = body.add<Mui_list>(id, std::vector<std::string>{}, -1);
		list->visible_rows = rows;
		list->row_ref      = 30;
		list->help         = Msg(help);
		return list;
	}

	Mui_label* add_label(Mui_box& body, const char* id, Mui_font_role role, Mui_colour colour, bool wrap) {
		auto* label = body.add<Mui_label>("", role, colour, wrap);
		label->id   = id;
		return label;
	}

	// Show rows in a list, keeping the selection visible.
	void set_rows(Mui_list* list, std::vector<std::string> rows, int sel) {
		if (!list) {
			return;
		}
		list->rows = std::move(rows);
		list->set_selected(sel, false);
		if (sel < 0) {
			list->scroll = 0;
		}
	}
}    // namespace

void Mui_cheat_window::clear_doctor_widgets() {
	doctor_flag_list     = nullptr;
	doctor_flag_name     = nullptr;
	doctor_flag_desc     = nullptr;
	doctor_flag_toggle   = nullptr;
	doctor_npc_list      = nullptr;
	doctor_npc_details   = nullptr;
	doctor_npc_location  = nullptr;
	doctor_npc_inventory = nullptr;
	doctor_npc_bring     = nullptr;
	doctor_npc_goto      = nullptr;
	doctor_npc_revive    = nullptr;
	doctor_npc_schedule  = nullptr;
	doctor_npc_heal      = nullptr;
	doctor_item_list     = nullptr;
	doctor_item_count    = nullptr;
	doctor_item_goto     = nullptr;
	doctor_item_fetch    = nullptr;
	doctor_checkup_list  = nullptr;
	doctor_checkup_goto  = nullptr;
}

void Mui_cheat_window::forget_world_rows() {
	doctor_flag_rows.clear();
	doctor_npc_rows.clear();
	item_hits.clear();
	item_selected  = -1;
	items_searched = false;
	findings.clear();
	finding_selected = -1;
	checkup_ran      = false;
	npc_rows.clear();
	flag_rows.clear();
	read_world();
}

void Mui_cheat_window::doctor_change(
		const std::string& question, const std::string& focus_id, std::function<Game_doctor::Action_result()> fn) {
	post([this, question, focus_id, fn = std::move(fn)] {
		show_confirm(question, false, [this, focus_id, fn](bool ok) {
			if (ok) {
				const Game_doctor::Action_result result = fn();
				message                                 = result.message;
				read_world();
			}
			rebuild(focus_id);
		});
	});
}

/*
 *  Named flags.
 */

void Mui_cheat_window::filter_doctor_flags() {
	doctor_flag_rows = Game_doctor::get().search_flags(doctor_flag_filter);
	const auto found = std::find_if(doctor_flag_rows.begin(), doctor_flag_rows.end(), [this](const auto& f) {
		return int(f.num) == doctor_flag_selected;
	});
	if (found == doctor_flag_rows.end()) {
		doctor_flag_selected = doctor_flag_rows.empty() ? -1 : int(doctor_flag_rows.front().num);
	}
}

void Mui_cheat_window::show_doctor_flag() {
	Game_doctor&                  doc = Game_doctor::get();
	const Game_doctor::Flag_info* cur = nullptr;
	std::vector<std::string>      rows;
	int                           sel = -1;
	rows.reserve(doctor_flag_rows.size());
	for (size_t i = 0; i < doctor_flag_rows.size(); i++) {
		const auto& f = doctor_flag_rows[i];
		char        buf[256];
		snprintf(buf, sizeof(buf), "0x%04X %s  %s", f.num, f.name.c_str(), Msg(doc.flag_value(f.num) ? 0xD47 : 0xD48));
		rows.emplace_back(buf);
		if (int(f.num) == doctor_flag_selected) {
			sel = int(i);
			cur = &f;
		}
	}
	set_rows(doctor_flag_list, std::move(rows), sel);
	if (doctor_flag_name) {
		std::string text;
		if (cur) {
			char buf[256];
			snprintf(buf, sizeof(buf), "0x%04X (%u) %s", cur->num, cur->num, cur->name.c_str());
			text = buf;
			if (!cur->alias.empty()) {
				text += " / " + cur->alias;
			}
		} else if (doctor_flag_rows.empty()) {
			text = Msg(0xD49);
		}
		doctor_flag_name->text = text;
	}
	if (doctor_flag_desc) {
		doctor_flag_desc->text = cur ? (cur->description.empty() ? std::string(Msg(0xD4D)) : cur->description) : std::string();
	}
	if (doctor_flag_toggle) {
		doctor_flag_toggle->value   = cur && doc.flag_value(cur->num);
		doctor_flag_toggle->enabled = cur != nullptr;
	}
	invalidate_all();
}

void Mui_cheat_window::build_doctor_flags(Mui_box& body) {
	filter_doctor_flags();
	body.add<Help_text>(Msg(0xD4E));
	auto* search        = body.add<Mui_text_field>("doctor.flags.search", doctor_flag_filter, Msg(0xD46));
	search->search_icon = true;
	search->help        = Msg(0xD46);
	search->on_change   = [this](const std::string& s) {
        doctor_flag_filter = s;
        filter_doctor_flags();
        show_doctor_flag();
	};
	doctor_flag_list            = add_list(body, "doctor.flags.list", 8, 0xD4A);
	doctor_flag_list->on_select = [this](int i) {
		if (i >= 0 && i < int(doctor_flag_rows.size())) {
			doctor_flag_selected = int(doctor_flag_rows[size_t(i)].num);
			post([this] {
				show_doctor_flag();
			});
		}
	};
	auto* row                 = body.add<Mui_box>(Mui_box::Dir::horizontal);
	doctor_flag_name          = row->add<Mui_label>("", Mui_font_role::heading);
	doctor_flag_name->id      = "doctor.flags.name";
	doctor_flag_name->stretch = true;
	doctor_flag_toggle        = row->add<Mui_toggle>("doctor.flags.toggle", false, [this](bool) {
        if (doctor_flag_selected < 0) {
            return;
        }
        const unsigned    num  = unsigned(doctor_flag_selected);
        const bool        on   = !Game_doctor::get().flag_value(num);
        const auto*       info = Game_doctor::get().find_flag(num);
        const std::string name = info ? info->name : std::to_string(num);
        // The toggle shows the real value again after the question.
        doctor_change(Fmt(on ? 0xD4B : 0xD4C, name), "doctor.flags.toggle", [num, on] {
            return Game_doctor::get().set_flag(num, on);
        });
    });
	doctor_flag_toggle->help  = Msg(0xD4A);
	doctor_flag_desc          = add_label(body, "doctor.flags.desc", Mui_font_role::body, Mui_colour::text_dim, true);
	show_doctor_flag();
}

/*
 *  NPC browser.
 */

void Mui_cheat_window::filter_doctor_npcs() {
	doctor_npc_rows = Game_doctor::get().find_npcs(doctor_npc_filter);
	if (std::find(doctor_npc_rows.begin(), doctor_npc_rows.end(), doctor_npc_selected) == doctor_npc_rows.end()) {
		doctor_npc_selected = doctor_npc_rows.empty() ? -1 : doctor_npc_rows.front();
	}
}

void Mui_cheat_window::show_doctor_npc() {
	Game_doctor&             doc = Game_doctor::get();
	std::vector<std::string> rows;
	int                      sel = -1;
	rows.reserve(doctor_npc_rows.size());
	for (size_t i = 0; i < doctor_npc_rows.size(); i++) {
		const int    num = doctor_npc_rows[i];
		const Actor* a   = gwin->get_npc(num);
		char         buf[200];
		snprintf(buf, sizeof(buf), "%03d %s - %s", num, a ? a->get_npc_name().c_str() : "", Msg(a && a->is_dead() ? 0xD51 : 0xD50));
		rows.emplace_back(buf);
		if (num == doctor_npc_selected) {
			sel = int(i);
		}
	}
	set_rows(doctor_npc_list, std::move(rows), sel);
	const Game_doctor::Npc_info info = doctor_npc_selected >= 0 ? doc.npc_info(doctor_npc_selected) : Game_doctor::Npc_info{};
	const bool                  have = info.num >= 0;
	if (doctor_npc_details) {
		std::string text = Msg(0xD52);
		if (have) {
			char buf[160];
			snprintf(
					buf, sizeof(buf), "%03d %s - %s   %s: %s   %s: %s   %s: %d/%d", info.num, info.name.c_str(),
					Msg(info.alive ? 0xD50 : 0xD51), Msg(0xD53), info.schedule_name.c_str(), Msg(0xD54),
					info.alignment_name.c_str(), Msg(0xD56), info.health, info.strength);
			text = buf;
			text += std::string("   ") + Msg(0xD55) + ": "
					+ (info.flags.empty() ? std::string(Msg(0xD67)) : join(info.flags, ", "));
		}
		doctor_npc_details->text = text;
	}
	if (doctor_npc_location) {
		std::string text;
		if (have) {
			text = std::string(Msg(0xD57)) + ": " + (info.on_map ? tile_text(info.tile, info.map) : std::string(Msg(0xD58)));
			if (info.has_body) {
				text += std::string("   ") + Msg(0xD59) + ": " + tile_text(info.body_tile, info.body_map);
				if (!info.body_where.empty()) {
					text += std::string(" - ") + Msg(0xD75) + " " + info.body_where;
				}
			}
		}
		doctor_npc_location->text = text;
	}
	if (doctor_npc_inventory) {
		std::string text;
		if (have) {
			std::vector<std::string> items;
			for (const auto& row : info.inventory) {
				// Item names already say how many ("50 gold coins").
				items.push_back(row.name);
			}
			text = std::string(Msg(0xD5A)) + ": " + (items.empty() ? std::string(Msg(0xD5B)) : join(items, ", "));
		}
		doctor_npc_inventory->text = text;
	}
	const bool alive = have && info.alive;
	if (doctor_npc_bring) {
		doctor_npc_bring->enabled = alive && info.num != 0;
	}
	if (doctor_npc_goto) {
		doctor_npc_goto->enabled = alive && info.on_map && info.num != 0;
	}
	if (doctor_npc_revive) {
		doctor_npc_revive->enabled = have && !info.alive;
	}
	if (doctor_npc_schedule) {
		doctor_npc_schedule->enabled = alive && info.num != 0;
	}
	if (doctor_npc_heal) {
		doctor_npc_heal->enabled = alive;
	}
	invalidate_all();
}

void Mui_cheat_window::build_doctor_npcs(Mui_box& body) {
	filter_doctor_npcs();
	body.add<Help_text>(Msg(0xD66));
	auto* search        = body.add<Mui_text_field>("doctor.npcs.search", doctor_npc_filter, Msg(0xD4F));
	search->search_icon = true;
	search->help        = Msg(0xD4F);
	search->on_change   = [this](const std::string& s) {
        doctor_npc_filter = s;
        filter_doctor_npcs();
        show_doctor_npc();
	};
	doctor_npc_list            = add_list(body, "doctor.npcs.list", 7, 0xD66);
	doctor_npc_list->on_select = [this](int i) {
		if (i >= 0 && i < int(doctor_npc_rows.size())) {
			doctor_npc_selected = doctor_npc_rows[size_t(i)];
			post([this] {
				show_doctor_npc();
			});
		}
	};
	doctor_npc_details   = add_label(body, "doctor.npcs.details", Mui_font_role::body, Mui_colour::text, true);
	doctor_npc_location  = add_label(body, "doctor.npcs.location", Mui_font_role::body, Mui_colour::text_dim, true);
	doctor_npc_inventory = add_label(body, "doctor.npcs.inventory", Mui_font_role::small, Mui_colour::text_dim, true);
	auto* actions        = body.add<Mui_box>(Mui_box::Dir::horizontal);

	struct Action {
		const char*  id;
		unsigned     label;
		unsigned     question;
		Mui_button** widget;
		Game_doctor::Action_result (Game_doctor::*fn)(int);
	};

	const Action list[] = {
			{   "doctor.npcs.bring", 0xD5C, 0xD61,    &doctor_npc_bring,        &Game_doctor::bring_npc},
			{    "doctor.npcs.goto", 0xD5D, 0xD62,     &doctor_npc_goto,         &Game_doctor::goto_npc},
			{  "doctor.npcs.revive", 0xD5E, 0xD63,   &doctor_npc_revive,       &Game_doctor::revive_npc},
			{"doctor.npcs.schedule", 0xD5F, 0xD64, &doctor_npc_schedule, &Game_doctor::restore_schedule},
			{    "doctor.npcs.heal", 0xD60, 0xD65,     &doctor_npc_heal,         &Game_doctor::heal_npc},
	};
	for (const auto& it : list) {
		const std::string id       = it.id;
		const unsigned    question = it.question;
		const auto        fn       = it.fn;
		*it.widget         = actions->add<Mui_button>(id, Msg(it.label), Mui_button::Style::secondary, [this, id, question, fn] {
            const int num = doctor_npc_selected;
            if (num < 0) {
                return;
            }
            const Actor* a = gwin->get_npc(num);
            doctor_change(Fmt(question, a ? Game_doctor::object_name(a) : std::to_string(num)), id, [num, fn] {
                return (Game_doctor::get().*fn)(num);
            });
        });
		(*it.widget)->help = Msg(0xD66);
	}
	show_doctor_npc();
}

/*
 *  Item finder.
 */

void Mui_cheat_window::search_doctor_items() {
	Game_doctor::Item_query query;
	query.shape    = to_query(item_shape, c_any_shapenum);
	query.frame    = to_query(item_frame, c_any_framenum);
	query.quality  = to_query(item_quality, c_any_qual);
	query.name     = item_name;
	items_searched = true;
	item_hits      = Game_doctor::get().find_items(query);
	item_selected  = item_hits.empty() ? -1 : std::clamp(item_selected, 0, int(item_hits.size()) - 1);
}

void Mui_cheat_window::show_doctor_items() {
	std::vector<std::string> rows;
	rows.reserve(item_hits.size());
	for (const auto& hit : item_hits) {
		char buf[128];
		snprintf(buf, sizeof(buf), "%d:%d %s %d - ", hit.shape, hit.frame, Msg(0xD8C), hit.quality);
		std::string row = buf;
		row += hit.tile.tx >= 0 ? tile_text(hit.tile, hit.map) : std::string(Msg(0xD58));
		if (!hit.chain.empty()) {
			row += std::string(" - ") + Msg(0xD75) + " " + join(hit.chain, " > ");
		}
		row += "  " + hit.name;
		rows.push_back(std::move(row));
	}
	set_rows(doctor_item_list, std::move(rows), item_selected);
	if (doctor_item_count) {
		doctor_item_count->text = !items_searched     ? std::string(Msg(0xD8D))
								  : item_hits.empty() ? std::string(Msg(0xD6F))
													  : count_text(item_hits.size(), 0xD93, 0xD6E);
	}
	const bool have = item_selected >= 0 && item_selected < int(item_hits.size());
	if (doctor_item_goto) {
		doctor_item_goto->enabled = have && item_hits[size_t(item_selected)].tile.tx >= 0;
	}
	if (doctor_item_fetch) {
		doctor_item_fetch->enabled = have;
	}
	invalidate_all();
}

void Mui_cheat_window::build_doctor_items(Mui_box& body) {
	body.add<Help_text>(Msg(0xD6D));
	auto* fields = body.add<Mui_box>(Mui_box::Dir::horizontal);
	fields->gap  = 12;
	auto number  = [&](const char* id, unsigned label, int maxv, int& var) {
        auto* col = fields->add<Mui_box>(Mui_box::Dir::vertical);
        col->gap  = 2;
        col->add<Mui_label>(Msg(label), Mui_font_role::small, Mui_colour::text_dim);
        col->add<Mui_number_field>(
                   id, -1, maxv, 1, std::clamp(var, -1, maxv),
                   [&var](int v) {
                       var = v;
                   })
                ->help
                = Msg(0xD6D);
	};
	// Mods can add shapes beyond c_max_shapes.
	number("doctor.items.shape", 0xD68, 0xFFFF, item_shape);
	number("doctor.items.frame", 0xD69, 1023, item_frame);
	number("doctor.items.quality", 0xD6A, 255, item_quality);
	auto* namecol    = fields->add<Mui_box>(Mui_box::Dir::vertical);
	namecol->gap     = 2;
	namecol->stretch = true;
	namecol->add<Mui_label>(Msg(0xD6B), Mui_font_role::small, Mui_colour::text_dim);
	auto* name      = namecol->add<Mui_text_field>("doctor.items.name", item_name, Msg(0xD6B));
	name->help      = Msg(0xD6B);
	name->on_change = [this](const std::string& s) {
		item_name = s;
	};
	auto* gocol = fields->add<Mui_box>(Mui_box::Dir::vertical);
	gocol->gap  = 2;
	gocol->add<Mui_label>(" ", Mui_font_role::small);
	auto* search    = gocol->add<Mui_button>("doctor.items.search", Msg(0xD6C), Mui_button::Style::primary, [this] {
        post([this] {
            if (item_shape < 0 && item_frame < 0 && item_quality < 0 && item_name.empty()) {
                items_searched = false;
                item_hits.clear();
                item_selected = -1;
                message       = Msg(0xD8D);
            } else {
                item_selected = 0;
                search_doctor_items();
                message.clear();
            }
            rebuild("doctor.items.search");
        });
    });
	search->help    = Msg(0xD8E);
	name->on_submit = [search](const std::string&) {
		if (search->on_click) {
			search->on_click();
		}
	};
	doctor_item_count           = add_label(body, "doctor.items.count", Mui_font_role::small, Mui_colour::text_dim, false);
	doctor_item_list            = add_list(body, "doctor.items.list", 8, 0xD90);
	doctor_item_list->on_select = [this](int i) {
		if (i >= 0 && i < int(item_hits.size())) {
			item_selected = i;
			post([this] {
				show_doctor_items();
			});
		}
	};
	auto* actions     = body.add<Mui_box>(Mui_box::Dir::horizontal);
	auto  item_action = [this](const char* id, unsigned question, bool teleport) {
        const std::string wid = id;
        return [this, wid, question, teleport] {
            if (item_selected < 0 || item_selected >= int(item_hits.size())) {
                return;
            }
            const Game_doctor::Item_hit hit = item_hits[size_t(item_selected)];
            doctor_change(Fmt(question, hit.name), wid, [this, hit, teleport] {
                Game_doctor::Action_result r = teleport ? Game_doctor::get().goto_item(hit) : Game_doctor::get().fetch_item(hit);
                // Where the items are now.
                search_doctor_items();
                return r;
            });
        };
	};
	doctor_item_goto = actions->add<Mui_button>(
			"doctor.items.goto", Msg(0xD70), Mui_button::Style::secondary, item_action("doctor.items.goto", 0xD72, true));
	doctor_item_goto->help = Msg(0xD90);
	doctor_item_fetch      = actions->add<Mui_button>(
            "doctor.items.fetch", Msg(0xD71), Mui_button::Style::secondary, item_action("doctor.items.fetch", 0xD73, false));
	doctor_item_fetch->help = Msg(0xD90);
	show_doctor_items();
}

/*
 *  Check-up.
 */

void Mui_cheat_window::show_doctor_findings() {
	std::vector<std::string> rows;
	rows.reserve(findings.size());
	for (const auto& f : findings) {
		rows.push_back(f.severity + " " + f.id + ": " + f.message);
	}
	set_rows(doctor_checkup_list, std::move(rows), finding_selected);
	if (doctor_checkup_goto) {
		const bool have = finding_selected >= 0 && finding_selected < int(findings.size());
		doctor_checkup_goto->enabled
				= have && (findings[size_t(finding_selected)].tile.tx >= 0 || findings[size_t(finding_selected)].npc > 0);
	}
	invalidate_all();
}

void Mui_cheat_window::build_doctor_checkup(Mui_box& body) {
	body.add<Help_text>(Msg(0xD77));
	auto* bar     = body.add<Mui_box>(Mui_box::Dir::horizontal);
	auto* run     = bar->add<Mui_button>("doctor.checkup.run", Msg(0xD76), Mui_button::Style::primary, [this] {
        post([this] {
            findings         = Game_doctor::get().run_checkup();
            checkup_ran      = true;
            finding_selected = findings.empty() ? -1 : 0;
            message.clear();
            rebuild("doctor.checkup.run");
        });
    });
	run->help     = Msg(0xD77);
	auto* summary = bar->add<Mui_label>(
			!checkup_ran       ? std::string(Msg(0xD7A))
			: findings.empty() ? std::string(Msg(0xD79))
							   : count_text(findings.size(), 0xD7D, 0xD78),
			Mui_font_role::heading);
	summary->id                    = "doctor.checkup.summary";
	summary->stretch               = true;
	doctor_checkup_list            = add_list(body, "doctor.checkup.list", 9, 0xD91);
	doctor_checkup_list->on_select = [this](int i) {
		if (i >= 0 && i < int(findings.size())) {
			finding_selected = i;
			post([this] {
				show_doctor_findings();
			});
		}
	};
	doctor_checkup_goto         = body.add<Mui_button>("doctor.checkup.goto", Msg(0xD7B), Mui_button::Style::secondary, [this] {
        if (finding_selected < 0 || finding_selected >= int(findings.size())) {
            return;
        }
        const Game_doctor::Finding f = findings[size_t(finding_selected)];
        doctor_change(Fmt(0xD7C, f.id), "doctor.checkup.goto", [f] {
            Game_doctor& doc = Game_doctor::get();
            if (f.tile.tx < 0) {
                return doc.goto_npc(f.npc);
            }
            return doc.apply_change(Fmt(0xD8B, f.id), Game_doctor::Undo_entry::Kind::teleport, [f] {
                Game_window::get_instance()->teleport_party(f.tile, false, f.map);
                return Game_doctor::Action_result{true, Fmt(0xD8B, f.id)};
            });
        });
    });
	doctor_checkup_goto->help   = Msg(0xD91);
	doctor_checkup_goto->fill_w = false;
	show_doctor_findings();
}

/*
 *  Safety and report.
 */

void Mui_cheat_window::build_doctor_safety(Mui_box& body) {
	Game_doctor& doc = Game_doctor::get();
	body.add<Help_text>(Msg(0xD89));
	body.add<Mui_section_header>(Msg(0xD7E));
	std::string backup = Msg(0xD7F);
	if (!doc.backup_name().empty()) {
		const int   slot  = doc.backup_slot_number();
		std::string title = slot >= 0 && slot < 10 ? gwin->get_save_name(size_t(slot)) : std::string();
		backup            = Fmt(0xD80, std::to_string(slot), title.empty() ? doc.backup_name() : title);
	}
	add_label(body, "doctor.safety.backup", Mui_font_role::body, Mui_colour::text, true)->text = backup;
	body.add<Mui_section_header>(Msg(0xD81));
	const auto labels = doc.undo_labels();
	add_label(body, "doctor.safety.history", Mui_font_role::body, Mui_colour::text_dim, true)->text
			= labels.empty() ? std::string(Msg(0xD82)) : join(labels, "\n");
	auto* undo    = body.add<Mui_button>("doctor.safety.undo", Msg(0xD83), Mui_button::Style::secondary, [this] {
        const auto last = Game_doctor::get().undo_labels();
        if (last.empty()) {
            return;
        }
        post([this, label = last.front()] {
            show_confirm(Fmt(0xD84, label), true, [this](bool ok) {
                if (ok) {
                    message = Game_doctor::get().undo().message;
                    // A snapshot undo replaced the world.
                    forget_world_rows();
                }
                rebuild("doctor.safety.undo");
            });
        });
    });
	undo->help    = Msg(0xD92);
	undo->enabled = doc.undo_depth() > 0;
	undo->fill_w  = false;
	body.add<Mui_section_header>(Msg(0xD8F));
	auto* exp   = body.add<Mui_button>("doctor.safety.export", Msg(0xD85), Mui_button::Style::primary, [this] {
        post([this] {
            std::string err;
            report_path = Game_doctor::get().export_bug_report(&err);
            message     = report_path.empty() ? Fmt(0xD8A, err) : Fmt(0xD87, report_path);
            rebuild("doctor.safety.export");
        });
    });
	exp->help   = Msg(0xD86);
	exp->fill_w = false;
	body.add<Help_text>(exp->help);
	add_label(body, "doctor.safety.report", Mui_font_role::small, Mui_colour::text_dim, true)->text
			= report_path.empty() ? std::string(Msg(0xD88)) : Fmt(0xD87, report_path);
}
