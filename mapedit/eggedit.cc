/**
 ** Eggedit.cc - Egg-editing methods.
 **
 ** Written: 6/8/01 - JSF
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
using std::string;

class Egg_object;

/*
 *  Open egg window.
 */

C_EXPORT void on_open_egg_activate(
    GtkMenuItem     *menuitem,
    gpointer         user_data
) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->open_egg_window();
}

/*
 *  Egg window's Apply button.
 */
C_EXPORT void on_egg_apply_btn_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_egg_window();
}

/*
 *  Egg window's Cancel button.
 */
C_EXPORT void on_egg_cancel_btn_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_egg_window();
}

/*
 *  Egg window's close button.
 */
C_EXPORT gboolean on_egg_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_egg_window();
	return TRUE;
}

/*
 *  Egg 'usecode' panel's 'browse' button.
 */
C_EXPORT void on_egg_browse_usecode_clicked(
    GtkButton *button,
    gpointer         user_data
) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	const char *uc = studio->browse_usecode(true);
	if (*uc)
		studio->set_entry("usecode_number", uc, true);
}

/*
 *  "Teleport coords" toggled.
 */
C_EXPORT void on_teleport_coord_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	studio->set_sensitive("teleport_x", on);
	studio->set_sensitive("teleport_y", on);
	studio->set_sensitive("teleport_z", on);
	studio->set_sensitive("teleport_eggnum", !on);
}

/*
 *  Open the egg-editing window.
 */

void ExultStudio::open_egg_window(
    unsigned char *data,        // Serialized egg, or null.
    int datalen
) {
	bool first_time = false;
	if (!eggwin) {          // First time?
		first_time = true;
		eggwin = get_widget("egg_window");
		if (vgafile && palbuf) {
			egg_monster_single = new Shape_single(
			    get_widget("monst_shape"), nullptr,
			    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
			                                (shnum < c_max_shapes); },
			    get_widget("monst_frame"),
			    U7_SHAPE_SHAPES,
			    vgafile->get_ifile(),
			    palbuf.get(),
			    get_widget("egg_monster_draw"));
			egg_missile_single = new Shape_single(
			    get_widget("missile_shape"), nullptr,
			    [](int shnum)->bool{ return (shnum >= 0) &&
			                                (shnum < c_max_shapes); },
			    nullptr,
			    U7_SHAPE_SHAPES,
			    vgafile->get_ifile(),
			    palbuf.get(),
			    get_widget("missile_draw"), true);
		}
		egg_ctx = gtk_statusbar_get_context_id(
		              GTK_STATUSBAR(get_widget("egg_status")), "Egg Editor");
	}
	// Init. egg address to null.
	g_object_set_data(G_OBJECT(eggwin), "user_data", nullptr);
	// Make 'apply' sensitive.
	gtk_widget_set_sensitive(get_widget("egg_apply_btn"), true);
	remove_statusbar("egg_status", egg_ctx, egg_status_id);
	if (data) {
		if (!init_egg_window(data, datalen))
			return;
	} else if (first_time) {    // Init. empty dialog first time.
		set_toggle("teleport_coord", true);
		set_sensitive("teleport_x", true);
		set_sensitive("teleport_y", true);
		set_sensitive("teleport_z", true);
		set_sensitive("teleport_eggnum", false);
	}
	gtk_widget_show(eggwin);
}

/*
 *  Close the egg-editing window.
 */

void ExultStudio::close_egg_window(
) {
	if (eggwin) {
		gtk_widget_hide(eggwin);
	}
}

