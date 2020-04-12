/**
 ** A GTK widget showing a palette's colors.
 **
 ** Written: 12/24/2000 - JSF
 **/

/*
Copyright (C) 2000 The Exult Team

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
#  include <config.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wparentheses"
#if !defined(__llvm__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuseless-cast"
#else
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif  // __GNUC__
#include <gtk/gtk.h>
#ifdef XWIN
#include <gdk/gdkx.h>
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__
#include "gtk_redefines.h"

#include <glib.h>
#include "paledit.h"
#include "u7drag.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "studio.h"
#include "shapefile.h"
#include "ignore_unused_variable_warning.h"

using   std::cout;
using   std::endl;
using   std::string;
using   std::vector;
using   std::ostream;
using   std::ofstream;
using   std::setw;
using   std::ifstream;
using   std::make_unique;
using   EStudio::Prompt;
using   EStudio::Alert;

/*
 *  Write out a single palette to a buffer.
 */

static void Write_palette(
    unsigned char *buf,     // 3*256 bytes.
    GdkRgbCmap *pal         // Palette to write.
) {
	for (int i = 0; i < 256; i++) {
		int r = (pal->colors[i] >> 16) & 255;
		int g = (pal->colors[i] >> 8) & 255;
		int b = pal->colors[i] & 255;
		buf[3 * i] = r / 4; // Range 0-63.
		buf[3 * i + 1] = g / 4;
		buf[3 * i + 2] = b / 4;
	}
}

/*
 *  Blit onto screen.
 */

inline void Palette_edit::show(
    int x, int y, int w, int h  // Area to blit.
) {
	int stride = draw->allocation.width;
	gdk_draw_indexed_image(draw->window, drawgc, x, y, w, h,
	                       GDK_RGB_DITHER_NORMAL,
	                       image + y * stride + x,
	                       stride, palettes[cur_pal]);
	if (selected >= 0)      // Show selected.
		// Draw yellow box.
		gdk_draw_rectangle(draw->window, drawgc, FALSE,
		                   selected_box.x, selected_box.y,
		                   selected_box.w, selected_box.h);
}

/*
 *  Select an entry.  This should be called after rendering
 *  the shape.
 */

void Palette_edit::select(
    int new_sel
) {
	selected = new_sel;
	// Remove prev. selection msg.
	gtk_statusbar_pop(GTK_STATUSBAR(sbar), sbar_sel);
	char buf[150];          // Show new selection.
	g_snprintf(buf, sizeof(buf), "Color %d (0x%02x)", new_sel, new_sel);
	gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
}

/*
 *  Load/reload from file.
 */

void Palette_edit::load(
) {
	// Free old.
	for (vector<GdkRgbCmap *>::iterator it = palettes.begin();
	        it != palettes.end(); ++it)
		gdk_rgb_cmap_free(*it);
	unsigned cnt = flex_info->size();
	palettes.resize(cnt);       // Set size of list.
	if (!cnt)           // No palettes?
		new_palette();      // Create 1 blank palette.
	else {
		for (unsigned pnum = 0; pnum < cnt; pnum++) {
			size_t len;
			unsigned char *buf = flex_info->get(pnum, len);
			guint32 colors[256];
			for (size_t i = 0; i < len / 3; i++)
				colors[i] = (buf[3 * i] << 16) * 4 +
				            (buf[3 * i + 1] << 8) * 4 + buf[3 * i + 2] * 4;
			for (size_t i = len / 3; i < 256; i++)
				colors[i] = 0;
			palettes[pnum] = gdk_rgb_cmap_new(colors, 256);
		}
	}
}

/*
 *  Draw the palette.  This need only be called when it changes.
 */

