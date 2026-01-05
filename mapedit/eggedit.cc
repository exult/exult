/**
 ** Eggedit.cc - Egg-editing methods.
 **
 ** Written: 6/8/01 - JSF
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
using std::string;

class Egg_object;

/*
 *  Callback to mark egg window as dirty on any widget change.
 */
static void on_egg_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (!studio->is_egg_window_initializing()) {
		studio->set_egg_window_dirty(true);
	}
}

/*
 *  Helper functions to wrap GLib pointer conversions and avoid old-style cast
 * warnings.
 */
inline gpointer int_to_gpointer(int value) {
	return reinterpret_cast<gpointer>(static_cast<intptr_t>(value));
}

inline int gpointer_to_int(gpointer ptr) {
	return static_cast<int>(reinterpret_cast<intptr_t>(ptr));
}

/*
 *  Enable/disable locate button based on whether coordinates or egg number are
 * valid.
 */
static void Update_teleport_locate_button(ExultStudio* studio, int tx, int ty) {
	// Check if using coordinates or egg number mode.
	const bool using_coords = studio->get_toggle("teleport_coord");
	bool       has_location = false;

	if (using_coords) {
		// Enable if either x or y coordinate is non-zero.
		has_location = (tx != 0 || ty != 0);
	} else {
		// Enable if egg number is non-zero.
		const int egg_num = studio->get_spin("teleport_eggnum");
		has_location      = (egg_num != 0);
	}

	studio->set_sensitive("teleport_locate", has_location);
}

/*
 *  Open egg window.
 */

C_EXPORT void on_open_egg_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->open_egg_window();
}

/*
 *  Egg window's OK button.
 */
C_EXPORT void on_egg_okay_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_egg_window();
	ExultStudio::get_instance()->close_egg_window();
}

/*
 *  Egg window's Apply button.
 */
C_EXPORT void on_egg_apply_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_egg_window();
}

/*
 *  Egg window's Cancel button.
 */
C_EXPORT void on_egg_cancel_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWindow*   parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));

	// Check if there are unsaved changes
	if (studio->is_egg_window_dirty()) {
		if (!studio->prompt_for_discard(
					studio->egg_window_dirty, "Egg", parent)) {
			return;    // User chose not to discard
		}
	}

	studio->close_egg_window();
}

/*
 *  Egg window's close button.
 */
C_EXPORT gboolean on_egg_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	// Check if there are unsaved changes
	if (studio->is_egg_window_dirty()) {
		if (!studio->prompt_for_discard(
					studio->egg_window_dirty, "Egg", GTK_WINDOW(widget))) {
			return true;    // Block the close event
		}
	}

	studio->close_egg_window();
	return true;
}

/*
 *  Egg 'usecode' panel's 'browse' button.
 */
C_EXPORT void on_egg_browse_usecode_clicked(
		GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	const char*  uc     = studio->browse_usecode(true);
	if (*uc) {
		studio->set_entry("usecode_number", uc, true);
	}
}

/*
 *  "Teleport coords" toggled.
 */
C_EXPORT void on_teleport_coord_toggled(
		GtkToggleButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	const bool   on     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	studio->set_sensitive("teleport_x", on);
	studio->set_sensitive("teleport_y", on);
	studio->set_sensitive("teleport_z", on);
	studio->set_sensitive("teleport_eggnum", !on);

	// Update locate button based on current mode and values.
	if (on) {
		// Coordinate mode - check if coordinates are valid.
		const int tx = studio->get_num_entry("teleport_x");
		const int ty = studio->get_num_entry("teleport_y");
		Update_teleport_locate_button(studio, tx, ty);
	} else {
		// Egg number mode - check if egg number is valid.
		Update_teleport_locate_button(studio, 0, 0);    // Will check egg_num
														// internally.
	}
}

/*
 *  Open the egg-editing window.
 */

