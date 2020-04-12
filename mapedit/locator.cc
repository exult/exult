/**
 ** Locator.cc - Locate game positions.
 **
 ** Written: March 2, 2002 - JSF
 **/

/*
Copyright (C) 2001-2013 The Exult Team

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

#include "studio.h"
#include "servemsg.h"
#include "exult_constants.h"
#include "locator.h"
#include "utils.h"
#include "ignore_unused_variable_warning.h"

using   std::cout;
using   std::endl;

/*
 *  Open locator window.
 */

C_EXPORT void on_locator1_activate(
    GtkMenuItem     *menuitem,
    gpointer         user_data
) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->open_locator_window();
}
void ExultStudio::open_locator_window(
) {
	if (!locwin) {          // First time?
		locwin = new Locator();
	}
	locwin->show(true);
}

/*
 *  Locator window's close button.
 */
C_EXPORT void on_loc_close_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn)))));
	loc->show(false);
}

/*
 *  Locator window's X button.
 */
C_EXPORT gboolean on_loc_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(event, user_data);
	Locator *loc = static_cast<Locator *>(
	               gtk_object_get_user_data(GTK_OBJECT(widget)));
	loc->show(false);
	return TRUE;
}

/*
 *  Draw area created, or size changed.
 */
C_EXPORT gint on_loc_draw_configure_event(
    GtkWidget *widget,      // The view window.
    GdkEventConfigure *event,
    gpointer data           // ->Shape_chooser
) {
	ignore_unused_variable_warning(event, data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget)))));
	loc->configure(widget);
	return TRUE;
}
/*
 *  Draw area needs a repain.
 */
C_EXPORT gint on_loc_draw_expose_event(
    GtkWidget *widget,      // The view window.
    GdkEventExpose *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget)))));
	loc->render(&event->area);
	return TRUE;
}
/*
 *  Mouse events in draw area.
 */
C_EXPORT gint on_loc_draw_button_press_event(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event,
    gpointer data           // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget)))));
	return loc->mouse_press(event);
}

C_EXPORT gint on_loc_draw_button_release_event(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event,
    gpointer data           // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget)))));
	return loc->mouse_release(event);
}

C_EXPORT gint on_loc_draw_motion_notify_event(
    GtkWidget *widget,      // The view window.
    GdkEventMotion *event,
    gpointer data           // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(data);
	Locator *loc = static_cast<Locator *>(gtk_object_get_user_data(
	                   GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget)))));
	return loc->mouse_motion(event);
}

/*
 *  Create locator window.
 */

