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

#include <cctype>
#include "objbrowse.h"
#include "shapegroup.h"
#include "shapefile.h"
#include "exceptions.h"

using EStudio::Add_menu_item;
using EStudio::Create_arrow_button;

Object_browser::Object_browser(Shape_group *grp, Shape_file_info *fi)
	: group(grp), file_info(fi) {
	widget = nullptr;
}

Object_browser::~Object_browser() {
	if (popup)
		gtk_widget_destroy(popup);
}

void Object_browser::set_widget(GtkWidget *w) {
	widget = w;
}

/*
 *  Search a name for a string, ignoring case.
 */

bool Object_browser::search_name(
    const char *nm,
    const char *srch
) {
	auto safe_tolower = [](const char ch) {
		return static_cast<char>(tolower(static_cast<unsigned char>(ch)));
	};
	char first = safe_tolower(*srch);
	while (*nm) {
		if (safe_tolower(*nm) == first) {
			const char *np = nm + 1;
			const char *sp = srch + 1;
			while (*sp && safe_tolower(*np) == safe_tolower(*sp)) {
				sp++;
				np++;
			}
			if (!*sp)   // Matched to end of search string.
				return true;
		}
		nm++;
	}
	return false;
}


bool Object_browser::server_response(int id, unsigned char *data, int datalen) {
	ignore_unused_variable_warning(id, data, datalen);
	return false;           // Not handled here.
}

void Object_browser::end_terrain_editing() {
}

void Object_browser::set_background_color(guint32 c) {
	ignore_unused_variable_warning(c);
}

GtkWidget *Object_browser::get_widget() {
	return widget;
}

void Object_browser::on_browser_group_add(
    GtkMenuItem *item,
    gpointer udata
) {
	auto *chooser = static_cast<Object_browser *>(udata);
	auto *grp = static_cast<Shape_group *>(g_object_get_data(
	        G_OBJECT(item), "user_data"));
	int id = chooser->get_selected_id();
	if (id >= 0) {          // Selected shape?
		grp->add(id);       // Add & redisplay open windows.
		ExultStudio::get_instance()->update_group_windows(grp);
	}
}

/*
 *  Add an "Add to group..." submenu to a popup for our group.
 */

void Object_browser::add_group_submenu(
    GtkWidget *popup
) {
	// Use our group, or assume we're in
	//   the main window.
	Shape_group_file *groups = group ? group->get_file()
	                           : ExultStudio::get_instance()->get_cur_groups();
	int gcnt = groups ? groups->size() : 0;
	if (gcnt > 1 ||         // Groups besides ours?
	        (gcnt == 1 && !group)) {
		GtkWidget *mitem = Add_menu_item(popup,
		                                 "Add to group...");
		GtkWidget *group_menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem), group_menu);
		for (int i = 0; i < gcnt; i++) {
			Shape_group *grp = groups->get(i);
			if (grp == group)
				continue;// Skip ourself.
			GtkWidget *gitem = Add_menu_item(
			                       group_menu, grp->get_name(),
			                       G_CALLBACK(
			                           Object_browser::on_browser_group_add),
			                       this);
			// Store group on menu item.
			g_object_set_data(G_OBJECT(gitem), "user_data", grp);
		}
	}
}

/*
 *  Create a modal file selector.
 */

void Create_file_selection(
    const char *title,
    const char *path,
    const char *filtername,
    const std::vector<std::string> &filters,
    GtkFileChooserAction action,
    File_sel_okay_fun ok_handler,
    gpointer user_data
) {
	const char *stock_accept = (action == GTK_FILE_CHOOSER_ACTION_OPEN) ? "_Open" : "_Save";
	GtkFileChooser *fsel = GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(
	        title, nullptr, action,
	        "_Cancel", GTK_RESPONSE_CANCEL,
	        stock_accept, GTK_RESPONSE_ACCEPT,
	        nullptr));
	GtkWidget *btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(fsel), GTK_RESPONSE_CANCEL);
	GtkWidget *img = gtk_image_new_from_icon_name("gtk-cancel",
	                 GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(btn), img);
	btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(fsel), GTK_RESPONSE_ACCEPT);
	img = gtk_image_new_from_icon_name(
	          (action == GTK_FILE_CHOOSER_ACTION_OPEN) ? "document-open" : "document-save",
	          GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(btn), img);
	gtk_window_set_modal(GTK_WINDOW(fsel), true);
	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		gtk_file_chooser_set_do_overwrite_confirmation(fsel, TRUE);
	}
	if (path != nullptr && is_system_path_defined(path)) {
		// Default to a writable location.
		std::string startdir = get_system_path(path);
		gtk_file_chooser_set_current_folder(fsel, startdir.c_str());
	}
	if (!filters.empty()) {
		GtkFileFilter *gfilt = gtk_file_filter_new();
		for (const auto &filter : filters) {
			gtk_file_filter_add_pattern(gfilt, filter.c_str());
		}
		if (filtername != nullptr) {
			gtk_file_filter_set_name(gfilt, filtername);
		}
		gtk_file_chooser_add_filter(fsel, gfilt);
	}
	if (gtk_dialog_run(GTK_DIALOG(fsel)) == GTK_RESPONSE_ACCEPT) {
		char *filename = gtk_file_chooser_get_filename(fsel);
		ok_handler(filename, user_data);
		g_free(filename);
	}
	gtk_widget_destroy(GTK_WIDGET(fsel));
}

