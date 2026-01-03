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

#include "npcpresets.h"

#include "databuf.h"
#include "exceptions.h"
#include "shapefile.h"
#include "studio.h"
#include "utils.h"

#include <cstring>

using std::string;
using std::vector;

/*
 *  Create preset.
 */
Npc_preset::Npc_preset(const char* nm, Npc_preset_file* f)
		: name(nm), file(f), modified(false) {}

/*
 *  Set a value in the preset.
 */
void Npc_preset::set_value(const string& key, const string& value) {
	data[key] = value;
	modified  = true;
	if (file) {
		file->modified = true;
	}
}

/*
 *  Get a value from the preset.
 */
string Npc_preset::get_value(const string& key) const {
	auto it = data.find(key);
	return it != data.end() ? it->second : string();
}

/*
 *  Check if preset has a key.
 */
bool Npc_preset::has_value(const string& key) const {
	return data.find(key) != data.end();
}

/*
 *  Write out preset.
 */
void Npc_preset::write(ODataSource& out) {
	out.write2(name.size());
	out.write(name.c_str(), name.size());

	// Write all field values
	// Face and shape (-1 if not set)
	const int face_num
			= has_value("face") ? std::atoi(get_value("face").c_str()) : -1;
	const int shape_num
			= has_value("shape") ? std::atoi(get_value("shape").c_str()) : -1;
	out.write4(face_num);
	out.write4(shape_num);

	// Properties (10 values)
	for (int i = 0; i < 10; i++) {
		const string key = "prop_" + std::to_string(i);
		const int val = has_value(key) ? std::atoi(get_value(key).c_str()) : 0;
		out.write4(val);
	}

	// Alignment and attack mode
	const int alignment = has_value("alignment")
								  ? std::atoi(get_value("alignment").c_str())
								  : 0;
	const int attack_mode
			= has_value("attack_mode")
					  ? std::atoi(get_value("attack_mode").c_str())
					  : 0;
	out.write2(alignment);
	out.write2(attack_mode);

	// Flags (3 flag groups: oflags, xflags, type_flags)
	const unsigned long oflags
			= has_value("oflags") ? std::stoul(get_value("oflags")) : 0;
	const unsigned long xflags
			= has_value("xflags") ? std::stoul(get_value("xflags")) : 0;
	const unsigned long type_flags
			= has_value("type_flags") ? std::stoul(get_value("type_flags")) : 0;
	out.write4(oflags);
	out.write4(xflags);
	out.write4(type_flags);

	// Schedules (8 time slots, activity type only)
	for (int i = 0; i < 8; i++) {
		const string key = "sched_" + std::to_string(i);
		const int    type
				= has_value(key) ? std::atoi(get_value(key).c_str()) : -1;
		out.write2(type);
	}
}

/*
 *  Read in preset.
 */
void Npc_preset::read(IDataSource& in) {
	const int name_len = in.read2();
	char*     nm       = new char[name_len + 1];
	in.read(nm, name_len);
	nm[name_len] = 0;
	name         = nm;
	delete[] nm;

	// Read all field values
	const int face_num  = in.read4();
	const int shape_num = in.read4();
	if (face_num >= 0) {
		data["face"] = std::to_string(face_num);
	}
	if (shape_num >= 0) {
		data["shape"] = std::to_string(shape_num);
	}

	// Properties (10 values)
	for (int i = 0; i < 10; i++) {
		const int val                     = in.read4();
		data["prop_" + std::to_string(i)] = std::to_string(val);
	}

	// Alignment and attack mode
	const int alignment   = in.read2();
	const int attack_mode = in.read2();
	data["alignment"]     = std::to_string(alignment);
	data["attack_mode"]   = std::to_string(attack_mode);

	// Flags
	const unsigned long oflags     = in.read4();
	const unsigned long xflags     = in.read4();
	const unsigned long type_flags = in.read4();
	data["oflags"]                 = std::to_string(oflags);
	data["xflags"]                 = std::to_string(xflags);
	data["type_flags"]             = std::to_string(type_flags);

	// Schedules (8 time slots)
	for (int i = 0; i < 8; i++) {
		const int type = static_cast<int16_t>(in.read2());
		if (type >= 0) {
			data["sched_" + std::to_string(i)] = std::to_string(type);
		}
	}

	modified = false;
}

