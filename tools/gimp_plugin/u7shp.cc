/*
 * SHP loading file filter for The GIMP version 3.x.
 *
 * (C) 2000-2001 Tristan Tarrant
 * (C) 2001-2004 Willem Jan Palenstijn
 * (C) 2010-2025 The Exult Team
 *
 * You can find the most recent version of this file in the Exult sources,
 * available from https://exult.info/
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

/*
 * GIMP Side of the Shape load / export plugin
 */

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wvariadic-macros"
#	pragma GCC diagnostic ignored "-Wignored-qualifiers"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wpedantic"
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	else
#		pragma GCC diagnostic ignored "-Wc99-extensions"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#		if __clang_major__ >= 16
#			pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#		endif
#	endif
#endif    // __GNUC__
#ifdef USE_STRICT_GTK
#	define GTK_DISABLE_SINGLE_INCLUDES
#	define GSEAL_ENABLE
#	define GTK_DISABLE_DEPRECATED
#	define GDK_DISABLE_DEPRECATED
#else
#	define GDK_DISABLE_DEPRECATION_WARNINGS
#	define GLIB_DISABLE_DEPRECATION_WARNINGS
#endif    // USE_STRICT_GTK
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "U7obj.h"
#include "databuf.h"
#include "gtk_redefines.h"
#include "ibuf8.h"
#include "ignore_unused_variable_warning.h"
#include "vgafile.h"

#include <cerrno>
#include <iostream>
#include <string>
#include <vector>

/*
 * GIMP Side of the Shape load / export plugin
 *   Borrowed from plug-ins/common/file-cel.c
 *      -- KISS CEL file format plug-in
 *      -- (copyright) 1997,1998 Nick Lamb (njl195@zepler.org.uk)
 */

#define LOAD_PROC      "file-shp-load"
#define EXPORT_PROC    "file-shp-export"
#define PLUG_IN_BINARY "file-shp"
#define PLUG_IN_ROLE   "gimp-file-shp"

using Shp      = struct _Shp;
using ShpClass = struct _ShpClass;

struct _Shp {
	GimpPlugIn parent_instance;
};

struct _ShpClass {
	GimpPlugInClass parent_class;
};

#define SHP_TYPE (shp_get_type())
#define SHP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SHP_TYPE, Shp))

GType shp_get_type(void) G_GNUC_CONST;

static GList*         shp_query_procedures(GimpPlugIn* plug_in);
static GimpProcedure* shp_create_procedure(
		GimpPlugIn* plug_in, const gchar* name);

static GimpValueArray* shp_load(
		GimpProcedure* procedure, GimpRunMode run_mode, GFile* file,
		GimpMetadata* metadata, GimpMetadataLoadFlags* flags,
		GimpProcedureConfig* config, gpointer run_data);
static GimpValueArray* shp_export(
		GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
		GFile* file, GimpExportOptions* options, GimpMetadata* metadata,
		GimpProcedureConfig* config, gpointer run_data);
static gboolean shp_palette_dialog(
		const gchar* title, GimpProcedure* procedure,
		GimpProcedureConfig* config);

static gint       load_palette(GFile* file, guchar palette[], GError** error);
static GimpImage* load_image(
		GFile* file, GFile* palette_file, GimpRunMode run_mode, GError** error);
static gboolean export_image(
		GFile* file, GimpImage* image, GimpRunMode run_mode, GError** error);

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	if defined(__llvm__) || defined(__clang__)
#		pragma GCC diagnostic ignored "-Wunused-parameter"
#		if __clang_major__ >= 16
#			pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#		endif
#	endif
#endif    // __GNUC__
G_DEFINE_TYPE(Shp, shp, GIMP_TYPE_PLUG_IN)
GIMP_MAIN(SHP_TYPE)
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

static void shp_class_init(ShpClass* klass) {
	GimpPlugInClass* plug_in_class  = GIMP_PLUG_IN_CLASS(klass);
	plug_in_class->query_procedures = shp_query_procedures;
	plug_in_class->create_procedure = shp_create_procedure;
	plug_in_class->set_i18n         = nullptr;
}

static void shp_init(Shp* shp) {
	ignore_unused_variable_warning(shp);
}