/*
 *  Init. the egg editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_egg_window(
    unsigned char *data,
    int datalen
) {
	Egg_object *addr;
	int tx;
	int ty;
	int tz;
	int shape;
	int frame;
	int type;
	int criteria;
	int probability;
	int distance;
	bool nocturnal;
	bool once;
	bool hatched;
	bool auto_reset;
	int data1;
	int data2;
	int data3;
	string str1;
	if (!Egg_object_in(data, datalen, addr, tx, ty, tz, shape, frame,
	                   type, criteria, probability, distance,
	                   nocturnal, once, hatched, auto_reset,
	                   data1, data2, data3, str1)) {
		cout << "Error decoding egg" << endl;
		return 0;
	}
	// Store address with window.
	g_object_set_data(G_OBJECT(eggwin), "user_data", addr);
	GtkWidget *notebook = get_widget("notebook1");
	if (notebook)           // 1st is monster (1).
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), type - 1);
	set_spin("probability", probability);
	set_spin("distance", distance);
	set_optmenu("criteria", criteria);
	set_toggle("nocturnal", nocturnal);
	set_toggle("once", once);
	set_toggle("hatched", hatched);
	set_toggle("autoreset", auto_reset);
	switch (type) {         // Set notebook page.
	case 1: {           // Monster:
		if (data3 > 0) {    // Exult extension.
			set_entry("monst_shape", data3);
			set_entry("monst_frame", data2 & 0xff);
		} else {
			set_entry("monst_shape", data2 & 1023);
			set_entry("monst_frame", data2 >> 10);
		}
		set_optmenu("monst_schedule", data1 >> 8);
		set_optmenu("monst_align", data1 & 3);
		set_spin("monst_count", (data1 & 0xff) >> 2);
		break;
	}
	case 2:             // Jukebox:
		set_spin("juke_song", data1 & 0xff);
		set_toggle("juke_cont", (data1 >> 8) & 0x01);
		break;
	case 3:             // Sound effect:
		set_spin("sfx_number", data1 & 0xff);
		set_toggle("sfx_cont", (data1 >> 8) & 0x01);
		break;
	case 4:             // Voice:
		set_spin("speech_number", data1 & 0xff);
		break;
	case 5:             // Usecode:
		if (!str1.empty())
			set_entry("usecode_number", str1.c_str(), true);
		else
			set_entry("usecode_number", data2, true);
		set_spin("usecode_quality", data1 & 0xff);
		break;
	case 6:             // Missile:
		set_entry("missile_shape", data1);
		set_optmenu("missile_dir", data2 & 0xff);
		set_spin("missile_delay", data2 >> 8);
		break;
	case 7: {           // Teleport:
		int qual = data1 & 0xff;
		if (qual == 255) {
			set_toggle("teleport_coord", true);
			int schunk = data1 >> 8;
			set_entry("teleport_x",
			          (schunk % 12)*c_tiles_per_schunk + (data2 & 0xff),
			          true);
			set_entry("teleport_y",
			          (schunk / 12)*c_tiles_per_schunk + (data2 >> 8),
			          true);
			set_entry("teleport_z", data3 & 0xff);
			set_spin("teleport_eggnum", 0, false);
		} else {        // Egg #.
			set_toggle("teleport_coord", false);
			set_entry("teleport_x", 0, false, false);
			set_entry("teleport_y", 0, false, false);
			set_entry("teleport_z", 0, false, false);
			set_spin("teleport_eggnum", qual);
		}
		break;
	}
	case 8:             // Weather:
		set_optmenu("weather_type", data1 & 0xff);
		set_spin("weather_length", data1 >> 8);
		break;
	case 9:             // Path:
		set_spin("pathegg_num", data1 & 0xff);
		break;
	case 10:            // Button:
		set_spin("btnegg_distance", data1 & 0xff);
		break;
	case 11: {          // Intermap:
		int mapnum = data1 & 0xff;
		int schunk = data1 >> 8;
		set_spin("intermap_mapnum", mapnum);
		set_entry("intermap_x",
		          (schunk % 12)*c_tiles_per_schunk + (data2 & 0xff), true);
		set_entry("intermap_y",
		          (schunk / 12)*c_tiles_per_schunk + (data2 >> 8), true);
		set_entry("intermap_z", data3 & 0xff);
		break;
	}
	default:
		break;
	}
	return 1;
}

/*
 *  Callback for when user clicked where egg should be inserted.
 */

static void Egg_response(
    Exult_server::Msg_type id,
    const unsigned char *data,
    int datalen,
    void * /* client */
) {
	ignore_unused_variable_warning(data, datalen);
	if (id == Exult_server::user_responded)
		ExultStudio::get_instance()->close_egg_window();
	//+++++cancel??
}

