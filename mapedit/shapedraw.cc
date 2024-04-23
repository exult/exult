/**
 ** Shapedraw.cc - Manage a drawing area that shows one or more shapes.
 **
 ** Written: 6/2/2001 - JSF
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
		int x, int y, int w, int h    // Area to blit.
) {
	const int stride = iwin->get_line_width();
	if (drawgc != nullptr) {
		GdkPixbuf* pixbuf  = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
		guchar*    pixels  = gdk_pixbuf_get_pixels(pixbuf);
		const int  rstride = gdk_pixbuf_get_rowstride(pixbuf);
		const int  pstride = gdk_pixbuf_get_n_channels(pixbuf);
		for (int ly = 0; ly < h; ly++) {
			for (int lx = 0; lx < w; lx++) {
				guchar*      t = pixels + ly * rstride + lx * pstride;
				const guchar s = iwin->get_bits()[(y + ly) * stride + (x + lx)];
				const guint32 c = palette->colors[s];
				t[0]            = (c >> 16) & 255;
				t[1]            = (c >> 8) & 255;
				t[2]            = (c >> 0) & 255;
			}
		}
		const int zoom_scale = ExultStudio::get_instance()->get_shape_scale();
		if (zoom_scale == 2) {    // No Zoom
			gdk_cairo_set_source_pixbuf(drawgc, pixbuf, x, y);
			cairo_rectangle(drawgc, x, y, w, h);
			cairo_fill(drawgc);
		} else {
			// Using GdkPixbuf scaling :
			const bool zoom_bilinear
					= ExultStudio::get_instance()->get_shape_bilinear();
			GdkPixbuf* zoom_pixbuf = gdk_pixbuf_scale_simple(
					pixbuf, (w * zoom_scale) / 2, (h * zoom_scale) / 2,
					(zoom_bilinear ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST));
			gdk_cairo_set_source_pixbuf(
					drawgc, zoom_pixbuf, (x * zoom_scale) / 2,
					(y * zoom_scale) / 2);
			cairo_rectangle(
					drawgc, (x * zoom_scale) / 2, (y * zoom_scale) / 2,
					(w * zoom_scale) / 2, (h * zoom_scale) / 2);
			cairo_fill(drawgc);
			g_object_unref(zoom_pixbuf);
			//
			// Using Cairo scaling :
			// cairo_scale(drawgc, zoom_scale / 2.0, zoom_scale / 2.0);
			// gdk_cairo_set_source_pixbuf(drawgc, pixbuf, x, y);
			// cairo_rectangle(drawgc, x, y, w, h);
			// cairo_fill(drawgc);
		}
		g_object_unref(pixbuf);
	}
}

/*
 *  Draw one shape at a particular place.
 */

void Shape_draw::draw_shape(Shape_frame* shape, int x, int y) {
	shape->paint(iwin, x + shape->get_xleft(), y + shape->get_yabove());
}

/*
 *  Draw one shape at a particular place.
 */

void Shape_draw::draw_shape(int shapenum, int framenum, int x, int y) {
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes()) {
		return;
	}
	Shape_frame* shape = ifile->get_shape(shapenum, framenum);
	if (shape) {
		draw_shape(shape, x, y);
	}
}

/*
 *  Draw one shape's outline at a particular place.
 */

void Shape_draw::draw_shape_outline(
		int shapenum, int framenum, int x, int y,
		unsigned char color    // Color index for outline.
) {
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes()) {
		return;
	}
	Shape_frame* shape = ifile->get_shape(shapenum, framenum);
	if (shape) {
		if (shape->is_rle()) {
			shape->paint_rle_outline(
					iwin, x + shape->get_xleft(), y + shape->get_yabove(),
					color);
		} else {
			const int w = shape->get_width();
			const int h = shape->get_height();
			iwin->fill_hline8(color, w, x, y);
			iwin->fill_hline8(color, w, x, y + h - 1);
			iwin->fill8(color, 1, h, x, y);
			iwin->fill8(color, 1, h, x + w - 1, y);
		}
	}
}

