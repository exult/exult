/**
 ** Contedit.cc - container-editing methods.
 **
 ** Written: 7/18/05 - Marzo Junior
 ** Based on objedit.cc by JSF
 **/

/*
Copyright (C) 2005-2013 The Exult Team

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

class Container_game_object;

/*
 *  Container window's Okay button.
 */
C_EXPORT void on_cont_okay_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_cont_window();
	ExultStudio::get_instance()->close_cont_window();
}

/*
 *  Container window's Apply button.
 */
C_EXPORT void on_cont_apply_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_cont_window();
}

/*
 *  Container window's Cancel button.
 */
C_EXPORT void on_cont_cancel_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_cont_window();
}

/*
 *  Display the container's gump.
 */
C_EXPORT void on_cont_show_gump_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	cout << "In on_cont_show_gump_clicked()" << endl;
	unsigned char data[Exult_server::maxlength];
	// Get container address.
	uintptr addr = reinterpret_cast<uintptr>(gtk_object_get_user_data(
	                         GTK_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn)))));
	unsigned char *ptr = &data[0];
	Serial_out io(ptr);
	io << addr;

	ExultStudio::get_instance()->send_to_server(Exult_server::cont_show_gump, data, ptr - data);
	cout << "Sent container data to server" << endl;
}

/*
 *  Rotate frame (clockwise).
 */
C_EXPORT void on_cont_rotate_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rotate_cont();
}

/*
 *  Container window's close button.
 */
C_EXPORT gboolean on_cont_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_cont_window();
	return TRUE;
}

/*
 *  Draw shape in container shape area.
 */
C_EXPORT gboolean on_cont_draw_expose_event(
    GtkWidget *widget,      // The view window.
    GdkEventExpose *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, data);
	ExultStudio::get_instance()->show_cont_shape(
	    event->area.x, event->area.y, event->area.width,
	    event->area.height);
	return TRUE;
}

/*
 *  Container shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_cont_shape_changed(
    GtkWidget *widget,
    GdkEventFocus *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	int shnum = studio->get_num_entry("cont_shape");
	int frnum = studio->get_num_entry("cont_frame");
	int nframes =
	    studio->get_vgafile()->get_ifile()->get_num_frames(shnum);
	int xfrnum = frnum & 31;    // Look at unrotated value.
	int newfrnum = xfrnum >= nframes ? 0 : frnum;
	if (newfrnum != frnum) {
		studio->set_spin("cont_frame", newfrnum);
		return TRUE;
	}
	studio->show_cont_shape();
	return TRUE;
}

/*
 *  Container shape/frame # changed, so update shape displayed.
 */
