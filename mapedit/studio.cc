/*
Copyright (C) 2000-2022 The Exult Team

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

#include "studio.h"

#include "Configuration.h"
#include "Flex.h"
#include "U7fileman.h"
#include "combo.h"
#include "databuf.h"
#include "exceptions.h"
#include "execbox.h"
#include "fnames.h"
#include "items.h"
#include "locator.h"
#include "logo.xpm"
#include "modmgr.h"
#include "objserial.h"
#include "shapefile.h"
#include "shapegroup.h"
#include "shapelst.h"
#include "shapevga.h"
#include "u7drag.h"
#include "ucbrowse.h"
#include "utils.h"

#include <dirent.h>
#include <glib.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdio> /* These are for sockets. */
#include <cstdlib>
#include <memory>
#include <type_traits>

#ifdef _WIN32
#	include "servewin32.h"
#else
// #	include <fcntl.h> The call to fcntl is for !_WIN32 and has been
// commented out
#	include <sys/stat.h>
#	include <sys/socket.h>
#	include <sys/un.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#	include <sys/types.h>
#endif

#ifdef HAVE_GETOPT_LONG
#	include <getopt.h>
#endif

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::make_unique;
using std::ofstream;
using std::string;
using std::vector;

#ifdef _WIN32
static void do_cleanup_output() {
	cleanup_output("studio_");
}
#endif

ExultStudio*   ExultStudio::self = nullptr;
Configuration* config            = nullptr;
GameManager*   gamemanager       = nullptr;

// Mode menu items:
constexpr static const std::array mode_names{
		"move1", "paint1", "paint_with_chunks1", "pick_for_combo1",
		"select_chunks1"};

enum ExultFileTypes {
	ShapeArchive = 1,
	ChunksArchive,
	NpcsArchive,
	PaletteFile,
	FlexArchive,
	ComboArchive
};

struct FileTreeItem {
	const gchar*   label;
	ExultFileTypes datatype;
	FileTreeItem*  children;
};

/* columns */
enum {
	FOLDER_COLUMN = 0,
	FILE_COLUMN,
	DATA_COLUMN,
	NUM_COLUMNS
};

static void Filelist_selection(GtkTreeView* treeview, GtkTreePath* path) {
	int           type = -1;
	char*         text;
	GtkTreeIter   iter;
	GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(
			model, &iter, FILE_COLUMN, &text, DATA_COLUMN, &type, -1);
	printf("%s %d\n", text, type);

	ExultStudio* studio = ExultStudio::get_instance();
	switch (type) {
	case ShapeArchive:
		studio->set_browser("Shape Browser", studio->create_browser(text));
		break;
	case ComboArchive:
		studio->set_browser("Combo Browser", studio->create_browser(text));
		break;
	case ChunksArchive:
		studio->set_browser("Chunk Browser", studio->create_browser(text));
		break;
	case NpcsArchive:
		studio->set_browser("NPC Browser", studio->create_browser(text));
		break;
	case PaletteFile:
		studio->set_browser("Palette Browser", studio->create_browser(text));
		break;
	default:
		break;
	}
	g_free(text);
}

C_EXPORT void on_filelist_tree_cursor_changed(GtkTreeView* treeview) {
	GtkTreePath*       path;
	GtkTreeViewColumn* col;

	gtk_tree_view_get_cursor(treeview, &path, &col);
	if (path == nullptr) {
		return;
	}
	Filelist_selection(treeview, path);
}

C_EXPORT void on_open_game_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->open_game_dialog();
}

C_EXPORT void on_new_game_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->new_game();
}

C_EXPORT void on_new_mod_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->open_game_dialog(true);
}

C_EXPORT void on_connect_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->connect_to_server();
}

C_EXPORT void on_save_all1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->save_all();
}

C_EXPORT void on_new_shapes_file_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->new_shape_file(false);
}

C_EXPORT void on_new_shape_file_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->new_shape_file(true);
}

C_EXPORT void on_save_map_menu_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->write_map();
}

C_EXPORT void on_read_map_menu_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->read_map();
}

C_EXPORT void on_save_shape_info1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->write_shape_info();
}

C_EXPORT void on_reload_usecode_menu_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->reload_usecode();
}

C_EXPORT void on_compile_usecode_menu_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->compile();
}

C_EXPORT void on_fix_old_shape_info_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio*     studio = ExultStudio::get_instance();
	Shape_file_info* vga    = studio->get_vgafile();
	if (!vga) {
		return;
	}
	Vga_file* ifile = vga->get_ifile();
	if (!ifile) {
		return;
	}
	auto* svga = dynamic_cast<Shapes_vga_file*>(ifile);
	if (!svga) {
		return;
	}
	std::string msg = "Older versions of Exult Studio lost information when "
					  "saving the following files:\n";
	msg += "\t\t\"AMMO.DAT\", \"MONSTERS.DAT\", \"WEAPONS.DAT\"\n";
	msg += "If you click \"yes\", Exult Studio will reload the STATIC version "
		   "of the above\n";
	msg += "data files to recover the lost data. This will overwrite any "
		   "modifications you\n";
	msg += "may have done to the data of the shapes in the aforementioned "
		   "files. For example,\n";
	msg += "the Death Scythe's weapon information would be reloaded from the "
		   "static file,\n";
	msg += "but if you had added weapon information to a moongate, it would "
		   "remain intact.\n\n";
	msg += "\t\tAre you sure you want to proceed?";
	int choice = studio->prompt(msg.c_str(), "Yes", "No", nullptr);
	if (choice == 1) {    // No?
		return;
	}
	svga->fix_old_shape_info(studio->get_game_type());
	studio->set_shapeinfo_modified();
	choice = studio->prompt(
			"The data has been reloaded. You should save shape information "
			"now.\n\n"
			"\t\tDo you wish to save the shape information now?",
			"Yes", "No", nullptr);
	if (choice == 1) {    // No?
		return;
	}
	studio->write_shape_info(true);
}

C_EXPORT void on_save_groups1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->save_groups();
}

C_EXPORT void on_save_combos1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->save_combos();
}

C_EXPORT void on_reload_css_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->reload_css();
}

C_EXPORT void on_preferences_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->open_preferences();
}

C_EXPORT void on_cut1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	unsigned char z = 0;
	ExultStudio::get_instance()->send_to_server(Exult_server::cut, &z, 1);
}

C_EXPORT void on_copy1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	unsigned char o = 1;
	ExultStudio::get_instance()->send_to_server(Exult_server::cut, &o, 1);
}

C_EXPORT void on_paste1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->send_to_server(Exult_server::paste);
}

C_EXPORT void on_properties1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	unsigned char o = 0;    // 0=npc/egg properties.
	ExultStudio::get_instance()->send_to_server(
			Exult_server::edit_selected, &o, 1);
}

C_EXPORT void on_basic_properties1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	unsigned char o = 1;    // 1=basic object properties.
	ExultStudio::get_instance()->send_to_server(
			Exult_server::edit_selected, &o, 1);
}

C_EXPORT void on_move1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	// NOTE:  modes are defined in cheat.h.
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
		ExultStudio::get_instance()->set_edit_mode(0);
	}
}

C_EXPORT void on_paint1_activate(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
		ExultStudio::get_instance()->set_edit_mode(1);
	}
}

C_EXPORT void on_paint_with_chunks1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
		ExultStudio::get_instance()->set_edit_mode(2);
	}
}

C_EXPORT void on_pick_for_combo1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
		ExultStudio::get_instance()->set_edit_mode(3);
	}
}

C_EXPORT void on_select_chunks1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
		ExultStudio::get_instance()->set_edit_mode(4);
	}
}

C_EXPORT void on_unused_shapes1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	if (EStudio::Prompt(
				"Finding unused shapes may take several minutes\nProceed?",
				"Yes", "No")
		!= 0) {
		return;
	}
	ExultStudio::get_instance()->send_to_server(Exult_server::unused_shapes);
}

C_EXPORT void on_play_button_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->set_play(gtk_toggle_button_get_active(button));
}

C_EXPORT void on_tile_grid_button_toggled(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->set_tile_grid(
			gtk_toggle_button_get_active(button));
}

C_EXPORT void on_edit_lift_spin_changed(
		GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->set_edit_lift(
			gtk_spin_button_get_value_as_int(button));
}

C_EXPORT void on_hide_lift_spin_changed(
		GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->set_hide_lift(
			gtk_spin_button_get_value_as_int(button));
}

C_EXPORT void on_edit_terrain_button_toggled(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->set_edit_terrain(
			gtk_toggle_button_get_active(button));
}

/*
 *  Configure main window.
 */
C_EXPORT gboolean on_main_window_configure_event(
		GtkWidget*         widget,    // The view window.
		GdkEventConfigure* event, gpointer data) {
	ignore_unused_variable_warning(widget, event, data);
	ExultStudio* studio = ExultStudio::get_instance();
	// Configure "Hide lift" spin range.
	studio->set_spin(
			"hide_lift_spin", studio->get_spin("hide_lift_spin"), 1, 255);
	return false;
}

/*
 *  Main window's close button.
 */
C_EXPORT gboolean on_main_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	if (!ExultStudio::get_instance()->okay_to_close()) {
		return true;    // Can't quit.
	}
	return false;
}

C_EXPORT void on_main_window_destroy_event(GtkWidget* widget, gpointer data) {
	ignore_unused_variable_warning(widget, data);
	gtk_main_quit();
}

/*
 *  "Exit" in main window.
 */
C_EXPORT void on_main_window_quit(GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	if (ExultStudio::get_instance()->okay_to_close()) {
		gtk_main_quit();
	}
}

/*
 *  Main window got focus.
 */
C_EXPORT gboolean on_main_window_focus_in_event(
		GtkWidget* widget, GdkEventFocus* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	Shape_chooser::check_editing_files();
	return false;
}

/*
 *  Set up everything.
 */

