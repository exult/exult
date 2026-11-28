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

#include "Mui_options_model.h"

#include "AudioOptions_gump.h"
#include "Configuration.h"
#include "Enabled_button.h"
#include "GameDisplayOptions_gump.h"
#include "GameEngineOptions_gump.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "InputOptions_gump.h"
#include "Mui_router.h"
#include "Mui_theme.h"
#include "UIOptions_gump.h"
#include "VideoOptions_gump.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "items.h"

#include <algorithm>
#include <cctype>

namespace {
	// Classic toggle callbacks repaint their gump; while the model changes a
	// value that paint goes to this off-screen buffer and is thrown away.
	Image_buffer8* scratch_buffer() {
		static std::unique_ptr<Image_buffer> scratch;
		static int                           scratch_w = 0;
		static int                           scratch_h = 0;
		Game_window*                         gwin      = Game_window::get_instance();
		const int                            w         = std::max(1, gwin->get_width());
		const int                            h         = std::max(1, gwin->get_height());
		if (!scratch || scratch_w != w || scratch_h != h) {
			scratch   = gwin->get_win()->create_buffer(w, h);
			scratch_w = w;
			scratch_h = h;
		}
		return static_cast<Image_buffer8*>(scratch.get());
	}

	std::string msg(unsigned id) {
		const char* s = id ? get_text_msg(id - unsigned(msg_file_start)) : nullptr;
		return s ? s : "";
	}

	// Classic labels end in ':' and sub-settings start with spaces.
	std::string clean_label(const std::string& s) {
		size_t b = s.find_first_not_of(' ');
		if (b == std::string::npos) {
			return {};
		}
		size_t e = s.find_last_not_of(" :");
		if (e == std::string::npos || e < b) {
			return {};
		}
		std::string out = s.substr(b, e - b + 1);
		out[0]          = char(std::toupper(static_cast<unsigned char>(out[0])));
		return out;
	}

	bool is_indented(unsigned id) {
		const std::string s = msg(id);
		return !s.empty() && s[0] == ' ';
	}
}    // namespace

Mui_options_model::Mui_options_model(Tab t) : which(t), table(build_table(t)) {
	create();
	refresh();
	mark_baseline();
}

Mui_options_model::~Mui_options_model() = default;

const char* Mui_options_model::tab_key(Tab t) {
	static const char* keys[] = {"video", "audio", "engine", "display", "input", "ui"};
	const int          i      = int(t);
	return i >= 0 && i < int(Tab::count) ? keys[i] : "";
}

/*
 *  The rows of each page, in the order of the classic screen. Labels are
 *  the exultmsg ids the classic paint() uses; keys are the config keys the
 *  classic save_settings() writes.
 */

