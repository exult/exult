/**
 ** Maps.cc - Multimap handling.
 **
 ** Written: February 2, 2004 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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

#include "endianio.h"
#include "fnames.h"
#include "servemsg.h"
#include "studio.h"
#include "utils.h"

using EStudio::Add_menu_item;

/*
 *  Find highest map #.
 */

static int Find_highest_map() {
	int n = 0;
	int next;

	while ((next = Find_next_map(n + 1, 10)) != -1) {
		n = next;
	}
	return n;
}

/*
 *  Jump to desired map.
 */

static void on_map_activate(GtkMenuItem* item, gpointer udata) {
	ignore_unused_variable_warning(item);
	unsigned char  data[50];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, reinterpret_cast<uintptr>(udata));
	ExultStudio::get_instance()->send_to_server(
			Exult_server::goto_map, &data[0], ptr - data);
}

C_EXPORT void on_main_map_activate(GtkMenuItem* item, gpointer udata) {
	ignore_unused_variable_warning(udata);
	on_map_activate(item, nullptr);
}

/*
 *  Open new-map dialog.
 */

C_EXPORT void on_newmap_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->new_map_dialog();
}

void ExultStudio::new_map_dialog() {
	GtkWidget* win = get_widget("newmap_dialog");
	gtk_window_set_modal(GTK_WINDOW(win), true);
	const int highest = Find_highest_map();
	set_spin("newmap_num", highest + 1, 1, 100);
	set_entry("newmap_name", "", false);    // LATER.
	set_spin("newmap_copy_num", 0, 0, highest);
	set_toggle("newmap_copy_flats", false);
	set_toggle("newmap_copy_fixed", false);
	set_toggle("newmap_copy_ireg", false);
	gtk_widget_set_visible(win, true);
}

/*
 *  Copy from patch, else from static directory to new map in patch.
 */

static bool Copy_static_file(
		const char* sname,      // Name in static, from fnames.h.
		const char* pname,      // Name in patch.
		int         frommap,    // # of map to copy from.
		int         tomap       // # of map to copy to.
) {
	char srcname[128];
	char destname[128];
	// Check length to prevent buffer overflow
	if (strlen(pname) >= sizeof(destname) - 20) {    // -20 for map dir suffix
		std::cerr << "Name too long for Get_mapped_name: " << pname
				  << std::endl;
		return false;
	}
	if (strlen(sname) >= sizeof(srcname) - 20) {
		std::cerr << "Name too long for Get_mapped_name: " << sname
				  << std::endl;
		return false;
	}
	Get_mapped_name(pname, tomap, destname);
	Get_mapped_name(pname, frommap, srcname);    // Patch?
	if (U7exists(srcname) && EStudio::Copy_file(srcname, destname)) {
		return true;
	}
	Get_mapped_name(sname, frommap, srcname);    // Static next.
	if (U7exists(srcname)) {
		return EStudio::Copy_file(srcname, destname);
	}
	return true;
}

/*
 *  Create new map.
 */
C_EXPORT void on_newmap_ok_clicked(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	char         fname[128];
	char         sname[128];
	ExultStudio* studio = ExultStudio::get_instance();
	GtkWidget*   win    = studio->get_widget("newmap_dialog");
	const int    num    = studio->get_spin("newmap_num");

	if (U7exists(Get_mapped_name("<PATCH>/", num, fname))
		|| U7exists(Get_mapped_name("<STATIC>/", num, sname))) {
		EStudio::Alert("Map %02x already exists", num);
		return;    // Leave dialog open.
	}
	U7mkdir(fname, 0755);    // Create map directory in 'patch'.
	const int frommap = studio->get_spin("newmap_copy_num");
	if (studio->get_toggle("newmap_copy_flats")) {
		Copy_static_file(U7MAP, PATCH_U7MAP, frommap, num);
	}
	if (studio->get_toggle("newmap_copy_fixed")) {
		for (int schunk = 0; schunk < 12 * 12; schunk++) {
			char pname[128];
			char sname[128];
			snprintf(pname, sizeof(pname), "%s%02x", PATCH_U7IFIX, schunk);
			snprintf(sname, sizeof(sname), "%s%02x", U7IFIX, schunk);
			if (!Copy_static_file(sname, pname, frommap, num)) {
				break;
			}
		}
	}
	U7mkdir(Get_mapped_name(GAMEDAT, num, fname), 0755);
	if (studio->get_toggle("newmap_copy_ireg")) {
		char tname[128];
		for (int schunk = 0; schunk < 12 * 12; schunk++) {
			Get_mapped_name(U7IREG, frommap, fname);
			const size_t fnamelen = strlen(fname);
			snprintf(
					fname + fnamelen, sizeof(fname) - fnamelen, "%02x", schunk);
			Get_mapped_name(U7IREG, num, tname);
			const size_t tnamelen = strlen(tname);
			snprintf(
					tname + tnamelen, sizeof(tname) - tnamelen, "%02x", schunk);
			if (U7exists(fname)) {
				if (!EStudio::Copy_file(fname, tname)) {
					break;
				}
			}
		}
	}
	studio->setup_maps_list();
	gtk_widget_set_visible(win, false);
}

/*
 *  Set up the list of maps in the "maps" menu.
 */

void ExultStudio::setup_maps_list() {
	GtkWidget* maps
			= gtk_menu_item_get_submenu(GTK_MENU_ITEM(get_widget("map1")));
	GList* items
			= g_list_first(gtk_container_get_children(GTK_CONTAINER(maps)));
	GList*  each   = g_list_last(items);
	GSList* group  = nullptr;
	int     curmap = 0;

	while (each) {
		GtkMenuItem* item  = GTK_MENU_ITEM(each->data);
		GtkWidget*   label = gtk_bin_get_child(GTK_BIN(item));
		const char*  text  = gtk_label_get_label(GTK_LABEL(label));
		if (strcmp(text, "Main") == 0) {
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
			g_object_set_data(G_OBJECT(item), "user_data", nullptr);
			if (curmap == 0) {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), true);
			}
			break;
		}
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
			curmap = reinterpret_cast<sintptr>(
					g_object_get_data(G_OBJECT(item), "user_data"));
		}
		GList* prev = g_list_previous(each);
		gtk_container_remove(GTK_CONTAINER(maps), GTK_WIDGET(item));
		each = prev;
	}
	int num = 0;
	while ((num = Find_next_map(num + 1, 10)) != -1) {
		char name[40];
		snprintf(name, sizeof(name), "Map #%02x", num);
		auto*      ptrnum = reinterpret_cast<gpointer>(uintptr(num));
		GtkWidget* item   = Add_menu_item(
                maps, name, G_CALLBACK(on_map_activate), ptrnum, group);
		g_object_set_data(G_OBJECT(item), "user_data", ptrnum);
		if (curmap == num) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), true);
		}
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
	}
	g_list_free(items);
}