void ExultStudio::open_egg_window(
		unsigned char* data,    // Serialized egg, or null.
		int            datalen) {
	bool first_time = false;
	if (!eggwin) {    // First time?
		first_time = true;
		eggwin     = get_widget("egg_window");
		if (vgafile && palbuf) {
			egg_monster_single = new Shape_single(
					get_widget("monst_shape"), nullptr,
					[](int shnum) -> bool {
						return (shnum >= c_first_obj_shape)
							   && (shnum < c_max_shapes);
					},
					get_widget("monst_frame"), U7_SHAPE_SHAPES,
					vgafile->get_ifile(), palbuf.get(),
					get_widget("egg_monster_draw"));
			egg_missile_single = new Shape_single(
					get_widget("missile_shape"), nullptr,
					[](int shnum) -> bool {
						return (shnum >= 0) && (shnum < c_max_shapes);
					},
					nullptr, U7_SHAPE_SHAPES, vgafile->get_ifile(),
					palbuf.get(), get_widget("missile_draw"), true);
		}
		egg_ctx = gtk_statusbar_get_context_id(
				GTK_STATUSBAR(get_widget("egg_status")), "Egg Editor");
	}
	// Init. egg address to null.
	g_object_set_data(G_OBJECT(eggwin), "user_data", nullptr);
	// Make 'apply' and 'cancel' sensitive.
	gtk_widget_set_sensitive(get_widget("egg_apply_btn"), true);
	gtk_widget_set_sensitive(get_widget("egg_cancel_btn"), true);
	remove_statusbar("egg_status", egg_ctx, egg_status_id);
	if (data) {
		if (!init_egg_window(data, datalen)) {
			return;
		}
		gtk_widget_set_visible(get_widget("egg_okay_btn"), true);
		gtk_widget_set_visible(get_widget("egg_status"), false);
	} else {    // Init. empty dialog.
		if (first_time) {
			set_toggle("teleport_coord", true);
			set_sensitive("teleport_x", true);
			set_sensitive("teleport_y", true);
			set_sensitive("teleport_z", true);
			set_sensitive("teleport_eggnum", false);
		}
		// Disable locate and show egg buttons in creation mode.
		set_sensitive("teleport_locate", false);
		set_sensitive("teleport_show_egg", false);
		set_sensitive("intermap_locate", false);
		set_sensitive("intermap_show_egg", false);
		// Reset audio track values to 0.
		set_spin("juke_song", 0);
		set_toggle("juke_cont", false);
		set_spin("sfx_number", 0);
		set_toggle("sfx_cont", false);
		set_spin("speech_number", 0);
		gtk_widget_set_visible(get_widget("egg_okay_btn"), false);
		gtk_widget_set_visible(get_widget("egg_status"), false);
	}
	gtk_widget_set_visible(eggwin, true);
}

/*
 *  Close the egg-editing window.
 */

void ExultStudio::close_egg_window() {
	if (eggwin) {
		gtk_widget_set_visible(eggwin, false);
	}
}

