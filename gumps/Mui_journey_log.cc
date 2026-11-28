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

#include "Mui_journey_log.h"

#include "Configuration.h"
#include "Mui_font.h"
#include "Newfile_gump.h"
#include "actors.h"
#include "exceptions.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "listfiles.h"
#include "party.h"
#include "utils.h"
#include "version.h"
#include "vgafile.h"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

namespace {
	// Thread-safe localtime. Returns false if the conversion fails.
	bool local_time(const time_t& t, tm& out) {
#ifdef _WIN32
		return localtime_s(&out, &t) == 0;
#else
		return localtime_r(&t, &out) != nullptr;
#endif
	}

	class Strings : public GumpStrings {
	public:
		static auto JourneyLog() {
			return get_text_msg(0xC12 - msg_file_start);
		}

		static auto Msg(unsigned id) {
			return get_text_msg(id - unsigned(msg_file_start));
		}

		static auto SearchSaves() {
			return Msg(0xC96);
		}

		static auto SortName(int i) {
			return Msg(0xC97 + unsigned(i));
		}

		static auto Cards() {
			return Msg(0xC9B);
		}

		static auto List() {
			return Msg(0xC9C);
		}

		static auto NewSave() {
			return Msg(0xC9D);
		}

		static auto NewSaveHint() {
			return Msg(0xC9E);
		}

		static auto QuickSave() {
			return Msg(0xC9F);
		}

		static auto Unreadable() {
			return Msg(0xCA0);
		}

		static auto Day() {
			return Msg(0xCA1);
		}

		static auto PlayTime() {
			return Msg(0xCA2);
		}

		static auto SavedAt() {
			return Msg(0xCA3);
		}

		static auto InGameTime() {
			return Msg(0xCA4);
		}

		static auto TimesSaved() {
			return Msg(0xCA5);
		}

		static auto Party() {
			return Msg(0xCA6);
		}

		static auto Level() {
			return Msg(0xCA7);
		}

		static auto Health() {
			return Msg(0xCA8);
		}

		static auto Game() {
			return Msg(0xCA9);
		}

		static auto RunningBuild() {
			return Msg(0xCAA);
		}

		static auto Load() {
			return Msg(0xCAB);
		}

		static auto SaveHere() {
			return Msg(0xCAC);
		}

		static auto Rename() {
			return Msg(0xCAD);
		}

		static auto Delete() {
			return Msg(0xCAE);
		}

		static auto NewSavePrompt() {
			return Msg(0xCAF);
		}

		static auto RenamePrompt() {
			return Msg(0xCB0);
		}

		static auto Overwrite() {
			return Msg(0xCB1);
		}

		static auto DeleteConfirm() {
			return Msg(0xCB2);
		}

		static auto RenameFailed() {
			return Msg(0xCB3);
		}

		static auto SavesWord() {
			return Msg(0xCB4);
		}

		static auto SaveWord() {
			return Msg(0xCB5);
		}

		static auto NoMatches() {
			return Msg(0xCB6);
		}

		static auto SortHelp() {
			return Msg(0xCB7);
		}

		static auto ViewHelp() {
			return Msg(0xCB8);
		}

		static auto LoadHelp() {
			return Msg(0xCB9);
		}

		static auto SaveHelp() {
			return Msg(0xCBA);
		}

		static auto RenameHelp() {
			return Msg(0xCBB);
		}

		static auto DeleteHelp() {
			return Msg(0xCBC);
		}

		static auto SearchHelp() {
			return Msg(0xCBD);
		}

		static auto DefaultHelp() {
			return Msg(0xCBE);
		}

		static auto File() {
			return Msg(0xCBF);
		}

		static auto Hours() {
			return Msg(0xCC0);
		}

		static auto Minutes() {
			return Msg(0xCC1);
		}

		static auto NotSaved() {
			return Msg(0xCC2);
		}

		static auto SaveFailed() {
			return Msg(0xCC3);
		}

		static auto Mod() {
			return Msg(0xCC4);
		}

		static auto PlayTimeNote() {
			return Msg(0xCC5);
		}

		// Classic month abbreviations (Newfile_gump).
		static auto Month(int month) {
			if (month < 0 || month > 11) {
				return "";
			}
			return Msg(0x670 + unsigned(month));
		}
	};

	constexpr const char* view_key    = "config/gameplay/menu_journey_view";
	constexpr const char* sort_key    = "config/gameplay/menu_journey_sort";
	constexpr const char* sort_keys[] = {"newest", "oldest", "name", "day"};

	// Card and row sizes (reference pixels).
	constexpr float card_w_ref    = 216;
	constexpr float card_h_ref    = 212;
	constexpr float thumb_h_ref   = 124;
	constexpr float row_h_ref     = 68;
	constexpr float details_w_ref = 330;

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	std::string two(int v) {
		char buf[8];
		snprintf(buf, sizeof(buf), "%02d", v);
		return buf;
	}
}    // namespace

/*
 *  One card.
 */

struct Mui_journey_window::Save {
	enum class Kind {
		current,
		quick,
		slot
	};

	Kind                              kind = Kind::slot;
	int                               num  = -1;
	std::string                       filename;
	std::string                       name;
	bool                              readable = true;
	std::unique_ptr<SaveGame_Details> details;
	std::unique_ptr<SaveGame_Party[]> party;
	std::unique_ptr<Shape_file>       shot;

	int slot_key() const {
		return kind == Kind::current ? -2 : kind == Kind::quick ? -1 : num;
	}

