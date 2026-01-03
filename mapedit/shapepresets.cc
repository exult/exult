/*
Copyright (C) 2025 The Exult Team

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

#include "shapepresets.h"

#include "databuf.h"
#include "exceptions.h"
#include "shapefile.h"
#include "shapeinf.h"
#include "shapevga.h"
#include "studio.h"
#include "utils.h"

#include <cstring>

using std::string;
using std::vector;

/*
 *  Create preset.
 */
Shape_preset::Shape_preset(const char* nm, Shape_preset_file* f)
		: name(nm), file(f), modified(false) {}

/*
 *  Set a value in the preset.
 */
void Shape_preset::set_value(const string& key, const string& value) {
	data[key] = value;
	modified  = true;
	if (file) {
		file->modified = true;
	}
}

/*
 *  Get a value from the preset.
 */
string Shape_preset::get_value(const string& key) const {
	auto it = data.find(key);
	return it != data.end() ? it->second : string();
}

/*
 *  Check if preset has a key.
 */
bool Shape_preset::has_value(const string& key) const {
	return data.find(key) != data.end();
}

/*
 *  Write out preset.
 */
void Shape_preset::write(ODataSource& out) {
	out.write2(name.size());
	out.write(name.c_str(), name.size());

	// Write shape number directly as 4-byte int
	int shape_num = -1;
	if (has_value("shape_number")) {
		shape_num = std::atoi(get_value("shape_number").c_str());
	}
	out.write4(shape_num);
}

/*
 *  Read in preset.
 */
void Shape_preset::read(IDataSource& in) {
	const int name_len = in.read2();
	char*     nm       = new char[name_len + 1];
	in.read(nm, name_len);
	nm[name_len] = 0;
	name         = nm;
	delete[] nm;

	// Read shape number directly as 4-byte int
	const int shape_num = in.read4();
	if (shape_num >= 0) {
		data["shape_number"] = std::to_string(shape_num);
	}

	modified = false;
}

/*
 *  Create preset file.
 */
Shape_preset_file::Shape_preset_file(const char* nm)
		: filename(nm ? nm : ""), modified(false) {}

/*
 *  Delete.
 */
Shape_preset_file::~Shape_preset_file() {
	for (auto* preset : presets) {
		delete preset;
	}
}

/*
 *  Find by name.
 */
Shape_preset* Shape_preset_file::find(const char* nm) {
	for (auto* preset : presets) {
		if (std::strcmp(preset->get_name(), nm) == 0) {
			return preset;
		}
	}
	return nullptr;
}

/*
 *  Find index by name.
 */
