/**
 ** Shapedraw.h - Manage a drawing area that shows one or more shapes.
 **
 ** Written: 6/2/2001 - JSF
 **/

#ifndef INCL_SHAPEDRAW
#define INCL_SHAPEDRAW 1

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

class Vga_file;
class Shape_frame;
class Image_buffer8;

using Drop_callback
		= void (*)(int filenum, int shapenum, int framenum, void* udata);

#include "studio.h"

/*
 *  The class Shape_draw draws shapes from a .vga file.
 *    It is used in the various Shape Browsers, and by the class Shape_single.
 */
class Shape_draw {
protected:
	Vga_file*      ifile;            // Where the shapes come from.
	GtkWidget*     draw;             // GTK draw area to display them in.
	cairo_t*       drawgc;           // For drawing in 'draw'.
	guint32        drawfg;           // Foreground color.
	Image_buffer8* iwin;             // What we render into.
	ExultRgbCmap*  palette;          // For palette indexed image.
	Drop_callback  drop_callback;    // Called when a shape is dropped here.
	void*          drop_user_data;
	bool           dragging;    // Dragging from here.
public:
	static const int outline_color = 249;    // Palette index of outline color

	Shape_draw(Vga_file* i, const unsigned char* palbuf, GtkWidget* drw);
	virtual ~Shape_draw();

	// Manage graphic context.
	void set_graphic_context(cairo_t* cairo) {
		drawgc = cairo;
	}

	cairo_t* get_graphic_context() {
		return drawgc;
	}

	// Blit onto screen.
	void show(int x, int y, int w, int h);

	void show() {
		GtkAllocation alloc = {0, 0, 0, 0};
		gtk_widget_get_allocation(draw, &alloc);
		show(0, 0, ZoomDown(alloc.width), ZoomDown(alloc.height));
	}

	guint32 get_color(int i) {
		return palette->colors[i];
	}

	virtual void draw_shape(Shape_frame* shape, int x, int y);
	void         draw_shape(int shapenum, int framenum, int x, int y);
	void         draw_shape_outline(
					int shapenum, int framenum, int x, int y, unsigned char color);
	void draw_shape_centered(int shapenum, int framenum, int& x, int& y);
	virtual void render();    // Update what gets shown.
	void         set_background_color(guint32 c);

	void configure();    // Configure when created/resized.
	// Handler for drop.
	static void drag_data_received(
			GtkWidget* widget, GdkDragContext* context, gint x, gint y,
			GtkSelectionData* seldata, guint info, guint time, gpointer udata);
	gulong enable_drop(Drop_callback callback, void* udata);
	void   set_drag_icon(GdkDragContext* context, Shape_frame* shape);
	// Start/end dragging from here.
	void start_drag(const char* target, int id, GdkEvent* event);

	void mouse_up() {
		dragging = false;
	}
};

/*
 * The Shape_single class draws a single shape from a .vga file.
 *   It is used in various places of the Shape Editor.
 *   It tracks a Shape and a Frame GtkSpinButton.
 */

class Shape_shape_single;
class Shape_gump_single;

class Shape_single : public Shape_draw {
protected:
	GtkWidget* shape;             // The ShapeID   holding GtkWidget :
								  //     GtkSpinButton / GtkEntry,
								  //     or GtkFrame ( NPCEditor NPC Face ).
	GtkWidget* shapename;         // The ShapeName holding GtkLabel.
	bool (*shapevalid)(int s);    // The ShapeID   validating lambda.
	GtkWidget* frame;             // The FrameID   holding GtkWidget :
								  //     GtkSpinButton / GtkEntry.
	int    vganum;                // For a Drag and Drop enabled Shape_single :
	bool   hide;                  // Whether the Shape should be hidden.
	gulong shape_connect;    // The Shape Widget g_signal_connect changed ID
	gulong frame_connect;    // The Frame Widget g_signal_connect changed ID
	gulong draw_connect;     // The Draw  Widget g_signal_connect draw ID
	gulong drop_connect;     // The Draw  Widget g_signal_connect drop ID
	gulong hide_connect;     // The Hide  Widget g_signal_connect changed ID

public:
	Shape_single(
			GtkWidget* shp,        // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,      // The ShapeName holding GtkWidget.
			bool (*shvld)(int),    // The ShapeUD   validating lambda.
			GtkWidget* frm,        // The FrameID   holding GtkWidget.
			int        vgnum,      // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*  vg,         // The VGA File for the Shape_draw ctor.
			const unsigned char*
					   palbuf,    // The Palette for the Shape_draw ctor.
			GtkWidget* drw,       // The GtkDrawingArea for the Shape_draw ctor.
			bool       hdd = false);    // Whether the Shape should be hidden.
	~Shape_single() override;
	static void     on_shape_changed(GtkWidget* widget, gpointer user_data);
	static void     on_frame_changed(GtkWidget* widget, gpointer user_data);
	static gboolean on_draw_expose_event(
			GtkWidget* widget, cairo_t* cairo, gpointer user_data);
	static void on_shape_dropped(
			int filenum, int shapenum, int framenum, gpointer user_data);
	static void on_state_changed(
			GtkWidget* widget, GtkStateFlags flags, gpointer user_data);

