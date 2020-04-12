/**
 ** Locator.h - Locate game positions.
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

#ifndef INCL_LOCATOR
#define INCL_LOCATOR

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
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

/*
 *  The 'locator' window:
 */
class Locator {
	GtkWidget *win;                  // Main window.
	GtkWidget *draw;                 // GTK draw area to display.
	GdkGC *drawgc = nullptr;         // For drawing in 'draw'.
	GtkAdjustment *hadj, *vadj;      // For horiz., vert. scales.
	int tx = 0, ty = 0, txs = 40, tys = 25/*, scale*/;    // Current Exult win. info. in tiles.
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
	// Configure when created/resized.
	void configure(GtkWidget *widget);
	void render(GdkRectangle *area = nullptr);
	// Message from exult.
	void view_changed(const unsigned char *data, int datalen);
	// Handle scrollbar.
	static void vscrolled(GtkAdjustment *adj, gpointer data);
	static void hscrolled(GtkAdjustment *adj, gpointer data);
	// Handle mouse.
	gint mouse_press(GdkEventButton *event);
	gint mouse_release(GdkEventButton *event);
	gint mouse_motion(GdkEventMotion *event);
};

#endif