void Palette_edit::render(
) {
	int neww = draw->allocation.width;
	int newh = draw->allocation.height;
	// Changed size?
	if (neww != width || newh != height) {
		delete [] image;
		width = neww;
		height = newh;
		image = new guchar[width * height];
	}
	// Figure cell size.
	int eachw = width / 16;
	int eachh = height / 16;
	// Figure extra pixels.
	int extraw = width % 16;
	int extrah = height % 16;
	int color = 0;          // Color index.
	int cury = 0;
	unsigned char *out = image;
	for (int y = 0; y < 16; y++, color += 16) {
		int stopy = cury + eachh + (y < extrah);
		for (; cury < stopy; cury++)
			for (int x = 0, c = color; x < 16; x++, c++) {
				int cntx = eachw + (x < extraw);
				while (cntx--)
					*out++ = c;
			}
	}
	if (selected >= 0) {    // Update selected box.
		int selx = selected % 16;
		int sely = selected / 16;
		selected_box.x = selx * eachw;
		selected_box.y = sely * eachh;
		selected_box.w = eachw;
		selected_box.h = eachh;
		if (selx < extraw) { // Watch for extra pixels.
			selected_box.w++;
			selected_box.x += selx;
		} else
			selected_box.x += extraw;
		if (sely < extrah) {
			selected_box.h++;
			selected_box.y += sely;
		} else
			selected_box.y += extrah;
		select(selected);
	}
}

/*
 *  Color box was closed.
 */

int Palette_edit::color_closed(
    GtkWidget *dlg,
    GdkEvent *event,
    gpointer data
) {
	ignore_unused_variable_warning(dlg, event);
	cout << "color_closed" << endl;
	Palette_edit *paled = static_cast<Palette_edit *>(data);
	paled->colorsel = nullptr;
	return FALSE;
}

/*
 *  'Cancel' was hit in the color selector.
 */

void Palette_edit::color_cancel(
    GtkWidget *dlg,
    gpointer data
) {
	ignore_unused_variable_warning(dlg);
	Palette_edit *paled = static_cast<Palette_edit *>(data);
	if (paled->colorsel)
		gtk_widget_destroy(GTK_WIDGET(paled->colorsel));
	paled->colorsel = nullptr;
}

/*
 *  'Okay' was hit in the color selector.
 */

void Palette_edit::color_okay(
    GtkWidget *dlg,
    gpointer data
) {
	ignore_unused_variable_warning(dlg);
	Palette_edit *paled = static_cast<Palette_edit *>(data);
	if (paled->colorsel) {
		gdouble rgb[4];
		gtk_color_selection_get_color(
		    GTK_COLOR_SELECTION(paled->colorsel->colorsel), rgb);
		unsigned char r = static_cast<unsigned char>(rgb[0] * 256);
		unsigned char g = static_cast<unsigned char>(rgb[1] * 256);
		unsigned char b = static_cast<unsigned char>(rgb[2] * 256);
		if (paled->selected >= 0)
			paled->palettes[paled->cur_pal]->colors[
			    paled->selected] =
			        (r << 16) + (g << 8) + b;
		gtk_widget_destroy(GTK_WIDGET(paled->colorsel));
		// Send to flex file.
		paled->update_flex(paled->cur_pal);
		paled->render();
		paled->show();
	}
	paled->colorsel = nullptr;
}

/*
 *  Handle double-click on a color by bringing up a color-selector.
 */

void Palette_edit::double_clicked(
) {
	cout << "Double-clicked" << endl;
	if (selected < 0 || colorsel)   // Only one at a time.
		return;         // Nothing selected.
	char buf[150];          // Show new selection.
	g_snprintf(buf, sizeof(buf), "Color %d (0x%02x)", selected, selected);
	colorsel = GTK_COLOR_SELECTION_DIALOG(
	               gtk_color_selection_dialog_new(buf));
	// Set mouse click handler.
	gtk_signal_connect(GTK_OBJECT(colorsel->ok_button), "clicked",
	                   GTK_SIGNAL_FUNC(color_okay), this);
	gtk_signal_connect(GTK_OBJECT(colorsel->cancel_button), "clicked",
	                   GTK_SIGNAL_FUNC(color_cancel), this);
	// Set delete handler.
	gtk_signal_connect(GTK_OBJECT(colorsel), "delete_event",
	                   GTK_SIGNAL_FUNC(color_closed), this);
	// Get color.
	guint32 c = palettes[cur_pal]->colors[selected];
	gdouble rgb[3];
	rgb[0] = ((c >> 16) & 0xff) / 256.0;
	rgb[1] = ((c >>  8) & 0xff) / 256.0;
	rgb[2] = ((c >>  0) & 0xff) / 256.0;
	gtk_color_selection_set_color(GTK_COLOR_SELECTION(colorsel->colorsel),
	                              rgb);
	gtk_widget_show(GTK_WIDGET(colorsel));
}

