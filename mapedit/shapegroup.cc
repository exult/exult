/**
 ** A group of shapes/chunks that can be used as a 'palette'.
 **
 ** Written: 1/22/02 - JSF
 **/

/*
Copyright (C) 2002-2022 The Exult Team

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

#include "shapegroup.h"

#include "Flex.h"
#include "exceptions.h"
#include "objbrowse.h"
#include "shapefile.h"
#include "shapevga.h"
#include "studio.h"
#include "utils.h"

#include <cassert>
#include <cstdlib>
#include <fstream>

using EStudio::Alert;
using std::ios;
using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;

/*
 *  Create an empty group.
 */

Shape_group::Shape_group(
		const char*       nm,    // Name.  Copied.
		Shape_group_file* f,
		int               built    // Builtin type.
		)
		: name(nm), file(f), builtin(built) {
	if (builtin == -1) {
		return;
	}
	// Add shapes for builtin group.
	ExultStudio* es = ExultStudio::get_instance();
	auto*        vgafile
			= static_cast<Shapes_vga_file*>(es->get_vgafile()->get_ifile());
	// Read info. the first time.
	if (vgafile->read_info(es->get_game_type(), true)) {
		es->set_shapeinfo_modified();
	}
	int        i;
	const int  cnt      = vgafile->get_num_shapes();
	const bool modified = file->modified;

	if (builtin >= 0 && builtin <= 15) {
		for (i = 0; i < cnt; i++) {
			if (vgafile->get_info(i).get_shape_class() == builtin) {
				add(i);
			}
		}
	} else {
		switch (builtin) {
		case ammo_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_ammo_info()) {
					add(i);
				}
			}
			break;
		case armour_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_armor_info()) {
					add(i);
				}
			}
			break;
		case monsters_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_monster_info()) {
					add(i);
				}
			}
			break;
		case weapons_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_weapon_info()
					&& !vgafile->get_info(i).has_monster_info()) {
					add(i);
				}
			}
			break;
		case containers_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).get_gump_shape() >= 0) {
					add(i);
				}
			}
			break;
		case mountain_top:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).get_mountain_top_type()) {
					add(i);
				}
			}
			break;
		case animation_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_animation_info()) {
					add(i);
				}
			}
			break;
		case barge_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).get_barge_type()) {
					add(i);
				}
			}
			break;
		case body_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_body_info()) {
					add(i);
				}
			}
			break;
		case explosion_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_explosion_info()) {
					add(i);
				}
			}
			break;
		case npcflags_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).get_actor_flags()) {
					add(i);
				}
			}
			break;
		case npcpaperdoll_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_npc_paperdoll_info()) {
					add(i);
				}
			}
			break;
		case sfx_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_sfx_info()) {
					add(i);
				}
			}
			break;
		case content_rules_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_content_rules()) {
					add(i);
				}
			}
			break;
		case frameflags_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_frame_flags()) {
					add(i);
				}
			}
			break;
		case framehps_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_effective_hp_info()) {
					add(i);
				}
			}
			break;
		case framenames_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_frame_name_info()) {
					add(i);
				}
			}
			break;
		case frameusecode_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_frame_usecode_info()) {
					add(i);
				}
			}
			break;
		case objpaperdoll_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_paperdoll_info()) {
					add(i);
				}
			}
			break;
		case warmth_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_warmth_info()) {
					add(i);
				}
			}
			break;
		case light_group:
			for (i = 0; i < cnt; i++) {
				if (vgafile->get_info(i).has_light_info()) {
					add(i);
				}
			}
			break;
		}
	}
	file->modified = modified;    // Add() sets this.
}

/*
 *  Remove i'th entry.
 */

void Shape_group::del(int i) {
	assert(i >= 0 && i < size());
	std::vector<int>::erase(begin() + i);
	file->modified = true;
}

/*
 *  Swap two entries.
 */

void Shape_group::swap(int i    // Lower one.
) {
	const int x0   = (*this)[i];
	(*this)[i]     = (*this)[i + 1];
	(*this)[i + 1] = x0;
	file->modified = true;
}

/*
 *  Add a new entry if not already there.
 */

void Shape_group::add(int id) {
	for (const int it : *this) {
		if (it == id) {
			return;    // Already there.
		}
	}
	push_back(id);
	file->modified = true;
}