/*
 *  Init. the egg editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_egg_window(unsigned char* data, int datalen) {
	// Check if window is already visible and dirty (opening another egg while
	// editor is dirty)
	if (eggwin && gtk_widget_get_visible(eggwin) && egg_window_dirty) {
		if (!prompt_for_discard(egg_window_dirty, "Egg", GTK_WINDOW(eggwin))) {
			return 0;    // User chose not to discard changes
		}
	}

	Egg_object* addr;
	int         tx;
	int         ty;
	int         tz;
	int         shape;
	int         frame;
	int         type;
	int         criteria;
	int         probability;
	int         distance;
	bool        nocturnal;
	bool        once;
	bool        hatched;
	bool        auto_reset;
	int         data1;
	int         data2;
	int         data3;
	string      str1;
	if (!Egg_object_in(
				data, datalen, addr, tx, ty, tz, shape, frame, type, criteria,
				probability, distance, nocturnal, once, hatched, auto_reset,
				data1, data2, data3, str1)) {
		cout << "Error decoding egg" << endl;
		return 0;
	}
	// Set initializing flag before loading data
	egg_window_initializing = true;

	// Store address with window.
	g_object_set_data(G_OBJECT(eggwin), "user_data", addr);
	// Store egg's map position.
	g_object_set_data(G_OBJECT(eggwin), "egg_tx", int_to_gpointer(tx));
	g_object_set_data(G_OBJECT(eggwin), "egg_ty", int_to_gpointer(ty));
	g_object_set_data(G_OBJECT(eggwin), "egg_tz", int_to_gpointer(tz));
	// Store egg's map number - request it from server.
	g_object_set_data(G_OBJECT(eggwin), "egg_mapnum", int_to_gpointer(-1));
	// Request current map number from server.
	if (Send_data(get_server_socket(), Exult_server::game_pos) != -1) {
		set_msg_callback(
				[](Exult_server::Msg_type id, const unsigned char* data,
				   int datalen, void* client) {
					ignore_unused_variable_warning(datalen, client);
					if (id != Exult_server::game_pos) {
						return;
					}
					ExultStudio* studio = ExultStudio::get_instance();
					GtkWidget*   eggwin = studio->get_widget("egg_window");
					// Skip tx, ty, tz (3 * 2 bytes).
					data += 6;
					const int mapnum = little_endian::Read2(data);
					// Store egg's map number.
					g_object_set_data(
							G_OBJECT(eggwin), "egg_mapnum",
							int_to_gpointer(mapnum));
					cout << "Egg is on map " << mapnum << endl;
					// Enable "Show egg" button for intermap widget.
					studio->set_sensitive("intermap_show_egg", true);
				},
				nullptr);
	}
	cout << "Editing egg at position (" << tx << ", " << ty << ", " << tz << ")"
		 << endl;
	// Enable the "Show egg" button since we have a valid egg position.
	set_sensitive("teleport_show_egg", true);
	GtkWidget* notebook = get_widget("notebook1");
	if (notebook) {    // 1st is monster (1).
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), type - 1);
	}
	set_spin("probability", probability);
	set_spin("distance", distance);
	set_optmenu("criteria", criteria);
	set_toggle("nocturnal", nocturnal);
	set_toggle("once", once);
	set_toggle("hatched", hatched);
	set_toggle("autoreset", auto_reset);
	switch (type) {         // Set notebook page.
	case 1: {               // Monster:
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
	case 2:    // Jukebox:
		set_spin("juke_song", data1 & 0xff);
		set_toggle("juke_cont", (data1 >> 8) & 0x01);
		break;
	case 3:    // Sound effect:
		set_spin("sfx_number", data1 & 0xff);
		set_toggle("sfx_cont", (data1 >> 8) & 0x01);
		break;
	case 4:    // Voice:
		set_spin("speech_number", data1 & 0xff);
		break;
	case 5:    // Usecode:
		if (!str1.empty()) {
			set_entry("usecode_number", str1.c_str(), true);
		} else {
			set_entry("usecode_number", data2, true);
		}
		set_spin("usecode_quality", data1 & 0xff);
		break;
	case 6:    // Missile:
		set_entry("missile_shape", data1);
		set_optmenu("missile_dir", data2 & 0xff);
		set_spin("missile_delay", data2 >> 8);
		break;
	case 7: {    // Teleport:
		const int qual = data1 & 0xff;
		if (qual == 255) {
			set_toggle("teleport_coord", true);
			const int schunk = data1 >> 8;
			const int teleport_x
					= (schunk % 12) * c_tiles_per_schunk + (data2 & 0xff);
			const int teleport_y
					= (schunk / 12) * c_tiles_per_schunk + (data2 >> 8);
			const int teleport_z = data3 & 0xff;
			set_entry("teleport_x", teleport_x, true);
			set_entry("teleport_y", teleport_y, true);
			set_entry("teleport_z", teleport_z);
			set_spin("teleport_eggnum", 0, false);
			// Update locate button state.
			Update_teleport_locate_button(this, teleport_x, teleport_y);
		} else {    // Egg #.
			set_toggle("teleport_coord", false);
			set_entry("teleport_x", 0, false, false);
			set_entry("teleport_y", 0, false, false);
			set_entry("teleport_z", 0, false, false);
			set_spin("teleport_eggnum", qual);
			// Disable locate button.
			Update_teleport_locate_button(this, 0, 0);
		}
		break;
	}
	case 8:    // Weather:
		set_optmenu("weather_type", data1 & 0xff);
		set_spin("weather_length", data1 >> 8);
		break;
	case 9:    // Path:
		set_spin("pathegg_num", data1 & 0xff);
		break;
	case 10:    // Button:
		set_spin("btnegg_distance", data1 & 0xff);
		break;
	case 11: {    // Intermap:
		const int mapnum = data1 & 0xff;
		const int schunk = data1 >> 8;
		set_spin("intermap_mapnum", mapnum);
		set_entry(
				"intermap_x",
				(schunk % 12) * c_tiles_per_schunk + (data2 & 0xff), true);
		set_entry(
				"intermap_y", (schunk / 12) * c_tiles_per_schunk + (data2 >> 8),
				true);
		set_entry("intermap_z", data3 & 0xff);
		break;
	}
	default:
		break;
	}

	// Connect dirty tracking signals on first time
	static bool signals_connected = false;
	if (!signals_connected) {
		connect_widget_signals(eggwin, G_CALLBACK(on_egg_changed), nullptr);
		signals_connected = true;
	}

	// Clear dirty and initializing flags after loading data
	egg_window_dirty        = false;
	egg_window_initializing = false;

	return 1;
}

/*
 *  Callback for when user clicked where egg should be inserted.
 */

