/**
 ** Objedit.cc - object-editing methods.
 **
 ** Written: 7/29/01 - JSF
 **/

/*
Copyright (C) 2000-2020 The Exult Team

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
#include "ignore_unused_variable_warning.h"

#include "u7drag.h"
#include "servemsg.h"
#include "objserial.h"
#include "exult_constants.h"
#include "utils.h"
#include "shapefile.h"
#include "shapedraw.h"
#include "shapevga.h"

using std::cout;
using std::endl;

class Game_object;

/*
 *  Object window's Okay button.
 */
C_EXPORT void on_obj_okay_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_obj_window();
	ExultStudio::get_instance()->close_obj_window();
}

/*
 *  Object window's Apply button.
 */
C_EXPORT void on_obj_apply_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_obj_window();
}

/*
 *  Object window's Cancel button.
 */
C_EXPORT void on_obj_cancel_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_obj_window();
}

/*
 *  Rotate frame (clockwise).
 */
C_EXPORT void on_obj_rotate_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rotate_obj();
}

/*
 *  Object window's close button.
 */
C_EXPORT gboolean on_obj_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_obj_window();
	return TRUE;
}

/*
 *  Draw shape in object shape area.
 */
gboolean ExultStudio::on_obj_draw_expose_event(
    GtkWidget *widget,      // The view window.
    cairo_t *cairo,
    gpointer data           // -> ExultStudio.
) {
	ignore_unused_variable_warning(widget, data);
	ExultStudio *studio = static_cast<ExultStudio *>(data);
	GdkRectangle area = { 0, 0, 0, 0 };
	gdk_cairo_get_clip_rectangle(cairo, &area);
	studio->obj_draw->set_graphic_context(cairo);
	studio->show_obj_shape(area.x, area.y, area.width, area.height);
	studio->obj_draw->set_graphic_context(nullptr);
	return TRUE;
}

/*
 *  Object shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_obj_shape_changed(
    GtkWidget *widget,
    GdkEventFocus *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	int shnum = studio->get_num_entry("obj_shape");
	int frnum = studio->get_num_entry("obj_frame");
	int nframes =
	    studio->get_vgafile()->get_ifile()->get_num_frames(shnum);
	int xfrnum = frnum & 31;    // Look at unrotated value.
	int newfrnum = xfrnum >= nframes ? 0 : frnum;
	if (newfrnum != frnum) {
		studio->set_spin("obj_frame", newfrnum);
		return TRUE;
	}
	studio->show_obj_shape();
	return TRUE;
}

/*
 *  Object shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_obj_pos_changed(
    GtkWidget *widget,
    GdkEventFocus *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	//++++Maybe later, change pos. immediately?
	return TRUE;
}

/*
 *  Callback for when a shape is dropped on the draw area.
 */

static void Obj_shape_dropped(
    int file,           // U7_SHAPE_SHAPES.
    int shape,
    int frame,
    void *udata
) {
	if (file == U7_SHAPE_SHAPES &&
	        shape >= c_first_obj_shape && shape < c_max_shapes)
		static_cast<ExultStudio *>(udata)->set_obj_shape(shape, frame);
}

#ifdef _WIN32

static void Drop_dragged_shape(int shape, int frame, int x, int y, void *data) {
	cout << "Dropped a shape: " << shape << "," << frame << " " << data << endl;
	ignore_unused_variable_warning(x, y);
	Obj_shape_dropped(U7_SHAPE_SHAPES, shape, frame, data);
}

#endif


/*
 *  Open the object-editing window.
 */

void ExultStudio::open_obj_window(
    unsigned char *data,        // Serialized object.
    int datalen
) {
#ifdef _WIN32
	bool first_time = false;
#endif
	if (!objwin) {          // First time?
#ifdef _WIN32
		first_time = true;
#endif
		objwin = get_widget("obj_window");
		// Note: vgafile can't be null here.
		if (palbuf) {
			obj_draw = new Shape_draw(vgafile->get_ifile(),
			                          palbuf.get(),
			                          get_widget("obj_draw"));
			obj_draw->enable_drop(Obj_shape_dropped, this);
		}
	}
	// Init. obj address to null.
	g_object_set_data(G_OBJECT(objwin), "user_data", nullptr);
	g_signal_connect(G_OBJECT(get_widget("obj_draw")), "draw",
	                 G_CALLBACK(on_obj_draw_expose_event), this);
	if (!init_obj_window(data, datalen))
		return;
	gtk_widget_show(objwin);
#ifdef _WIN32
	if (first_time || !objdnd)
		Windnd::CreateStudioDropDest(objdnd, objhwnd, Drop_dragged_shape,
		                             nullptr, nullptr, this);
#endif
}