static GList* shp_query_procedures(GimpPlugIn* plug_in) {
	ignore_unused_variable_warning(plug_in);
	GList* list = nullptr;
	list        = g_list_append(list, g_strdup(LOAD_PROC));
	list        = g_list_append(list, g_strdup(EXPORT_PROC));
	return list;
}

static GimpProcedure* shp_create_procedure(
		GimpPlugIn* plug_in, const gchar* name) {
	GimpProcedure* procedure = nullptr;
	if (!strcmp(name, LOAD_PROC)) {
		procedure = gimp_load_procedure_new(
				plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, shp_load, nullptr,
				nullptr);
		gimp_procedure_set_menu_label(procedure, "Ultima VII Shape");
		gimp_procedure_set_documentation(
				procedure, "Loads files in Ultima VII Shape file format",
				"This plug-in loads individual Ultima VII "
				"Shape files.",
				name);
		gimp_procedure_set_attribution(
				procedure, "The Exult Team", "The Exult Team", "2010-2025");
		gimp_file_procedure_set_extensions(
				GIMP_FILE_PROCEDURE(procedure), "shp");
		gimp_procedure_add_file_argument(
				procedure, "palette-file", "_Palette file",
				"PAL file to load palette from", GIMP_FILE_CHOOSER_ACTION_OPEN,
				TRUE, nullptr, G_PARAM_READWRITE);
	} else if (!strcmp(name, EXPORT_PROC)) {
		procedure = gimp_export_procedure_new(
				plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, FALSE, shp_export,
				nullptr, nullptr);
		gimp_procedure_set_image_types(procedure, "RGB*, INDEXED*");
		gimp_procedure_set_menu_label(procedure, "Ultima VII Shape");
		gimp_procedure_set_documentation(
				procedure, "Exports files in Ultima VII Shape file format",
				"This plug-in exports individual Ultima VII "
				"Shape files.",
				name);
		gimp_procedure_set_attribution(
				procedure, "The Exult Team", "The Exult Team", "2010-2025");
		gimp_file_procedure_set_handles_remote(
				GIMP_FILE_PROCEDURE(procedure), TRUE);
		gimp_file_procedure_set_extensions(
				GIMP_FILE_PROCEDURE(procedure), "shp");
		gimp_export_procedure_set_capabilities(
				GIMP_EXPORT_PROCEDURE(procedure),
				static_cast<GimpExportCapabilities>(
						GIMP_EXPORT_CAN_HANDLE_RGB
						| GIMP_EXPORT_CAN_HANDLE_ALPHA
						| GIMP_EXPORT_CAN_HANDLE_LAYERS
						| GIMP_EXPORT_CAN_HANDLE_INDEXED),
				nullptr, nullptr, nullptr);
		gimp_procedure_add_file_argument(
				procedure, "palette-file", "_Palette file",
				"PAL file to save palette to", GIMP_FILE_CHOOSER_ACTION_OPEN,
				TRUE, nullptr, G_PARAM_READWRITE);
	}
	return procedure;
}

static GimpValueArray* shp_load(
		GimpProcedure* procedure, GimpRunMode run_mode, GFile* file,
		GimpMetadata* metadata, GimpMetadataLoadFlags* flags,
		GimpProcedureConfig* config, gpointer run_data) {
	ignore_unused_variable_warning(
			procedure, run_mode, file, metadata, flags, config, run_data);
	GimpValueArray* return_vals;
	GimpImage*      image        = nullptr;
	GFile*          palette_file = nullptr;
	GError*         error        = nullptr;

	gegl_init(nullptr, nullptr);

	if (error != nullptr) {
		return gimp_procedure_new_return_values(
				procedure, GIMP_PDB_EXECUTION_ERROR, error);
	}

	g_object_get(config, "palette-file", &palette_file, nullptr);
	if (run_mode != GIMP_RUN_NONINTERACTIVE) {
		if (!shp_palette_dialog("Load PAL Palette", procedure, config)) {
			return gimp_procedure_new_return_values(
					procedure, GIMP_PDB_CANCEL, nullptr);
		}
		g_clear_object(&palette_file);
		g_object_get(config, "palette-file", &palette_file, nullptr);
	}

	image = load_image(file, palette_file, run_mode, &error);
	g_clear_object(&palette_file);
	if (!image) {
		return gimp_procedure_new_return_values(
				procedure, GIMP_PDB_EXECUTION_ERROR, error);
	}
	return_vals = gimp_procedure_new_return_values(
			procedure, GIMP_PDB_SUCCESS, nullptr);
	GIMP_VALUES_SET_IMAGE(return_vals, 1, image);
	return return_vals;
}