/*
 *  Save file in browser.
 */

void Object_browser::on_browser_file_save(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	auto *chooser = static_cast<Object_browser *>(udata);
	if (!chooser->file_info)
		return;         // Nothing to write to.
	try {
		chooser->file_info->flush();
	} catch (const exult_exception &e) {
		EStudio::Alert("%s", e.what());
	}
}

/*
 *  Revert file in browser to what's on disk.
 */

void Object_browser::on_browser_file_revert(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	auto *chooser = static_cast<Object_browser *>(udata);
	if (!chooser->file_info)
		return;         // No file?
	char *msg = g_strdup_printf("Okay to throw away any changes to '%s'?",
	                            chooser->file_info->get_basename());
	if (EStudio::Prompt(msg, "Yes", "No") != 0)
		return;
	if (!chooser->file_info->revert())
		EStudio::Alert("Revert not yet implemented for this file");
	else {
		chooser->load();    // Reload from file.
		chooser->render();  // Repaint.
	}
}

/*
 *  Set up popup menu for shape browser.
 */

GtkWidget *Object_browser::create_popup_internal(
    bool files          // Include 'files'.
) {
	if (popup)          // Clean out old.
		gtk_widget_destroy(popup);
	popup = gtk_menu_new();     // Create popup menu.
	if (files) {
		// Add "File" submenu.
		GtkWidget *mitem = Add_menu_item(popup, "File...");
		GtkWidget *file_menu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem), file_menu);
		Add_menu_item(file_menu, "Save",
		              G_CALLBACK(on_browser_file_save), this);
		Add_menu_item(file_menu, "Revert",
		              G_CALLBACK(on_browser_file_revert), this);
	}
	if (selected >= 0)      // Item selected?  Add groups.
		add_group_submenu(popup);
	return popup;
}

/*
 *  Callbacks for controls:
 */
static void
on_find_down(GtkButton       *button,
             gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->search(gtk_entry_get_text(
	                    GTK_ENTRY(chooser->get_find_text())), 1);
}
static void
on_find_up(GtkButton       *button,
           gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->search(gtk_entry_get_text(
	                    GTK_ENTRY(chooser->get_find_text())), -1);
}
static gboolean
on_find_key(GtkEntry   *entry,
            GdkEventKey    *event,
            gpointer    user_data) {
	ignore_unused_variable_warning(entry);
	if (event->keyval == GDK_KEY_Return) {
		auto *chooser = static_cast<Object_browser *>(user_data);
		chooser->search(gtk_entry_get_text(
		                    GTK_ENTRY(chooser->get_find_text())), 1);
		return TRUE;
	}
	return FALSE;           // Let parent handle it.
}

static void
on_loc_down(GtkButton       *button,
            gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->locate(false);
}
static void
on_loc_up(GtkButton       *button,
          gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->locate(true);
}

static void
on_move_down(GtkButton       *button,
             gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->move(false);
}
static void
on_move_up(GtkButton       *button,
           gpointer         user_data) {
	ignore_unused_variable_warning(button);
	auto *chooser = static_cast<Object_browser *>(user_data);
	chooser->move(true);
}


/*
 *  Create box with various controls.
 *
 *  Note:   'this' is passed as user-data to all the signal handlers,
 *      which call various virtual methods.
 */