	int party_count() const {
		return details && party ? std::clamp(int(details->party_size), 0, 8) : 0;
	}

	std::string member_name(int i) const {
		const char* n = party[i].name;
		return std::string(n, strnlen(n, sizeof(party[i].name)));
	}

	std::string file_base() const {
		const auto slash = filename.find_last_of("/\\");
		return slash == std::string::npos ? filename : filename.substr(slash + 1);
	}
};

namespace {
	using Save = Mui_journey_window::Save;

	std::string day_time(const SaveGame_Details* d) {
		if (!d) {
			return {};
		}
		return std::string(Strings::Day()) + " " + std::to_string(d->game_day) + ", " + two(d->game_hour) + ":"
			   + two(d->game_minute);
	}

	// Empty when not recorded (painted as a dash).
	std::string play_time(const SaveGame_Details* d) {
		if (!d || d->play_seconds == 0) {
			return {};
		}
		const uint32 h = d->play_seconds / 3600;
		const uint32 m = (d->play_seconds % 3600) / 60;
		if (h > 0) {
			return std::to_string(h) + " " + Strings::Hours() + " " + two(int(m)) + " " + Strings::Minutes();
		}
		return std::to_string(m) + " " + Strings::Minutes();
	}

	// d Mon yyyy HH:MM
	std::string saved_at(const SaveGame_Details* d) {
		if (!d) {
			return {};
		}
		return std::to_string(d->real_day) + " " + Strings::Month(d->real_month - 1) + " " + std::to_string(d->real_year) + " "
			   + two(d->real_hour) + ":" + two(d->real_minute);
	}

	std::string running_build() {
		std::string out = "Exult " VERSION;
		const auto  rev = VersionGetGitRevision(true);
		if (!rev.empty()) {
			out += " (" + std::string(rev) + ")";
		}
		return out;
	}

	// Menu strings can hold line breaks (Forge of Virtue's title has
	// one); the details panel shows them on one line.
	std::string one_line(std::string s) {
		for (char& c : s) {
			if (c == '\n' || c == '\r') {
				c = ' ';
			}
		}
		return s;
	}

	std::string game_name() {
		std::string g = Game::get_menustring();
		if (g.empty()) {
			g = Game::get_gametitle();
		}
		return one_line(g);
	}

	// Classic Newfile_gump::SaveInfo::CompareThis: newest first, saves
	// without details last, then by slot number.
	int compare_dates(const Save& a, const Save& b) {
		if (a.details && b.details) {
			const auto* x    = a.details.get();
			const auto* y    = b.details.get();
			const int   fx[] = {x->real_year, x->real_month, x->real_day, x->real_hour, x->real_minute, x->real_second};
			const int   fy[] = {y->real_year, y->real_month, y->real_day, y->real_hour, y->real_minute, y->real_second};
			for (size_t i = 0; i < std::size(fx); i++) {
				if (fx[i] != fy[i]) {
					return fx[i] < fy[i] ? 1 : -1;
				}
			}
		} else if (a.details) {
			return -1;
		} else if (b.details) {
			return 1;
		}
		return a.num - b.num;
	}

	int compare_days(const Save& a, const Save& b) {
		if (a.details && b.details) {
			const auto* x  = a.details.get();
			const auto* y  = b.details.get();
			const long  tx = (long(x->game_day) * 24 + x->game_hour) * 60 + x->game_minute;
			const long  ty = (long(y->game_day) * 24 + y->game_hour) * 60 + y->game_minute;
			if (tx != ty) {
				return tx < ty ? 1 : -1;
			}
		} else if (a.details) {
			return -1;
		} else if (b.details) {
			return 1;
		}
		return a.num - b.num;
	}

	// Thumbnail at the largest whole scale that fits r, with a 1-px frame.
	TileRect thumb_rect(Shape_file* shot, const TileRect& r, int max_scale = 4) {
		int fw = 96;
		int fh = 60;
		if (shot && shot->get_frame(0)) {
			fw = std::max(1, shot->get_frame(0)->get_width());
			fh = std::max(1, shot->get_frame(0)->get_height());
		}
		const int s = std::clamp(std::min((r.w - 2) / fw, (r.h - 2) / fh), 1, max_scale);
		const int w = fw * s;
		const int h = fh * s;
		return TileRect(r.x + (r.w - w) / 2, r.y + (r.h - h) / 2, w, h);
	}

	void paint_thumb(Mui_painter& p, const Save* s, const TileRect& r, int max_scale = 4) {
		Shape_file*    shot = s ? s->shot.get() : nullptr;
		const TileRect t    = thumb_rect(shot, r, max_scale);
		TileRect       f    = t;
		f.enlarge(1);
		p.frame_rect(f, 1, p.col(Mui_colour::frame_dark));
		if (shot && shot->get_frame(0)) {
			p.blit_shape(shot->get_frame(0), t, false);
		} else {
			p.fill_rect(t, p.col(Mui_colour::frame_dark));
		}
		if (s && s->kind == Save::Kind::current) {
			// A plus in a disc marks the New save card.
			const int d = std::max(p.px(28), t.h / 3);
			TileRect  c(t.x + (t.w - d) / 2, t.y + (t.h - d) / 2, d, d);
			p.rounded_fill(c, d / 2, p.col(Mui_colour::accent));
			TileRect i = c;
			i.enlarge(-d / 5);
			p.icon(Mui_icon::plus, i, p.col(Mui_colour::text_on_accent));
		}
	}