int Shape_preset_file::find_index(const char* nm) {
	for (size_t i = 0; i < presets.size(); i++) {
		if (std::strcmp(presets[i]->get_name(), nm) == 0) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

/*
 *  Create a new preset.
 */
Shape_preset* Shape_preset_file::create(const char* nm) {
	auto* preset = new Shape_preset(nm, this);
	presets.push_back(preset);
	modified = true;
	return preset;
}

/*
 *  Remove a preset.
 */
void Shape_preset_file::remove(Shape_preset* preset) {
	for (auto it = presets.begin(); it != presets.end(); ++it) {
		if (*it == preset) {
			delete preset;
			presets.erase(it);
			modified = true;
			break;
		}
	}
}

/*
 *  Write out file.
 */
bool Shape_preset_file::write() {
	try {
		OFileDataSource out(filename.c_str());
		// Write header
		const char* header = "ExultStudio shape presets";
		out.write(header, std::strlen(header));
		out.write1(0);    // Null terminator

		// Write version
		out.write2(1);    // Version 1

		// Write preset count
		out.write4(presets.size());

		// Write each preset
		for (auto* preset : presets) {
			preset->write(out);
			preset->modified = false;
		}
		modified = false;
		return true;
	} catch (std::exception& e) {
		return false;
	}
}

/*
 *  Read in file.
 */
bool Shape_preset_file::read(const char* nm) {
	filename = nm;
	// Clean out old data.
	for (auto* preset : presets) {
		delete preset;
	}
	presets.clear();

	try {
		IFileDataSource in(nm);

		// Read and verify header
		char header[100];
		int  i = 0;
		while (i < 99 && (header[i] = in.read1()) != 0) {
			i++;
		}
		header[i] = 0;
		if (std::strcmp(header, "ExultStudio shape presets") != 0) {
			return false;
		}

		// Read version
		const int version = in.read2();
		if (version != 1) {
			return false;    // Unsupported version
		}

		// Read preset count
		const int count = in.read4();
		for (int j = 0; j < count; j++) {
			auto* preset = new Shape_preset("", this);
			preset->read(in);
			presets.push_back(preset);
		}
		modified = false;
		return true;
	} catch (std::exception& e) {
		return false;
	}
}

/*
 *  Read file, creating object.
 */
Shape_preset_file* Shape_preset_file::read_file(const char* pathname) {
	auto* pfile = new Shape_preset_file();
	if (!pfile->read(pathname)) {
		delete pfile;
		return nullptr;
	}
	return pfile;
}

/*
 *  Setup the presets list from the current preset file.
 */
void ExultStudio::setup_presets_list() {
	if (!presets_file) {
		return;
	}

	GtkTreeView*  list  = GTK_TREE_VIEW(get_widget("presets_list"));
	GtkListStore* store = GTK_LIST_STORE(gtk_tree_view_get_model(list));
	gtk_list_store_clear(store);

	const int cnt = presets_file->size();
	for (int i = 0; i < cnt; i++) {
		Shape_preset* preset = presets_file->get(i);
		if (!preset) {
			continue;
		}

		GdkPixbuf* shape_pixbuf      = nullptr;
		char       shape_num_str[16] = "";

		// Try to get shape number if it exists
		if (preset->has_value("shape_number")) {
			const int shape_num
					= std::atoi(preset->get_value("shape_number").c_str());

			// Get shape preview image
			if (vgafile && shape_num >= 0) {
				GdkPixbuf* full_pixbuf
						= shape_image(vgafile->get_ifile(), shape_num, 0, true);
				if (full_pixbuf) {
					// Scale to half size
					const int width  = gdk_pixbuf_get_width(full_pixbuf);
					const int height = gdk_pixbuf_get_height(full_pixbuf);
					shape_pixbuf     = gdk_pixbuf_scale_simple(
                            full_pixbuf, width / 2, height / 2,
                            GDK_INTERP_BILINEAR);
					g_object_unref(full_pixbuf);
				}
			}

			// Convert shape number to string
			snprintf(shape_num_str, sizeof(shape_num_str), "%d", shape_num);
		}

		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(
				store, &iter, 0, preset->get_name(), 1, shape_pixbuf, 2,
				shape_num_str, -1);

		// Free the pixbuf after adding to store (store takes a reference)
		if (shape_pixbuf) {
			g_object_unref(shape_pixbuf);
		}
	}
}

/*
 *  Save presets to file.
 */
void ExultStudio::save_shape_presets() {
	if (presets_file && presets_file->modified) {
		presets_file->write();
	}
}

/*
 *  Check if presets have been modified.
 */
bool ExultStudio::shape_presets_modified() {
	return presets_file && presets_file->modified;
}

/*
 *  Get the currently selected preset name from the list.
 */
static const char* get_selected_preset_name(ExultStudio* studio) {
	GtkTreeView*      list = GTK_TREE_VIEW(studio->get_widget("presets_list"));
	GtkTreeSelection* sel  = gtk_tree_view_get_selection(list);
	GtkTreeModel*     model;
	GtkTreeIter       iter;

	if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
		char* name;
		gtk_tree_model_get(model, &iter, 0, &name, -1);
		const char* result = name;
		return result;
	}
	return nullptr;
}

/*
 *  Save current shape to a preset.
 */
void ExultStudio::save_shape_to_preset(const char* preset_name) {
	if (!presets_file || !preset_name || !*preset_name) {
		return;
	}

	// Check if shape window is open
	if (!shapewin || !gtk_widget_get_visible(shapewin)) {
		EStudio::Alert("Please open the shape info window first");
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (!preset) {
		preset = presets_file->create(preset_name);
	}

	preset->clear_data();

	// Only save the shape number
	const int shape_num = get_num_entry("shinfo_shape");

	if (shape_num >= 0) {
		preset->set_value("shape_number", std::to_string(shape_num));

		// Verify the data was set
		if (!preset->has_value("shape_number")) {
			EStudio::Alert("Error: Data was not saved to preset!");
			return;
		}
	} else {
		EStudio::Alert("Invalid shape number");
		return;
	}

	setup_presets_list();
}

/*
 *  Load a preset to the current shape.
 */
void ExultStudio::load_preset_to_shape(const char* preset_name) {
	if (!presets_file || !preset_name) {
		return;
	}

	// Check if shape window is open
	if (!shapewin || !gtk_widget_get_visible(shapewin)) {
		EStudio::Alert("Please open the shape info window first");
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Only load the shape number
	if (preset->has_value("shape_number")) {
		GtkWidget* widget = get_widget("shinfo_shape");
		if (widget) {
			const int shape_num
					= std::atoi(preset->get_value("shape_number").c_str());
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), shape_num);
			// Trigger the shape change to load all shape data
			gtk_widget_activate(widget);
		}
	}
}

/*
 *  Apply a preset's shape settings to the current shape.
 */
void ExultStudio::apply_preset() {
	const char* preset_name = get_selected_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to apply");
		return;
	}

	// Check if shape window is open
	if (!shapewin || !gtk_widget_get_visible(shapewin)) {
		EStudio::Alert("Please open the shape info window first");
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (!preset || !preset->has_value("shape_number")) {
		return;
	}

	const int preset_shape_num
			= std::atoi(preset->get_value("shape_number").c_str());
	const int current_shape_num = get_num_entry("shinfo_shape");

	// Silently fail if preset shape == current shape
	if (preset_shape_num == current_shape_num) {
		return;
	}

	// Ask user for confirmation before applying
	if (EStudio::Prompt("Apply this preset to the current shape?", "Yes", "No")
		!= 0) {
		return;
	}

	// Get the shape info for the current shape
	auto* info = static_cast<Shape_info*>(
			g_object_get_data(G_OBJECT(shapewin), "user_data"));
	if (!info) {
		return;
	}

	// Get the Shapes_vga_file to access shape info by shape number
	auto* svga = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
	if (!svga) {
		EStudio::Alert("Cannot access shape information");
		return;
	}

	// Get the shape info for the preset shape
	Shape_info& preset_info = svga->get_info(preset_shape_num);

	// Save current state for undo before applying preset
	// Delete old backup if it exists to prevent memory leak
	if (temp_shape_info) {
		delete temp_shape_info;
		temp_shape_info = nullptr;
	}
	temp_shape_info = new Shape_info(*info);

	// Copy all shape data from preset shape to current shape
	*info = preset_info;

	// Mark as modified
	shape_info_modified = true;
	set_shape_window_dirty(true);

	// Re-initialize the shape notebook with the copied data
	const int frnum = get_num_entry("shinfo_frame");
	if (current_shape_num >= 0 && frnum >= 0) {
		init_shape_notebook(
				*info, get_widget("shinfo_notebook"), current_shape_num, frnum);
	}
}

/*
 *  Export a preset to a file.
 */
void ExultStudio::export_preset() {
	const char* preset_name = get_selected_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to export");
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Create file chooser dialog for saving
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
			"Export Preset to File", GTK_WINDOW(get_widget("shape_window")),
			GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL,
			"_Save", GTK_RESPONSE_ACCEPT, nullptr);

	gtk_file_chooser_set_do_overwrite_confirmation(
			GTK_FILE_CHOOSER(dialog), true);

	// Set default folder to patch directory
	if (is_system_path_defined("<PATCH>")) {
		const std::string patchdir = get_system_path("<PATCH>");
		gtk_file_chooser_set_current_folder(
				GTK_FILE_CHOOSER(dialog), patchdir.c_str());
	}

	// Add filter for .pre files
	GtkFileFilter* filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Preset files (*.pre)");
	gtk_file_filter_add_pattern(filter, "*.pre");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	char*      filename = nullptr;
	if (response == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
	gtk_widget_destroy(dialog);

	if (!filename) {
		return;
	}

	// Create a temporary preset file with just this one preset
	Shape_preset_file temp_file(filename);
	Shape_preset*     copy = temp_file.create(preset->get_name());

	// Copy all data
	for (const auto& [key, value] : preset->get_data()) {
		copy->set_value(key, value);
	}

	if (temp_file.write()) {
		EStudio::Alert("Preset exported successfully");
	} else {
		EStudio::Alert("Failed to export preset");
	}
	g_free(filename);
}

/*
 *  Import presets from a file.
 */
void ExultStudio::import_presets() {
	// Create file chooser dialog for opening
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
			"Import Presets File", GTK_WINDOW(get_widget("shape_window")),
			GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
			"_Open", GTK_RESPONSE_ACCEPT, nullptr);

	// Set default folder to patch directory
	if (is_system_path_defined("<PATCH>")) {
		const std::string patchdir = get_system_path("<PATCH>");
		gtk_file_chooser_set_current_folder(
				GTK_FILE_CHOOSER(dialog), patchdir.c_str());
	}

	// Add filter for .pre files
	GtkFileFilter* filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Preset files (*.pre)");
	gtk_file_filter_add_pattern(filter, "*.pre");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	char*      filename = nullptr;
	if (response == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	}
	gtk_widget_destroy(dialog);

	if (!filename) {
		return;
	}

	Shape_preset_file* import_file = Shape_preset_file::read_file(filename);
	if (!import_file) {
		EStudio::Alert("This is not a Shape preset. Import aborted.");
		g_free(filename);
		return;
	}
	g_free(filename);

	const int count = import_file->size();
	for (int i = 0; i < count; i++) {
		Shape_preset* src = import_file->get(i);
		if (src) {
			// Append " imported" to the preset name
			std::string imported_name
					= std::string(src->get_name()) + " imported";

			// Check if preset already exists
			Shape_preset* dest = presets_file->find(imported_name.c_str());
			if (!dest) {
				dest = presets_file->create(imported_name.c_str());
			}

			dest->clear_data();
			for (const auto& [key, value] : src->get_data()) {
				dest->set_value(key, value);
			}
		}
	}

	delete import_file;
	setup_presets_list();
	EStudio::Alert("Presets imported successfully");
}