/*
 *  Configure the viewing window.
 */

gint Palette_edit::configure(
    GtkWidget *widget,      // The view window.
    GdkEventConfigure *event,
    gpointer data           // ->Palette_edit
) {
	ignore_unused_variable_warning(event);
	Palette_edit *paled = static_cast<Palette_edit *>(data);
	if (!paled->width) {    // First time?
		paled->drawgc = gdk_gc_new(widget->window);
		// Foreground = yellow.
		gdk_rgb_gc_set_foreground(paled->drawgc,
		                          (255 << 16) + (255 << 8));
	}
	paled->render();
	return TRUE;
}

/*
 *  Handle an expose event.
 */

gint Palette_edit::expose(
    GtkWidget *widget,      // The view window.
    GdkEventExpose *event,
    gpointer data           // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget);
	Palette_edit *paled = static_cast<Palette_edit *>(data);
	paled->show(event->area.x, event->area.y, event->area.width,
	            event->area.height);
	return TRUE;
}

/*
 *  Handle a mouse button press event.
 */

gint Palette_edit::mouse_press(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event,
    gpointer data           // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget);
	Palette_edit *paled = static_cast<Palette_edit *>(data);

	if (event->button == 4 || event->button == 5) // mouse wheel
		return TRUE;

	if (paled->colorsel)
		return TRUE;      // Already editing a color.
	int old_selected = paled->selected;
	int width = paled->width;
	int height = paled->height;
	int eventx = static_cast<int>(event->x);
	int eventy = static_cast<int>(event->y);
	// Figure cell size.
	int eachw = width / 16;
	int eachh = height / 16;
	// Figure extra pixels.
	int extraw = width % 16;
	int extrah = height % 16;
	int extrax = extraw * (eachw + 1); // Total length of extra-sized boxes.
	int extray = extrah * (eachh + 1);
	int selx;
	int sely;         // Gets box indices.
	if (eventx < extrax)
		selx = eventx / (eachw + 1);
	else
		selx = extraw + (eventx - extrax) / eachw;
	if (eventy < extray)
		sely = eventy / (eachh + 1);
	else
		sely = extrah + (eventy - extray) / eachh;
	paled->selected = sely * 16 + selx;
	if (paled->selected == old_selected) {
		// Same square.  Check for dbl-click.
		if (reinterpret_cast<GdkEvent *>(event)->type == GDK_2BUTTON_PRESS)
			paled->double_clicked();
	} else {
		paled->render();
		paled->show();
	}
	if (event->button == 3)
		gtk_menu_popup(GTK_MENU(paled->create_popup()),
		               nullptr, nullptr, nullptr, nullptr, event->button, event->time);
	return TRUE;
}

/*
 *  Someone wants the dragged shape.
 */

void Palette_edit::drag_data_get(
    GtkWidget *widget,      // The view window.
    GdkDragContext *context,
    GtkSelectionData *seldata,  // Fill this in.
    guint info,
    guint time,
    gpointer data           // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget, context, seldata, info, time, data);
	cout << "In DRAG_DATA_GET" << endl;
}

/*
 *  Another app. has claimed the selection.
 */

gint Palette_edit::selection_clear(
    GtkWidget *widget,      // The view window.
    GdkEventSelection *event,
    gpointer data           // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget, event, data);
//	Palette_edit *paled = static_cast<Palette_edit *>(data);
	cout << "SELECTION_CLEAR" << endl;
	return TRUE;
}