std::vector<Mui_options_model::Row> Mui_options_model::build_table(Tab t) {
	using R  = Row;
	auto row = [](const char* id, int button, unsigned label, unsigned help, const char* keys, Apply apply = Apply::now,
				  Kind kind = Kind::choice, bool pending = false) {
		R r;
		r.id      = id;
		r.button  = button;
		r.label   = label;
		r.help    = help;
		r.keys    = keys;
		r.apply   = apply;
		r.kind    = kind;
		r.pending = pending;
		r.indent  = label && is_indented(label);
		return r;
	};
	std::vector<R> rows;
	switch (t) {
	case Tab::video: {
		using G = VideoOptions_gump;
		rows    = {
                row("fullscreen", G::id_fullscreen, 0x68C, 0xC40, "config/video/fullscreen"),
                row("resolution", G::id_resolution, 0x68E, 0xC41,
					   "config/video/display/width config/video/display/height config/video/window/display/width "
						  "config/video/window/display/height"),
                row("scaler", G::id_scaler, 0x690, 0xC42, "config/video/scale_method"),
                row("scaling", G::id_scaling, 0x691, 0xC43, "config/video/scale"),
                row("game_area", G::id_game_resolution, 0x692, 0xC44, "config/video/game/width config/video/game/height"),
                row("fill_quality", G::id_fill_scaler, 0x693, 0xC45, "config/video/fill_scaler"),
                row("fill_mode", G::id_fill_mode, 0x694, 0xC46, "config/video/fill_mode"),
                row("aspect", G::id_has_ac, 0x695, 0xC47, "config/video/fill_mode"),
                row("share", G::id_share_settings, 0x696, 0xC48, "config/video/share_video_settings"),
        };
		break;
	}
	case Tab::audio: {
		using G = AudioOptions_gump;
		rows    = {
                row("mixer", G::id_mixer, 0xC31, 0xC49, "config/audio/effects/sfx_volume config/audio/speech/speech_volume",
					   Apply::now, Kind::action),
                row("audio", G::id_audio_enabled, 0x614, 0xC4A, "config/audio/enabled"),
                row("sample_rate", G::id_sample_rate, 0x615, 0xC4B, "config/audio/sample_rate"),
                row("speaker", G::id_speaker_type, 0x616, 0xC4C, "config/audio/stereo"),
                row("music", G::id_music_enabled, 0x617, 0xC4D, "config/audio/midi/enabled"),
                row("looping", G::id_music_looping, 0x618, 0xC4E, "config/audio/midi/looping"),
                row("digital_music", G::id_music_digital, 0x619, 0xC4F, "config/audio/midi/use_oggs"),
                row("midi_driver", G::id_midi_driver, 0x61A, 0xC50, "config/audio/midi/driver"),
                row("midi_settings", G::id_advanced, 0xC32, 0xC51, "config/audio/midi", Apply::now, Kind::action),
                row("sfx", G::id_sfx_enabled, 0x61B, 0xC52, "config/audio/effects/enabled config/audio/effects/midi"),
                row("sfx_pack", G::id_sfx_pack, 0x61C, 0xC53, "config/disk/game/*/waves config/audio/effects/convert"),
                row("speech", G::id_speech_enabled, 0x61D, 0xC54, "config/audio/speech/enabled config/audio/speech/with_subs"),
        };
		// The two action rows belong to the rows above them.
		rows[8].indent = true;
		break;
	}
	case Tab::engine: {
		using G = GameEngineOptions_gump;
		rows    = {
                row("autonotes", G::id_allow_autonotes, 0x62A, 0xC55, "config/gameplay/allow_autonotes"),
                row("gumps_pause", G::id_gumps_pause, 0x62B, 0xC56, "config/gameplay/gumps_dont_pause_game"),
                row("alternate_drop", G::id_alternate_drop, 0x62C, 0xC57, "config/gameplay/alternate_drop"),
                row("speed", G::id_frames, 0x62D, 0xC58, "config/video/fps"),
                row("show_hits", G::id_show_hits, 0x62E, 0xC59, "config/gameplay/combat/show_hits"),
                row("combat_mode", G::id_mode, 0x62F, 0xC5A, "config/gameplay/combat/mode"),
                row("charmed", G::id_charmDiff, 0x630, 0xC5B, "config/gameplay/combat/charmDifficulty"),
                row("difficulty", G::id_difficulty, 0x631, 0xC5C, "config/gameplay/combat/difficulty"),
                row("enhancements", G::id_enhancements, 0x634, 0xC5D, "config/gameplay/enhancements"),
                row("cheats", G::id_cheats, 0x632, 0xC5E, "config/gameplay/cheat"),
                row("feeding", G::id_feeding, 0x633, 0xC5F, "config/gameplay/feeding"),
        };
		rows.back().indent = true;
		break;
	}
	case Tab::display: {
		using G = GameDisplayOptions_gump;
		rows    = {
                row("status_bars", G::id_facestats, 0x5C8, 0xC60, "config/gameplay/facestats"),
                row("shortcut_bar", G::id_sc_enabled, 0x5C9, 0xC61, "config/shortcutbar/use_shortcutbar"),
                row("outline", G::id_sc_outline, 0x5CA, 0xC62, "config/shortcutbar/use_outline_color"),
                row("hide_missing", G::id_sb_hide_missing, 0x5CB, 0xC63, "config/shortcutbar/hide_missing_items"),
                row("text_bg", G::id_text_bg, 0x5CC, 0xC64, "config/gameplay/textbackground"),
                row("smooth_scrolling", G::id_smooth_scrolling, 0x5CD, 0xC65, "config/gameplay/smooth_scrolling"),
                row("skip_intro", G::id_menu_intro, 0x5CE, 0xC66, "config/gameplay/skip_splash", Apply::restart),
                row("skip_first_scene", G::id_usecode_intro, 0x5CF, 0xC67, "config/gameplay/skip_intro", Apply::restart),
                row("extended_intro", G::id_extended_intro, 0x5D0, 0xC68, "config/gameplay/extended_intro", Apply::restart),
                row("paperdolls", G::id_paperdolls, 0x5D1, 0xC69, "config/gameplay/bg_paperdolls"),
                row("android_autolaunch", G::id_android_autolaunch, 0x5D2, 0xC6A, "", Apply::restart),
                row("language", G::id_language, 0x5D3, 0xC6B, "config/gameplay/language"),
                row("fonts", G::id_fonts, 0x5D8, 0xC6C, "config/gameplay/fonts"),
        };
		break;
	}
	case Tab::input: {
		using G = InputOptions_gump;
		rows    = {
                row("doubleclick", G::id_doubleclick, 0x647, 0xC6D, "config/gameplay/double_click_closes_gumps"),
                row("rightclick_close", G::id_rightclick_close, 0x648, 0xC6E, "config/gameplay/right_click_closes_gumps"),
                row("right_pathfind", G::id_right_pathfind, 0x649, 0xC6F, "config/gameplay/allow_right_pathfind"),
                row("scroll_mouse", G::id_scroll_mouse, 0x64A, 0xC70, "config/gameplay/scroll_with_mouse"),
                row("mouse3rd", G::id_mouse3rd, 0x64B, 0xC71, "config/gameplay/mouse3rd"),
                row("fastmouse", G::id_fastmouse, 0x64C, 0xC72, "config/gameplay/fastmouse"),
                row("item_menu", G::id_item_menu, 0x64D, 0xC73, "config/touch/item_menu"),
                row("dpad", G::id_dpad_location, 0x64E, 0xC74, "config/touch/dpad_location"),
                row("touch_pathfind", G::id_touch_pathfind, 0x64F, 0xC75, "config/touch/touch_pathfind"),
        };
		break;
	}
	case Tab::ui: {
		using G = UIOptions_gump;
		rows    = {
                row("menu_style", -1, 0xD22, 0xD23, "config/gameplay/menu_style"),
                row("menu_scale", -1, 0xD28, 0xD29, "config/gameplay/menu_scale"),
                row("size", G::id_size, 0xB00, 0xC76, "config/video/ui/width config/video/ui/height"),
                row("scale_method", G::id_scale_method, 0xB01, 0xC77, "config/video/ui/scale_method"),
                row("fill_mode", G::id_fill_mode, 0xB02, 0xC78, "config/video/ui/fill_mode"),
                row("fill_scaler", G::id_fill_scaler, 0xB03, 0xC79, "config/video/ui/fill_scaler"),
                row("palette", G::id_palette, 0xB10, 0xC7A, "config/video/ui/palette"),
                row("universal", G::id_universal, 0xB04, 0xC7B, "config/video/ui/universal"),
                row("layer_mouse", G::id_layer_mouse, 0xB09, 0xC7C, "config/video/ui/mouse_pointer", Apply::now, Kind::action,
					   true),
                row("layer_conversations", G::id_layer_conversations, 0xB0A, 0xC7C, "config/video/ui/conversations", Apply::now,
					   Kind::action, true),
                row("layer_gumps", G::id_layer_gumps, 0xB0B, 0xC7C, "config/video/ui/gumps", Apply::now, Kind::action, true),
                row("layer_hud_gumps", G::id_layer_hud_gumps, 0xB0C, 0xC7C, "config/video/ui/hud_gumps", Apply::now, Kind::action,
					   true),
                row("layer_text_gumps", G::id_layer_text_gumps, 0xB0D, 0xC7C, "config/video/ui/text_gumps", Apply::now,
					   Kind::action, true),
                row("layer_modal_gumps", G::id_layer_modal_gumps, 0xB0E, 0xC7C, "config/video/ui/modal_gumps", Apply::now,
					   Kind::action, true),
                row("layer_text_effects", G::id_layer_text_effect, 0xB0F, 0xC7C, "config/video/ui/text_effects", Apply::now,
					   Kind::action, true),
        };
		rows[0].native_key = rows[0].keys;
		rows[1].native_key = rows[1].keys;
		for (size_t i = 8; i < rows.size(); i++) {
			rows[i].indent = true;
		}
		break;
	}
	default:
		break;
	}
	return rows;
}