/*
 *  Clone the selected preset.
 */
void ExultStudio::clone_preset() {
	const char* preset_name = get_selected_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to clone");
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Create a simple dialog to get new name
	GtkWidget* dialog = gtk_message_dialog_new(
			GTK_WINDOW(get_widget("shape_window")), GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
			"Enter name for cloned preset:");
	GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget* entry   = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), preset_name);
	gtk_container_add(GTK_CONTAINER(content), entry);
	gtk_widget_show(entry);

	const gint  response      = gtk_dialog_run(GTK_DIALOG(dialog));
	const char* new_name_cstr = (response == GTK_RESPONSE_OK)
										? gtk_entry_get_text(GTK_ENTRY(entry))
										: nullptr;
	string      new_name      = new_name_cstr ? new_name_cstr : "";
	gtk_widget_destroy(dialog);

	if (new_name.empty() || new_name == preset_name) {
		return;
	}

	Shape_preset* clone = presets_file->create(new_name.c_str());
	for (const auto& [key, value] : preset->get_data()) {
		clone->set_value(key, value);
	}

	setup_presets_list();
}

/*
 *  Rename the selected preset.
 */
void ExultStudio::rename_preset() {
	const char* preset_name = get_selected_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to rename");
		return;
	}

	// Create a simple dialog to get new name
	GtkWidget* dialog = gtk_message_dialog_new(
			GTK_WINDOW(get_widget("shape_window")), GTK_DIALOG_MODAL,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
			"Enter new name for preset:");
	GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget* entry   = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), preset_name);
	gtk_container_add(GTK_CONTAINER(content), entry);
	gtk_widget_show(entry);

	const gint  response      = gtk_dialog_run(GTK_DIALOG(dialog));
	const char* new_name_cstr = (response == GTK_RESPONSE_OK)
										? gtk_entry_get_text(GTK_ENTRY(entry))
										: nullptr;
	string      new_name      = new_name_cstr ? new_name_cstr : "";
	gtk_widget_destroy(dialog);

	if (new_name.empty() || new_name == preset_name) {
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (preset) {
		preset->set_name(new_name.c_str());
		preset->set_modified();
		setup_presets_list();
	}
}