/*
 *  Beginning of a drag.
 */

gint Palette_edit::drag_begin(
    GtkWidget *widget,      // The view window.
    GdkDragContext *context,
    gpointer data           // ->Palette_edit.
) {
	ignore_unused_variable_warning(widget, context, data);
	cout << "In DRAG_BEGIN" << endl;
	//Palette_edit *paled = static_cast<Palette_edit *>(data);
	// Maybe someday.
	return TRUE;
}

/*
 *  Handle a change to the 'Palette #' spin button.
 */

void Palette_edit::palnum_changed(
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Shape_chooser.
) {
	Palette_edit *ed = static_cast<Palette_edit *>(data);
	gint newnum = static_cast<gint>(adj->value);
	ed->show_palette(newnum);
	ed->render();
	ed->show();
	ed->enable_controls();
}

/*
 *  Callbacks for buttons:
 */
void
on_exportbtn_clicked(GtkButton       *button,
                     gpointer         user_data) {
	ignore_unused_variable_warning(button);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Export palette to text format",
	                             reinterpret_cast<File_sel_okay_fun>(Palette_edit::export_palette),
	                             user_data);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}

void
on_importbtn_clicked(GtkButton       *button,
                     gpointer         user_data) {
	ignore_unused_variable_warning(button);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Import palette from text format",
	                             reinterpret_cast<File_sel_okay_fun>(Palette_edit::import_palette),
	                             user_data);
	gtk_widget_show(GTK_WIDGET(fsel));
}

void
on_insert_btn_clicked(GtkButton       *button,
                      gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	ed->add_palette();
}
void
on_remove_btn_clicked(GtkButton       *button,
                      gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	ed->remove_palette();
}
void
on_up_btn_clicked(GtkButton       *button,
                  gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	ed->move_palette(true);
}
void
on_down_btn_clicked(GtkButton       *button,
                    gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	ed->move_palette(false);
}

/*
 *  Create box with 'Palette #', 'Import', 'Move' controls.
 */

GtkWidget *Palette_edit::create_controls(
) {
	// Create main box.
	GtkWidget *topframe = gtk_frame_new(nullptr);
	gtk_widget_show(topframe);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(topframe), vbox);

	GtkWidget *hbox0 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox0, TRUE, TRUE, 2);
	/*
	 *  The 'Edit' controls.
	 */
	GtkWidget *frame = gtk_frame_new("Edit");
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, FALSE, FALSE, 2);
	GtkWidget *hbuttonbox = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox);
	gtk_container_add(GTK_CONTAINER(frame), hbuttonbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox),
	                          GTK_BUTTONBOX_START);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbuttonbox), 0);

	insert_btn = gtk_button_new_with_label("New");
	gtk_widget_show(insert_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), insert_btn);
	GTK_WIDGET_SET_FLAGS(insert_btn, GTK_CAN_DEFAULT);

	remove_btn = gtk_button_new_with_label("Remove");
	gtk_widget_show(remove_btn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), remove_btn);
	GTK_WIDGET_SET_FLAGS(remove_btn, GTK_CAN_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(insert_btn), "clicked",
	                   GTK_SIGNAL_FUNC(on_insert_btn_clicked),
	                   this);
	gtk_signal_connect(GTK_OBJECT(remove_btn), "clicked",
	                   GTK_SIGNAL_FUNC(on_remove_btn_clicked),
	                   this);
	/*
	 *  The 'Move' controls.
	 */
	frame = gtk_frame_new("Move");
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, FALSE, FALSE, 2);
	GtkWidget *bbox = gtk_hbox_new(TRUE, 0);
	gtk_widget_show(bbox);
	gtk_container_add(GTK_CONTAINER(frame), bbox);
	down_btn = gtk_button_new();
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(bbox), down_btn, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(down_btn, GTK_CAN_DEFAULT);
	GtkWidget *arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_container_add(GTK_CONTAINER(down_btn), arrow);

	up_btn = gtk_button_new();
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(bbox), up_btn, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(up_btn, GTK_CAN_DEFAULT);
	arrow = gtk_arrow_new(GTK_ARROW_UP, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_container_add(GTK_CONTAINER(up_btn), arrow);
	gtk_signal_connect(GTK_OBJECT(down_btn), "clicked",
	                   GTK_SIGNAL_FUNC(on_down_btn_clicked),
	                   this);
	gtk_signal_connect(GTK_OBJECT(up_btn), "clicked",
	                   GTK_SIGNAL_FUNC(on_up_btn_clicked),
	                   this);
	/*
	 *  The 'File' controls.
	 */
	frame = gtk_frame_new("File");
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(hbox0), frame, FALSE, FALSE, 2);
	hbuttonbox = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox);
	gtk_container_add(GTK_CONTAINER(frame), hbuttonbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox),
	                          GTK_BUTTONBOX_START);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbuttonbox), 0);

	GtkWidget *importbtn = gtk_button_new_with_label("Import");
	gtk_widget_show(importbtn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), importbtn);
	GTK_WIDGET_SET_FLAGS(importbtn, GTK_CAN_DEFAULT);

	GtkWidget *exportbtn = gtk_button_new_with_label("Export");
	gtk_widget_show(exportbtn);
	gtk_container_add(GTK_CONTAINER(hbuttonbox), exportbtn);
	GTK_WIDGET_SET_FLAGS(exportbtn, GTK_CAN_DEFAULT);
	gtk_signal_connect(GTK_OBJECT(importbtn), "clicked",
	                   GTK_SIGNAL_FUNC(on_importbtn_clicked),
	                   this);
	gtk_signal_connect(GTK_OBJECT(exportbtn), "clicked",
	                   GTK_SIGNAL_FUNC(on_exportbtn_clicked),
	                   this);
	return topframe;
}

