/**
 ** Ucbrowse.cc - Browse usecode functions.
 **
 ** Written: Nov. 19, 2006 - JSF
 **/

/*
Copyright (C) 2001-2020 The Exult Team

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

#include "ucbrowse.h"
#include "ucsymtbl.h"
#include "utils.h"

using std::ifstream;
using std::string;

/*  Columns in our table. */
enum { NAME_COL, NUM_COL, TYPE_COL, N_COLS };
/*  Sort ID's. */
enum { SORTID_NAME, SORTID_NUM, SORTID_TYPE };

/*
 *  Open browser window.
 */

const char *ExultStudio::browse_usecode(
    bool want_objfun
) {
	if (!ucbrowsewin) {         // First time?
		ucbrowsewin = new Usecode_browser();
		set_toggle("view_uc_functions", !want_objfun, !want_objfun);
		set_toggle("view_uc_classes", !want_objfun, !want_objfun);
		set_toggle("view_uc_shapes", true);
		set_toggle("view_uc_objects", true);
		ucbrowsewin->setup_list();
	}
	ucbrowsewin->show(true);
	while (gtk_widget_get_visible(GTK_WIDGET(ucbrowsewin->get_win())))  // Spin.
		gtk_main_iteration();   // (Blocks).
	const char *choice = ucbrowsewin->get_choice();
	return choice;
}

/*
 *  Usecode_browser window's okay button.
 */
C_EXPORT void on_usecodes_ok_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->okay();
}

/*
 *  Row was double-clicked.
 */
C_EXPORT void on_usecodes_treeview_row_activated(
    GtkTreeView *treeview,
    GtkTreePath *path,
    GtkTreeViewColumn *column,
    gpointer user_data
) {
	ignore_unused_variable_warning(path, column, user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(treeview))), "user_data"));
	ucb->okay();
}

/*
 *  Usecode_browser window's cancel button.
 */
C_EXPORT void on_usecodes_cancel_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->cancel();
}

/*
 *  Usecode_browser window's X button.
 */
C_EXPORT gboolean on_usecodes_dialog_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(event, user_data);
	auto *ucb = static_cast<Usecode_browser *>(
	                g_object_get_data(G_OBJECT(widget), "user_data"));

	ucb->cancel();
	return TRUE;
}

/*
 *  View classes/functions toggled.
 */
C_EXPORT void on_view_uc_classes_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->setup_list();
}
C_EXPORT void on_view_uc_functions_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->setup_list();
}
C_EXPORT void on_view_uc_shapes_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->setup_list();
}

C_EXPORT void on_view_uc_objects_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	auto *ucb = static_cast<Usecode_browser *>(g_object_get_data(
	                G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(btn))), "user_data"));
	ucb->setup_list();
}

/*
 *  Comparison for sorting each (text) column.
 */
static gint ucbrowser_compare_func(
    GtkTreeModel *model,
    GtkTreeIter  *a,
    GtkTreeIter  *b,
    gpointer      userdata
) {
	gint colnum = *static_cast<gint *>(userdata);
	gint ret = 0;
	gchar *name1 = nullptr;
	gchar *name2 = nullptr;
	gtk_tree_model_get(model, a, colnum, &name1, -1);
	gtk_tree_model_get(model, b, colnum, &name2, -1);

	if (name1 == nullptr || name2 == nullptr) {
		if (name1 == nullptr && name2 == nullptr)
			ret = 0;
		else
			ret = (name1 == nullptr) ? -1 : 1;
	} else
		ret = g_utf8_collate(name1, name2);
	return ret;
}

extern "C" {
	void gint_deleter(gpointer data) {
		gint *ptr = static_cast<gint *>(data);
		delete ptr;
	}
}
/*
 *  Create usecode browser window.
 */

