/**
 ** Bargeedit.cc - Barge-editing methods.
 **
 ** Written: 10/16/04 - JSF
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
#include "shapefile.h"
#include "studio.h"

using std::cout;
using std::endl;

class Barge_object;

/*
 *  Callback to mark barge window as dirty on any widget change.
 */
static void on_barge_changed(GtkWidget* widget, gpointer user_data) {
	ignore_unused_variable_warning(widget, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (!studio->is_barge_window_initializing()) {
		studio->set_barge_window_dirty(true);
	}
}

/*
 *  Open barge window.
 */

C_EXPORT void on_open_barge_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->open_barge_window();
}

/*
 *  Barge window's OK button.
 */
C_EXPORT void on_barge_okay_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_barge_window();
	ExultStudio::get_instance()->close_barge_window();
}

/*
 *  Barge window's Apply button.
 */
C_EXPORT void on_barge_apply_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_barge_window();
}

/*
 *  Barge window's Cancel button.
 */
C_EXPORT void on_barge_cancel_btn_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWindow*   parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
	if (studio->is_barge_window_dirty()
		&& !studio->prompt_for_discard(
				studio->barge_window_dirty, "Barge", parent)) {
		return;    // User chose not to discard
	}
	studio->close_barge_window();
}

/*
 *  Barge window's close button.
 */
C_EXPORT gboolean on_barge_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	if (studio->is_barge_window_dirty()
		&& !studio->prompt_for_discard(
				studio->barge_window_dirty, "Barge", GTK_WINDOW(widget))) {
		return true;    // Block window close
	}
	studio->close_barge_window();
	return true;
}

/*
 *  Open the barge-editing window.
 */

void ExultStudio::open_barge_window(
		unsigned char* data,    // Serialized barge, or null.
		int            datalen) {
	bool first_time = false;
	if (!bargewin) {    // First time?
		first_time = true;
		bargewin   = get_widget("barge_window");
		barge_ctx  = gtk_statusbar_get_context_id(
                GTK_STATUSBAR(get_widget("barge_status")), "Barge Editor");
	}
	// Init. barge address to null.
	g_object_set_data(G_OBJECT(bargewin), "user_data", nullptr);
	// Connect signals (only once)
	static bool signals_connected = false;
	if (!signals_connected) {
		connect_widget_signals(bargewin, G_CALLBACK(on_barge_changed), nullptr);
		signals_connected = true;
	}
	// Make 'apply' and 'cancel' sensitive.
	gtk_widget_set_sensitive(get_widget("barge_apply_btn"), true);
	gtk_widget_set_sensitive(get_widget("barge_cancel_btn"), true);
	remove_statusbar("barge_status", barge_ctx, barge_status_id);
	if (data) {
		if (!init_barge_window(data, datalen)) {
			return;
		}
		gtk_widget_set_visible(get_widget("barge_okay_btn"), true);
		gtk_widget_set_visible(get_widget("barge_status"), false);
	} else {
		if (first_time) {    // Init. empty dialog first time.
		}
		gtk_widget_set_visible(get_widget("barge_okay_btn"), false);
		gtk_widget_set_visible(get_widget("barge_status"), false);
	}
	gtk_widget_set_visible(bargewin, true);
}

/*
 *  Close the barge-editing window.
 */

void ExultStudio::close_barge_window() {
	if (bargewin) {
		gtk_widget_set_visible(bargewin, false);
	}
}

/*
 *  Init. the barge editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_barge_window(unsigned char* data, int datalen) {
	// Check for unsaved changes before opening new barge
	if (bargewin && gtk_widget_get_visible(bargewin)) {
		if (!prompt_for_discard(
					barge_window_dirty, "Barge", GTK_WINDOW(bargewin))) {
			return 0;    // User canceled
		}
	}

	// Set initializing flag to prevent marking dirty during setup
	barge_window_initializing = true;
	// Clear dirty flag for new barge
	barge_window_dirty = false;

	Barge_object* addr;
	int           tx;
	int           ty;
	int           tz;
	int           shape;
	int           frame;
	int           xtiles;
	int           ytiles;
	int           dir;
	if (!Barge_object_in(
				data, datalen, addr, tx, ty, tz, shape, frame, xtiles, ytiles,
				dir)) {
		cout << "Error decoding barge" << endl;
		return 0;
	}
	// Store address with window.
	g_object_set_data(G_OBJECT(bargewin), "user_data", addr);
	set_spin("barge_xtiles", xtiles);
	set_spin("barge_ytiles", ytiles);
	set_optmenu("barge_dir", dir);

	// Initialization complete - allow dirty marking now
	barge_window_initializing = false;
	return 1;
}

/*
 *  Callback for when user clicked where barge should be inserted.
 */

static void Barge_response(
		Exult_server::Msg_type id, const unsigned char* data, int datalen,
		void* /* client */
) {
	ignore_unused_variable_warning(data, datalen);
	if (id == Exult_server::user_responded) {
		ExultStudio::get_instance()->close_barge_window();
	}
	//+++++cancel??
}

/*
 *  Send updated barge info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_barge_window() {
	cout << "In save_barge_window()" << endl;
	// Get barge (null if creating new).
	auto* addr = static_cast<Barge_object*>(
			g_object_get_data(G_OBJECT(bargewin), "user_data"));
	const int tx     = -1;
	const int ty     = -1;
	const int tz     = -1;    // +++++For now.
	const int shape  = -1;
	const int frame  = -1;    // For now.
	const int dir    = get_optmenu("barge_dir");
	const int xtiles = get_spin("barge_xtiles");
	const int ytiles = get_spin("barge_ytiles");
	if (Barge_object_out(
				server_socket, addr, tx, ty, tz, shape, frame, xtiles, ytiles,
				dir)
		== -1) {
		cout << "Error sending barge data to server" << endl;
		return 0;
	}
	cout << "Sent barge data to server" << endl;
	ExultStudio::get_instance()->set_barge_window_dirty(false);
	if (!addr) {
		barge_status_id = set_statusbar(
				"barge_status", barge_ctx,
				"Click on map to set lower-right corner of  barge");
		gtk_widget_set_visible(get_widget("barge_status"), true);
		// Make 'apply' and 'cancel' insensitive.
		gtk_widget_set_sensitive(get_widget("barge_apply_btn"), false);
		gtk_widget_set_sensitive(get_widget("barge_cancel_btn"), false);
		waiting_for_server = Barge_response;
		return 1;    // Leave window open.
	}
	return 1;
}