/*
 *  Apply shape properties from a specified shape number.
 */
void ExultStudio::apply_preset_from_shape() {
	// Check if shape window is open
	if (!shapewin || !gtk_widget_get_visible(shapewin)) {
		EStudio::Alert("Please open the shape info window first");
		return;
	}

	// Get the source shape number from the spin button
	GtkWidget* spin_widget = get_widget("preset_apply_shape_spin");
	const int  source_shape
			= gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_widget));
	const int current_shape = get_num_entry("shinfo_shape");

	// Silently fail if source shape == current shape
	if (source_shape == current_shape) {
		return;
	}

	// Ask user for confirmation before applying
	char msg[128];
	snprintf(
			msg, sizeof(msg),
			"Apply shape #%d properties to the current shape #%d?",
			source_shape, current_shape);
	if (EStudio::Prompt(msg, "Yes", "No") != 0) {
		return;
	}

	// Get the shape info for the current shape
	auto* info = static_cast<Shape_info*>(
			g_object_get_data(G_OBJECT(shapewin), "user_data"));
	if (!info) {
		return;
	}

	// Get the Shapes_vga_file to access shape info by shape number
	auto* svga = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
	if (!svga) {
		EStudio::Alert("Cannot access shape information");
		return;
	}

	// Get the shape info for the source shape
	Shape_info& source_info = svga->get_info(source_shape);

	// Save current state for undo before applying preset
	ExultStudio* studio = ExultStudio::get_instance();
	// Delete old backup if it exists to prevent memory leak
	if (studio->temp_shape_info) {
		delete studio->temp_shape_info;
		studio->temp_shape_info = nullptr;
	}
	studio->temp_shape_info = new Shape_info(*info);

	// Copy all shape data from source shape to current shape
	*info = source_info;

	// Mark as modified
	shape_info_modified = true;
	studio->set_shape_window_dirty(true);

	// Re-initialize the shape notebook with the copied data
	const int frnum = get_num_entry("shinfo_frame");
	if (current_shape >= 0 && frnum >= 0) {
		init_shape_notebook(
				*info, get_widget("shinfo_notebook"), current_shape, frnum);
	}
}