	// A dash for values that were not recorded.
	void paint_dash(Mui_painter& p, const TileRect& r, Mui_font_role role, uint8 c) {
		const int w = std::max(p.px(10), p.line_height(role) * 2 / 3);
		p.fill_rect(TileRect(r.x, r.y + r.h / 2, w, std::max(1, p.px(1.5f))), c);
	}

	void paint_value(Mui_painter& p, const std::string& s, const TileRect& r, Mui_font_role role, uint8 fg, uint8 bg) {
		if (s.empty()) {
			paint_dash(p, r, role, fg);
		} else {
			p.text_in_idx(s, r, role, fg, bg, Mui_align::left);
		}
	}

	void paint_badge(Mui_painter& p, const std::string& text, int x, int y) {
		const int tw = p.text_width(text, Mui_font_role::small);
		const int h  = p.line_height(Mui_font_role::small) + p.px(4);
		TileRect  r(x, y, tw + p.px(12), h);
		p.rounded_fill(r, p.px(4), p.col(Mui_colour::danger));
		p.text_in_idx(
				text, r, Mui_font_role::small, p.col(Mui_colour::text_on_danger), p.col(Mui_colour::danger), Mui_align::centre);
	}

	std::string card_title(const Save* s) {
		return s->kind == Save::Kind::current ? std::string(Strings::NewSave()) : s->name;
	}

	/*
	 *  Two columns: the saves on the left, the details panel on the right.
	 *  The details stay at the top of the visible part while the saves
	 *  scroll.
	 */
	class Columns : public Mui_box {
	public:
		Columns() : Mui_box(Dir::horizontal, "journey.columns") {}

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			int lw = 0;
			int rw = 0;
			widths(p, avail_w, lw, rw);
			w = avail_w;
			h = 0;
			for (size_t i = 0; i < children.size() && i < 2; i++) {
				int cw = 0;
				int ch = 0;
				children[i]->measure(p, i == 0 ? lw : rw, cw, ch);
				h = std::max(h, ch);
			}
		}

		void layout(Mui_painter& p) override {
			int lw = 0;
			int rw = 0;
			widths(p, rect.w, lw, rw);
			int x = rect.x;
			for (size_t i = 0; i < children.size() && i < 2; i++) {
				const int cwant = i == 0 ? lw : rw;
				int       cw    = 0;
				int       ch    = 0;
				children[i]->measure(p, cwant, cw, ch);
				children[i]->rect = TileRect(x, rect.y, cwant, ch);
				children[i]->layout(p);
				x += cwant + gap_px(p);
			}
		}

		void paint(Mui_painter& p) override {
			if (children.size() > 1) {
				Mui_widget*    side = children[1].get();
				const TileRect clip = p.get_clip();
				const int      top  = std::max(rect.y, std::min(clip.y, rect.y + rect.h - side->rect.h));
				side->rect.y        = top;
			}
			Mui_box::paint(p);
		}

