/**
 ** Locator.cc - Locate game positions.
 **
 ** Written: March 2, 2002 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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

#include "locator.h"

#include "endianio.h"
#include "exult_constants.h"
#include "servemsg.h"

using std::cout;
using std::endl;

/*
 *  Open locator window.
 */

C_EXPORT void on_locator1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->open_locator_window();
}

void ExultStudio::open_locator_window() {
	if (!locwin) {    // First time?
		locwin = new Locator();
	}
	locwin->show(true);
}

/*
 *  Locator window's close button.
 */
C_EXPORT void on_loc_close_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	loc->show(false);
}

/*
 *  Locator window's X button.
 */
C_EXPORT gboolean on_loc_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(event, user_data);
	auto* loc = static_cast<Locator*>(
			g_object_get_data(G_OBJECT(widget), "user_data"));
	loc->show(false);
	return true;
}

/*
 *  Draw area created, or size changed.
 */
C_EXPORT gboolean on_loc_draw_configure_event(
		GtkWidget*         widget,    // The view window.
		GdkEventConfigure* event,
		gpointer           data    // ->Shape_chooser
) {
	ignore_unused_variable_warning(event, data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	loc->configure(widget);
	return true;
}

/*
 *  Draw area needs a repaint.
 */
gboolean Locator::on_loc_draw_expose_event(
		GtkWidget* widget,    // The view window.
		cairo_t* cairo, gpointer data) {
	ignore_unused_variable_warning(widget, data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	loc->set_graphic_context(cairo);
	GdkRectangle area = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	loc->render(&area);
	loc->set_graphic_context(nullptr);
	return true;
}

/*
 *  Mouse events in draw area.
 */
C_EXPORT gboolean on_loc_draw_button_press_event(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	return loc->mouse_press(event);
}

C_EXPORT gboolean on_loc_draw_button_release_event(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	return loc->mouse_release(event);
}

C_EXPORT gboolean on_loc_draw_motion_notify_event(
		GtkWidget*      widget,    // The view window.
		GdkEventMotion* event,
		gpointer        data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	auto* loc = static_cast<Locator*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	return loc->mouse_motion(event);
}

/*
 *  Create locator window.
 */

Locator::Locator() {
	ExultStudio* studio = ExultStudio::get_instance();
	win                 = studio->get_widget("loc_window");
	gtk_widget_set_size_request(win, 400, 400);
	g_object_set_data(G_OBJECT(win), "user_data", this);
	draw = studio->get_widget("loc_draw");
	// Indicate the events we want.
	gtk_widget_set_events(
			draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
						  | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON1_MOTION_MASK);
	// Set up scales.
	GtkWidget* scale = studio->get_widget("loc_hscale");
	hadj             = gtk_range_get_adjustment(GTK_RANGE(scale));
	scale            = studio->get_widget("loc_vscale");
	vadj             = gtk_range_get_adjustment(GTK_RANGE(scale));
	gtk_adjustment_set_upper(hadj, c_num_chunks);
	gtk_adjustment_set_upper(vadj, c_num_chunks);
	gtk_adjustment_set_page_increment(hadj, c_chunks_per_schunk);
	gtk_adjustment_set_page_increment(vadj, c_chunks_per_schunk);
	gtk_adjustment_set_page_size(hadj, 2);
	gtk_adjustment_set_page_size(vadj, 2);
	g_signal_emit_by_name(G_OBJECT(hadj), "changed");
	g_signal_emit_by_name(G_OBJECT(vadj), "changed");
	g_signal_connect(
			G_OBJECT(studio->get_widget("loc_draw")), "draw",
			G_CALLBACK(on_loc_draw_expose_event), this);
	// Set scrollbar handlers.
	g_signal_connect(
			G_OBJECT(hadj), "value-changed", G_CALLBACK(hscrolled), this);
	g_signal_connect(
			G_OBJECT(vadj), "value-changed", G_CALLBACK(vscrolled), this);
}

/*
 *  Delete.
 */

Locator::~Locator() {
	if (send_location_timer != -1) {
		g_source_remove(send_location_timer);
	}
	gtk_widget_destroy(win);
}

/*
 *  Show/hide.
 */

void Locator::show(bool tf) {
	if (tf) {
		gtk_widget_set_visible(win, true);
		query_location();    // Get Exult's loc.
	} else {
		gtk_widget_set_visible(win, false);
	}
}

/*
 *  Configure the draw window.
 */

void Locator::configure(GtkWidget* widget    // The draw window.
) {
	if (!gtk_widget_get_realized(widget)) {
		return;    // Not ready yet.
	}
	if (!drawgc) {    // First time?
	}
}

/*
 *  Display.
 */

void Locator::render(GdkRectangle* area    // nullptr for whole draw area.
) {
	// Get dims.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	const int    draww = alloc.width;
	const int    drawh = alloc.height;
	GdkRectangle all;
	if (!area) {
		all.x = all.y = 0;
		all.width     = draww;
		all.height    = drawh;
		area          = &all;
	}
	if (drawgc == nullptr) {
		gtk_widget_queue_draw(draw);
		return;
	}
	cairo_rectangle(drawgc, area->x, area->y, area->width, area->height);
	cairo_clip(drawgc);
	// Background is dark blue.
	cairo_set_source_rgb(drawgc, 0.0, 0.0, 64.0 / 255.0);
	cairo_rectangle(drawgc, area->x, area->y, area->width, area->height);
	cairo_fill(drawgc);
	// Show superchunks with dotted lines.
	// Paint in light grey.
	int i;
	int cur = 0;    // Cur. pixel.
	// First the rows.
	for (i = 0; i < c_num_schunks - 1; i++) {
		const int rowht = (drawh - cur) / (c_num_schunks - i);
		cur += rowht;
		cairo_set_source_rgb(
				drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
		cairo_move_to(drawgc, 0, cur);
		cairo_line_to(drawgc, draww, cur);
		cairo_stroke(drawgc);
		if (i == c_num_schunks / 2 - 1) {
			// Make middle one 3 pixels.
			cairo_set_source_rgb(
					drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
			cairo_move_to(drawgc, 0, cur - 1);
			cairo_line_to(drawgc, draww, cur - 1);
			cairo_stroke(drawgc);
			cairo_set_source_rgb(
					drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
			cairo_move_to(drawgc, 0, cur + 1);
			cairo_line_to(drawgc, draww, cur + 1);
			cairo_stroke(drawgc);
		}
	}
	cur = 0;    // Now the columns.
	for (i = 0; i < c_num_schunks - 1; i++) {
		const int colwd = (draww - cur) / (c_num_schunks - i);
		cur += colwd;
		cairo_set_source_rgb(
				drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
		cairo_move_to(drawgc, cur, 0);
		cairo_line_to(drawgc, cur, drawh);
		cairo_stroke(drawgc);
		if (i == c_num_schunks / 2 - 1) {
			// Make middle one 3 pixels.:
			cairo_set_source_rgb(
					drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
			cairo_move_to(drawgc, cur - 1, 0);
			cairo_line_to(drawgc, cur - 1, drawh);
			cairo_stroke(drawgc);
			cairo_set_source_rgb(
					drawgc, 192.0 / 255.0, 192.0 / 255.0, 192.0 / 255.0);
			cairo_move_to(drawgc, cur + 1, 0);
			cairo_line_to(drawgc, cur + 1, drawh);
			cairo_stroke(drawgc);
		}
	}
	// Figure where to draw box.
	const int cx = tx / c_tiles_per_chunk;
	const int cy = ty / c_tiles_per_chunk;
	const int x  = (cx * draww) / c_num_chunks;
	const int y  = (cy * drawh) / c_num_chunks;
	int       w  = (txs * draww) / c_num_tiles;
	int       h  = (tys * drawh) / c_num_tiles;
	if (w == 0) {
		w = 1;
	}
	if (h == 0) {
		h = 1;
	}
	// Draw location in yellow.
	cairo_set_source_rgb(drawgc, 255.0 / 255.0, 255.0 / 255.0, 0.0);
	cairo_rectangle(drawgc, x, y, w, h);
	cairo_stroke(drawgc);
	viewbox.x      = x;
	viewbox.y      = y;    // Save location.
	viewbox.width  = w;
	viewbox.height = h;
	// Put a light-red box around it.
	cairo_set_source_rgb(drawgc, 255.0 / 255.0, 128.0 / 255.0, 128.0 / 255.0);
	cairo_rectangle(drawgc, x - 3, y - 3, w + 6, h + 6);
	cairo_stroke(drawgc);
}

/*
 *  Message was received from Exult that the view has changed.
 */

void Locator::view_changed(
		const unsigned char* data,    // Tx, Ty, xtiles, ytiles, scale.
		int                  datalen) {
	if (datalen < 4 * 4) {
		return;    // Bad length.
	}
	if (dragging) {
		return;    //+++++++Not sure about this.
	}
	tx  = little_endian::Read4(data);
	ty  = little_endian::Read4(data);
	txs = little_endian::Read4(data);
	tys = little_endian::Read4(data);
	// ++++Scale?  Later.
	// Do things by chunk.
	const int cx = tx / c_tiles_per_chunk;
	const int cy = ty / c_tiles_per_chunk;
	tx           = cx * c_tiles_per_chunk;
	ty           = cy * c_tiles_per_chunk;
	// Update scrolls.
	gtk_adjustment_set_value(hadj, cx);
	gtk_adjustment_set_value(vadj, cy);
	render();
}

/*
 *  Handle a scrollbar event.
 */

void Locator::vscrolled(       // For vertical scrollbar.
		GtkAdjustment* adj,    // The adjustment.
		gpointer       data    // ->Shape_chooser.
) {
	auto*     loc   = static_cast<Locator*>(data);
	const int oldty = loc->ty;
	loc->ty         = static_cast<gint>(gtk_adjustment_get_value(adj))
			  * c_tiles_per_chunk;
	if (loc->ty != oldty)    // (Already equal if this event came
							 //   from Exult msg.).
	{
		loc->render();
		loc->send_location();
	}
}

void Locator::hscrolled(       // For horizontal scrollbar.
		GtkAdjustment* adj,    // The adjustment.
		gpointer       data    // ->Locator.
) {
	auto*     loc   = static_cast<Locator*>(data);
	const int oldtx = loc->tx;
	loc->tx         = static_cast<gint>(gtk_adjustment_get_value(adj))
			  * c_tiles_per_chunk;
	if (loc->tx != oldtx)    // (Already equal if this event came
							 //   from Exult msg.).
	{
		loc->render();
		loc->send_location();
	}
}

/*
 *  Send location to Exult.
 */

void Locator::send_location() {
	unsigned char  data[50];
	unsigned char* ptr = &data[0];
	little_endian::Write4(ptr, tx);
	little_endian::Write4(ptr, ty);
	little_endian::Write4(ptr, txs);
	little_endian::Write4(ptr, tys);
	little_endian::Write4(ptr, static_cast<uint32>(-1));    // Don't change.
	cout << "Locator::send_location" << endl;
	ExultStudio::get_instance()->send_to_server(
			Exult_server::view_pos, &data[0], ptr - data);
}

/*
 *  Query Exult for its location.
 */

void Locator::query_location() {
	unsigned char  data[50];
	unsigned char* ptr = &data[0];
	little_endian::Write4(ptr, static_cast<uint32>(-1));
	little_endian::Write4(ptr, static_cast<uint32>(-1));
	little_endian::Write4(ptr, static_cast<uint32>(-1));
	little_endian::Write4(ptr, static_cast<uint32>(-1));
	little_endian::Write4(ptr, static_cast<uint32>(-1));
	ExultStudio::get_instance()->send_to_server(
			Exult_server::view_pos, &data[0], ptr - data);
}

/*
 *  This is called a fraction of a second after mouse motion to send
 *  the new location to Exult if the mouse hasn't moved further.
 */

gint Locator::delayed_send_location(gpointer data    // ->locator.
) {
	auto* loc = static_cast<Locator*>(data);
	loc->send_location();
	loc->send_location_timer = -1;
	return 0;    // Cancels timer.
}

/*
 *  Go to a mouse location in the draw area.
 */

void Locator::goto_mouse(
		int mx, int my,    // Pixel coords. in draw area.
		bool delay_send    // Delay send_location for a bit.
) {
	const GdkRectangle oldbox = viewbox;    // Old location of box.
	const int          oldtx  = tx;
	const int          oldty  = ty;
	// Set tx,ty here so hscrolled() &
	//   vscrolled() don't send to Exult.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	if (alloc.width <= 0 || alloc.height <= 0) {
		return;    // Widget not ready yet.
	}
	tx     = (mx * c_num_tiles) / alloc.width;
	ty     = (my * c_num_tiles) / alloc.height;
	int cx = tx / c_tiles_per_chunk;
	int cy = ty / c_tiles_per_chunk;
	if (cx > c_num_chunks - 2) {
		cx = c_num_chunks - 2;
	}
	if (cy > c_num_chunks - 2) {
		cy = c_num_chunks - 2;
	}
	tx = cx * c_tiles_per_chunk;
	ty = cy * c_tiles_per_chunk;
	// Update scrolls.
	gtk_adjustment_set_value(hadj, cx);
	gtk_adjustment_set_value(vadj, cy);
	// Now we just send it once.
	if (tx != oldtx || ty != oldty) {
		if (send_location_timer != -1) {
			g_source_remove(send_location_timer);
		}
		if (delay_send) {    // Send in 1/3 sec. if no more motion.
			send_location_timer
					= g_timeout_add(333, delayed_send_location, this);
		} else {
			send_location();
			send_location_timer = -1;
		}
		GdkRectangle  newbox;    // Figure dirty rectangle;
		GdkRectangle  dirty;
		GtkAllocation alloc_1 = {0, 0, 0, 0};
		gtk_widget_get_allocation(draw, &alloc_1);
		newbox.x      = (cx * alloc_1.width) / c_num_chunks,
		newbox.y      = (cy * alloc_1.height) / c_num_chunks;
		newbox.width  = oldbox.width;
		newbox.height = oldbox.height;
		gdk_rectangle_union(&oldbox, &newbox, &dirty);
		// Expand a bit.
		dirty.x -= 4;
		dirty.y -= 4;
		dirty.width += 8;
		dirty.height += 8;
		render(&dirty);
	}
}

/*
 *  Handle a mouse-press event.
 */

gboolean Locator::mouse_press(GdkEventButton* event) {
	dragging = false;
	if (event->button != 1) {
		return false;    // Handling left-click.
	}
	// Get mouse position, draw dims.
	const int mx = static_cast<int>(event->x);
	const int my = static_cast<int>(event->y);
	// Double-click?
	if (reinterpret_cast<GdkEvent*>(event)->type == GDK_2BUTTON_PRESS) {
		goto_mouse(mx, my);
		return true;
	}
	// On (or close to) view box?
	if (mx < viewbox.x - 3 || my < viewbox.y - 3
		|| mx > viewbox.x + viewbox.width + 6
		|| my > viewbox.y + viewbox.height + 6) {
		return false;
	}
	dragging  = true;
	drag_relx = mx - viewbox.x;    // Save rel. pos.
	drag_rely = my - viewbox.y;
	return true;
}

/*
 *  Handle a mouse-release event.
 */

gboolean Locator::mouse_release(GdkEventButton* event) {
	ignore_unused_variable_warning(event);
	dragging = false;
	return true;
}

/*
 *  Handle a mouse-motion event.
 */

gboolean Locator::mouse_motion(GdkEventMotion* event) {
	int             mx;
	int             my;
	GdkModifierType state;
	mx    = static_cast<int>(event->x);
	my    = static_cast<int>(event->y);
	state = static_cast<GdkModifierType>(event->state);
	if (!dragging || !(state & GDK_BUTTON1_MASK)) {
		return false;    // Not dragging with left button.
	}
	// Delay sending location to Exult.
	goto_mouse(mx - drag_relx, my - drag_rely, true);
	return true;
}
