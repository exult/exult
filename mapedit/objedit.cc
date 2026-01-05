/**
 ** Objedit.cc - object-editing methods.
 **
 ** Written: 7/29/01 - JSF
 **/

/*
Copyright (C) 2000-2022 The Exult Team

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

#include "exult_constants.h"
#include "objserial.h"
#include "servemsg.h"
#include "shapedraw.h"
#include "shapefile.h"
#include "shapevga.h"
#include "studio.h"
#include "u7drag.h"

using std::cout;
using std::endl;

class Game_object;

/*
 *  Callback to mark object window as dirty on any widget change.
 */
static void on_obj_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (!studio->is_obj_window_initializing()) {
		studio->set_obj_window_dirty(true);
	}
}

/*
 *  Object window's Okay button.
 */
C_EXPORT void on_obj_okay_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_obj_window();
	ExultStudio::get_instance()->close_obj_window();
}

/*
 *  Object window's Apply button.
 */
C_EXPORT void on_obj_apply_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_obj_window();
}

/*
 *  Object window's Cancel button.
 */
C_EXPORT void on_obj_cancel_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWindow*   parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
	if (studio->is_obj_window_dirty()
		&& !studio->prompt_for_discard(
				studio->obj_window_dirty, "Object", parent)) {
		return;    // User chose not to discard
	}
	studio->close_obj_window();
}

/*
 *  Rotate frame (clockwise).
 */
C_EXPORT void on_obj_rotate_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rotate_obj();
}

/*
 *  Object window's close button.
 */
C_EXPORT gboolean on_obj_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (studio->is_obj_window_dirty()
		&& !studio->prompt_for_discard(
				studio->obj_window_dirty, "Object", GTK_WINDOW(widget))) {
		return true;    // Block window close
	}
	studio->close_obj_window();
	return true;
}

/*
 *  Object shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_obj_pos_changed(
		GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	//++++Maybe later, change pos. immediately?
	return true;
}

/*
 *  Open the object-editing window.
 */

void ExultStudio::open_obj_window(
		unsigned char* data,    // Serialized object.
		int            datalen) {
	if (!objwin) {    // First time?
		objwin = get_widget("obj_window");
		// Note: vgafile can't be null here.
		if (palbuf) {
			obj_single = new Shape_single(
					get_widget("obj_shape"), get_widget("obj_name"),
					[](int shnum) -> bool {
						return (shnum >= c_first_obj_shape)
							   && (shnum < c_max_shapes);
					},
					get_widget("obj_frame"), U7_SHAPE_SHAPES,
					vgafile->get_ifile(), palbuf.get(), get_widget("obj_draw"));
		}
	}
	// Init. obj address to null.
	g_object_set_data(G_OBJECT(objwin), "user_data", nullptr);
	// Connect signals (only once)
	static bool signals_connected = false;
	if (!signals_connected) {
		connect_widget_signals(objwin, G_CALLBACK(on_obj_changed), nullptr);
		signals_connected = true;
	}
	if (!init_obj_window(data, datalen)) {
		return;
	}
	gtk_widget_set_visible(objwin, true);
}

/*
 *  Close the object-editing window.
 */

void ExultStudio::close_obj_window() {
	if (objwin) {
		gtk_widget_set_visible(objwin, false);
	}
}

/*
 *  Init. the object editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_obj_window(unsigned char* data, int datalen) {
	// Check for unsaved changes before opening new object
	if (objwin && gtk_widget_get_visible(objwin)) {
		if (!prompt_for_discard(
					obj_window_dirty, "Object", GTK_WINDOW(objwin))) {
			return 0;    // User canceled
		}
	}

	// Set initializing flag to prevent marking dirty during setup
	obj_window_initializing = true;
	// Clear dirty flag for new object
	obj_window_dirty = false;

	Game_object* addr;
	int          tx;
	int          ty;
	int          tz;
	int          shape;
	int          frame;
	int          quality;
	std::string  name;
	if (!Object_in(
				data, datalen, addr, tx, ty, tz, shape, frame, quality, name)) {
		cout << "Error decoding object" << endl;
		return 0;
	}
	// Store address with window.
	g_object_set_data(
			G_OBJECT(objwin), "user_data", reinterpret_cast<gpointer>(addr));
	// Store name. (Not allowed to change.)
	const std::string locname(convertToUTF8(name.c_str()));
	set_entry("obj_name", locname.c_str(), false);
	// Shape/frame, quality.
	//	set_entry("obj_shape", shape);
	//	set_entry("obj_frame", frame);
	//	set_entry("obj_quality", quality);
	// Only allow real objects, not 8x8 flats.
	set_spin("obj_shape", shape, c_first_obj_shape, 8096);
	set_spin("obj_frame", frame);
	set_spin("obj_quality", quality);
	set_spin("obj_x", tx);    // Position.
	set_spin("obj_y", ty);
	set_spin("obj_z", tz);
	// Set limit on frame #.
	GtkWidget* btn = get_widget("obj_frame");
	if (btn) {
		GtkAdjustment* adj
				= gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(btn));
		const int nframes = vgafile->get_ifile()->get_num_frames(shape);
		gtk_adjustment_set_upper(
				adj, (nframes - 1) | 32);    // So we can rotate.
		g_signal_emit_by_name(G_OBJECT(adj), "changed");
	}

	// Initialization complete - allow dirty marking now
	obj_window_initializing = false;
	return 1;
}

/*
 *  Send updated object info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_obj_window() {
	cout << "In save_obj_window()" << endl;
	// Get object address.
	auto* addr = static_cast<Game_object*>(
			g_object_get_data(G_OBJECT(objwin), "user_data"));
	const int         tx = get_spin("obj_x");
	const int         ty = get_spin("obj_y");
	const int         tz = get_spin("obj_z");
	const std::string name(get_text_entry("obj_name"));
	const std::string locname(convertFromUTF8(name.c_str()));
	//	int shape = get_num_entry("obj_shape");
	//	int frame = get_num_entry("obj_frame");
	//	int quality = get_num_entry("obj_quality");
	const int shape   = get_spin("obj_shape");
	const int frame   = get_spin("obj_frame");
	const int quality = get_spin("obj_quality");

	if (Object_out(
				server_socket, Exult_server::obj, addr, tx, ty, tz, shape,
				frame, quality, locname)
		== -1) {
		cout << "Error sending object data to server" << endl;
		return 0;
	}
	cout << "Sent object data to server" << endl;
	ExultStudio::get_instance()->set_obj_window_dirty(false);
	return 1;
}

/*
 *  Rotate obj. frame 90 degrees clockwise.
 */

void ExultStudio::rotate_obj() {
	const int shnum = get_num_entry("obj_shape");
	if (shnum <= 0) {
		return;
	}
	auto* shfile = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
	// Make sure data's been read in.
	if (shfile->read_info(game_type, true)) {
		set_shapeinfo_modified();
	}
	const Shape_info& info  = shfile->get_info(shnum);
	int               frnum = get_num_entry("obj_frame");
	frnum                   = info.get_rotated_frame(frnum, 1);
	set_spin("obj_frame", frnum);
	obj_single->render();
}
