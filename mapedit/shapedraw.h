/**
 ** Shapedraw.h - Manage a drawing area that shows one or more shapes.
 **
 ** Written: 6/2/2001 - JSF
 **/

#ifndef INCL_SHAPEDRAW
#define INCL_SHAPEDRAW  1

/*
Copyright (C) 2001  The Exult Team

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
#include <gdk/gdktypes.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__
#include "gtk_redefines.h"

class Vga_file;
class Shape_frame;
class Image_buffer8;

using Drop_callback = void (*)(int filenum,
                               int shapenum, int framenum, void *udata);

/*
 *  This class can draw shapes from a .vga file.
 */
class Shape_draw {
protected:
	Vga_file *ifile;        // Where the shapes come from.
	GtkWidget *draw;        // GTK draw area to display them in.
	GdkGC *drawgc;          // For drawing in 'draw'.
	Image_buffer8 *iwin;        // What we render into.
	GdkRgbCmap *palette;        // For gdk_draw_indexed_image().
	Drop_callback drop_callback;    // Called when a shape is dropped here.
	void *drop_user_data;
	bool dragging;          // Dragging from here.
public:
	Shape_draw(Vga_file *i, const unsigned char *palbuf, GtkWidget *drw);
	virtual ~Shape_draw();
	// Blit onto screen.
	void show(GdkDrawable *drawable, int x, int y, int w, int h);
	void show(int x, int y, int w, int h) {
		show(draw->window, x, y, w, h);
	}
	void show() {
		show(0, 0, draw->allocation.width, draw->allocation.height);
	}
	guint32 get_color(int i) {
		return palette->colors[i];
	}
	void draw_shape(Shape_frame *shape, int x, int y);
	void draw_shape(int shapenum, int framenum, int x, int y);
	void draw_shape_outline(int shapenum, int framenum,
	                        int x, int y, unsigned char color);
	void draw_shape_centered(int shapenum, int framenum);
	virtual void render();      // Update what gets shown.
	void set_background_color(guint32 c);

	void configure();       // Configure when created/resized.
	// Handler for drop.
	static void drag_data_received(GtkWidget *widget,
	                               GdkDragContext *context, gint x, gint y,
	                               GtkSelectionData *seldata, guint info, guint time,
	                               gpointer udata);
	void enable_drop(Drop_callback callback, void *udata);
	void set_drag_icon(GdkDragContext *context, Shape_frame *shape);
	// Start/end dragging from here.
	void start_drag(const char *target, int id, GdkEvent *event);
	void mouse_up() {
		dragging = false;
	}
};

#endif