	private:
		void widths(Mui_painter& p, int avail_w, int& lw, int& rw) const {
			rw = std::min(p.px(details_w_ref), avail_w / 2);
			lw = std::max(1, avail_w - rw - gap_px(p));
		}
	};

	/*
	 *  The details of the selected card, painted directly (no child
	 *  widgets, so a new selection only needs a relayout).
	 */
	class Details : public Mui_widget {
	public:
		explicit Details(Mui_journey_window& w) : Mui_widget("journey.details"), owner(w) {}

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			w = avail_w;
			h = run(p, nullptr, avail_w);
		}

		void paint(Mui_painter& p) override {
			if (p.get_theme().panel_parchment) {
				p.fill_rect(rect, p.col(Mui_colour::panel_alt));
				p.bevel(rect, std::max(1, p.px(2)), p.col(Mui_colour::frame_light), p.col(Mui_colour::frame_dark));
			} else {
				p.rounded_fill(rect, p.px(p.get_theme().metrics.radius), p.col(Mui_colour::panel_alt));
			}
			run(p, &p, rect.w);
		}

		uint8 surface(const Mui_painter& p) const override {
			return p.col(Mui_colour::panel_alt);
		}

		// Play time of the selection, for the test driver.
		std::string value_text() const override {
			const Save* s = owner.selected_save();
			if (!s) {
				return "<none>";
			}
			const std::string t = play_time(s->details.get());
			return t.empty() ? "none" : t;
		}

	private:
		Mui_journey_window& owner;

		// Lay out (and paint when out is set); returns the height.
		int run(Mui_painter& p, Mui_painter* out, int width) {
			const Save* s   = owner.selected_save();
			const int   pad = p.px(12);
			const int   x   = rect.x + pad;
			const int   w   = std::max(1, width - 2 * pad);
			int         y   = (out ? rect.y : 0) + pad;
			const uint8 bg  = p.col(Mui_colour::panel_alt);
			const uint8 ink = p.col(Mui_colour::text);
			const uint8 dim = p.col(Mui_colour::text_dim);
			const int   sm  = p.line_height(Mui_font_role::small);
			const int   bd  = p.line_height(Mui_font_role::body);
			const int   hd  = p.line_height(Mui_font_role::heading);
			const int   gap = p.px(6);
			if (!s) {
				if (out) {
					out->text_in_idx(Strings::NoMatches(), TileRect(x, y, w, bd), Mui_font_role::body, dim, bg, Mui_align::left);
				}
				return 2 * pad + bd;
			}
			// Screenshot.
			const TileRect tr(x, y, w, p.px(thumb_h_ref));
			const TileRect t = thumb_rect(s->shot.get(), tr, 3);
			if (out) {
				paint_thumb(*out, s, tr, 3);
			}
			y = t.y + t.h + 2 + gap;
			// Name, and a badge for unreadable saves.
			if (out) {
				out->text_in_idx(card_title(s), TileRect(x, y, w, hd), Mui_font_role::heading, ink, bg, Mui_align::left);
			}
			y += hd;
			if (!s->readable) {
				if (out) {
					paint_badge(*out, Strings::Unreadable(), x, y + p.px(2));
				}
				y += sm + p.px(8);
			}
			y += gap;
			const int lw  = std::min(w / 2, std::max(p.px(110), w * 2 / 5));
			auto      row = [&](const std::string& label, const std::string& value) {
                if (out) {
                    out->text_in_idx(label, TileRect(x, y, lw, bd), Mui_font_role::small, dim, bg, Mui_align::left);
                    paint_value(*out, value, TileRect(x + lw, y, w - lw, bd), Mui_font_role::body, ink, bg);
                }
                y += bd + p.px(2);
			};
			const SaveGame_Details* d = s->details.get();
			if (s->kind == Save::Kind::current) {
				row(Strings::SavedAt(), Strings::NotSaved());
			} else {
				row(Strings::SavedAt(), saved_at(d));
			}
			row(Strings::InGameTime(), day_time(d));
			row(Strings::PlayTime(), play_time(d));
			if (d && s->kind != Save::Kind::current) {
				row(Strings::TimesSaved(), std::to_string(d->save_count));
			}
			if (!s->filename.empty()) {
				row(Strings::File(), s->file_base());
			}
			// Party.
			const int n = s->party_count();
			if (n > 0) {
				y += gap;
				if (out) {
					out->text_in_idx(Strings::Party(), TileRect(x, y, w, hd), Mui_font_role::heading, ink, bg, Mui_align::left);
					out->divider(x, y + hd, w);
				}
				y += hd + p.px(4);
				for (int i = 0; i < n; i++) {
					const SaveGame_Party& m     = s->party[i];
					const std::string     stats = std::string(Strings::Level()) + " " + std::to_string(1 + Log2(m.exp / 50)) + "   "
											  + Strings::Health() + " " + std::to_string(m.health);
					if (out) {
						const int sw = out->text_width(stats, Mui_font_role::small);
						out->text_in_idx(
								s->member_name(i), TileRect(x, y, w - sw - p.px(8), bd), Mui_font_role::body, ink, bg,
								Mui_align::left);
						out->text_in_idx(stats, TileRect(x + w - sw, y, sw, bd), Mui_font_role::small, dim, bg, Mui_align::left);
					}
					y += bd;
				}
			}
			// What is running now (saves do not record the build that wrote them).
			y += gap;
			auto block = [&](const std::string& label, const std::string& value) {
				if (out) {
					out->text_in_idx(label, TileRect(x, y, w, sm), Mui_font_role::small, dim, bg, Mui_align::left);
					out->text_in_idx(value, TileRect(x, y + sm, w, bd), Mui_font_role::body, ink, bg, Mui_align::left);
				}
				y += sm + bd + p.px(2);
			};
			block(Strings::Game(), game_name());
			if (!Game::get_modtitle().empty()) {
				block(Strings::Mod(), one_line(Game::get_modtitle()));
			}
			block(Strings::RunningBuild(), running_build());
			y += pad;
			return y - (out ? rect.y : 0);
		}
	};

	// Fixed-width dropdown for the sort order.
	class Sort_dropdown : public Mui_dropdown {
	public:
		using Mui_dropdown::Mui_dropdown;

		void measure(Mui_painter& p, int avail_w, int& w, int& h) override {
			Mui_dropdown::measure(p, avail_w, w, h);
			w = std::min(std::max(w, p.px(220)), avail_w);
		}
	};
}    // namespace

/*
 *  Window.
 */

Mui_journey_window::Mui_journey_window() : Mui_window(Strings::JourneyLog()) {
	gwin->get_tqueue()->pause(SDL_GetTicks());
	std::string v;
	config->value(view_key, v, "grid");
	list_view = v == "list";
	config->value(sort_key, v, "newest");
	for (int i = 0; i < int(std::size(sort_keys)); i++) {
		if (v == sort_keys[i]) {
			sort = Sort(i);
		}
	}
	load_model();
	make_cards();
	sel_slot = default_slot();
	rebuild(list_view ? "journey.list" : "journey.grid");
}

Mui_journey_window::~Mui_journey_window() {
	gwin->get_tqueue()->resume(SDL_GetTicks());
}

void Mui_journey_window::preferred_size(float& w, float& h) const {
	w = 1180;
	h = 700;
}

std::string Mui_journey_window::default_help() const {
	return Strings::DefaultHelp();
}

/*
 *  Model: the same enumeration and calls as Newfile_gump::LoadSaveGameDetails.
 */

