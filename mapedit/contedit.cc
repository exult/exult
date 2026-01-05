/**
 ** Contedit.cc - container-editing methods.
 **
 ** Written: 7/18/05 - Marzo Junior
 ** Based on objedit.cc by JSF
 **/

/*
Copyright (C) 2005-2022 The Exult Team

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

class Container_game_object;

/*
 *  Callback to mark container window as dirty on any widget change.
 */
static void on_cont_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (!studio->is_cont_window_initializing()) {
		studio->set_cont_window_dirty(true);
	}
}

/*
 *  Container window's Okay button.
 */
C_EXPORT void on_cont_okay_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_cont_window();
	ExultStudio::get_instance()->close_cont_window();
}

/*
 *  Container window's Apply button.
 */
C_EXPORT void on_cont_apply_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_cont_window();
}

/*
 *  Container window's Cancel button.
 */
C_EXPORT void on_cont_cancel_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWindow*   parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
	if (studio->is_cont_window_dirty()
		&& !studio->prompt_for_discard(
				studio->cont_window_dirty, "Container", parent)) {
		return;    // User chose not to discard
	}
	studio->close_cont_window();
}

/*
 *  Display the container's gump.
 */
C_EXPORT void on_cont_show_gump_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	cout << "In on_cont_show_gump_clicked()" << endl;
	unsigned char data[Exult_server::maxlength];
	// Get container address.
	auto           addr = reinterpret_cast<uintptr>(g_object_get_data(
            G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	unsigned char* ptr  = &data[0];
	Serial_out     io(ptr);
	io << addr;

	ExultStudio::get_instance()->send_to_server(
			Exult_server::cont_show_gump, data, ptr - data);
	cout << "Sent container data to server" << endl;
}

/*
 *  Rotate frame (clockwise).
 */
C_EXPORT void on_cont_rotate_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rotate_cont();
}

/*
 *  Container window's close button.
 */
C_EXPORT gboolean on_cont_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (studio->is_cont_window_dirty()
		&& !studio->prompt_for_discard(
				studio->cont_window_dirty, "Container", GTK_WINDOW(widget))) {
		return true;    // Block window close
	}
	studio->close_cont_window();
	return true;
}

/*
 *  Container shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_cont_pos_changed(
		GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	//++++Maybe later, change pos. immediately?
	return true;
}

/*
 *  Open the container-editing window.
 */

void ExultStudio::open_cont_window(
		unsigned char* data,    // Serialized object.
		int            datalen) {
	if (!contwin) {    // First time?
		contwin = get_widget("cont_window");
		// Note: vgafile can't be null here.
		if (palbuf) {
			cont_single = new Shape_single(
					get_widget("cont_shape"), get_widget("cont_name"),
					[](int shnum) -> bool {
						return (shnum >= c_first_obj_shape)
							   && (shnum < c_max_shapes);
					},
					get_widget("cont_frame"), U7_SHAPE_SHAPES,
					vgafile->get_ifile(), palbuf.get(),
					get_widget("cont_draw"));
		}
	}
	// Init. cont address to null.
	g_object_set_data(G_OBJECT(contwin), "user_data", nullptr);
	// Connect signals (only once)
	static bool signals_connected = false;
	if (!signals_connected) {
		connect_widget_signals(contwin, G_CALLBACK(on_cont_changed), nullptr);
		signals_connected = true;
	}
	if (!init_cont_window(data, datalen)) {
		return;
	}
	gtk_widget_set_visible(contwin, true);
}

/*
 *  Close the container-editing window.
 */

void ExultStudio::close_cont_window() {
	if (contwin) {
		gtk_widget_set_visible(contwin, false);
	}
}

/*
 *  Init. the container editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_cont_window(unsigned char* data, int datalen) {
	// Check for unsaved changes before opening new container
	if (contwin && gtk_widget_get_visible(contwin)) {
		if (!prompt_for_discard(
					cont_window_dirty, "Container", GTK_WINDOW(contwin))) {
			return 0;    // User canceled
		}
	}

	// Set initializing flag to prevent marking dirty during setup
	cont_window_initializing = true;
	// Clear dirty flag for new container
	cont_window_dirty = false;

	Container_game_object* addr;
	int                    tx;
	int                    ty;
	int                    tz;
	int                    shape;
	int                    frame;
	int                    quality;
	std::string            name;
	unsigned char          res;
	bool                   invis;
	bool                   can_take;
	if (!Container_in(
				data, datalen, addr, tx, ty, tz, shape, frame, quality, name,
				res, invis, can_take)) {
		cout << "Error decoding container" << endl;
		return 0;
	}
	// Store address with window.
	g_object_set_data(
			G_OBJECT(contwin), "user_data", reinterpret_cast<gpointer>(addr));
	// Store name. (Not allowed to change.)
	set_entry("cont_name", name.c_str(), false);
	// Shape/frame, quality.
	// Only allow real objects, not 8x8 flats.
	set_spin("cont_shape", shape, c_first_obj_shape, 8096);
	set_spin("cont_frame", frame);
	set_spin("cont_quality", quality);
	set_spin("cont_x", tx);    // Position.
	set_spin("cont_y", ty);
	set_spin("cont_z", tz);
	set_spin("cont_resistance", res);
	set_toggle("cont_invisible", invis);
	set_toggle("cont_okay_to_take", can_take);
	// Set limit on frame #.
	GtkWidget* btn = get_widget("cont_frame");
	if (btn) {
		GtkAdjustment* adj
				= gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(btn));
		const int nframes = vgafile->get_ifile()->get_num_frames(shape);
		gtk_adjustment_set_upper(
				adj, (nframes - 1) | 32);    // So we can rotate.
		g_signal_emit_by_name(G_OBJECT(adj), "changed");
	}

	// Initialization complete - allow dirty marking now
	cont_window_initializing = false;
	return 1;
}

/*
 *  Send updated container info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_cont_window() {
	cout << "In save_cont_window()" << endl;
	// Get container address.
	auto* addr = static_cast<Container_game_object*>(
			g_object_get_data(G_OBJECT(contwin), "user_data"));
	const int           tx = get_spin("cont_x");
	const int           ty = get_spin("cont_y");
	const int           tz = get_spin("cont_z");
	const std::string   name(get_text_entry("cont_name"));
	const int           shape    = get_spin("cont_shape");
	const int           frame    = get_spin("cont_frame");
	const int           quality  = get_spin("cont_quality");
	const unsigned char res      = get_spin("cont_resistance");
	const bool          invis    = get_toggle("cont_invisible");
	const bool          can_take = get_toggle("cont_okay_to_take");

	if (Container_out(
				server_socket, addr, tx, ty, tz, shape, frame, quality, name,
				res, invis, can_take)
		== -1) {
		cout << "Error sending container data to server" << endl;
		return 0;
	}
	cout << "Sent container data to server" << endl;
	ExultStudio::get_instance()->set_cont_window_dirty(false);
	return 1;
}

/*
 *  Rotate cont. frame 90 degrees clockwise.
 */

void ExultStudio::rotate_cont() {
	const int shnum = get_num_entry("cont_shape");
	if (shnum <= 0) {
		return;
	}
	auto* shfile = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
	// Make sure data's been read in.
	if (shfile->read_info(game_type, true)) {
		set_shapeinfo_modified();
	}
	const Shape_info& info  = shfile->get_info(shnum);
	int               frnum = get_num_entry("cont_frame");
	frnum                   = info.get_rotated_frame(frnum, 1);
	set_spin("cont_frame", frnum);
	cont_single->render();
}