static GimpValueArray* shp_export(
		GimpProcedure* procedure, GimpRunMode run_mode, GimpImage* image,
		GFile* file, GimpExportOptions* options, GimpMetadata* metadata,
		GimpProcedureConfig* config, gpointer run_data) {
	ignore_unused_variable_warning(
			procedure, run_mode, image, file, options, metadata, config,
			run_data);
	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	GimpExportReturn  expret = GIMP_EXPORT_IGNORE;
	GError*           error  = nullptr;

	gegl_init(nullptr, nullptr);

	expret = gimp_export_options_get_image(options, &image);

	if (!export_image(file, image, run_mode, &error)) {
		status = GIMP_PDB_EXECUTION_ERROR;
	}

	if (expret == GIMP_EXPORT_EXPORT) {
		gimp_image_delete(image);
	}

	return gimp_procedure_new_return_values(procedure, status, error);
}

static gboolean shp_palette_dialog(
		const gchar* title, GimpProcedure* procedure,
		GimpProcedureConfig* config) {
	GtkWidget* dialog;
	gboolean   run;

	gimp_ui_init(PLUG_IN_BINARY);
	dialog = gimp_procedure_dialog_new(
			GIMP_PROCEDURE(procedure), GIMP_PROCEDURE_CONFIG(config), title);
	gimp_procedure_dialog_set_ok_label(GIMP_PROCEDURE_DIALOG(dialog), "_Open");
	gimp_procedure_dialog_fill(GIMP_PROCEDURE_DIALOG(dialog), nullptr);
	run = gimp_procedure_dialog_run(GIMP_PROCEDURE_DIALOG(dialog));

	gtk_widget_destroy(dialog);
	return run;
}

/*
 * Exult Side of the Shape load / export plugin
 */