/*
 *  Save shape group to a file.
 */
void Shape_group::write(ODataSource& out) {
	// Name, #entries, entries(2-bytes).
	out.write(name.c_str(), name.size() + 1);
	out.write2(size());    // # entries.
	for (auto elem : *this) {
		out.write2(elem);
	}
}

/*
 *  Init. and read in (if it exists) a groups file.
 */

Shape_group_file::Shape_group_file(const char* nm    // Basename.
								   )
		: name(nm), modified(false) {
	unique_ptr<Flex>  flex;
	const std::string patchname  = "<PATCH>/" + name;
	const std::string staticname = "<STATIC>/" + name;
	if (U7exists(patchname)) {    // First try 'patch' directory.
		flex = make_unique<FlexFile>(patchname.c_str());
	} else if (U7exists(staticname)) {
		flex = make_unique<FlexFile>(staticname.c_str());
	}
	if (flex) {    // Exists?
		const int cnt = flex->number_of_objects();
		for (int i = 0; i < cnt; i++) {
			// Get each group.
			std::size_t len;
			auto        buf   = flex->retrieve(i, len);
			const char* gname = reinterpret_cast<const char*>(
					buf.get());    // Starts with name.
			const unsigned char* ptr = buf.get() + strlen(gname) + 1;
			const size_t         sz  = Read2(ptr);    // Get # entries.
			assert((len - (ptr - buf.get())) / 2 == sz);
			auto* grp = new Shape_group(gname, this);
			grp->reserve(sz);
			for (size_t j = 0; j < sz; j++) {
				grp->push_back(Read2(ptr));
			}
			groups.push_back(grp);
		}
	}
}

/*
 *  Search for a group with a given name.
 *
 *  Output: Index if found, else -1.
 */

int Shape_group_file::find(const char* nm) {
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		if ((*it)->name == nm) {
			return it - groups.begin();
		}
	}
	return -1;
}

/*
 *  Clean up.
 */

Shape_group_file::~Shape_group_file() {
	for (auto* group : groups) {
		delete group;    // Delete each group.
	}
}

/*
 *  Remove and delete a group.
 */

void Shape_group_file::remove(
		int  index,
		bool del    // True to delete the group.
) {
	modified = true;
	assert(index >= 0 && unsigned(index) < groups.size());
	Shape_group* grp = groups[index];
	groups.erase(groups.begin() + index);
	if (del) {
		delete grp;
	}
}

/*
 *  Get/create desired builtin group.
 */

Shape_group* Shape_group_file::get_builtin(
		int         menindex,    // Index of item in 'builtin_group'.
		const char* nm           // Label on menu item.
) {
	int       type;
	const int grpdelta
			= Shape_group::last_builtin_group - Shape_group::first_group;
	if (menindex < grpdelta) {
		type = menindex + Shape_group::ammo_group;
	} else {
		switch (menindex - grpdelta) {
		case 0:
			type = Shape_info::unusable;
			break;
		case 1:
			type = Shape_info::quality;
			break;
		case 2:
			type = Shape_info::quantity;
			break;
		case 3:
			type = Shape_info::has_hp;
			break;
		case 4:
			type = Shape_info::quality_flags;
			break;
		case 5:
			type = Shape_info::container;
			break;
		case 6:
			type = Shape_info::hatchable;
			break;
		case 7:
			type = Shape_info::spellbook;
			break;
		case 8:
			type = Shape_info::barge;
			break;
		case 9:
			type = Shape_info::virtue_stone;
			break;
		case 10:
			type = Shape_info::monster;
			break;
		case 11:
			type = Shape_info::human;
			break;
		case 12:
			type = Shape_info::building;
			break;
		default:
			type = -1;
			break;
		}
	}
	if (type < 0 || type >= Shape_group::last_builtin_group) {
		return nullptr;
	}
	if (builtins.size() <= unsigned(type)) {
		builtins.resize(type + 1);
	}
	if (!builtins[type]) {    // Create if needed.
		builtins[type] = new Shape_group(nm, this, type);
	}
	return builtins[type];
}

/*
 *  Write out to the 'patch' directory.
 */

