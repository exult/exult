/**
 ** Shapedraw.cc - Manage a drawing area that shows one or more shapes.
 **
 ** Written: 6/2/2001 - JSF
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "shapedraw.h"

#include "ibuf8.h"
#include "u7drag.h"
#include "vgafile.h"

using std::cout;
using std::endl;

/*
 *  Blit onto screen.
 */

void Shape_draw::show(
    int x, int y, int w, int h  // Area to blit.
) {
	int stride = iwin->get_line_width();
	if (drawgc != nullptr) {
		GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
		guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
		int rstride = gdk_pixbuf_get_rowstride(pixbuf);
		int pstride = gdk_pixbuf_get_n_channels(pixbuf);
		for (int ly = 0; ly < h; ly++) for (int lx = 0; lx < w; lx++) {
				guchar *t = pixels + ly * rstride + lx * pstride;
				guchar  s = iwin->get_bits() [ ( y + ly ) * stride + ( x + lx ) ];
				guint32 c = palette->colors[s];
				t[0] = (c >> 16) & 255;
				t[1] = (c >>  8) & 255;
				t[2] = (c >>  0) & 255;
			}
		gdk_cairo_set_source_pixbuf(drawgc, pixbuf, x, y);
		cairo_rectangle(drawgc, x, y, w, h);
		cairo_fill(drawgc);
		g_object_unref(pixbuf);
	}
}

/*
 *  Draw one shape at a particular place.
 */

void Shape_draw::draw_shape(
    Shape_frame *shape,
    int x, int y
) {
	shape->paint(iwin, x + shape->get_xleft(), y + shape->get_yabove());
}

/*
 *  Draw one shape at a particular place.
 */

void Shape_draw::draw_shape(
    int shapenum, int framenum,
    int x, int y
) {
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes())
		return;
	Shape_frame *shape = ifile->get_shape(shapenum, framenum);
	if (shape)
		draw_shape(shape, x, y);
}

/*
 *  Draw one shape's outline at a particular place.
 */

void Shape_draw::draw_shape_outline(
    int shapenum, int framenum,
    int x, int y,
    unsigned char color     // Color index for outline.
) {
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes())
		return;
	Shape_frame *shape = ifile->get_shape(shapenum, framenum);
	if (shape) {
		if (shape->is_rle())
			shape->paint_rle_outline(iwin, x + shape->get_xleft(),
			                         y + shape->get_yabove(), color);
		else {
			int w = shape->get_width();
			int h = shape->get_height();
			iwin->fill_line8(color, w, x, y);
			iwin->fill_line8(color, w, x, y + h - 1);
			iwin->fill8(color, 1, h, x, y);
			iwin->fill8(color, 1, h, x + w - 1, y);
		}
	}
}

/*
 *  Draw a shape centered in the drawing area.
 */

void Shape_draw::draw_shape_centered(
    int shapenum,           // -1 to not draw shape.
    int framenum
) {
	iwin->fill8(255);       // Background (transparent) color.
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes()
	        || framenum >= ifile->get_num_frames(shapenum))
		return;
	Shape_frame *shape = ifile->get_shape(shapenum, framenum);
	if (!shape || shape->is_empty())
		return;
	// Get drawing area dimensions.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	gint winw = alloc.width;
	gint winh = alloc.height;
	if (( alloc.width <  shape->get_width() + 8) ||
	    (alloc.height < shape->get_height() + 8)) {
		gtk_widget_set_size_request(draw,
		    alloc.width  <  shape->get_width() + 8 ?
		    shape->get_width() + 8 :  alloc.width,
		    alloc.height < shape->get_height() + 8 ?
		    shape->get_height() + 8 : alloc.height);
		gtk_widget_queue_draw(draw);
	}
	else {
		draw_shape(shape, (winw -  shape->get_width()) / 2,
	                          (winh - shape->get_height()) / 2);
	}
}

/*
 *  Create.
 */

Shape_draw::Shape_draw(
    Vga_file *i,            // Where they're kept.
    const unsigned char *palbuf,      // Palette, 3*256 bytes (rgb triples).
    GtkWidget *drw          // Drawing area to use.
) : ifile(i), draw(drw), drawgc(nullptr),
	iwin(nullptr), palette(nullptr),
	drop_callback(nullptr), drop_user_data(nullptr), dragging(false) {
	palette = new ExultRgbCmap;
	for (int i = 0; i < 256; i++)
		palette->colors[i] = (palbuf[3 * i] << 16) * 4 +
		            (palbuf[3 * i + 1] << 8) * 4 + palbuf[3 * i + 2] * 4;
}

/*
 *  Delete.
 */

Shape_draw::~Shape_draw(
) {
	delete palette;
	delete iwin;
}

/*
 *  Default render.
 */

void Shape_draw::render(
) {
	gtk_widget_queue_draw(draw);
}

/*
 *  Set background color and repaint.
 */

void Shape_draw::set_background_color(
    guint32 c
) {
	palette->colors[255] = c;
	render();
}

/*
 *  Configure the viewing window.
 */

void Shape_draw::configure(
) {
	if (!gtk_widget_get_realized(draw))
		return;         // Not ready yet.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	if (!iwin) {        // First time?
		// Foreground = yellow.
		drawfg = (255 << 16) + (255 << 8);
		iwin = new Image_buffer8(alloc.width, alloc.height);
	} else if (static_cast<int>(iwin->get_width()) != alloc.width ||
	           static_cast<int>(iwin->get_height()) != alloc.height) {
		delete iwin;
		iwin = new Image_buffer8(alloc.width, alloc.height);
	}
}