static void Egg_response(
		Exult_server::Msg_type id, const unsigned char* data, int datalen,
		void* /* client */
) {
	ignore_unused_variable_warning(data, datalen);
	if (id == Exult_server::user_responded) {
		ExultStudio::get_instance()->close_egg_window();
	}
	//+++++cancel??
}

/*
 *  Send updated egg info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_egg_window() {
	cout << "In save_egg_window()" << endl;
	// Get egg (null if creating new).
	const int  tx       = -1;
	const int  ty       = -1;
	const int  tz       = -1;    // +++++For now.
	const int  shape    = -1;
	const int  frame    = -1;    // For now.
	int        type     = -1;
	GtkWidget* notebook = get_widget("notebook1");
	if (notebook) {    // 1st is monster (1).
		type = 1 + gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
	} else {
		cout << "Can't find notebook widget" << endl;
		return 0;
	}
	const int   criteria    = get_optmenu("criteria");
	const int   probability = get_spin("probability");
	const int   distance    = get_spin("distance");
	const bool  nocturnal   = get_toggle("nocturnal");
	const bool  once        = get_toggle("once");
	const bool  hatched     = get_toggle("hatched");
	const bool  auto_reset  = get_toggle("autoreset");
	int         data1       = -1;
	int         data2       = -1;
	int         data3       = 0;
	const char* str1        = "";
	switch (type) {    // Set notebook page.
	case 1: {          // Monster:
		const int shnum = get_num_entry("monst_shape");
		const int frnum = get_num_entry("monst_frame");
		if (shnum >= 1024 or frnum >= 64) {
			data3 = shnum;
			data2 = frnum & 0xff;
		} else {
			data2 = (shnum & 1023) + (frnum << 10);
		}
		data1 = (get_optmenu("monst_schedule") << 8)
				+ (get_optmenu("monst_align") & 3)
				+ (get_spin("monst_count") << 2);
	} break;
	case 2:    // Jukebox:
		data1 = (get_spin("juke_song") & 0xff)
				+ (get_toggle("juke_cont") ? (1 << 8) : 0);
		break;
	case 3:    // Sound effect:
		data1 = (get_spin("sfx_number") & 0xff)
				+ (get_toggle("sfx_cont") ? (1 << 8) : 0);
		break;
	case 4:    // Voice:
		data1 = get_spin("speech_number") & 0xff;
		break;
	case 5:    // Usecode:
		data2 = get_num_entry("usecode_number");
		if (!data2) {
			str1 = get_text_entry("usecode_number");
		}
		data1 = get_spin("usecode_quality") & 0xff;
		break;
	case 6:    // Missile:
		data1 = get_num_entry("missile_shape");
		data2 = (get_optmenu("missile_dir") & 0xff)
				+ (get_spin("missile_delay") << 8);
		break;
	case 7:    // Teleport:
		if (get_toggle("teleport_coord")) {
			// Abs. coords.
			const int tx = get_num_entry("teleport_x");
			const int ty = get_num_entry("teleport_y");
			const int tz = get_num_entry("teleport_z");
			data3        = tz;
			data2        = (tx & 0xff) + ((ty & 0xff) << 8);
			const int sx = tx / c_tiles_per_schunk;
			const int sy = ty / c_tiles_per_schunk;
			data1        = 255 + ((sy * 12 + sx) << 8);
		} else {    // Egg #.
			data1 = get_spin("teleport_eggnum") & 0xff;
		}
		break;
	case 8:    // Weather:
		data1 = (get_optmenu("weather_type") & 0xff)
				+ (get_spin("weather_length") << 8);
		break;
	case 9:    // Path:
		data1 = get_spin("pathegg_num") & 0xff;
		break;
	case 10:    // Button:
		data1 = get_spin("btnegg_distance") & 0xff;
		break;
	case 11: {    // Intermap.
		const int tx     = get_num_entry("intermap_x");
		const int ty     = get_num_entry("intermap_y");
		const int tz     = get_num_entry("intermap_z");
		const int mapnum = get_spin("intermap_mapnum");
		data3            = tz;
		data2            = (tx & 0xff) + ((ty & 0xff) << 8);
		const int sx     = tx / c_tiles_per_schunk;
		const int sy     = ty / c_tiles_per_schunk;
		data1            = mapnum + ((sy * 12 + sx) << 8);
		break;
	}
	default:
		cout << "Unknown egg type" << endl;
		return 0;
	}
	auto* addr = static_cast<Egg_object*>(
			g_object_get_data(G_OBJECT(eggwin), "user_data"));
	if (Egg_object_out(
				server_socket, addr, tx, ty, tz, shape, frame, type, criteria,
				probability, distance, nocturnal, once, hatched, auto_reset,
				data1, data2, data3, str1)
		== -1) {
		cout << "Error sending egg data to server" << endl;
		return 0;
	}
	cout << "Sent egg data to server" << endl;
	if (!addr) {
		egg_status_id = set_statusbar(
				"egg_status", egg_ctx, "Click on map at place to insert egg");
		gtk_widget_set_visible(get_widget("egg_status"), true);
		// Make 'apply' and 'cancel' insensitive.
		gtk_widget_set_sensitive(get_widget("egg_apply_btn"), false);
		gtk_widget_set_sensitive(get_widget("egg_cancel_btn"), false);
		waiting_for_server = Egg_response;
		// Clear dirty flag after successful save
		egg_window_dirty = false;
		return 1;    // Leave window open.
	}
	// Clear dirty flag after successful save
	egg_window_dirty = false;
	return 1;
}

/*
 *  Helper function to play audio from egg editor.
 */