void Shape_group_file::write() {
	const std::string patchname = "<PATCH>/" + name;
	try {
		OFileDataSource out(patchname.c_str());
		Flex_writer     gfile(out, "ExultStudio shape groups", groups.size());
		// Write each group.
		for (auto* grp : groups) {
			gfile.write_object(grp);
		}
	} catch (exult_exception& e) {
		Alert("Error writing '%s'", patchname.c_str());
	}
	modified = false;
}

/*
 *  Get the tree row # (assuming just a simple list) for a given tree-
 *  model position.
 */

static int Get_tree_row(GtkTreePath* path) {
	gchar*    str = gtk_tree_path_to_string(path);
	const int row = atoi(str);
	g_free(str);
	return row;
}

static int Get_tree_row(
		GtkTreeModel* model,
		GtkTreeIter*  iter    // Position we want.
) {
	GtkTreePath* path = gtk_tree_model_get_path(model, iter);
	const int    row  = Get_tree_row(path);
	gtk_tree_path_free(path);
	return row;
}

/*
 *  Group buttons:
 */
C_EXPORT void on_groups_add_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->add_group();
}

C_EXPORT void on_groups_del_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->del_group();
}

C_EXPORT gboolean on_groups_new_name_key_press(
		GtkEntry* entry, GdkEventKey* event, gpointer user_data) {
	ignore_unused_variable_warning(entry, user_data);
	if (event->keyval == GDK_KEY_Return) {
		ExultStudio::get_instance()->add_group();
		return true;
	}
	return false;    // Let parent handle it.
}

C_EXPORT void on_open_builtin_group_clicked(
		GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->open_builtin_group_window();
}

/*
 *  Groups list signals:
 */
C_EXPORT void on_group_list_cursor_changed(GtkTreeView* tview) {
	GtkTreePath*       path;
	GtkTreeViewColumn* col;

	gtk_tree_view_get_cursor(tview, &path, &col);
	if (path == nullptr) {
		return;
	}
	ExultStudio::get_instance()->setup_group_controls();
}

void on_group_list_row_inserted(
		GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter,
		gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->groups_changed(model, path, iter);
}

void on_group_list_row_changed(
		GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter,
		gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->groups_changed(model, path, iter, true);
}

void on_group_list_row_deleted(
		GtkTreeModel* model, GtkTreePath* path, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->groups_changed(model, path, nullptr);
}

C_EXPORT void on_group_list_row_activated(
		GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* column,
		gpointer user_data) {
	ignore_unused_variable_warning(treeview, path, column, user_data);
	ExultStudio::get_instance()->open_group_window();
}

/* columns */
enum {
	GRP_FILE_COLUMN = 0,
	GRP_GROUP_COLUMN,
	GRP_NUM_COLUMNS
};

extern "C" {
void gulong_deleter(gpointer data) {
	auto* ptr = static_cast<gulong*>(data);
	delete ptr;
}
}

/*
 *  Initialize the list of shape groups for the file being shown in the
 *  browser.
 */

