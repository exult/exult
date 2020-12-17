/**
 ** Locator.h - Locate game positions.
 **
 ** Written: March 2, 2002 - JSF
 **/

/*
Copyright (C) 2001-2020 The Exult Team

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

#ifndef INCL_LOCATOR
#define INCL_LOCATOR

/*
 *  The 'locator' window:
 */
class Locator {
	GtkWidget *win;                  // Main window.
	GtkWidget *draw;                 // GTK draw area to display.
	cairo_t *drawgc = nullptr;       // For drawing in 'draw'.
	GtkAdjustment *hadj, *vadj;      // For horiz., vert. scales.
	int tx = 0, ty = 0;              // Current Exult win. info. in tiles.
	int txs = 40, tys = 25;          // Current Exult win. info. in tiles.
	GdkRectangle viewbox;            // Where view box was last drawn.
	bool dragging = false;           // True if dragging view box.
	int drag_relx, drag_rely;        // Mouse pos. rel to view box.
	int send_location_timer = -1;    // For sending new loc. to Exult.
	void send_location();            // Send location/size to Exult.
	void query_location();
	static gint delayed_send_location(gpointer data);
	// Set view to mouse location.
	void goto_mouse(int mx, int my, bool delay_send = false);
public:
	Locator();
	~Locator();
	void show(bool tf);     // Show/hide.
	static gboolean on_loc_draw_expose_event(
	    GtkWidget *widget, cairo_t *cairo, gpointer data);
	// Configure when created/resized.
	void configure(GtkWidget *widget);
	void render(GdkRectangle *area = nullptr);
	// Manage graphic context.
	void set_graphic_context(cairo_t *cairo) {
		drawgc = cairo;
	}
	cairo_t *get_graphic_context() {
		return drawgc;
	}
	// Message from exult.
	void view_changed(const unsigned char *data, int datalen);
	// Handle scrollbar.
	static void vscrolled(GtkAdjustment *adj, gpointer data);
	static void hscrolled(GtkAdjustment *adj, gpointer data);
	// Handle mouse.
	gboolean mouse_press(GdkEventButton *event);
	gboolean mouse_release(GdkEventButton *event);
	gboolean mouse_motion(GdkEventMotion *event);
};

#endif