ExultStudio::ExultStudio(int argc, char** argv)
		: glade_path(nullptr), css_path(nullptr), css_provider(nullptr),
		  static_path(nullptr), image_editor(nullptr), default_game(nullptr),
		  background_color(0), shape_scale(0), shape_bilinear(false),
		  shape_info_modified(false), shape_names_modified(false),
		  npc_modified(false), files(nullptr), curfile(nullptr),
		  vgafile(nullptr), facefile(nullptr), fontfile(nullptr),
		  gumpfile(nullptr), spritefile(nullptr), paperdolfile(nullptr),
		  browser(nullptr), bargewin(nullptr), barge_ctx(0), barge_status_id(0),
		  eggwin(nullptr), egg_monster_single(nullptr),
		  egg_missile_single(nullptr), egg_ctx(0), egg_status_id(0),
		  npcwin(nullptr), npc_single(nullptr), npc_face_single(nullptr),
		  npc_ctx(0), npc_status_id(0), objwin(nullptr), obj_single(nullptr),
		  contwin(nullptr), cont_single(nullptr), shapewin(nullptr),
		  shape_single(nullptr), gump_single(nullptr), body_single(nullptr),
		  explosion_single(nullptr), weapon_family_single(nullptr),
		  weapon_projectile_single(nullptr), ammo_family_single(nullptr),
		  ammo_sprite_single(nullptr), cntrules_shape_single(nullptr),
		  frameflags_frame_single(nullptr), effhps_frame_single(nullptr),
		  framenames_frame_single(nullptr), frameusecode_frame_single(nullptr),
		  objpaperdoll_wframe_single(nullptr),
		  objpaperdoll_spotframe_single(nullptr),
		  brightness_frame_single(nullptr), warmth_frame_single(nullptr),
		  npcpaperdoll_aframe_single(nullptr),
		  npcpaperdoll_atwohanded_single(nullptr),
		  npcpaperdoll_astaff_single(nullptr),
		  npcpaperdoll_bframe_single(nullptr),
		  npcpaperdoll_hframe_single(nullptr),
		  npcpaperdoll_hhelm_single(nullptr),
		  objpaperdoll_frame0_single(nullptr),
		  objpaperdoll_frame1_single(nullptr),
		  objpaperdoll_frame2_single(nullptr),
		  objpaperdoll_frame3_single(nullptr), equipwin(nullptr),
		  locwin(nullptr), combowin(nullptr), compilewin(nullptr),
		  compile_box(nullptr), ucbrowsewin(nullptr), gameinfowin(nullptr),
		  game_type(BLACK_GATE), expansion(false), sibeta(false), curr_game(-1),
		  curr_mod(-1), server_socket(-1), server_input_tag(-1),
		  waiting_for_server(nullptr) {
#ifdef _WIN32
	// Enable the GTK+ 3 OLE Drag and Drop
	g_setenv("GDK_WIN32_USE_EXPERIMENTAL_OLE2_DND", "1", 0);
#endif
	// Initialize the various subsystems
	self = this;
	gtk_init(&argc, &argv);
	g_object_set(
			gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
			true, nullptr);
#ifdef _WIN32
	bool portable = false;
#endif
	// Get options.
	bool               silent = false;      // Hide game search.
	const char*        xmldir = nullptr;    // Default:  Look here for .glade.
	const char*        cssdir = nullptr;    // Default:  Look here for .css.
	string             game;                // Game to look up in .exult.cfg.
	string             modtitle;    // Mod title to look up in <MODS>/*.cfg.
	string             alt_cfg;
	static const char* optstring = "hsc:g:m:px:y:z:";
	opterr                       = 0;    // Don't let getopt() print errs.
	int  optchr;
	auto get_next_option = [&]() {
#ifdef HAVE_GETOPT_LONG
		static const option optarray[] = {
				{    "help",       no_argument, nullptr, 'h'},
				{  "silent",       no_argument, nullptr, 's'},
				{  "config", required_argument, nullptr, 'c'},
				{    "game", required_argument, nullptr, 'g'},
				{     "mod", required_argument, nullptr, 'm'},
				{"portable",       no_argument, nullptr, 'p'},
				{  "xmldir", required_argument, nullptr, 'x'},
				{  "cssdir", required_argument, nullptr, 'y'},
				{    "zoom", required_argument, nullptr, 'z'},
				{   nullptr,                 0, nullptr,   0}
        };
		return getopt_long(argc, argv, optstring, optarray, nullptr);
#else
		return getopt(argc, argv, optstring);
#endif    // HAVE_GETOPT_LONG
	};
	while ((optchr = get_next_option()) != -1) {
		switch (optchr) {
		case 'c':    // Configuration file.
			alt_cfg = optarg;
			break;
		case 'g':    // Game.  Replaces use of -d, -x.
			game = optarg;
			if (game == "bg") {
				game = CFG_BG_NAME;
			}
			if (game == "fov") {
				game = CFG_FOV_NAME;
			}
			if (game == "si") {
				game = CFG_SI_NAME;
			}
			if (game == "ss") {
				game = CFG_SS_NAME;
			}
			if (game == "sib") {
				game = CFG_SIB_NAME;
			}
			break;
		case 'x':    // XML (.glade) directory.
			xmldir = optarg;
			break;
		case 'y':    // CSS (.css) directory.
			cssdir = optarg;
			break;
		case 'm':    // Mod.
			modtitle = optarg;
			break;
		case 'z':
			if ((optarg[0] == 'b') || (optarg[0] == 'B')) {
				shape_bilinear = true;
				optarg++;
			} else if ((optarg[0] == 'n') || (optarg[0] == 'N')) {
				shape_bilinear = false;
				optarg++;
			} else {
				shape_bilinear = false;
			}
			shape_scale
					= (static_cast<int>(std::strtoul(optarg, nullptr, 10))
					   / 50);
			if (shape_scale < 2) {
				shape_scale = 2;
			} else if (shape_scale > 32) {
				shape_scale = 32;
			} else {
				// The expected values are 2,3, 4,6, 8,12, 16,24, 32 :
				//   the bounds 2 and 32 are checked above.
				//   Loop : divide by 2, until reaching 2 or 3.
				int shape_pow2 = 1;
				while (shape_scale > 4) {
					shape_scale >>= 1;
					shape_pow2 <<= 1;
				}
				shape_scale *= shape_pow2;
			}
			break;
		case 'p':
#ifdef _WIN32
			portable = true;
#endif
			break;
		case 's':
			silent = true;
			break;
		case 'h':
#ifdef HAVE_GETOPT_LONG
			cerr << "Usage: exult_studio [--help|-h] [--silent|-s] "
					"[--config|-c <configfile>]"
				 << endl
				 << "                    [--game|-g <game>] [--mod|-m <mod>] "
					"[--zoom|-z <zoomshape>]"
				 << endl;
#	ifdef _WIN32
			cerr << "                    [--portable|-p]" << endl;
#	endif
			cerr << "                    [--xmldir|-x <gladedir>] [--cssdir|-y "
					"<cssdir>]"
				 << endl
				 << "        --help                   Show this information"
				 << endl
				 << "        --silent                 Do not display the state "
					"of the games"
				 << endl
				 << "        --config configfile      Specify alternate config "
					"file, default is (.)exult.cfg"
				 << endl
				 << "        --game game              Start with <game> "
					"directly, one of :"
				 << endl
				 << "                                         blackgate or bg, "
					"forgeofvirtue or fov"
				 << endl
				 << "                                         serpentisle or "
					"si, silverseed or ss, serpentbeta or sib"
				 << endl
				 << "        --mod mod                With --game, Start vith "
					"<game> and <mod>"
				 << endl;
#	ifdef _WIN32
			cerr << "        --portable               Make the home path the "
					"Exult directory (old Windows way)"
				 << endl;
#	endif
			cerr << "        --xmldir gladedir        Specify where the "
					"exult_studio.glade resides, default is share/exult"
				 << endl;
			cerr << "        --cssdir cssdir          Specify where the "
					"exult_studio.css   resides, default is share/exult"
				 << endl;
			cerr << "        --zoom zoomshape         Enlarge the displayed "
					"Shapes, [B|N(default)]percent, default is 100"
				 << endl;
			cerr << "                                         With Nearest : N "
					"( default ), or Bilinear : B"
				 << endl;
			cerr << "                                         Percent is one "
					"of 100, 150, 200, 300, 400, 600, 800, 1200 or 1600"
				 << endl;
#else
			cerr << "Usage: exult_studio [-h] [-s] [-c <configfile>]" << endl
				 << "                    [-g <game>] [-m <mod>]" << endl;
#	ifdef _WIN32
			cerr << "                    [-p]" << endl;
#	endif
			cerr << "                    [-x <gladedir>] [-y <cssdir>]" << endl
				 << "        -h              Show this information" << endl
				 << "        -s              Do not display the state of the "
					"games"
				 << endl
				 << "        -c configfile   Specify alternate config file, "
					"default is (.)exult.cfg"
				 << endl
				 << "        -g game         Start with <game> directly, one "
					"of :"
				 << endl
				 << "                                blackgate or bg, "
					"forgeofvirtue or fov"
				 << endl
				 << "                                serpentisle or si, "
					"silverseed or ss, serpentbeta or sib"
				 << endl
				 << "        -m mod          With -g, Start vith <game> and "
					"<mod>"
				 << endl;
#	ifdef _WIN32
			cerr << "        -p              Make the home path the Exult "
					"directory (old Windows way)"
				 << endl;
#	endif
			cerr << "        -x gladedir     Specify where the "
					"exult_studio.glade resides, default is share/exult"
				 << endl;
			cerr << "        -y cssdir       Specify where the "
					"exult_studio.css   resides, default is share/exult"
				 << endl;
			cerr << "        -z zoomshape    Enlarge the displayed Shapes, "
					"[B|N(default)]percent, default is 100"
				 << endl;
			cerr << "                                With Nearest : N ( "
					"default ), or Bilinear : B"
				 << endl;
			cerr << "                                Percent is one of 100, "
					"150, 200, 300, 400, 600, 800, 1200 or 1600"
				 << endl;
#endif    // HAVE_GETOPT_LONG
			exit(1);
		}
	}
#ifdef _WIN32
	if (portable) {
		add_system_path("<HOME>", ".");
	}
#endif
	cout << endl
		 << "ExultStudio compiled with GTK+ " << GTK_MAJOR_VERSION << "."
		 << GTK_MINOR_VERSION << "." << GTK_MICRO_VERSION
		 << " running with GTK+ " << gtk_major_version << "."
		 << gtk_minor_version << "." << gtk_micro_version << endl
		 << endl;
	setup_program_paths();
#ifdef _WIN32
	redirect_output("studio_");
	std::atexit(do_cleanup_output);
#endif
	config = new Configuration;
	if (!alt_cfg.empty()) {
		config->read_abs_config_file(alt_cfg);
	} else {
		config->read_config_file(USER_CONFIGURATION_FILE);
	}
	// Get Shape scaling from config if not set by command line, report if above
	// 100%
	if (shape_scale == 0) {
		config->value("config/estudio/shape_scale", shape_scale, 2);
		config->value("config/estudio/shape_bilinear", shape_bilinear, false);
		if (shape_scale > 2) {
			cout << "Scaling Shapes by " << (shape_scale * 50) << "% using "
				 << (shape_bilinear ? "Bilinear." : "Nearest (by config).")
				 << endl;
		}
	} else if (shape_scale > 2) {
		cout << "Scaling Shapes by " << (shape_scale * 50) << "% using "
			 << (shape_bilinear ? "Bilinear." : "Nearest (by command line).")
			 << endl;
	}
	// Setup virtual directories
	string data_path;
	config->value("config/disk/data_path", data_path, EXULT_DATADIR);
	setup_data_dir(data_path, argv[0]);
	string datastr;
#ifdef MACOSX
	if (is_system_path_defined("<BUNDLE>")) {
		datastr = get_system_path("<BUNDLE>");
	} else
#endif
		datastr = get_system_path("<DATA>");

	if (!xmldir) {
		xmldir = datastr.c_str();
	}
	char path[256];    // Set up paths.
	if (xmldir) {
		strcpy(path, xmldir);
	} else if (U7exists(EXULT_DATADIR "/exult_studio.glade")) {
		strcpy(path, EXULT_DATADIR);
	} else {
		strcpy(path, ".");
	}
	strcat(path, "/exult_studio.glade");
	// Load the Glade interface
	GError* error = nullptr;
	app_xml       = gtk_builder_new();
	if (!gtk_builder_add_from_file(app_xml, path, &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		exit(1);
	}
	{
		GSList* objects = gtk_builder_get_objects(app_xml);
		for (GSList* l = objects; l; l = l->next) {
			auto* obj = static_cast<GObject*>(l->data);
			if (GTK_IS_WIDGET(obj) && GTK_IS_BUILDABLE(obj)) {
				gtk_widget_set_name(
						GTK_WIDGET(obj),
						gtk_buildable_get_name(GTK_BUILDABLE(obj)));
			}
		}
		g_slist_free(objects);
	}
	assert(app_xml);
	app        = get_widget("main_window");
	w_at_close = 0;
	h_at_close = 0;
	assert(app);
	glade_path = g_strdup(path);

	if (!cssdir) {
		cssdir = datastr.c_str();
	}
	if (cssdir) {
		strcpy(path, cssdir);
	} else if (U7exists(EXULT_DATADIR "/exult_studio.css")) {
		strcpy(path, EXULT_DATADIR);
	} else {
		strcpy(path, ".");
	}
	strcat(path, "/exult_studio.css");
	// Load the CSS provider
	css_provider = gtk_css_provider_new();
	assert(css_provider);
	gtk_widget_set_sensitive(get_widget("reload_css"), true);
	cout << "Looking for CSS at '" << path << "'... ";
	if (U7exists(path)) {
		cout << "loading." << endl;
		if (!gtk_css_provider_load_from_path(css_provider, path, &error)) {
			cerr << "Couldn't load css file: " << error->message << endl;
			cerr << "ExultStudio proceeds without it. "
				 << "You can fix it and reload it in 'File > Reload > Style "
					"CSS'"
				 << endl;
			g_error_free(error);
		}
	} else {
		cout << "but it wasn't there." << endl;
	}
	gtk_style_context_add_provider_for_screen(
			gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_USER);
	css_path = g_strdup(path);

	mainnotebook = GTK_NOTEBOOK(get_widget("notebook3"));

	// Create Zoom shapes GUI
	create_zoom_controls();

	// More setting up...
	// Connect signals automagically.
	gtk_builder_connect_signals(app_xml, nullptr);
	int w;
	int h;    // Get main window dims.
	config->value("config/estudio/main/width", w, 0);
	config->value("config/estudio/main/height", h, 0);
	if (w > 1 && h > 1) {
		//++++Used to work  gtk_window_set_default_size(GTK_WINDOW(app), w, h);
		gtk_window_resize(GTK_WINDOW(app), w, h);
	}
	gtk_widget_set_visible(app, true);
	g_signal_connect(
			G_OBJECT(app), "key-press-event", G_CALLBACK(on_app_key_press),
			this);
	// Background color for shape browser.
	int bcolor;
	config->value("config/estudio/background_color", bcolor, 0);
	set_background_color(bcolor);

	// Load games and mods; also stores system paths:
	gamemanager = new GameManager(silent);

	if (gamemanager->get_game_count() == 0) {
		const int choice = prompt(
				"Exult Studio could not find any games to edit.\n\n"
				"Do you wish to create a new game to edit?",
				"Yes", "No");
		if (choice == 0) {
			new_game();
		} else {
			exit(1);
		}
	} else {
		string defgame;    // Default game name.
		config->value("config/estudio/default_game", defgame, "blackgate");
		default_game = g_strdup(defgame.c_str());
		if (game.empty()) {
			game = default_game;
		}
		config->set("config/estudio/default_game", defgame, true);
		if (!game.empty()) {    // Game given?
			set_game_path(game, modtitle);
		}
	}

	string iedit;    // Get image-editor command.
	config->value("config/estudio/image_editor", iedit, "gimp");
	image_editor = g_strdup(iedit.c_str());
	config->set("config/estudio/image_editor", iedit, true);
	// Init. 'Mode' menu, since Glade
	//   doesn't seem to do it right.
	GSList* group = nullptr;
	for (size_t i = 0; i < mode_names.size(); i++) {
		GtkWidget* item = get_widget(mode_names[i]);
		gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(item), group);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), i == 0);
	}
	setup_maps_list();    // Init. 'maps' menu.
}

ExultStudio::~ExultStudio() {
	// Store main window size.
	const int w = w_at_close;
	const int h = h_at_close;
	// Finish up external edits.
	Shape_chooser::clear_editing_files();

	config->set("config/estudio/main/width", w, true);
	config->set("config/estudio/main/height", h, true);
	config->set("config/estudio/shape_scale", shape_scale, true);
	config->set("config/estudio/shape_bilinear", shape_bilinear, true);
	Free_text();
	g_free(glade_path);
	if (css_path) {
		g_free(css_path);
	}
	delete files;
	files = nullptr;
	palbuf.reset();
	if (objwin) {
		gtk_widget_destroy(objwin);
	}
	delete obj_single;
	if (contwin) {
		gtk_widget_destroy(contwin);
	}
	delete cont_single;
	if (eggwin) {
		gtk_widget_destroy(eggwin);
	}
	delete egg_monster_single;
	delete egg_missile_single;
	eggwin = nullptr;
	if (npcwin) {
		gtk_widget_destroy(npcwin);
	}
	delete npc_single;
	npcwin = nullptr;
	if (shapewin) {
		gtk_widget_destroy(shapewin);
	}
	delete shape_single;
	delete gump_single;
	delete body_single;
	delete explosion_single;
	delete weapon_family_single;
	delete weapon_projectile_single;
	delete ammo_family_single;
	delete ammo_sprite_single;
	delete cntrules_shape_single;
	delete frameflags_frame_single;
	delete effhps_frame_single;
	delete framenames_frame_single;
	delete frameusecode_frame_single;
	delete objpaperdoll_wframe_single;
	delete objpaperdoll_spotframe_single;
	delete brightness_frame_single;
	delete warmth_frame_single;
	delete npcpaperdoll_aframe_single;
	delete npcpaperdoll_atwohanded_single;
	delete npcpaperdoll_astaff_single;
	delete npcpaperdoll_bframe_single;
	delete npcpaperdoll_hframe_single;
	delete npcpaperdoll_hhelm_single;
	delete objpaperdoll_frame0_single;
	delete objpaperdoll_frame1_single;
	delete objpaperdoll_frame2_single;
	delete objpaperdoll_frame3_single;
	shapewin = nullptr;
	if (equipwin) {
		gtk_widget_destroy(equipwin);
	}
	equipwin = nullptr;
	if (compilewin) {
		gtk_widget_destroy(compilewin);
	}
	compilewin = nullptr;
	delete compile_box;
	compile_box = nullptr;
	delete locwin;
	locwin = nullptr;
	delete combowin;
	combowin = nullptr;
	delete ucbrowsewin;
	if (gameinfowin) {
		gtk_widget_destroy(gameinfowin);
	}
	gameinfowin = nullptr;
	g_object_unref(G_OBJECT(app_xml));
	if (server_input_tag >= 0) {
		g_source_remove(server_input_tag);
	}
	if (server_socket >= 0) {
		disconnect_from_server();
	}
	g_free(static_path);
	g_free(image_editor);
	g_free(default_game);
	self = nullptr;
	delete config;
	config = nullptr;
}

/*
 *  Okay to close game?
 */

bool ExultStudio::okay_to_close() {
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(app, &alloc);
	w_at_close = alloc.width;
	h_at_close = alloc.height;

	if (need_to_save()) {
		const int choice
				= prompt("File(s) modified.  Save?", "Yes", "No", "Cancel");
		if (choice == 2) {    // Cancel?
			return false;
		}
		if (choice == 0) {
			save_all();    // Write out changes.
		}
	}
	return true;
}

/*
 *  See if given window has focus (experimental).
 */

inline bool Window_has_focus(GtkWindow* win) {
	return gtk_window_get_focus(win) != nullptr
		   && gtk_widget_has_focus(GTK_WIDGET(gtk_window_get_focus(win)));
}