/*
 *  Enable/disable controls after changes.
 */

void Palette_edit::enable_controls(
) {
	// Can't delete last one.
	gtk_widget_set_sensitive(remove_btn, cur_pal >= 0 &&
	                         palettes.size() > 1);
	if (cur_pal == -1) {    // No palette?
		gtk_widget_set_sensitive(down_btn, false);
		gtk_widget_set_sensitive(up_btn, false);
		gtk_widget_set_sensitive(remove_btn, false);
	} else {
		gtk_widget_set_sensitive(down_btn,
		                         static_cast<unsigned>(cur_pal) < palettes.size() - 1);
		gtk_widget_set_sensitive(up_btn, cur_pal > 0);
		gtk_widget_set_sensitive(remove_btn, palettes.size() > 1);
	}
}

/*
 *  Set up box.
 */

void Palette_edit::setup(
) {
	// Put things in a vert. box.
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	set_widget(vbox);
	// A frame looks nice.
	GtkWidget *frame = gtk_frame_new(nullptr);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	draw = gtk_drawing_area_new();  // Create drawing area window.
//	gtk_drawing_area_size(GTK_DRAWING_AREA(draw), w, h);
	// Indicate the events we want.
	gtk_widget_set_events(draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
	                      | GDK_POINTER_MOTION_HINT_MASK |
	                      GDK_BUTTON1_MOTION_MASK);
	// Set "configure" handler.
	gtk_signal_connect(GTK_OBJECT(draw), "configure_event",
	                   GTK_SIGNAL_FUNC(configure), this);
	// Set "expose" handler.
	gtk_signal_connect(GTK_OBJECT(draw), "expose_event",
	                   GTK_SIGNAL_FUNC(expose), this);
	// Set mouse click handler.
	gtk_signal_connect(GTK_OBJECT(draw), "button_press_event",
	                   GTK_SIGNAL_FUNC(mouse_press), this);
	// Mouse motion.
	gtk_signal_connect(GTK_OBJECT(draw), "drag_begin",
	                   GTK_SIGNAL_FUNC(drag_begin), this);
	gtk_signal_connect(GTK_OBJECT(draw), "drag_data_get",
	                   GTK_SIGNAL_FUNC(drag_data_get), this);
	gtk_signal_connect(GTK_OBJECT(draw), "selection_clear_event",
	                   GTK_SIGNAL_FUNC(selection_clear), this);
	gtk_container_add(GTK_CONTAINER(frame), draw);
	gtk_widget_show(draw);
	// At bottom, a status bar.
	sbar = gtk_statusbar_new();
	sbar_sel = gtk_statusbar_get_context_id(GTK_STATUSBAR(sbar),
	                                        "selection");
	// At the bottom, status bar & frame:
	GtkWidget *hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(hbox1), sbar, TRUE, TRUE, 0);
	// Palette # to right of sbar.
	GtkWidget *label = gtk_label_new("Palette #:");
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 4);
	gtk_widget_show(label);
	// A spin button for palette#.
	palnum_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0,
	                            palettes.size() - 1, 1,
	                            2, 2));
	pspin = gtk_spin_button_new(palnum_adj, 1, 0);
	gtk_signal_connect(GTK_OBJECT(palnum_adj), "value_changed",
	                   GTK_SIGNAL_FUNC(palnum_changed), this);
	gtk_box_pack_start(GTK_BOX(hbox1), pspin, FALSE, FALSE, 0);
	gtk_widget_show(pspin);

	// Add edit controls to bottom.
	gtk_box_pack_start(GTK_BOX(vbox), create_controls(), FALSE, FALSE, 0);
	gtk_widget_show(sbar);
	enable_controls();
}