static void play_egg_audio(
		int type, const char* spin_name, int volume = 100,
		bool repeat = false) {
	ExultStudio* studio = ExultStudio::get_instance();
	const int    track  = studio->get_spin(spin_name);

	if (track >= 0) {
		studio->play_audio(type, track, volume, repeat);
	}
}

/*
 *  Helper function to stop audio playback.
 */
static void stop_egg_audio() {
	ExultStudio::get_instance()->stop_audio();
}

/*
 *  Jukebox play button clicked.
 */
C_EXPORT void on_juke_song_play_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	const bool   repeat = studio->get_toggle("juke_cont");
	// type = 0 for music
	play_egg_audio(0, "juke_song", 100, repeat);
}

/*
 *  Jukebox stop button clicked.
 */
C_EXPORT void on_juke_song_stop_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	stop_egg_audio();
}

/*
 *  Egg SFX play button clicked.
 */
C_EXPORT void on_egg_sfx_play_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	const bool   repeat = studio->get_toggle("sfx_cont");
	// type = 1 for SFX
	play_egg_audio(1, "sfx_number", 100, repeat);
}

/*
 *  Egg SFX stop button clicked.
 */
C_EXPORT void on_egg_sfx_stop_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	stop_egg_audio();
}

/*
 *  Speech play button clicked.
 */
C_EXPORT void on_speech_play_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	// type = 2 for speech
	play_egg_audio(2, "speech_number");
}

/*
 *  Speech stop button clicked.
 */
C_EXPORT void on_speech_stop_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	stop_egg_audio();
}