static guchar gimp_cmap[768] = {
		0x00, 0x00, 0x00, 0xF8, 0xF0, 0xCC, 0xF4, 0xE4, 0xA4, 0xF0, 0xDC, 0x78,
		0xEC, 0xD0, 0x50, 0xEC, 0xC8, 0x28, 0xD8, 0xAC, 0x20, 0xC4, 0x94, 0x18,
		0xB0, 0x80, 0x10, 0x9C, 0x68, 0x0C, 0x88, 0x54, 0x08, 0x74, 0x44, 0x04,
		0x60, 0x30, 0x00, 0x4C, 0x24, 0x00, 0x38, 0x14, 0x00, 0xF8, 0xFC, 0xFC,
		0xFC, 0xD8, 0xD8, 0xFC, 0xB8, 0xB8, 0xFC, 0x98, 0x9C, 0xFC, 0x78, 0x80,
		0xFC, 0x58, 0x64, 0xFC, 0x38, 0x4C, 0xFC, 0x1C, 0x34, 0xDC, 0x14, 0x28,
		0xC0, 0x0C, 0x1C, 0xA4, 0x08, 0x14, 0x88, 0x04, 0x0C, 0x6C, 0x00, 0x04,
		0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18, 0x00, 0x00, 0xFC, 0xEC, 0xD8,
		0xFC, 0xDC, 0xB8, 0xFC, 0xCC, 0x98, 0xFC, 0xBC, 0x7C, 0xFC, 0xAC, 0x5C,
		0xFC, 0x9C, 0x3C, 0xFC, 0x8C, 0x1C, 0xFC, 0x7C, 0x00, 0xE0, 0x6C, 0x00,
		0xC0, 0x60, 0x00, 0xA4, 0x50, 0x00, 0x88, 0x44, 0x00, 0x6C, 0x34, 0x00,
		0x50, 0x24, 0x00, 0x34, 0x18, 0x00, 0x18, 0x08, 0x00, 0xFC, 0xFC, 0xD8,
		0xF4, 0xF4, 0x9C, 0xEC, 0xEC, 0x60, 0xE4, 0xE4, 0x2C, 0xDC, 0xDC, 0x00,
		0xC0, 0xC0, 0x00, 0xA4, 0xA4, 0x00, 0x88, 0x88, 0x00, 0x6C, 0x6C, 0x00,
		0x50, 0x50, 0x00, 0x34, 0x34, 0x00, 0x18, 0x18, 0x00, 0xD8, 0xFC, 0xD8,
		0xB0, 0xFC, 0xAC, 0x8C, 0xFC, 0x80, 0x6C, 0xFC, 0x54, 0x50, 0xFC, 0x28,
		0x38, 0xFC, 0x00, 0x28, 0xDC, 0x00, 0x1C, 0xC0, 0x00, 0x14, 0xA4, 0x00,
		0x0C, 0x88, 0x00, 0x04, 0x6C, 0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00,
		0x00, 0x18, 0x00, 0xD4, 0xD8, 0xFC, 0xB8, 0xB8, 0xFC, 0x98, 0x98, 0xFC,
		0x7C, 0x7C, 0xFC, 0x5C, 0x5C, 0xFC, 0x3C, 0x3C, 0xFC, 0x00, 0x00, 0xFC,
		0x00, 0x00, 0xE0, 0x00, 0x00, 0xC0, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x88,
		0x00, 0x00, 0x6C, 0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18,
		0xE8, 0xC8, 0xE8, 0xD4, 0x98, 0xD4, 0xC4, 0x6C, 0xC4, 0xB0, 0x48, 0xB0,
		0xA0, 0x28, 0xA0, 0x8C, 0x10, 0x8C, 0x7C, 0x00, 0x7C, 0x6C, 0x00, 0x6C,
		0x60, 0x00, 0x60, 0x50, 0x00, 0x50, 0x44, 0x00, 0x44, 0x34, 0x00, 0x34,
		0x24, 0x00, 0x24, 0x18, 0x00, 0x18, 0xF4, 0xE8, 0xE4, 0xEC, 0xDC, 0xD4,
		0xE4, 0xCC, 0xC0, 0xE0, 0xC0, 0xB0, 0xD8, 0xB0, 0xA0, 0xD0, 0xA4, 0x90,
		0xC8, 0x98, 0x80, 0xC4, 0x8C, 0x74, 0xAC, 0x7C, 0x64, 0x98, 0x6C, 0x58,
		0x80, 0x5C, 0x4C, 0x6C, 0x4C, 0x3C, 0x54, 0x3C, 0x30, 0x3C, 0x2C, 0x24,
		0x28, 0x1C, 0x14, 0x10, 0x0C, 0x08, 0xEC, 0xEC, 0xEC, 0xDC, 0xDC, 0xDC,
		0xCC, 0xCC, 0xCC, 0xBC, 0xBC, 0xBC, 0xAC, 0xAC, 0xAC, 0x9C, 0x9C, 0x9C,
		0x8C, 0x8C, 0x8C, 0x7C, 0x7C, 0x7C, 0x6C, 0x6C, 0x6C, 0x60, 0x60, 0x60,
		0x50, 0x50, 0x50, 0x44, 0x44, 0x44, 0x34, 0x34, 0x34, 0x24, 0x24, 0x24,
		0x18, 0x18, 0x18, 0x08, 0x08, 0x08, 0xE8, 0xE0, 0xD4, 0xD8, 0xC8, 0xB0,
		0xC8, 0xB0, 0x90, 0xB8, 0x98, 0x70, 0xA8, 0x84, 0x58, 0x98, 0x70, 0x40,
		0x88, 0x5C, 0x2C, 0x7C, 0x4C, 0x18, 0x6C, 0x3C, 0x0C, 0x5C, 0x34, 0x0C,
		0x4C, 0x2C, 0x0C, 0x3C, 0x24, 0x0C, 0x2C, 0x1C, 0x08, 0x20, 0x14, 0x08,
		0xEC, 0xE8, 0xE4, 0xDC, 0xD4, 0xD0, 0xCC, 0xC4, 0xBC, 0xBC, 0xB0, 0xAC,
		0xAC, 0xA0, 0x98, 0x9C, 0x90, 0x88, 0x8C, 0x80, 0x78, 0x7C, 0x70, 0x68,
		0x6C, 0x60, 0x5C, 0x60, 0x54, 0x50, 0x50, 0x48, 0x44, 0x44, 0x3C, 0x38,
		0x34, 0x30, 0x2C, 0x24, 0x20, 0x20, 0x18, 0x14, 0x14, 0xE0, 0xE8, 0xD4,
		0xC8, 0xD4, 0xB4, 0xB4, 0xC0, 0x98, 0x9C, 0xAC, 0x7C, 0x88, 0x98, 0x60,
		0x70, 0x84, 0x4C, 0x5C, 0x70, 0x38, 0x4C, 0x5C, 0x28, 0x40, 0x50, 0x20,
		0x38, 0x44, 0x1C, 0x30, 0x3C, 0x18, 0x28, 0x30, 0x14, 0x20, 0x24, 0x10,
		0x18, 0x1C, 0x08, 0x0C, 0x10, 0x04, 0xEC, 0xD8, 0xCC, 0xDC, 0xB8, 0xA0,
		0xCC, 0x98, 0x7C, 0xBC, 0x80, 0x5C, 0xAC, 0x64, 0x3C, 0x9C, 0x50, 0x24,
		0x8C, 0x3C, 0x0C, 0x7C, 0x2C, 0x00, 0x6C, 0x24, 0x00, 0x60, 0x20, 0x00,
		0x50, 0x1C, 0x00, 0x44, 0x14, 0x00, 0x34, 0x10, 0x00, 0x24, 0x0C, 0x00,
		0xF0, 0xF0, 0xFC, 0xE4, 0xE4, 0xFC, 0xD8, 0xD8, 0xFC, 0xCC, 0xCC, 0xFC,
		0xC0, 0xC0, 0xFC, 0xB4, 0xB4, 0xFC, 0xA8, 0xA8, 0xFC, 0x9C, 0x9C, 0xFC,
		0x84, 0xD0, 0x00, 0x84, 0xB0, 0x00, 0x7C, 0x94, 0x00, 0x68, 0x78, 0x00,
		0x50, 0x58, 0x00, 0x3C, 0x40, 0x00, 0x2C, 0x24, 0x00, 0x1C, 0x08, 0x00,
		0x20, 0x00, 0x00, 0xEC, 0xD8, 0xC4, 0xDC, 0xC0, 0xB4, 0xCC, 0xB4, 0xA0,
		0xBC, 0x9C, 0x94, 0xAC, 0x90, 0x80, 0x9C, 0x84, 0x74, 0x8C, 0x74, 0x64,
		0x7C, 0x64, 0x58, 0x6C, 0x54, 0x4C, 0x60, 0x48, 0x44, 0x50, 0x40, 0x38,
		0x44, 0x34, 0x2C, 0x34, 0x2C, 0x24, 0x24, 0x18, 0x18, 0x18, 0x10, 0x10,
		0xFC, 0xF8, 0xFC, 0xAC, 0xD4, 0xF0, 0x70, 0xAC, 0xE4, 0x34, 0x8C, 0xD8,
		0x00, 0x6C, 0xD0, 0x30, 0x8C, 0xD8, 0x6C, 0xB0, 0xE4, 0xB0, 0xD4, 0xF0,
		0xFC, 0xFC, 0xF8, 0xFC, 0xEC, 0x40, 0xFC, 0xC0, 0x28, 0xFC, 0x8C, 0x10,
		0xFC, 0x50, 0x00, 0xC8, 0x38, 0x00, 0x98, 0x28, 0x00, 0x68, 0x18, 0x00,
		0x7C, 0xDC, 0x7C, 0x44, 0xB4, 0x44, 0x18, 0x90, 0x18, 0x00, 0x6C, 0x00,
		0xF8, 0xB8, 0xFC, 0xFC, 0x64, 0xEC, 0xFC, 0x00, 0xB4, 0xCC, 0x00, 0x70,
		0xFC, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFC, 0x00, 0xFC, 0x00, 0x00,
		0xFC, 0xFC, 0xFC, 0x61, 0x61, 0x61, 0xC0, 0xC0, 0xC0, 0xFC, 0x00, 0xF1};