/*
 *  Draw a shape centered in the drawing area.
 */

void Shape_draw::draw_shape_centered(
		int shapenum,    // -1 to not draw shape.
		int framenum) {
	iwin->fill8(255);    // Background (transparent) color.
	if (shapenum < 0 || shapenum >= ifile->get_num_shapes()) {
		return;
	}
	const int num_frames = ifile->get_num_frames(shapenum);
	if ((framenum < 0 || framenum >= num_frames)
		&& (num_frames > 32 || framenum < 32
			|| framenum >= (32 + num_frames))) {
		return;
	}
	Shape_frame* shape = ifile->get_shape(shapenum, framenum);
	if (!shape || shape->is_empty()) {
		return;
	}
	// Get drawing area dimensions.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	const gint winw = ZoomDown(alloc.width);
	const gint winh = ZoomDown(alloc.height);
	if ((winw < shape->get_width() + 8) || (winh < shape->get_height() + 8)) {
		gtk_widget_set_size_request(
				draw,
				winw < shape->get_width() + 8 ? ZoomUp(shape->get_width() + 8)
											  : alloc.width,
				winh < shape->get_height() + 8 ? ZoomUp(shape->get_height() + 8)
											   : alloc.height);
		cairo_reset_clip(drawgc);
		gtk_widget_queue_draw(draw);
	} else {
		draw_shape(
				shape, (winw - shape->get_width()) / 2,
				(winh - shape->get_height()) / 2);
	}
}

/*
 *  Create.
 */

Shape_draw::Shape_draw(
		Vga_file*            i,         // Where they're kept.
		const unsigned char* palbuf,    // Palette, 3*256 bytes (rgb triples).
		GtkWidget*           drw        // Drawing area to use.
		)
		: ifile(i), draw(drw), drawgc(nullptr), iwin(nullptr), palette(nullptr),
		  drop_callback(nullptr), drop_user_data(nullptr), dragging(false) {
	palette = new ExultRgbCmap;
	for (int i = 0; i < 256; i++) {
		palette->colors[i] = (palbuf[3 * i] << 16) * 4
							 + (palbuf[3 * i + 1] << 8) * 4
							 + palbuf[3 * i + 2] * 4;
	}
}

/*
 *  Delete.
 */

Shape_draw::~Shape_draw() {
	delete palette;
	delete iwin;
}

/*
 *  Default render.
 */

void Shape_draw::render() {
	gtk_widget_queue_draw(draw);
}

/*
 *  Set background color and repaint.
 */

void Shape_draw::set_background_color(guint32 c) {
	palette->colors[255] = c;
	render();
}

/*
 *  Configure the viewing window.
 */

void Shape_draw::configure() {
	if (!gtk_widget_get_realized(draw)) {
		return;    // Not ready yet.
	}
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	if (!iwin) {    // First time?
		// Foreground = yellow.
		drawfg = (255 << 16) + (255 << 8);
		iwin   = new Image_buffer8(alloc.width, alloc.height);
	} else if (
			static_cast<int>(iwin->get_width()) != alloc.width
			|| static_cast<int>(iwin->get_height()) != alloc.height) {
		delete iwin;
		iwin = new Image_buffer8(alloc.width, alloc.height);
	}
}

/*
 *  Shape was dropped.
 */