Usecode_browser::Usecode_browser(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	win = studio->get_widget("usecodes_dialog");
	g_object_set_data(G_OBJECT(win), "user_data", this);
	string ucname = get_system_path("<PATCH>/usecode");
	if (!U7exists(ucname.c_str())) {
		ucname = get_system_path("<STATIC>/usecode");
		if (!U7exists(ucname.c_str()))
			ucname = "";
	}
	studio->set_entry("usecodes_file", ucname.c_str());
	/*
	 *  Set up table.
	 */
	model = gtk_tree_store_new(
	            N_COLS,
	            G_TYPE_STRING,      // Name.
	            G_TYPE_STRING,      // Number.
	            G_TYPE_STRING);     // Type:  function, class.
	// Create view, and set our model.
	tree = studio->get_widget("usecodes_treeview");
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(model));;
	// Set up sorting.
	GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);
	gint *iname = new gint(NAME_COL);
	gint *inum = new gint(NUM_COL);
	gint *itype = new gint(TYPE_COL);
	gtk_tree_sortable_set_sort_func(sortable, SORTID_NAME,
	                                ucbrowser_compare_func, iname, gint_deleter);
	gtk_tree_sortable_set_sort_func(sortable, SORTID_NUM,
	                                ucbrowser_compare_func, inum, gint_deleter);
	gtk_tree_sortable_set_sort_func(sortable, SORTID_TYPE,
	                                ucbrowser_compare_func, itype, gint_deleter);
	// Create each column.
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
	                             "Name", renderer, "text", NAME_COL, nullptr);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
	gtk_tree_view_column_set_sort_column_id(col, SORTID_NAME);
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
	          "Number", renderer, "text", NUM_COL, nullptr);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
	gtk_tree_view_column_set_sort_column_id(col, SORTID_NUM);
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
	          "Type", renderer, "text", TYPE_COL, nullptr);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
	gtk_tree_view_column_set_sort_column_id(col, SORTID_TYPE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
	// Set initial sort.
	gtk_tree_sortable_set_sort_column_id(sortable, SORTID_NAME,
	                                     GTK_SORT_ASCENDING);
	gtk_widget_show(tree);
}

/*
 *  Delete.
 */

Usecode_browser::~Usecode_browser(
) {
	g_object_unref(model);
	gtk_widget_destroy(win);
}

/*
 *  Show/hide.
 */

void Usecode_browser::show(
    bool tf
) {
	if (tf)
		gtk_widget_show(win);
	else
		gtk_widget_hide(win);
}

/*
 *  "Okay" button.
 */

void Usecode_browser::okay(
) {
	GtkTreeModel *model;
	GtkTreeIter iter;

	choice = "";
	/* This will only work in single or browse selection mode! */
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gchar *name;
		gtk_tree_model_get(model, &iter, NAME_COL, &name, -1);
		if (name[0])
			choice = name;
		g_free(name);
		if (choice.empty()) { // No name? Get number.
			gtk_tree_model_get(model, &iter, NUM_COL, &name, -1);
			choice = name;
			g_free(name);
		}
		g_print("selected row is: %s\n", choice.c_str());
	}
	show(FALSE);
}

/*
 *  Set up list of usecode entries.
 */

void Usecode_browser::setup_list(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	const char *ucfile = studio->get_text_entry("usecodes_file");
	ifstream in;
	U7open(in, ucfile);
	Usecode_symbol_table symtbl;
	unsigned int magic = Read4(in);     // Test for symbol table.
	if (!in.good()) {
		EStudio::Alert("Error reading '%s'.", ucfile);
		return;
	}
	if (magic != UCSYMTBL_MAGIC0 || (magic = Read4(in))
	        != UCSYMTBL_MAGIC1)
		return;
	symtbl.read(in);
	gtk_tree_store_clear(model);
	bool show_funs = studio->get_toggle("view_uc_functions");
	bool show_classes = studio->get_toggle("view_uc_classes");
	bool show_shapes = studio->get_toggle("view_uc_shapes");
	bool show_objects = studio->get_toggle("view_uc_objects");
	const Usecode_symbol_table::Syms_vector &syms = symtbl.get_symbols();
	for (auto *sym : syms) {
		Usecode_symbol::Symbol_kind kind = sym->get_kind();
		const char *kindstr = nullptr;
		const char *nm = sym->get_name();
		if (!nm[0])
			continue;
		switch (kind) {
		case Usecode_symbol::fun_defined:
			if (!show_funs)
				continue;
			kindstr = "Utility";
			break;
		case Usecode_symbol::shape_fun:
			if (!show_shapes)
				continue;
			kindstr = "Shape";
			break;
		case Usecode_symbol::object_fun:
			if (!show_objects)
				continue;
			kindstr = "Object";
			break;
		case Usecode_symbol::class_scope:
			if (!show_classes)
				continue;
			kindstr = "Class";
			break;
		default:
			continue;
		}
		char num[20];
		sprintf(num, "%05xH", sym->get_val());
		GtkTreeIter iter;
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(model, &iter, NAME_COL, nm,
		                   NUM_COL, num, TYPE_COL, kindstr, -1);
	}
}