Locator::Locator() {
	GladeXML *app_xml = ExultStudio::get_instance()->get_xml();
	win = glade_xml_get_widget(app_xml, "loc_window");
	gtk_object_set_user_data(GTK_OBJECT(win), this);
	draw = glade_xml_get_widget(app_xml, "loc_draw");
	// Indicate the events we want.
	gtk_widget_set_events(draw, GDK_EXPOSURE_MASK |
	                      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	                      GDK_BUTTON1_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	// Set up scales.
	GtkWidget *scale = glade_xml_get_widget(app_xml, "loc_hscale");
	hadj = gtk_range_get_adjustment(GTK_RANGE(scale));
	scale = glade_xml_get_widget(app_xml, "loc_vscale");
	vadj = gtk_range_get_adjustment(GTK_RANGE(scale));
	hadj->upper = vadj->upper = c_num_chunks;
	hadj->page_increment = vadj->page_increment = c_chunks_per_schunk;
	hadj->page_size = vadj->page_size = 2;
	gtk_signal_emit_by_name(GTK_OBJECT(hadj), "changed");
	gtk_signal_emit_by_name(GTK_OBJECT(vadj), "changed");
	// Set scrollbar handlers.
	gtk_signal_connect(GTK_OBJECT(hadj), "value_changed",
	                   GTK_SIGNAL_FUNC(hscrolled), this);
	gtk_signal_connect(GTK_OBJECT(vadj), "value_changed",
	                   GTK_SIGNAL_FUNC(vscrolled), this);
}

/*
 *  Delete.
 */

Locator::~Locator(
) {
	if (send_location_timer != -1)
		gtk_timeout_remove(send_location_timer);
	gtk_widget_destroy(win);
}

/*
 *  Show/hide.
 */

void Locator::show(
    bool tf
) {
	if (tf) {
		gtk_widget_show(win);
		query_location();   // Get Exult's loc.
	} else
		gtk_widget_hide(win);
}

/*
 *  Configure the draw window.
 */

void Locator::configure(
    GtkWidget *widget       // The draw window.
) {
	if (!widget->window)
		return;         // Not ready yet.
	if (!drawgc) {          // First time?
		drawgc = gdk_gc_new(widget->window);
	}
}

/*
 *  Display.
 */

void Locator::render(
    GdkRectangle *area      // nullptr for whole draw area.
) {
	// Get dims.
	int draww = draw->allocation.width;
	int drawh = draw->allocation.height;
	GdkRectangle all;
	if (!area) {
		all.x = all.y = 0;
		all.width = draww;
		all.height = drawh;
		area = &all;
	}
	gdk_gc_set_clip_rectangle(drawgc, area);
	// Background is dark blue.
	gdk_rgb_gc_set_foreground(drawgc, 64);
	gdk_draw_rectangle(draw->window, drawgc, TRUE, area->x, area->y,
	                   area->width, area->height);
	// Show superchunks with dotted lines.
	// Paint in light grey.
	gdk_rgb_gc_set_foreground(drawgc, 0xc0c0c0);
	int i;
	int cur = 0;            // Cur. pixel.
	// First the rows.
	for (i = 0; i < c_num_schunks - 1; i++) {
		int rowht = (drawh - cur) / (c_num_schunks - i);
		cur += rowht;
		gdk_draw_line(draw->window, drawgc, 0, cur, draww, cur);
		if (i == c_num_schunks / 2 - 1) {
			// Make middle one 3 pixels.
			gdk_draw_line(draw->window, drawgc, 0, cur - 1,
			              draww, cur - 1);
			gdk_draw_line(draw->window, drawgc, 0, cur + 1,
			              draww, cur + 1);
		}
	}
	cur = 0;            // Now the columns.
	for (i = 0; i < c_num_schunks - 1; i++) {
		int colwd = (draww - cur) / (c_num_schunks - i);
		cur += colwd;
		gdk_draw_line(draw->window, drawgc, cur, 0, cur, drawh);
		if (i == c_num_schunks / 2 - 1) {
			// Make middle one 3 pixels.
			gdk_draw_line(draw->window, drawgc, cur - 1, 0,
			              cur - 1, drawh);
			gdk_draw_line(draw->window, drawgc, cur + 1, 0,
			              cur + 1, drawh);
		}
	}
	// Figure where to draw box.
	int cx = tx / c_tiles_per_chunk;
	int cy = ty / c_tiles_per_chunk;
	int x = (cx * draww) / c_num_chunks;
	int y = (cy * drawh) / c_num_chunks;
	int w = (txs * draww) / c_num_tiles;
	int h = (tys * drawh) / c_num_tiles;
	if (w == 0)
		w = 1;
	if (h == 0)
		h = 1;
	// Draw location in yellow.
	gdk_rgb_gc_set_foreground(drawgc, (255 << 16) + (255 << 8));
	gdk_draw_rectangle(draw->window, drawgc, FALSE, x, y, w, h);
	viewbox.x = x;
	viewbox.y = y;   // Save location.
	viewbox.width = w;
	viewbox.height = h;
	// Put a light-red box around it.
	gdk_rgb_gc_set_foreground(drawgc, (255 << 16) + (128 << 8) + 128);
	gdk_draw_rectangle(draw->window, drawgc, FALSE,
	                   x - 3, y - 3, w + 6, h + 6);
}

/*
 *  Message was received from Exult that the view has changed.
 */

void Locator::view_changed(
    const unsigned char *data,        // Tx, Ty, xtiles, ytiles, scale.
    int datalen
) {
	if (datalen < 4 * 4)
		return;         // Bad length.
	if (dragging)
		return;     //+++++++Not sure about this.
	tx = Read4(data);
	ty = Read4(data);
	txs = Read4(data);
	tys = Read4(data);
	// ++++Scale?  Later.
	// Do things by chunk.
	int cx = tx / c_tiles_per_chunk;
	int cy = ty / c_tiles_per_chunk;
	tx = cx * c_tiles_per_chunk;
	ty = cy * c_tiles_per_chunk;
	// Update scrolls.
	gtk_adjustment_set_value(hadj, cx);
	gtk_adjustment_set_value(vadj, cy);
	render();
}

/*
 *  Handle a scrollbar event.
 */

void Locator::vscrolled(        // For vertical scrollbar.
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Shape_chooser.
) {
	Locator *loc = static_cast<Locator *>(data);
	int oldty = loc->ty;
	loc->ty = static_cast<gint>(adj->value) * c_tiles_per_chunk;
	if (loc->ty != oldty)       // (Already equal if this event came
		//   from Exult msg.).
	{
		loc->render();
		loc->send_location();
	}
}
void Locator::hscrolled(        // For horizontal scrollbar.
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Locator.
) {
	Locator *loc = static_cast<Locator *>(data);
	int oldtx = loc->tx;
	loc->tx = static_cast<gint>(adj->value) * c_tiles_per_chunk;
	if (loc->tx != oldtx)       // (Already equal if this event came
		//   from Exult msg.).
	{
		loc->render();
		loc->send_location();
	}
}

/*
 *  Send location to Exult.
 */

void Locator::send_location(
) {
	unsigned char data[50];
	unsigned char *ptr = &data[0];
	Write4(ptr, tx);
	Write4(ptr, ty);
	Write4(ptr, txs);
	Write4(ptr, tys);
	Write4(ptr, static_cast<uint32>(-1));       // Don't change.
	cout << "Locator::send_location" << endl;
	ExultStudio::get_instance()->send_to_server(Exult_server::view_pos,
	        &data[0], ptr - data);
}

/*
 *  Query Exult for its location.
 */

void Locator::query_location(
) {
	unsigned char data[50];
	unsigned char *ptr = &data[0];
	Write4(ptr, static_cast<uint32>(-1));
	Write4(ptr, static_cast<uint32>(-1));
	Write4(ptr, static_cast<uint32>(-1));
	Write4(ptr, static_cast<uint32>(-1));
	Write4(ptr, static_cast<uint32>(-1));
	ExultStudio::get_instance()->send_to_server(Exult_server::view_pos,
	        &data[0], ptr - data);
}

/*
 *  This is called a fraction of a second after mouse motion to send
 *  the new location to Exult if the mouse hasn't moved further.
 */

gint Locator::delayed_send_location(
    gpointer data           // ->locator.
) {
	Locator *loc = static_cast<Locator *>(data);
	loc->send_location();
	loc->send_location_timer = -1;
	return 0;           // Cancels timer.
}

/*
 *  Go to a mouse location in the draw area.
 */

void Locator::goto_mouse(
    int mx, int my,         // Pixel coords. in draw area.
    bool delay_send         // Delay send_location for a bit.
) {
	GdkRectangle oldbox = viewbox;  // Old location of box.
	int oldtx = tx;
	int oldty = ty;
	// Set tx,ty here so hscrolled() &
	//   vscrolled() don't send to Exult.
	tx = (mx * c_num_tiles) / draw->allocation.width;
	ty = (my * c_num_tiles) / draw->allocation.height;
	int cx = tx / c_tiles_per_chunk;
	int cy = ty / c_tiles_per_chunk;
	if (cx > c_num_chunks - 2)
		cx = c_num_chunks - 2;
	if (cy > c_num_chunks - 2)
		cy = c_num_chunks - 2;
	tx = cx * c_tiles_per_chunk;
	ty = cy * c_tiles_per_chunk;
	// Update scrolls.
	gtk_adjustment_set_value(hadj, cx);
	gtk_adjustment_set_value(vadj, cy);
	// Now we just send it once.
	if (tx != oldtx || ty != oldty) {
		if (send_location_timer != -1)
			gtk_timeout_remove(send_location_timer);
		if (delay_send)     // Send in 1/3 sec. if no more motion.
			send_location_timer = gtk_timeout_add(333,
			                                      delayed_send_location, this);
		else {
			send_location();
			send_location_timer = -1;
		}
		GdkRectangle newbox;    // Figure dirty rectangle;
		GdkRectangle dirty;
		newbox.x = (cx * draw->allocation.width) / c_num_chunks,
		       newbox.y = (cy * draw->allocation.height) / c_num_chunks;
		newbox.width = oldbox.width;
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

gint Locator::mouse_press(
    GdkEventButton *event
) {
	dragging = false;
	if (event->button != 1)
		return FALSE;       // Handling left-click.
	// Get mouse position, draw dims.
	int mx = static_cast<int>(event->x);
	int my = static_cast<int>(event->y);
	// Double-click?
	if (reinterpret_cast<GdkEvent *>(event)->type == GDK_2BUTTON_PRESS) {
		goto_mouse(mx, my);
		return TRUE;
	}
	// On (or close to) view box?
	if (mx < viewbox.x - 3 || my < viewbox.y - 3 ||
	        mx > viewbox.x + viewbox.width + 6 ||
	        my > viewbox.y + viewbox.height + 6)
		return FALSE;
	dragging = true;
	drag_relx = mx - viewbox.x; // Save rel. pos.
	drag_rely = my - viewbox.y;
	return TRUE;
}

/*
 *  Handle a mouse-release event.
 */

gint Locator::mouse_release(
    GdkEventButton *event
) {
	ignore_unused_variable_warning(event);
	dragging = false;
	return TRUE;
}

/*
 *  Handle a mouse-motion event.
 */

gint Locator::mouse_motion(
    GdkEventMotion *event
) {
	int mx;
	int my;
	GdkModifierType state;
	if (event->is_hint)
		gdk_window_get_pointer(event->window, &mx, &my, &state);
	else {
		mx = static_cast<int>(event->x);
		my = static_cast<int>(event->y);
		state = static_cast<GdkModifierType>(event->state);
	}
	if (!dragging || !(state & GDK_BUTTON1_MASK))
		return FALSE;       // Not dragging with left button.
	// Delay sending location to Exult.
	goto_mouse(mx + drag_relx, my + drag_rely, true);
	return TRUE;
}