void ExultStudio::setup_groups() {
	Shape_group_file* groups = curfile->get_groups();
	if (!groups) {    // No groups?
		set_visible("groups_frame", false);
		return;
	}
	GtkTreeView*  tview  = GTK_TREE_VIEW(get_widget("group_list"));
	GtkTreeModel* oldmod = gtk_tree_view_get_model(tview);
	GtkTreeStore* model;
	gulong        addsig = 0;
	gulong        delsig = 0;
	gulong        chgsig = 0;
	if (!oldmod) {    // Create model first time.
		model = gtk_tree_store_new(
				GRP_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
		gtk_tree_view_set_model(tview, GTK_TREE_MODEL(model));
		g_object_unref(model);
		// Create column.
		GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
		g_object_set(renderer, "xalign", 0.0, nullptr);
		const gint col_offset = gtk_tree_view_insert_column_with_attributes(
				tview, -1, "Names", renderer, "text", GRP_FILE_COLUMN, nullptr);
		GtkTreeViewColumn* column
				= gtk_tree_view_get_column(tview, col_offset - 1);
		gtk_tree_view_column_set_clickable(column, true);
		addsig = g_signal_connect(
				G_OBJECT(model), "row-inserted",
				G_CALLBACK(on_group_list_row_inserted), this);
		// Store signal id with model.
		g_object_set_data_full(
				G_OBJECT(model), "row-inserted", new gulong(addsig),
				gulong_deleter);
		delsig = g_signal_connect(
				G_OBJECT(model), "row-deleted",
				G_CALLBACK(on_group_list_row_deleted), this);
		g_object_set_data_full(
				G_OBJECT(model), "row-deleted", new gulong(delsig),
				gulong_deleter);
		chgsig = g_signal_connect(
				G_OBJECT(model), "row-changed",
				G_CALLBACK(on_group_list_row_changed), this);
		g_object_set_data_full(
				G_OBJECT(model), "row-changed", new gulong(delsig),
				gulong_deleter);
	} else {
		model  = GTK_TREE_STORE(oldmod);
		addsig = *static_cast<gulong*>(
				g_object_get_data(G_OBJECT(model), "row-inserted"));
		delsig = *static_cast<gulong*>(
				g_object_get_data(G_OBJECT(model), "row-deleted"));
		chgsig = *static_cast<gulong*>(
				g_object_get_data(G_OBJECT(model), "row-changed"));
	}
	// Block this signal during creation.
	g_signal_handler_block(model, addsig);
	g_signal_handler_block(model, delsig);
	g_signal_handler_block(model, chgsig);
	gtk_tree_store_clear(model);
	set_visible("groups_frame", true);
	// Show builtins for shapes.vga.
	set_visible("builtin_groups", curfile == vgafile);
	gtk_tree_view_set_reorderable(tview, true);
	const int   cnt = groups->size();    // Add groups from file.
	GtkTreeIter iter;
	for (int i = 0; i < cnt; i++) {
		Shape_group* grp = groups->get(i);
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(
				model, &iter, GRP_FILE_COLUMN, grp->get_name(),
				GRP_GROUP_COLUMN, grp, -1);
	}
	g_signal_handler_unblock(model, addsig);
	g_signal_handler_unblock(model, delsig);
	g_signal_handler_unblock(model, chgsig);
	setup_group_controls();    // Enable/disable the controls.
}

/*
 *  Enable/disable the controls based on whether there's a selection.
 */

void ExultStudio::setup_group_controls() {
	set_visible("groups_frame", true);
	GtkTreeView*      tview = GTK_TREE_VIEW(get_widget("group_list"));
	GtkTreeSelection* list  = gtk_tree_view_get_selection(tview);
	if (list) {
		//		int row = static_cast<int>(list->data);
		//		set_sensitive("groups_open", true);
		set_sensitive("groups_del", true);
		//		set_sensitive("groups_up_arrow", row > 0);
		//		set_sensitive("groups_down_arrow", row < clist->rows);
	} else {
		//		set_sensitive("groups_open", false);
		set_sensitive("groups_del", false);
		//		set_sensitive("groups_up_arrow", false);
		//		set_sensitive("groups_down_arrow", false);
	}
}

/*
 *  Add/delete a new group.
 */

void ExultStudio::add_group() {
	if (!curfile) {
		return;
	}
	GtkTreeView*      tview  = GTK_TREE_VIEW(get_widget("group_list"));
	GtkTreeStore*     model  = GTK_TREE_STORE(gtk_tree_view_get_model(tview));
	const char*       nm     = get_text_entry("groups_new_name");
	Shape_group_file* groups = curfile->get_groups();
	// Make sure name isn't already there.
	if (nm && *nm && groups->find(nm) < 0) {
		auto*       grp = new Shape_group(nm, groups);
		GtkTreeIter iter;
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(
				model, &iter, GRP_FILE_COLUMN, nm, GRP_GROUP_COLUMN, grp, -1);
	}
	set_entry("groups_new_name", "");
}

void ExultStudio::del_group() {
	if (!curfile) {
		return;
	}
	GtkTreeView*      tview = GTK_TREE_VIEW(get_widget("group_list"));
	GtkTreeSelection* list  = gtk_tree_view_get_selection(tview);
	if (!list) {
		return;
	}
	GtkTreeModel* model;
	GtkTreeIter   iter;
	if (!gtk_tree_selection_get_selected(list, &model, &iter)) {
		return;
	}
	const int         row    = Get_tree_row(model, &iter);
	Shape_group_file* groups = curfile->get_groups();
	Shape_group*      grp    = groups->get(row);
	string            msg("Delete group '");
	msg += grp->get_name();
	msg += "'?";
	const int choice = prompt(msg.c_str(), "Yes", "No");
	if (choice != 0) {    // Yes?
		return;
	}
	delete groups->get(row);    // Delete the group.
	gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	// Close the group's windows.
	vector<GtkWindow*> toclose;
	for (auto* widg : group_windows) {
		auto* chooser = static_cast<Object_browser*>(
				g_object_get_data(G_OBJECT(widg), "browser"));
		if (chooser->get_group() == grp) {
			// A match?
			toclose.push_back(widg);
		}
	}
	for (auto* widg : toclose) {
		close_group_window(GTK_WIDGET(widg));
	}
}

/*
 *  A group has been inserted or deleted from the list.
 */

void ExultStudio::groups_changed(
		GtkTreeModel* model,
		GtkTreePath*  path,    // Position.
		GtkTreeIter*  loc,     // Insert/change location, or 0.
		bool          value    // True if value changed at loc.
) {
	if (!curfile) {
		return;
	}
	Shape_group_file* groups = curfile->get_groups();
	const int         row    = Get_tree_row(path);
	if (!loc) {
		groups->remove(row, false);
	} else {
		void* grpaddr = nullptr;
		gtk_tree_model_get(model, loc, GRP_GROUP_COLUMN, &grpaddr, -1);
		auto* grp = static_cast<Shape_group*>(grpaddr);
		if (value) {    // Changed?
			groups->set(grp, row);
		} else {
			groups->insert(grp, row);
		}
	}
}

/*
 *  Events on 'group' window:
 */
/*
 *  Npc window's close button.
 */
C_EXPORT gboolean on_group_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(event, user_data);
	ExultStudio::get_instance()->close_group_window(widget);
	return true;
}