static gint load_palette(GFile* file, guchar palette[], GError** error) {
	ignore_unused_variable_warning(file, palette, error);
	const U7object pal(gimp_file_get_utf8_name(file), 0);
	size_t         len;
	auto           data = pal.retrieve(len);
	if (!data || len == 0) {
		return 0;
	}
	const auto* ptr = data.get();
	if (len == 768) {
		for (unsigned i = 0; i < 256; i++) {
			gimp_cmap[i * 3 + 0] = Read1(ptr) << 2;
			gimp_cmap[i * 3 + 1] = Read1(ptr) << 2;
			gimp_cmap[i * 3 + 2] = Read1(ptr) << 2;
		}
	} else if (len == 1536) {
		// Double palette
		for (unsigned i = 0; i < 256; i++) {
			gimp_cmap[i * 3 + 0] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
			gimp_cmap[i * 3 + 1] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
			gimp_cmap[i * 3 + 2] = Read1(ptr) << 2;
			Read1(ptr);    // Skip entry from second palette
		}
	}
	return 256;
}

struct Bounds {
	int xright = -1;
	int xleft  = -1;
	int yabove = -1;
	int ybelow = -1;
};

Bounds get_shape_bounds(Shape_file& shape) {
	if (shape.is_rle()) {
		Bounds bounds;
		for (const auto& frame : shape) {
			bounds.xright = std::max(bounds.xright, frame->get_xright());
			bounds.xleft  = std::max(bounds.xleft, frame->get_xleft());
			bounds.yabove = std::max(bounds.yabove, frame->get_yabove());
			bounds.ybelow = std::max(bounds.ybelow, frame->get_ybelow());
		}
		return bounds;
	}
	// Shape is composed of flats
	return Bounds{7, 0, 0, 7};
}