/*
 *  Do we have focus?  Currently only checks main window and group
 *  windows.
 */

bool ExultStudio::has_focus() {
	if (Window_has_focus(GTK_WINDOW(app))) {
		return true;    // Main window.
	}
	// Group windows:
	return std::any_of(
			group_windows.cbegin(), group_windows.cend(), Window_has_focus);
}

/*
 *  Set browser area.  NOTE:  obj may be null.
 */
void ExultStudio::set_browser(const char* name, Object_browser* obj) {
	GtkWidget* browser_frame = get_widget("browser_frame");
	GtkWidget* browser_box   = get_widget("browser_box");
	//+++Now owned by Shape_file_info.  delete browser;
	if (browser) {
		gtk_container_remove(GTK_CONTAINER(browser_box), browser->get_widget());
	}
	browser = obj;

	gtk_frame_set_label(GTK_FRAME(browser_frame), name);
	gtk_widget_set_visible(browser_box, true);
	if (browser) {
		gtk_box_pack_start(
				GTK_BOX(browser_box), browser->get_widget(), true, true, 0);
	}
}

Object_browser* ExultStudio::create_browser(const char* fname) {
	curfile = open_shape_file(fname);
	if (!curfile) {
		return nullptr;
	}
	Object_browser* chooser = curfile->get_browser(vgafile, palbuf.get());
	setup_groups();    // Set up 'groups' page.
	return chooser;
}

/*
 *  Get shape name.
 */

const char* ExultStudio::get_shape_name(int shnum) {
	return shnum >= 0 && shnum < get_num_item_names() ? get_item_name(shnum)
													  : nullptr;
}

/*
 *  Get current groups.
 */

Shape_group_file* ExultStudio::get_cur_groups() {
	return curfile ? curfile->get_groups() : nullptr;
}

/*
 *  Test for a directory 'slash' or colon.
 */

inline bool Is_dir_marker(char c) {
	return c == '/' || c == '\\' || c == ':';
}

/*
 *  New game directory was chosen.
 */

void on_choose_new_game_dir(
		const char* dir,
		gpointer    udata    // ->studio.
) {
	(static_cast<ExultStudio*>(udata))->create_new_game(dir);
}

void ExultStudio::create_new_game(const char* dir    // Directory for new game.
) {
	// Take basename as game name.
	const char* eptr = dir + strlen(dir) - 1;
	while (eptr >= dir && Is_dir_marker(*eptr)) {
		eptr--;
	}
	eptr++;
	const char* ptr = eptr - 1;
	for (; ptr >= dir; ptr--) {
		if (Is_dir_marker(*ptr)) {
			ptr++;
			break;
		}
	}
	if (ptr < dir || eptr - ptr <= 0) {
		EStudio::Alert("Can't find base game name in '%s'", dir);
		return;
	}
	const string gamestr(ptr, eptr - ptr);
	const string dirstr(dir);
	const string static_path = dirstr + "/static";
	if (U7exists(static_path)) {
		string msg("Directory '");
		msg += static_path;
		msg += "' already exists.\n";
		msg += "Files within may be overwritten.\n";
		msg += "Proceed?";
		if (prompt(msg.c_str(), "Yes", "No") != 0) {
			return;
		}
	}
	U7mkdir(dir, 0755);    // Create "game", "game/static",
	//   "game/patch".
	U7mkdir(static_path.c_str(), 0755);
	const string patch_path = dirstr + "/patch";
	U7mkdir(patch_path.c_str(), 0755);
	// Set .exult.cfg.
	string       d("config/disk/game/");
	const string gameconfig = d + gamestr;
	d                       = gameconfig + "/path";
	config->set(d.c_str(), dirstr, false);
	d = gameconfig + "/editing";    // We are editing.
	config->set(d.c_str(), "yes", true);
	d = gameconfig + "/title";
	config->set(d.c_str(), gamestr, false);
	string esdir;    // Get dir. for new files.
	config->value("config/disk/data_path", esdir, EXULT_DATADIR);
	esdir += "/estudio/new";
	DIR* dirrd = opendir(esdir.c_str());
	if (!dirrd) {
		EStudio::Alert("'%s' for initial data files not found", esdir.c_str());
	} else {
		dirent* entry;
		while ((entry = readdir(dirrd))) {
			char* fname = entry->d_name;
			// Ignore case of extension.
			if (!strcmp(fname, ".") || !strcmp(fname, "..")) {
				continue;
			}
			const string src  = esdir + '/' + fname;
			const string dest = static_path + '/' + fname;
			EStudio::Copy_file(src.c_str(), dest.c_str());
		}
		closedir(dirrd);
	}
	// Add new game to master list.
	gamemanager->add_game(gamestr, gamestr);
	set_game_path(gamestr);    // Open as current game.
	write_shape_info(true);    // Create initial .dat files.
}

/*
 *  Prompt for a 'new game' directory.
 */

void ExultStudio::new_game() {
	if (!okay_to_close()) {    // Okay to close prev. game?
		return;
	}
	Create_file_selection(
			"Choose new game directory", "<SAVEHOME>", nullptr, {},
			GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER, on_choose_new_game_dir,
			this);
}

C_EXPORT void on_gameselect_ok_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	// Get selected game:
	GtkTreeView* treeview
			= GTK_TREE_VIEW(studio->get_widget("gameselect_gamelist"));
	GtkTreePath*       path;
	GtkTreeViewColumn* col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	int           gamenum = -1;
	int           modnum  = -1;
	char*         text;
	GtkTreeIter   iter;
	GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, 0, &text, 1, &gamenum, -1);
	g_free(text);

	ModManager* game = gamemanager->get_game(gamenum);
	string      modtitle;

	GtkWidget* frame = studio->get_widget("modinfo_frame");
	if (gtk_widget_get_visible(GTK_WIDGET(frame))) {
		// Creating a new mod
		modtitle = studio->get_text_entry("gameselect_cfgname");
		if (modtitle.empty()) {
			return;
		}
		// Get the mod's Exult menu string.
		GtkTextBuffer* buff = gtk_text_view_get_buffer(
				GTK_TEXT_VIEW(studio->get_widget("gameselect_menustring")));
		GtkTextIter startpos;
		GtkTextIter endpos;
		gtk_text_buffer_get_bounds(buff, &startpos, &endpos);
		gchar*       modmenu = gtk_text_iter_get_text(&startpos, &endpos);
		const string modmenustr(convertFromUTF8(modmenu, "ASCII"));
		g_free(modmenu);
		if (modmenustr.empty()) {
			return;
		}
		// Ensure <MODS> dir exists
		string pathname("<" + game->get_path_prefix() + "_MODS>");
		to_uppercase(pathname);
		if (!U7exists(pathname)) {
			U7mkdir(pathname.c_str(), 0755);
		}
		// Create mod directories:
		pathname += "/" + modtitle;
		U7mkdir(pathname.c_str(), 0755);
		string d = pathname + "/patch";
		U7mkdir(d.c_str(), 0755);
		d = pathname + "/gamedat";
		U7mkdir(d.c_str(), 0755);
		// Create mod cfg file:
		const string  cfgfile = pathname + ".cfg";
		Configuration modcfg(cfgfile, "modinfo");
		modcfg.set("mod_info/display_string", modmenustr, true);
		modcfg.set("mod_info/required_version", VERSION, true);

		// Add mod to base game's list:
		game->add_mod(modtitle, cfgfile);
	} else {
		// Selecting a game/mod to load
		treeview = GTK_TREE_VIEW(studio->get_widget("gameselect_modlist"));
		gtk_tree_view_get_cursor(treeview, &path, &col);

		modnum = -1;
		if (path != nullptr) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

			gtk_tree_model_get_iter(model, &iter, path);
			gtk_tree_model_get(model, &iter, 0, &text, 1, &modnum, -1);
			g_free(text);
			gtk_tree_path_free(path);
		}
		ModInfo* mod = modnum > -1 ? game->get_mod(modnum) : nullptr;
		modtitle     = mod ? mod->get_mod_title() : "";
	}

	studio->set_game_path(game->get_cfgname(), modtitle);
	GtkWidget* win = studio->get_widget("game_selection");
	gtk_widget_set_visible(win, false);
}

/*
 *  A different game was chosen.
 */
C_EXPORT void on_gameselect_gamelist_cursor_changed(
		GtkTreeView* treeview, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	// Get selection info:
	GtkTreePath*       path;
	GtkTreeViewColumn* col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if (path == nullptr) {
		return;
	}
	int           gamenum = -1;
	char*         text;
	GtkTreeIter   gameiter;
	GtkTreeModel* gamemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	gtk_tree_model_get_iter(gamemodel, &gameiter, path);
	gtk_tree_model_get(gamemodel, &gameiter, 0, &text, 1, &gamenum, -1);
	g_free(text);

	ExultStudio* studio = ExultStudio::get_instance();
	GtkWidget*   win    = studio->get_widget("game_selection");
	gtk_window_set_modal(GTK_WINDOW(win), true);

	GtkWidget* mod_tree = studio->get_widget("gameselect_modlist");

	GtkTreeModel* oldmod = gtk_tree_view_get_model(GTK_TREE_VIEW(mod_tree));
	GtkTreeStore* model  = GTK_TREE_STORE(oldmod);
	gtk_tree_store_clear(model);

	std::vector<ModInfo>& mods = gamemanager->get_game(gamenum)->get_mod_list();
	GtkTreeIter           iter;

	gtk_tree_store_append(model, &iter, nullptr);
	gtk_tree_store_set(
			model, &iter, 0, "(unmodded game -- default if no mod is selected)",
			1, -1, -1);

	for (size_t j = 0; j < mods.size(); j++) {
		const ModInfo&          currmod = mods[j];
		string                  modname = currmod.get_menu_string();
		const string::size_type t       = modname.find('\n', 0);
		if (t != string::npos) {
			modname.replace(t, 1, " ");
		}

		// Titles need to be displayable in Exult menu, hence should not
		// have any extra characters.
		const string title(convertToUTF8(modname.c_str(), "ASCII"));
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(model, &iter, 0, title.c_str(), 1, j, -1);
	}
}

void fill_game_tree(GtkTreeView* treeview, int curr_game) {
	GtkTreeModel*            oldmod = gtk_tree_view_get_model(treeview);
	GtkTreeStore*            model  = GTK_TREE_STORE(oldmod);
	std::vector<ModManager>& games  = gamemanager->get_game_list();
	GtkTreeIter              iter;
	GtkTreePath*             path = nullptr;
	for (size_t j = 0; j < games.size(); j++) {
		const ModManager&       currgame = games[j];
		string                  gamename = currgame.get_menu_string();
		const string::size_type t        = gamename.find('\n', 0);
		if (t != string::npos) {
			gamename.replace(t, 1, " ");
		}

		// Titles need to be displayable in Exult menu, hence should not
		// have any extra characters.
		const string title(convertToUTF8(gamename.c_str(), "ASCII"));
		gtk_tree_store_append(model, &iter, nullptr);
		gtk_tree_store_set(model, &iter, 0, title.c_str(), 1, j, -1);
		if (j == size_t(curr_game)) {
			gtk_tree_selection_select_iter(
					gtk_tree_view_get_selection(treeview), &iter);
			path = gtk_tree_model_get_path(oldmod, &iter);
		}
	}
	if (!path) {    // Probably no game selected.
		return;
	}
	// Force the modlist to be updated:
	gtk_tree_view_set_cursor(treeview, path, nullptr, false);
	gtk_tree_path_free(path);
}

/*
 *  Choose game/mod to open.
 */
void ExultStudio::open_game_dialog(bool createmod) {
	GtkWidget* win = get_widget("game_selection");
	gtk_window_set_modal(GTK_WINDOW(win), true);

	g_signal_connect(
			G_OBJECT(get_widget("gameselect_ok")), "clicked",
			G_CALLBACK(on_gameselect_ok_clicked), nullptr);

	GtkWidget* dlg_list[2]
			= {get_widget("gameselect_gamelist"),
			   get_widget("gameselect_modlist")};

	/* create the store for both trees */
	for (auto* widg : dlg_list) {
		auto*         tree   = GTK_TREE_VIEW(widg);
		GtkTreeModel* oldmod = gtk_tree_view_get_model(tree);
		GtkTreeStore* model;
		if (oldmod) {
			model = GTK_TREE_STORE(oldmod);
		} else {    // Create the first time.
			model = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);
			gtk_tree_view_set_model(tree, GTK_TREE_MODEL(model));
			g_object_unref(model);
			gint             col_offset;
			GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

			/* column for game names */
			g_object_set(renderer, "xalign", 0.0, nullptr);
			col_offset = gtk_tree_view_insert_column_with_attributes(
					tree, -1, "Column1", renderer, "text", FOLDER_COLUMN,
					nullptr);
			GtkTreeViewColumn* column
					= gtk_tree_view_get_column(tree, col_offset - 1);
			gtk_tree_view_column_set_clickable(
					GTK_TREE_VIEW_COLUMN(column), true);
		}
		gtk_tree_store_clear(model);
	}

	if (!createmod) {
		g_signal_connect(
				G_OBJECT(dlg_list[0]), "cursor-changed",
				G_CALLBACK(on_gameselect_gamelist_cursor_changed), nullptr);
		set_visible("modlist_frame", true);
		set_visible("modinfo_frame", false);
	} else {
		set_visible("modinfo_frame", true);
		set_visible("modlist_frame", false);
	}
	fill_game_tree(GTK_TREE_VIEW(dlg_list[0]), curr_game);
	gtk_widget_set_visible(win, true);
}

/*
 *  Note:   If mod name is "", no mod is loaded
 */