/*
 *  The classic gump.
 */

void Mui_options_model::create() {
	switch (which) {
	case Tab::video: {
		auto g = std::make_unique<VideoOptions_gump>();
		add_game_areas(*g);
		gump = std::move(g);
		break;
	}
	case Tab::audio:
		gump = std::make_unique<AudioOptions_gump>();
		break;
	case Tab::engine:
		gump = std::make_unique<GameEngineOptions_gump>();
		break;
	case Tab::display:
		gump = std::make_unique<GameDisplayOptions_gump>();
		break;
	case Tab::input:
		gump = std::make_unique<InputOptions_gump>();
		break;
	case Tab::ui:
		gump = std::make_unique<UIOptions_gump>();
		break;
	default:
		break;
	}
	load_native();
}

/*
 *  The settings of the themed menus (UI page): menu style and size.
 */

void Mui_options_model::load_native() {
	native_value.clear();
	for (auto& r : table) {
		if (r.native_key.empty()) {
			continue;
		}
		std::string cur;
		r.choices.clear();
		r.native_values.clear();
		if (r.id == "menu_style") {
			cur             = Mui::menu_style();
			r.native_values = {"classic", "ultima", "modern"};
			r.choices       = {msg(0xC05), msg(0xC01), msg(0xC02)};
			// Theme files of the game or mod.
			for (const auto& name : Mui_theme::list()) {
				if (std::find(r.native_values.cbegin(), r.native_values.cend(), name) != r.native_values.cend()) {
					continue;
				}
				const auto t = Mui_theme::load(name);
				if (t) {
					r.native_values.push_back(name);
					r.choices.push_back(t->label.empty() ? name : t->label);
				}
			}
		} else if (r.id == "menu_scale") {
			cur                   = std::to_string(Mui::menu_scale());
			std::vector<int> pcts = {75, 90, 100, 110, 125, 150};
			if (std::find(pcts.cbegin(), pcts.cend(), Mui::menu_scale()) == pcts.cend()) {
				pcts.push_back(Mui::menu_scale());
				std::sort(pcts.begin(), pcts.end());
			}
			for (const int pct : pcts) {
				r.native_values.push_back(std::to_string(pct));
				r.choices.push_back(std::to_string(pct) + "%");
			}
		}
		const auto it = std::find(r.native_values.cbegin(), r.native_values.cend(), cur);
		if (it == r.native_values.cend()) {
			// An unknown style: keep it as it is until another is chosen.
			r.native_values.push_back(cur);
			r.choices.push_back(cur);
			native_value[r.id] = int(r.native_values.size()) - 1;
		} else {
			native_value[r.id] = int(it - r.native_values.cbegin());
		}
	}
}