/* Load Shape image into GIMP */

static GimpImage* load_image(
		GFile* file, GFile* palette_file, GimpRunMode run_mode,
		GError** error) {
	ignore_unused_variable_warning(file, palette_file, run_mode, error);
	if (!g_file_query_exists(file, nullptr)) {
		return nullptr;
	}
	Shape_file shape(gimp_file_get_utf8_name(file));
	if (shape.get_num_frames() == 0) {
		return nullptr;
	}
#ifdef DEBUG
	std::cout << "num_frames = " << shape.get_num_frames() << '\n';
#endif
	if (palette_file) {
		load_palette(palette_file, nullptr, error);
	}
	const Bounds  bounds = get_shape_bounds(shape);
	GimpImageType image_type;
	if (shape.is_rle()) {
		image_type = GIMP_INDEXEDA_IMAGE;
	} else {
		image_type = GIMP_INDEXED_IMAGE;
	}

	GimpImage* image = gimp_image_new(
			bounds.xleft + bounds.xright + 1, bounds.yabove + bounds.ybelow + 1,
			GIMP_INDEXED);
	gimp_palette_set_colormap(
			gimp_image_get_palette(image), babl_format("R'G'B' u8"), gimp_cmap,
			256 * 3);
	int framenum = 0;
	for (auto& frame : shape) {
		const std::string framename = "Frame " + std::to_string(framenum);
		GimpLayer*        layer     = gimp_layer_new(
                image, framename.c_str(), frame->get_width(),
                frame->get_height(), image_type, 100,
                gimp_image_get_default_new_layer_mode(image));
		gimp_image_insert_layer(image, layer, nullptr, 0);
		gimp_item_transform_translate(
				GIMP_ITEM(layer), bounds.xleft - frame->get_xleft(),
				bounds.yabove - frame->get_yabove());

		GeglBuffer* drawable = gimp_drawable_get_buffer(GIMP_DRAWABLE(layer));
		const GeglRectangle rect{
				0, 0, gegl_buffer_get_width(drawable),
				gegl_buffer_get_height(drawable)};

		Image_buffer8 img(
				frame->get_width(),
				frame->get_height());    // Render into a buffer.
		const unsigned char transp = 255;
		img.fill8(transp);    // Fill with transparent pixel.
		if (!frame->is_empty()) {
			frame->paint(&img, frame->get_xleft(), frame->get_yabove());
		}
		std::vector<unsigned char> pixels;
		const size_t num_pixels = frame->get_width() * frame->get_height();
		const unsigned char* pixel_data = nullptr;
		if (shape.is_rle()) {
			// Need to expand from (pixel)* to (pixel, alpha)*.
			pixels.reserve(num_pixels * 2);
			const auto* data = img.get_bits();
			for (const auto* ptr = data; ptr != data + num_pixels; ptr++) {
				pixels.push_back(*ptr);
				pixels.push_back(*ptr == transp ? 0 : 255);    // Alpha
			}
			pixel_data = pixels.data();
		} else {
			pixel_data = img.get_bits();
		}

		gegl_buffer_set(
				drawable, &rect, 0, nullptr, pixel_data, GEGL_AUTO_ROWSTRIDE);
		g_object_unref(drawable);
		framenum++;
	}

	gimp_image_add_hguide(image, bounds.yabove);
	gimp_image_add_vguide(image, bounds.xleft);
#ifdef DEBUG
	std::cout << "Added hguide=" << bounds.yabove << '\n'
			  << "Added vguide=" << bounds.xleft << '\n';
#endif

	return image;
}