void Shape_draw::drag_data_received(
		GtkWidget* widget, GdkDragContext* context, gint x, gint y,
		GtkSelectionData* seldata, guint info, guint time,
		gpointer udata    // Should point to Shape_draw.
) {
	ignore_unused_variable_warning(widget, context, x, y, info, time);
	auto* draw = static_cast<Shape_draw*>(udata);
	cout << "In DRAG_DATA_RECEIVED of Shape for '"
		 << gdk_atom_name(gtk_selection_data_get_data_type(seldata)) << "'"
		 << endl;
	if (draw->drop_callback
		&& (gtk_selection_data_get_data_type(seldata)
					== gdk_atom_intern(U7_TARGET_SHAPEID_NAME, 0)
			|| gtk_selection_data_get_data_type(seldata)
					   == gdk_atom_intern(U7_TARGET_DROPFILE_NAME_MIME, 0)
			|| gtk_selection_data_get_data_type(seldata)
					   == gdk_atom_intern(U7_TARGET_DROPFILE_NAME_MACOSX, 0))
		&& Is_u7_shapeid(gtk_selection_data_get_data(seldata))
		&& gtk_selection_data_get_format(seldata) == 8
		&& gtk_selection_data_get_length(seldata) > 0) {
		int file;
		int shape;
		int frame;
		Get_u7_shapeid(
				gtk_selection_data_get_data(seldata), file, shape, frame);
		(*draw->drop_callback)(file, shape, frame, draw->drop_user_data);
	}
}

/*
 *  Set to accept drops from drag-n-drop of a shape.
 */

gulong Shape_draw::enable_drop(
		Drop_callback callback,    // Call this when shape dropped.
		void*         udata        // Passed to callback.
) {
	gtk_widget_realize(draw);    //???????
	drop_callback  = callback;
	drop_user_data = udata;
	GtkTargetEntry tents[3];
	tents[0].target = const_cast<char*>(U7_TARGET_SHAPEID_NAME);
	tents[1].target = const_cast<char*>(U7_TARGET_DROPFILE_NAME_MIME);
	tents[2].target = const_cast<char*>(U7_TARGET_DROPFILE_NAME_MACOSX);
	tents[0].flags  = 0;
	tents[1].flags  = 0;
	tents[2].flags  = 0;
	tents[0].info   = U7_TARGET_SHAPEID;
	tents[1].info   = U7_TARGET_SHAPEID;
	tents[2].info   = U7_TARGET_SHAPEID;
	gtk_drag_dest_set(
			draw, GTK_DEST_DEFAULT_ALL, tents, 3,
			static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));

	return g_signal_connect(
			G_OBJECT(draw), "drag-data-received",
			G_CALLBACK(drag_data_received), this);
}

/*
 *  Set an icon for dragging FROM this area.
 */

void Shape_draw::set_drag_icon(
		GdkDragContext* context,
		Shape_frame*    shape    // Shape to use for the icon.
) {
	const int     w      = shape->get_width();
	const int     h      = shape->get_height();
	const int     xright = shape->get_xright();
	const int     ybelow = shape->get_ybelow();
	Image_buffer8 tbuf(w, h);    // Create buffer to render to.
	tbuf.fill8(0xff);            // Fill with 'transparent' pixel.
	unsigned char* tbits = tbuf.get_bits();
	shape->paint(&tbuf, w - 1 - xright, h - 1 - ybelow);
	// Put shape on a pixmap.
	GdkPixbuf* pixbuf  = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
	guchar*    pixels  = gdk_pixbuf_get_pixels(pixbuf);
	const int  rstride = gdk_pixbuf_get_rowstride(pixbuf);
	const int  pstride = gdk_pixbuf_get_n_channels(pixbuf);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			guchar*       t = pixels + y * rstride + x * pstride;
			const guchar  s = tbits[y * w + x];
			const guint32 c = palette->colors[s];
			t[0]            = (s == 255 ? 0 : (c >> 16) & 255);
			t[1]            = (s == 255 ? 0 : (c >> 8) & 255);
			t[2]            = (s == 255 ? 0 : (c >> 0) & 255);
			t[3]            = (s == 255 ? 0 : 255);
		}
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
		const char* target,    // Target (ie, U7_TARGET_SHAPEID_NAME).
		int         id,        // ID (ie, U7_TARGET_SHAPEID).
		GdkEvent*   event      // Event that started this.
) {
	if (dragging) {
		return;
	}
	dragging = true;
	GtkTargetEntry tents[3];    // Set up for dragging.
	tents[0].target      = const_cast<char*>(target);
	tents[1].target      = const_cast<char*>(U7_TARGET_DROPFILE_NAME_MIME);
	tents[2].target      = const_cast<char*>(U7_TARGET_DROPFILE_NAME_MACOSX);
	tents[0].flags       = 0;
	tents[1].flags       = 0;
	tents[2].flags       = 0;
	tents[0].info        = id;
	tents[1].info        = id;
	tents[2].info        = id;
	GtkTargetList* tlist = gtk_target_list_new(&tents[0], 3);
	gtk_drag_begin_with_coordinates(
			draw, tlist,
			static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE), 1,
			event, -1, -1);
	gtk_target_list_unref(tlist);
}