void ExultStudio::set_game_path(const string& gamename, const string& modname) {
	// Finish up external edits.
	Shape_chooser::clear_editing_files();

	curr_game = curr_mod = -1;

	ModManager* basegame = nullptr;
	if (gamename == CFG_BG_NAME) {
		if (!(basegame = gamemanager->get_bg())) {
			cerr << "Black Gate not found." << endl;
		}
	} else if (gamename == CFG_FOV_NAME) {
		if (!(basegame = gamemanager->get_fov())) {
			cerr << "Forge of Virtue not found." << endl;
		}
	} else if (gamename == CFG_SI_NAME) {
		if (!(basegame = gamemanager->get_si())) {
			cerr << "Serpent Isle not found." << endl;
		}
	} else if (gamename == CFG_SS_NAME) {
		if (!(basegame = gamemanager->get_ss())) {
			cerr << "Silver Seed not found." << endl;
		}
	} else {
		if ((curr_game = gamemanager->find_game_index(gamename)) < 0) {
			cerr << "Game '" << gamename << "' not found." << endl;
		} else {
			basegame = gamemanager->get_game(curr_game);
		}
	}

	if (!basegame) {
		string dlg = "Exult Studio could not find the specified game: '";
		dlg += gamename + "'.\n\nDo you wish to ";
		if (gamemanager->get_game_count() > 0) {
			dlg += "browse for a different game, ";
		}
		dlg += "create a new game or exit?";
		const int choice
				= (gamemanager->get_game_count() > 0)
						  ? prompt(dlg.c_str(), "Browse", "Create New", "Quit")
						  : (prompt(dlg.c_str(), "Create New", "Quit") + 1);
		if (choice == 0) {
			open_game_dialog(false);
			return;
		} else if (choice == 1) {
			new_game();
			return;
		} else {
			exit(1);
		}
	}

	if (curr_game < 0 && basegame) {
		curr_game = gamemanager->find_game_index(basegame->get_cfgname());
	}
	BaseGameInfo* gameinfo = nullptr;
	curr_mod = modname.empty() ? -1 : basegame->find_mod_index(modname);
	if (curr_mod > -1) {
		gameinfo = basegame->get_mod(curr_mod);
	} else {
		gameinfo = basegame;
	}

	game_encoding = gameinfo->get_codepage();

	gameinfo->setup_game_paths();
	game_type = gameinfo->get_game_type();
	expansion = gameinfo->have_expansion();
	sibeta    = gameinfo->is_si_beta();
	if (static_path) {
		g_free(static_path);
	}
	// Set up path to static.
	static_path             = g_strdup(get_system_path("<STATIC>").c_str());
	const string patch_path = get_system_path("<PATCH>");
	if (!U7exists(patch_path)) {    // Create patch if not there.
		U7mkdir(patch_path.c_str(), 0755);
	}
	// Reset EVERYTHING.
	// Clear file cache!
	U7FileManager::get_ptr()->reset();
	palbuf.reset();    // Delete old.
	delete files;      // Close old shape files.
	Free_text();       // Delete old names.
	// These were owned by 'files':
	curfile             = nullptr;
	browser             = nullptr;
	shape_info_modified = shape_names_modified = npc_modified = false;
	for (auto* win : group_windows) {
		GtkWidget* grpwin = GTK_WIDGET(win);
		auto*      xml    = static_cast<GtkBuilder*>(
                g_object_get_data(G_OBJECT(grpwin), "xml"));
		auto* chooser = static_cast<Object_browser*>(
				g_object_get_data(G_OBJECT(grpwin), "browser"));
		delete chooser;
		gtk_widget_destroy(grpwin);
		g_object_unref(G_OBJECT(xml));
	}
	group_windows.clear();
	close_obj_window();
	close_cont_window();
	close_barge_window();
	close_egg_window();
	close_npc_window();
	close_shape_window();
	close_equip_window();
	close_combo_window();
	close_compile_window();
	if (gameinfowin) {
		gtk_widget_set_visible(gameinfowin, false);
	}

	gtk_notebook_prev_page(mainnotebook);
	const U7multiobject palobj(PALETTES_FLX, PATCH_PALETTES, 0);
	size_t              len;
	palbuf = palobj.retrieve(len);
	if (!palbuf || !len) {
		// No palette file, so create fake.
		// Just in case.
		palbuf = make_unique<unsigned char[]>(
				3 * 256);    // How about all white?
		memset(palbuf.get(), 63, 3 * 256);
	}
	// Set background color.
	palbuf[3 * 255]     = (background_color >> 18) & 0x3f;
	palbuf[3 * 255 + 1] = (background_color >> 10) & 0x3f;
	palbuf[3 * 255 + 2] = (background_color >> 2) & 0x3f;
	files               = new Shape_file_set();
	vgafile             = open_shape_file("shapes.vga");
	facefile            = open_shape_file("faces.vga");
	fontfile            = open_shape_file("fonts.vga");
	gumpfile            = open_shape_file("gumps.vga");
	spritefile          = open_shape_file("sprites.vga");
	if (game_type == SERPENT_ISLE) {
		paperdolfile = open_shape_file("paperdol.vga");
	}
	Setup_text(
			game_type == SERPENT_ISLE, expansion,
			sibeta);    // Read in shape names.
	misc_name_map.clear();
	for (int i = 0; i < get_num_misc_names(); i++) {
		if (get_misc_name(i) != nullptr) {
			misc_name_map.insert(
					std::pair<string, int>(string(get_misc_name(i)), i));
		}
	}
	setup_file_list();           // Set up file-list window.
	set_browser("", nullptr);    // No browser.
	connect_to_server();         // Connect to server with 'gamedat'.
}

/*  Note:  Args after extcnt are in (name, file_type) pairs.    */
void add_to_tree(
		GtkTreeStore* model, const char* folderName, const char* files,
		ExultFileTypes file_type, int extra_cnt, ...) {
	dirent*     entry;
	GtkTreeIter iter;

	// First, we add the folder
	gtk_tree_store_append(model, &iter, nullptr);
	gtk_tree_store_set(
			model, &iter, FOLDER_COLUMN, folderName, FILE_COLUMN, nullptr,
			DATA_COLUMN, -1, -1);

	// Scan the files which are separated by commas
	GtkTreeIter child_iter;
	const char* startpos        = files;
	int         adding_children = 1;
	do {
		char*       pattern;
		const char* commapos = strstr(startpos, ",");
		if (commapos == nullptr) {
			pattern         = g_strdup(startpos);
			adding_children = 0;
		} else {
			pattern  = g_strndup(startpos, commapos - startpos);
			startpos = commapos + 1;
		}

		const string spath("<STATIC>");
		const string ppath("<PATCH>");
		const char*  ext = strstr(pattern, "*");
		if (!ext) {
			ext = pattern;
		} else {
			ext++;
		}
		DIR* dir = U7opendir(ppath.c_str());    // Get names from 'patch' first.
		if (dir) {
			while ((entry = readdir(dir))) {
				char*     fname = entry->d_name;
				const int flen  = strlen(fname);
				// Ignore case of extension.
				if (!strcmp(fname, ".") || !strcmp(fname, "..")
					|| strcasecmp(fname + flen - strlen(ext), ext) != 0) {
					continue;
				}
				gtk_tree_store_append(model, &child_iter, &iter);
				gtk_tree_store_set(
						model, &child_iter, FOLDER_COLUMN, nullptr, FILE_COLUMN,
						fname, DATA_COLUMN, file_type, -1);
			}
			closedir(dir);
		}
		dir = U7opendir(spath.c_str());    // Now go through 'static'.
		if (dir) {
			while ((entry = readdir(dir))) {
				char*     fname = entry->d_name;
				const int flen  = strlen(fname);
				// Ignore case of extension.
				if (!strcmp(fname, ".") || !strcmp(fname, "..")
					|| strcasecmp(fname + flen - strlen(ext), ext) != 0) {
					continue;
				}
				// Filter out 'font0000.vga' file as it is apparently
				// from an old origin screensaver.
				if (!strcasecmp(fname, "font0000.vga")) {
					continue;
				}
				// See if also in 'patch'.
				string fullpath(ppath);
				string lowfname(fname);
				for (auto& chr : lowfname) {
					chr = static_cast<char>(
							std::tolower(static_cast<unsigned char>(chr)));
				}
				fullpath += "/";
				fullpath += lowfname;
				if (U7exists(fullpath)) {
					continue;
				}
				gtk_tree_store_append(model, &child_iter, &iter);
				gtk_tree_store_set(
						model, &child_iter, FOLDER_COLUMN, nullptr, FILE_COLUMN,
						fname, DATA_COLUMN, file_type, -1);
			}
			closedir(dir);
		}

		g_free(pattern);
	} while (adding_children);

	std::va_list ap;
	va_start(ap, extra_cnt);
	while (extra_cnt--) {
		char*     nm = va_arg(ap, char*);
		const int ty = va_arg(ap, int);
		gtk_tree_store_append(model, &child_iter, &iter);
		gtk_tree_store_set(
				model, &child_iter, FOLDER_COLUMN, nullptr, FILE_COLUMN, nm,
				DATA_COLUMN, ty, -1);
	}
	va_end(ap);
}

/*
 *  Set up the file list to the left of the main window.
 */

void ExultStudio::setup_file_list() {
	GtkWidget* file_list = get_widget("file_list");

	/* create tree store */
	GtkTreeModel* oldmod = gtk_tree_view_get_model(GTK_TREE_VIEW(file_list));
	GtkTreeStore* model;
	if (oldmod) {
		model = GTK_TREE_STORE(oldmod);
	} else {    // Create the first time.
		model = gtk_tree_store_new(
				NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
		gtk_tree_view_set_model(
				GTK_TREE_VIEW(file_list), GTK_TREE_MODEL(model));
		g_object_unref(model);
		gint               col_offset;
		GtkCellRenderer*   renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* column;

		/* column for folder names */
		g_object_set(renderer, "xalign", 0.0, nullptr);
		col_offset = gtk_tree_view_insert_column_with_attributes(
				GTK_TREE_VIEW(file_list), -1, "Folders", renderer, "text",
				FOLDER_COLUMN, nullptr);
		column = gtk_tree_view_get_column(
				GTK_TREE_VIEW(file_list), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), true);
		/* column for file names */
		col_offset = gtk_tree_view_insert_column_with_attributes(
				GTK_TREE_VIEW(file_list), -1, "Files", renderer, "text",
				FILE_COLUMN, nullptr);
		column = gtk_tree_view_get_column(
				GTK_TREE_VIEW(file_list), col_offset - 1);
		gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), true);
	}
	gtk_tree_store_clear(model);
	add_to_tree(
			model, "Shape Files", "*.vga,*.shp", ShapeArchive, 1, "combos.flx",
			ComboArchive);
	add_to_tree(
			model, "Map Files", "u7chunks", ChunksArchive, 1, "npcs",
			NpcsArchive);
	add_to_tree(model, "Palette Files", "*.pal,palettes.flx", PaletteFile, 0);

	// Expand all entries.
	gtk_tree_view_expand_all(GTK_TREE_VIEW(file_list));
	gtk_tree_selection_set_mode(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(file_list)),
			GTK_SELECTION_SINGLE);
}

/*
 *  Save all the files we've changed:
 *      Object browser.
 *      Group files.
 *      Shape files.
 *      Shape info (tfa.dat, weapons.dat, etc.)
 *      Game map.
 */

void ExultStudio::save_all() {
	save_groups();    // Group files.
	if (files) {
		// Get back any changed images.
		Shape_chooser::check_editing_files();
		files->flush();    // Write out the .vga files.
	}
	write_shape_info();
	if (need_to_save()) {
		write_map();
	}
}

/*
 *  Any files need saving?
 */

bool ExultStudio::need_to_save() {
	if (groups_modified()) {
		return true;
	}
	if (files && files->is_modified()) {
		return true;
	}
	if (shape_info_modified || shape_names_modified) {
		return true;
	}
	if (npc_modified) {
		return true;
	}
	// Ask Exult about the map.
	if (Send_data(server_socket, Exult_server::info) != -1) {
		// Should get immediate answer.
		unsigned char          data[Exult_server::maxlength];
		Exult_server::Msg_type id;
		Exult_server::wait_for_response(server_socket, 100);
		const int len = Exult_server::Receive_data(
				server_socket, id, data, sizeof(data));
		int  vers;
		int  edlift;
		int  hdlift;
		int  edmode;
		bool editing;
		bool grid;
		bool mod;
		if (id == Exult_server::info
			&& Game_info_in(
					data, len, vers, edlift, hdlift, editing, grid, mod, edmode)
			&& mod) {
			return true;
		}
	}
	return false;
}

/*
 *  Write out map.
 */

void ExultStudio::write_map() {
	npc_modified = false;
	send_to_server(Exult_server::write_map);
}

/*
 *  Read in map.
 */

void ExultStudio::read_map() {
	send_to_server(Exult_server::read_map);
}

/*
 *  Write out shape info. and text.
 */

void ExultStudio::write_shape_info(bool force    // If set, always write.
) {
	if ((force || shape_info_modified) && vgafile) {
		auto* svga = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
		// Make sure data's been read in.
		svga->read_info(game_type, true);
		svga->write_info(game_type);
		// Tell Exult to reload.
		unsigned char  buf[Exult_server::maxlength];
		unsigned char* ptr    = &buf[0];
		ExultStudio*   studio = ExultStudio::get_instance();
		studio->send_to_server(
				Exult_server::reload_shapes_info, buf, ptr - buf);
	}
	shape_info_modified = false;
	if (force || shape_names_modified) {
		shape_names_modified = false;
		Write_text_file();
	}
}

/*
 *  Reload usecode.
 */

void ExultStudio::reload_usecode() {
	send_to_server(Exult_server::reload_usecode);
}

/*
 *  Tell Exult to start/stop playing.
 */

void ExultStudio::set_play(gboolean play    // True to play, false to edit.
) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, play ? 0 : 1);    // Map_edit = !play.
	send_to_server(Exult_server::map_editing_mode, data, ptr - data);
}

/*
 *  Tell Exult to show or not show the tile grid.
 */

void ExultStudio::set_tile_grid(gboolean grid    // True to show.
) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, grid ? 1 : 0);    // Map_edit = !play.
	send_to_server(Exult_server::tile_grid, data, ptr - data);
}

/*
 *  Tell Exult to edit at a given lift.
 */

void ExultStudio::set_edit_lift(int lift) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, lift);
	send_to_server(Exult_server::edit_lift, data, ptr - data);
}

/*
 *  Tell Exult to hide objects at or above a given lift.
 */

void ExultStudio::set_hide_lift(int lift) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, lift);
	send_to_server(Exult_server::hide_lift, data, ptr - data);
}

/*
 *  Tell Exult to enter/leave 'terrain-edit' mode.
 */

void ExultStudio::set_edit_terrain(gboolean terrain    // True/false
) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(
			ptr, terrain ? 1 : 0);    // NOTE:  Pass -1 to abort.  But I
	//   haven't got an interface yet.
	send_to_server(Exult_server::terrain_editing_mode, data, ptr - data);
	if (!terrain) {
		// Turning it off.
		if (browser) {
			browser->end_terrain_editing();
		}
		// FOR NOW, skip_lift is reset.
		set_spin("hide_lift_spin", 255, true);
	} else {    // Disable "Hide lift".
		set_sensitive("hide_lift_spin", false);
	}
	// Set edit-mode to paint.
	GtkWidget* mitem = get_widget(terrain ? "paint1" : "move1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem), true);
}

void ExultStudio::set_edit_mode(int md    // 0-2 (drag, paint, pick.
) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	little_endian::Write2(ptr, md);
	//   haven't got an interface yet.
	send_to_server(Exult_server::set_edit_mode, data, ptr - data);
}

/*
 *  Insert 0-delimited text into an edit box.
 */

static void Insert_text(
		GtkEditable* ed, const char* text,
		int& pos    // Where to insert.  Updated.
) {
	gtk_editable_insert_text(ed, text, strlen(text), &pos);
}

/*
 *  Show unused shape list received from exult.
 */