GtkWidget *Object_browser::create_controls(
    int controls            // Browser_control flags.
) {
	GtkWidget *topframe = gtk_frame_new(nullptr);
	gtk_widget_show(topframe);

	// Everything goes in here.
	GtkWidget *tophbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(tophbox), FALSE);
	gtk_widget_show(tophbox);
	gtk_container_add(GTK_CONTAINER(topframe), tophbox);
	/*
	 *  The 'Find' controls.
	 */
	if (controls & static_cast<int>(find_controls)) {
		GtkWidget *frame = gtk_frame_new("Find");
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 2);

		GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous(GTK_BOX(hbox2), FALSE);
		gtk_widget_show(hbox2);
		gtk_container_add(GTK_CONTAINER(frame), hbox2);

		find_text = gtk_entry_new();
		gtk_editable_set_editable(GTK_EDITABLE(find_text), TRUE);
		gtk_entry_set_visibility(GTK_ENTRY(find_text), TRUE);
		gtk_widget_set_can_focus(GTK_WIDGET(find_text), TRUE);
		gtk_widget_show(find_text);
		gtk_box_pack_start(GTK_BOX(hbox2), find_text, FALSE, FALSE, 0);
		gtk_widget_set_size_request(find_text, 110, -1);
		GtkWidget *hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous(GTK_BOX(hbox3), TRUE);
		gtk_widget_show(hbox3);
		gtk_box_pack_start(GTK_BOX(hbox2), hbox3, FALSE, FALSE, 0);

		GtkWidget *find_down = Create_arrow_button(
		                           GTK_ARROW_DOWN,
		                           G_CALLBACK(on_find_down), this);
		gtk_box_pack_start(GTK_BOX(hbox3), find_down, TRUE, TRUE, 2);

		GtkWidget *find_up = Create_arrow_button(GTK_ARROW_UP,
		                     G_CALLBACK(on_find_up), this);
		gtk_box_pack_start(GTK_BOX(hbox3), find_up, TRUE, TRUE, 2);
		g_signal_connect(G_OBJECT(find_text), "key-press-event",
		                 G_CALLBACK(on_find_key), this);
	}
	/*
	 *  The 'Locate' controls.
	 */
	if (controls & static_cast<int>(locate_controls)) {
		GtkWidget *frame = gtk_frame_new("Locate");
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 2);
		GtkWidget *lbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous(GTK_BOX(lbox), FALSE);
		gtk_widget_show(lbox);
		gtk_container_add(GTK_CONTAINER(frame), lbox);
		GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous(GTK_BOX(bbox), TRUE);
		gtk_widget_show(bbox);
		gtk_box_pack_start(GTK_BOX(lbox), bbox, TRUE, TRUE, 2);
		loc_down = Create_arrow_button(GTK_ARROW_DOWN,
		                               G_CALLBACK(on_loc_down), this);
		gtk_box_pack_start(GTK_BOX(bbox), loc_down, TRUE, TRUE, 2);

		loc_up = Create_arrow_button(GTK_ARROW_UP,
		                             G_CALLBACK(on_loc_up), this);
		gtk_box_pack_start(GTK_BOX(bbox), loc_up, TRUE, TRUE, 2);
		if (controls & static_cast<int>(locate_frame)) {
			GtkWidget *lbl = gtk_label_new(" F:");
			gtk_label_set_xalign(GTK_LABEL(lbl), 0.9);
			gtk_label_set_yalign(GTK_LABEL(lbl), 0.5);
			gtk_box_pack_start(GTK_BOX(lbox), lbl, TRUE, TRUE, 0);
			gtk_widget_show(lbl);
			loc_f = gtk_entry_new();
			gtk_editable_set_editable(GTK_EDITABLE(loc_f), TRUE);
			gtk_entry_set_visibility(GTK_ENTRY(loc_f), TRUE);
			gtk_widget_set_can_focus(GTK_WIDGET(loc_f), TRUE);
			gtk_widget_show(loc_f);
			gtk_box_pack_start(GTK_BOX(lbox), loc_f, TRUE, TRUE, 4);
			gtk_widget_set_size_request(loc_f, 64, -1);
		}
		if (controls & static_cast<int>(locate_quality)) {
			GtkWidget *lbl = gtk_label_new(" Q:");
			gtk_label_set_xalign(GTK_LABEL(lbl), 0.9);
			gtk_label_set_yalign(GTK_LABEL(lbl), 0.5);
			gtk_box_pack_start(GTK_BOX(lbox), lbl, TRUE, TRUE, 0);
			gtk_widget_show(lbl);
			loc_q = gtk_entry_new();
			gtk_editable_set_editable(GTK_EDITABLE(loc_q), TRUE);
			gtk_entry_set_visibility(GTK_ENTRY(loc_q), TRUE);
			gtk_widget_set_can_focus(GTK_WIDGET(loc_q), TRUE);
			gtk_widget_show(loc_q);
			gtk_box_pack_start(GTK_BOX(lbox), loc_q, TRUE, TRUE, 4);
			gtk_widget_set_size_request(loc_q, 64, -1);
		}
	}
	/*
	 *  The 'Move' controls.
	 */
	if (controls & static_cast<int>(move_controls)) {
		GtkWidget *frame = gtk_frame_new("Move");
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 2);
		GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous(GTK_BOX(bbox), TRUE);
		gtk_widget_show(bbox);
		gtk_container_add(GTK_CONTAINER(frame), bbox);

		move_down = Create_arrow_button(GTK_ARROW_DOWN,
		                                G_CALLBACK(on_move_down), this);
		gtk_box_pack_start(GTK_BOX(bbox), move_down, TRUE, TRUE, 2);
		move_up = Create_arrow_button(GTK_ARROW_UP,
		                              G_CALLBACK(on_move_up), this);
		gtk_box_pack_start(GTK_BOX(bbox), move_up, TRUE, TRUE, 2);
	}
	return topframe;
}