void Mui_journey_window::load_model() {
	cards.clear();
	saves.clear();
	quick.reset();
	current.reset();

	// Gamedat details (the quick save).
	{
		auto              q = std::make_unique<Save>();
		SaveGame_Details* d = nullptr;
		SaveGame_Party*   p = nullptr;
		gwin->get_saveinfo(q->shot, d, p);
		q->details.reset(d);
		q->party.reset(p);
		q->kind = Save::Kind::quick;
		q->name = Strings::QuickSave();
		if (q->details) {
			quick = std::move(q);
		}
	}

	// The current game.
	current       = std::make_unique<Save>();
	current->kind = Save::Kind::current;
	current->name = Strings::NewSave();
	current->shot = gwin->create_mini_screenshot();
	auto cd       = std::make_unique<SaveGame_Details>();
	std::memset(cd.get(), 0, sizeof(SaveGame_Details));
	cd->save_count   = quick ? quick->details->save_count : 0;
	cd->party_size   = char(partyman->get_count() + 1);
	cd->game_day     = static_cast<short>(gclock->get_total_hours() / 24);
	cd->game_hour    = char(gclock->get_hour());
	cd->game_minute  = char(gclock->get_minute());
	cd->play_seconds = gwin->get_play_seconds();
	const time_t t   = time(nullptr);
	tm           ti{};
	if (local_time(t, ti)) {
		cd->real_day    = char(ti.tm_mday);
		cd->real_hour   = char(ti.tm_hour);
		cd->real_minute = char(ti.tm_min);
		cd->real_month  = char(ti.tm_mon + 1);
		cd->real_year   = short(ti.tm_year + 1900);
		cd->real_second = char(ti.tm_sec);
	}
	const int n    = std::clamp(int(cd->party_size), 0, 8);
	current->party = std::make_unique<SaveGame_Party[]>(size_t(std::max(1, n)));
	for (int i = 0; i < n; i++) {
		Actor* npc = i == 0 ? gwin->get_main_actor() : gwin->get_npc(partyman->get_member(i - 1));
		if (!npc) {
			continue;
		}
		SaveGame_Party& m       = current->party[i];
		std::string     namestr = npc->get_npc_name();
		namestr.resize(sizeof(m.name), '\0');
		std::memcpy(m.name, namestr.data(), sizeof(m.name));
		m.shape      = short(npc->get_shapenum());
		m.shape_file = short(npc->get_shapefile());
		m.exp        = unsigned(npc->get_property(Actor::exp));
		m.health     = short(npc->get_property(Actor::health));
	}
	current->details = std::move(cd);

	// Savegame files.
	char mask[256];
	snprintf(mask, sizeof(mask), SAVENAME2, GAME_BG ? "bg" : GAME_SI ? "si" : "dev");
	FileList filenames;
	U7ListFiles(mask, filenames, true);
	for (const auto& f : filenames) {
		auto s      = std::make_unique<Save>();
		s->filename = f;
		// Sequence number: the last run of digits (SaveInfo::SetSeqNumber).
		size_t e = f.size();
		while (e > 0 && !std::isdigit(static_cast<unsigned char>(f[e - 1]))) {
			e--;
		}
		size_t b = e;
		while (b > 0 && std::isdigit(static_cast<unsigned char>(f[b - 1]))) {
			b--;
		}
		s->num = b < e ? std::atoi(f.substr(b, e - b).c_str()) : 0;
		saves.push_back(std::move(s));
	}
	std::sort(saves.begin(), saves.end(), [](const auto& a, const auto& b) {
		return a->num < b->num;
	});
	first_free = -1;
	for (size_t i = 0; i < saves.size(); i++) {
		Save&             s    = *saves[i];
		char*             name = nullptr;
		SaveGame_Details* d    = nullptr;
		SaveGame_Party*   p    = nullptr;
		try {
			s.readable = gwin->get_saveinfo(s.num, name, s.shot, d, p);
		} catch (const exult_exception& e) {
			std::cerr << "Journey Log: " << e.what() << std::endl;
			s.readable = false;
		}
		if (name) {
			s.name = name;
			delete[] name;
		}
		s.details.reset(d);
		s.party.reset(p);
		if (first_free == -1 && int(i) != s.num) {
			first_free = int(i);
		}
	}
	if (first_free == -1) {
		first_free = int(saves.size());
	}
}

void Mui_journey_window::make_cards() {
	cards.clear();
	cards.push_back(current.get());
	if (quick) {
		cards.push_back(quick.get());
	}
	std::vector<Save*> shown;
	const std::string  needle = lower(search);
	for (auto& s : saves) {
		if (needle.empty() || lower(s->name).find(needle) != std::string::npos) {
			shown.push_back(s.get());
		}
	}
	std::stable_sort(shown.begin(), shown.end(), [this](const Save* a, const Save* b) {
		switch (sort) {
		case Sort::oldest:
			if (a->details && b->details) {
				return compare_dates(*a, *b) > 0;
			}
			return compare_dates(*a, *b) < 0;
		case Sort::name: {
			const std::string x = lower(a->name);
			const std::string y = lower(b->name);
			return x != y ? x < y : a->num < b->num;
		}
		case Sort::day:
			return compare_days(*a, *b) < 0;
		case Sort::newest:
		default:
			return compare_dates(*a, *b) < 0;
		}
	});
	cards.insert(cards.end(), shown.begin(), shown.end());
}

int Mui_journey_window::default_slot() const {
	// The first save shown (the newest by default), else the New save card.
	for (const Save* c : cards) {
		if (c->kind == Save::Kind::slot) {
			return c->num;
		}
	}
	return -2;
}

int Mui_journey_window::card_index(int slot) const {
	for (size_t i = 0; i < cards.size(); i++) {
		if (cards[i]->slot_key() == slot) {
			return int(i);
		}
	}
	return -1;
}