void ExultStudio::show_unused_shapes(
		const unsigned char* data,      // Bits set for unused shapes.
		int                  datalen    // #bytes.
) {
	const int      nshapes = datalen * 8;
	GtkTextView*   text    = GTK_TEXT_VIEW(get_widget("msg_text"));
	GtkTextBuffer* buffer  = gtk_text_view_get_buffer(text);
	gtk_text_buffer_set_text(buffer, "", 0);    // Clear out old text
	set_visible("msg_win", true);               // Show message window.
	int          pos = 0;
	GtkEditable* ed  = GTK_EDITABLE(text);
	Insert_text(ed, "The following shapes were not found.\n", pos);
	Insert_text(
			ed, "Note that some may be created by usecode (script)\n\n", pos);
	// Ignore flats (<0x96).
	for (int i = c_first_obj_shape; i < nshapes; i++) {
		if (!(data[i / 8] & (1 << (i % 8)))) {
			const char* nm = get_shape_name(i);
			char*       msg
					= g_strdup_printf("  Shape %4d:    %s\n", i, nm ? nm : "");
			Insert_text(ed, msg, pos);
			g_free(msg);
		}
	}
	// FIXME: gtk_text_set_point(text, 0);  // Scroll back to top.
}

/*
 *  Open a shape (or chunks) file in 'patch' or 'static' directory.
 *
 *  Output: ->file info (may already have existed), or 0 if error.
 */

Shape_file_info* ExultStudio::open_shape_file(
		const char* basename    // Base name of file to open.
) {
	Shape_file_info* info = files->create(basename);
	if (!info) {
		EStudio::Alert(
				"'%s' not found in static or patch directories", basename);
	}
	return info;
}

/*
 *  Prompt for a new shape file name.
 */

void ExultStudio::new_shape_file(bool single    // Not a FLEX file.
) {
	Create_file_selection(
			single ? "Write new .shp file" : "Write new .vga file", "<PATCH>",
			single ? "Shape files" : "VGA files", {single ? "*.shp" : "*.vga"},
			GTK_FILE_CHOOSER_ACTION_SAVE, create_shape_file,
			reinterpret_cast<gpointer>(uintptr_t(single)));
}

/*
 *  Create a new shape/shapes file.
 */

void ExultStudio::create_shape_file(
		const char* pathname,    // Full path.
		gpointer    udata        // 1 if NOT a FLEX file.
) {
	const bool oneshape = reinterpret_cast<uintptr>(udata) != 0;
	try {                  // Write file.
		if (oneshape) {    // Single-shape?
			// Create one here.
			const int     w = c_tilesize;
			const int     h = c_tilesize;
			unsigned char pixels[w * h];    // Create an 8x8 shape.
			memset(pixels, 1, w * h);       // Just use color #1.
			Shape shape(
					make_unique<Shape_frame>(pixels, w, h, w - 1, h - 1, true));
			Shape* ptr = &shape;
			Image_file_info::write_file(pathname, &ptr, 1, true);
		} else {
			Image_file_info::write_file(pathname, nullptr, 0, false);
		}
	} catch (const exult_exception& e) {
		EStudio::Alert("%s", e.what());
	}
	ExultStudio* studio = ExultStudio::get_instance();
	studio->setup_file_list();    // Rescan list of shape files.
}

/*
 *  Get value of a toggle button (false if not found).
 */

bool ExultStudio::get_toggle(const char* name) {
	GtkWidget* btn = get_widget(name);
	assert(btn);
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
}

/*
 *  Find and set a toggle/checkbox button.
 */

void ExultStudio::set_toggle(const char* name, bool val, bool sensitive) {
	GtkWidget* btn = get_widget(name);
	if (btn) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), val);
		gtk_widget_set_sensitive(btn, sensitive);
	}
}

/*
 *  Get an 8-bit set of flags from a group of toggles.
 */

unsigned int ExultStudio::get_bit_toggles(
		tcb::span<const char* const> names    // Names for bit 0, 1, 2,...
) {
	unsigned int bits = 0;
	for (size_t i = 0; i < names.size(); i++) {
		bits |= (get_toggle(names[i]) ? 1 : 0) << i;
	}
	return bits;
}

/*
 *  Set a group of toggles based on a sequential (0, 1, 2...) set of bit
 *  flags.
 */

void ExultStudio::set_bit_toggles(
		tcb::span<const char* const> names,    // Names for bit 0, 1, 2,...
		unsigned int                 bits) {
	for (size_t i = 0; i < names.size(); i++) {
		set_toggle(names[i], (bits & (1 << i)) != 0);
	}
}

/*
 *  Get value of option-menu button (-1 if unsuccessful).
 */

int ExultStudio::get_optmenu(const char* name) {
	GtkWidget* btn = get_widget(name);
	assert(btn);
	return gtk_combo_box_get_active(GTK_COMBO_BOX(btn));
}

/*
 *  Find and set an option-menu button.
 */

void ExultStudio::set_optmenu(const char* name, int val, bool sensitive) {
	GtkWidget* btn = get_widget(name);
	if (btn) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(btn), val);
		gtk_widget_set_sensitive(btn, sensitive);
	}
}

/*
 *  Get value of spin button (-1 if unsuccessful).
 */

int ExultStudio::get_spin(const char* name) {
	GtkWidget* btn = get_widget(name);
	assert(btn);
	return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(btn));
}

/*
 *  Find and set a spin button.
 */

void ExultStudio::set_spin(const char* name, int val, bool sensitive) {
	GtkWidget* btn = get_widget(name);
	if (btn) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(btn), val);
		gtk_widget_set_sensitive(btn, sensitive);
	}
}

/*
 *  Find and set a spin button's range.
 */

void ExultStudio::set_spin(
		const char* name, int low, int high    // Range.
) {
	GtkWidget* btn = get_widget(name);
	if (btn) {
		gtk_spin_button_set_adjustment(
				GTK_SPIN_BUTTON(btn),
				GTK_ADJUSTMENT(gtk_adjustment_new(0, low, high, 1, 10, 0)));
	}
}

/*
 *  Find and set a spin button, along with its range.
 */

void ExultStudio::set_spin(
		const char* name, int val, int low, int high    // Range.
) {
	GtkWidget* btn = get_widget(name);
	if (btn) {
		gtk_spin_button_set_adjustment(
				GTK_SPIN_BUTTON(btn),
				GTK_ADJUSTMENT(gtk_adjustment_new(0, low, high, 1, 10, 0)));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(btn), val);
	}
}

/*
 *  Get number from a text field.
 *
 *  Output: Number, or -1 if not found.
 */

int ExultStudio::get_num_entry(const char* name) {
	GtkWidget* field = get_widget(name);
	return get_num_entry(field, 0);
}

int ExultStudio::get_num_entry(GtkWidget* field, int if_empty) {
	assert(field);
	const gchar* txt = gtk_entry_get_text(GTK_ENTRY(field));
	if (!txt) {
		return -1;
	}
	while (*txt == ' ') {
		txt++;    // Skip space.
	}
	if (!*txt) {
		return if_empty;
	}
	if (txt[0] == '0' && txt[1] == 'x') {
		return static_cast<int>(strtoul(txt + 2, nullptr, 16));    // Hex.
	} else {
		return atoi(txt);
	}
}

/*
 *  Get text from a text field.
 *
 *  Output: ->text, or null if not found.
 */

const gchar* ExultStudio::get_text_entry(const char* name) {
	GtkWidget* field = get_widget(name);
	if (!field) {
		return nullptr;
	}
	return gtk_entry_get_text(GTK_ENTRY(field));
}

/*
 *  Find and set a text field to a number.
 */

void ExultStudio::set_entry(
		const char* name, int val, bool hex, bool sensitive) {
	GtkWidget* field = get_widget(name);
	if (field) {
		char* txt = hex ? g_strdup_printf("0x%x", val)
						: g_strdup_printf("%d", val);
		gtk_entry_set_text(GTK_ENTRY(field), txt);
		g_free(txt);
		gtk_widget_set_sensitive(field, sensitive);
	}
}

/*
 *  Set text field.
 */

void ExultStudio::set_entry(const char* name, const char* val, bool sensitive) {
	GtkWidget* field = get_widget(name);
	if (field) {
		gtk_entry_set_text(GTK_ENTRY(field), val);
		gtk_widget_set_sensitive(field, sensitive);
	}
}

/*
 *  Set statusbar.
 *  Output: Msg. ID, or 0.
 */

guint ExultStudio::set_statusbar(
		const char* name, int context, const char* msg) {
	GtkWidget* sbar = get_widget(name);
	if (sbar) {
		return gtk_statusbar_push(GTK_STATUSBAR(sbar), context, msg);
	} else {
		return 0;
	}
}

/*
 *  Pop last message that was set.
 */

void ExultStudio::remove_statusbar(const char* name, int context, guint msgid) {
	if (msgid == 0) {
		return;
	}
	GtkWidget* sbar = get_widget(name);
	if (sbar) {
		return gtk_statusbar_remove(GTK_STATUSBAR(sbar), context, msgid);
	}
}

/*
 *  Set a button's text.
 */

void ExultStudio::set_button(const char* name, const char* text) {
	GtkWidget* btn   = get_widget(name);
	GtkLabel*  label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(btn)));
	gtk_label_set_text(label, text);
}

/*
 *  Show/hide a widget.
 */
C_EXPORT gboolean exult_widget_hide(GtkWidget* w) {
	gtk_widget_set_visible(w, false);
	return true;
}

void ExultStudio::set_visible(const char* name, bool vis) {
	GtkWidget* widg = get_widget(name);
	if (widg) {
		if (vis) {
			gtk_widget_set_visible(widg, true);
		} else {
			gtk_widget_set_visible(widg, false);
		}
	}
}

/*
 *  Enable/disable a widget.
 */

void ExultStudio::set_sensitive(const char* name, bool tf) {
	GtkWidget* widg = get_widget(name);
	if (widg) {
		gtk_widget_set_sensitive(widg, tf);
	}
}

/*
 *  Handle 'prompt' dialogs:
 */
static int prompt_choice = 0;    // Gets prompt choice.

C_EXPORT void on_prompt3_yes_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	prompt_choice = 0;
}

C_EXPORT void on_prompt3_no_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	prompt_choice = 1;
}

C_EXPORT void on_prompt3_cancel_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	prompt_choice = 2;
}

/*
 *  Prompt for one of two/three answers.
 *
 *  Output: 0 for 1st choice, 1 for 2nd, 2 for 3rd.
 */

int ExultStudio::prompt(
		const char* msg,        // Question to ask.
		const char* choice0,    // 1st choice.
		const char* choice1,    // 2nd choice, or nullptr.
		const char* choice2     // 3rd choice, or nullptr.
) {
	static GtkWidget* dlg = nullptr;
	if (!dlg) {    // First time?
		dlg                    = get_widget("prompt3_dialog");
		GdkPixbuf* logo_pixbuf = gdk_pixbuf_new_from_xpm_data(
				static_cast<const gchar**>(logo_xpm));
		GtkWidget* draw = gtk_image_new_from_pixbuf(logo_pixbuf);
		g_object_unref(logo_pixbuf);
		GtkWidget* hbox = get_widget("prompt3_hbox");
		gtk_widget_set_size_request(
				draw, gdk_pixbuf_get_width(logo_pixbuf),
				gdk_pixbuf_get_height(logo_pixbuf));
		gtk_widget_set_visible(draw, true);
		gtk_box_pack_start(GTK_BOX(hbox), draw, false, false, 12);
		// Make logo show to left.
		gtk_box_reorder_child(GTK_BOX(hbox), draw, 0);
	}
	gtk_label_set_text(GTK_LABEL(get_widget("prompt3_label")), msg);
	set_button("prompt3_yes", choice0);
	if (choice1) {
		set_button("prompt3_no", choice1);
		set_visible("prompt3_no", true);
	} else {
		set_visible("prompt3_no", false);
	}
	if (choice2) {    // 3rd choice?  Show btn if so.
		set_button("prompt3_cancel", choice2);
		set_visible("prompt3_cancel", true);
	} else {
		set_visible("prompt3_cancel", false);
	}
	prompt_choice = -1;
	gtk_window_set_modal(GTK_WINDOW(dlg), true);
	gtk_widget_set_visible(dlg, true);    // Should be modal.
	while (prompt_choice == -1) {         // Spin.
		gtk_main_iteration();             // (Blocks).
	}
	gtk_widget_set_visible(dlg, false);
	assert(prompt_choice >= 0 && prompt_choice <= 2);
	return prompt_choice;
}

namespace EStudio {

	/*
	 *  Same as ExultStudio::prompt, but as a routine.
	 */
	int Prompt(
			const char* msg,        // Question to ask.
			const char* choice0,    // 1st choice.
			const char* choice1,    // 2nd choice, or nullptr.
			const char* choice2     // 3rd choice, or nullptr.
	) {
		return ExultStudio::get_instance()->prompt(
				msg, choice0, choice1, choice2);
	}

	/*
	 *  Just print a message (usually an error).
	 */
	void Alert(
			const char* msg,    // May be in printf format.
			...) {
		std::va_list args;
		va_start(args, msg);
		char* fullmsg = g_strdup_vprintf(msg, args);
		va_end(args);
		Prompt(fullmsg, "Okay");
		g_free(fullmsg);
	}

	/*
	 *  Add a menu item to a menu.
	 *
	 *  Output: Menu item.
	 */

	GtkWidget* Add_menu_item(
			GtkWidget*  menu,         // Menu to add to.
			const char* label,        // What to put.  nullptr for separator.
			GCallback   func,         // Handle menu choice.
			gpointer    func_data,    // Data passed to func().
			GSList*     group         // If a radio menu item is wanted.
	) {
		GtkWidget* mitem = group ? (label ? gtk_radio_menu_item_new_with_label(
													group, label)
										  : gtk_radio_menu_item_new(group))
								 : (label ? gtk_menu_item_new_with_label(label)
										  : gtk_menu_item_new());
		gtk_widget_set_visible(mitem, true);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
		if (!label) {    // Want separator?
			gtk_widget_set_sensitive(mitem, false);
		}
		if (func) {    // Function?
			g_signal_connect(G_OBJECT(mitem), "activate", func, func_data);
		}
		return mitem;
	}

	/*
	 *  Create an arrow button.
	 */

	GtkWidget* Create_arrow_button(
			GtkArrowType dir,         // Direction.
			GCallback    clicked,     // Call this when clicked.
			gpointer     func_data    // Passed to 'clicked'.
	) {
		GtkWidget* btn = gtk_button_new();
		GtkWidget* img = gtk_image_new_from_icon_name(
				dir == GTK_ARROW_UP ? "go-up" : "go-down",
				GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image(GTK_BUTTON(btn), img);
		gtk_widget_set_visible(btn, true);
		g_signal_connect(G_OBJECT(btn), "clicked", clicked, func_data);
		return btn;
	}

	/*
	 *  Copy a file and show an alert box if unsuccessful.
	 */

	bool Copy_file(const char* src, const char* dest) {
		try {
			U7copy(src, dest);
		} catch (exult_exception& e) {
			EStudio::Alert("%s", e.what());
			return false;
		}
		return true;
	}

}    // namespace EStudio

/*
 *  'Preferences' window events.
 */

C_EXPORT void on_prefs_cancel_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	gtk_widget_set_visible(gtk_widget_get_toplevel(GTK_WIDGET(button)), false);
}

