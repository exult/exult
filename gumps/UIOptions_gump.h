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

#ifndef UIOPTIONS_GUMP_H
#define UIOPTIONS_GUMP_H

#include "Modal_gump.h"
#include "imagewin/imagewin.h"

#include <array>
#include <memory>

class Gump_button;

class UIOptions_gump : public Modal_gump {
public:
	struct UiLayerSettings {
		int                    width       = 420;
		int                    height      = 263;
		int                    scaler      = Image_window::point;
		Image_window::FillMode fill_mode   = Image_window::Fit;
		int                    fill_scaler = Image_window::point;
		int                    palette     = Image_window::UiPaletteDisabled;
	};

	UIOptions_gump();

	void         close() override;
	void         apply();
	void         help();
	void         cancel();
	void         paint() override;
	Gump_button* on_button(int mx, int my) override;

private:
	friend class Mui_options_model;
	enum button_ids {
		id_first = 0,
		id_apply = id_first,
		id_help,
		id_cancel,
		id_first_setting,
		id_size = id_first_setting,
		id_scale_method,
		id_fill_mode,
		id_fill_scaler,
		id_palette,
		id_universal,
		id_layer_mouse,
		id_layer_conversations,
		id_layer_gumps,
		id_layer_hud_gumps,
		id_layer_text_gumps,
		id_layer_modal_gumps,
		id_layer_text_effect,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;
	UiLayerSettings                                    global_cfg;
	std::array<UiLayerSettings, 7>                     layer_cfgs;
	bool                                               universal = true;

	void build_buttons();
	void load_settings();
	void save_settings();

	void toggle_size(int state);
	void toggle_scale_method(int state);
	void toggle_fill_mode(int state);
	void toggle_fill_scaler(int state);
	void toggle_palette(int state);
	void toggle_universal(int state);

	void open_mouse_advanced();
	void open_conversations_advanced();
	void open_gumps_advanced();
	void open_hud_gumps_advanced();
	void open_text_gumps_advanced();
	void open_modal_gumps_advanced();
	void open_text_effect_advanced();
	void open_layer_advanced(size_t idx);
};

#endif