/*
 *  Create preset file.
 */
Npc_preset_file::Npc_preset_file(const char* nm)
		: filename(nm ? nm : ""), modified(false) {}

/*
 *  Delete.
 */
Npc_preset_file::~Npc_preset_file() {
	for (auto* preset : presets) {
		delete preset;
	}
}

/*
 *  Find by name.
 */
Npc_preset* Npc_preset_file::find(const char* nm) {
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
int Npc_preset_file::find_index(const char* nm) {
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
Npc_preset* Npc_preset_file::create(const char* nm) {
	auto* preset = new Npc_preset(nm, this);
	presets.push_back(preset);
	modified = true;
	return preset;
}

/*
 *  Remove a preset.
 */
void Npc_preset_file::remove(Npc_preset* preset) {
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
bool Npc_preset_file::write() {
	try {
		OFileDataSource out(filename.c_str());
		// Write header
		const char* header = "ExultStudio NPC presets";
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
bool Npc_preset_file::read(const char* nm) {
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
		if (std::strcmp(header, "ExultStudio NPC presets") != 0) {
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
			auto* preset = new Npc_preset("", this);
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
Npc_preset_file* Npc_preset_file::read_file(const char* pathname) {
	auto* pfile = new Npc_preset_file();
	if (!pfile->read(pathname)) {
		delete pfile;
		return nullptr;
	}
	return pfile;
}

/*
 *  Setup the NPC presets list from the current preset file.
 */
void ExultStudio::setup_npc_presets_list() {
	if (!npc_presets_file) {
		return;
	}

	GtkTreeView*  list  = GTK_TREE_VIEW(get_widget("npc_presets_list"));
	GtkListStore* store = GTK_LIST_STORE(gtk_tree_view_get_model(list));
	gtk_list_store_clear(store);

	const int cnt = npc_presets_file->size();
	for (int i = 0; i < cnt; i++) {
		Npc_preset* preset = npc_presets_file->get(i);
		if (!preset) {
			continue;
		}

		GdkPixbuf* face_pixbuf  = nullptr;
		GdkPixbuf* shape_pixbuf = nullptr;

		// Get face image if face number is set
		if (preset->has_value("face")) {
			const int face_num = std::atoi(preset->get_value("face").c_str());
			if (face_num >= 0 && facefile) {
				GdkPixbuf* full_pixbuf
						= shape_image(facefile->get_ifile(), face_num, 0, true);
				if (full_pixbuf) {
					const int width  = gdk_pixbuf_get_width(full_pixbuf);
					const int height = gdk_pixbuf_get_height(full_pixbuf);
					face_pixbuf      = gdk_pixbuf_scale_simple(
                            full_pixbuf, width * 0.3, height * 0.3,
                            GDK_INTERP_BILINEAR);
					g_object_unref(full_pixbuf);
				}
			}
		}

		// Get shape image if shape number is set (skip shape 0)
		if (preset->has_value("shape")) {
			const int shape_num = std::atoi(preset->get_value("shape").c_str());
			if (shape_num > 0 && vgafile) {
				GdkPixbuf* full_pixbuf
						= shape_image(vgafile->get_ifile(), shape_num, 0, true);
				if (full_pixbuf) {
					const int width  = gdk_pixbuf_get_width(full_pixbuf);
					const int height = gdk_pixbuf_get_height(full_pixbuf);
					shape_pixbuf     = gdk_pixbuf_scale_simple(
                            full_pixbuf, width * 0.4, height * 0.4,
                            GDK_INTERP_BILINEAR);
					g_object_unref(full_pixbuf);
				}
			}
		}

		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(
				store, &iter, 0, preset->get_name(), 1, face_pixbuf, 2,
				shape_pixbuf, -1);

		// Free pixbufs after adding to store (store takes a reference)
		if (face_pixbuf) {
			g_object_unref(face_pixbuf);
		}
		if (shape_pixbuf) {
			g_object_unref(shape_pixbuf);
		}
	}
}

/*
 *  Save NPC presets to file.
 */
void ExultStudio::save_npc_presets() {
	if (npc_presets_file && npc_presets_file->modified) {
		npc_presets_file->write();
	}
}

/*
 *  Check if NPC presets have been modified.
 */
bool ExultStudio::npc_presets_modified() {
	return npc_presets_file && npc_presets_file->modified;
}

/*
 *  Get the currently selected NPC preset name from the list.
 */
static const char* get_selected_npc_preset_name(ExultStudio* studio) {
	GtkTreeView* list = GTK_TREE_VIEW(studio->get_widget("npc_presets_list"));
	GtkTreeSelection* sel = gtk_tree_view_get_selection(list);
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
 *  Save current NPC to a preset.
 */
void ExultStudio::save_npc_to_preset(const char* preset_name) {
	if (!npc_presets_file || !preset_name || !*preset_name) {
		return;
	}

	// Check if NPC window is open
	if (!npcwin || !gtk_widget_get_visible(npcwin)) {
		EStudio::Alert("Please open the NPC info window first");
		return;
	}

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (!preset) {
		preset = npc_presets_file->create(preset_name);
	}

	preset->clear_data();

	// Read all NPC field values from the UI

	// Read face and shape
	const int face  = get_spin("npc_face");
	const int shape = get_spin("npc_shape");
	preset->set_value("face", std::to_string(face));
	preset->set_value("shape", std::to_string(shape));

	// Read properties (0-9)
	for (int i = 0; i < 10; i++) {
		char propname[20];
		snprintf(propname, sizeof(propname), "npc_prop_%02d", i);
		const int value = get_spin(propname);
		char      key[20];
		snprintf(key, sizeof(key), "prop_%d", i);
		preset->set_value(key, std::to_string(value));
	}

	// Read alignment and attack mode
	const int alignment   = get_optmenu("npc_alignment");
	const int attack_mode = get_optmenu("npc_attack_mode");
	preset->set_value("alignment", std::to_string(alignment));
	preset->set_value("attack_mode", std::to_string(attack_mode));

	// Read flags
	unsigned long oflags     = 0;
	unsigned long xflags     = 0;
	unsigned long type_flags = 0;

	GtkContainer* flags_table = GTK_CONTAINER(get_widget("npc_flags_table"));
	if (flags_table) {
		GList* children = gtk_container_get_children(flags_table);
		for (GList* list = children; list != nullptr;
			 list        = g_list_next(list)) {
			GtkCheckButton* cbox;
			unsigned long*  bits;
			int             fnum;
			if (Get_flag_cbox(
						list, &oflags, &xflags, &type_flags, cbox, bits,
						fnum)) {
				if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cbox))) {
					*bits |= (1 << fnum);
				}
			}
		}
		g_list_free(children);
	}

	preset->set_value("oflags", std::to_string(oflags));
	preset->set_value("xflags", std::to_string(xflags));
	preset->set_value("type_flags", std::to_string(type_flags));

	// Read schedules (only the activity type for each time slot 0-7)
	for (int time = 0; time < 8; time++) {
		char*     lname = g_strdup_printf("npc_sched%d", time);
		GtkLabel* label = GTK_LABEL(get_widget(lname));
		g_free(lname);

		int sched_type = -1;
		if (label) {
			sched_type = reinterpret_cast<sintptr>(
					g_object_get_data(G_OBJECT(label), "user_data"));
		}

		char key[20];
		snprintf(key, sizeof(key), "sched_%d", time);
		preset->set_value(key, std::to_string(sched_type));
	}

	setup_npc_presets_list();
}

/*
 *  Load a preset to the current NPC.
 */
void ExultStudio::load_npc_preset_to_npc(const char* preset_name) {
	if (!npc_presets_file || !preset_name) {
		return;
	}

	// Check if NPC window is open
	if (!npcwin || !gtk_widget_get_visible(npcwin)) {
		EStudio::Alert("Please open the NPC info window first");
		return;
	}

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Load all NPC field values to the UI

	// Set face and shape
	if (preset->has_value("face")) {
		const int face = std::atoi(preset->get_value("face").c_str());
		if (face >= 0) {
			set_spin("npc_face", face, false);
		}
	}
	if (preset->has_value("shape")) {
		const int shape = std::atoi(preset->get_value("shape").c_str());
		if (shape >= 0) {
			set_spin("npc_shape", shape, false);
		}
	}

	// Set properties (0-9)
	for (int i = 0; i < 10; i++) {
		char key[20];
		snprintf(key, sizeof(key), "prop_%d", i);
		if (preset->has_value(key)) {
			const int value = std::atoi(preset->get_value(key).c_str());
			char      propname[20];
			snprintf(propname, sizeof(propname), "npc_prop_%02d", i);
			set_spin(propname, value, false);
		}
	}

	// Set alignment and attack mode
	if (preset->has_value("alignment")) {
		const int alignment = std::atoi(preset->get_value("alignment").c_str());
		set_optmenu("npc_alignment", alignment, false);
	}
	if (preset->has_value("attack_mode")) {
		const int attack_mode
				= std::atoi(preset->get_value("attack_mode").c_str());
		set_optmenu("npc_attack_mode", attack_mode, false);
	}

	// Set flags
	unsigned long oflags     = 0;
	unsigned long xflags     = 0;
	unsigned long type_flags = 0;

	if (preset->has_value("oflags")) {
		oflags = std::strtoul(preset->get_value("oflags").c_str(), nullptr, 10);
	}
	if (preset->has_value("xflags")) {
		xflags = std::strtoul(preset->get_value("xflags").c_str(), nullptr, 10);
	}
	if (preset->has_value("type_flags")) {
		type_flags = std::strtoul(
				preset->get_value("type_flags").c_str(), nullptr, 10);
	}

	GtkContainer* flags_table = GTK_CONTAINER(get_widget("npc_flags_table"));
	if (flags_table) {
		GList* children = gtk_container_get_children(flags_table);
		for (GList* list = children; list != nullptr;
			 list        = g_list_next(list)) {
			GtkCheckButton* cbox;
			unsigned long*  bits;
			int             fnum;
			if (Get_flag_cbox(
						list, &oflags, &xflags, &type_flags, cbox, bits,
						fnum)) {
				const bool active = (*bits & (1 << fnum)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbox), active);
			}
		}
		g_list_free(children);
	}

	// Set schedules (only the activity type for each time slot 0-7)
	for (int time = 0; time < 8; time++) {
		char key[20];
		snprintf(key, sizeof(key), "sched_%d", time);

		int sched_type = -1;
		if (preset->has_value(key)) {
			sched_type = std::atoi(preset->get_value(key).c_str());
		}

		char*     lname = g_strdup_printf("npc_sched%d", time);
		GtkLabel* label = GTK_LABEL(get_widget(lname));
		g_free(lname);

		if (label) {
			// Set user data
			g_object_set_data(
					G_OBJECT(label), "user_data",
					reinterpret_cast<gpointer>(uintptr(sched_type)));
			// Update label text
			gtk_label_set_text(
					label, sched_type >= 0 && sched_type < 32
								   ? sched_names[sched_type]
								   : "-----");
		}
	}
}

/*
 *  Apply a preset's NPC settings to the current NPC.
 */
void ExultStudio::apply_npc_preset() {
	const char* preset_name = get_selected_npc_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to apply");
		return;
	}

	// Check if NPC window is open
	if (!npcwin || !gtk_widget_get_visible(npcwin)) {
		EStudio::Alert("Please open the NPC info window first");
		return;
	}

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Ask user for confirmation before applying
	if (EStudio::Prompt("Apply this preset to the current NPC?", "Yes", "No")
		!= 0) {
		return;
	}

	// Load the preset values to the NPC window
	load_npc_preset_to_npc(preset_name);

	// Mark as modified and dirty
	set_npc_modified();
	set_npc_window_dirty(true);
}

/*
 *  Export an NPC preset to a file.
 */
void ExultStudio::export_npc_preset() {
	const char* preset_name = get_selected_npc_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to export");
		return;
	}

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Create file chooser dialog for saving
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
			"Export NPC Preset to File", GTK_WINDOW(get_widget("npc_window")),
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
	gtk_file_filter_set_name(filter, "NPC Preset files (*.pre)");
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
	Npc_preset_file temp_file(filename);
	Npc_preset*     copy = temp_file.create(preset->get_name());

	// Copy all data
	for (const auto& [key, value] : preset->get_data()) {
		copy->set_value(key, value);
	}

	if (temp_file.write()) {
		EStudio::Alert("NPC preset exported successfully");
	} else {
		EStudio::Alert("Failed to export NPC preset");
	}
	g_free(filename);
}

/*
 *  Import NPC presets from a file.
 */
void ExultStudio::import_npc_presets() {
	// Create file chooser dialog for opening
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
			"Import NPC Presets File", GTK_WINDOW(get_widget("npc_window")),
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
	gtk_file_filter_set_name(filter, "NPC Preset files (*.pre)");
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

	Npc_preset_file* import_file = Npc_preset_file::read_file(filename);
	if (!import_file) {
		EStudio::Alert("This is not an NPC preset. Import aborted.");
		g_free(filename);
		return;
	}
	g_free(filename);

	const int count = import_file->size();
	for (int i = 0; i < count; i++) {
		Npc_preset* src = import_file->get(i);
		if (src) {
			// Append " imported" to the preset name
			std::string imported_name
					= std::string(src->get_name()) + " imported";

			// Check if preset already exists
			Npc_preset* dest = npc_presets_file->find(imported_name.c_str());
			if (!dest) {
				dest = npc_presets_file->create(imported_name.c_str());
			}

			dest->clear_data();
			for (const auto& [key, value] : src->get_data()) {
				dest->set_value(key, value);
			}
		}
	}

	delete import_file;
	setup_npc_presets_list();
	EStudio::Alert("NPC presets imported successfully");
}

/*
 *  Clone the selected NPC preset.
 */
void ExultStudio::clone_npc_preset() {
	const char* preset_name = get_selected_npc_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to clone");
		return;
	}

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (!preset) {
		return;
	}

	// Create a simple dialog to get new name
	GtkWidget* dialog = gtk_message_dialog_new(
			GTK_WINDOW(get_widget("npc_window")), GTK_DIALOG_MODAL,
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

	Npc_preset* clone = npc_presets_file->create(new_name.c_str());
	for (const auto& [key, value] : preset->get_data()) {
		clone->set_value(key, value);
	}

	setup_npc_presets_list();
}

/*
 *  Rename the selected NPC preset.
 */
void ExultStudio::rename_npc_preset() {
	const char* preset_name = get_selected_npc_preset_name(this);
	if (!preset_name) {
		EStudio::Alert("Please select a preset to rename");
		return;
	}

	// Create a simple dialog to get new name
	GtkWidget* dialog = gtk_message_dialog_new(
			GTK_WINDOW(get_widget("npc_window")), GTK_DIALOG_MODAL,
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

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (preset) {
		preset->set_name(new_name.c_str());
		preset->set_modified();
		setup_npc_presets_list();
	}
}

/*
 *  Delete the selected NPC preset.
 */
void ExultStudio::del_npc_preset() {
	const char* preset_name = get_selected_npc_preset_name(this);
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

	Npc_preset* preset = npc_presets_file->find(preset_name);
	if (preset) {
		npc_presets_file->remove(preset);
		setup_npc_presets_list();
	}
}

/*
 *  Signal handlers
 */

C_EXPORT void on_npc_preset_new_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	const char* name = studio->get_text_entry("npc_preset_name_entry");
	if (!name || !*name) {
		EStudio::Alert("Please enter a preset name");
		return;
	}

	studio->save_npc_to_preset(name);
	studio->set_entry("npc_preset_name_entry", "");
}

C_EXPORT void on_npc_preset_export_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->export_npc_preset();
}

C_EXPORT void on_npc_preset_apply_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->apply_npc_preset();
}

C_EXPORT void on_npc_preset_import_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->import_npc_presets();
}

C_EXPORT void on_npc_preset_clone_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->clone_npc_preset();
}

C_EXPORT void on_npc_preset_rename_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->rename_npc_preset();
}

C_EXPORT void on_npc_preset_delete_clicked(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->del_npc_preset();
}

C_EXPORT void on_npc_presets_list_row_activated(
		GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col,
		gpointer user_data) {
	ignore_unused_variable_warning(treeview, path, col, user_data);
	// Double-clicking does nothing
}

C_EXPORT void on_npc_presets_list_cursor_changed(
		GtkTreeView* treeview, gpointer user_data) {
	ignore_unused_variable_warning(treeview, user_data);
	// Could enable/disable buttons based on selection here
}