/*
 *  Implement class Shape_single
 */

static inline int extract_value(GtkWidget* widget) {
	if (widget) {
		if (GTK_IS_SPIN_BUTTON(widget)) {
			return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
		}
		if (GTK_IS_ENTRY(widget)) {
			return ExultStudio::get_num_entry(widget, 0);
		}
		if (GTK_IS_FRAME(widget)) {
			return reinterpret_cast<sintptr>(
					g_object_get_data(G_OBJECT(widget), "user_data"));
		}
	}
	return -1;
}

Shape_single::Shape_single(
		GtkWidget* shp, GtkWidget* shpnm, bool (*shvalid)(int), GtkWidget* frm,
		int vgnum, Vga_file* vg, const unsigned char* palbuf, GtkWidget* drw,
		bool hdd)
		: Shape_draw(vg, palbuf, drw), shape(shp), shapename(shpnm),
		  shapevalid(shvalid), frame(frm), vganum(vgnum), hide(hdd),
		  shape_connect(0), frame_connect(0), draw_connect(0), drop_connect(0),
		  hide_connect(0) {
	if (shape && (GTK_IS_SPIN_BUTTON(shape) || GTK_IS_ENTRY(shape))) {
		shape_connect = g_signal_connect(
				G_OBJECT(shape), "changed",
				G_CALLBACK(Shape_single::on_shape_changed), this);
	}
	if (frame && (GTK_IS_SPIN_BUTTON(frame) || GTK_IS_ENTRY(frame))) {
		frame_connect = g_signal_connect(
				G_OBJECT(frame), "changed",
				G_CALLBACK(Shape_single::on_frame_changed), this);
	}
	draw_connect = g_signal_connect(
			G_OBJECT(draw), "draw",
			G_CALLBACK(Shape_single::on_draw_expose_event), this);
	if (vganum >= 0) {
		drop_connect = enable_drop(Shape_single::on_shape_dropped, this);
	}
	if (hide) {
		if (frame) {
			if (GTK_IS_SPIN_BUTTON(frame)) {
				hide_connect = g_signal_connect(
						G_OBJECT(frame), "state-flags-changed",
						G_CALLBACK(Shape_single::on_state_changed), this);
			}
		} else {
			if (GTK_IS_SPIN_BUTTON(shape)) {
				hide_connect = g_signal_connect(
						G_OBJECT(shape), "state-flags-changed",
						G_CALLBACK(Shape_single::on_state_changed), this);
			}
		}
	}
}

Shape_single::~Shape_single() {
	if (shape_connect
		&& g_signal_handler_is_connected(G_OBJECT(shape), shape_connect)) {
		g_signal_handler_disconnect(G_OBJECT(shape), shape_connect);
		shape_connect = 0;
	}
	if (frame_connect
		&& g_signal_handler_is_connected(G_OBJECT(frame), frame_connect)) {
		g_signal_handler_disconnect(G_OBJECT(frame), frame_connect);
		frame_connect = 0;
	}
	if (draw_connect
		&& g_signal_handler_is_connected(G_OBJECT(draw), draw_connect)) {
		g_signal_handler_disconnect(G_OBJECT(draw), draw_connect);
		draw_connect = 0;
	}
	if (drop_connect
		&& g_signal_handler_is_connected(G_OBJECT(draw), drop_connect)) {
		g_signal_handler_disconnect(G_OBJECT(draw), drop_connect);
		drop_connect = 0;
	}
	if (hide_connect
		&& g_signal_handler_is_connected(
				G_OBJECT(frame ? frame : shape), hide_connect)) {
		g_signal_handler_disconnect(
				G_OBJECT(frame ? frame : shape), hide_connect);
		hide_connect = 0;
	}
}

