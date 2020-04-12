/**
 ** Shapedraw.cc - Manage a drawing area that shows one or more shapes.
 **
 ** Written: 6/2/2001 - JSF
 **/

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "shapedraw.h"
#include "vgafile.h"
#include "ibuf8.h"
#include "u7drag.h"
#include "ignore_unused_variable_warning.h"

using std::cout;
using std::endl;

/*
 *  Blit onto screen.
 */

void Shape_draw::show(
    GdkDrawable *drawable,
    int x, int y, int w, int h  // Area to blit.
) {
	int stride = iwin->get_line_width();
	gdk_draw_indexed_image(drawable, drawgc, x, y, w, h,
	                       GDK_RGB_DITHER_NORMAL,
	                       iwin->get_bits() + y * stride + x,
	                       stride, palette);
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
	gint winw = draw->allocation.width;
	gint winh = draw->allocation.height;
	draw_shape(shape, (winw - shape->get_width()) / 2,
	           (winh - shape->get_height()) / 2);
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
	guint32 colors[256];
	for (int i = 0; i < 256; i++)
		colors[i] = (palbuf[3 * i] << 16) * 4 + (palbuf[3 * i + 1] << 8) * 4 +
		            palbuf[3 * i + 2] * 4;
	palette = gdk_rgb_cmap_new(colors, 256);
}

/*
 *  Delete.
 */

Shape_draw::~Shape_draw(
) {
	gdk_rgb_cmap_free(palette);
	delete iwin;
}

/*
 *  Default render.
 */

void Shape_draw::render(
) {
}

/*
 *  Set background color and repaint.
 */

void Shape_draw::set_background_color(
    guint32 c
) {
	palette->colors[255] = c;
	render();
	show();
}

/*
 *  Configure the viewing window.
 */

void Shape_draw::configure(
) {
	if (!draw->window)
		return;         // Not ready yet.
	if (!iwin) {        // First time?
		drawgc = gdk_gc_new(draw->window);
		// Foreground = yellow.
		gdk_rgb_gc_set_foreground(drawgc, (255 << 16) + (255 << 8));
		iwin = new Image_buffer8(
		    draw->allocation.width, draw->allocation.height);
	} else if (static_cast<int>(iwin->get_width() ) != draw->allocation.width ||
	           static_cast<int>(iwin->get_height()) != draw->allocation.height) {
		delete iwin;
		iwin = new Image_buffer8(
		    draw->allocation.width, draw->allocation.height);
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
	Shape_draw *draw = static_cast<Shape_draw *>(udata);
	cout << "drag_data_received" << endl;
	if (draw->drop_callback &&
	        seldata->type == gdk_atom_intern(U7_TARGET_SHAPEID_NAME, 0) &&
	        seldata->format == 8 && seldata->length > 0) {
		int file;
		int shape;
		int frame;
		Get_u7_shapeid(seldata->data, file, shape, frame);
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
	GtkTargetEntry tents[1];
	tents[0].target = const_cast<char *>(U7_TARGET_SHAPEID_NAME);
	tents[0].flags = 0;
	tents[0].info = U7_TARGET_SHAPEID;
	gtk_drag_dest_set(draw, GTK_DEST_DEFAULT_ALL, tents, 1,
	                  static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));

	gtk_signal_connect(GTK_OBJECT(draw), "drag_data_received",
	                   GTK_SIGNAL_FUNC(drag_data_received), this);
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
	GdkPixmap *pixmap = gdk_pixmap_new(draw->window, w, h, -1);
	gdk_draw_indexed_image(pixmap, drawgc, 0, 0, w, h,
	                       GDK_RGB_DITHER_NORMAL, tbits,
	                       tbuf.get_line_width(), palette);
	int mask_stride = (w + 7) / 8;  // Round up to nearest byte.
	char *mdata = new char[mask_stride * h];
	for (int y = 0; y < h; y++) // Do each row.
		// Do each byte.
		for (int b = 0; b < mask_stride; b++) {
			char bits = 0;
			unsigned char *vals = tbits + y * w + b * 8;
			for (int i = 0; i < 8; i++)
				if (vals[i] != 0xff)
					bits |= (1 << i);
			mdata[y * mask_stride + b] = bits;
		}
	GdkBitmap *mask = gdk_bitmap_create_from_data(draw->window,
	                  mdata, w, h);
	delete [] mdata;
	// This will be the shape dragged.
	gtk_drag_set_icon_pixmap(context,
	                         gdk_window_get_colormap(draw->window), pixmap, mask,
	                         w - 2 - xright, h - 2 - ybelow);
	gdk_pixmap_unref(pixmap);
	gdk_bitmap_unref(mask);
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
	GtkTargetEntry tents[1];// Set up for dragging.
	tents[0].target = const_cast<char *>(target);
	tents[0].flags = 0;
	tents[0].info = id;
	GtkTargetList *tlist = gtk_target_list_new(&tents[0], 1);
	// ??+++ Do we need to free tlist?
	gtk_drag_begin(draw, tlist,
	               static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE),
	               1, event);
}