/*
 *  Send updated egg info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_egg_window(
) {
	cout << "In save_egg_window()" << endl;
	// Get egg (null if creating new).
	int tx = -1;
	int ty = -1;
	int tz = -1;  // +++++For now.
	int shape = -1;
	int frame = -1; // For now.
	int type = -1;
	GtkWidget *notebook = get_widget("notebook1");
	if (notebook)           // 1st is monster (1).
		type = 1 + gtk_notebook_get_current_page(
		           GTK_NOTEBOOK(notebook));
	else {
		cout << "Can't find notebook widget" << endl;
		return 0;
	}
	int criteria = get_optmenu("criteria");
	int probability = get_spin("probability");
	int distance = get_spin("distance");
	bool nocturnal = get_toggle("nocturnal");
	bool once = get_toggle("once");
	bool hatched = get_toggle("hatched");
	bool auto_reset = get_toggle("autoreset");
	int data1 = -1;
	int data2 = -1;
	int data3 = 0;
	string str1;
	switch (type) {         // Set notebook page.
	case 1: {           // Monster:
		int shnum = get_num_entry("monst_shape");
		int frnum = get_num_entry("monst_frame");
		if (shnum >= 1024 or frnum >= 64) {
			data3 = shnum;
			data2 = frnum & 0xff;
		} else
			data2 = (shnum & 1023) + (frnum << 10);
		data1 = (get_optmenu("monst_schedule") << 8) +
		        (get_optmenu("monst_align") & 3) +
		        (get_spin("monst_count") << 2);
	}
	break;
	case 2:             // Jukebox:
		data1 = (get_spin("juke_song") & 0xff) +
		        (get_toggle("juke_cont") ? (1 << 8) : 0);
		break;
	case 3:             // Sound effect:
		data1 = (get_spin("sfx_number") & 0xff) +
		        (get_toggle("sfx_cont") ? (1 << 8) : 0);
		break;
	case 4:             // Voice:
		data1 = get_spin("speech_number") & 0xff;
		break;
	case 5:             // Usecode:
		data2 = get_num_entry("usecode_number");
		if (!data2)
			str1 = get_text_entry("usecode_number");
		data1 = get_spin("usecode_quality") & 0xff;
		break;
	case 6:             // Missile:
		data1 = get_num_entry("missile_shape");
		data2 = (get_optmenu("missile_dir") & 0xff) +
		        (get_spin("missile_delay") << 8);
		break;
	case 7:             // Teleport:
		if (get_toggle("teleport_coord")) {
			// Abs. coords.
			int tx = get_num_entry("teleport_x");
			int ty = get_num_entry("teleport_y");
			int tz = get_num_entry("teleport_z");
			data3 = tz;
			data2 = (tx & 0xff) + ((ty & 0xff) << 8);
			int sx = tx / c_tiles_per_schunk;
			int sy = ty / c_tiles_per_schunk;
			data1 = 255 + ((sy * 12 + sx) << 8);
		} else          // Egg #.
			data1 = get_spin("teleport_eggnum") & 0xff;
		break;
	case 8:             // Weather:
		data1 = (get_optmenu("weather_type") & 0xff) +
		        (get_spin("weather_length") << 8);
		break;
	case 9:             // Path:
		data1 = get_spin("pathegg_num") & 0xff;
		break;
	case 10:            // Button:
		data1 = get_spin("btnegg_distance") & 0xff;
		break;
	case 11: {          // Intermap.
		int tx = get_num_entry("intermap_x");
		int ty = get_num_entry("intermap_y");
		int tz = get_num_entry("intermap_z");
		int mapnum = get_spin("intermap_mapnum");
		data3 = tz;
		data2 = (tx & 0xff) + ((ty & 0xff) << 8);
		int sx = tx / c_tiles_per_schunk;
		int sy = ty / c_tiles_per_schunk;
		data1 = mapnum + ((sy * 12 + sx) << 8);
		break;
	}
	default:
		cout << "Unknown egg type" << endl;
		return 0;
	}
	auto *addr = static_cast<Egg_object *>(g_object_get_data(G_OBJECT(eggwin), "user_data"));
	if (Egg_object_out(server_socket, addr, tx, ty, tz,
	                   shape, frame, type, criteria, probability, distance,
	                   nocturnal, once, hatched, auto_reset,
	                   data1, data2, data3, str1) == -1) {
		cout << "Error sending egg data to server" << endl;
		return 0;
	}
	cout << "Sent egg data to server" << endl;
	if (!addr) {
		egg_status_id = set_statusbar("egg_status", egg_ctx,
		                              "Click on map at place to insert egg");
		// Make 'apply' insensitive.
		gtk_widget_set_sensitive(get_widget("egg_apply_btn"), false);
		waiting_for_server = Egg_response;
		return 1;       // Leave window open.
	}
	close_egg_window();
	return 1;
}