const Mui_journey_window::Save* Mui_journey_window::card(int index) const {
	return index >= 0 && index < int(cards.size()) ? cards[size_t(index)] : nullptr;
}

const Mui_journey_window::Save* Mui_journey_window::selected_save() const {
	return card(card_index(sel_slot));
}

int Mui_journey_window::saves_count() const {
	return int(saves.size());
}

int Mui_journey_window::visible_count() const {
	int n = 0;
	for (const Save* c : cards) {
		n += c->kind == Save::Kind::slot ? 1 : 0;
	}
	return n;
}

std::string Mui_journey_window::visible_name(int index) const {
	int n = 0;
	for (const Save* c : cards) {
		if (c->kind == Save::Kind::slot && n++ == index) {
			return c->name;
		}
	}
	return "<none>";
}

bool Mui_journey_window::select_by_name(const std::string& name) {
	for (size_t i = 0; i < cards.size(); i++) {
		if (cards[i]->kind == Save::Kind::slot && cards[i]->name == name) {
			select_card(int(i));
			if (auto* g = dynamic_cast<Mui_grid*>(find("journey.grid"))) {
				g->set_selected(int(i), false);
			}
			if (auto* l = dynamic_cast<Mui_list*>(find("journey.list"))) {
				l->set_selected(int(i), false);
			}
			return true;
		}
	}
	return false;
}

void Mui_journey_window::select_card(int index) {
	const Save* s = card(index);
	if (!s) {
		return;
	}
	sel_slot = s->slot_key();
	update_buttons();
	relayout();    // The details panel changed size.
	set_dirty();
}

void Mui_journey_window::update_buttons() {
	const Save* s    = selected_save();
	const bool  slot = s && s->kind == Save::Kind::slot;
	if (load_button) {
		load_button->enabled = s && s->kind != Save::Kind::current && s->readable;
	}
	if (save_button) {
		save_button->enabled = s != nullptr;
	}
	if (rename_button) {
		rename_button->enabled = slot;
	}
	if (delete_button) {
		delete_button->enabled = slot;
	}
}

void Mui_journey_window::reload(int keep_slot) {
	load_model();
	make_cards();
	sel_slot = card_index(keep_slot) >= 0 ? keep_slot : default_slot();
	rebuild();
}

/*
 *  Widgets.
 */

void Mui_journey_window::build_title(Mui_box& bar) {
	auto* field        = bar.add<Mui_text_field>("journey.search", search, Strings::SearchSaves());
	field->search_icon = true;
	field->stretch     = true;
	field->help        = Strings::SearchHelp();
	field->on_change   = [this](const std::string& s) {
        post([this, s] {
            search = s;
            make_cards();
            if (card_index(sel_slot) < 0) {
                sel_slot = default_slot();
            }
            rebuild("journey.search");
        });
	};
	field->on_submit = [this](const std::string&) {
		post([this] {
			set_focus(find(list_view ? "journey.list" : "journey.grid"));
		});
	};
}