void Mui_options_model::save_native() {
	for (const auto& r : table) {
		if (r.native_key.empty() || !row_changed(r) || r.value < 0 || r.value >= int(r.native_values.size())) {
			continue;
		}
		config->set(r.native_key, r.native_values[size_t(r.value)], true);
	}
}

/*
 *  The classic Video gump offers Auto, 320x200 and a custom size from
 *  exult.cfg on desktops, plus 400x250 and 480x300 on phones and tablets.
 *  The themed page offers all of them on every platform, in a dropdown.
 */

void Mui_options_model::add_game_areas(VideoOptions_gump& g) {
	auto res = [](uint32 w, uint32 h) {
		return (w << 16) | h;
	};
	std::vector<uint32>& list = g.game_resolutions;
	for (const uint32 r : {res(320, 200), res(400, 250), res(480, 300)}) {
		if (std::find(list.cbegin(), list.cend(), r) == list.cend()) {
			list.push_back(r);
		}
	}
	// Auto first, then by size (a custom size falls into place).
	std::sort(list.begin(), list.end(), [](uint32 a, uint32 b) {
		if (a == 0 || b == 0) {
			return a == 0 && b != 0;
		}
		return (a >> 16) * (a & 0xffffu) < (b >> 16) * (b & 0xffffu);
	});
	g.rebuild_buttons();
}