C_EXPORT void on_prefs_apply_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->save_preferences();
}

C_EXPORT void on_prefs_okay_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio::get_instance()->save_preferences();
	gtk_widget_set_visible(gtk_widget_get_toplevel(GTK_WIDGET(button)), false);
}

/*
 *  'Okay' was hit in the color selector.
 */

C_EXPORT void on_prefs_background_choose_clicked(
		GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	GtkColorChooserDialog* colorsel = GTK_COLOR_CHOOSER_DIALOG(
			gtk_color_chooser_dialog_new("Background color", nullptr));
	g_object_set(colorsel, "show-editor", true, nullptr);
	gtk_window_set_modal(GTK_WINDOW(colorsel), true);
	// Get color.
	const guint32 c = ExultStudio::get_instance()->get_background_color();
	GdkRGBA       rgba;
	rgba.red   = ((c >> 16) & 0xff) / 255.0;
	rgba.green = ((c >> 8) & 0xff) / 255.0;
	rgba.blue  = ((c >> 0) & 0xff) / 255.0;
	rgba.alpha = 1.0;
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
	if (gtk_dialog_run(GTK_DIALOG(colorsel)) == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(colorsel), &rgba);
		auto         r      = static_cast<unsigned char>(rgba.red * 255.0);
		auto         g      = static_cast<unsigned char>(rgba.green * 255.0);
		auto         b      = static_cast<unsigned char>(rgba.blue * 255.0);
		ExultStudio* studio = ExultStudio::get_instance();
		studio->set_background_color((r << 16) + (g << 8) + b);
		// Show new color.
		GtkWidget* backgrnd = studio->get_widget("prefs_background");
		g_object_set_data(
				G_OBJECT(backgrnd), "user_data",
				reinterpret_cast<gpointer>(
						uintptr(studio->get_background_color())));
		gtk_widget_queue_draw(backgrnd);
	}
	gtk_widget_destroy(GTK_WIDGET(colorsel));
}

// Background color area exposed.

gboolean ExultStudio::on_prefs_background_expose_event(
		GtkWidget* widget,    // The draw area.
		cairo_t*   cairo,
		gpointer   data    // -> ExultStudio.
) {
	ignore_unused_variable_warning(widget, data);
	auto         color = static_cast<guint32>(reinterpret_cast<uintptr>(
            g_object_get_data(G_OBJECT(widget), "user_data")));
	GdkRectangle area  = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	cairo_set_source_rgb(
			cairo, ((color >> 16) & 255) / 255.0, ((color >> 8) & 255) / 255.0,
			(color & 255) / 255.0);
	cairo_rectangle(cairo, area.x, area.y, area.width, area.height);
	cairo_fill(cairo);
	return true;
}

// X at top of window.
C_EXPORT gboolean on_prefs_window_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(event, user_data);
	gtk_widget_set_visible(widget, false);
	return true;
}

/*
 *  Reload CSS.
 */

void ExultStudio::reload_css() {
	GError* error = nullptr;
	if (!gtk_css_provider_load_from_path(css_provider, css_path, &error)) {
		cerr << "Couldn't reload the CSS file: " << error->message << endl;
		prompt("Failed to reload the CSS file.\n"
			   "ExultStudio proceeds without it. You can fix it and Reload it.",
			   "OK");
		g_error_free(error);
	}
}

/*
 *  Open preferences window.
 */

void ExultStudio::open_preferences() {
	set_entry("prefs_image_editor", image_editor ? image_editor : "");
	set_entry("prefs_default_game", default_game ? default_game : "");
	GtkWidget* backgrnd = get_widget("prefs_background");
	g_object_set_data(
			G_OBJECT(backgrnd), "user_data",
			reinterpret_cast<gpointer>(uintptr(background_color)));
	GtkWidget* win = get_widget("prefs_window");
	g_signal_connect(
			G_OBJECT(get_widget("prefs_background")), "draw",
			G_CALLBACK(on_prefs_background_expose_event), this);
	gtk_widget_set_visible(win, true);
}

/*
 *  Save preferences.
 */

void ExultStudio::save_preferences() {
	const char* text = get_text_entry("prefs_image_editor");
	g_free(image_editor);
	image_editor = g_strdup(text);
	config->set("config/estudio/image_editor", image_editor, true);
	text = get_text_entry("prefs_default_game");
	g_free(default_game);
	default_game = g_strdup(text);
	config->set("config/estudio/default_game", default_game, true);
	GtkWidget* backgrnd = get_widget("prefs_background");
	set_background_color(reinterpret_cast<uintptr>(
			g_object_get_data(G_OBJECT(backgrnd), "user_data")));
	config->set("config/estudio/background_color", background_color, true);
	// Set background color.
	palbuf[3 * 255]     = (background_color >> 18) & 0x3f;
	palbuf[3 * 255 + 1] = (background_color >> 10) & 0x3f;
	palbuf[3 * 255 + 2] = (background_color >> 2) & 0x3f;
	if (browser) {    // Repaint browser.
		browser->set_background_color(background_color);
	}
}

/*
 *  Main routine.
 */
void ExultStudio::run() {
	gtk_main();
}

/*
 *  This is called every few seconds to try to reconnect to Exult.
 */

static gint Reconnect(gpointer data    // ->ExultStudio.
) {
	auto* studio = static_cast<ExultStudio*>(data);
	if (studio->connect_to_server()) {
		return 0;    // Cancel timer.  We succeeded.
	} else {
		return 1;
	}
}

/*
 *  Send message to server.
 *
 *  Output: false if error sending (reported).
 */

bool ExultStudio::send_to_server(
		Exult_server::Msg_type id, unsigned char* data, int datalen) {
	if (Send_data(server_socket, id, data, datalen) == -1) {
		cerr << "Error sending to server" << endl;
		return false;
	}
	return true;
}

/*
 *  Input from server is available.
 */

#ifndef _WIN32
static gboolean Read_from_server(
		GIOChannel* source, GIOCondition condition,
		gpointer data    // ->ExultStudio.
) {
	ignore_unused_variable_warning(source, condition);
	ExultStudio* studio = static_cast<ExultStudio*>(data);
	studio->read_from_server();
	return true;
}
#else
static gint Read_from_server(gpointer data    // ->ExultStudio.
) {
	auto* studio = static_cast<ExultStudio*>(data);
	studio->read_from_server();
	return true;
}
#endif

gint Do_Drop_Callback(gpointer data);

void ExultStudio::read_from_server() {
#ifdef _WIN32
	// Nothing
	const int len = Exult_server::peek_pipe();

	// Do_Drop_Callback(&len);

	if (len == -1) {
		cout << "Disconnected from server" << endl;
		g_source_remove(server_input_tag);
		Exult_server::disconnect_from_server();
		server_input_tag = -1;
		server_socket    = -1;
		// Try again every 4 secs.
		g_timeout_add(4000, Reconnect, this);
		return;
	}
	if (len < 1) {
		return;
	}
#endif
	unsigned char          data[Exult_server::maxlength];
	Exult_server::Msg_type id;
	const int              datalen
			= Exult_server::Receive_data(server_socket, id, data, sizeof(data));
	if (datalen < 0) {
		cout << "Error reading from server" << endl;
		if (server_socket == -1) {    // Socket closed?
			g_source_remove(server_input_tag);
#ifdef _WIN32
			Exult_server::disconnect_from_server();
#endif
			server_input_tag = -1;
			// Try again every 4 secs.
			g_timeout_add(4000, Reconnect, this);
		}
		return;
	}
	cout << "Read " << datalen << " bytes from server" << endl;
	cout << "ID = " << static_cast<int>(id) << endl;
	switch (id) {
	case Exult_server::obj:
		open_obj_window(data, datalen);
		break;
	case Exult_server::barge:
		open_barge_window(data, datalen);
		break;
	case Exult_server::egg:
		open_egg_window(data, datalen);
		break;
	case Exult_server::npc:
		open_npc_window(data, datalen);
		break;
	case Exult_server::container:
		open_cont_window(data, datalen);
		break;
	case Exult_server::user_responded:
	case Exult_server::cancel:
	case Exult_server::locate_terrain:
	case Exult_server::swap_terrain:
	case Exult_server::insert_terrain:
	case Exult_server::delete_terrain:
	case Exult_server::locate_shape:
	case Exult_server::game_pos:
		if (waiting_for_server) {    // Send msg. to callback.
			waiting_for_server(id, data, datalen, waiting_client);
			waiting_for_server = nullptr;
			waiting_client     = nullptr;
		} else if (browser) {
			browser->server_response(static_cast<int>(id), data, datalen);
		}
		break;
	case Exult_server::info:
		info_received(data, datalen);
		break;
	case Exult_server::view_pos:
		if (locwin) {
			locwin->view_changed(data, datalen);
		}
		break;
	case Exult_server::combo_pick:
		open_combo_window();    // Open if necessary.
		if (combowin) {
			combowin->add(data, datalen, false);
		}
		break;
	case Exult_server::combo_toggle:
		open_combo_window();    // Open if necessary.
		if (combowin) {
			combowin->add(data, datalen, true);
		}
		break;
	case Exult_server::unused_shapes:
		show_unused_shapes(data, datalen);
		break;
	case Exult_server::select_status:
		set_edit_menu(data[0] != 0, data[1] != 0);
		break;
	case Exult_server::usecode_debugging:
		cerr << "Warning: got a usecode debugging message! (ignored)" << endl;
		break;
	default:
		cerr << "Warning: received unhandled message " << id
			 << "; this message is being ignored." << endl;
		break;
	}
}

/*
 *  Try to connect to the Exult game.
 *
 *  Output: True if succeeded.
 */
bool ExultStudio::connect_to_server() {
	if (!static_path) {
		return false;    // No place to go.
	}
#ifndef _WIN32
	if (server_socket >= 0) {    // Close existing socket.
		close(server_socket);
		g_source_remove(server_input_tag);
	}
	// Use <GAMEDAT>/exultserver.
	// Use stat() instead of U7exists which no longer supports sockets.
	struct stat       fs;
	const std::string servename = get_system_path(EXULT_SERVER);
	if (!U7exists(GAMEDAT) || (stat(servename.c_str(), &fs)) != 0) {
		cout << "Can't find gamedat for socket" << endl;
		return false;
	}
	server_socket = server_input_tag = -1;
	sockaddr_un addr;
	addr.sun_family  = AF_UNIX;
	addr.sun_path[0] = 0;
	strcpy(addr.sun_path, servename.c_str());
	server_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (server_socket < 0) {
		perror("Failed to open map-editor socket");
		return false;
	}
	// Set to be non-blocking.
	//	fcntl(server_socket, F_SETFL,
	//				fcntl(server_socket, F_GETFL) | O_NONBLOCK);
	cout << "Trying to connect to server at '" << addr.sun_path << "'" << endl;
	if (connect(server_socket, reinterpret_cast<sockaddr*>(&addr),
				sizeof(addr.sun_family) + strlen(addr.sun_path) + 1)
		== -1) {
		perror("Socket connect");
		close(server_socket);
		server_socket = -1;
		return false;
	}
	GIOChannel* gio  = g_io_channel_unix_new(server_socket);
	server_input_tag = g_io_add_watch(
			gio, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
			Read_from_server, this);
	g_io_channel_unref(gio);
#else
	// Close existing socket.
	if (server_socket != -1) {
		Exult_server::disconnect_from_server();
	}
	if (server_input_tag != -1) {
		g_source_remove(server_input_tag);
	}
	server_socket = server_input_tag = -1;

	if (Exult_server::try_connect_to_server(
				get_system_path(EXULT_SERVER).c_str())
		> 0) {
		server_input_tag = g_timeout_add(50, Read_from_server, this);
	} else {
		return false;
	}
#endif
	cout << "Connected to server" << endl;
	send_to_server(Exult_server::info);    // Request version, etc.
	set_edit_menu(false, false);           // For now, init. edit menu.
	return true;
}

void ExultStudio::disconnect_from_server() {
#ifndef _WIN32
	close(server_socket);
#else
	Exult_server::disconnect_from_server();
#endif
}

/*
 *  'Info' message received.
 */

void ExultStudio::info_received(
		unsigned char* data,    // Message from Exult.
		int            len) {
	int  vers;
	int  edlift;
	int  hdlift;
	int  edmode;
	bool editing;
	bool grid;
	bool mod;
	Game_info_in(data, len, vers, edlift, hdlift, editing, grid, mod, edmode);
	if (vers != Exult_server::version) {
		// Wrong version of Exult.
		EStudio::Alert(
				"Expected ExultServer version %d, but got %d",
				Exult_server::version, vers);
		disconnect_from_server();
		g_source_remove(server_input_tag);
		server_socket = server_input_tag = -1;
		return;
	}
	// Set controls to what Exult thinks.
	set_spin("edit_lift_spin", edlift);
	set_spin("hide_lift_spin", hdlift);
	set_toggle("play_button", !editing);
	set_toggle("tile_grid_button", grid);
	if (edmode >= 0 && static_cast<unsigned>(edmode) < mode_names.size()) {
		GtkWidget* mitem = get_widget(mode_names[edmode]);

		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem), true);
	}
}

/*
 *  Set edit menu entries depending on whether there's a selection or
 *  clipboard available.
 */

void ExultStudio::set_edit_menu(
		bool sel,    // Selection available.
		bool clip    // Clipboard isn't empty.
) {
	set_sensitive("cut1", sel);
	set_sensitive("copy1", sel);
	set_sensitive("paste1", clip);
	set_sensitive("properties1", sel);
	set_sensitive("basic_properties1", sel);
}

/*
 *  Find index (0-255) of closest color (r,g,b < 64).
 */

int ExultStudio::find_palette_color(int r, int g, int b) {
	int  best_index    = -1;
	long best_distance = 0xfffffff;
	for (int i = 0; i < 256; i++) {
		// Get deltas.
		const long dr = r - palbuf[3 * i];
		const long dg = g - palbuf[3 * i + 1];
		const long db = b - palbuf[3 * i + 2];
		// Figure distance-squared.
		const long dist = dr * dr + dg * dg + db * db;
		if (dist < best_distance) {    // Better than prev?
			best_index    = i;
			best_distance = dist;
		}
	}
	return best_index;
}

BaseGameInfo* ExultStudio::get_game() const {
	ModManager* basegame = gamemanager->get_game(curr_game);
	if (curr_mod > -1) {
		return basegame->get_mod(curr_mod);
	}
	return basegame;
}

// List partially copied from Firefox and from GLib's config.charset.
constexpr static const std::array encodings{
		"ASCII",       "CP437",       "CP850",      "ISO-8859-1",  "CP1252",
		"ISO-8859-15", "ISO-8859-2",  "CP1250",     "ISO-8859-3",  "ISO-8859-4",
		"CP1257",      "ISO-8859-13", "ISO-8859-5", "CP1251",      "KOI8-R",
		"CP866",       "KOI8-U",      "ISO-8859-6", "CP1256",      "ISO-8859-7",
		"CP1253",      "ISO-8859-8",  "CP1255",     "ISO-8859-9",  "CP1254",
		"ISO-8859-10", "ISO-8859-11", "CP874",      "ISO-8859-14",
};