/*
 *  Shape was dropped.
 */

void Shape_draw::drag_data_received(
    GtkWidget *widget,
    GdkDragContext *context,
    gint x,
    gint y,
    GtkSelectionData *seldata,
    guint info,
    guint time,
    gpointer udata          // Should point to Shape_draw.
) {
	ignore_unused_variable_warning(widget, context, x, y, info, time);
	auto *draw = static_cast<Shape_draw *>(udata);
	cout << "In DRAG_DATA_RECEIVED of Shape for '"
	     << gdk_atom_name(gtk_selection_data_get_data_type(seldata))
	     << "'" << endl;
	if (draw->drop_callback &&
	        (gtk_selection_data_get_data_type(seldata) == gdk_atom_intern(U7_TARGET_SHAPEID_NAME, 0) ||
	         gtk_selection_data_get_data_type(seldata) == gdk_atom_intern(U7_TARGET_GENERIC_NAME_X11, 0) ||
	         gtk_selection_data_get_data_type(seldata) == gdk_atom_intern(U7_TARGET_GENERIC_NAME_MACOSX, 0)) &&
	        Is_u7_shapeid(gtk_selection_data_get_data(seldata)) == true &&
	        gtk_selection_data_get_format(seldata) == 8 &&
	        gtk_selection_data_get_length(seldata) > 0) {
		int file;
		int shape;
		int frame;
		Get_u7_shapeid(gtk_selection_data_get_data(seldata), file, shape, frame);
		(*draw->drop_callback)(file, shape, frame,
		                       draw->drop_user_data);
	}
}

/*
 *  Set to accept drops from drag-n-drop of a shape.
 */

void Shape_draw::enable_drop(
    Drop_callback callback,     // Call this when shape dropped.
    void *udata         // Passed to callback.
) {
	gtk_widget_realize(draw);//???????
#ifndef _WIN32
	drop_callback = callback;
	drop_user_data = udata;
	GtkTargetEntry tents[3];
	tents[0].target = const_cast<char *>(U7_TARGET_SHAPEID_NAME);
	tents[1].target = const_cast<char *>(U7_TARGET_GENERIC_NAME_X11);
	tents[2].target = const_cast<char *>(U7_TARGET_GENERIC_NAME_MACOSX);
	tents[0].flags = 0;
	tents[1].flags = 0;
	tents[2].flags = 0;
	tents[0].info = U7_TARGET_SHAPEID;
	tents[1].info = U7_TARGET_SHAPEID;
	tents[2].info = U7_TARGET_SHAPEID;
	gtk_drag_dest_set(draw, GTK_DEST_DEFAULT_ALL, tents, 3,
	                  static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));

	g_signal_connect(G_OBJECT(draw), "drag-data-received",
	                 G_CALLBACK(drag_data_received), this);
#else
	ignore_unused_variable_warning(callback, udata);
#endif
}

/*
 *  Set an icon for dragging FROM this area.
 */

void Shape_draw::set_drag_icon(
    GdkDragContext *context,
    Shape_frame *shape      // Shape to use for the icon.
) {
	int w = shape->get_width();
	int h = shape->get_height();
	int xright = shape->get_xright();
	int ybelow = shape->get_ybelow();
	Image_buffer8 tbuf(w, h);   // Create buffer to render to.
	tbuf.fill8(0xff);       // Fill with 'transparent' pixel.
	unsigned char *tbits = tbuf.get_bits();
	shape->paint(&tbuf, w - 1 - xright, h - 1 - ybelow);
	// Put shape on a pixmap.
	GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
	guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
	int rstride = gdk_pixbuf_get_rowstride(pixbuf);
	int pstride = gdk_pixbuf_get_n_channels(pixbuf);
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
			guchar *t = pixels + y * rstride + x * pstride;
			guchar  s = tbits  [ y * w       + x ];
			guint32 c = palette->colors[s];
			t[0] = (s == 255 ? 0 : (c >> 16) & 255);
			t[1] = (s == 255 ? 0 : (c >>  8) & 255);
			t[2] = (s == 255 ? 0 : (c >>  0) & 255);
			t[3] = (s == 255 ? 0 : 255);
		}
	// This will be the shape dragged.
	gtk_drag_set_icon_pixbuf(context, pixbuf, w - 2 - xright, h - 2 - ybelow);
	g_object_unref(pixbuf);
}

/*
 *  Start dragging from here.
 *
 *  Note:   Sets 'dragging', which is only cleared by 'mouse_up()'.
 */

void Shape_draw::start_drag(
    const char *target,         // Target (ie, U7_TARGET_SHAPEID_NAME).
    int id,             // ID (ie, U7_TARGET_SHAPEID).
    GdkEvent *event         // Event that started this.
) {
	if (dragging)
		return;
	dragging = true;
	GtkTargetEntry tents[3];// Set up for dragging.
	tents[0].target = const_cast<char *>(target);
	tents[1].target = const_cast<char *>(U7_TARGET_GENERIC_NAME_X11);
	tents[2].target = const_cast<char *>(U7_TARGET_GENERIC_NAME_MACOSX);
	tents[0].flags = 0;
	tents[1].flags = 0;
	tents[2].flags = 0;
	tents[0].info = id;
	tents[1].info = id;
	tents[2].info = id;
	GtkTargetList *tlist = gtk_target_list_new(&tents[0], 3);
	// ??+++ Do we need to free tlist?
	gtk_drag_begin_with_coordinates(draw, tlist,
	                                static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE),
	                                1, event, -1, -1);
}

