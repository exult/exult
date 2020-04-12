/**
 ** Bargeedit.cc - Barge-editing methods.
 **
 ** Written: 10/16/04 - JSF
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
#include "servemsg.h"
#include "objserial.h"
#include "exult_constants.h"
#include "shapefile.h"
#include "ignore_unused_variable_warning.h"

using std::cout;
using std::endl;

class Barge_object;

/*
 *  Open barge window.
 */

C_EXPORT void on_open_barge_activate(
    GtkMenuItem     *menuitem,
    gpointer         user_data
) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->open_barge_window();
}

/*
 *  Barge window's Apply button.
 */
C_EXPORT void on_barge_apply_btn_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_barge_window();
}

/*
 *  Barge window's Cancel button.
 */
C_EXPORT void on_barge_cancel_btn_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_barge_window();
}

/*
 *  Barge window's close button.
 */
C_EXPORT gboolean on_barge_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_barge_window();
	return TRUE;
}

/*
 *  Open the barge-editing window.
 */

void ExultStudio::open_barge_window(
    unsigned char *data,        // Serialized barge, or null.
    int datalen
) {
	bool first_time = false;
	if (!bargewin) {        // First time?
		first_time = true;
		bargewin = glade_xml_get_widget(app_xml, "barge_window");
		barge_ctx = gtk_statusbar_get_context_id(
		                GTK_STATUSBAR(glade_xml_get_widget(
		                                  app_xml, "barge_status")), "Barge Editor");
	}
	// Init. barge address to null.
	gtk_object_set_user_data(GTK_OBJECT(bargewin), nullptr);
	// Make 'apply' sensitive.
	gtk_widget_set_sensitive(glade_xml_get_widget(app_xml,
	                         "barge_apply_btn"), true);
	remove_statusbar("barge_status", barge_ctx, barge_status_id);
	if (data) {
		if (!init_barge_window(data, datalen))
			return;
	} else if (first_time) {    // Init. empty dialog first time.
	}
	gtk_widget_show(bargewin);
}

/*
 *  Close the barge-editing window.
 */

void ExultStudio::close_barge_window(
) {
	if (bargewin) {
		gtk_widget_hide(bargewin);
	}
}

/*
 *  Init. the barge editor with data from Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::init_barge_window(
    unsigned char *data,
    int datalen
) {
	Barge_object *addr;
	int tx;
	int ty;
	int tz;
	int shape;
	int frame;
	int xtiles;
	int ytiles;
	int dir;
	if (!Barge_object_in(data, datalen, addr, tx, ty, tz, shape, frame,
	                     xtiles, ytiles, dir)) {
		cout << "Error decoding barge" << endl;
		return 0;
	}
	// Store address with window.
	gtk_object_set_user_data(GTK_OBJECT(bargewin), addr);
	set_spin("barge_xtiles", xtiles);
	set_spin("barge_ytiles", ytiles);
	set_optmenu("barge_dir", dir);
	return 1;
}

/*
 *  Callback for when user clicked where barge should be inserted.
 */

static void Barge_response(
    Exult_server::Msg_type id,
    const unsigned char *data,
    int datalen,
    void * /* client */
) {
	ignore_unused_variable_warning(data, datalen);
	if (id == Exult_server::user_responded)
		ExultStudio::get_instance()->close_barge_window();
	//+++++cancel??
}

/*
 *  Send updated barge info. back to Exult.
 *
 *  Output: 0 if error (reported).
 */

int ExultStudio::save_barge_window(
) {
	cout << "In save_barge_window()" << endl;
	// Get barge (null if creating new).
	Barge_object *addr = static_cast<Barge_object*>(gtk_object_get_user_data(GTK_OBJECT(bargewin)));
	int tx = -1;
	int ty = -1;
	int tz = -1;  // +++++For now.
	int shape = -1;
	int frame = -1; // For now.
	int dir = get_optmenu("barge_dir");
	int xtiles = get_spin("barge_xtiles");
	int ytiles = get_spin("barge_ytiles");
	if (Barge_object_out(server_socket, addr, tx, ty, tz,
	                     shape, frame, xtiles, ytiles, dir) == -1) {
		cout << "Error sending barge data to server" << endl;
		return 0;
	}
	cout << "Sent barge data to server" << endl;
	if (!addr) {
		barge_status_id = set_statusbar("barge_status", barge_ctx,
		                                "Click on map to set lower-right corner of  barge");
		// Make 'apply' insensitive.
		gtk_widget_set_sensitive(glade_xml_get_widget(app_xml,
		                         "barge_apply_btn"), false);
		waiting_for_server = Barge_response;
		return 1;       // Leave window open.
	}
	close_barge_window();
	return 1;
}


