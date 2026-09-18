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
class Animation_info;

using Drop_callback = void (*)(int filenum, int shapenum, int framenum, void* udata);

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

	int animation_frame = 0;    // Current animation frame

	int animating_paused = INT_MAX;    // Number of frames to pause animating for, While this is >0 animating_frame wont increment.
									   // If this is INT_MAX animating is disabled

private:
	// Linked List Pointers

	static Shape_draw* list_head;
	Shape_draw*        list_next;

public:
	// A simple iterator class for the linked list
	struct iterator {
		Shape_draw* node;

		iterator(Shape_draw* node) : node(node) {}

		Shape_draw* operator*() {
			return node;
		}

		Shape_draw* operator->() {
			return node;
		}

		iterator& operator++(int) {
			node = node->list_next;
			return *this;
		}

		iterator operator++() {
			iterator ret = *this;
			node         = node->list_next;
			return ret;
		}

		bool operator==(const iterator& other) {
			return node == other.node;
		}

		bool operator!=(const iterator& other) {
			return node != other.node;
		}
	};

	static inline const struct {
		iterator begin() const {
			return list_head;
		}

		iterator end() const {
			return nullptr;
		}
	} iteratable;

	// Call with timeout <= 0 to unpause animating
	// Call with timeout = INT_MAX to stop animating until explicitly unpaused
	// Call with other timeout values to temporarily to pause for the specified number of frames with the animation automatically
	// unpausing
	void PauseAnimating(int timeout = INT_MAX, bool reset_frame = true) {
		if (timeout < 0) {
			timeout = 0;
		}
		animating_paused = timeout;
		if (reset_frame) {
			animation_frame = 0;
		}
	}

	constexpr static int CHANGE_ANIM_PAUSE_FRAMES = 20;

	bool IsAnimatingPaused() {
		return animating_paused > 0;
	}

	bool IsAnimatingStopped() {
		return animating_paused == INT_MAX;
	}

	static const int outline_color = 50;    // Palette index of outline color

	int GetAnimInfoFrame(const Animation_info* aniinfm, unsigned short first_frame, unsigned short nframes, bool pausable = false);
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

	virtual void draw_shape(Shape_frame* shape, int x, int y, bool trans = false);
	void         draw_shape(int shapenum, int framenum, int x, int y, bool trans = false);
	void         draw_shape_outline(int shapenum, int framenum, int x, int y, unsigned char color);
	void         draw_shape_centered(int shapenum, int framenum, int& x, int& y, bool trans = false);
	virtual void render();    // Update what gets shown.
	void         set_background_color(guint32 c);
	void         update_palette(const unsigned char* palbuf, unsigned start, unsigned count);
	virtual void animate();
	void         configure();    // Configure when created/resized.
	// Handler for drop.
	static void drag_data_received(
			GtkWidget* widget, GdkDragContext* context, gint x, gint y, GtkSelectionData* seldata, guint info, guint time,
			gpointer udata);
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
public:
	enum class TA_type {
		Disabled  = 0,
		Enabled   = 1,
		shapeinfo = 2,    // For a shape from Shapes.vgs, use shapeinfo to determine things
		widget    = 3,    // use the checkbox from the shapeinfo widget
	};

	struct WidgetChangedConnect {
		GtkWidget* widget  = nullptr;
		gulong     connect = 0;
		WidgetChangedConnect(
				const char* name, const char* signal, void (*on_widget_changed)(GtkWidget* widget, gpointer user_data),
				gpointer user_data);
		~WidgetChangedConnect();
	};

protected:
	GtkWidget* shape;             // The ShapeID   holding GtkWidget: GtkSpinButton / GtkEntry, or GtkFrame ( NPCEditor NPC Face ).
	GtkWidget* shapename;         // The ShapeName holding GtkLabel.
	bool (*shapevalid)(int s);    // The ShapeID   validating lambda.
	GtkWidget*            frame;               // The FrameID   holding GtkWidget: GtkSpinButton / GtkEntry.
	int                   vganum;              // For a Drag and Drop enabled Shape_single :
	bool                  hide;                // Whether the Shape should be hidden.
	TA_type               translucent;         // How translucent drawing should be handled
	TA_type               animating;           // How shape animation should be handled
	const Animation_info* aniinf = nullptr;    // Ifset use this for animation if animating is not Disabled
	gulong                shape_connect;       // The Shape Widget g_signal_connect changed ID
	gulong                frame_connect;       // The Frame Widget g_signal_connect changed ID
	gulong                draw_connect;        // The Draw  Widget g_signal_connect draw ID
	gulong                drop_connect;        // The Draw  Widget g_signal_connect drop ID
	gulong                hide_connect;        // The Hide  Widget g_signal_connect changed ID