/*
 *  Received game position for teleport.
 */
static void Teleport_loc_response(
		Exult_server::Msg_type id, const unsigned char* data, int datalen,
		void* client) {
	ignore_unused_variable_warning(datalen, client);
	if (id != Exult_server::game_pos) {
		return;
	}
	ExultStudio* studio = ExultStudio::get_instance();
	const int    tx     = little_endian::Read2(data);
	const int    ty     = little_endian::Read2(data);
	const int    tz     = little_endian::Read2(data);

	studio->set_entry("teleport_x", tx, false, false);
	studio->set_entry("teleport_y", ty, false, false);
	studio->set_entry("teleport_z", tz, false, false);

	// Enable locate button since we now have coordinates.
	Update_teleport_locate_button(studio, tx, ty);
}

/*
 *  "Set current position" button - set teleport location from current game
 * position.
 */
C_EXPORT void on_teleport_loc_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (Send_data(studio->get_server_socket(), Exult_server::game_pos) == -1) {
		cout << "Error sending message to server" << endl;
	} else {
		studio->set_msg_callback(Teleport_loc_response, nullptr);
	}
}

/*
 *  Callback for receiving clicked map coordinates for teleport location.
 */
static void Teleport_click_loc_response(
		Exult_server::Msg_type id, const unsigned char* data, int datalen,
		void* client) {
	ignore_unused_variable_warning(datalen, client);
	ExultStudio*  studio    = ExultStudio::get_instance();
	GtkStatusbar* statusbar = GTK_STATUSBAR(studio->get_widget("egg_status"));
	const int     ctx = gtk_statusbar_get_context_id(statusbar, "Egg Editor");

	if (id == Exult_server::cancel) {
		// User cancelled the click operation.
		// Clear and hide statusbar, then re-enable buttons.
		gtk_statusbar_remove_all(statusbar, ctx);
		gtk_widget_set_visible(studio->get_widget("egg_status"), false);
		studio->set_sensitive("egg_okay_btn", true);
		studio->set_sensitive("egg_apply_btn", true);
		studio->set_sensitive("egg_cancel_btn", true);
		return;
	}
	if (id != Exult_server::get_user_click) {
		return;
	}

	const int tx = little_endian::Read2(data);
	const int ty = little_endian::Read2(data);
	const int tz = little_endian::Read2(data);

	studio->set_entry("teleport_x", tx, false, false);
	studio->set_entry("teleport_y", ty, false, false);
	studio->set_entry("teleport_z", tz, false, false);

	// Enable locate button since we now have coordinates.
	Update_teleport_locate_button(studio, tx, ty);

	// Clear and hide statusbar, then re-enable buttons.
	gtk_statusbar_remove_all(statusbar, ctx);
	gtk_widget_set_visible(studio->get_widget("egg_status"), false);
	studio->set_sensitive("egg_okay_btn", true);
	studio->set_sensitive("egg_apply_btn", true);
	studio->set_sensitive("egg_cancel_btn", true);
}

/*
 *  "Set in game" button - wait for user to click in Exult and capture
 * coordinates.
 */
C_EXPORT void on_teleport_loc_click(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	// Send request for user click.
	if (Send_data(studio->get_server_socket(), Exult_server::get_user_click)
		== -1) {
		cout << "Error sending message to server" << endl;
	} else {
		// Show status message and disable buttons while waiting.
		GtkStatusbar* statusbar
				= GTK_STATUSBAR(studio->get_widget("egg_status"));
		const int ctx = gtk_statusbar_get_context_id(statusbar, "Egg Editor");
		studio->set_statusbar(
				"egg_status", ctx, "Click on map to set teleport position");
		gtk_widget_set_visible(studio->get_widget("egg_status"), true);
		studio->set_sensitive("egg_okay_btn", false);
		studio->set_sensitive("egg_apply_btn", false);
		studio->set_sensitive("egg_cancel_btn", false);
		// Use waiting_for_server mechanism like NPC creation does.
		studio->set_msg_callback(Teleport_click_loc_response, nullptr);
	}
}

/*
 *  "Show egg" button - center Exult view on the egg's original position.
 */