static gboolean export_image(
		GFile* file, GimpImage* image, GimpRunMode run_mode, GError** error) {
	ignore_unused_variable_warning(file, image, run_mode, error);
	if (run_mode != GIMP_RUN_NONINTERACTIVE) {
		std::string name_buf("Saving ");
		name_buf += gimp_file_get_utf8_name(file);
		name_buf += ':';
		gimp_progress_init(name_buf.c_str());
	}

	// Find the guides...
	int hotx = -1;
	int hoty = -1;
	for (gint32 guide_ID = gimp_image_find_next_guide(image, 0); guide_ID > 0;
		 guide_ID        = gimp_image_find_next_guide(image, guide_ID)) {
#ifdef DEBUG
		std::cout << "Found guide " << guide_ID << ':';
#endif

		switch (gimp_image_get_guide_orientation(image, guide_ID)) {
		case GIMP_ORIENTATION_HORIZONTAL:
			if (hoty < 0) {
				hoty = gimp_image_get_guide_position(image, guide_ID);
#ifdef DEBUG
				std::cout << " horizontal=" << hoty << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_VERTICAL:
			if (hotx < 0) {
				hotx = gimp_image_get_guide_position(image, guide_ID);
#ifdef DEBUG
				std::cout << " vertical=" << hotx << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_UNKNOWN:
			break;
		}
	}

	GList* layers  = g_list_reverse(gimp_image_list_layers(image));
	gint32 nlayers = g_list_length(layers);
	std::cout << "SHP: Exporting " << g_list_length(layers) << " layers"
			  << std::endl;

	if (layers && !gimp_drawable_is_indexed(GIMP_DRAWABLE(layers->data))) {
		g_message("SHP: You can only save indexed images!");
		return FALSE;
	}

	Shape  shape(nlayers);
	GList* cur_layer = g_list_first(layers);
	for (auto& frame : shape) {
		GeglBuffer* drawable
				= gimp_drawable_get_buffer(GIMP_DRAWABLE(cur_layer->data));
		gint offsetX;
		gint offsetY;
		gimp_drawable_get_offsets(
				GIMP_DRAWABLE(cur_layer->data), &offsetX, &offsetY);
		const int    width           = gegl_buffer_get_width(drawable);
		const int    height          = gegl_buffer_get_height(drawable);
		const int    xleft           = hotx - offsetX;
		const int    yabove          = hoty - offsetY;
		const Babl*  format          = gegl_buffer_get_format(drawable);
		const size_t num_pixels      = width * height;
		const size_t bytes_per_pixel = babl_format_get_bytes_per_pixel(format);
		std::vector<guchar> pix(num_pixels * bytes_per_pixel);
		const GeglRectangle rect{
				0, 0, gegl_buffer_get_width(drawable),
				gegl_buffer_get_height(drawable)};
		gegl_buffer_get(
				drawable, &rect, 1.0, format, pix.data(), GEGL_AUTO_ROWSTRIDE,
				GEGL_ABYSS_NONE);
		g_object_unref(drawable);

		std::vector<unsigned char> out;
		unsigned char*             outptr;
		const bool has_alpha = babl_format_has_alpha(format) != 0;
		if (has_alpha) {
			out.reserve(pix.size() / bytes_per_pixel);
			for (size_t ii = 0; ii < pix.size(); ii += bytes_per_pixel) {
				out.push_back(pix[ii + 1] == 0 ? 255 : pix[ii]);
			}
			outptr = out.data();
		} else {
			outptr = pix.data();
		}
		frame = std::make_unique<Shape_frame>(
				outptr, width, height, xleft, yabove, has_alpha);
		cur_layer = g_list_next(cur_layer);
	}

	OFileDataSource ds(gimp_file_get_utf8_name(file));
	if (!ds.good()) {
		g_message("SHP: can't create \"%s\"\n", gimp_file_get_utf8_name(file));
		return -1;
	}
	shape.write(ds);
	g_list_free(layers);

	return TRUE;
}
