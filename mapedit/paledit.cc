/**
 ** A GTK widget showing a palette's colors.
 **
 ** Written: 12/24/2000 - JSF
 **/

/*
Copyright (C) 2000-2022 The Exult Team

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

#include "paledit.h"

#include "shapefile.h"
#include "utils.h"

#include <glib.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>

using EStudio::Alert;
using EStudio::Prompt;
using std::cout;
using std::endl;
using std::ifstream;
using std::make_unique;
using std::ofstream;
using std::ostream;
using std::setw;
using std::string;

/*
 *  Write out a single palette to a buffer.
 */

static void Write_palette(
		unsigned char* buf,    // 3*256 bytes.
		ExultRgbCmap*  pal     // Palette to write.
) {
	for (int i = 0; i < 256; i++) {
		const int r    = (pal->colors[i] >> 16) & 255;
		const int g    = (pal->colors[i] >> 8) & 255;
		const int b    = pal->colors[i] & 255;
		buf[3 * i]     = r / 4;    // Range 0-63.
		buf[3 * i + 1] = g / 4;
		buf[3 * i + 2] = b / 4;
	}
}

/*
 *  Blit onto screen.
 */

inline void Palette_edit::show(
		int x, int y, int w, int h    // Area to blit.
) {
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	if (drawgc != nullptr) {    // In expose Callback
		const int bw = alloc.width / 16;
		const int bh = alloc.height / 16;
		const int rw = alloc.width % 16;
		const int rh = alloc.height % 16;
		// Two segments intersect iff neither is entirely right of the other
		for (int ly = 0; ly < 16; ly++) {
			const int yp = (ly * bh) + (ly <= rh ? ly : rh);
			const int hp = bh + (ly <= rh ? 1 : 0);
			if ((yp <= (y + h)) && (y <= (yp + hp))) {
				for (int lx = 0; lx < 16; lx++) {
					const int xp = (lx * bw) + (lx <= rw ? lx : rw);
					const int wp = bw + (lx <= rw ? 1 : 0);
					if ((xp <= (x + w)) && (x <= (xp + wp))) {
						const guint32 color
								= palettes[cur_pal]->colors[lx + ly * 16];
						cairo_set_source_rgb(
								drawgc, ((color >> 16) & 255) / 255.0,
								((color >> 8) & 255) / 255.0,
								(color & 255) / 255.0);
						cairo_rectangle(drawgc, xp, yp, wp, hp);
						cairo_fill(drawgc);
					}
				}
			}
		}
	}
	if ((selected >= 0) && (drawgc != nullptr)) {    // Show selected.
		// Draw yellow box.
		cairo_set_line_width(drawgc, 1.0);
		cairo_set_source_rgb(
				drawgc, ((drawfg >> 16) & 255) / 255.0,
				((drawfg >> 8) & 255) / 255.0, (drawfg & 255) / 255.0);
		cairo_rectangle(
				drawgc, selected_box.x, selected_box.y, selected_box.w,
				selected_box.h);
		cairo_stroke(drawgc);
	}
}

/*
 *  Select an entry.  This should be called after rendering
 *  the shape.
 */

void Palette_edit::select(int new_sel) {
	selected = new_sel;
	// Remove prev. selection msg.
	gtk_statusbar_pop(GTK_STATUSBAR(sbar), sbar_sel);
	char buf[150];    // Show new selection.
	g_snprintf(buf, sizeof(buf), "Color %d (0x%02x)", new_sel, new_sel);
	gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
}

/*
 *  Load/reload from file.
 */

void Palette_edit::load_internal() {
	// Free old.
	for (auto* palette : palettes) {
		delete palette;
	}
	const unsigned cnt = flex_info->size();
	palettes.resize(cnt);    // Set size of list.
	if (!cnt) {              // No palettes?
		new_palette();       // Create 1 blank palette.
	} else {
		for (unsigned pnum = 0; pnum < cnt; pnum++) {
			size_t         len;
			unsigned char* buf = flex_info->get(pnum, len);
			palettes[pnum]     = new ExultRgbCmap;
			for (size_t i = 0; i < len / 3; i++) {
				palettes[pnum]->colors[i] = (buf[3 * i] << 16) * 4
											+ (buf[3 * i + 1] << 8) * 4
											+ buf[3 * i + 2] * 4;
			}
			for (size_t i = len / 3; i < 256; i++) {
				palettes[pnum]->colors[i] = 0;
			}
		}
	}
}

/*
 *  Draw the palette.  This need only be called when it changes.
 */

