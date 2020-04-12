#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cctype>
#include <gdk/gdkkeysyms.h>
#include "objbrowse.h"
#include "shapegroup.h"
#include "shapefile.h"
#include "studio.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"


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
	Object_browser *chooser = static_cast<Object_browser *>(udata);
	Shape_group *grp = static_cast<Shape_group *>(gtk_object_get_user_data(
	                       GTK_OBJECT(item)));
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
			                       GTK_SIGNAL_FUNC(
			                           Object_browser::on_browser_group_add),
			                       this);
			// Store group on menu item.
			gtk_object_set_user_data(GTK_OBJECT(gitem), grp);
		}
	}
}

/*
 *  Okay clicked in file-selector.
 */

void File_selector_ok(
    GtkButton *btn,
    gpointer user_data
) {
	GtkFileSelection *fsel = GTK_FILE_SELECTION(gtk_widget_get_toplevel(
	                             GTK_WIDGET(btn)));
	const char *fname = gtk_file_selection_get_filename(fsel);
	File_sel_okay_fun fun = reinterpret_cast<File_sel_okay_fun>(
	                        	reinterpret_cast<uintptr_t>(
									gtk_object_get_user_data(GTK_OBJECT(fsel))));
	if (fname && *fname && fun)
		(*fun)(fname, user_data);
}

/*
 *  Create a modal file selector.
 */

GtkFileSelection *Create_file_selection(
    const char *title,
    File_sel_okay_fun ok_handler,
    gpointer user_data
) {
	GtkFileSelection *fsel = GTK_FILE_SELECTION(gtk_file_selection_new(
	                             title));
	gtk_window_set_modal(GTK_WINDOW(fsel), true);
	gtk_object_set_user_data(GTK_OBJECT(fsel),
							reinterpret_cast<void *>(
	                        	reinterpret_cast<uintptr_t>(ok_handler)));
	gtk_signal_connect(GTK_OBJECT(fsel->ok_button), "clicked",
	                   GTK_SIGNAL_FUNC(File_selector_ok), user_data);
	// Destroy when done.
	gtk_signal_connect_object(GTK_OBJECT(fsel->ok_button), "clicked",
	                          GTK_SIGNAL_FUNC(gtk_widget_destroy),
	                          GTK_OBJECT(fsel));
	gtk_signal_connect_object(GTK_OBJECT(fsel->cancel_button), "clicked",
	                          GTK_SIGNAL_FUNC(gtk_widget_destroy),
	                          GTK_OBJECT(fsel));
	return fsel;
}

/*
 *  Save file in browser.
 */

