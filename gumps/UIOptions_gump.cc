/*
 *  Copyright (C) 2026  The Exult Team
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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Configuration.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Text_button.h"
#include "UIOptions_gump.h"
#include "exult.h"
#include "font.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"
#include "palette.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using std::string;

namespace {
	class Strings : public GumpStrings {
	public:
		static auto Size_() {
			return get_text_msg(0xB00 - msg_file_start);
		}

		static auto Scalemethod_() {
			return get_text_msg(0xB01 - msg_file_start);
		}

		static auto Fillmode_() {
			return get_text_msg(0xB02 - msg_file_start);
		}

		static auto Fillscaler_() {
			return get_text_msg(0xB03 - msg_file_start);
		}

		static auto SameSettingsFor_() {
			return get_text_msg(0xB04 - msg_file_start);
		}

		static auto AllLayers_() {
			return get_text_msg(0xB05 - msg_file_start);
		}

		static auto Full320x200_() {
			return get_text_msg(0xB06 - msg_file_start);
		}

		static auto Medium420x263_() {
			return get_text_msg(0xB07 - msg_file_start);
		}

		static auto Auto0x0_() {
			return get_text_msg(0xB08 - msg_file_start);
		}

		static auto MousePointer_() {
			return get_text_msg(0xB09 - msg_file_start);
		}

		static auto Conversations_() {
			return get_text_msg(0xB0A - msg_file_start);
		}

		static auto Gumps_() {
			return get_text_msg(0xB0B - msg_file_start);
		}

		static const char* HudGumps_() {
			return get_text_msg(0xB0C - msg_file_start);
		}

		static auto TextGumps_() {
			return get_text_msg(0xB0D - msg_file_start);
		}

		static auto ModalGumps_() {
			return get_text_msg(0xB0E - msg_file_start);
		}

		static auto TextEffect_() {
			return get_text_msg(0xB0F - msg_file_start);
		}

		static auto AdvancedSettings_() {
			return get_text_msg(0x601 - msg_file_start);
		}

		static auto Palette_() {
			return get_text_msg(0xB10 - msg_file_start);
		}

		static auto Fill_() {
			return get_text_msg(0x683 - msg_file_start);
		}

		static auto Fit_() {
			return get_text_msg(0x684 - msg_file_start);
		}

		static auto Centre_() {
			return get_text_msg(0x685 - msg_file_start);
		}
	};

	using UIOptions_button = CallbackTextButton<UIOptions_gump>;
	using UIOptions_toggle = CallbackToggleTextButton<UIOptions_gump>;

	constexpr const char* help_url = "https://exult.info/docs.html#ui_options_gump";

	constexpr int row_size       = 0;
	constexpr int row_scale      = 1;
	constexpr int row_fill_mode  = 2;
	constexpr int row_fill_scl   = 3;
	constexpr int row_palette    = 4;
	constexpr int row_same       = 5;
	constexpr int row_universal  = 6;
	constexpr int row_layer_base = 7;
	constexpr int row_buttons    = 14;

	constexpr int yForLayerIndex(int idx) {
		return UIOptions_gump::yForRow(row_layer_base) + idx * 12;
	}

	constexpr std::array<Image_window::UiLayerKind, 7> kLayerKinds = {
			Image_window::UiLayerMousePointer, Image_window::UiLayerConversations, Image_window::UiLayerGumps,
			Image_window::UiLayerHudGumps,     Image_window::UiLayerTextGumps,     Image_window::UiLayerModalGumps,
			Image_window::UiLayerTextEffects,
	};

	constexpr std::array<const char*, 7> kLayerKeys = {
			"mouse_pointer", "conversations", "gumps", "hud_gumps", "text_gumps", "modal_gumps", "text_effects",
	};

	constexpr size_t kLayerCount = 7;

	const char* get_layer_name(size_t idx) {
		switch (idx) {
		case 0:
			return Strings::MousePointer_();
		case 1:
			return Strings::Conversations_();
		case 2:
			return Strings::Gumps_();
		case 3:
			return Strings::HudGumps_();
		case 4:
			return Strings::TextGumps_();
		case 5:
			return Strings::ModalGumps_();
		case 6:
			return Strings::TextEffect_();
		default:
			return "";
		}
	}

	static void normalize_dims(int& width, int& height) {
		if (width == 0 && height == 0) {
			return;
		}
		if (width <= 0 || height <= 0) {
			width  = 420;
			height = 263;
		}
	}

	static int size_to_selection(const UIOptions_gump::UiLayerSettings& cfg) {
		if (cfg.width == 0 && cfg.height == 0) {
			return 2;
		}
		if (cfg.width == 320 && cfg.height == 200) {
			return 0;
		}
		return 1;
	}

	static void apply_size_selection(UIOptions_gump::UiLayerSettings& cfg, int state) {
		if (state == 0) {
			cfg.width  = 320;
			cfg.height = 200;
		} else if (state == 2) {
			cfg.width  = 0;
			cfg.height = 0;
		} else {
			cfg.width  = 420;
			cfg.height = 263;
		}
	}

	static int fillmode_to_selection(Image_window::FillMode fmode) {
		if (fmode == Image_window::Fill) {
			return 0;
		}
		if (fmode == Image_window::Centre || fmode == Image_window::AspectCorrectCentre) {
			return 2;
		}
		return 1;
	}

	static Image_window::FillMode selection_to_fillmode(int state) {
		if (state == 0) {
			return Image_window::Fill;
		}
		if (state == 2) {
			return Image_window::Centre;
		}
		return Image_window::Fit;
	}

	static int fillscaler_to_selection(int fill_scaler) {
		if (fill_scaler == Image_window::SDLScaler) {
			return 2;
		}
		if (fill_scaler == Image_window::bilinear) {
			return 1;
		}
		return 0;
	}

	static int selection_to_fillscaler(int state) {
		if (state == 2) {
			return Image_window::SDLScaler;
		}
		if (state == 1) {
			return Image_window::bilinear;
		}
		return Image_window::point;
	}

	// Fixed-palette option. The list order below defines the toggle selection
	// index; each entry maps a UiPaletteMode to its config string and label
	// message id. Excludes PALETTE_NIGHT and PALETTE_DAWN (same as dusk).
	struct PaletteOption {
		int         mode;        // Image_window::UiPaletteMode.
		const char* cfg_name;    // Config string.
		int         msg_id;      // exultmsg.txt id for the label.
	};

	constexpr std::array<PaletteOption, 12> kPaletteOptions = {
			{
             {Image_window::UiPaletteDisabled, "disabled", 0x59A},
             {Image_window::UiPaletteDay, "day", 0x658},
             {Image_window::UiPaletteDusk, "dusk", 0xB11},
             {Image_window::UiPaletteInvisible, "invisible", 0xB12},
             {Image_window::UiPaletteOvercast, "overcast", 0xB13},
             {Image_window::UiPaletteFog, "fog", 0xB14},
             {Image_window::UiPaletteSpell, "spell", 0xB15},
             {Image_window::UiPaletteCandle, "candle", 0xB16},
             {Image_window::UiPaletteRed, "red", 0xB17},
             {Image_window::UiPaletteLightning, "lightning", 0xB18},
             {Image_window::UiPaletteSingleLight, "single_light", 0xB19},
             {Image_window::UiPaletteManyLights, "many_lights", 0xB1A},
			 }
    };

	std::vector<std::string> palette_options() {
		std::vector<std::string> out;
		out.reserve(kPaletteOptions.size());
		for (const auto& opt : kPaletteOptions) {
			out.emplace_back(get_text_msg(opt.msg_id - msg_file_start));
		}
		return out;
	}

	int palette_to_selection(int mode) {
		for (size_t i = 0; i < kPaletteOptions.size(); ++i) {
			if (kPaletteOptions[i].mode == mode) {
				return static_cast<int>(i);
			}
		}
		return 0;
	}

	int selection_to_palette(int state) {
		if (state >= 0 && static_cast<size_t>(state) < kPaletteOptions.size()) {
			return kPaletteOptions[state].mode;
		}
		return Image_window::UiPaletteDisabled;
	}

	const char* palette_name(int mode) {
		for (const auto& opt : kPaletteOptions) {
			if (opt.mode == mode) {
				return opt.cfg_name;
			}
		}
		return "disabled";
	}

	int palette_from_name(const std::string& s, int fallback) {
		for (const auto& opt : kPaletteOptions) {
			if (s == opt.cfg_name) {
				return opt.mode;
			}
		}
		return fallback;
	}

	class UIOptionsLayerSettings_gump : public Modal_gump {
	public:
		UIOptionsLayerSettings_gump(UIOptions_gump::UiLayerSettings& source, std::string layer_name)
				: Modal_gump(nullptr, -1), source_cfg(source), working_cfg(source), layer_name(std::move(layer_name)) {
			SetProceduralBackground(TileRect(0, 0, 100, yForRow(8)), -1);

			for (auto& btn : buttons) {
				btn.reset();
			}

			buttons[id_ok]
					= std::make_unique<Layer_button>(this, &UIOptionsLayerSettings_gump::ok, Strings::OK(), 12, yForRow(7), 50);
			buttons[id_help]
					= std::make_unique<Layer_button>(this, &UIOptionsLayerSettings_gump::help, Strings::HELP(), 50, yForRow(7), 50);
			buttons[id_cancel] = std::make_unique<Layer_button>(
					this, &UIOptionsLayerSettings_gump::cancel, Strings::CANCEL(), 88, yForRow(7), 50);

			build_buttons();
		}

		void close() override {
			done = true;
		}

		void cancel() {
			done = true;
		}

		Gump_button* on_button(int mx, int my) override {
			for (auto& btn : buttons) {
				auto found = btn ? btn->on_button(mx, my) : nullptr;
				if (found) {
					return found;
				}
			}
			return Modal_gump::on_button(mx, my);
		}

		void paint() override {
			Modal_gump::paint();
			for (auto& btn : buttons) {
				if (btn) {
					btn->paint();
				}
			}

			Image_window8* iwin = gwin->get_win();
			font->paint_text(iwin->get_ib8(), layer_name.c_str(), x + label_margin, y + yForRow(0) + 1);
			font->paint_text(iwin->get_ib8(), Strings::Size_(), x + label_margin, y + yForRow(2) + 1);
			font->paint_text(iwin->get_ib8(), Strings::Scalemethod_(), x + label_margin, y + yForRow(3) + 1);
			font->paint_text(iwin->get_ib8(), Strings::Fillmode_(), x + label_margin, y + yForRow(4) + 1);
			font->paint_text(iwin->get_ib8(), Strings::Fillscaler_(), x + label_margin, y + yForRow(5) + 1);
			font->paint_text(iwin->get_ib8(), Strings::Palette_(), x + label_margin, y + yForRow(6) + 1);
			gwin->set_painted();
		}

	private:
		enum button_ids {
			id_first = 0,
			id_ok    = id_first,
			id_help,
			id_cancel,
			id_size,
			id_scale_method,
			id_fill_mode,
			id_fill_scaler,
			id_palette,
			id_count
		};

		using Layer_button = CallbackTextButton<UIOptionsLayerSettings_gump>;
		using Layer_toggle = CallbackToggleTextButton<UIOptionsLayerSettings_gump>;

		std::array<std::unique_ptr<Gump_button>, id_count> buttons;
		UIOptions_gump::UiLayerSettings&                   source_cfg;
		UIOptions_gump::UiLayerSettings                    working_cfg;
		std::string                                        layer_name;
		std::vector<int>                                   scaler_values;

		void ok() {
			source_cfg = working_cfg;
			done       = true;
		}

		void help() {
			SDL_OpenURL(help_url);
		}

		void build_buttons() {
			std::vector<std::string> size_text = {Strings::Full320x200_(), Strings::Medium420x263_(), Strings::Auto0x0_()};
			buttons[id_size]                   = std::make_unique<Layer_toggle>(
                    this, &UIOptionsLayerSettings_gump::toggle_size, std::move(size_text), size_to_selection(working_cfg),
                    get_button_pos_for_label(Strings::Size_()), yForRow(2), 108);

			scaler_values.clear();
			std::vector<std::string> scaler_text;
			scaler_text.reserve(Image_window::SDLScaler);
			for (int i = 0; i < Image_window::SDLScaler; ++i) {
				scaler_values.push_back(i);
				scaler_text.emplace_back(Image_window::get_displayname_for_scaler(i));
			}
			int scaler_sel = 0;
			for (size_t i = 0; i < scaler_values.size(); ++i) {
				if (scaler_values[i] == working_cfg.scaler) {
					scaler_sel = i;
					break;
				}
			}
			buttons[id_scale_method] = std::make_unique<Layer_toggle>(
					this, &UIOptionsLayerSettings_gump::toggle_scale_method, std::move(scaler_text), scaler_sel,
					get_button_pos_for_label(Strings::Scalemethod_()), yForRow(3), 108);

			std::vector<std::string> fill_mode_text = {Strings::Fill_(), Strings::Fit_(), Strings::Centre_()};
			buttons[id_fill_mode]                   = std::make_unique<Layer_toggle>(
                    this, &UIOptionsLayerSettings_gump::toggle_fill_mode, std::move(fill_mode_text),
                    fillmode_to_selection(working_cfg.fill_mode), get_button_pos_for_label(Strings::Fillmode_()), yForRow(4), 108);

			std::vector<std::string> fill_scaler_text
					= {Image_window::get_displayname_for_scaler(Image_window::point),
					   Image_window::get_displayname_for_scaler(Image_window::bilinear)};
			const char* renderer_name = SDL_GetRendererName(SDL_GetRenderer(gwin->get_win()->get_screen_window()));
			fill_scaler_text.emplace_back(
					renderer_name ? renderer_name : Image_window::get_displayname_for_scaler(Image_window::SDLScaler));
			buttons[id_fill_scaler] = std::make_unique<Layer_toggle>(
					this, &UIOptionsLayerSettings_gump::toggle_fill_scaler, std::move(fill_scaler_text),
					fillscaler_to_selection(working_cfg.fill_scaler), get_button_pos_for_label(Strings::Fillscaler_()), yForRow(5),
					108);

			buttons[id_palette] = std::make_unique<Layer_toggle>(
					this, &UIOptionsLayerSettings_gump::toggle_palette, palette_options(),
					palette_to_selection(working_cfg.palette), get_button_pos_for_label(Strings::Palette_()), yForRow(6), 108);

			ResizeWidthToFitWidgets(tcb::span(buttons.data(), buttons.size()));
			HorizontalArrangeWidgets(tcb::span(buttons.data(), 3));
			RightAlignWidgets(tcb::span(buttons.data() + id_size, id_count - id_size));
			set_pos();
		}

		void toggle_size(int state) {
			apply_size_selection(working_cfg, state);
		}

		void toggle_scale_method(int state) {
			if (state >= 0 && size_t(state) < scaler_values.size()) {
				working_cfg.scaler = scaler_values[state];
			}
		}

		void toggle_fill_mode(int state) {
			working_cfg.fill_mode = selection_to_fillmode(state);
		}

		void toggle_fill_scaler(int state) {
			working_cfg.fill_scaler = selection_to_fillscaler(state);
		}

		void toggle_palette(int state) {
			working_cfg.palette = selection_to_palette(state);
		}
	};
}    // namespace

UIOptions_gump::UIOptions_gump() : Modal_gump(nullptr, -1) {
	SetProceduralBackground(TileRect(0, 0, 100, yForRow(row_buttons + 1)), -1);

	for (auto& btn : buttons) {
		btn.reset();
	}

	buttons[id_apply]
			= std::make_unique<UIOptions_button>(this, &UIOptions_gump::apply, Strings::APPLY(), 12, yForRow(row_buttons), 50);
	buttons[id_help]
			= std::make_unique<UIOptions_button>(this, &UIOptions_gump::help, Strings::HELP(), 50, yForRow(row_buttons), 50);
	buttons[id_cancel]
			= std::make_unique<UIOptions_button>(this, &UIOptions_gump::cancel, Strings::CANCEL(), 88, yForRow(row_buttons), 50);

	load_settings();
	build_buttons();
}

void UIOptions_gump::close() {
	done = true;
}

void UIOptions_gump::apply() {
	save_settings();
	paint();
}

void UIOptions_gump::help() {
	SDL_OpenURL(help_url);
}

void UIOptions_gump::cancel() {
	done = true;
}

Gump_button* UIOptions_gump::on_button(int mx, int my) {
	for (auto& btn : buttons) {
		auto found = btn ? btn->on_button(mx, my) : nullptr;
		if (found) {
			return found;
		}
	}
	return Modal_gump::on_button(mx, my);
}

void UIOptions_gump::build_buttons() {
	for (int i = id_first_setting; i < id_count; ++i) {
		// Don't kill the universal button as it may have called us
		if (i != id_universal) {
			buttons[i].reset();
		}
	}

	std::vector<std::string> size_text = {Strings::Full320x200_(), Strings::Medium420x263_(), Strings::Auto0x0_()};
	buttons[id_size]                   = std::make_unique<UIOptions_toggle>(
            this, &UIOptions_gump::toggle_size, std::move(size_text), size_to_selection(global_cfg),
            get_button_pos_for_label(Strings::Size_()), yForRow(row_size), 108);

	std::vector<std::string> scaler_text;
	scaler_text.reserve(Image_window::SDLScaler);
	int scaler_sel = 0;
	for (int i = 0; i < Image_window::SDLScaler; ++i) {
		scaler_text.emplace_back(Image_window::get_displayname_for_scaler(i));
		if (i == global_cfg.scaler) {
			scaler_sel = i;
		}
	}
	buttons[id_scale_method] = std::make_unique<UIOptions_toggle>(
			this, &UIOptions_gump::toggle_scale_method, std::move(scaler_text), scaler_sel,
			get_button_pos_for_label(Strings::Scalemethod_()), yForRow(row_scale), 108);

	std::vector<std::string> fill_mode_text = {Strings::Fill_(), Strings::Fit_(), Strings::Centre_()};
	buttons[id_fill_mode]                   = std::make_unique<UIOptions_toggle>(
            this, &UIOptions_gump::toggle_fill_mode, std::move(fill_mode_text), fillmode_to_selection(global_cfg.fill_mode),
            get_button_pos_for_label(Strings::Fillmode_()), yForRow(row_fill_mode), 108);

	std::vector<std::string> fill_scaler_text
			= {Image_window::get_displayname_for_scaler(Image_window::point),
			   Image_window::get_displayname_for_scaler(Image_window::bilinear)};
	const char* renderer_name = SDL_GetRendererName(SDL_GetRenderer(gwin->get_win()->get_screen_window()));
	fill_scaler_text.emplace_back(
			renderer_name ? renderer_name : Image_window::get_displayname_for_scaler(Image_window::SDLScaler));
	buttons[id_fill_scaler] = std::make_unique<UIOptions_toggle>(
			this, &UIOptions_gump::toggle_fill_scaler, std::move(fill_scaler_text), fillscaler_to_selection(global_cfg.fill_scaler),
			get_button_pos_for_label(Strings::Fillscaler_()), yForRow(row_fill_scl), 108);

	const std::vector<std::string> yes_no = {Strings::No(), Strings::Yes()};
	// Only create the button if it doesn't exist
	// If it does exist just change the frame
	if (auto button = dynamic_cast<UIOptions_toggle*>(buttons[id_universal].get())) {
		button->set_frame(universal ? 1 : 0);
	} else {
		buttons[id_universal] = std::make_unique<UIOptions_toggle>(
				this, &UIOptions_gump::toggle_universal, yes_no, universal ? 1 : 0, get_button_pos_for_label(Strings::AllLayers_()),
				yForRow(row_universal), 44);
	}

	buttons[id_palette] = std::make_unique<UIOptions_toggle>(
			this, &UIOptions_gump::toggle_palette, palette_options(), palette_to_selection(global_cfg.palette),
			get_button_pos_for_label(Strings::Palette_()), yForRow(row_palette), 108);

	if (!universal) {
		buttons[id_layer_mouse] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_mouse_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::MousePointer_()), yForLayerIndex(0), 108);
		buttons[id_layer_conversations] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_conversations_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::Conversations_()), yForLayerIndex(1), 108);
		buttons[id_layer_gumps] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_gumps_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::Gumps_()), yForLayerIndex(2), 108);
		buttons[id_layer_hud_gumps] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_hud_gumps_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::HudGumps_()), yForLayerIndex(3), 108);
		buttons[id_layer_text_gumps] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_text_gumps_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::TextGumps_()), yForLayerIndex(4), 108);
		buttons[id_layer_modal_gumps] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_modal_gumps_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::ModalGumps_()), yForLayerIndex(5), 108);
		buttons[id_layer_text_effect] = std::make_unique<UIOptions_button>(
				this, &UIOptions_gump::open_text_effect_advanced, Strings::AdvancedSettings_(),
				get_button_pos_for_label(Strings::TextEffect_()), yForLayerIndex(6), 108);
	}

	ResizeWidthToFitWidgets(tcb::span(buttons.data(), buttons.size()));
	HorizontalArrangeWidgets(tcb::span(buttons.data(), 3));
	RightAlignWidgets(tcb::span(buttons.data() + id_first_setting, id_count - id_first_setting));
	set_pos();
}

void UIOptions_gump::load_settings() {
	config->value("config/video/ui/universal", universal, true);

	auto read_cfg = [&](const string& base, const UiLayerSettings& fallback) {
		UiLayerSettings cfg = fallback;
		config->value(base + "/width", cfg.width, fallback.width);
		config->value(base + "/height", cfg.height, fallback.height);
		normalize_dims(cfg.width, cfg.height);

		const int fb_scaler      = fallback.scaler < 0 ? 0 : fallback.scaler;
		const int fb_fill_scaler = fallback.fill_scaler < 0 ? 0 : fallback.fill_scaler;

		string scaler_name;
		config->value(base + "/scale_method", scaler_name, Image_window::get_name_for_scaler(fb_scaler));
		cfg.scaler = Image_window::get_scaler_for_name(scaler_name.c_str());
		if (cfg.scaler < 0) {
			cfg.scaler = fb_scaler;
		}

		string fallback_fillmode;
		Image_window::fillmode_to_string(fallback.fill_mode, fallback_fillmode);
		string fillmode_str;
		config->value(base + "/fill_mode", fillmode_str, fallback_fillmode.c_str());
		cfg.fill_mode = Image_window::string_to_fillmode(fillmode_str.c_str());
		if (cfg.fill_mode == 0) {
			cfg.fill_mode = fallback.fill_mode;
		}

		string fill_scaler_name;
		config->value(base + "/fill_scaler", fill_scaler_name, Image_window::get_name_for_scaler(fb_fill_scaler));
		cfg.fill_scaler = Image_window::get_scaler_for_name(fill_scaler_name.c_str());
		if (cfg.fill_scaler < 0) {
			cfg.fill_scaler = fb_fill_scaler;
		}

		string palette_str;
		config->value(base + "/palette", palette_str, palette_name(fallback.palette));
		cfg.palette = palette_from_name(palette_str, fallback.palette);

		return cfg;
	};

	global_cfg.width       = 420;
	global_cfg.height      = 263;
	global_cfg.scaler      = Image_window::point;
	global_cfg.fill_mode   = Image_window::Fit;
	global_cfg.fill_scaler = Image_window::point;
	global_cfg             = read_cfg("config/video/ui", global_cfg);

	for (size_t i = 0; i < layer_cfgs.size(); ++i) {
		layer_cfgs[i] = read_cfg(string("config/video/ui/") + kLayerKeys[i], global_cfg);
	}
}

void UIOptions_gump::save_settings() {
	auto write_cfg = [&](const string& base, const UiLayerSettings& cfg) {
		config->set(base + "/width", cfg.width, false);
		config->set(base + "/height", cfg.height, false);
		config->set(base + "/scale_method", Image_window::get_name_for_scaler(cfg.scaler), false);
		string fillmode_str;
		Image_window::fillmode_to_string(cfg.fill_mode, fillmode_str);
		config->set(base + "/fill_mode", fillmode_str, false);
		config->set(base + "/fill_scaler", Image_window::get_name_for_scaler(cfg.fill_scaler), false);
		config->set(base + "/palette", palette_name(cfg.palette), false);
	};

	config->set("config/video/ui/universal", universal ? "yes" : "no", false);
	write_cfg("config/video/ui", global_cfg);
	gwin->set_ui_config(global_cfg.width, global_cfg.height, global_cfg.scaler, global_cfg.fill_mode, global_cfg.fill_scaler);

	for (size_t i = 0; i < layer_cfgs.size(); ++i) {
		write_cfg(string("config/video/ui/") + kLayerKeys[i], layer_cfgs[i]);
		const UiLayerSettings& cfg = universal ? global_cfg : layer_cfgs[i];
		gwin->set_ui_layer_config(kLayerKinds[i], cfg.width, cfg.height, cfg.scaler, cfg.fill_mode, cfg.fill_scaler);
		gwin->set_ui_layer_palette(kLayerKinds[i], cfg.palette);
	}

	config->write_back();
	// Recompute the layers' fixed-palette overrides for the new settings.
	if (Palette* pal = gwin->get_pal()) {
		pal->apply();
	}
	gwin->set_all_dirty();
}

void UIOptions_gump::toggle_size(int state) {
	apply_size_selection(global_cfg, state);
}

void UIOptions_gump::toggle_scale_method(int state) {
	if (state >= 0 && state < Image_window::SDLScaler) {
		global_cfg.scaler = state;
	}
}

void UIOptions_gump::toggle_fill_mode(int state) {
	global_cfg.fill_mode = selection_to_fillmode(state);
}

void UIOptions_gump::toggle_fill_scaler(int state) {
	global_cfg.fill_scaler = selection_to_fillscaler(state);
}

void UIOptions_gump::toggle_palette(int state) {
	global_cfg.palette = selection_to_palette(state);
}

void UIOptions_gump::toggle_universal(int state) {
	universal = (state != 0);
	build_buttons();
	paint();
}

void UIOptions_gump::open_layer_advanced(size_t idx) {
	if (idx >= layer_cfgs.size()) {
		return;
	}
	UIOptionsLayerSettings_gump options(layer_cfgs[idx], get_layer_name(idx));
	gumpman->do_modal_gump(&options, Mouse::hand);
	paint();
}

void UIOptions_gump::open_mouse_advanced() {
	open_layer_advanced(0);
}

void UIOptions_gump::open_conversations_advanced() {
	open_layer_advanced(1);
}

void UIOptions_gump::open_gumps_advanced() {
	open_layer_advanced(2);
}

void UIOptions_gump::open_hud_gumps_advanced() {
	open_layer_advanced(3);
}

void UIOptions_gump::open_text_gumps_advanced() {
	open_layer_advanced(4);
}

void UIOptions_gump::open_modal_gumps_advanced() {
	open_layer_advanced(5);
}

void UIOptions_gump::open_text_effect_advanced() {
	open_layer_advanced(6);
}

void UIOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	Image_window8* iwin = gwin->get_win();
	font->paint_text(iwin->get_ib8(), Strings::Size_(), x + label_margin, y + yForRow(row_size) + 1);
	font->paint_text(iwin->get_ib8(), Strings::Scalemethod_(), x + label_margin, y + yForRow(row_scale) + 1);
	font->paint_text(iwin->get_ib8(), Strings::Fillmode_(), x + label_margin, y + yForRow(row_fill_mode) + 1);
	font->paint_text(iwin->get_ib8(), Strings::Fillscaler_(), x + label_margin, y + yForRow(row_fill_scl) + 1);
	font->paint_text(iwin->get_ib8(), Strings::Palette_(), x + label_margin, y + yForRow(row_palette) + 1);
	font->paint_text(iwin->get_ib8(), Strings::SameSettingsFor_(), x + label_margin, y + yForRow(row_same) + 1);
	font->paint_text(iwin->get_ib8(), Strings::AllLayers_(), x + label_margin, y + yForRow(row_universal) + 1);

	if (!universal) {
		for (size_t i = 0; i < kLayerCount; ++i) {
			font->paint_text(iwin->get_ib8(), get_layer_name(i), x + label_margin, y + yForLayerIndex(static_cast<int>(i)) + 1);
		}
	}

	gwin->set_painted();
}