C_EXPORT void on_teleport_show_egg(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWidget*   eggwin = studio->get_widget("egg_window");

	// Retrieve the egg's stored position.
	const int tx
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_tx"));
	const int ty
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_ty"));
	const int tz
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_tz"));

	// Send view_pos command with tz to always trigger centering.
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	if (ptr + 12 > data + Exult_server::maxlength) {
		return;    // Safety check, should not happen.
	}
	little_endian::Write4(ptr, tx);
	little_endian::Write4(ptr, ty);
	little_endian::Write4(ptr, tz);
	studio->send_to_server(Exult_server::view_pos, data, ptr - data);
}

/*
 *  "Locate" button - center Exult view on the teleport location coordinates or
 * find the path egg.
 */
C_EXPORT void on_teleport_loc_locate(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	const bool using_coords = studio->get_toggle("teleport_coord");

	if (using_coords) {
		// Read coordinates from entry fields and center view on them.
		const int tx = studio->get_num_entry("teleport_x");
		const int ty = studio->get_num_entry("teleport_y");

		// Send view_pos command to center on these coordinates.
		unsigned char  data[Exult_server::maxlength];
		unsigned char* ptr = &data[0];
		// Ensure buffer has space for two 4-byte values (8 bytes total).
		if (ptr + 8 > data + Exult_server::maxlength) {
			return;    // Safety check, should not happen.
		}
		little_endian::Write4(ptr, tx);
		little_endian::Write4(ptr, ty);
		studio->send_to_server(Exult_server::view_pos, data, ptr - data);
	} else {
		// Using egg number - send locate_egg command with the egg path number.
		const int egg_num = studio->get_spin("teleport_eggnum");
		// Validate egg number is in valid range (0-255).
		if (egg_num < 0 || egg_num > 255) {
			return;    // Invalid egg number.
		}
		// Get the egg's origin position for distance constraint.
		GtkWidget* eggwin   = studio->get_widget("egg_window");
		const int  origin_x = gpointer_to_int(
                g_object_get_data(G_OBJECT(eggwin), "egg_tx"));
		const int origin_y = gpointer_to_int(
				g_object_get_data(G_OBJECT(eggwin), "egg_ty"));

		unsigned char  data[Exult_server::maxlength];
		unsigned char* ptr = &data[0];
		// Ensure buffer has space for 2-byte egg_num + two 4-byte coordinates.
		if (ptr + 10 > data + Exult_server::maxlength) {
			return;    // Safety check, should not happen.
		}
		little_endian::Write2(ptr, egg_num);
		little_endian::Write4(ptr, origin_x);
		little_endian::Write4(ptr, origin_y);
		studio->send_to_server(Exult_server::locate_egg, data, ptr - data);
	}
}

/*
 *  "Set current position" button for intermap - set intermap location from
 * current game position.
 */
C_EXPORT void on_intermap_set_pos(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (Send_data(studio->get_server_socket(), Exult_server::game_pos) == -1) {
		cout << "Error sending message to server" << endl;
	} else {
		studio->set_msg_callback(
				[](Exult_server::Msg_type id, const unsigned char* data,
				   int datalen, void* client) {
					ignore_unused_variable_warning(datalen, client);
					if (id != Exult_server::game_pos) {
						return;
					}
					ExultStudio* studio = ExultStudio::get_instance();
					const int    tx     = little_endian::Read2(data);
					const int    ty     = little_endian::Read2(data);
					const int    tz     = little_endian::Read2(data);
					const int    mapnum = little_endian::Read2(data);

					studio->set_entry("intermap_x", tx, false, false);
					studio->set_entry("intermap_y", ty, false, false);
					studio->set_entry("intermap_z", tz, false, false);
					studio->set_spin("intermap_mapnum", mapnum);
				},
				nullptr);
	}
}

/*
 *  "Set in game" button for intermap - wait for user to click in Exult and
 * capture coordinates.
 */