void Object_browser::on_browser_file_save(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	Object_browser *chooser = static_cast<Object_browser *>(udata);
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
	Object_browser *chooser = static_cast<Object_browser *>(udata);
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
		chooser->show();
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
		              GTK_SIGNAL_FUNC(on_browser_file_save), this);
		Add_menu_item(file_menu, "Revert",
		              GTK_SIGNAL_FUNC(on_browser_file_revert), this);
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
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
	chooser->search(gtk_entry_get_text(
	                    GTK_ENTRY(chooser->get_find_text())), 1);
}
static void
on_find_up(GtkButton       *button,
           gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
	chooser->search(gtk_entry_get_text(
	                    GTK_ENTRY(chooser->get_find_text())), -1);
}
static gboolean
on_find_key(GtkEntry   *entry,
            GdkEventKey    *event,
            gpointer    user_data) {
	ignore_unused_variable_warning(entry);
	if (event->keyval == GDK_Return) {
		Object_browser *chooser = static_cast<Object_browser *>(user_data);
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
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
	chooser->locate(false);
}
static void
on_loc_up(GtkButton       *button,
          gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
	chooser->locate(true);
}

static void
on_move_down(GtkButton       *button,
             gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
	chooser->move(false);
}
static void
on_move_up(GtkButton       *button,
           gpointer         user_data) {
	ignore_unused_variable_warning(button);
	Object_browser *chooser = static_cast<Object_browser *>(user_data);
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
	GtkWidget *tophbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(tophbox);
	gtk_container_add(GTK_CONTAINER(topframe), tophbox);
	/*
	 *  The 'Find' controls.
	 */
	if (controls & static_cast<int>(find_controls)) {
		GtkWidget *frame = gtk_frame_new("Find");
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 2);

		GtkWidget *hbox2 = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(hbox2);
		gtk_container_add(GTK_CONTAINER(frame), hbox2);

		find_text = gtk_entry_new();
		gtk_editable_set_editable(GTK_EDITABLE(find_text), TRUE);
		gtk_entry_set_visibility(GTK_ENTRY(find_text), TRUE);
		GTK_OBJECT_SET_FLAGS(find_text, GTK_CAN_FOCUS);
		gtk_widget_show(find_text);
		gtk_box_pack_start(GTK_BOX(hbox2), find_text, FALSE, FALSE, 0);
		gtk_widget_set_size_request(find_text, 110, -1);
		GtkWidget *hbox3 = gtk_hbox_new(TRUE, 0);
		gtk_widget_show(hbox3);
		gtk_box_pack_start(GTK_BOX(hbox2), hbox3, FALSE, FALSE, 0);

		GtkWidget *find_down = Create_arrow_button(
		                           GTK_ARROW_DOWN,
		                           GTK_SIGNAL_FUNC(on_find_down), this);
		gtk_box_pack_start(GTK_BOX(hbox3), find_down, TRUE, TRUE, 2);

		GtkWidget *find_up = Create_arrow_button(GTK_ARROW_UP,
		                     GTK_SIGNAL_FUNC(on_find_up), this);
		gtk_box_pack_start(GTK_BOX(hbox3), find_up, TRUE, TRUE, 2);
		gtk_signal_connect(GTK_OBJECT(find_text), "key-press-event",
		                   GTK_SIGNAL_FUNC(on_find_key), this);
	}
	/*
	 *  The 'Locate' controls.
	 */
	if (controls & static_cast<int>(locate_controls)) {
		GtkWidget *frame = gtk_frame_new("Locate");
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(tophbox), frame, FALSE, FALSE, 2);
		GtkWidget *lbox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(lbox);
		gtk_container_add(GTK_CONTAINER(frame), lbox);
		GtkWidget *bbox = gtk_hbox_new(TRUE, 0);
		gtk_widget_show(bbox);
		gtk_box_pack_start(GTK_BOX(lbox), bbox, TRUE, TRUE, 2);
		loc_down = Create_arrow_button(GTK_ARROW_DOWN,
		                               GTK_SIGNAL_FUNC(on_loc_down), this);
		gtk_box_pack_start(GTK_BOX(bbox), loc_down, TRUE, TRUE, 2);

		loc_up = Create_arrow_button(GTK_ARROW_UP,
		                             GTK_SIGNAL_FUNC(on_loc_up), this);
		gtk_box_pack_start(GTK_BOX(bbox), loc_up, TRUE, TRUE, 2);
		if (controls & static_cast<int>(locate_frame)) {
			GtkWidget *lbl = gtk_label_new(" F:");
			gtk_misc_set_alignment(GTK_MISC(lbl), 0.9, 0.5);
			gtk_box_pack_start(GTK_BOX(lbox), lbl, TRUE, TRUE, 0);
			gtk_widget_show(lbl);
			loc_f = gtk_entry_new();
			gtk_editable_set_editable(GTK_EDITABLE(loc_f), TRUE);
			gtk_entry_set_visibility(GTK_ENTRY(loc_f), TRUE);
			GTK_OBJECT_SET_FLAGS(loc_f, GTK_CAN_FOCUS);
			gtk_widget_show(loc_f);
			gtk_box_pack_start(GTK_BOX(lbox), loc_f, TRUE, TRUE, 4);
			gtk_widget_set_size_request(loc_f, 64, -1);
		}
		if (controls & static_cast<int>(locate_quality)) {
			GtkWidget *lbl = gtk_label_new(" Q:");
			gtk_misc_set_alignment(GTK_MISC(lbl), 0.9, 0.5);
			gtk_box_pack_start(GTK_BOX(lbox), lbl, TRUE, TRUE, 0);
			gtk_widget_show(lbl);
			loc_q = gtk_entry_new();
			gtk_editable_set_editable(GTK_EDITABLE(loc_q), TRUE);
			gtk_entry_set_visibility(GTK_ENTRY(loc_q), TRUE);
			GTK_OBJECT_SET_FLAGS(loc_q, GTK_CAN_FOCUS);
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
		GtkWidget *bbox = gtk_hbox_new(TRUE, 0);
		gtk_widget_show(bbox);
		gtk_container_add(GTK_CONTAINER(frame), bbox);

		move_down = Create_arrow_button(GTK_ARROW_DOWN,
		                                GTK_SIGNAL_FUNC(on_move_down), this);
		gtk_box_pack_start(GTK_BOX(bbox), move_down, TRUE, TRUE, 2);
		move_up = Create_arrow_button(GTK_ARROW_UP,
		                              GTK_SIGNAL_FUNC(on_move_up), this);
		gtk_box_pack_start(GTK_BOX(bbox), move_up, TRUE, TRUE, 2);
	}
	return topframe;
}