void Mui_journey_window::build(Mui_box& body) {
	body.gap = 10;
	// Header: count, sort order and view.
	auto*     head  = body.add<Mui_box>(Mui_box::Dir::horizontal, "journey.head");
	const int n     = visible_count();
	auto*     count = head->add<Mui_label>(
            std::to_string(n) + " " + (n == 1 ? Strings::SaveWord() : Strings::SavesWord()), Mui_font_role::body,
            Mui_colour::text_dim);
	count->id      = "journey.count";
	count->stretch = true;
	std::vector<std::string> sorts;
	for (int i = 0; i < int(std::size(sort_keys)); i++) {
		sorts.emplace_back(Strings::SortName(i));
	}
	auto* sd   = head->add<Sort_dropdown>("journey.sort", sorts, int(sort), [this](int i) {
        post([this, i] {
            sort = Sort(i);
            config->set(sort_key, sort_keys[i], true);
            make_cards();
            rebuild("journey.sort");
        });
    });
	sd->help   = Strings::SortHelp();
	auto* view = head->add<Mui_segmented>(
			"journey.view", std::vector<std::string>{Strings::Cards(), Strings::List()}, list_view ? 1 : 0, [this](int i) {
				post([this, i] {
					list_view = i == 1;
					config->set(view_key, list_view ? "list" : "grid", true);
					rebuild(list_view ? "journey.list" : "journey.grid");
				});
			});
	view->keys = {"grid", "list"};
	view->help = Strings::ViewHelp();

	auto* cols          = body.add<Columns>();
	cols->gap           = 16;
	auto*     left      = cols->add<Mui_box>(Mui_box::Dir::vertical, "journey.saves");
	const int sel       = card_index(sel_slot);
	auto      on_select = [this](int i) {
        post([this, i] {
            select_card(i);
        });
	};
	auto on_activate = [this](int i) {
		post([this, i] {
			select_card(i);
			const Save* s = card(i);
			if (s && s->kind == Save::Kind::current) {
				new_save();
			} else if (s && s->readable) {
				load_selected();
			}
		});
	};
	if (list_view) {
		std::vector<std::string> rows;
		for (const Save* c : cards) {
			rows.push_back(card_title(c));
		}
		auto* list         = left->add<Mui_list>("journey.list", rows, sel);
		list->row_ref      = row_h_ref;
		list->visible_rows = std::max(1, int(rows.size()));
		list->on_select    = on_select;
		list->on_activate  = on_activate;
		list->paint_row    = [this, list](Mui_painter& p, int i, const TileRect& r, bool is_sel) {
            const Save* s = card(i);
            if (!s) {
                return;
            }
            const uint8 bg  = is_sel ? p.col(Mui_colour::accent) : list->surface(p);
            const uint8 ink = p.col(is_sel ? Mui_colour::text_on_accent : Mui_colour::text);
            const uint8 dim = is_sel ? ink : p.col(Mui_colour::text_dim);
            const int   pad = p.px(6);
            TileRect    tr(r.x + pad, r.y + pad, p.px(row_h_ref * 1.6f), r.h - 2 * pad);
            paint_thumb(p, s, tr, 2);
            const int x   = tr.x + tr.w + p.px(12);
            const int w   = r.x + r.w - pad - x;
            const int bd  = p.line_height(Mui_font_role::body);
            const int sm  = p.line_height(Mui_font_role::small);
            const int top = r.y + (r.h - bd - sm) / 2;
            const int c1  = w * 45 / 100;
            const int c2  = w * 30 / 100;
            p.text_in_idx(card_title(s), TileRect(x, top, c1 - pad, bd), Mui_font_role::body, ink, bg, Mui_align::left);
            if (s->kind == Save::Kind::current) {
                p.text_in_idx(Strings::NewSaveHint(), TileRect(x, top + bd, w, sm), Mui_font_role::small, dim, bg, Mui_align::left);
            } else {
                p.text_in_idx(s->file_base(), TileRect(x, top + bd, c1 - pad, sm), Mui_font_role::small, dim, bg, Mui_align::left);
            }
            if (!s->readable) {
                paint_badge(p, Strings::Unreadable(), x + c1, top + (bd + sm - p.line_height(Mui_font_role::small)) / 2);
                return;
            }
            const SaveGame_Details* d = s->details.get();
            p.text_in_idx(day_time(d), TileRect(x + c1, top, c2 - pad, bd), Mui_font_role::body, ink, bg, Mui_align::left);
            paint_value(p, play_time(d), TileRect(x + c1, top + bd, c2 - pad, sm), Mui_font_role::small, dim, bg);
            if (s->kind != Save::Kind::current) {
                p.text_in_idx(
                        saved_at(d), TileRect(x + c1 + c2, top, w - c1 - c2, bd), Mui_font_role::small, dim, bg, Mui_align::left);
            }
		};
	} else {
		auto* grid        = left->add<Mui_grid>("journey.grid", int(cards.size()), card_w_ref, card_h_ref);
		grid->selected    = sel;
		grid->on_select   = on_select;
		grid->on_activate = on_activate;
		grid->paint_card  = [this](Mui_painter& p, int i, const TileRect& r, bool is_sel) {
            const Save* s = card(i);
            if (!s) {
                return;
            }
            const bool  st   = p.get_theme().bevelled();
            const uint8 fill = p.col(is_sel ? Mui_colour::panel : Mui_colour::panel_alt);
            if (st) {
                p.fill_rect(r, fill);
                p.bevel(r, std::max(1, p.px(2)), p.col(Mui_colour::frame_light), p.col(Mui_colour::frame_dark));
            } else {
                p.rounded_fill(r, p.px(p.get_theme().metrics.radius), fill);
                p.rounded_frame(r, p.px(p.get_theme().metrics.radius), std::max(1, p.px(1)), p.col(Mui_colour::frame));
            }
            if (is_sel) {
                TileRect o = r;
                if (st) {
                    o.enlarge(-std::max(1, p.px(2)));
                    p.frame_rect(o, std::max(2, p.px(2)), p.col(Mui_colour::accent));
                } else {
                    p.rounded_frame(o, p.px(p.get_theme().metrics.radius), std::max(2, p.px(2)), p.col(Mui_colour::accent));
                }
            }
            const uint8 ink = p.col(Mui_colour::text);
            const uint8 dim = p.col(Mui_colour::text_dim);
            const int   pad = p.px(8);
            TileRect    tr(r.x + pad, r.y + pad, r.w - 2 * pad, p.px(thumb_h_ref));
            paint_thumb(p, s, tr);
            int       y  = tr.y + tr.h + p.px(6);
            const int w  = r.w - 2 * pad;
            const int bd = p.line_height(Mui_font_role::body);
            const int sm = p.line_height(Mui_font_role::small);
            p.text_in_idx(card_title(s), TileRect(r.x + pad, y, w, bd), Mui_font_role::body, ink, fill, Mui_align::left);
            y += bd;
            if (s->kind == Save::Kind::current) {
                p.text_in_idx(
                        Strings::NewSaveHint(), TileRect(r.x + pad, y, w, sm), Mui_font_role::small, dim, fill, Mui_align::left);
                return;
            }
            if (!s->readable) {
                paint_badge(p, Strings::Unreadable(), r.x + pad, y + p.px(2));
                return;
            }
            const SaveGame_Details* d = s->details.get();
            p.text_in_idx(day_time(d), TileRect(r.x + pad, y, w, sm), Mui_font_role::small, dim, fill, Mui_align::left);
            y += sm;
            const std::string label = std::string(Strings::PlayTime()) + " ";
            const int         lw    = p.text_width(label, Mui_font_role::small);
            p.text_in_idx(label, TileRect(r.x + pad, y, lw, sm), Mui_font_role::small, dim, fill, Mui_align::left);
            paint_value(p, play_time(d), TileRect(r.x + pad + lw, y, w - lw, sm), Mui_font_role::small, dim, fill);
		};
	}
	if (visible_count() == 0 && !search.empty()) {
		left->add<Mui_label>(Strings::NoMatches(), Mui_font_role::body, Mui_colour::text_dim)->id = "journey.nomatch";
	}
	auto* note = left->add<Mui_label>(Strings::PlayTimeNote(), Mui_font_role::small, Mui_colour::text_dim, true);
	note->id   = "journey.note";
	cols->add<Details>(*this);
}