C_EXPORT void on_intermap_set_ingame(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (Send_data(studio->get_server_socket(), Exult_server::get_user_click)
		== -1) {
		cout << "Error sending message to server" << endl;
	} else {
		GtkStatusbar* statusbar
				= GTK_STATUSBAR(studio->get_widget("egg_status"));
		const int ctx = gtk_statusbar_get_context_id(statusbar, "Egg Editor");
		studio->set_statusbar(
				"egg_status", ctx, "Click on map to set intermap position");
		gtk_widget_set_visible(studio->get_widget("egg_status"), true);
		studio->set_sensitive("egg_okay_btn", false);
		studio->set_sensitive("egg_apply_btn", false);
		studio->set_sensitive("egg_cancel_btn", false);
		studio->set_msg_callback(
				[](Exult_server::Msg_type id, const unsigned char* data,
				   int datalen, void* client) {
					ignore_unused_variable_warning(datalen, client);
					ExultStudio*  studio = ExultStudio::get_instance();
					GtkStatusbar* statusbar
							= GTK_STATUSBAR(studio->get_widget("egg_status"));
					const int ctx = gtk_statusbar_get_context_id(
							statusbar, "Egg Editor");

					if (id == Exult_server::cancel) {
						gtk_statusbar_remove_all(statusbar, ctx);
						gtk_widget_set_visible(
								studio->get_widget("egg_status"), false);
						studio->set_sensitive("egg_okay_btn", true);
						studio->set_sensitive("egg_apply_btn", true);
						studio->set_sensitive("egg_cancel_btn", true);
						return;
					}
					if (id != Exult_server::get_user_click) {
						return;
					}

					const int tx     = little_endian::Read2(data);
					const int ty     = little_endian::Read2(data);
					const int tz     = little_endian::Read2(data);
					const int mapnum = little_endian::Read2(data);

					studio->set_entry("intermap_x", tx, false, false);
					studio->set_entry("intermap_y", ty, false, false);
					studio->set_entry("intermap_z", tz, false, false);
					studio->set_spin("intermap_mapnum", mapnum);

					gtk_statusbar_remove_all(statusbar, ctx);
					gtk_widget_set_visible(
							studio->get_widget("egg_status"), false);
					studio->set_sensitive("egg_okay_btn", true);
					studio->set_sensitive("egg_apply_btn", true);
					studio->set_sensitive("egg_cancel_btn", true);
				},
				nullptr);
	}
}

/*
 *  "Locate" button for intermap - center view on intermap destination,
 * switching maps if needed.
 */
C_EXPORT void on_intermap_locate(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	// Read coordinates and map number from entry fields.
	const int tx     = studio->get_num_entry("intermap_x");
	const int ty     = studio->get_num_entry("intermap_y");
	const int tz     = studio->get_num_entry("intermap_z");
	const int mapnum = studio->get_spin("intermap_mapnum");

	// Don't overwrite egg_mapnum here - it should remain the egg's original
	// map, not the destination map.

	// Send locate_intermap command with coordinates and map number.
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	if (ptr + 14 > data + Exult_server::maxlength) {
		return;    // Safety check.
	}
	little_endian::Write4(ptr, tx);
	little_endian::Write4(ptr, ty);
	little_endian::Write4(ptr, tz);
	little_endian::Write2(ptr, mapnum);
	studio->send_to_server(Exult_server::locate_intermap, data, ptr - data);
}

/*
 *  "Show egg" button for intermap - center view on egg's original position,
 * switching maps if needed.
 */
C_EXPORT void on_intermap_show_egg(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWidget*   eggwin = studio->get_widget("egg_window");

	// Retrieve the egg's stored position and map number.
	const int tx
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_tx"));
	const int ty
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_ty"));
	const int tz
			= gpointer_to_int(g_object_get_data(G_OBJECT(eggwin), "egg_tz"));
	const int mapnum = gpointer_to_int(
			g_object_get_data(G_OBJECT(eggwin), "egg_mapnum"));

	// Send locate_intermap command with egg's coordinates and map number.
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	if (ptr + 14 > data + Exult_server::maxlength) {
		return;    // Safety check.
	}
	little_endian::Write4(ptr, tx);
	little_endian::Write4(ptr, ty);
	little_endian::Write4(ptr, tz);
	little_endian::Write2(ptr, mapnum);
	studio->send_to_server(Exult_server::locate_intermap, data, ptr - data);
}