/*
 *  Close the object-editing window.
 */

void ExultStudio::close_obj_window(
) {
	if (objwin) {
		gtk_widget_hide(objwin);
#ifdef _WIN32
		Windnd::DestroyStudioDropDest(objdnd, objhwnd);
#endif
	}
}

/*
 *  Init. the object editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_obj_window(
    unsigned char *data,
    int datalen
) {
	Game_object *addr;
	int tx;
	int ty;
	int tz;
	int shape;
	int frame;
	int quality;
	std::string name;
	if (!Object_in(data, datalen, addr, tx, ty, tz, shape, frame,
	               quality, name)) {
		cout << "Error decoding object" << endl;
		return 0;
	}
	// Store address with window.
	g_object_set_data(G_OBJECT(objwin), "user_data", reinterpret_cast<gpointer>(addr));
	// Store name. (Not allowed to change.)
	set_entry("obj_name", name.c_str(), false);
	// Shape/frame, quality.
//	set_entry("obj_shape", shape);
//	set_entry("obj_frame", frame);
//	set_entry("obj_quality", quality);
	// Only allow real objects, not 8x8 flats.
	set_spin("obj_shape", shape, c_first_obj_shape, 8096);
	set_spin("obj_frame", frame);
	set_spin("obj_quality", quality);
	set_spin("obj_x", tx);      // Position.
	set_spin("obj_y", ty);
	set_spin("obj_z", tz);
	// Set limit on frame #.
	GtkWidget *btn = get_widget("obj_frame");
	if (btn) {
		GtkAdjustment *adj = gtk_spin_button_get_adjustment(
		                         GTK_SPIN_BUTTON(btn));
		int nframes = vgafile->get_ifile()->get_num_frames(shape);
		gtk_adjustment_set_upper(adj, (nframes - 1) | 32); // So we can rotate.
		g_signal_emit_by_name(G_OBJECT(adj), "changed");
	}
	return 1;
}

/*
 *  Send updated object info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_obj_window(
) {
	cout << "In save_obj_window()" << endl;
	// Get object address.
	auto *addr = static_cast<Game_object *>(g_object_get_data(G_OBJECT(objwin), "user_data"));
	int tx = get_spin("obj_x");
	int ty = get_spin("obj_y");
	int tz = get_spin("obj_z");
	std::string name(get_text_entry("obj_name"));
//	int shape = get_num_entry("obj_shape");
//	int frame = get_num_entry("obj_frame");
//	int quality = get_num_entry("obj_quality");
	int shape = get_spin("obj_shape");
	int frame = get_spin("obj_frame");
	int quality = get_spin("obj_quality");

	if (Object_out(server_socket, Exult_server::obj, addr, tx, ty, tz,
	               shape, frame, quality, name) == -1) {
		cout << "Error sending object data to server" << endl;
		return 0;
	}
	cout << "Sent object data to server" << endl;
	return 1;
}

/*
 *  Rotate obj. frame 90 degrees clockwise.
 */

void ExultStudio::rotate_obj(
) {
	int shnum = get_num_entry("obj_shape");
	int frnum = get_num_entry("obj_frame");
	if (shnum <= 0)
		return;
	auto *shfile = static_cast<Shapes_vga_file *>(vgafile->get_ifile());
	// Make sure data's been read in.
	shfile->read_info(game_type, true);
	const Shape_info &info = shfile->get_info(shnum);
	frnum = info.get_rotated_frame(frnum, 1);
	set_spin("obj_frame", frnum);
	show_obj_shape();
}

/*
 *  Paint the shape in the draw area.
 */

void ExultStudio::show_obj_shape(
    int x, int y, int w, int h  // Rectangle. w=-1 to show all.
) {
	ignore_unused_variable_warning(x, y, w, h);
	if (!obj_draw)
		return;
	if (w == -1) {
		obj_draw->render();
		return;
	}
	obj_draw->configure();
	// Yes, this is kind of redundant...
	int shnum = get_num_entry("obj_shape");
	int frnum = get_num_entry("obj_frame");
	if (!shnum)         // Don't draw shape 0.
		shnum = -1;
	obj_draw->draw_shape_centered(shnum, frnum);
	obj_draw->show(x, y, w, h);
}

/*
 *  Set object shape.
 */

void ExultStudio::set_obj_shape(
    int shape,
    int frame
) {
	set_spin("obj_shape", shape);
	set_spin("obj_frame", frame);
	const char *nm = get_shape_name(shape);
	set_entry("obj_name", nm ? nm : "", false);
	show_obj_shape();
}