C_EXPORT void on_group_up_clicked(GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto*        chooser = static_cast<Object_browser*>(g_object_get_data(
            G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))), "browser"));
	Shape_group* grp     = chooser->get_group();
	const int    i       = chooser->get_selected();
	if (grp && i > 0) {    // Moving item up.
		grp->swap(i - 1);
		ExultStudio::get_instance()->update_group_windows(grp);
	}
}

C_EXPORT void on_group_down_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto*        chooser = static_cast<Object_browser*>(g_object_get_data(
            G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))), "browser"));
	Shape_group* grp     = chooser->get_group();
	const int    i       = chooser->get_selected();
	if (grp && i < grp->size() - 1) {    // Moving down.
		grp->swap(i);
		ExultStudio::get_instance()->update_group_windows(grp);
	}
}

C_EXPORT void on_group_shape_remove_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto*        chooser = static_cast<Object_browser*>(g_object_get_data(
            G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))), "browser"));
	Shape_group* grp     = chooser->get_group();
	const int    i       = chooser->get_selected();
	if (grp && i >= 0) {
		grp->del(i);
		ExultStudio::get_instance()->update_group_windows(grp);
	}
}

/*
 *  Open a 'group' window for the currently selected group.
 */

void ExultStudio::open_group_window() {
	GtkTreeView*      tview = GTK_TREE_VIEW(get_widget("group_list"));
	GtkTreeSelection* list  = gtk_tree_view_get_selection(tview);
	if (!list || !curfile || !vgafile || !palbuf) {
		return;
	}
	GtkTreeModel* model;
	GtkTreeIter   iter;
	if (!gtk_tree_selection_get_selected(list, &model, &iter)) {
		return;
	}
	const int         row    = Get_tree_row(model, &iter);
	Shape_group_file* groups = curfile->get_groups();
	Shape_group*      grp    = groups->get(row);
	open_group_window(grp);
}

/*
 *  Open window for currently selected builtin group.
 */

void ExultStudio::open_builtin_group_window() {
	if (!curfile || !vgafile || !palbuf) {
		return;
	}
	Shape_group_file* groups = curfile->get_groups();
	GtkComboBox*      btn    = GTK_COMBO_BOX(get_widget("builtin_group"));
	assert(btn != nullptr);
	const int index = gtk_combo_box_get_active(btn);
	// gchar * label = gtk_combo_box_get_active_text(btn);
	GtkTreeIter iter;
	gtk_combo_box_get_active_iter(btn, &iter);
	GtkTreeModel* model = gtk_combo_box_get_model(btn);
	gchar*        label = nullptr;
	gtk_tree_model_get(model, &iter, 0, &label, -1);
	Shape_group* grp = groups->get_builtin(index, label);
	g_free(label);
	if (grp) {
		open_group_window(grp);
	}
}