Gump_button* Mui_options_model::button(int id) const {
	if (!gump || id < 0) {
		return nullptr;
	}
	Modal_gump* g = gump.get();
	switch (which) {
	case Tab::video:
		return static_cast<VideoOptions_gump*>(g)->buttons[size_t(id)].get();
	case Tab::audio:
		return static_cast<AudioOptions_gump*>(g)->buttons[size_t(id)].get();
	case Tab::engine:
		return static_cast<GameEngineOptions_gump*>(g)->buttons[size_t(id)].get();
	case Tab::display:
		return static_cast<GameDisplayOptions_gump*>(g)->buttons[size_t(id)].get();
	case Tab::input:
		return static_cast<InputOptions_gump*>(g)->buttons[size_t(id)].get();
	case Tab::ui:
		return static_cast<UIOptions_gump*>(g)->buttons[size_t(id)].get();
	default:
		return nullptr;
	}
}

void Mui_options_model::refresh() {
	for (auto& r : table) {
		if (!r.native_key.empty()) {
			r.visible = true;
			r.value   = native_value[r.id];
			continue;
		}
		Gump_button* b = button(r.button);
		r.visible      = b != nullptr;
		r.choices.clear();
		r.value = -1;
		if (!b || r.kind != Kind::choice) {
			continue;
		}
		if (auto* t = dynamic_cast<Gump_ToggleTextButton*>(b)) {
			r.choices = t->get_selections();
		} else if (dynamic_cast<Enabled_button*>(b)) {
			r.choices = {GumpStrings::Disabled(), GumpStrings::Enabled()};
		} else {
			// Not a choice button after all: show it as an action.
			r.kind = Kind::action;
			continue;
		}
		r.value = b->getselection();
		if (r.value < -1 || r.value >= int(r.choices.size())) {
			r.value = -1;
		}
	}
}

std::string Mui_options_model::signature() const {
	std::string s;
	for (const auto& r : table) {
		s += r.visible ? '1' : '0';
		for (const auto& c : r.choices) {
			s += c;
			s += '\x1f';
		}
		s += '\x1e';
	}
	return s;
}

void Mui_options_model::mark_baseline() {
	baseline.clear();
	for (const auto& r : table) {
		if (r.visible && r.kind == Kind::choice) {
			baseline[r.id] = r.value;
		}
	}
	touched = false;
}

/*
 *  Queries.
 */

const Mui_options_model::Row* Mui_options_model::find(const std::string& id) const {
	for (const auto& r : table) {
		if (r.id == id) {
			return &r;
		}
	}
	return nullptr;
}

std::string Mui_options_model::label(const Row& r) const {
	if (which == Tab::video && r.id == "resolution") {
		const auto* g = static_cast<const VideoOptions_gump*>(gump.get());
		return clean_label(msg(g && g->fullscreen ? 0x68D : 0x68E));
	}
	if (which == Tab::video && r.id == "share") {
		return clean_label(msg(0x696) + " " + msg(0x697));
	}
	if (which == Tab::ui && r.id == "universal") {
		return clean_label(msg(0xB04) + " " + msg(0xB05));
	}
	if (which == Tab::audio && r.id == "sfx_pack") {
		const auto* g = static_cast<const AudioOptions_gump*>(gump.get());
		return clean_label(msg(g && g->sfx_enabled == 1 && g->have_digital_sfx() ? 0x61C : 0x61E));
	}
	return clean_label(msg(r.label));
}