void Palette_edit::render() {
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	const int neww = alloc.width;
	const int newh = alloc.height;
	// Changed size?
	if (neww != width || newh != height) {
		width  = neww;
		height = newh;
	}
	// Figure cell size.
	const int eachw = width / 16;
	const int eachh = height / 16;
	// Figure extra pixels.
	const int extraw = width % 16;
	const int extrah = height % 16;
	if (selected >= 0) {    // Update selected box.
		const int selx = selected % 16;
		const int sely = selected / 16;
		selected_box.x = selx * eachw;
		selected_box.y = sely * eachh;
		selected_box.w = eachw;
		selected_box.h = eachh;
		if (selx < extraw) {    // Watch for extra pixels.
			selected_box.w++;
			selected_box.x += selx;
		} else {
			selected_box.x += extraw;
		}
		if (sely < extrah) {
			selected_box.h++;
			selected_box.y += sely;
		} else {
			selected_box.y += extrah;
		}
		select(selected);
	}
	gtk_widget_queue_draw(draw);
}

/*
 *  Handle double-click on a color by bringing up a color-selector.
 */

void Palette_edit::double_clicked() {
	cout << "Double-clicked" << endl;
	if (selected < 0 || colorsel) {    // Only one at a time.
		return;                        // Nothing selected.
	}
	char buf[150];    // Show new selection.
	g_snprintf(buf, sizeof(buf), "Color %d (0x%02x)", selected, selected);
	colorsel = GTK_COLOR_CHOOSER_DIALOG(
			gtk_color_chooser_dialog_new(buf, nullptr));
	// Get color.
	const guint32 c = palettes[cur_pal]->colors[selected];
	GdkRGBA       rgba;
	rgba.red   = ((c >> 16) & 0xff) / 255.0;
	rgba.green = ((c >> 8) & 0xff) / 255.0;
	rgba.blue  = ((c >> 0) & 0xff) / 255.0;
	rgba.alpha = 1.0;
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
	if (gtk_dialog_run(GTK_DIALOG(colorsel)) == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
		auto r = static_cast<unsigned char>(rgba.red * 255.0);
		auto g = static_cast<unsigned char>(rgba.green * 255.0);
		auto b = static_cast<unsigned char>(rgba.blue * 255.0);
		if (selected >= 0) {
			palettes[cur_pal]->colors[selected] = (r << 16) + (g << 8) + b;
		}
		// Send to flex file.
		update_flex(cur_pal);
		render();
	}
	gtk_widget_destroy(GTK_WIDGET(colorsel));
	colorsel = nullptr;
}

/*
 *  Configure the viewing window.
 */

gint Palette_edit::configure(
		GtkWidget*         widget,    // The view window.
		GdkEventConfigure* event,
		gpointer           data    // ->Palette_edit
) {
	ignore_unused_variable_warning(event, widget);
	auto* paled = static_cast<Palette_edit*>(data);
	if (!paled->width) {    // First time?
		// Foreground = yellow.
		paled->drawfg = (255 << 16) + (255 << 8);
	}
	paled->render();
	return true;
}

/*
 *  Handle an expose event.
 */

gint Palette_edit::expose(
		GtkWidget* widget,    // The view window.
		cairo_t*   cairo,
		gpointer   data    // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget);
	auto* paled = static_cast<Palette_edit*>(data);
	paled->set_graphic_context(cairo);
	GdkRectangle area = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	paled->show(area.x, area.y, area.width, area.height);
	paled->set_graphic_context(nullptr);
	return true;
}

/*
 *  Handle a mouse button press event.
 */

gint Palette_edit::mouse_press(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget);
	auto* paled = static_cast<Palette_edit*>(data);

	if (event->button == 4 || event->button == 5) {    // mouse wheel
		return true;
	}

	if (paled->colorsel) {
		return true;    // Already editing a color.
	}
	const int old_selected = paled->selected;
	const int width        = paled->width;
	const int height       = paled->height;
	const int eventx       = static_cast<int>(event->x);
	const int eventy       = static_cast<int>(event->y);
	// Figure cell size.
	const int eachw = width / 16;
	const int eachh = height / 16;
	// Figure extra pixels.
	const int extraw = width % 16;
	const int extrah = height % 16;
	const int extrax
			= extraw * (eachw + 1);    // Total length of extra-sized boxes.
	const int extray = extrah * (eachh + 1);
	int       selx;
	int       sely;    // Gets box indices.
	if (eventx < extrax) {
		selx = eventx / (eachw + 1);
	} else {
		selx = extraw + (eventx - extrax) / eachw;
	}
	if (eventy < extray) {
		sely = eventy / (eachh + 1);
	} else {
		sely = extrah + (eventy - extray) / eachh;
	}
	paled->selected = sely * 16 + selx;
	if (paled->selected == old_selected) {
		// Same square.  Check for dbl-click.
		if (reinterpret_cast<GdkEvent*>(event)->type == GDK_2BUTTON_PRESS) {
			paled->double_clicked();
		}
	} else {
		paled->render();
	}
	if (event->button == 3) {
		gtk_menu_popup_at_pointer(
				GTK_MENU(paled->create_popup()),
				reinterpret_cast<GdkEvent*>(event));
	}
	return true;
}