/*
 *  Create/add a new palette.
 */

void Palette_edit::new_palette(
) {
	guint32 colors[256];        // R, G, B, then all black.
	memset(&colors[0], 0, sizeof(colors));
	colors[0] = 255 << 16;
	colors[1] = 255 << 8;
	colors[2] = 255;
	GdkRgbCmap *newpal = gdk_rgb_cmap_new(colors, 256);
	int index = palettes.size();    // Index of new palette.
	palettes.push_back(newpal);
	update_flex(index);     // Add to file.
}

/*
 *  Update palette entry in flex file.
 */

void Palette_edit::update_flex(
    int pnum            // Palette # to send to file.
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

Palette_edit::Palette_edit(
    Flex_file_info *flinfo      // Flex-file info.
) : Object_browser(nullptr, flinfo),
	flex_info(flinfo), image(nullptr), width(0), height(0),
	cur_pal(0), colorsel(nullptr) {
	load();             // Load from file.
	setup();
}

/*
 *  Delete.
 */

Palette_edit::~Palette_edit(
) {
	for (vector<GdkRgbCmap *>::iterator it = palettes.begin();
	        it != palettes.end(); ++it)
		gdk_rgb_cmap_free(*it);
	gtk_widget_destroy(get_widget());
	delete [] image;
}

/*
 *  Get i'th palette.
 */

void Palette_edit::show_palette(
    int palnum
) {
	cur_pal = palnum;
}

/*
 *  Unselect.
 */

void Palette_edit::unselect(
    bool need_render            // 1 to render and show.
) {
	if (selected >= 0) {
		selected = -1;
		if (need_render) {
			render();
			show();
		}
	}
}

/*
 *  Move a palette within the list.
 */

void Palette_edit::move_palette(
    bool up
) {
	if (cur_pal < 0)
		return;
	GdkRgbCmap *tmp;
	if (up) {
		if (cur_pal > 0) {
			tmp = palettes[cur_pal - 1];
			palettes[cur_pal - 1] = palettes[cur_pal];
			palettes[cur_pal] = tmp;
			cur_pal--;
			flex_info->swap(cur_pal);// Update flex-file list.
		}
	} else {
		if (static_cast<unsigned>(cur_pal) < palettes.size() - 1) {
			tmp = palettes[cur_pal + 1];
			palettes[cur_pal + 1] = palettes[cur_pal];
			palettes[cur_pal] = tmp;
			flex_info->swap(cur_pal);// Update flex-file list.
			cur_pal++;
		}
	}
	flex_info->set_modified();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
}

/*
 *  Update upper bound of a range widget.
 */

void Update_range_upper(
    GtkAdjustment *adj,
    int new_upper
) {
	adj->upper = new_upper;
	gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
}

/*
 *  Add a new palette at the end of the list.
 */

void Palette_edit::add_palette(
) {
	new_palette();
	cur_pal = palettes.size() - 1;  // Set to display new palette.
	Update_range_upper(palnum_adj, palettes.size() - 1);
	// This will update the display:
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
}

/*
 *  Remove the current palette.
 */

void Palette_edit::remove_palette(
) {
	// Don't delete the last one.
	if (cur_pal < 0 || palettes.size() < 2)
		return;
	if (Prompt(
	            "Do you really want to delete the palette you're viewing?",
	            "Yes", "No") != 0)
		return;
	gdk_rgb_cmap_free(palettes[cur_pal]);
	palettes.erase(palettes.begin() + cur_pal);
	flex_info->remove(cur_pal);
	flex_info->set_modified();
	if (static_cast<unsigned>(cur_pal) >= palettes.size())
		cur_pal = palettes.size() - 1;
	Update_range_upper(palnum_adj, palettes.size() - 1);
	// This will update the display:
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(pspin), cur_pal);
	render();           // Cur_pal may not have changed.
	show();
}

