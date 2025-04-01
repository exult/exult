/*
 * SHP loading file filter for The GIMP version 2.x.
 *
 * (C) 2000-2001 Tristan Tarrant
 * (C) 2001-2004 Willem Jan Palenstijn
 * (C) 2010-2022 The Exult Team
 *
 * You can find the most recent version of this file in the Exult sources,
 * available from https://exult.info/
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#	pragma GCC diagnostic ignored "-Wold-style-cast"
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

#include <iostream>
#include <string>
#include <vector>

/* Declare some local functions.
 */
static void query();
static void run(
		const gchar* name, gint nparams, const GimpParam* param,
		gint* nreturn_vals, GimpParam** return_vals);
static void   load_palette(const std::string& filename);
static void   choose_palette();
static gint32 load_image(gchar* filename);
static gint32 save_image(
		gchar* filename, gint32 image_ID, gint32 drawable_ID,
		gint32 orig_image_ID);
static GimpRunMode run_mode;

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

GimpPlugInInfo PLUG_IN_INFO = {
		nullptr, /* init_proc  */
		nullptr, /* quit_proc  */
		query,   /* query_proc */
		run,     /* run_proc   */
};

struct u7frame {
	guchar* pixels;
	size_t  datalen;
	gint16  leftX;
	gint16  leftY;
	gint16  rightX;
	gint16  rightY;
	gint16  width;
	gint16  height;
};

struct u7shape {
	u7frame* frames;
	size_t   num_frames;
};

MAIN()    // NOLINT

static void query(void) {
	constexpr static const GimpParamDef load_args[] = {
			{ GIMP_PDB_INT32,     const_cast<gchar*>("run_mode"),
			 const_cast<gchar*>("Interactive, non-interactive")},
			{GIMP_PDB_STRING,     const_cast<gchar*>("filename"),
			 const_cast<gchar*>("The name of the file to load")},
			{GIMP_PDB_STRING, const_cast<gchar*>("raw_filename"),
			 const_cast<gchar*>("The name entered")            }
    };
	constexpr static const GimpParamDef load_return_vals[] = {
			{GIMP_PDB_IMAGE, const_cast<gchar*>("image"),
			 const_cast<gchar*>("Output image")}
    };
	constexpr static const gint nload_args
			= sizeof(load_args) / sizeof(load_args[0]);
	constexpr static const gint nload_return_vals
			= (sizeof(load_return_vals) / sizeof(load_return_vals[0]));

	constexpr static const GimpParamDef save_args[] = {
			{   GIMP_PDB_INT32,     const_cast<gchar*>("run_mode"),
			 const_cast<gchar*>("Interactive, non-interactive")},
			{   GIMP_PDB_IMAGE,        const_cast<gchar*>("image"),
			 const_cast<gchar*>("Image to save")               },
			{GIMP_PDB_DRAWABLE,     const_cast<gchar*>("drawable"),
			 const_cast<gchar*>("Drawable to save")            },
			{  GIMP_PDB_STRING,     const_cast<gchar*>("filename"),
			 const_cast<gchar*>("The name of the file to save")},
			{  GIMP_PDB_STRING, const_cast<gchar*>("raw_filename"),
			 const_cast<gchar*>("The name entered")            }
    };
	constexpr static const gint nsave_args
			= sizeof(save_args) / sizeof(save_args[0]);

	gimp_install_procedure(
			"file_shp_load", "loads files in Ultima VII SHP format",
			"FIXME: write help for shp_load", "Tristan Tarrant",
			"Tristan Tarrant", "2000", "<Load>/SHP", nullptr, GIMP_PLUGIN,
			nload_args, nload_return_vals, load_args, load_return_vals);

	gimp_register_magic_load_handler("file_shp_load", "shp", "", "");

	gimp_install_procedure(
			"file_shp_save", "Save files in Ultima VII SHP format",
			"FIXME: write help for shp_save", "Tristan Tarrant",
			"Tristan Tarrant", "2000", "<Save>/SHP", "INDEXEDA", GIMP_PLUGIN,
			nsave_args, 0, save_args, nullptr);

	gimp_register_save_handler("file_shp_save", "shp", "");
}

static void run(
		const gchar* name, gint nparams, const GimpParam* param,
		gint* nreturn_vals, GimpParam** return_vals) {
	ignore_unused_variable_warning(nparams);
	static GimpParam values[2];

	gegl_init(nullptr, nullptr);

	run_mode = static_cast<GimpRunMode>(param[0].data.d_int32);

	*nreturn_vals           = 1;
	*return_vals            = values;
	values[0].type          = GIMP_PDB_STATUS;
	values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	if (strcmp(name, "file_shp_load") == 0) {
		gimp_ui_init("u7shp", false);
		if (run_mode != GIMP_RUN_NONINTERACTIVE) {
			choose_palette();
		}
		const gint32 image_ID = load_image(param[1].data.d_string);

		if (image_ID != -1) {
			*nreturn_vals          = 2;
			values[1].type         = GIMP_PDB_IMAGE;
			values[1].data.d_image = image_ID;
		} else {
			status = GIMP_PDB_EXECUTION_ERROR;
		}
	} else if (strcmp(name, "file_shp_save") == 0) {
		const gint32 orig_image_ID = param[1].data.d_int32;
		const gint32 image_ID      = orig_image_ID;
		const gint32 drawable_ID   = param[2].data.d_int32;
		save_image(
				param[3].data.d_string, image_ID, drawable_ID, orig_image_ID);
	} else {
		status = GIMP_PDB_CALLING_ERROR;
	}
	values[0].data.d_status = status;
}