/*
 *  Someone wants the dragged shape.
 */

void Palette_edit::drag_data_get(
		GtkWidget*        widget,    // The view window.
		GdkDragContext*   context,
		GtkSelectionData* seldata,    // Fill this in.
		guint info, guint time,
		gpointer data    // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget, context, seldata, info, time, data);
	cout << "In DRAG_DATA_GET" << endl;
}

/*
 *  Beginning of a drag.
 */

gint Palette_edit::drag_begin(
		GtkWidget*      widget,    // The view window.
		GdkDragContext* context,
		gpointer        data    // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget, context, data);
	cout << "In DRAG_BEGIN of Palette" << endl;
	// Palette_edit *paled = static_cast<Palette_edit *>(data);
	//  Maybe someday.
	return true;
}

/*
 *  Handle a change to the 'Palette #' spin button.
 */

void Palette_edit::palnum_changed(
		GtkAdjustment* adj,    // The adjustment.
		gpointer       data    // ->Shape_chooser.
) {
	auto*      paled  = static_cast<Palette_edit*>(data);
	const gint newnum = static_cast<gint>(gtk_adjustment_get_value(adj));
	paled->show_palette(newnum);
	paled->render();
	paled->enable_controls();
}

/*
 *  Callbacks for buttons:
 */
void on_exportbtn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	Create_file_selection(
			"Export palette to text format", "<PATCH>", nullptr, {},
			GTK_FILE_CHOOSER_ACTION_SAVE, Palette_edit::export_palette,
			user_data);
}

void on_importbtn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	Create_file_selection(
			"Import palette from text format", "<STATIC>", nullptr, {},
			GTK_FILE_CHOOSER_ACTION_OPEN, Palette_edit::import_palette,
			user_data);
}

void on_insert_btn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	auto* paled = static_cast<Palette_edit*>(user_data);
	paled->add_palette();
}

void on_remove_btn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	auto* paled = static_cast<Palette_edit*>(user_data);
	paled->remove_palette();
}

void on_up_btn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	auto* paled = static_cast<Palette_edit*>(user_data);
	paled->move_palette(true);
}

void on_down_btn_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button);
	auto* paled = static_cast<Palette_edit*>(user_data);
	paled->move_palette(false);
}

/*
 *  Create box with 'Palette #', 'Import', 'Move' controls.
 */