public:
	Shape_single(
			GtkWidget* shp,                                   // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,                                 // The ShapeName holding GtkWidget.
			bool (*shvld)(int),                               // The ShapeUD   validating lambda.
			GtkWidget*           frm,                         // The FrameID   holding GtkWidget.
			int                  vgnum,                       // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*            vg,                          // The VGA File for the Shape_draw ctor.
			const unsigned char* palbuf,                      // The Palette for the Shape_draw ctor.
			GtkWidget*           drw,                         // The GtkDrawingArea for the Shape_draw ctor.
			bool                 hdd = false,                 // Whether the Shape should be hidden.
			TA_type translucent      = TA_type::shapeinfo,    // How translucent drawing should be handled, defaults to shapeinfo
			TA_type animating        = TA_type::shapeinfo     // How shape animation should be handled, defaults to shapeinfo

	);

	~Shape_single() override;
	static void     on_shape_changed(GtkWidget* widget, gpointer user_data);
	static void     on_frame_changed(GtkWidget* widget, gpointer user_data);
	static gboolean on_draw_expose_event(GtkWidget* widget, cairo_t* cairo, gpointer user_data);
	static void     on_shape_dropped(int filenum, int shapenum, int framenum, gpointer user_data);
	static void     on_state_changed(GtkWidget* widget, GtkStateFlags flags, gpointer user_data);

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
			GtkWidget* shp,                 // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,               // The ShapeName holding GtkWidget.
			bool (*shvld)(int),             // The ShapeUD   validating lambda.
			GtkWidget*           frm,       // The FrameID   holding GtkWidget.
			int                  vgnum,     // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*            vg,        // The VGA File for the Shape_draw ctor.
			const unsigned char* palbuf,    // The Palette for the Shape_draw ctor.
			GtkWidget*           drw,       // The GtkDrawingArea for the Shape_draw ctor.
			bool                 hdd         = false,
			TA_type              translucent = TA_type::Disabled);    // Whether the Shape should be hidden.
	~Shape_gump_single() override;
	static gboolean on_draw_expose_event(GtkWidget* widget, cairo_t* cairo, gpointer user_data);

	Shape_gump_single* get_shape_gump_single() override {
		return this;
	}

	static void on_widget_changed(GtkWidget* widget, gpointer user_data);
	static void on_widget_state(GtkWidget* widget, GtkStateFlags flags, gpointer user_data);
};

/*
 * The Shape_shape_single class draws a single Shape from the shapes.vga.
 *   It is used only in the top left window of the Shape Editor.
 *   It displays the 3D Ouline according to
 *      the 3D GtkSpinButton and GtkCheckButton widgets.
 */

class Shape_shape_single : public Shape_single {
protected:
	WidgetChangedConnect shape_3d_x    = {"shinfo_xtiles", "changed", on_widget_changed, this};
	WidgetChangedConnect shape_3d_y    = {"shinfo_ytiles", "changed", on_widget_changed, this};
	WidgetChangedConnect shape_3d_z    = {"shinfo_ztiles", "changed", on_widget_changed, this};
	WidgetChangedConnect show_shape_3d = {"shinfo_tiles_preview", "toggled", on_widget_changed, this};
	WidgetChangedConnect shape_trans   = {"shinfo_transl_check", "toggled", on_widget_changed, this};

	WidgetChangedConnect shape_animated                = {"shinfo_animated_check", "toggled", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_check        = {"shinfo_animation_check", "toggled", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_frcount      = {"shinfo_animation_frcount", "changed", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_frtype       = {"shinfo_animation_frtype", "toggled", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_type         = {"shinfo_animation_type", "changed", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_ticks        = {"shinfo_animation_ticks", "changed", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_freezefirst  = {"shinfo_animation_freezefirst", "changed", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_rectype      = {"shinfo_animation_rectype", "toggled", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_recycle      = {"shinfo_animation_recycle", "changed", on_widget_changed, this};
	WidgetChangedConnect shinfo_animation_freezechance = {"shinfo_animation_freezechance", "changed", on_widget_changed, this};

	std::unique_ptr<Animation_info> up_aniinf;

public:
	Shape_shape_single(
			GtkWidget* shp,                 // The ShapeID   holding GtkWidget.
			GtkWidget* shpnm,               // The ShapeName holding GtkWidget.
			bool (*shvld)(int),             // The ShapeUD   validating lambda.
			GtkWidget*           frm,       // The FrameID   holding GtkWidget.
			int                  vgnum,     // The D&D U7_SHAPE_xxx VGA file category.
			Vga_file*            vg,        // The VGA File for the Shape_draw ctor.
			const unsigned char* palbuf,    // The Palette for the Shape_draw ctor.
			GtkWidget*           drw,       // The GtkDrawingArea for the Shape_draw ctor.
			bool                 hdd = false);              // Whether the Shape should be hidden.
	~Shape_shape_single() override;
	void draw_shape(Shape_frame* shape, int x, int y, bool trans) override;

	Shape_shape_single* get_shape_shape_single() override {
		return this;
	}

	static void on_widget_changed(GtkWidget* widget, gpointer user_data);
	static void on_widget_state(GtkWidget* widget, GtkStateFlags flags, gpointer user_data);
	void        setup_aniinf();
};

#endif