void Shape_single::on_shape_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget);
	auto* single = static_cast<Shape_single*>(user_data);
	if (single->shapename && GTK_IS_LABEL(single->shapename)
		&& GTK_IS_SPIN_BUTTON(widget)) {
		int shnum = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
		if ((shnum == 0) && !(single->shapevalid(0))) {
			shnum = -1;
		}
		const char* nm = ExultStudio::get_instance()->get_shape_name(shnum);
		gtk_label_set_text(GTK_LABEL(single->shapename), nm ? nm : "");
	}
	if (single->hide && !(single->frame) && !(single->hide_connect)
		&& (GTK_IS_ENTRY(widget))) {
		if (strlen(gtk_entry_get_text(GTK_ENTRY(widget))) > 0) {
			gtk_widget_set_visible(single->draw, true);
		} else {
			gtk_widget_set_visible(single->draw, false);
		}
	}
	single->render();
}

void Shape_single::on_frame_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget);
	auto* single = static_cast<Shape_single*>(user_data);
	if (single->hide && !(single->hide_connect) && (GTK_IS_ENTRY(widget))) {
		if (strlen(gtk_entry_get_text(GTK_ENTRY(widget))) > 0) {
			gtk_widget_set_visible(single->draw, true);
		} else {
			gtk_widget_set_visible(single->draw, false);
		}
	}
	single->render();
}

void Shape_single::on_state_changed(
		GtkWidget* widget, GtkStateFlags flags, gpointer user_data) {
	ignore_unused_variable_warning(flags);
	auto* single = static_cast<Shape_single*>(user_data);
	if (!(gtk_widget_get_state_flags(widget) & GTK_STATE_FLAG_INSENSITIVE)) {
		gtk_widget_set_visible(single->draw, true);
	} else {
		gtk_widget_set_visible(single->draw, false);
	}
	single->render();
}

gboolean Shape_single::on_draw_expose_event(
		GtkWidget* widget, cairo_t* cairo, gpointer user_data) {
	ignore_unused_variable_warning(widget);
	auto*        single = static_cast<Shape_single*>(user_data);
	GdkRectangle area   = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	single->set_graphic_context(cairo);
	single->configure();
	int shnum = 0, frnum = 0;
	if (single->shape) {
		shnum = extract_value(single->shape);
	}
	if (single->vganum == U7_SHAPE_SPRITES && shnum >= 0 && single->ifile) {
		frnum = single->ifile->get_num_frames(shnum) / 2;
	}
	if (single->frame) {
		frnum = extract_value(single->frame);
	}
	if ((shnum == 0) && !(single->shapevalid(0))) {
		shnum = -1;
	}
	single->draw_shape_centered(shnum, frnum);
	single->show(
			ZoomDown(area.x), ZoomDown(area.y), ZoomDown(area.width),
			ZoomDown(area.height));
	single->set_graphic_context(nullptr);
	return true;
}