GtkWidget* Palette_edit::create_controls() {
	// Create main box.
	GtkWidget* topframe = gtk_frame_new(nullptr);
	widget_set_margins(
			topframe, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(topframe, true);

	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(vbox), false);
	gtk_widget_set_visible(vbox, true);
	gtk_container_add(GTK_CONTAINER(topframe), vbox);

	GtkWidget* hbox0 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox0), false);
	widget_set_margins(
			hbox0, 1 * HMARGIN, 1 * HMARGIN, 1 * VMARGIN, 1 * VMARGIN);
	gtk_widget_set_visible(hbox0, true);
	gtk_box_pack_start(GTK_BOX(vbox), hbox0, true, true, 0);
	/*
	 *  The 'Edit' controls.
	 */
	GtkWidget* frame = gtk_frame_new("Edit");
	widget_set_margins(
			frame, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(frame, true);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, false, false, 0);

	GtkWidget* hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_visible(hbuttonbox, true);
	gtk_container_add(GTK_CONTAINER(frame), hbuttonbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(hbuttonbox), 0);

	insert_btn = gtk_button_new_with_label("New");
	widget_set_margins(
			insert_btn, 2 * HMARGIN, 1 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(insert_btn, true);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), insert_btn);
	gtk_widget_set_can_default(GTK_WIDGET(insert_btn), true);

	remove_btn = gtk_button_new_with_label("Remove");
	widget_set_margins(
			remove_btn, 1 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(remove_btn, true);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), remove_btn);
	gtk_widget_set_can_default(GTK_WIDGET(remove_btn), true);

	g_signal_connect(
			G_OBJECT(insert_btn), "clicked", G_CALLBACK(on_insert_btn_clicked),
			this);
	g_signal_connect(
			G_OBJECT(remove_btn), "clicked", G_CALLBACK(on_remove_btn_clicked),
			this);
	/*
	 *  The 'Move' controls.
	 */
	frame = gtk_frame_new("Move");
	widget_set_margins(
			frame, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(frame, true);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, false, false, 0);

	GtkWidget* bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(bbox), true);
	gtk_widget_set_visible(bbox, true);
	gtk_container_add(GTK_CONTAINER(frame), bbox);

	down_btn = gtk_button_new();
	widget_set_margins(
			down_btn, 2 * HMARGIN, 1 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(down_btn, true);
	gtk_box_pack_start(GTK_BOX(bbox), down_btn, false, false, 0);
	gtk_widget_set_can_default(GTK_WIDGET(down_btn), true);
	GtkWidget* arrow
			= gtk_image_new_from_icon_name("go-down", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(down_btn), arrow);

	up_btn = gtk_button_new();
	widget_set_margins(
			up_btn, 1 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(up_btn, true);
	gtk_box_pack_start(GTK_BOX(bbox), up_btn, false, false, 0);
	gtk_widget_set_can_default(GTK_WIDGET(up_btn), true);
	arrow = gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(up_btn), arrow);

	g_signal_connect(
			G_OBJECT(down_btn), "clicked", G_CALLBACK(on_down_btn_clicked),
			this);
	g_signal_connect(
			G_OBJECT(up_btn), "clicked", G_CALLBACK(on_up_btn_clicked), this);
	/*
	 *  The 'File' controls.
	 */
	frame = gtk_frame_new("File");
	widget_set_margins(
			frame, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(frame, true);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, false, false, 0);

	hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_visible(hbuttonbox, true);
	gtk_container_add(GTK_CONTAINER(frame), hbuttonbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(hbuttonbox), 0);

	GtkWidget* importbtn = gtk_button_new_with_label("Import");
	widget_set_margins(
			importbtn, 2 * HMARGIN, 1 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(importbtn, true);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), importbtn);
	gtk_widget_set_can_default(GTK_WIDGET(importbtn), true);

	GtkWidget* exportbtn = gtk_button_new_with_label("Export");
	widget_set_margins(
			exportbtn, 1 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(exportbtn, true);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), exportbtn);
	gtk_widget_set_can_default(GTK_WIDGET(exportbtn), true);

	g_signal_connect(
			G_OBJECT(importbtn), "clicked", G_CALLBACK(on_importbtn_clicked),
			this);
	g_signal_connect(
			G_OBJECT(exportbtn), "clicked", G_CALLBACK(on_exportbtn_clicked),
			this);
	return topframe;
}

/*
 *  Enable/disable controls after changes.
 */

void Palette_edit::enable_controls() {
	// Can't delete last one.
	gtk_widget_set_sensitive(remove_btn, cur_pal >= 0 && palettes.size() > 1);
	if (cur_pal == -1) {    // No palette?
		gtk_widget_set_sensitive(down_btn, false);
		gtk_widget_set_sensitive(up_btn, false);
		gtk_widget_set_sensitive(remove_btn, false);
	} else {
		gtk_widget_set_sensitive(
				down_btn, static_cast<unsigned>(cur_pal) < palettes.size() - 1);
		gtk_widget_set_sensitive(up_btn, cur_pal > 0);
		gtk_widget_set_sensitive(remove_btn, palettes.size() > 1);
	}
}

/*
 *  Set up box.
 */