/*
 *  Open window for given group.
 */

void ExultStudio::open_group_window(Shape_group* grp) {
	GError*      error = nullptr;
	GtkBuilder*  xml   = gtk_builder_new();
	const gchar* objects[]
			= {"group_window_goup_img", "group_window_godown_img",
			   "group_window_remove_img", "group_window", nullptr};
	if (!gtk_builder_add_objects_from_file(
				xml, glade_path, const_cast<gchar**>(objects), &error)) {
		g_warning("Couldn't load group window: %s", error->message);
		g_error_free(error);
		exit(1);
	}
	gtk_builder_connect_signals(xml, nullptr);
	GtkWidget*      grpwin = get_widget(xml, "group_window");
	Object_browser* chooser
			= curfile->create_browser(vgafile, palbuf.get(), grp);
	// Set xml as data on window.
	g_object_set_data(G_OBJECT(grpwin), "xml", xml);
	g_object_set_data(G_OBJECT(grpwin), "browser", chooser);
	// Set window title, name field.
	string title("Exult Group:  ");
	title += grp->get_name();
	gtk_window_set_title(GTK_WINDOW(grpwin), title.c_str());
	GtkWidget* field = get_widget(xml, "group_name");
	if (field) {
		gtk_entry_set_text(GTK_ENTRY(field), grp->get_name());
	}
	// Attach browser.
	GtkWidget* browser_box = get_widget(xml, "group_shapes");
	gtk_widget_set_visible(browser_box, true);
	gtk_box_pack_start(
			GTK_BOX(browser_box), chooser->get_widget(), true, true, 0);
	// Auto-connect doesn't seem to work.
	g_signal_connect(
			G_OBJECT(grpwin), "delete-event",
			G_CALLBACK(on_group_window_delete_event), this);
	group_windows.push_back(GTK_WINDOW(grpwin));
	gtk_widget_set_visible(grpwin, true);
}

/*
 *  Close a 'group' window.
 */

void ExultStudio::close_group_window(GtkWidget* grpwin) {
	// Remove from list.
	for (auto it = group_windows.begin(); it != group_windows.end(); ++it) {
		if (*it == GTK_WINDOW(grpwin)) {
			group_windows.erase(it);
			break;
		}
	}
	auto* xml = static_cast<GtkBuilder*>(
			g_object_get_data(G_OBJECT(grpwin), "xml"));
	auto* chooser = static_cast<Object_browser*>(
			g_object_get_data(G_OBJECT(grpwin), "browser"));
	delete chooser;
	gtk_widget_destroy(grpwin);
	g_object_unref(G_OBJECT(xml));
}

/*
 *  Save all modified group files.
 */

void ExultStudio::save_groups() {
	if (!files) {
		return;
	}
	const int cnt = files->size();
	for (int i = 0; i < cnt; i++) {    // Check each file.
		Shape_file_info*  info  = (*files)[i];
		Shape_group_file* gfile = info->get_groups();
		if (gfile && gfile->is_modified()) {
			gfile->write();
		}
	}
}

/*
 *  Return true if any groups have been modified.
 */

bool ExultStudio::groups_modified() {
	if (!files) {
		return false;
	}
	const int cnt = files->size();
	for (int i = 0; i < cnt; i++) {    // Check each file.
		Shape_file_info*  info  = (*files)[i];
		Shape_group_file* gfile = info->get_groups();
		if (gfile && gfile->is_modified()) {
			return true;
		}
	}
	return false;
}

/*
 *  Update all windows showing a given group.
 */

void ExultStudio::update_group_windows(
		Shape_group* grp    // Group, or 0 for all.
) {
	for (auto* win : group_windows) {
		auto* chooser = static_cast<Object_browser*>(
				g_object_get_data(G_OBJECT(win), "browser"));
		if (!grp || chooser->get_group() == grp) {
			// A match?
			chooser->setup_info();
			chooser->render();
		}
	}
}