void Mui_journey_window::build_footer(Mui_box& footer) {
	delete_button       = footer.add<Mui_button>("journey.delete", Strings::Delete(), Mui_button::Style::destructive, [this] {
        post([this] {
            delete_selected();
        });
    });
	delete_button->help = Strings::DeleteHelp();
	rename_button       = footer.add<Mui_button>("journey.rename", Strings::Rename(), Mui_button::Style::secondary, [this] {
        post([this] {
            rename_selected();
        });
    });
	rename_button->help = Strings::RenameHelp();
	save_button         = footer.add<Mui_button>("journey.save", Strings::SaveHere(), Mui_button::Style::secondary, [this] {
        post([this] {
            save_selected();
        });
    });
	save_button->help   = Strings::SaveHelp();
	load_button         = footer.add<Mui_button>("journey.load", Strings::Load(), Mui_button::Style::primary, [this] {
        post([this] {
            load_selected();
        });
    });
	load_button->help   = Strings::LoadHelp();
	update_buttons();
}

bool Mui_journey_window::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	Mui_widget* f = get_focus();
	if (!has_overlay() && chr == SDLK_F2) {
		// F2 renames even while the search field has the focus.
		post([this] {
			rename_selected();
		});
		set_dirty();
		return true;
	}
	if (!has_overlay() && !(f && f->wants_text()) && chr != SDLK_UNKNOWN) {
		switch (chr) {
		case SDLK_DELETE:
			post([this] {
				delete_selected();
			});
			set_dirty();
			return true;
		case SDLK_SLASH:
			post([this] {
				set_focus(find("journey.search"));
			});
			set_dirty();
			return true;
		default:
			break;
		}
	}
	return Mui_window::key_down(chr, unicode);
}

/*
 *  Actions (always run from post(), outside widget callbacks).
 */

void Mui_journey_window::load_selected() {
	const Save* s = selected_save();
	if (!s || s->kind == Save::Kind::current || !s->readable) {
		return;
	}
	// As Newfile_gump::load(): restore the slot (the quick save is gamedat
	// itself), read gamedat, done.
	if (s->kind == Save::Kind::slot) {
		gwin->restore_gamedat(s->num);
	}
	gwin->read();
	restored = true;
	std::cout << "Journey Log: restored " << (s->kind == Save::Kind::slot ? s->file_base() : std::string("gamedat")) << std::endl;
	finish();
}

void Mui_journey_window::write_save(int num, const std::string& name) {
	try {
		// Write to gamedat, then to the savegame file (Newfile_gump::save()).
		gwin->write();
		if (num >= 0) {
			gwin->save_gamedat(num, name.c_str());
		}
	} catch (const exult_exception& e) {
		std::cerr << "Journey Log: " << e.what() << std::endl;
		show_error(Strings::SaveFailed());
		return;
	}
	std::cout << "Saved game #" << num << " successfully." << std::endl;
	gwin->got_bad_feeling(4);
	reload(num >= 0 ? num : -1);
}

void Mui_journey_window::save_selected() {
	const Save* s = selected_save();
	if (!s) {
		return;
	}
	if (s->kind == Save::Kind::current) {
		new_save();
		return;
	}
	const int         num  = s->kind == Save::Kind::slot ? s->num : -1;
	const std::string name = s->name;
	show_confirm(Strings::Overwrite(), false, [this, num, name](bool ok) {
		if (ok) {
			write_save(num, name);
		}
	});
}

void Mui_journey_window::new_save() {
	show_prompt(Strings::NewSavePrompt(), "", [this](bool ok, const std::string& name) {
		if (!ok || name.empty()) {
			return;
		}
		write_save(first_free, name.substr(0, MAX_SAVEGAME_NAME_LEN - 1));
	});
}

void Mui_journey_window::rename_selected() {
	const Save* s = selected_save();
	if (!s || s->kind != Save::Kind::slot) {
		return;
	}
	const int num = s->num;
	show_prompt(Strings::RenamePrompt(), s->name, [this, num](bool ok, const std::string& name) {
		if (!ok || name.empty()) {
			return;
		}
		if (!gwin->rename_savegame(num, name.c_str())) {
			show_error(Strings::RenameFailed());
			return;
		}
		std::cout << "Renamed save game #" << num << " to \"" << name << "\"." << std::endl;
		reload(num);
	});
}

void Mui_journey_window::delete_selected() {
	const Save* s = selected_save();
	if (!s || s->kind != Save::Kind::slot) {
		return;
	}
	const std::string file = s->filename;
	show_confirm(Strings::DeleteConfirm(), true, [this, file](bool ok) {
		if (!ok) {
			return;
		}
		U7remove(file.c_str());
		std::cout << "Deleted save game (" << file << ") successfully." << std::endl;
		reload(-3);    // Select the first save left.
	});
}

void Mui_journey_window::show_error(const std::string& msg) {
	show_confirm(msg, false, [](bool) {});
}
