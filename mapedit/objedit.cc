/**
 ** Objedit.cc - object-editing methods.
 **
 ** Written: 7/29/01 - JSF
 **/

/*
Copyright (C) 2000-2013 The Exult Team

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
#include "u7drag.h"
#include "servemsg.h"
#include "objserial.h"
#include "exult_constants.h"
#include "utils.h"
#include "shapefile.h"
#include "shapedraw.h"
#include "shapevga.h"
#include "ignore_unused_variable_warning.h"

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
C_EXPORT gboolean on_obj_draw_expose_event(
    GtkWidget *widget,      // The view window.
    GdkEventExpose *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, data);
	ExultStudio::get_instance()->show_obj_shape(
	    event->area.x, event->area.y, event->area.width,
	    event->area.height);
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
		objwin = glade_xml_get_widget(app_xml, "obj_window");
		// Note: vgafile can't be null here.
		if (palbuf) {
			obj_draw = new Shape_draw(vgafile->get_ifile(), palbuf.get(),
			                          glade_xml_get_widget(app_xml, "obj_draw"));
			obj_draw->enable_drop(Obj_shape_dropped, this);
		}
	}
	// Init. obj address to null.
	gtk_object_set_user_data(GTK_OBJECT(objwin), nullptr);
	if (!init_obj_window(data, datalen))
		return;
	gtk_widget_show(objwin);
#ifdef _WIN32
	if (first_time || !objdnd)
		Windnd::CreateStudioDropDest(objdnd, objhwnd, Drop_dragged_shape, nullptr, nullptr, this);
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
	gtk_object_set_user_data(GTK_OBJECT(objwin), reinterpret_cast<gpointer>(addr));
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
	GtkWidget *btn = glade_xml_get_widget(app_xml, "obj_frame");
	if (btn) {
		GtkAdjustment *adj = gtk_spin_button_get_adjustment(
		                         GTK_SPIN_BUTTON(btn));
		int nframes = vgafile->get_ifile()->get_num_frames(shape);
		adj->upper = (nframes - 1) | 32; // So we can rotate.
		gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
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
	Game_object *addr = static_cast<Game_object*>(gtk_object_get_user_data(GTK_OBJECT(objwin)));
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
	Shapes_vga_file *shfile = static_cast<Shapes_vga_file *>(vgafile->get_ifile());
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
	if (!obj_draw)
		return;
	obj_draw->configure();
	// Yes, this is kind of redundant...
	int shnum = get_num_entry("obj_shape");
	int frnum = get_num_entry("obj_frame");
	if (!shnum)         // Don't draw shape 0.
		shnum = -1;
	obj_draw->draw_shape_centered(shnum, frnum);
	if (w != -1)
		obj_draw->show(x, y, w, h);
	else
		obj_draw->show();
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