static void load_palette(const std::string& filename) {
	const U7object pal(filename, 0);
	size_t         len;
	auto           data = pal.retrieve(len);
	if (!data || len == 0) {
		return;
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
}

std::string file_select(const gchar* title) {
	GtkFileChooser* fsel = GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(
			title, nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel",
			GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr));
	GtkWidget*      btn  = gtk_dialog_get_widget_for_response(
            GTK_DIALOG(fsel), GTK_RESPONSE_CANCEL);
	GtkWidget* img = gtk_image_new_from_icon_name(
			"window-close", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(btn), img);
	btn = gtk_dialog_get_widget_for_response(
			GTK_DIALOG(fsel), GTK_RESPONSE_ACCEPT);
	img = gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(btn), img);
	gtk_window_set_modal(GTK_WINDOW(fsel), true);
	if (gtk_dialog_run(GTK_DIALOG(fsel)) == GTK_RESPONSE_ACCEPT) {
		std::string filename(gtk_file_chooser_get_filename(fsel));
		return filename;
	} else {
		return "";
	}
}

static void choose_palette() {
	load_palette(file_select("Choose palette"));
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

static gint32 load_image(gchar* filename) {
	Shape_file shape(filename);
#ifdef DEBUG
	std::cout << "num_frames = " << shape.get_num_frames() << '\n';
#endif
	const Bounds  bounds = get_shape_bounds(shape);
	GimpImageType image_type;
	if (shape.is_rle()) {
		image_type = GIMP_INDEXEDA_IMAGE;
	} else {
		image_type = GIMP_INDEXED_IMAGE;
	}

	const gint32 image_ID = gimp_image_new(
			bounds.xleft + bounds.xright + 1, bounds.yabove + bounds.ybelow + 1,
			GIMP_INDEXED);
	gimp_image_set_filename(image_ID, filename);
	gimp_image_set_colormap(image_ID, gimp_cmap, 256);
	int framenum = 0;
	for (auto& frame : shape) {
		const std::string framename = "Frame " + std::to_string(framenum);
		const gint32      layer_ID  = gimp_layer_new(
                image_ID, framename.c_str(), frame->get_width(),
                frame->get_height(), image_type, 100, GIMP_NORMAL_MODE);
		gimp_image_insert_layer(image_ID, layer_ID, -1, 0);
		gimp_item_transform_translate(
				layer_ID, bounds.xleft - frame->get_xleft(),
				bounds.yabove - frame->get_yabove());

		GeglBuffer*         drawable = gimp_drawable_get_buffer(layer_ID);
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

	gimp_image_add_hguide(image_ID, bounds.yabove);
	gimp_image_add_vguide(image_ID, bounds.xleft);
#ifdef DEBUG
	std::cout << "Added hguide=" << bounds.yabove << '\n'
			  << "Added vguide=" << bounds.xleft << '\n';
#endif

	return image_ID;
}

static gint32 save_image(
		gchar* filename, gint32 image_ID, gint32 drawable_ID,
		gint32 orig_image_ID) {
	ignore_unused_variable_warning(drawable_ID, orig_image_ID);
	if (run_mode != GIMP_RUN_NONINTERACTIVE) {
		std::string name_buf("Saving ");
		name_buf += filename;
		name_buf += ':';
		gimp_progress_init(name_buf.c_str());
	}

	// Find the guides...
	int hotx = -1;
	int hoty = -1;
	for (gint32 guide_ID = gimp_image_find_next_guide(image_ID, 0);
		 guide_ID > 0;
		 guide_ID = gimp_image_find_next_guide(image_ID, guide_ID)) {
#ifdef DEBUG
		std::cout << "Found guide " << guide_ID << ':';
#endif

		switch (gimp_image_get_guide_orientation(image_ID, guide_ID)) {
		case GIMP_ORIENTATION_HORIZONTAL:
			if (hoty < 0) {
				hoty = gimp_image_get_guide_position(image_ID, guide_ID);
#ifdef DEBUG
				std::cout << " horizontal=" << hoty << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_VERTICAL:
			if (hotx < 0) {
				hotx = gimp_image_get_guide_position(image_ID, guide_ID);
#ifdef DEBUG
				std::cout << " vertical=" << hotx << '\n';
#endif
			}
			break;
		case GIMP_ORIENTATION_UNKNOWN:
			break;
		}
	}

	// get a list of layers for this image_ID
	int     nlayers;
	gint32* layers = gimp_image_get_layers(image_ID, &nlayers);

	if (nlayers > 0 && !gimp_drawable_is_indexed(layers[0])) {
		g_message("SHP: You can only save indexed images!");
		return -1;
	}

	Shape shape(nlayers);
	int   layer = nlayers - 1;
	for (auto& frame : shape) {
		GeglBuffer* drawable = gimp_drawable_get_buffer(layers[layer]);
		gint        offsetX;
		gint        offsetY;
		gimp_drawable_offsets(layers[layer], &offsetX, &offsetY);
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
		layer--;
	}

	OFileDataSource ds(filename);
	if (!ds.good()) {
		g_message("SHP: can't create \"%s\"\n", filename);
		return -1;
	}
	shape.write(ds);

	return 0;
}