void Palette_edit::setup() {
	// Put things in a vert. box.
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(vbox), false);
	gtk_widget_set_visible(vbox, true);
	set_widget(vbox);

	// A frame looks nice.
	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	widget_set_margins(
			frame, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(frame, true);
	gtk_box_pack_start(GTK_BOX(vbox), frame, true, true, 0);

	draw = gtk_drawing_area_new();    // Create drawing area window.
	//	gtk_widget_set_size_request(draw, w, h);
	// Indicate the events we want.
	gtk_widget_set_events(
			draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
						  | GDK_BUTTON1_MOTION_MASK);
	// Set "configure" handler.
	g_signal_connect(
			G_OBJECT(draw), "configure-event", G_CALLBACK(configure), this);
	// Set "expose-event" - "draw" handler.
	g_signal_connect(G_OBJECT(draw), "draw", G_CALLBACK(expose), this);
	// Set mouse click handler.
	g_signal_connect(
			G_OBJECT(draw), "button-press-event", G_CALLBACK(mouse_press),
			this);
	// Mouse motion.
	g_signal_connect(
			G_OBJECT(draw), "drag-begin", G_CALLBACK(drag_begin), this);
	g_signal_connect(
			G_OBJECT(draw), "drag-data-get", G_CALLBACK(drag_data_get), this);
	gtk_container_add(GTK_CONTAINER(frame), draw);
	widget_set_margins(
			draw, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(draw, true);

	// At bottom, a status bar.
	sbar     = gtk_statusbar_new();
	sbar_sel = gtk_statusbar_get_context_id(GTK_STATUSBAR(sbar), "selection");
	// At the bottom, status bar & frame:
	GtkWidget* hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox1), false);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, false, false, 0);
	gtk_widget_set_visible(hbox1, true);
	gtk_box_pack_start(GTK_BOX(hbox1), sbar, true, true, 0);
	// Palette # to right of sbar.
	GtkWidget* label = gtk_label_new("Palette #:");
	widget_set_margins(
			label, 2 * HMARGIN, 1 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_box_pack_start(GTK_BOX(hbox1), label, false, false, 0);
	gtk_widget_set_visible(label, true);

	// A spin button for palette#.
	palnum_adj = GTK_ADJUSTMENT(
			gtk_adjustment_new(0, 0, palettes.size() - 1, 1, 2, 2));
	pspin = gtk_spin_button_new(palnum_adj, 1, 0);
	g_signal_connect(
			G_OBJECT(palnum_adj), "value-changed", G_CALLBACK(palnum_changed),
			this);
	widget_set_margins(
			pspin, 1 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_box_pack_start(GTK_BOX(hbox1), pspin, false, false, 0);
	gtk_widget_set_visible(pspin, true);

	// Add edit controls to bottom.
	gtk_box_pack_start(GTK_BOX(vbox), create_controls(), false, false, 0);
	widget_set_margins(
			sbar, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(sbar, true);
	enable_controls();
}

/*
 *  Create/add a new palette.
 */

void Palette_edit::new_palette() {
	auto* newpal = new ExultRgbCmap;    // R, G, B, then all black.
	memset(&newpal->colors[0], 0, 256 * sizeof(guint32));
	newpal->colors[0] = 255 << 16;
	newpal->colors[1] = 255 << 8;
	newpal->colors[2] = 255;
	const int index   = palettes.size();    // Index of new palette.
	palettes.push_back(newpal);
	update_flex(index);    // Add to file.
}

/*
 *  Update palette entry in flex file.
 */

void Palette_edit::update_flex(int pnum    // Palette # to send to file.
) {
	auto buf = make_unique<unsigned char[]>(3 * 256);
	Write_palette(buf.get(), palettes[pnum]);
	// Update or append file data.
	flex_info->set(pnum, std::move(buf), 3 * 256);
	flex_info->set_modified();
}

/*
 *  Create the list for a single palette.
 */

Palette_edit::Palette_edit(Flex_file_info* flinfo    // Flex-file info.
						   )
		: Object_browser(nullptr, flinfo), flex_info(flinfo),
		  /* image(nullptr),*/ width(0), height(0), cur_pal(0),
		  colorsel(nullptr) {
	load_internal();    // Load from file.
	setup();
}

/*
 *  Delete.
 */

Palette_edit::~Palette_edit() {
	for (auto* palette : palettes) {
		delete palette;
	}
	gtk_widget_destroy(get_widget());
}

/*
 *  Get i'th palette.
 */

void Palette_edit::show_palette(int palnum) {
	cur_pal = palnum;
}

/*
 *  Unselect.
 */

void Palette_edit::unselect(bool need_render    // 1 to render and show.
) {
	if (selected >= 0) {
		reset_selected();
		if (need_render) {
			render();
		}
	}
}

/*
 *  Move a palette within the list.
 */

void Palette_edit::move_palette(bool up) {
	if (cur_pal < 0) {
		return;
	}
	ExultRgbCmap* tmp;
	if (up) {
		if (cur_pal > 0) {
			tmp                   = palettes[cur_pal - 1];
			palettes[cur_pal - 1] = palettes[cur_pal];
			palettes[cur_pal]     = tmp;
			cur_pal--;
			flex_info->swap(cur_pal);    // Update flex-file list.
		}
	} else {
		if (static_cast<unsigned>(cur_pal) < palettes.size() - 1) {
			tmp                   = palettes[cur_pal + 1];
			palettes[cur_pal + 1] = palettes[cur_pal];
			palettes[cur_pal]     = tmp;
			flex_info->swap(cur_pal);    // Update flex-file list.
			cur_pal++;
		}
	}
	flex_info->set_modified();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
}

/*
 *  Update upper bound of a range widget.
 */

void Update_range_upper(GtkAdjustment* adj, int new_upper) {
	gtk_adjustment_set_upper(adj, new_upper);
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
}

/*
 *  Add a new palette at the end of the list.
 */

void Palette_edit::add_palette() {
	new_palette();
	cur_pal = palettes.size() - 1;    // Set to display new palette.
	Update_range_upper(palnum_adj, palettes.size() - 1);
	// This will update the display:
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
}

/*
 *  Remove the current palette.
 */

void Palette_edit::remove_palette() {
	// Don't delete the last one.
	if (cur_pal < 0 || palettes.size() < 2) {
		return;
	}
	if (Prompt("Do you really want to delete the palette you're viewing?",
			   "Yes", "No")
		!= 0) {
		return;
	}
	delete palettes[cur_pal];
	palettes.erase(palettes.begin() + cur_pal);
	flex_info->remove(cur_pal);
	flex_info->set_modified();
	if (static_cast<unsigned>(cur_pal) >= palettes.size()) {
		cur_pal = palettes.size() - 1;
	}
	Update_range_upper(palnum_adj, palettes.size() - 1);
	// This will update the display:
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
	render();    // Cur_pal may not have changed.
}

/*
 *  Export current palette.
 */

void Palette_edit::export_palette(const char* fname, gpointer user_data) {
	auto* paled = static_cast<Palette_edit*>(user_data);
	if (U7exists(fname)) {
		char* msg = g_strdup_printf("'%s' already exists.  Overwrite?", fname);
		const int answer = Prompt(msg, "Yes", "No");
		g_free(msg);
		if (answer != 0) {
			return;
		}
	}
	// Write out current palette.
	ExultRgbCmap* pal = paled->palettes[paled->cur_pal];
	ofstream      out(fname);    // OKAY that it's a 'text' file.
	out << "GIMP Palette" << endl;
	out << "# Exported from ExultStudio" << endl;
	int i;    // Skip 0's at end.
	for (i = 255; i > 0; i--) {
		if (pal->colors[i] != 0) {
			break;
		}
	}
	const int last_color = i;
	for (i = 0; i <= last_color; i++) {
		const int r = (pal->colors[i] >> 16) & 255;
		const int g = (pal->colors[i] >> 8) & 255;
		const int b = pal->colors[i] & 255;
		out << setw(3) << r << ' ' << setw(3) << g << ' ' << setw(3) << b
			<< endl;
	}
	out.close();
}

/*
 *  Import current palette.
 */

void Palette_edit::import_palette(const char* fname, gpointer user_data) {
	auto* paled = static_cast<Palette_edit*>(user_data);
	char* msg = g_strdup_printf("Overwrite current palette from '%s'?", fname);
	const int answer = Prompt(msg, "Yes", "No");
	g_free(msg);
	if (answer != 0) {
		return;
	}
	// Read in current palette.
	ExultRgbCmap* pal = paled->palettes[paled->cur_pal];
	ifstream      in(fname);    // OKAY that it's a 'text' file.
	char          buf[256];
	in.getline(buf, sizeof(buf));    // Skip 1st line.
	if (!in.good()) {
		Alert("Error reading '%s'", fname);
		return;
	}
	int i = 0;    // Color #.
	while (i < 256 && !in.eof()) {
		in.getline(buf, sizeof(buf));
		char* ptr = &buf[0];
		// Skip spaces.
		while (ptr < buf + sizeof(buf) && *ptr
			   && isspace(static_cast<unsigned char>(*ptr))) {
			ptr++;
		}
		if (*ptr == '#') {
			continue;    // Comment.
		}
		int r;
		int g;
		int b;
		if (sscanf(buf, "%d %d %d", &r, &g, &b) == 3) {
			pal->colors[i++] = (r << 16) + (g << 8) + b;
		}
	}
	in.close();
	// Add to file.
	paled->update_flex(paled->cur_pal);
	paled->render();
}