static inline int Find_Encoding_Index(const char* enc) {
	for (size_t i = 0; i < encodings.size(); i++) {
		if (!strcmp(encodings[i], enc)) {
			return i;
		}
	}
	return -1;    // Not found.
}

static inline const char* Get_Encoding(int index) {
	if (index >= 0 && unsigned(index) < encodings.size()) {
		return encodings[index];
	} else {
		return encodings[0];
	}
}

C_EXPORT void on_set_game_information_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->set_game_information();
}

C_EXPORT void on_gameinfo_apply_clicked(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	const char*  enc    = Get_Encoding(studio->get_optmenu("gameinfo_charset"));

	GtkTextBuffer* buff = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(studio->get_widget("gameinfo_menustring")));
	GtkTextIter startpos;
	GtkTextIter endpos;
	gtk_text_buffer_get_bounds(buff, &startpos, &endpos);
	gchar* modmenu = gtk_text_iter_get_text(&startpos, &endpos);
	// Titles need to be displayable in Exult menu, hence should not
	// have any extra characters.
	string menustr(convertFromUTF8(modmenu, "ASCII"));
	for (size_t i = 0; i < strlen(menustr.c_str()); i++) {
		if ((static_cast<unsigned char>(menustr[i]) & 0x80) != 0) {
			menustr[i] = '?';
		}
	}
	g_free(modmenu);

	BaseGameInfo* gameinfo = studio->get_game();
	studio->set_encoding(enc);
	gameinfo->set_codepage(enc);
	gameinfo->set_menu_string(menustr);

	Configuration* cfg;
	string         root;
	const bool     ismod = gameinfo->get_config_file(cfg, root);
	cfg->set(root + (ismod ? "display_string" : "title"), menustr, true);
	cfg->set(root + "codepage", enc, true);
	if (ismod) {
		delete cfg;
	}

	GtkWidget* win = studio->get_widget("game_information");
	gtk_widget_set_visible(win, false);
}

C_EXPORT void on_gameinfo_charset_changed(
		GtkToggleButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->show_charset();
}

/*
 *  Show character set for current selection.
 */

void ExultStudio::show_charset() {
	const char*       enc = Get_Encoding(get_optmenu("gameinfo_charset"));
	static const char half_charset[]
			= {" \t00\t01\t02\t03\t04\t05\t06\t07\t08\t09\t0A\t0B\t0C\t0D\t0E\t"
			   "0F\n"
			   "00\t \t\x01\t\x02\t\x03\t\x04\t\x05\t\x06\t\x07\t\x08\t \t "
			   "\t\x0B\t\x0C\t \t\x0E\t\x0F\n"
			   "10\t\x10\t\x11\t\x12\t\x13\t\x14\t\x15\t\x16\t\x17\t\x18\t\x19"
			   "\t\x1A\t\x1B\t\x1C\t\x1D\t\x1E\t\x1F\n"
			   "20\t\x20\t\x21\t\x22\t\x23\t\x24\t\x25\t\x26\t\x27\t\x28\t\x29"
			   "\t\x2A\t\x2B\t\x2C\t\x2D\t\x2E\t\x2F\n"
			   "30\t\x30\t\x31\t\x32\t\x33\t\x34\t\x35\t\x36\t\x37\t\x38\t\x39"
			   "\t\x3A\t\x3B\t\x3C\t\x3D\t\x3E\t\x3F\n"
			   "40\t\x40\t\x41\t\x42\t\x43\t\x44\t\x45\t\x46\t\x47\t\x48\t\x49"
			   "\t\x4A\t\x4B\t\x4C\t\x4D\t\x4E\t\x4F\n"
			   "50\t\x50\t\x51\t\x52\t\x53\t\x54\t\x55\t\x56\t\x57\t\x58\t\x59"
			   "\t\x5A\t\x5B\t\x5C\t\x5D\t\x5E\t\x5F\n"
			   "60\t\x60\t\x61\t\x62\t\x63\t\x64\t\x65\t\x66\t\x67\t\x68\t\x69"
			   "\t\x6A\t\x6B\t\x6C\t\x6D\t\x6E\t\x6F\n"
			   "70\t\x70\t\x71\t\x72\t\x73\t\x74\t\x75\t\x76\t\x77\t\x78\t\x79"
			   "\t\x7A\t\x7B\t\x7C\t\x7D\t\x7E\t\x7F\n\0"};
	static const char full_charset[]
			= {" \t00\t01\t02\t03\t04\t05\t06\t07\t08\t09\t0A\t0B\t0C\t0D\t0E\t"
			   "0F\n"
			   "00\t \t\x01\t\x02\t\x03\t\x04\t\x05\t\x06\t\x07\t\x08\t \t "
			   "\t\x0B\t\x0C\t \t\x0E\t\x0F\n"
			   "10\t\x10\t\x11\t\x12\t\x13\t\x14\t\x15\t\x16\t\x17\t\x18\t\x19"
			   "\t\x1A\t\x1B\t\x1C\t\x1D\t\x1E\t\x1F\n"
			   "20\t\x20\t\x21\t\x22\t\x23\t\x24\t\x25\t\x26\t\x27\t\x28\t\x29"
			   "\t\x2A\t\x2B\t\x2C\t\x2D\t\x2E\t\x2F\n"
			   "30\t\x30\t\x31\t\x32\t\x33\t\x34\t\x35\t\x36\t\x37\t\x38\t\x39"
			   "\t\x3A\t\x3B\t\x3C\t\x3D\t\x3E\t\x3F\n"
			   "40\t\x40\t\x41\t\x42\t\x43\t\x44\t\x45\t\x46\t\x47\t\x48\t\x49"
			   "\t\x4A\t\x4B\t\x4C\t\x4D\t\x4E\t\x4F\n"
			   "50\t\x50\t\x51\t\x52\t\x53\t\x54\t\x55\t\x56\t\x57\t\x58\t\x59"
			   "\t\x5A\t\x5B\t\x5C\t\x5D\t\x5E\t\x5F\n"
			   "60\t\x60\t\x61\t\x62\t\x63\t\x64\t\x65\t\x66\t\x67\t\x68\t\x69"
			   "\t\x6A\t\x6B\t\x6C\t\x6D\t\x6E\t\x6F\n"
			   "70\t\x70\t\x71\t\x72\t\x73\t\x74\t\x75\t\x76\t\x77\t\x78\t\x79"
			   "\t\x7A\t\x7B\t\x7C\t\x7D\t\x7E\t\x7F\n"
			   "80\t\x80\t\x81\t\x82\t\x83\t\x84\t\x85\t\x86\t\x87\t\x88\t\x89"
			   "\t\x8A\t\x8B\t\x8C\t\x8D\t\x8E\t\x8F\n"
			   "90\t\x90\t\x91\t\x92\t\x93\t\x94\t\x95\t\x96\t\x97\t\x98\t\x99"
			   "\t\x9A\t\x9B\t\x9C\t\x9D\t\x9E\t\x9F\n"
			   "A0\t\xA0\t\xA1\t\xA2\t\xA3\t\xA4\t\xA5\t\xA6\t\xA7\t\xA8\t\xA9"
			   "\t\xAA\t\xAB\t\xAC\t\xAD\t\xAE\t\xAF\n"
			   "B0\t\xB0\t\xB1\t\xB2\t\xB3\t\xB4\t\xB5\t\xB6\t\xB7\t\xB8\t\xB9"
			   "\t\xBA\t\xBB\t\xBC\t\xBD\t\xBE\t\xBF\n"
			   "C0\t\xC0\t\xC1\t\xC2\t\xC3\t\xC4\t\xC5\t\xC6\t\xC7\t\xC8\t\xC9"
			   "\t\xCA\t\xCB\t\xCC\t\xCD\t\xCE\t\xCF\n"
			   "D0\t\xD0\t\xD1\t\xD2\t\xD3\t\xD4\t\xD5\t\xD6\t\xD7\t\xD8\t\xD9"
			   "\t\xDA\t\xDB\t\xDC\t\xDD\t\xDE\t\xDF\n"
			   "E0\t\xE0\t\xE1\t\xE2\t\xE3\t\xE4\t\xE5\t\xE6\t\xE7\t\xE8\t\xE9"
			   "\t\xEA\t\xEB\t\xEC\t\xED\t\xEE\t\xEF\n"
			   "F0\t\xF0\t\xF1\t\xF2\t\xF3\t\xF4\t\xF5\t\xF6\t\xF7\t\xF8\t\xF9"
			   "\t\xFA\t\xFB\t\xFC\t\xFD\t\xFE\t\xFF\n\0"};

	const string   codechars(convertToUTF8(
            ((strcmp(enc, "ASCII") == 0) ? half_charset : full_charset), enc));
	GtkTextBuffer* buff = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(get_widget("gameinfo_codepage_display")));
	gtk_text_buffer_set_text(buff, codechars.c_str(), -1);
}

/*
 *  Edit game/mod title and character set.
 */

void ExultStudio::set_game_information() {
	if (!gameinfowin) {
		GtkWidget* win = get_widget("game_information");
		gtk_window_set_modal(GTK_WINDOW(win), true);
		gameinfowin = win;

		g_signal_connect(
				G_OBJECT(get_widget("gameinfo_apply")), "clicked",
				G_CALLBACK(on_gameinfo_apply_clicked), nullptr);
	}

	// game_encoding should equal gameinfo->get_codepage().
	const int index = Find_Encoding_Index(game_encoding.c_str());

	// Override 'unknown' encodings.
	set_optmenu("gameinfo_charset", index >= 0 ? index : 0);
	show_charset();

	BaseGameInfo*  gameinfo = get_game();
	GtkTextBuffer* buff     = gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(get_widget("gameinfo_menustring")));
	// Titles need to be displayable in Exult menu, hence should not
	// have any extra characters.
	const string title(
			convertToUTF8(gameinfo->get_menu_string().c_str(), "ASCII"));
	gtk_text_buffer_set_text(buff, title.c_str(), -1);

	gtk_widget_set_visible(gameinfowin, true);
}

/*
 * Callbacks for the ICU Converters
 * Replace Illegal or Not Translatable characters by '?'
 */
static void convertToUnicodeCallback(    // Called in the direction Codepage ->
										 // UTF8
		const void* context, UConverterToUnicodeArgs* toArgs,
		const char* codeUnits, int32_t length, UConverterCallbackReason reason,
		UErrorCode* err) {
	ignore_unused_variable_warning(context, codeUnits, length);
	if (reason == UCNV_ILLEGAL
		|| reason == UCNV_UNASSIGNED) {    // Illegal : ASCII unexpected
										   // code 80..ff, Unassigned : all non
										   // defined
		*err = U_ZERO_ERROR;
		UChar toSubSource[]{'?'};
		ucnv_cbToUWriteUChars(toArgs, toSubSource, 1, 0, err);
	}
}

static void convertFromUnicodeCallback(    // Called in the direction UTF8 ->
										   // Codepage
		const void* context, UConverterFromUnicodeArgs* fromArgs,
		const UChar* codeUnits, int32_t length, UChar32 codePoint,
		UConverterCallbackReason reason, UErrorCode* err) {
	ignore_unused_variable_warning(context, codeUnits, length, codePoint);
	if (reason == UCNV_UNASSIGNED) {    // Unassigned : all non defined
		*err = U_ZERO_ERROR;
		const char toSubSource[]{'?'};
		ucnv_cbFromUWriteBytes(fromArgs, toSubSource, 1, 0, err);
	}
}

/*
 * Cache for the ICU Converters
 * The two Encodings,  strings
 * The two Converters, UConverters
 */
static const char* EncName      = nullptr;
static const char* UtfName      = nullptr;
static UConverter* EncConverter = nullptr;
static UConverter* UtfConverter = nullptr;

static bool setupUtfConverter(const char* utf) {
	UErrorCode status{U_ZERO_ERROR};
	if (!UtfName || strcmp(UtfName, utf) || UtfConverter) {
		if (UtfName && UtfConverter) {
			ucnv_close(UtfConverter);
			UtfName      = nullptr;
			UtfConverter = nullptr;
		}
		UtfConverter = ucnv_open(utf, &status);
		if (!U_SUCCESS(status)) {
			UtfName      = nullptr;
			UtfConverter = nullptr;
			return false;
		}
		ucnv_setToUCallBack(
				UtfConverter, convertToUnicodeCallback, nullptr, nullptr,
				nullptr, &status);
		if (!U_SUCCESS(status)) {
			ucnv_close(UtfConverter);
			UtfName      = nullptr;
			UtfConverter = nullptr;
			return false;
		}
		ucnv_setFromUCallBack(
				UtfConverter, convertFromUnicodeCallback, nullptr, nullptr,
				nullptr, &status);
		if (!U_SUCCESS(status)) {
			ucnv_close(UtfConverter);
			UtfName      = nullptr;
			UtfConverter = nullptr;
			return false;
		}
		UtfName = utf;
	}
	return true;
}

static bool setupEncConverter(const char* enc) {
	UErrorCode status{U_ZERO_ERROR};
	if (!EncName || strcmp(EncName, enc) || EncConverter) {
		if (EncName && EncConverter) {
			ucnv_close(EncConverter);
			EncName      = nullptr;
			EncConverter = nullptr;
		}
		EncConverter = ucnv_open(enc, &status);
		if (!U_SUCCESS(status)) {
			EncName      = nullptr;
			EncConverter = nullptr;
			return false;
		}
		ucnv_setToUCallBack(
				EncConverter, convertToUnicodeCallback, nullptr, nullptr,
				nullptr, &status);
		if (!U_SUCCESS(status)) {
			ucnv_close(EncConverter);
			EncName      = nullptr;
			EncConverter = nullptr;
			return false;
		}
		ucnv_setFromUCallBack(
				EncConverter, convertFromUnicodeCallback, nullptr, nullptr,
				nullptr, &status);
		if (!U_SUCCESS(status)) {
			ucnv_close(EncConverter);
			EncName      = nullptr;
			EncConverter = nullptr;
			return false;
		}
		EncName = enc;
	}
	return true;
}

/*
 *  Takes a UTF-8 string and converts into an 8-bit clean
 *  string using specified codepage. If the conversion fails,
 *  tries a lossy conversion to the same codepage.
 */
