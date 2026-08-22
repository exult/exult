/**
**  perf.cc - PerformanceTimer class implemenation
**
**/

/*
Copyright (C) 2026 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/
#include "perf.h"

#include "gamewin.h"

#include <SDL3/SDL.h>

PerformanceTimer* PerformanceTimer::all    = nullptr;
PerformanceTimer* PerformanceTimer::active = nullptr;
int               PerformanceTimer::mode   = 0;

PerformanceTimer& PerformanceTimer::GetPerfTimer(std::string_view name, std::string_view name2, bool unique) {
	if (!mode) {
		static PerformanceTimer none;
		return none;
	}
	PerformanceTimer* node;
	for (node = all; node; node = node->next) {
		// Use node with matching name if unique is false or the node hasn't been used this frame
		std::string_view node_name = node->name;
		if (node_name.substr(0, name.size()) == name && node_name.substr(name.size()) == name2 && (!unique || !node->used)) {
			return *node;
		}
	}
	node = new PerformanceTimer();
	node->name.reserve(name.size() + name2.size());
	node->name += name;
	node->name += name2;
	node->AddToList(all);
	return *node;
}

uint64 PerformanceTimer::GetNS() {
	return SDL_GetTicksNS();
}

void PerformanceTimer::paintPerfMetrics() {
	auto       gwin              = Game_window::get_instance();
	static int layer_handle      = -1;
	static int layer_handle_mini = -1;
	if (layer_handle != -1) {
		gwin->layer_set_visible(layer_handle, mode == 2);
	}
	if (layer_handle_mini != -1) {
		gwin->layer_set_visible(layer_handle_mini, mode == 1);
	}
	if (!mode) {
		return;
	}

	auto              font = fontManager.get_font("SMALL_BLACK_FONT");
	Xform_palette     textbg;
	uint32            rgba[256];
	PerformanceTimer* node;
	char              line[80];
	Image_buffer8*    ibuf;

	for (uint8 i = 0; i < 255; i++) {
		textbg.colors[i] = i;
	}
	textbg.colors[255] = 254;
	float scale_factor = std::max(.75f, gwin->get_win()->get_display_width() / 1440.f);

	if (mode == 2) {
		if (layer_handle == -1) {
			layer_handle = gwin->create_layer("Perf text", 960, 720, 255, 0, INT_MAX);
			if (layer_handle == -1) {
				// Failed, do nothing
				return;
			}
			// layer_set_ui_kind(layer_handle,)
			gwin->layer_set_opaque(layer_handle, false);
			std::memset(rgba, 255, sizeof(rgba));
			rgba[254] = 0xAf000000;
			gwin->layer_set_index_argb(layer_handle, rgba);
		}
		gwin->layer_set_dest(layer_handle, 0, 0, int(960 * scale_factor), int(720 * scale_factor));
		gwin->layer_set_ui_kind(layer_handle, Image_window::UiLayerFullScreenNoScaler);
	}
	if (mode == 1) {
		if (layer_handle_mini == -1) {
			layer_handle_mini = gwin->create_layer("Perf text mini", 256, 20, 255, 0, INT_MAX);
			if (layer_handle_mini == -1) {
				// Failed, do nothing
				return;
			}
			gwin->layer_set_opaque(layer_handle_mini, false);
			std::memset(rgba, 255, sizeof(rgba));
			rgba[254] = 0xAf000000;
			gwin->layer_set_index_argb(layer_handle_mini, rgba);
			gwin->layer_set_ui_kind(layer_handle_mini, Image_window::UiLayerFullScreenNoScaler);
		}
		gwin->layer_set_dest(layer_handle_mini, 0, 0, int(256 * scale_factor), int(20 * scale_factor));
	}

	auto& frameperf = GetPerfTimer("Frame", "", false);

	frameperf.end_phase();

	if (font) {
		ibuf = gwin->get_layer_ibuf(mode == 2 ? layer_handle : layer_handle_mini);
		if (!ibuf) {
			// Shouldn't happen but just in case
			return;
		}
		ibuf->fill8(255);
		Image_buffer8* prev_ibuf = gwin->push_render_target(ibuf);
		int            y = 0, xlimit = 0;

		auto frame = frameperf.value / 1000;
		snprintf(line, 80, "Performance metrics %.02f fps", frame ? 1000000.0 / frame : 0);
		xlimit = font->paint_text_fixedwidth(ibuf, line, 0, y++ * 9, 8);
		ibuf->fill_translucent8(0, xlimit + 2, 9, 0, y * 9 - 9, textbg);

		for (node = all; node; node = node->next) {
			if (node->used && (mode == 2 || node == &frameperf)) {
				auto time = node->value / 1000;
				snprintf(line, 80, "%s: %lli us", node->name.c_str(), time);
				xlimit = font->paint_text_fixedwidth(ibuf, line, 0, y++ * 9, 8);
				ibuf->fill_translucent8(0, xlimit + 2, 9, 0, y * 9 - 9, textbg);
			}
		}
		gwin->push_render_target(prev_ibuf);
		gwin->layer_set_dirty(layer_handle);
		gwin->set_all_dirty();
	}

	for (node = all; node; node = node->next) {
		node->start_frame();
	}
	frameperf.start_phase(false);
}