std::string Mui_options_model::help(const Row& r) const {
	if (r.id == "menu_style" && r.value >= 0 && r.value < int(r.native_values.size())) {
		// What the chosen style looks like.
		const std::string& v    = r.native_values[size_t(r.value)];
		const unsigned     desc = v == "classic" ? 0xD24 : v == "ultima" ? 0xD25 : v == "modern" ? 0xD26 : 0xD27;
		return msg(r.help) + " " + msg(desc);
	}
	return msg(r.help);
}

bool Mui_options_model::row_changed(const Row& r) const {
	const auto it = baseline.find(r.id);
	return r.visible && r.kind == Kind::choice && it != baseline.end() && it->second != r.value;
}

bool Mui_options_model::changed() const {
	if (touched) {
		return true;
	}
	for (const auto& r : table) {
		if (row_changed(r)) {
			return true;
		}
	}
	return false;
}

int Mui_options_model::baseline_value(const std::string& id) const {
	const auto it = baseline.find(id);
	return it == baseline.end() ? -1 : it->second;
}

/*
 *  Changes.
 */

bool Mui_options_model::set_value(const std::string& id, int index) {
	const Row* r = find(id);
	if (!r || r->kind != Kind::choice || index < 0 || index >= int(r->choices.size()) || index == r->value) {
		return false;
	}
	if (!r->native_key.empty()) {
		native_value[id] = index;
		refresh();
		return false;
	}
	Gump_button* b = button(r->button);
	if (!b) {
		return false;
	}
	const std::string before = signature();
	Game_window*      gwin   = Game_window::get_instance();
	Image_buffer8*    prev   = gwin->push_render_target(scratch_buffer());
	if (auto* t = dynamic_cast<Gump_ToggleTextButton*>(b)) {
		t->setselection(index);
		t->toggle(index);
	} else if (auto* e = dynamic_cast<Enabled_button*>(b)) {
		e->set_frame(index);
		e->toggle(index);
	}
	gwin->pop_render_target(prev);
	refresh();
	return signature() != before;
}

void Mui_options_model::activate(const std::string& id) {
	const Row* r = find(id);
	if (!r || r->kind != Kind::action) {
		return;
	}
	Gump_button* b = button(r->button);
	if (!b) {
		return;
	}
	const bool pending = r->pending;
	// The sub-screen is a classic modal gump shown normally.
	b->activate(Gump_Base::MouseButton::Left);
	Game_window::get_instance()->set_all_dirty();
	if (pending) {
		touched = true;
	}
	refresh();
}

void Mui_options_model::apply() {
	Modal_gump* g = gump.get();
	switch (which) {
	case Tab::video:
		static_cast<VideoOptions_gump*>(g)->save_settings();
		// The settings may have been kept or rolled back by the countdown:
		// start again from what is now in effect.
		gump.reset();
		create();
		break;
	case Tab::audio: {
		auto* audio = static_cast<AudioOptions_gump*>(g);
		audio->save_settings();
		if (audio->save_was_cancelled()) {
			// Nothing was written: keep the changes pending, as the
			// classic gump keeps them for its next Apply.
			refresh();
			return;
		}
		break;
	}
	case Tab::engine:
		static_cast<GameEngineOptions_gump*>(g)->save_settings();
		break;
	case Tab::display:
		static_cast<GameDisplayOptions_gump*>(g)->save_settings();
		break;
	case Tab::input:
		static_cast<InputOptions_gump*>(g)->save_settings();
		break;
	case Tab::ui:
		save_native();
		static_cast<UIOptions_gump*>(g)->save_settings();
		break;
	default:
		break;
	}
	Game_window::get_instance()->set_all_dirty();
	refresh();
	mark_baseline();
}

void Mui_options_model::revert() {
	// Destroy first: VideoOptions_gump restores the palette it forced.
	gump.reset();
	create();
	refresh();
	touched = false;
}