std::string convertFromUTF8(const char* src_str, const char* enc) {
	if (!src_str) {
		return "";
	}
	auto src_len = strlen(src_str);
	// The ASCII codepage stands for the Original Ultima VII French / German.
	//   It used a 7 bits ASCII codepage with reused codebytes 01 .. 1f
	//      except 09 0a 1a 1b for French, German and Spanish characters.

	bool is_7bit = (strcmp(enc, "ASCII") == 0);
	if (is_7bit) {
		enc = "CP437";
	}

	if (setupUtfConverter("UTF8") == false) {
		return "";
	}
	if (setupEncConverter(enc) == false) {
		return "";
	}

	string tgt_str(1 + src_len * 1, '\0');

	auto src_ptr = src_str;
	auto src_end = src_ptr + src_len + 1;
	auto tgt_ptr = tgt_str.data();
	auto tgt_end = tgt_str.data() + tgt_str.size();

	UErrorCode status{U_ZERO_ERROR};

	ucnv_convertEx(
			EncConverter,
			UtfConverter,         // Enc 2nd converter, UTF8 1st converter
			&tgt_ptr, tgt_end,    // Target progress pointer and limit
			&src_ptr, src_end,    // Source progress pointer and limit
			nullptr, nullptr,     // Pivot start and source pointer
			nullptr, nullptr,     // Pivot target pointer and limit
			true, true,
			&status);    // Reset boolean, Flush boolean, Error code pointer
	if (!U_SUCCESS(status)) {
		return "";
	}

	static unsigned char FromCP437[256] = {
			// 0     1     2     3     4     5     6     7     8     9     a b
			// c     d     e     f
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
			0x0b, 0x0c, 0x0d, 0x0e, 0x0f,    // 0
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
			0x1b, 0x1c, 0x1d, 0x1e, 0x1f,    // 1
			0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
			0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    // 2
			0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
			0x3b, 0x3c, 0x3d, 0x3e, 0x3f,    // 3
			0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
			0x4b, 0x4c, 0x4d, 0x4e, 0x4f,    // 4
			0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
			0x5b, 0x5c, 0x5d, 0x5e, 0x5f,    // 5
			0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
			0x6b, 0x6c, 0x6d, 0x6e, 0x6f,    // 6
			0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
			0x7b, 0x7c, 0x7d, 0x7e, 0x7f,    // 7
			// 80 -> 01 Ç:C cedilla,    81 -> 02 ü:u diaeresis,  82 -> 03 é:e
			// acute,      83 -> 04 â:a circumflex,
			// 84 -> 05 ä:a diaeresis,  85 -> 06 à:a grave, 87 -> 07 ç:c
			// cedilla,
			// 88 -> 08 ê:e circumflex, 89 -> 0b ë:e diaeresis,  8a -> 0c è:e
			// grave,      8b -> 0e ï:i diaeresis,
			// 8c -> 0f î:i circumflex, 8d -> 10 ì:i grave,      8e -> 11 Ä:A
			// diaeresis.
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x86, 0x07, 0x08, 0x0b, 0x0c,
			0x0e, 0x0f, 0x10, 0x11, 0x8f,    // 8
			// 90 -> 12 É:E acute E, 93 -> 13 ô:o circumflex,
			// 94 -> 14 ö:o diaeresis,                           96 -> 15 û:u
			// circumflex, 97 -> 16 ù:u grave,
			//                          99 -> 17 Ö:O diaeresis,  9a -> 18 Ü:U
			//                          diaeresis.
			0x12, 0x91, 0x92, 0x13, 0x14, 0x95, 0x15, 0x16, 0x98, 0x17, 0x18,
			0x9b, 0x9c, 0x9d, 0x9e, 0x9f,    // 9
			// a0 -> 19 á:a acute,      a1 -> 1d í:i acute,      a2 -> 1e ó:o
			// acute,      a3 -> 1f ú:u acute.
			0x19, 0x1d, 0x1e, 0x1f, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
			0xab, 0xac, 0xad, 0xae, 0xaf,    // a
			0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
			0xbb, 0xbc, 0xbd, 0xbe, 0xbf,    // b
			0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
			0xcb, 0xcc, 0xcd, 0xce, 0xcf,    // c
			0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
			0xdb, 0xdc, 0xdd, 0xde, 0xdf,    // d
			//                          e1 -> 1c ß:s sharp.
			0xe0, 0x1c, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
			0xeb, 0xec, 0xed, 0xee, 0xef,    // e
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
			0xfb, 0xfc, 0xfd, 0xfe, 0xff    // f
	};

	if (is_7bit) {
		for (char& c : tgt_str) {
			c = static_cast<char>(FromCP437[static_cast<unsigned char>(c)]);
		}
	}

	return tgt_str;
}

/*
 *  Takes an 8-bit clean string using specified codepage
 *  and converts into a UTF-8 string. If the conversion fails,
 *  tries a lossy conversion from the same codepage.
 */
std::string convertToUTF8(const char* src_str, const char* enc) {
	if (!src_str) {
		return "";
	}
	auto src_len = strlen(src_str);

	// The ASCII codepage stands for the Original Ultima VII French / German.
	//   It used a 7 bits ASCII codepage with reused codebytes 01 .. 1f
	//      except 09 0a 1a 1b for French, German and Spanish characters.

	bool is_7bit = (strcmp(enc, "ASCII") == 0);
	if (is_7bit) {
		enc = "CP437";
	}

	if (setupUtfConverter("UTF8") == false) {
		return "";
	}
	if (setupEncConverter(enc) == false) {
		return "";
	}

	static unsigned char ToCP437[256] = {
			// 0     1     2     3     4     5     6     7     8     9     a b
			// c     d     e     f
			//                          80 <- 01 Ç:C cedilla,    81 <- 02 ü:u
			//                          diaeresis,  82 <- 03 é:e acute,
			// 83 <- 04 â:a circumflex, 84 <- 05 ä:a diaeresis,  85 <- 06 à:a
			// grave,      87 <- 07 ç:c cedilla,
			// 88 <- 08 ê:e circumflex, 89 <- 0b ë:e diaeresis,
			// 8a <- 0c è:e grave,                               8b <- 0e ï:i
			// diaeresis,  8c <- 0f î:i circumflex.
			0x00, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x87, 0x88, 0x09, 0x0a,
			0x89, 0x8a, 0x0d, 0x8b, 0x8c,    // 0
			// 8d <- 10 ì:i grave,      8e <- 11 Ä:A diaeresis,  90 <- 12 É:E
			// acute E,    93 <- 13 ô:o circumflex,
			// 94 <- 14 ö:o diaeresis,  96 <- 15 û:u circumflex, 97 <- 16 ù:u
			// grave,      99 <- 17 Ö:O diaeresis,
			// 9a <- 18 Ü:U diaeresis,  a0 <- 19 á:a acute,
			// e1 <- 1c ß:s sharp,      a1 <- 1d í:i acute,      a2 <- 1e ó:o
			// acute,      a3 <- 1f ú:u acute.
			0x8d, 0x8e, 0x90, 0x93, 0x94, 0x96, 0x97, 0x99, 0x9a, 0xa0, 0x1a,
			0x1b, 0xe1, 0xa1, 0xa2, 0xa3,    // 1
			0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
			0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    // 2
			0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
			0x3b, 0x3c, 0x3d, 0x3e, 0x3f,    // 3
			0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
			0x4b, 0x4c, 0x4d, 0x4e, 0x4f,    // 4
			0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
			0x5b, 0x5c, 0x5d, 0x5e, 0x5f,    // 5
			0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
			0x6b, 0x6c, 0x6d, 0x6e, 0x6f,    // 6
			0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
			0x7b, 0x7c, 0x7d, 0x7e, 0x7f,    // 7
			0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
			0x8b, 0x8c, 0x8d, 0x8e, 0x8f,    // 8
			0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
			0x9b, 0x9c, 0x9d, 0x9e, 0x9f,    // 9
			0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
			0xab, 0xac, 0xad, 0xae, 0xaf,    // a
			0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
			0xbb, 0xbc, 0xbd, 0xbe, 0xbf,    // b
			0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
			0xcb, 0xcc, 0xcd, 0xce, 0xcf,    // c
			0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
			0xdb, 0xdc, 0xdd, 0xde, 0xdf,    // d
			0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
			0xeb, 0xec, 0xed, 0xee, 0xef,    // e
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
			0xfb, 0xfc, 0xfd, 0xfe, 0xff    // f
	};

	string cvt_str;
	if (is_7bit) {
		cvt_str = src_str;
		for (char& c : cvt_str) {
			c = static_cast<char>(ToCP437[static_cast<unsigned char>(c)]);
		}
		src_str = cvt_str.c_str();
	}

	string tgt_str(1 + src_len * 4, '\0');

	auto src_ptr = src_str;
	auto src_end = src_ptr + src_len + 1;
	auto tgt_ptr = tgt_str.data();
	auto tgt_end = tgt_str.data() + tgt_str.size();

	UErrorCode status{U_ZERO_ERROR};

	ucnv_convertEx(
			UtfConverter,
			EncConverter,         // UTF8 2nd converter, Enc 1st converter
			&tgt_ptr, tgt_end,    // Target progress pointer and limit
			&src_ptr, src_end,    // Source progress pointer and limit
			nullptr, nullptr,     // Pivot start and source pointer
			nullptr, nullptr,     // Pivot target pointer and limit
			true, true,
			&status);    // Reset boolean, Flush boolean, Error code pointer
	if (!U_SUCCESS(status)) {
		return "";
	}

	return tgt_str;
}

// HACK. NPC Paperdolls need this, but miscinf has too many
// Exult-dependant stuff to be included in ES. Thus, Exult
// defines the working version of this function.
// Maybe we should do something about this...
int get_skinvar(const std::string& key) {
	ignore_unused_variable_warning(key);
	return -1;
}

// Zoom callbacks
void ExultStudio::on_zoom_bilinear(GtkToggleButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	auto* studio           = ExultStudio::get_instance();
	studio->shape_bilinear = gtk_toggle_button_get_active(btn);
	if (studio->browser) {
		// No need to setup_info, the sizes of the shapes do not change.
		studio->browser->render();
	}
}

void ExultStudio::on_zoom_up(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	auto* studio = ExultStudio::get_instance();
	if (studio->shape_scale == 32) {
		return;
	}
	if (studio->shape_scale % 3 == 0) {
		studio->shape_scale = (studio->shape_scale * 4) / 3;
	} else {
		studio->shape_scale = (studio->shape_scale * 3) / 2;
	}
	string zvalue = std::to_string(50 * studio->shape_scale) + "%";
	gtk_label_set_text(GTK_LABEL(studio->shape_zlabel), zvalue.c_str());
	gtk_widget_set_sensitive(
			studio->shape_zup, (studio->shape_scale < 32 ? true : false));
	gtk_widget_set_sensitive(
			studio->shape_zdown, (studio->shape_scale > 2 ? true : false));
	if (studio->browser) {
		studio->browser->setup_info(true);
		studio->browser->render();
	}
}

void ExultStudio::on_zoom_down(GtkButton* btn, gpointer user_data) {
	ignore_unused_variable_warning(btn, user_data);
	auto* studio = ExultStudio::get_instance();
	if (studio->shape_scale == 2) {
		return;
	}
	if (studio->shape_scale % 3 == 0) {
		studio->shape_scale = (studio->shape_scale * 2) / 3;
	} else {
		studio->shape_scale = (studio->shape_scale * 3) / 4;
	}
	string zvalue = std::to_string(50 * studio->shape_scale) + "%";
	gtk_label_set_text(GTK_LABEL(studio->shape_zlabel), zvalue.c_str());
	gtk_widget_set_sensitive(
			studio->shape_zup, (studio->shape_scale < 32 ? true : false));
	gtk_widget_set_sensitive(
			studio->shape_zdown, (studio->shape_scale > 2 ? true : false));
	if (studio->browser) {
		studio->browser->setup_info(true);
		studio->browser->render();
	}
}

gboolean ExultStudio::on_app_key_press(
		GtkEntry* entry, GdkEventKey* event, gpointer user_data) {
	ignore_unused_variable_warning(entry, user_data);
	auto* studio = ExultStudio::get_instance();
	switch (event->keyval) {
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		if (studio->shape_zup) {
			g_signal_emit_by_name(G_OBJECT(studio->shape_zup), "clicked");
		}
		return true;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		if (studio->shape_zdown) {
			g_signal_emit_by_name(G_OBJECT(studio->shape_zdown), "clicked");
		}
		return true;
	}
	return false;
}

void ExultStudio::create_zoom_controls() {
	GtkWidget* zbox   = get_widget("menubar_box");
	GtkWidget* zframe = gtk_frame_new(nullptr);
	widget_set_margins(
			zframe, 2 * HMARGIN, 0 * HMARGIN, 0 * VMARGIN, 0 * VMARGIN);
	gtk_widget_set_visible(zframe, true);
	gtk_box_pack_start(GTK_BOX(zbox), zframe, false, false, 0);

	GtkWidget* zbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	widget_set_margins(
			zbox2, 0 * HMARGIN, 0 * HMARGIN, 0 * VMARGIN, 0 * VMARGIN);
	gtk_box_set_homogeneous(GTK_BOX(zbox2), false);
	gtk_widget_set_visible(zbox2, true);
	gtk_container_add(GTK_CONTAINER(zframe), zbox2);

	GtkWidget* zname = gtk_label_new("Zoom");
	gtk_widget_set_visible(GTK_WIDGET(zname), true);
	widget_set_margins(
			zname, 4 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_box_pack_start(GTK_BOX(zbox2), zname, false, false, 0);
	gtk_widget_set_visible(zname, true);

	string zvalue = std::to_string(50 * shape_scale) + "%";
	shape_zlabel  = gtk_label_new(zvalue.c_str());
	gtk_widget_set_visible(GTK_WIDGET(shape_zlabel), true);
	widget_set_margins(
			shape_zlabel, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_box_pack_start(GTK_BOX(zbox2), shape_zlabel, false, false, 0);
	gtk_widget_set_size_request(shape_zlabel, 50, -1);
	gtk_label_set_justify(GTK_LABEL(shape_zlabel), GTK_JUSTIFY_RIGHT);
	gtk_widget_set_visible(shape_zlabel, true);

	shape_zdown = EStudio::Create_arrow_button(
			GTK_ARROW_DOWN, G_CALLBACK(ExultStudio::on_zoom_down), this);
	widget_set_margins(
			shape_zdown, 2 * HMARGIN, 1 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_sensitive(shape_zdown, (shape_scale > 2 ? true : false));
	gtk_box_pack_start(GTK_BOX(zbox2), shape_zdown, true, true, 0);
	gtk_widget_set_visible(shape_zdown, true);

	shape_zup = EStudio::Create_arrow_button(
			GTK_ARROW_UP, G_CALLBACK(ExultStudio::on_zoom_up), this);
	widget_set_margins(
			shape_zup, 1 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_sensitive(shape_zup, (shape_scale < 32 ? true : false));
	gtk_box_pack_start(GTK_BOX(zbox2), shape_zup, true, true, 0);
	gtk_widget_set_visible(shape_zup, true);

	GtkWidget* zcheck = gtk_check_button_new_with_label("Bilinear");
	widget_set_margins(
			zcheck, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_box_pack_start(GTK_BOX(zbox2), zcheck, true, true, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(zcheck), shape_bilinear);
	gtk_widget_set_visible(zcheck, true);
	g_signal_connect(
			G_OBJECT(zcheck), "toggled",
			G_CALLBACK(ExultStudio::on_zoom_bilinear), this);
}