C_EXPORT gboolean on_cont_pos_changed(
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

static void cont_shape_dropped(
    int file,           // U7_SHAPE_SHAPES.
    int shape,
    int frame,
    void *udata
) {
	if (file == U7_SHAPE_SHAPES &&
	        shape >= c_first_obj_shape && shape < c_max_shapes)
		reinterpret_cast<ExultStudio *>(udata)->set_cont_shape(shape, frame);
}

#ifdef _WIN32

static void Drop_dragged_shape(int shape, int frame, int x, int y, void *data) {
	cout << "Dropped a shape: " << shape << "," << frame << " " << data << endl;
	ignore_unused_variable_warning(x, y);
	cont_shape_dropped(U7_SHAPE_SHAPES, shape, frame, data);
}

#endif


/*
 *  Open the container-editing window.
 */

void ExultStudio::open_cont_window(
    unsigned char *data,        // Serialized object.
    int datalen
) {
#ifdef _WIN32
	bool first_time = false;
#endif
	if (!contwin) {         // First time?
#ifdef _WIN32
		first_time = true;
#endif
		contwin = glade_xml_get_widget(app_xml, "cont_window");
		// Note: vgafile can't be null here.
		if (palbuf) {
			cont_draw = new Shape_draw(vgafile->get_ifile(), palbuf.get(),
			                           glade_xml_get_widget(app_xml, "cont_draw"));
			cont_draw->enable_drop(cont_shape_dropped, this);
		}
	}
	// Init. cont address to null.
	gtk_object_set_user_data(GTK_OBJECT(contwin), nullptr);
	if (!init_cont_window(data, datalen))
		return;
	gtk_widget_show(contwin);
#ifdef _WIN32
	if (first_time || !contdnd)
		Windnd::CreateStudioDropDest(contdnd, conthwnd, Drop_dragged_shape, nullptr, nullptr, this);
#endif
}

/*
 *  Close the container-editing window.
 */

void ExultStudio::close_cont_window(
) {
	if (contwin) {
		gtk_widget_hide(contwin);
#ifdef _WIN32
		Windnd::DestroyStudioDropDest(contdnd, conthwnd);
#endif
	}
}

/*
 *  Init. the container editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_cont_window(
    unsigned char *data,
    int datalen
) {
	Container_game_object *addr;
	int tx;
	int ty;
	int tz;
	int shape;
	int frame;
	int quality;
	std::string name;
	unsigned char res;
	bool invis;
	bool can_take;
	if (!Container_in(data, datalen, addr, tx, ty, tz, shape, frame,
	                  quality, name, res, invis, can_take)) {
		cout << "Error decoding container" << endl;
		return 0;
	}
	// Store address with window.
	gtk_object_set_user_data(GTK_OBJECT(contwin), reinterpret_cast<gpointer>(addr));
	// Store name. (Not allowed to change.)
	set_entry("cont_name", name.c_str(), false);
	// Shape/frame, quality.
	// Only allow real objects, not 8x8 flats.
	set_spin("cont_shape", shape, c_first_obj_shape, 8096);
	set_spin("cont_frame", frame);
	set_spin("cont_quality", quality);
	set_spin("cont_x", tx);     // Position.
	set_spin("cont_y", ty);
	set_spin("cont_z", tz);
	set_spin("cont_resistance", res);
	set_toggle("cont_invisible", invis);
	set_toggle("cont_okay_to_take", can_take);
	// Set limit on frame #.
	GtkWidget *btn = glade_xml_get_widget(app_xml, "cont_frame");
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
 *  Send updated container info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_cont_window(
) {
	cout << "In save_cont_window()" << endl;
	// Get container address.
	Container_game_object *addr = static_cast<Container_game_object*>(gtk_object_get_user_data(GTK_OBJECT(contwin)));
	int tx = get_spin("cont_x");
	int ty = get_spin("cont_y");
	int tz = get_spin("cont_z");
	std::string name(get_text_entry("cont_name"));
	int shape = get_spin("cont_shape");
	int frame = get_spin("cont_frame");
	int quality = get_spin("cont_quality");
	unsigned char res = get_spin("cont_resistance");
	bool invis = get_toggle("cont_invisible");
	bool can_take = get_toggle("cont_okay_to_take");

	if (Container_out(server_socket, addr, tx, ty, tz,
	                  shape, frame, quality, name, res, invis, can_take) == -1) {
		cout << "Error sending container data to server" << endl;
		return 0;
	}
	cout << "Sent container data to server" << endl;
	return 1;
}

/*
 *  Rotate cont. frame 90 degrees clockwise.
 */

void ExultStudio::rotate_cont(
) {
	int shnum = get_num_entry("cont_shape");
	int frnum = get_num_entry("cont_frame");
	if (shnum <= 0)
		return;
	Shapes_vga_file *shfile = static_cast<Shapes_vga_file *>(vgafile->get_ifile());
	// Make sure data's been read in.
	shfile->read_info(game_type, true);
	const Shape_info &info = shfile->get_info(shnum);
	frnum = info.get_rotated_frame(frnum, 1);
	set_spin("cont_frame", frnum);
	show_cont_shape();
}

/*
 *  Paint the shape in the draw area.
 */

void ExultStudio::show_cont_shape(
    int x, int y, int w, int h  // Rectangle. w=-1 to show all.
) {
	if (!cont_draw)
		return;
	cont_draw->configure();
	// Yes, this is kind of redundant...
	int shnum = get_num_entry("cont_shape");
	int frnum = get_num_entry("cont_frame");
	if (!shnum)         // Don't draw shape 0.
		shnum = -1;
	cont_draw->draw_shape_centered(shnum, frnum);
	if (w != -1)
		cont_draw->show(x, y, w, h);
	else
		cont_draw->show();
}

/*
 *  Set container shape.
 */

void ExultStudio::set_cont_shape(
    int shape,
    int frame
) {
	set_spin("cont_shape", shape);
	set_spin("cont_frame", frame);
	const char *nm = get_shape_name(shape);
	set_entry("cont_name", nm ? nm : "", false);
	show_cont_shape();
}