/*
 *  Delete the selected preset.
 */
void ExultStudio::del_preset() {
	const char* preset_name = get_selected_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to delete");
		return;
	}

	string msg = "Are you sure you want to delete preset '";
	msg += preset_name;
	msg += "'?";

	if (EStudio::Prompt(msg.c_str(), "Yes", "No") != 0) {
		return;
	}

	Shape_preset* preset = presets_file->find(preset_name);
	if (preset) {
		presets_file->remove(preset);
		setup_presets_list();
	}
}

/*
 *  Signal handlers
 */

C_EXPORT void on_preset_new_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	const char* name = studio->get_text_entry("preset_name_entry");
	if (!name || !*name) {
		EStudio::Alert("Please enter a preset name");
		return;
	}

	studio->save_shape_to_preset(name);
	studio->set_entry("preset_name_entry", "");
}

C_EXPORT void on_preset_export_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->export_preset();
}

C_EXPORT void on_preset_apply_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->apply_preset();
}

C_EXPORT void on_preset_import_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->import_presets();
}

C_EXPORT void on_preset_clone_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->clone_preset();
}

C_EXPORT void on_preset_rename_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rename_preset();
}

C_EXPORT void on_preset_delete_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->del_preset();
}

C_EXPORT void on_presets_list_row_activated(
		GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col,
		gpointer user_data) {
	ignore_unused_variable_warning(treeview, path, col, user_data);
	ExultStudio* studio      = ExultStudio::get_instance();
	const char*  preset_name = get_selected_preset_name(studio);

	if (preset_name) {
		studio->load_preset_to_shape(preset_name);
	}
}

C_EXPORT void on_presets_list_cursor_changed(
		GtkTreeView* treeview, gpointer user_data) {
	ignore_unused_variable_warning(treeview, user_data);
	// Could enable/disable buttons based on selection here
}

C_EXPORT void on_preset_apply_from_shape_clicked(
		GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->apply_preset_from_shape();
}