/*
 *  Export current palette.
 */

void Palette_edit::export_palette(
    char *fname,
    gpointer user_data
) {
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	if (U7exists(fname)) {
		char *msg = g_strdup_printf(
		                "'%s' already exists.  Overwrite?", fname);
		int answer = Prompt(msg, "Yes", "No");
		g_free(msg);
		if (answer != 0)
			return;
	}
	// Write out current palette.
	GdkRgbCmap *pal = ed->palettes[ed->cur_pal];
	ofstream out(fname);        // OKAY that it's a 'text' file.
	out << "GIMP Palette" << endl;
	out << "# Exported from ExultStudio" << endl;
	int i;              // Skip 0's at end.
	for (i = 255; i > 0; i--)
		if (pal->colors[i] != 0)
			break;
	int last_color = i;
	for (i = 0; i <= last_color; i++) {
		int r = (pal->colors[i] >> 16) & 255;
		int g = (pal->colors[i] >> 8) & 255;
		int b = pal->colors[i] & 255;
		out << setw(3) << r << ' ' << setw(3) << g << ' ' <<
		    setw(3) << b << endl;
	}
	out.close();
}

/*
 *  Import current palette.
 */

void Palette_edit::import_palette(
    char *fname,
    gpointer user_data
) {
	Palette_edit *ed = static_cast<Palette_edit *>(user_data);
	char *msg = g_strdup_printf(
	                "Overwrite current palette from '%s'?", fname);
	int answer = Prompt(msg, "Yes", "No");
	g_free(msg);
	if (answer != 0)
		return;
	// Read in current palette.
	GdkRgbCmap *pal = ed->palettes[ed->cur_pal];
	ifstream in(fname);     // OKAY that it's a 'text' file.
	char buf[256];
	in.getline(buf, sizeof(buf));   // Skip 1st line.
	if (!in.good()) {
		Alert("Error reading '%s'", fname);
		return;
	}
	int i = 0;          // Color #.
	while (i < 256 && !in.eof()) {
		in.getline(buf, sizeof(buf));
		char *ptr = &buf[0];
		// Skip spaces.
		while (ptr < buf + sizeof(buf) && *ptr && isspace(static_cast<unsigned char>(*ptr)))
			ptr++;
		if (*ptr == '#')
			continue;   // Comment.
		int r;
		int g;
		int b;
		if (sscanf(buf, "%d %d %d", &r, &g, &b) == 3)
			pal->colors[i++] = (r << 16) + (g << 8) + b;
	}
	in.close();
	// Add to file.
	ed->update_flex(ed->cur_pal);
	ed->render();
	ed->show();
}