	virtual Shape_shape_single* get_shape_shape_single() {
		return nullptr;
	}

	virtual Shape_gump_single* get_shape_gump_single() {
		return nullptr;
	}
};

/*
 * The Shape_gump_single class draws a single Gump shape from the gumps.vga.
 *   It is used only in the top left window of the Shape Editor.
 *   It displays the Gump Preview according to the Container + Checkmark
 *      GtkSpinButton and GtkCheckButton widgets.
 */

class Shape_gump_single : public Shape_single {
protected:
	GtkWidget* container_x_widget;
	gulong     container_x_connect;
	GtkWidget* container_y_widget;
	gulong     container_y_connect;
	GtkWidget* container_w_widget;
	gulong     container_w_connect;
	GtkWidget* container_h_widget;
	gulong     container_h_connect;
	GtkWidget* show_container_widget;
	gulong     show_container_connect;
	gulong     show_container_altered;
	GtkWidget* checkmark_x_widget;
	gulong     checkmark_x_connect;
	GtkWidget* checkmark_y_widget;
	gulong     checkmark_y_connect;
	GtkWidget* checkmark_shape_widget;
	gulong     checkmark_shape_connect;
	GtkWidget* show_checkmark_widget;
	gulong     show_checkmark_connect;
	gulong     show_checkmark_altered;

public:
	Shape_gump_single(
			GtkWidget* shp,        // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,      // The ShapeName holding GtkWidget.
			bool (*shvld)(int),    // The ShapeUD   validating lambda.
			GtkWidget* frm,        // The FrameID   holding GtkWidget.
			int        vgnum,      // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*  vg,         // The VGA File for the Shape_draw ctor.
			const unsigned char*
					   palbuf,    // The Palette for the Shape_draw ctor.
			GtkWidget* drw,       // The GtkDrawingArea for the Shape_draw ctor.
			bool       hdd = false);    // Whether the Shape should be hidden.
	~Shape_gump_single() override;
	static gboolean on_draw_expose_event(
			GtkWidget* widget, cairo_t* cairo, gpointer user_data);

	Shape_gump_single* get_shape_gump_single() override {
		return this;
	}

	static void on_widget_changed(GtkWidget* widget, gpointer user_data);
	static void on_widget_state(
			GtkWidget* widget, GtkStateFlags flags, gpointer user_data);
};

/*
 * The Shape_shape_single class draws a single Shape from the shapes.vga.
 *   It is used only in the top left window of the Shape Editor.
 *   It displays the 3D Ouline according to
 *      the 3D GtkSpinButton and GtkCheckButton widgets.
 */

class Shape_shape_single : public Shape_single {
protected:
	GtkWidget* shape_3d_x_widget;
	gulong     shape_3d_x_connect;
	GtkWidget* shape_3d_y_widget;
	gulong     shape_3d_y_connect;
	GtkWidget* shape_3d_z_widget;
	gulong     shape_3d_z_connect;
	GtkWidget* show_shape_3d_widget;
	gulong     show_shape_3d_connect;

public:
	Shape_shape_single(
			GtkWidget* shp,        // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,      // The ShapeName holding GtkWidget.
			bool (*shvld)(int),    // The ShapeUD   validating lambda.
			GtkWidget* frm,        // The FrameID   holding GtkWidget.
			int        vgnum,      // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*  vg,         // The VGA File for the Shape_draw ctor.
			const unsigned char*
					   palbuf,    // The Palette for the Shape_draw ctor.
			GtkWidget* drw,       // The GtkDrawingArea for the Shape_draw ctor.
			bool       hdd = false);    // Whether the Shape should be hidden.
	~Shape_shape_single() override;
	void draw_shape(Shape_frame* shape, int x, int y) override;

	Shape_shape_single* get_shape_shape_single() override {
		return this;
	}

	static void on_widget_changed(GtkWidget* widget, gpointer user_data);
	static void on_widget_state(
			GtkWidget* widget, GtkStateFlags flags, gpointer user_data);
};

#endif