void Shape_single::on_shape_dropped(
		int filenum, int shapenum, int framenum, gpointer user_data) {
	auto* single = static_cast<Shape_single*>(user_data);
	if (filenum == single->vganum) {
		if ((single->shape) && (single->shapevalid(shapenum))) {
			if (GTK_IS_SPIN_BUTTON(single->shape)) {
				gtk_spin_button_set_value(
						GTK_SPIN_BUTTON(single->shape), shapenum);
			} else if (GTK_IS_ENTRY(single->shape)) {
				char* txt = g_strdup_printf("%d", shapenum);
				gtk_entry_set_text(GTK_ENTRY(single->shape), txt);
				g_free(txt);
			} else if (GTK_IS_FRAME(single->shape)) {
				g_object_set_data(
						G_OBJECT(single->shape), "user_data",
						reinterpret_cast<gpointer>(uintptr(shapenum)));
				char* label = g_strdup_printf("Face #%d", shapenum);
				gtk_frame_set_label(GTK_FRAME(single->shape), label);
				g_free(label);
			}
		}
		if (single->frame
			&& ((framenum >= 0
				 && framenum < single->ifile->get_num_frames(shapenum))
				|| (single->ifile->get_num_frames(shapenum) <= 32
					&& framenum >= 32
					&& framenum
							   < (32
								  + single->ifile->get_num_frames(
										  shapenum))))) {
			if (GTK_IS_SPIN_BUTTON(single->frame)) {
				gtk_spin_button_set_value(
						GTK_SPIN_BUTTON(single->frame), framenum);
			} else if (GTK_IS_ENTRY(single->frame)) {
				char* txt = g_strdup_printf("%d", framenum);
				gtk_entry_set_text(GTK_ENTRY(single->frame), txt);
				g_free(txt);
			}
		}
	}
}

/*
 *  Build an Image out of a Shape
 */

GdkPixbuf* ExultStudio::shape_image(
		Vga_file* shpfile, int shnum, int frnum, bool transparent) {
	if (shnum < 0) {
		return nullptr;
	}
	if (frnum < 0) {
		return nullptr;
	}
	Shape_frame* shape = shpfile->get_shape(shnum, frnum);
	if (!shape) {
		return nullptr;
	}
	unsigned char* local_palbuf = palbuf.get();
	if (!local_palbuf) {
		return nullptr;
	}
	const int     w      = shape->get_width();
	const int     h      = shape->get_height();
	const int     xright = shape->get_xright();
	const int     ybelow = shape->get_ybelow();
	Image_buffer8 tbuf(w, h);    // Create buffer to render to.
	tbuf.fill8(0xff);            // Fill with 'transparent' pixel.
	unsigned char* tbits = tbuf.get_bits();
	shape->paint(&tbuf, w - 1 - xright, h - 1 - ybelow);
	// Put shape on a pixmap.
	GdkPixbuf* pixbuf
			= gdk_pixbuf_new(GDK_COLORSPACE_RGB, transparent, 8, w, h);
	guchar*   pixels  = gdk_pixbuf_get_pixels(pixbuf);
	const int rstride = gdk_pixbuf_get_rowstride(pixbuf);
	const int pstride = gdk_pixbuf_get_n_channels(pixbuf);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			guchar*      t = pixels + y * rstride + x * pstride;
			const guchar s = tbits[y * w + x];
			if (transparent) {
				t[0] = (s == 255 ? 0 : (4 * local_palbuf[3 * s]) & 255);
				t[1] = (s == 255 ? 0 : (4 * local_palbuf[3 * s + 1]) & 255);
				t[2] = (s == 255 ? 0 : (4 * local_palbuf[3 * s + 2]) & 255);
				t[3] = (s == 255 ? 0 : 255);
			} else {
				t[0] = ((4 * local_palbuf[3 * s]) & 255);
				t[1] = ((4 * local_palbuf[3 * s + 1]) & 255);
				t[2] = ((4 * local_palbuf[3 * s + 2]) & 255);
			}
		}
	}
	const int zoom_scale = ExultStudio::get_instance()->get_shape_scale();
	if (zoom_scale > 2) {
		// Using GdkPixbuf scaling :
		const bool zoom_bilinear
				= ExultStudio::get_instance()->get_shape_bilinear();
		GdkPixbuf* zoom_pixbuf = gdk_pixbuf_scale_simple(
				pixbuf, (w * zoom_scale) / 2, (h * zoom_scale) / 2,
				(zoom_bilinear ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST));
		g_object_unref(pixbuf);
		pixbuf = zoom_pixbuf;
	}
	return pixbuf;
}
