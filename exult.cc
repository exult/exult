/**
 ** Exult.cc - Multiplatform Ultima 7 game engine
 **
 ** Written: 7/22/98 - JSF
 **/
/*
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdlib>
#include <cctype>
#include <cmath>

#include <SDL.h>

#define Font _XFont_
#include <SDL_syswm.h>
#undef Font

#ifdef USE_EXULTSTUDIO  /* Only needed for communication with exult studio */
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef _WIN32
#include "windrag.h"
#endif
#include "servemsg.h"
#include "objserial.h"
#include "server.h"
#include "chunks.h"
#include "chunkter.h"
#endif

#if (defined(USECODE_DEBUGGER) && defined(XWIN))
#include <csignal>
#endif

#include "Audio.h"
#include "Configuration.h"
#include "Gump_manager.h"
#include "Scroll_gump.h"
#include "actors.h"
#include "args.h"
#include "cheat.h"
#include "effects.h"
#include "exult.h"
#include "exultmenu.h"
#include "fnames.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "gamemap.h"
#include "gump_utils.h"
#include "keyactions.h"
#include "keys.h"
#include "mouse.h"
#include "ucmachine.h"
#include "sdlrwopsistream.h"
#include "sdlrwopsostream.h"
#include "utils.h"
#include "version.h"
#include "u7drag.h"
#include "drag.h"
#include "palette.h"
#include "combat_opts.h"
#include "U7file.h"
#include "U7fileman.h"
#include "party.h"
#include "array_size.h"

#include "exult_flx.h"
#include "exult_bg_flx.h"
#include "exult_si_flx.h"
#include "crc.h"
#include "items.h"
#include "gamemgr/modmgr.h"
#include "AudioMixer.h"
#include "VideoOptions_gump.h"
#include "Gump_button.h"
#include "ShortcutBar_gump.h"
#include "ignore_unused_variable_warning.h"
#include "touchui.h"
#include "verify.h"
using namespace Pentagram;

#ifdef __IPHONEOS__
#  include "ios_utils.h"
#elif defined(ANDROID)
#  include <SDL_system.h>
#  include "TouchUI_Android.h"
#endif

using std::atof;
using std::cerr;
using std::cout;
using std::endl;
using std::atexit;
using std::exit;
using std::toupper;
using std::string;
using std::vector;

#if (defined(_WIN32) || (defined(MACOSX) && defined(USE_EXULTSTUDIO)))

static int SDLCALL SDL_putenv(const char *_var) {
    char *ptr = nullptr;
    char *var = SDL_strdup(_var);
    if (var == nullptr) {
        return -1;  /* we don't set errno. */
    }

    ptr = SDL_strchr(var, '=');
    if (ptr == nullptr) {
        SDL_free(var);
        return -1;
    }

    *ptr = '\0';  /* split the string into name and value. */
    SDL_setenv(var, ptr + 1, 1);
    SDL_free(var);
    return 0;
}
#endif

Configuration *config = nullptr;
KeyBinder *keybinder = nullptr;
GameManager *gamemanager = nullptr;

/*
 *  Globals:
 */
Game_window *gwin = nullptr;
quitting_time_enum quitting_time = QUIT_TIME_NO;

bool intrinsic_trace = false;       // Do we trace Usecode-intrinsics?
int usecode_trace = 0;      // Do we trace Usecode-instructions?
// 0 = no, 1 = short, 2 = long
bool combat_trace = false; // show combat messages?

// Save game compression level
int save_compression = 1;
bool ignore_crc = false;

TouchUI *touchui = nullptr;

bool g_waiting_for_click = false;
ShortcutBar_gump *g_shortcutBar = nullptr;

#ifdef USECODE_DEBUGGER
bool    usecode_debugging = false;  // Do we enable the usecode debugger?
extern void initialise_usecode_debugger();
#endif

struct resolution {
	int x;
	int y;
	int scale;
} res_list[] = {
	{ 320, 200, 1 },
	{ 320, 240, 1 },
	{ 400, 300, 1 },
	{ 320, 200, 2 },
	{ 320, 240, 2 },
	{ 400, 300, 2 },
	{ 512, 384, 1 },
	{ 640, 480, 1 },
	{ 800, 600, 1 }
};
int num_res = array_size(res_list);
int current_res = 0;
int current_scaleval = 1;

#if defined(_WIN32) && defined(USE_EXULTSTUDIO)
static HWND hgwin;
static Windnd *windnd = nullptr;
#endif

/*
 *  Local functions:
 */
static int exult_main(const char * runpath);
static void Init();
static int Play();
static bool Get_click(int &x, int &y, char *chr, bool drag_ok, bool rotate_colors = false);
static int find_resolution(int w, int h, int s);
static void set_scaleval(int new_scaleval);
#ifdef USE_EXULTSTUDIO
static void Move_dragged_shape(int shape, int frame, int x, int y,
                               int prevx, int prevy, bool show);
#ifdef _WIN32
static void Move_dragged_combo(int xtiles, int ytiles, int tiles_right,
                               int tiles_below, int x, int y, int prevx, int prevy, bool show);
#endif
static void Drop_dragged_shape(int shape, int frame, int x, int y);
static void Drop_dragged_chunk(int chunknum, int x, int y);
static void Drop_dragged_npc(int npcnum, int x, int y);
static void Drop_dragged_combo(int cnt, U7_combo_data *combo, int x, int y);
#endif
static void BuildGameMap(BaseGameInfo *game, int mapnum);
static void Handle_events();
static void Handle_event(SDL_Event &event);

/*
 *  Statics:
 */
static bool run_bg = false;     // skip menu and run bg
static bool run_si = false;     // skip menu and run si
static bool run_fov = false;        // skip menu and run fov
static bool run_ss = false;     // skip menu and run ss
static bool run_sib = false;        // skip menu and run sib
static string arg_gamename = "default"; // cmdline arguments
static string arg_modname = "default";  // cmdline arguments
static string arg_configfile;
static int arg_buildmap = -1;
static int arg_mapnum = -1;
static bool arg_nomenu = false;
static bool arg_edit_mode = false;  // Start up ExultStudio.
static bool arg_write_xml = false;  // Write out game's config. as XML.
static bool arg_reset_video = false;    // Resets the video setings.
static bool arg_verify_files = false;    // Verify a game's files.

static bool dragging = false;       // Object or gump being moved.
static bool dragged = false;        // Flag for when obj. moved.
static bool right_on_gump = false;  // Right clicked on gump?
static int show_items_x = 0, show_items_y = 0;
static unsigned int show_items_time = 0;
static bool show_items_clicked = false;
static int left_down_x = 0, left_down_y = 0;
static int joy_aim_x = 0, joy_aim_y = 0;
Mouse::Avatar_Speed_Factors joy_speed_factor = Mouse::medium_speed_factor;

#if defined _WIN32
void do_cleanup_output() {
	cleanup_output("std");
}
#endif

/*
 *  Main program.
 */

#ifdef HAVE_PNG_H
#define EXULT_IMAGE_SUFFIX "png"
#else
#define EXULT_IMAGE_SUFFIX "pcx"
#endif // HAVE_PNG_H

int main(
    int argc,
    char *argv[]
) {
	bool    needhelp = false;
	bool    showversion = false;
	int     result;
	Args    parameters;

	// Use SDL_RWops for file I/O in the main game engine for better
	// cross-platform support.  Standalone utilities continue to default
	// to the default std::fstream-based file I/O to avoid taking an SDL
	// dependency.
	U7set_istream_factory(
		[](const char* s, std::ios_base::openmode mode) {
			return std::make_unique<SdlRwopsIstream>(s, mode);
		}
	);
	U7set_ostream_factory(
		[](const char* s, std::ios_base::openmode mode) {
			return std::make_unique<SdlRwopsOstream>(s, mode);
		}
	);

#ifdef ANDROID
	// Doing this ifdef here rather than in utils.cc so that it doesn't have to take a dependency
	// on SDL.
	U7set_home(SDL_AndroidGetInternalStoragePath());
#endif

	// Declare everything from the commandline that we're interested in.
	parameters.declare("-h", &needhelp, true);
	parameters.declare("--help", &needhelp, true);
	parameters.declare("/?", &needhelp, true);
	parameters.declare("/h", &needhelp, true);
	parameters.declare("--bg", &run_bg, true);
	parameters.declare("--si", &run_si, true);
	parameters.declare("--fov", &run_fov, true);
	parameters.declare("--ss", &run_ss, true);
	parameters.declare("--sib", &run_sib, true);
	parameters.declare("--nomenu", &arg_nomenu, true);
	parameters.declare("-v", &showversion, true);
	parameters.declare("--version", &showversion, true);
	parameters.declare("--game", &arg_gamename, "default");
	parameters.declare("--mod", &arg_modname, "default");
	parameters.declare("--buildmap", &arg_buildmap, -1);
	parameters.declare("--mapnum", &arg_mapnum, -1);
	parameters.declare("--nocrc", &ignore_crc, true);
	parameters.declare("-c", &arg_configfile, "");
	parameters.declare("--edit", &arg_edit_mode, true);
	parameters.declare("--write-xml", &arg_write_xml, true);
	parameters.declare("--reset-video", &arg_reset_video, true);
	parameters.declare("--verify-files", &arg_verify_files, true);
#if defined _WIN32
	bool portable = false;
	parameters.declare("-p", &portable, true);
#endif
	// Process the args
	parameters.process(argc, argv);
	add_system_path("<alt_cfg>", arg_configfile);
#if defined _WIN32
	if (portable)
		add_system_path("<HOME>", ".");
	setup_program_paths();
	redirect_output("std");
	std::atexit(do_cleanup_output);
#endif

	if (needhelp) {
		cerr << "Usage: exult [--help|-h] [-v|--version] [-c configfile]" << endl
		     << "             [--bg|--fov|--si|--ss|--sib|--game <game>] [--mod <mod>]" << endl
		     << "             [--nomenu] [--buildmap 0|1|2] [--mapnum <num>]" << endl
		     << "             [--nocrc] [--edit] [--write-xml] [--reset-video]" << endl
		     << "--help\t\tShow this information" << endl
		     << "--version\tShow version info" << endl
		     << " -c configfile\tSpecify alternate config file" << endl
		     << "--bg\t\tSkip menu and run Black Gate (prefers original game)" << endl
		     << "--fov\t\tSkip menu and run Black Gate with Forge of Virtue expansion" << endl
		     << "--si\t\tSkip menu and run Serpent Isle (prefers original game)" << endl
		     << "--ss\t\tSkip menu and run Serpent Isle with Silver Seed expansion" << endl
		     << "--sib\t\tSkip menu and run Serpent Isle Beta" << endl
		     << "--nomenu\tSkip BG/SI game menu" << endl
		     << "--game <game>\tSkip menu and run the game with key '<game>' in Exult.cfg" << endl
		     << "--mod <mod>\tMust be used together with '--bg', '--fov', '--si', '--ss', '--sib' or" << endl
		     << "\t\t'--game <game>'; runs the specified game using the mod with" << endl
		     << "\t\ttitle equal to '<mod>'" << endl
		     << "--buildmap <N>\tCreate a fullsize map of the game world in u7map??." << EXULT_IMAGE_SUFFIX << endl
		     << "\t\t(N=0: all roofs, 1: no level 2 roofs, 2: no roofs)" << endl
		     << "\t\tOnly valid if used together with '--bg', '--fov', '--si', '--ss', '--sib'" << endl
		     << "\t\tor '--game <game>'; you may optionally specify a mod with" << endl
		     << "\t\t'--mod <mod>' (WARNING: requires big amounts of RAM, HD" << endl
		     << "\t\tspace and time!)" << endl
		     << "--mapnum <N>\tThis must be used with '--buildmap'. Selects which map" << endl
		     << "\t\t(for multimap games or mods) whose map is desired" << endl
		     << "--nocrc\t\tDon't check crc's of .flx files" << endl
		     << "--verify-files\tVerifies that the files in static dir are not corrupt" << endl
		     << "\t\tOnly valid if used together with '--bg', '--fov', '--si', '--ss', '--sib'" << endl
		     << "\t\tor '--game <game>'; you cannot specify a mod with this flag" << endl
		     << "--edit\t\tStart in map-edit mode" << endl;
#if defined _WIN32
		cerr << " -p\t\tMakes the home path the Exult directory (old Windows way)" << endl;
#endif
		cerr << "--write-xml\tWrite 'patch/exultgame.xml'" << endl
		     << "--reset-video\tResets to the default video settings" << endl;

		exit(1);
	}
	unsigned gameparam = static_cast<unsigned>(run_bg)
	                     + static_cast<unsigned>(run_si)
	                     + static_cast<unsigned>(run_fov)
	                     + static_cast<unsigned>(run_ss)
	                     + static_cast<unsigned>(run_sib)
	                     + static_cast<unsigned>(arg_gamename != "default");
	if (gameparam > 1) {
		cerr << "Error: You may only specify one of --bg, --fov, --si, --ss, --sib or --game!" <<
		     endl;
		exit(1);
	} else if (arg_buildmap >= 0 && gameparam == 0) {
		cerr << "Error: --buildmap requires one of --bg, --fov, --si, --ss, --sib or --game!" <<
		     endl;
		exit(1);
	} else if (arg_verify_files && gameparam == 0) {
		cerr << "Error: --verify-files requires one of --bg, --fov, --si, --ss, --sib or --game!" <<
		     endl;
		exit(1);
	}

	if (arg_mapnum >= 0 && arg_buildmap < 0) {
		cerr << "Error: '--mapnum' requires '--buildmap'!" << endl;
		exit(1);
	} else if (arg_mapnum < 0)
		arg_mapnum = 0;     // Sane default.

	if (arg_modname != "default" && !gameparam) {
		cerr << "Error: You must also specify the game to be used!" << endl;
		exit(1);
	} else if (arg_verify_files && arg_modname != "default") {
		cerr << "Error: You cannot combine --mod with --verify-files!" << endl;
		exit(1);
	}

	if (showversion) {
		getVersionInfo(cerr);
		return 0;
	}

	try {
		result = exult_main(argv[0]);
	} catch (const quit_exception & /*e*/) {
		// Your basic "shutup Valgrind" code.
		Free_text();
		fontManager.reset();
		delete gamemanager;
		delete Game_window::get_instance();
		delete game;
		Audio::Destroy();   // Deinit the sound system.
		delete config;
		SDL_VideoQuit();
		SDL_Quit();
		result = 0;
	} catch (const exult_exception &e) {
		cerr << "============================" << endl <<
		     "An exception occured: " << endl <<
		     e.what() << endl <<
		     "errno: " << e.get_errno() << endl;
		if (e.get_errno() != 0)
			perror("Error Description");
		cerr << "============================" << endl;
		result = e.get_errno();
	}

	return result;
}

/*
 *  Main program.
 */

int exult_main(const char *runpath) {
	string music_path;
	// output version info
	getVersionInfo(cout);

#ifndef _WIN32
	setup_program_paths();
#endif
	// Read in configuration file
	config = new Configuration;

	if (!arg_configfile.empty()) {
		config->read_abs_config_file(arg_configfile);
	} else {
		config->read_config_file(USER_CONFIGURATION_FILE);
	}

	// reset-video command line option
	if (arg_reset_video) {
		config->set("config/video/display/width", 1024, false);
		config->set("config/video/display/height", 768, false);
		config->set("config/video/game/width", 320, false);
		config->set("config/video/game/height", 200, false);
		config->set("config/video/scale", 2, false);
		config->set("config/video/scale_method", "point" , false);
		config->set("config/video/fill_mode", "fit", false);
		config->set("config/video/fill_scaler", "point", false);
		config->set("config/video/share_video_settings", "yes", false);
		config->set("config/video/fullscreen", "no", false);
		config->set("config/video/force_bpp", 0, false);

		config->write_back();
	}
	if (config->key_exists("config/gameplay/allow_double_right_move")) {
		string str;
		config->value("config/gameplay/allow_double_right_move", str, "yes");
		if (str == "no") {
			config->value("config/gameplay/allow_right_pathfind", str, "no");
			config->set("config/gameplay/allow_right_pathfind", str, false);
		}
		config->remove("config/gameplay/allow_double_right_move", false);
	}

	// Setup virtual directories
	string data_path;
	config->value("config/disk/data_path", data_path, EXULT_DATADIR);
	setup_data_dir(data_path, runpath);

	std::string default_music = get_system_path("<DATA>/music");
	config->value("config/disk/music_path", music_path, default_music.c_str());

	add_system_path("<MUSIC>", music_path);
	add_system_path("<STATIC>", "static");
	add_system_path("<GAMEDAT>", "gamedat");
	add_system_path("<PATCH>", "patch");
//	add_system_path("<SAVEGAME>", "savegame");
	add_system_path("<SAVEGAME>", ".");
	add_system_path("<MODS>", "mods");

	std::cout << "Exult path settings:" << std::endl;
#ifdef __IPHONEOS__
	std::cout << "Bundle        : " << get_system_path("<BUNDLE>") << std::endl;
#endif
	std::cout << "Data          : " << get_system_path("<DATA>") << std::endl;
	std::cout << "Digital music : " << get_system_path("<MUSIC>") << std::endl;
	std::cout << std::endl;

	// Check CRCs of our .flx files
	bool crc_ok = true;
	const char *flexname = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	uint32 crc = crc32(flexname);
	if (crc != EXULT_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult.flx has a wrong checksum!" << endl;
	}
	flexname = BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX);
	if (crc32(flexname) != EXULT_BG_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult_bg.flx has a wrong checksum!" << endl;
	}
	flexname = BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);
	if (crc32(flexname) != EXULT_SI_FLX_CRC32) {
		crc_ok = false;
		cerr << "exult_si.flx has a wrong checksum!" << endl;
	}

	bool config_ignore_crc;
	config->value("config/disk/no_crc", config_ignore_crc);
	ignore_crc |= config_ignore_crc;

	if (!ignore_crc && !crc_ok) {
		cerr << "This usually means the file(s) mentioned above are "
		     << "from a different version" << endl
		     << "of Exult than this one. Please re-install Exult" << endl
		     << endl
		     << "(Note: if you modified the .flx files yourself, "
		     << "you can skip this check" << endl
		     << "by passing the --nocrc parameter.)" << endl;

		return 1;
	}


	// Convert from old format if needed
	vector<string> vs = config->listkeys("config/disk/game", false);
	if (vs.empty() && config->key_exists("config/disk/u7path")) {
		// Convert from the older format
		string data_directory;
		config->value("config/disk/u7path", data_directory, "./blackgate");
		config->remove("config/disk/u7path", false);
		config->set("config/disk/game/blackgate/path", data_directory, true);
	}

	// Enable tracing of intrinsics?
	config->value("config/debug/trace/intrinsics", intrinsic_trace);

	// Enable tracing of UC-instructions?
	string uctrace;
	config->value("config/debug/trace/usecode", uctrace, "no");
	to_uppercase(uctrace);
	if (uctrace == "YES")
		usecode_trace = 1;
	else if (uctrace == "VERBOSE")
		usecode_trace = 2;
	else
		usecode_trace = 0;

	config->value("config/debug/trace/combat", combat_trace);

	// Save game compression level
	config->value("config/disk/save_compression_level", save_compression, 1);
	if (save_compression < 0 || save_compression > 2) save_compression = 1;
	config->set("config/disk/save_compression_level", save_compression, false);
#ifdef USECODE_DEBUGGER
	// Enable usecode debugger
	config->value("config/debug/debugger/enable", usecode_debugging);
	initialise_usecode_debugger();
#endif

#if (defined(USECODE_DEBUGGER) && defined(XWIN))
	signal(SIGUSR1, SIG_IGN);
#endif

	cheat.init();

#ifdef __IPHONEOS__
	touchui = new TouchUI_iOS();
#elif defined(ANDROID)
	touchui = new TouchUI_Android();
#endif
	Init();             // Create main window.

	cheat.finish_init();
	cheat.set_map_editor(arg_edit_mode);    // Start in map-edit mode?
	if (arg_write_xml)
		game->write_game_xml();

	Mouse::mouse = new Mouse(gwin);
	Mouse::mouse->set_shape(Mouse::hand);

	if (touchui != nullptr) {
		touchui->showButtonControls();
		// TODO(Marzo): This is probably the wrong place for this
		Usecode_machine *usecode = Game_window::get_instance()->get_usecode();
		if (!usecode->get_global_flag(Usecode_machine::did_first_scene) && GAME_BG) {
			touchui->hideGameControls();
		} else {
			touchui->showGameControls();
		}
	}

	int result = Play();        // start game

#ifdef USE_EXULTSTUDIO
	// Currently, leaving the game results in destruction of the window.
	//  Maybe sometime in the future, there is an option like "return to
	//  main menu and select another scenario". Becaule DnD isn't registered until
	//  you really enter the game, we remove it here to prevent possible bugs
	//  invilved with registering DnD a second time over an old variable.
#if defined(_WIN32)
	RevokeDragDrop(hgwin);
	windnd->Release();
#endif
	Server_close();
#endif

	return result;
}

namespace ExultIcon {
#include "exulticon.h"
}

static void SetIcon() {
#ifndef MACOSX      // Don't set icon on OS X; the external icon is *much* nicer
	SDL_Color iconpal[256];
	for (int i = 0; i < 256; ++i) {
		iconpal[i].r = ExultIcon::header_data_cmap[i][0];
		iconpal[i].g = ExultIcon::header_data_cmap[i][1];
		iconpal[i].b = ExultIcon::header_data_cmap[i][2];
	}
	SDL_Surface *iconsurface = SDL_CreateRGBSurface(0,
				ExultIcon::width,
				ExultIcon::height,
				32,
				0, 0, 0, 0);
	if (iconsurface == nullptr)
		cout << "Error creating icon surface: " << SDL_GetError() << std::endl;
	for (int y = 0; y < static_cast<int>(ExultIcon::height); ++y)
	{
		for (int x = 0; x < static_cast<int>(ExultIcon::width); ++x)
		{
			int idx = ExultIcon::header_data[(y*ExultIcon::height)+x];
			Uint32 pix = SDL_MapRGB(iconsurface->format,
					iconpal[idx].r,
					iconpal[idx].g,
					iconpal[idx].b);
			SDL_Rect destRect = {x, y, 1, 1};
			SDL_FillRect(iconsurface, &destRect, pix);
		}
	}
	SDL_SetColorKey(iconsurface, SDL_TRUE,
		SDL_MapRGB(iconsurface->format,
			iconpal[0].r, iconpal[0].g, iconpal[0].b));
	SDL_SetWindowIcon(gwin->get_win()->get_screen_window(), iconsurface);
	SDL_FreeSurface(iconsurface);
#endif
}

void Open_game_controller(int joystick_index) {
	SDL_GameController *input_device = SDL_GameControllerOpen(joystick_index);
	if (input_device) {
		SDL_GameControllerGetJoystick(input_device);
		std::cout << "Game controller attached and open: \""
		          << SDL_GameControllerName(input_device) << '"' << std::endl;
	} else {
		std::cout << "Game controller attached, but it failed to open. Error: \""
		          << SDL_GetError() << '"' << std::endl;
	}
}

int Handle_device_connection_event(void *userdata, SDL_Event *event) {
	ignore_unused_variable_warning(userdata);
	// Make sure that game-controllers are opened and closed, as they
	// become connected or disconnected.
	switch (event->type) {
		case SDL_CONTROLLERDEVICEADDED: {
			SDL_JoystickID joystick_id = SDL_JoystickGetDeviceInstanceID(event->cdevice.which);
			if (!SDL_GameControllerFromInstanceID(joystick_id)) {
				Open_game_controller(event->cdevice.which);
			}
			break;
		}
		case SDL_CONTROLLERDEVICEREMOVED: {
			SDL_GameController *input_device = SDL_GameControllerFromInstanceID(event->cdevice.which);
			if (input_device) {
				SDL_GameControllerClose(input_device);
				input_device = nullptr;
				std::cout << "Game controller detached and closed." << std::endl;
			}
			break;
		}
	}

	// Returning 1 will tell SDL2, which can invoke this via a callback
	// setup through SDL_AddEventWatch, to make sure the event gets posted
	// to its event-queue (rather than dropping it).
	return 1;
}

/*
 *  Initialize and create main window.
 */
static void Init(
) {
	Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER;
#ifdef NO_SDL_PARACHUTE
	init_flags |= SDL_INIT_NOPARACHUTE;
#endif
#ifdef _WIN32
	SDL_putenv("SDL_AUDIODRIVER=DirectSound");
#elif defined(MACOSX) && defined(XWIN) && defined(USE_EXULTSTUDIO)
	// Exult Studio drag'n'drop with SDL2 < 2.0.15 requires Exult
	// to use X11. Hence, we force the issue.
	SDL_putenv("SDL_VIDEODRIVER=x11");
#endif
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "Landscape");
#if 0
	init_flags |= SDL_INIT_JOYSTICK;
#endif
#if defined(__IPHONEOS__) || defined(ANDROID)
	Mouse::use_touch_input = true;
#endif
#ifdef __IPHONEOS__
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
#endif
	if (SDL_Init(init_flags) < 0) {
		cerr << "Unable to initialize SDL: " << SDL_GetError() << endl;
		exit(-1);
	}
	std::atexit(SDL_Quit);

	SDL_SysWMinfo info;     // Get system info.

	// KBD repeat should be nice.
	SDL_ShowCursor(0);
	SDL_VERSION(&info.version);

	// Open any connected game controllers.
	for (int i = 0, n = SDL_NumJoysticks(); i < n; ++i) {
		if (SDL_IsGameController(i)) {
			Open_game_controller(i);
		}
	}
	// Listen for game controller device connection and disconnection
	// events. Registering a listener allows these events to be received
	// and processed via any event-processing loop, of which Exult has
	// many, without needing to modify each individual loop, and to
	// make sure that SDL_GameController objects are always ready.
	SDL_AddEventWatch(Handle_device_connection_event, nullptr);

	// Load games and mods; also stores system paths:
	gamemanager = new GameManager();

	if (arg_buildmap < 0 && arg_verify_files == false) {
		string gr;
		string gg;
		string gb;
		config->value("config/video/gamma/red", gr, "1.0");
		config->value("config/video/gamma/green", gg, "1.0");
		config->value("config/video/gamma/blue", gb, "1.0");
		Image_window8::set_gamma(atof(gr.c_str()),
		                         atof(gg.c_str()),
		                         atof(gb.c_str()));
		string  fullscreenstr;      // Check config. for fullscreen mode.
		config->value("config/video/fullscreen", fullscreenstr, "no");
		bool    fullscreen = (fullscreenstr == "yes");
		config->set("config/video/fullscreen", fullscreen ? "yes" : "no", false);

		int border_red;
		int border_green;
		int border_blue;
		config->value("config/video/game/border/red", border_red, 0);
		if (border_red < 0) border_red = 0;
		else if (border_red > 255) border_red = 255;
		config->set("config/video/game/border/red", border_red, false);

		config->value("config/video/game/border/green", border_green, 0);
		if (border_green < 0) border_green = 0;
		else if (border_green > 255) border_green = 255;
		config->set("config/video/game/border/green", border_green, false);

		config->value("config/video/game/border/blue", border_blue, 0);
		if (border_blue < 0) border_blue = 0;
		else if (border_blue > 255) border_blue = 255;
		config->set("config/video/game/border/blue", border_blue, false);

		Palette::set_border(border_red, border_green, border_blue);
		bool disable_fades;
		config->value("config/video/disable_fades", disable_fades, false);

		setup_video(fullscreen, VIDEO_INIT);
		SetIcon();
		Audio::Init();
		gwin->get_pal()->set_fades_enabled(!disable_fades);
		gwin->set_in_exult_menu(false);
	}

	SDL_SetEventFilter(nullptr, nullptr);
	// Show the banner
	game = nullptr;

	do {
		reset_system_paths();
		fontManager.reset();
		U7FileManager::get_ptr()->reset();

		if (game) {
			delete game;
			game = nullptr;
		}

		ModManager *basegame = nullptr;
		if (run_bg) {
			basegame = gamemanager->get_bg();
			arg_gamename = CFG_BG_NAME;
			run_bg = false;
		} else if (run_fov) {
			basegame = gamemanager->get_fov();
			arg_gamename = CFG_FOV_NAME;
			run_fov = false;
		} else if (run_si) {
			basegame = gamemanager->get_si();
			arg_gamename = CFG_SI_NAME;
			run_si = false;
		} else if (run_ss) {
			basegame = gamemanager->get_ss();
			arg_gamename = CFG_SS_NAME;
			run_ss = false;
		} else if (run_sib) {
			basegame = gamemanager->get_sib();
			arg_gamename = CFG_SIB_NAME;
			run_sib = false;
		}
		BaseGameInfo *newgame = nullptr;
		if (basegame || arg_gamename != "default") {
			if (!basegame)
				basegame = gamemanager->find_game(arg_gamename);
			if (basegame) {
				if (arg_modname != "default")
					// Prints error messages:
					newgame = basegame->get_mod(arg_modname);
				else
					newgame = basegame;
				arg_modname = "default";
			} else {
				cerr << "Game '" << arg_gamename << "' not found." << endl;
				newgame = nullptr;
			}
			// Prevent game from being reloaded in case the player
			// tries to return to the main menu:
			arg_gamename = "default";
		}
		if (!newgame) {
			ExultMenu exult_menu(gwin);
			newgame = exult_menu.run();
		}
		assert(newgame != nullptr);

		if (arg_buildmap >= 0) {
			BuildGameMap(newgame, arg_mapnum);
			exit(0);
		}
		if (arg_verify_files) {
			newgame->setup_game_paths();
			exit(verify_files(newgame));
		}

		Game::create_game(newgame);
		Audio *audio = Audio::get_ptr();
		audio->Init_sfx();
		MyMidiPlayer *midi = audio->get_midi();

		Setup_text(GAME_SI, Game::has_expansion(), GAME_SIB);

		// Skip splash screen?
		bool skip_splash;
		config->value("config/gameplay/skip_splash", skip_splash);

		// Make sure we have a proper palette before playing the intro.
		gwin->get_pal()->load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
		                      EXULT_FLX_EXULT0_PAL);
		gwin->get_pal()->apply();
		if (!skip_splash && (Game::get_game_type() != EXULT_DEVEL_GAME
		                     || U7exists(INTRO_DAT))) {
			if (midi) midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);
			game->play_intro();
			std::cout << "played intro" << std::endl;
		}

		if (midi) {
			if (arg_nomenu)
				midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);
			else
				midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_MAINMENU);
		}
	} while (!game || !game->show_menu(arg_nomenu));

	// Should not be needed anymore:
	delete gamemanager;
	gamemanager = nullptr;

	Audio *audio = Audio::get_ptr();
	MyMidiPlayer *midi = audio->get_midi();
	if (midi) midi->set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GAME);

	gwin->init_files();
	gwin->read_gwin();
	gwin->setup_game(arg_edit_mode);    // This will start the scene.
	// Get scale factor for mouse.
#ifdef USE_EXULTSTUDIO
#ifndef _WIN32
	SDL_GetWindowWMInfo(gwin->get_win()->get_screen_window(), &info);
	Server_init();          // Initialize server (for map-editor).
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
#else
	SDL_GetWindowWMInfo(gwin->get_win()->get_screen_window(), &info);
	hgwin = info.info.win.window;
	Server_init();          // Initialize server (for map-editor).
	OleInitialize(nullptr);
	windnd = new Windnd(hgwin,
	                    Move_dragged_shape, Move_dragged_combo,
	                    Drop_dragged_shape, Drop_dragged_chunk,
	                    Drop_dragged_npc, Drop_dragged_combo);
	if (FAILED(RegisterDragDrop(hgwin, windnd))) {
		cout << "Something's wrong with OLE2 ..." << endl;
	};
#endif
#endif
}

/*
 *  Play game.
 */

static int Play() {
	do {
		quitting_time = QUIT_TIME_NO;
		Handle_events();
		if (quitting_time == QUIT_TIME_RESTART) {
			Mouse::mouse->hide();   // Turn off mouse.
			gwin->read();   // Restart
			/////gwin->setup_game();
			//// setup_game is already being called from inside
			//// of gwin->read(), so no need to call it here, I hope...
		}
	} while (quitting_time == QUIT_TIME_RESTART);

	delete gwin;
	delete Mouse::mouse;

	Audio::Destroy();   // Deinit the sound system.
	delete keybinder;
	delete config;
	Free_text();
	fontManager.reset();
	delete game;
	return 0;
}

#ifdef USE_EXULTSTUDIO          // Shift-click means 'paint'.
/*
 *  Add a shape while map-editing.
 */

static void Paint_with_shape(
    SDL_Event &event,
    bool dragging           // Painting terrain.
) {
	static int lasttx = -1;
	static int lastty = -1;
	//int scale = gwin->get_win()->get_scale();
	//int x = event.button.x/scale, y = event.button.y/scale;
	int x;
	int y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);

	int tx = (gwin->get_scrolltx() + x / c_tilesize);
	int ty = (gwin->get_scrollty() + y / c_tilesize);
	if (dragging) {         // See if moving to a new tile.
		if (tx == lasttx && ty == lastty)
			return;
	}
	lasttx = tx;
	lastty = ty;
	int shnum = cheat.get_edit_shape();
	int frnum;
	SDL_Keymod mod = SDL_GetModState();
	if (mod & KMOD_ALT) {   // ALT?  Pick random frame.
		ShapeID id(shnum, 0);
		frnum = std::rand() % id.get_num_frames();
	} else if (mod & KMOD_CTRL) { // Cycle through frames.
		frnum = cheat.get_edit_frame();
		ShapeID id(shnum, 0);
		int nextframe = (frnum + 1) % id.get_num_frames();
		cheat.set_edit_shape(shnum, nextframe);
	} else
		frnum = cheat.get_edit_frame();
	Drop_dragged_shape(shnum, frnum, event.button.x, event.button.y);
}

/*
 *  Set a complete chunk while map-editing.
 */

static void Paint_with_chunk(
    SDL_Event &event,
    bool dragging           // Painting terrain.
) {
	static int lastcx = -1;
	static int lastcy = -1;
	int x;
	int y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	int cx = (gwin->get_scrolltx() + x / c_tilesize) / c_tiles_per_chunk;
	int cy = (gwin->get_scrollty() + y / c_tilesize) / c_tiles_per_chunk;
	if (dragging) {         // See if moving to a new chunk.
		if (cx == lastcx && cy == lastcy)
			return;
	}
	lastcx = cx;
	lastcy = cy;
	int chnum = cheat.get_edit_chunknum();
	Drop_dragged_chunk(chnum, event.button.x, event.button.y);
}

/*
 *  Select chunks.
 */

static void Select_chunks(
    SDL_Event &event,
    bool dragging,          // Painting terrain.
    bool toggle
) {
	static int lastcx = -1;
	static int lastcy = -1;
	int x;
	int y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	int cx = (gwin->get_scrolltx() + x / c_tilesize) / c_tiles_per_chunk;
	int cy = (gwin->get_scrollty() + y / c_tilesize) / c_tiles_per_chunk;
	if (dragging) {         // See if moving to a new chunk.
		if (cx == lastcx && cy == lastcy)
			return;
	}
	lastcx = cx;
	lastcy = cy;
	Map_chunk *chunk = gwin->get_map()->get_chunk(cx, cy);
	if (toggle) {
		if (!chunk->is_selected())
			cheat.add_chunksel(chunk);
		else
			chunk->set_selected(false);
	} else
		cheat.add_chunksel(chunk);
	gwin->set_all_dirty();
}

/*
 *  Select for combo.
 */
static void Select_for_combo(
    SDL_Event &event,
    bool dragging,          // Dragging to select.
    bool toggle
) {
	static Game_object *last_obj = nullptr;
	int x;
	int y;
	gwin->get_win()->screen_to_game(event.button.x, event.button.y, false, x, y);
	//int tx = (gwin->get_scrolltx() + x/c_tilesize)%c_num_tiles;
	//int ty = (gwin->get_scrollty() + y/c_tilesize)%c_num_tiles;
	Game_object *obj = gwin->find_object(x, y);
	if (obj) {
		if (dragging && obj == last_obj)
			return;
		last_obj = obj;
	} else
		return;
	ShapeID id = *obj;
	Tile_coord t = obj->get_tile();
	std::string name = get_item_name(id.get_shapenum());
	if (toggle)
		cheat.toggle_selected(obj);
	else if (!cheat.is_selected(obj)) {
		cheat.append_selected(obj);
		gwin->add_dirty(obj);
	}
	if (Object_out(client_socket,
	               toggle ? Exult_server::combo_toggle : Exult_server::combo_pick,
	               nullptr, t.tx, t.ty, t.tz, id.get_shapenum(),
	               id.get_framenum(), 0, name) == -1)
		cout << "Error sending shape to ExultStudio" << endl;
}

#endif

/*
 *  Handle events until a flag is set.
 */

static void Handle_events(
) {
	uint32 last_repaint = 0;    // For insuring animation repaints.
	uint32 last_rotate = 0;
	uint32 last_rest = 0;
#ifdef DEBUG
	uint32 last_fps = 0;
#endif
	/*
	 *  Main event loop.
	 */
	int last_x = -1;
	int last_y = -1;
	while (!quitting_time) {
#ifdef USE_EXULTSTUDIO
		Server_delay();     // Handle requests.
#else
		Delay();        // Wait a fraction of a second.
#endif
		// Mouse scale factor
		//int scale = gwin->get_fastmouse() ? 1 :
		//              gwin->get_win()->get_scale();

		Mouse::mouse->hide();       // Turn off mouse.
		Mouse::mouse_update = false;

		// Get current time.
		uint32 ticks = SDL_GetTicks();
#if defined(_WIN32) && defined(USE_EXULTSTUDIO)
		if (ticks - Game::get_ticks() < 10) {
			// Reducing processor usage with a slight delay.
			SDL_Delay(10 - (ticks - Game::get_ticks()));
			continue;
		}
#endif
		Game::set_ticks(ticks);
#ifdef DEBUG
		if (last_fps == 0 || ticks >= last_fps + 10000) {
			double fps = (gwin->blits * 1000.0) / (ticks - last_fps);
			cerr << "***#ticks = " << ticks - last_fps <<
			     ", blits = " << gwin->blits << ", ";
			cerr << "FPS:  " << fps << endl;
			last_fps = ticks;
			gwin->blits = 0;
		}
#endif

		SDL_Event event;
		while (!quitting_time && SDL_PollEvent(&event))
			Handle_event(event);

		// Animate unless dormant.
		if (gwin->have_focus() && !dragging)
			gwin->get_tqueue()->activate(ticks);

		// Moved this out of the animation loop, since we want movement to be
		// more responsive. Also, if the step delta is only 1 tile,
		// always check every loop
		if ((!gwin->is_moving() || gwin->get_step_tile_delta() == 1) &&
		        gwin->main_actor_can_act_charmed()) {
			int x;
			int y;// Check for 'stuck' Avatar.
			int ms = SDL_GetMouseState(&x, &y);
			//mouse movement needs to be adjusted for HighDPI
			gwin->get_win()->screen_to_game_hdpi(x, y, gwin->get_fastmouse(), x, y);
			if ((SDL_BUTTON(3) & ms) && !right_on_gump)
				gwin->start_actor(x, y,
				                  Mouse::mouse->avatar_speed);
			else if (ticks > last_rest) {
				int resttime = ticks - last_rest;
				gwin->get_main_actor()->resting(resttime);

				Party_manager *party_man = gwin->get_party_man();
				int cnt = party_man->get_count();
				for (int i = 0; i < cnt; i++) {
					int party_member = party_man->get_member(i);
					Actor *person = gwin->get_npc(party_member);
					if (!person)
						continue;
					person->resting(resttime);
				}
				last_rest = ticks;
			}
		}

		// handle delayed showing of clicked items (wait for possible dblclick)
		if (show_items_clicked && ticks > show_items_time) {
			gwin->show_items(show_items_x, show_items_y, false);
			show_items_clicked = false;
		}

		if (joy_aim_x != 0 || joy_aim_y != 0) {
			// Calculate the player speed
			const int speed = 200 * gwin->get_std_delay() / static_cast<int>(joy_speed_factor);

			// [re]start moving
			gwin->start_actor(joy_aim_x, joy_aim_y, speed);
		}

		// Lerping stuff...
		int lerp = gwin->is_lerping_enabled();
		bool didlerp = false;
		if (lerp) {
			// Always repaint,
			Actor *act = gwin->get_camera_actor();
			int mswait = (act->get_frame_time() * lerp) / 100;
			if (mswait <= 0) mswait = (gwin->get_std_delay() * lerp) / 100;

			// Force a reset if position changed
			if (last_x != gwin->get_scrolltx() || last_y != gwin->get_scrollty()) {
				//printf ("%i: %i -> %i, %i -> %i\n", ticks, last_x, gwin->get_scrolltx(), last_y, gwin->get_scrollty());
				gwin->lerp_reset();
				last_repaint = ticks;
			}
			last_x = gwin->get_scrolltx();
			last_y = gwin->get_scrollty();

			// Is lerping (smooth scrolling) enabled
			if (mswait && ticks < (last_repaint + mswait * 2)) {
				gwin->paint_lerped(((ticks - last_repaint) * 0x10000) / mswait);
				didlerp = true;
			}
		}
		if (!lerp || !didlerp) { // No lerping
			if (gwin->is_dirty())   // Note the ending else in the above #if!
				gwin->paint_dirty();
			// Reset it for lerping.
			last_x = last_y = -1;
		}
		Mouse::mouse->show();   // Re-display mouse.
		// Rotate less often if scaling and
		//   not paletized.
		int rot_speed = 100 << (gwin->get_win()->fast_palette_rotate() ? 0 : 1);

		if (ticks > last_rotate + rot_speed) {
			// (Blits in simulated 8-bit mode.)
			gwin->get_win()->rotate_colors(0xfc, 3, 0);
			gwin->get_win()->rotate_colors(0xf8, 4, 0);
			gwin->get_win()->rotate_colors(0xf4, 4, 0);
			gwin->get_win()->rotate_colors(0xf0, 4, 0);
			gwin->get_win()->rotate_colors(0xe8, 8, 0);
			gwin->get_win()->rotate_colors(0xe0, 8, 1);
			while (ticks > last_rotate + rot_speed)
				last_rotate += rot_speed;
			// Non palettized needs explicit blit.
			if (!gwin->get_win()->is_palettized())
				gwin->set_painted();
		}
		if (!gwin->show() &&    // Blit to screen if necessary.
		        Mouse::mouse_update)    // If not, did mouse change?
			Mouse::mouse->blit_dirty();
	}
}

static inline bool EnteredWindow(SDL_Event& event) {
	return event.window.event == SDL_WINDOWEVENT_ENTER;
}

static inline bool GainedFocus(SDL_Event& event) {
	return event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
}

static inline bool LostFocus(SDL_Event& event) {
	return event.window.event == SDL_WINDOWEVENT_FOCUS_LOST;
}

/*
 *  Handle an event.  This should work for all platforms, and should only
 *  be called in 'normal' and 'gump' modes.
 */

static void Handle_event(
    SDL_Event &event
) {

	// Mouse scale factor
	bool dont_move_mode = gwin->main_actor_dont_move();
	bool avatar_can_act = gwin->main_actor_can_act();

	// We want this
	Gump_manager *gump_man = gwin->get_gump_man();
	Gump *gump = nullptr;
	
	// For detecting double-clicks.
	static uint32 last_b1_click = 0;
	static uint32 last_b3_click = 0;
	static uint32 last_b1down_click = 0;
	//cout << "Event " << (int) event.type << " received"<<endl;
	switch (event.type) {

	// Quick saving to make sure no game progress gets lost
	// when the app goes into background
	case SDL_APP_WILLENTERBACKGROUND: {
		Game_window *gwin = Game_window::get_instance();
		try {
			gwin->write();
		} catch (exult_exception &/*e*/) {
			break;
		}
		break;
	}
	case SDL_USEREVENT: {
		if (!dragged) {
			switch (event.user.code) {
				case SHORTCUT_BAR_USER_EVENT: {
					if(g_shortcutBar) // just in case
						g_shortcutBar->onUserEvent(&event);
					break;
				}
				default:
					break;
			}
		}
		dragging = dragged = false;
		break;
	}
	case SDL_CONTROLLERAXISMOTION: {
		// Ignore axis changes on anything but a specific thumb-stick
		// on the game-controller.
		if (!(event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
			  event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)) {
			break;
		}

		SDL_GameController *input_device = SDL_GameControllerFromInstanceID(event.caxis.which);
		if (input_device && !dont_move_mode && avatar_can_act &&
			gwin->main_actor_can_act_charmed()) {
			auto get_normalized_axis = [input_device](SDL_GameControllerAxis axis){
				return SDL_GameControllerGetAxis(input_device, axis) / static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
			};
			// Collect both of the controller thumb-stick's axis values.
			// The input-event only carries one axis, and each thumb-stick
			// has two axes.  Both axes' values are needed in order to
			// call start_actor.
			float axis_x = get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTX);
			float axis_y = get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTY);

			// Dead-zone is applied to each axis, X and Y, on the game's 2d plane.
			// All axis readings below this are ignored
			constexpr const float axis_dead_zone = 0.25f;
			// Medium-speed threashold; below this, move slow
			constexpr const float axis_length_move_medium = 0.60f;
			// Fast-speed threashold; above this, move fast
			constexpr const float axis_length_move_fast = 0.90f;

			// Many analog game-controllers report non-zero axis values,
			// even when the controller isn't moving.  These non-zero
			// values can change over time, as the thumb-stick is moved
			// by the player.
			//
			// In order to prevent idle controller sticks from leading
			// to unwanted movements, axis-values that are small will
			// be ignored.  This is sometimes referred to as a
			// "dead zone".
			if (axis_dead_zone >= std::fabs(axis_x)) {
				axis_x = 0;
			}
			if (axis_dead_zone >= std::fabs(axis_y)) {
				axis_y = 0;
			}

			// Pick a player's speed-factor, depending on how much the input-stick
			// is being pushed in a direction.
			const float joy_axis_length = std::sqrt((axis_x * axis_x) + (axis_y * axis_y));
			joy_speed_factor = Mouse::fast_speed_factor;
			if (joy_axis_length < axis_length_move_medium) {
				joy_speed_factor = Mouse::slow_speed_factor;
			} else if (joy_axis_length < axis_length_move_fast) {
				joy_speed_factor = Mouse::medium_speed_factor;
			}

			// Depending on axis values, we're either going to start moving,
			// restarting moving (which looks the same as starting, to us),
			// or stopping.
			if (axis_x == 0 && axis_y == 0) {
				// Both axes are zero, so we'll stop.
				gwin->stop_actor();
				joy_aim_x = 0;
				joy_aim_y = 0;
			} else {
				// At least one axis is non-zero, so we'll [re]start moving.

				// Declare a position to aim for, in window coordinates, relative
				// to the center of the window (where the player is).
				const float aim_distance = 50.f;
				const int aim_dx = static_cast<int>(std::lround(aim_distance * axis_x));
				const int aim_dy = static_cast<int>(std::lround(aim_distance * axis_y));
				joy_aim_x = (gwin->get_width() / 2) + aim_dx;
				joy_aim_y = (gwin->get_height() / 2) + aim_dy;
			}
		}
		break;
	}
	case SDL_FINGERDOWN: {
		if ((!Mouse::use_touch_input) && (event.tfinger.fingerId != 0)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;
	}
	case SDL_MOUSEBUTTONDOWN: {
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
		if (dont_move_mode)
			break;
		last_b1down_click = SDL_GetTicks();
		if (g_shortcutBar && g_shortcutBar->handle_event(&event))
			break;
		int x;
		int y;
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
		if (event.button.button == 1) {
			Gump_button *button;
			// Allow dragging only either if the avatar can act or if map edit
			// or hackmove is on.
			if (avatar_can_act || cheat.in_hack_mover() ||
			        ((gump = gump_man->find_gump(x, y, false)) && (button = gump->on_button(x, y)) &&
			         button->is_checkmark())) { // also allow closing parent gump when clicking on checkmark
#ifdef USE_EXULTSTUDIO
				if (cheat.in_map_editor()) {
					// Paint if shift-click.
					if (cheat.get_edit_shape() >= 0 &&
					        // But always if painting.
					        (cheat.get_edit_mode() == Cheat::paint ||
					         (SDL_GetModState() & KMOD_SHIFT))) {
						Paint_with_shape(event, false);
						break;
					} else if (cheat.get_edit_chunknum() >= 0 &&
					           cheat.get_edit_mode() == Cheat::paint_chunks) {
						Paint_with_chunk(event, false);
						break;
					} else if (cheat.get_edit_mode() ==
					           Cheat::select_chunks) {
						Select_chunks(event, false,
						              (SDL_GetModState()&KMOD_CTRL) != 0);
						break;
					} else if (cheat.get_edit_mode() ==
					           Cheat::combo_pick) {
						Select_for_combo(event, false,
						                 (SDL_GetModState()&KMOD_CTRL) != 0);
						break;
					}
					// Don't drag if not in 'move' mode.
					else if (cheat.get_edit_mode() != Cheat::move)
						break;
				}
#endif
				dragging = gwin->start_dragging(x, y);
				//Mouse::mouse->set_shape(Mouse::hand);
				dragged = false;
			}
			left_down_x = x;
			left_down_y = y;
		}

		// Middle click, use targetting crosshairs.
		if (gwin->get_mouse3rd())
			if (event.button.button == 2)
				ActionTarget(nullptr);

		// Right click. Only walk if avatar can act.
		if (event.button.button == 3) {
			if (!dragging &&    // Causes crash if dragging.
			        gump_man->can_right_click_close() &&
			        gump_man->gump_mode() &&
			        gump_man->find_gump(x, y, false)) {
				gump = nullptr;
				right_on_gump = true;
			} else if (avatar_can_act && gwin->main_actor_can_act_charmed()) {
				// Try removing old queue entry.
				gwin->get_tqueue()->remove(gwin->get_main_actor());
				gwin->start_actor(x, y, Mouse::mouse->avatar_speed);
			}
		}
		break;
	}
	// two-finger scrolling of view port with SDL2.
	case SDL_FINGERMOTION: {
		if (!cheat() || !gwin->can_scroll_with_mouse()) break;
		static int numFingers = 0;
		SDL_Finger* finger0 = SDL_GetTouchFinger(event.tfinger.touchId, 0);
		if (finger0) {
			numFingers = SDL_GetNumTouchFingers(event.tfinger.touchId);
		}
		if (numFingers > 1) {
			if(event.tfinger.dy < 0) {
				ActionScrollUp(nullptr);
			}
			else if(event.tfinger.dy > 0) {
				ActionScrollDown(nullptr);
			}
			if(event.tfinger.dx > 0) {
				ActionScrollRight(nullptr);
			}
			else if(event.tfinger.dx < 0) {
				ActionScrollLeft(nullptr);
			}
		}
		break;
	}
	// Mousewheel scrolling of view port with SDL2.
	case SDL_MOUSEWHEEL: {
		if (!cheat() || !gwin->can_scroll_with_mouse()) break;
		SDL_Keymod mod = SDL_GetModState();
		if(event.wheel.y > 0) {
			if (mod & KMOD_ALT)
				ActionScrollLeft(nullptr);
			else
				ActionScrollUp(nullptr);
		}
		else if(event.wheel.y < 0) {
			if (mod & KMOD_ALT)
				ActionScrollRight(nullptr);
			else
				ActionScrollDown(nullptr);
		}
		if(event.wheel.x > 0) {
			ActionScrollRight(nullptr);
		}
		else if(event.wheel.x < 0) {
			ActionScrollLeft(nullptr);
		}
		break;
	}
	case SDL_MOUSEBUTTONUP: {
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
		if (dont_move_mode)
			break;
		int x ;
		int y;
		gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);

		if (event.button.button == 3) {
			uint32 curtime = SDL_GetTicks();
			// If showing gumps, ignore all double-right-click results
			if (gump_man->gump_mode()) {
				if (right_on_gump &&
				        (gump = gump_man->find_gump(x, y, false))) {
					TileRect dirty = gump->get_dirty();
					gwin->add_dirty(dirty);
					gump_man->close_gump(gump);
					gump = nullptr;
					right_on_gump = false;
				}
			} else if (avatar_can_act) {
				// Last right click not within .5 secs (not a doubleclick or rapid right clicking)?
				if (gwin->get_allow_right_pathfind() == 1 && curtime - last_b3_click > 500 && gwin->main_actor_can_act_charmed())
					gwin->start_actor_along_path(x, y,
					                             Mouse::mouse->avatar_speed);

				// Last right click within .5 secs (doubleclick)?
				else if (gwin->get_allow_right_pathfind() == 2 && curtime - last_b3_click < 500 && gwin->main_actor_can_act_charmed())
					gwin->start_actor_along_path(x, y,
					                             Mouse::mouse->avatar_speed);

				else {
					gwin->stop_actor();
					if (Combat::is_paused() && gwin->in_combat())
						gwin->paused_combat_select(x, y);
				}
			}
			last_b3_click = curtime;
		} else if (event.button.button == 1) {
			uint32 curtime = SDL_GetTicks();
			bool click_handled = false;
			if (dragging) {
				click_handled = gwin->drop_dragged(x, y, dragged);
				Mouse::mouse->set_speed_cursor();
			}
			if (g_shortcutBar && g_shortcutBar->handle_event(&event))
				break;
			// Last click within .5 secs?
			if (curtime - last_b1_click < 500 &&
			        left_down_x - 1 <= x && x <= left_down_x + 1 &&
			        left_down_y - 1 <= y && y <= left_down_y + 1) {
				dragging = dragged = false;
				// This function handles the trouble of deciding what to
				// do when the avatar cannot act.
				gwin->double_clicked(x, y);
				Mouse::mouse->set_speed_cursor();
				show_items_clicked = false;
				break;
			}
			if (!dragging || !dragged)
				last_b1_click = curtime;

		    if (gwin->get_touch_pathfind() && !click_handled &&
			        (curtime - last_b1down_click > 500) && avatar_can_act &&
			        gwin->main_actor_can_act_charmed() && !dragging &&
			        !gump_man->find_gump(x, y, false)) {
				gwin->start_actor_along_path(x, y, Mouse::mouse->avatar_speed);
				dragging = dragged = false;
				break;
			}

			if (!click_handled && avatar_can_act &&
			        left_down_x - 1 <= x && x <= left_down_x + 1 &&
			        left_down_y - 1 <= y && y <= left_down_y + 1) {
				show_items_x = x;
				show_items_y = y;
				// Identify item(s) clicked on.
				if (cheat.in_map_editor())
					gwin->show_items(x, y, (SDL_GetModState() & KMOD_CTRL) != 0);
				else {
					show_items_time = curtime + 500;
					show_items_clicked = true;
				}
			}
			dragging = dragged = false;
		}
		break;
	}
	case SDL_MOUSEMOTION: {
		int mx ;
		int my;
		if ((Mouse::use_touch_input == true) && (event.motion.which != SDL_TOUCH_MOUSEID))
			Mouse::use_touch_input = false;
		gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

		Mouse::mouse->move(mx, my);
		if (!dragging)
			Mouse::mouse->set_speed_cursor();
		Mouse::mouse_update = true; // Need to blit mouse.
		if (right_on_gump &&
		        !(gump_man->can_right_click_close() &&
		          gump_man->gump_mode() &&
		          gump_man->find_gump(mx, my, false))) {
			right_on_gump = false;
		}

		// Dragging with left button?
		if (event.motion.state & SDL_BUTTON(1)) {
#ifdef USE_EXULTSTUDIO          // Painting?
			if (cheat.in_map_editor()) {
				if (cheat.get_edit_shape() >= 0 &&
				        (cheat.get_edit_mode() == Cheat::paint ||
				         (SDL_GetModState() & KMOD_SHIFT))) {
					Paint_with_shape(event, true);
					break;
				} else if (cheat.get_edit_chunknum() >= 0 &&
				           cheat.get_edit_mode() == Cheat::paint_chunks) {
					Paint_with_chunk(event, true);
					break;
				} else if (cheat.get_edit_mode() ==
				           Cheat::select_chunks) {
					Select_chunks(event, true,
					              (SDL_GetModState()&KMOD_CTRL) != 0);
					break;
				} else if (cheat.get_edit_mode() ==
				           Cheat::combo_pick) {
					Select_for_combo(event, true,
					                 (SDL_GetModState()&KMOD_CTRL) != 0);
					break;
				}
			}
#endif
			dragged = gwin->drag(mx, my);
		}
		// Dragging with right?
		else if ((event.motion.state & SDL_BUTTON(3)) && !right_on_gump) {
			if (avatar_can_act && gwin->main_actor_can_act_charmed())
				gwin->start_actor(mx, my, Mouse::mouse->avatar_speed);
		}
#ifdef USE_EXULTSTUDIO          // Painting?
		else if (cheat.in_map_editor() &&
		         cheat.get_edit_shape() >= 0 &&
		         (cheat.get_edit_mode() == Cheat::paint ||
		          (SDL_GetModState() & KMOD_SHIFT))) {
			static int prevx = -1;
			static int prevy = -1;
			Move_dragged_shape(cheat.get_edit_shape(),
			                   cheat.get_edit_frame(),
			                   event.motion.x, event.motion.y,
			                   prevx, prevy, false);
			prevx = event.motion.x;
			prevy = event.motion.y;
		}
#endif
		break;
	}
	case SDL_WINDOWEVENT:
		if (EnteredWindow(event)) {
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			gwin->get_win()->screen_to_game(x, y, gwin->get_fastmouse(), x, y);
			Mouse::mouse->set_location(x, y);
			gwin->set_painted();
		}

		if (GainedFocus(event))
			gwin->get_focus();
		else if (LostFocus(event))
			gwin->lose_focus();
		break;
	case SDL_QUIT:
		gwin->get_gump_man()->okay_to_quit();
		break;
	case SDL_KEYDOWN:       // Keystroke.
	case SDL_KEYUP:
		if (!dragging &&    // ESC while dragging causes crashes.
		        !gwin->get_gump_man()->handle_kbd_event(&event))
			keybinder->HandleEvent(event);
		break;
	case SDL_DROPFILE: {
#ifdef USE_EXULTSTUDIO
#ifndef _WIN32
		int x;
		int y;
		SDL_GetMouseState(&x, &y);
#ifdef DEBUG
		cout << "(EXULT) SDL_DROPFILE Event, type = " << event.drop.type
		     << ", file (" << strlen(event.drop.file) << ") = '" << event.drop.file
		     << "', at x = " << x << ", y = " << y << endl;
#endif
		const unsigned char *data = reinterpret_cast<const unsigned char *>(event.drop.file);
		if (Is_u7_shapeid(data) == true) {
			// Get shape info.
			int file, shape, frame;
			Get_u7_shapeid(data, file, shape, frame);
			cout << "(EXULT) SDL_DROPFILE Event, Shape: file = " << file
			     << ", shape = " << shape << ", frame = " << frame << endl;
			if (shape >= 0) {   // Dropping a shape?
				if (file == U7_SHAPE_SHAPES)
					// For now, just allow "shapes.vga".
					Drop_dragged_shape(shape, frame, x, y);
			}
		} else if (Is_u7_chunkid(data) == true) {
			// A whole chunk.
			int chunknum;
			Get_u7_chunkid(data, chunknum);
			cout << "(EXULT) SDL_DROPFILE Event, Chunk: num = " << chunknum << endl;
			if (chunknum >= 0) { // A whole chunk.
				Drop_dragged_chunk(chunknum, x, y);
			}
		} else if (Is_u7_npcid(data) == true) {
			int npcnum;
			Get_u7_npcid(data, npcnum);
			cout << "(EXULT) SDL_DROPFILE Event, Npc: num = " << npcnum << endl;
			if (npcnum >= 0) { // An NPC.
				Drop_dragged_npc(npcnum, x, y);
			}
		} else if (Is_u7_comboid(data) == true) {
			int combo_xtiles, combo_ytiles, combo_tiles_right, combo_tiles_below, combo_cnt;
			U7_combo_data *combo;
			Get_u7_comboid(data, combo_xtiles, combo_ytiles,
			               combo_tiles_right, combo_tiles_below, combo_cnt, combo);
			cout << "(EXULT) SDL_DROPFILE Event, Combo: xtiles = " << combo_xtiles
			     << ", ytiles = " << combo_ytiles << ", tiles_right = " << combo_tiles_right
			     << ", tiles_below = " << combo_tiles_below
			     << ", count = " << combo_cnt << endl;
			if (combo_cnt >= 0 && combo) {
				Drop_dragged_combo(combo_cnt, combo, x, y);
			}
			delete[] combo;
		}
#ifdef DEBUG
		cout << "(EXULT) SDL_DROPFILE Event complete" << endl;
#endif
#endif
#endif
		break;
	}
	}
}


/*
 *  Wait for a click, or optionally, a kbd. chr.
 *
 *  Output: false if user hit ESC.
 */
static bool Get_click(
    int &x, int &y,
    char *chr,          // Char. returned if not null.
    bool drag_ok,           // Okay to drag/close while here.
    bool rotate_colors      // If the palette colors should rotate.
) {
	dragging = false;       // Init.
	uint32 last_rotate = 0;
	g_waiting_for_click = true;
	while (true) {
		SDL_Event event;
		Delay();        // Wait a fraction of a second.

		uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		Mouse::mouse->hide();       // Turn off mouse.
		Mouse::mouse_update = false;

		if (rotate_colors) {
			int rot_speed = 100 << (gwin->get_win()->fast_palette_rotate() ? 0 : 1);
			if (ticks > last_rotate + rot_speed) {
				// (Blits in simulated 8-bit mode.)
				gwin->get_win()->rotate_colors(0xfc, 3, 0);
				gwin->get_win()->rotate_colors(0xf8, 4, 0);
				gwin->get_win()->rotate_colors(0xf4, 4, 0);
				gwin->get_win()->rotate_colors(0xf0, 4, 0);
				gwin->get_win()->rotate_colors(0xe8, 8, 0);
				gwin->get_win()->rotate_colors(0xe0, 8, 1);
				while (ticks > last_rotate + rot_speed)
					last_rotate += rot_speed;
				// Non palettized needs explicit blit.
				if (!gwin->get_win()->is_palettized())
					gwin->set_painted();
			}
		}

		// Mouse scale factor
		static bool rightclick;
		while (SDL_PollEvent(&event))
			switch (event.type) {
			case SDL_MOUSEBUTTONDOWN:
				SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
				if (g_shortcutBar && g_shortcutBar->handle_event(&event))
					break;
				if (event.button.button == 3)
					rightclick = true;
				else if (drag_ok && event.button.button == 1) {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
					dragging = gwin->start_dragging(x, y);
					dragged = false;
				}
				break;
			case SDL_MOUSEBUTTONUP:
				SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
				if (g_shortcutBar && g_shortcutBar->handle_event(&event))
					break;
				if (event.button.button == 1) {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), x, y);
					bool drg = dragging;
					bool drged = dragged;
					dragging = dragged = false;
					if (!drg ||
					        !gwin->drop_dragged(x, y, drged)) {
						if (chr) *chr = 0;
						g_waiting_for_click = false;
						return true;
					}
				} else if (event.button.button == 3) {
					// Just stop.  Don't get followers!
					gwin->get_main_actor()->stop();
					if (gwin->get_mouse3rd() && rightclick) {
						rightclick = false;
						g_waiting_for_click = false;
						return false;
					}
				}
				break;
			case SDL_MOUSEMOTION: {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse->move(mx, my);
				Mouse::mouse_update = true;
				if (drag_ok &&
				        (event.motion.state & SDL_BUTTON(1)))
					dragged = gwin->drag(mx, my);
				break;
			}
			case SDL_KEYDOWN: {
				//+++++ convert to unicode first?
				int c = event.key.keysym.sym;
				switch (c) {
				case SDLK_ESCAPE:
					g_waiting_for_click = false;
					return false;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
				case SDLK_RCTRL:
				case SDLK_LCTRL:
				case SDLK_RALT:
				case SDLK_LALT:
				case SDLK_RGUI:
				case SDLK_LGUI:
				case SDLK_NUMLOCKCLEAR:
				case SDLK_CAPSLOCK:
				case SDLK_SCROLLLOCK:
					break;
				default:
					if (keybinder->IsMotionEvent(event))
						break;
					if ((c == 's') &&
					        (event.key.keysym.mod & KMOD_ALT) &&
					        (event.key.keysym.mod & KMOD_CTRL)) {
						make_screenshot(true);
						break;
					}
					if (chr) { // Looking for a character?
						*chr = (event.key.keysym.mod &
						        KMOD_SHIFT)
						       ? toupper(c) : c;
						g_waiting_for_click = false;
						return true;
					}
					break;
				}
				break;
			}
			case SDL_WINDOWEVENT:
				if (GainedFocus(event))
					gwin->get_focus();
				else if (LostFocus(event))
					gwin->lose_focus();
			}
		if (dragging)
			gwin->paint_dirty();
		Mouse::mouse->show();       // Turn on mouse.
		if (!gwin->show() &&    // Blit to screen if necessary.
		        Mouse::mouse_update)
			Mouse::mouse->blit_dirty();
	}
	g_waiting_for_click = false;
	return false;         // Shouldn't get here.
}

/*
 *  Get a click, or, optionally, a keyboard char.
 *
 *  Output: false if user hit ESC.
 *      Chr gets keyboard char., or 0 if it's was a mouse click.
 */

bool Get_click(
    int &x, int &y,         // Location returned (if not ESC).
    Mouse::Mouse_shapes shape,  // Mouse shape to use.
    char *chr,          // Char. returned if not null.
    bool drag_ok,           // Okay to drag/close while here.
    Paintable *paint,       // Paint this over everything else.
    bool rotate_colors      // If the palette colors should rotate.
) {
	if (chr)
		*chr = 0;       // Init.
	Mouse::Mouse_shapes saveshape = Mouse::mouse->get_shape();
	if (shape != Mouse::dontchange)
		Mouse::mouse->set_shape(shape);
	if (paint)
		paint->paint();
	Mouse::mouse->show();
	gwin->show(true);          // Want to see new mouse.
	gwin->get_tqueue()->pause(Game::get_ticks());
	bool ret = Get_click(x, y, chr, drag_ok, rotate_colors);
	gwin->get_tqueue()->resume(Game::get_ticks());
	Mouse::mouse->set_shape(saveshape);
	return ret;
}

/*
 *  Wait for someone to stop walking.  If a timeout is given, at least
 *  one animation cycle will still always occur.
 */

void Wait_for_arrival(
    Actor *actor,           // Whom to wait for.
    const Tile_coord& dest,        // Where he's going.
    long maxticks           // Max. # msecs. to wait, or 0.
) {
	// Mouse scale factor
	int mx;
	int my;

	bool os = Mouse::mouse->is_onscreen();
	uint32 last_repaint = 0;    // For insuring animation repaints.
	Actor_action *orig_action = actor->get_action();
	uint32 stop_time = SDL_GetTicks() + maxticks;
	bool timeout = false;
	while (actor->is_moving() && actor->get_action() == orig_action &&
	        actor->get_tile() != dest && !timeout) {
		Delay();        // Wait a fraction of a second.

		Mouse::mouse->hide();       // Turn off mouse.
		Mouse::mouse_update = false;

		SDL_Event event;
		while (SDL_PollEvent(&event))
			switch (event.type) {
			case SDL_MOUSEMOTION:
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse->move(mx, my);
				Mouse::mouse_update = true;
				break;
			}
		// Get current time, & animate.
		uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		if (maxticks && ticks > stop_time)
			timeout = true;
		if (gwin->have_focus())
			gwin->get_tqueue()->activate(ticks);
		// Show animation every 1/20 sec.
		if (ticks > last_repaint + 50 || gwin->was_painted()) {
			gwin->paint_dirty();
			while (ticks > last_repaint + 50)last_repaint += 50;
		}

		Mouse::mouse->show();       // Re-display mouse.
		if (!gwin->show() &&    // Blit to screen if necessary.
		        Mouse::mouse_update)    // If not, did mouse change?
			Mouse::mouse->blit_dirty();
	}

	if (!os)
		Mouse::mouse->hide();

}

/*
 *  Shift 'wizard's view' according to mouse position.
 */

static void Shift_wizards_eye(
    int mx, int my
) {
	// Figure dir. from center.
	int cx = gwin->get_width() / 2;
	int cy = gwin->get_height() / 2;
	int dy = cy - my;
	int dx = mx - cx;
	Direction dir = Get_direction_NoWrap(dy, dx);
	static int deltas[16] = {0, -1, 1, -1, 1, 0, 1, 1, 0, 1,
	                         -1, 1, -1, 0, -1, -1
	                        };
	int dirx = deltas[2 * dir];
	int diry = deltas[2 * dir + 1];
	if (dirx == 1)
		gwin->view_right();
	else if (dirx == -1)
		gwin->view_left();
	if (diry == 1)
		gwin->view_down();
	else if (diry == -1)
		gwin->view_up();
}

/*
 *  Do the 'wizard's eye' spell by letting the user browse around.
 */

void Wizard_eye(
    long msecs          // Length of time in milliseconds.
) {
	// Center of screen.
	int cx = gwin->get_width() / 2;
	int cy = gwin->get_height() / 2;

	bool os = Mouse::mouse->is_onscreen();
	uint32 last_repaint = 0;    // For insuring animation repaints.
	uint32 stop_time = SDL_GetTicks() + msecs;
	bool timeout = false;
	while (!timeout) {
		if (touchui != nullptr) {
			touchui->showButtonControls();
			touchui->hideGameControls();
		}

		Delay();        // Wait a fraction of a second.

		Mouse::mouse->hide();       // Turn off mouse.
		Mouse::mouse_update = false;
		SDL_Event event;
		while (SDL_PollEvent(&event))
			switch (event.type) {
			case SDL_FINGERMOTION: {
				if(event.tfinger.dy > 0) {
					gwin->view_down();
				}
				else if(event.tfinger.dy < 0) {
					gwin->view_up();
				}
				if(event.tfinger.dx > 0) {
					gwin->view_right();
				}
				else if(event.tfinger.dx < 0) {
					gwin->view_left();
				}
				break;
			}
			case SDL_MOUSEMOTION: {
				int mx;
				int my;
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), mx, my);

				Mouse::mouse->move(mx, my);
				Mouse::mouse->set_shape(
				    Mouse::mouse->get_short_arrow(
				        Get_direction_NoWrap(cy - my, mx - cx)));
				Mouse::mouse_update = true;
				break;
			}
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
					timeout = true;
			}
		// Get current time, & animate.
		uint32 ticks = SDL_GetTicks();
		Game::set_ticks(ticks);
		if (ticks > stop_time)
			timeout = true;
		if (gwin->have_focus())
			gwin->get_tqueue()->activate(ticks);
		// Show animation every 1/20 sec.
		if (ticks > last_repaint + 50 || gwin->was_painted()) {
			// Right mouse button down?
			int x;
			int y;
			int ms = SDL_GetMouseState(&x, &y);
			int mx;
			int my;
			//mouse movement of the eye needs to adjust for HighDPI
			gwin->get_win()->screen_to_game_hdpi(x, y, gwin->get_fastmouse(), mx, my);
			if (SDL_BUTTON(3) & ms)
				Shift_wizards_eye(mx, my);
			gwin->set_all_dirty();
			gwin->paint_dirty();
			// Paint sprite over view.
			ShapeID eye(10, 0, SF_SPRITES_VGA);
			Shape_frame *spr = eye.get_shape();
			// Center it.
			int w = gwin->get_width();
			int h = gwin->get_height();
			int sw = spr->get_width();
			int sh = spr->get_height();
			int topx = (w - sw) / 2;
			int topy = (h - sh) / 2;
			eye.paint_shape(topx + spr->get_xleft(),
			                topy + spr->get_yabove());
			int sizex = (w - 320) / 2;
			int sizey = (h - 200) / 2;
			if (sizey) { // Black-fill area outside original resolution.
				gwin->get_win()->fill8(0, w, sizey, 0, 0);
				gwin->get_win()->fill8(0, w, sizey, 0, h - sizey);
			}
			if (sizex) {
				gwin->get_win()->fill8(0, sizex, 200, 0, sizey);
				gwin->get_win()->fill8(0, sizex, 200, w - sizex, sizey);
			}
			while (ticks > last_repaint + 50)last_repaint += 50;
		}

		Mouse::mouse->show();       // Re-display mouse.
		if (!gwin->show() &&    // Blit to screen if necessary.
		        Mouse::mouse_update)    // If not, did mouse change?
			Mouse::mouse->blit_dirty();
		if (touchui != nullptr) {
			touchui->showGameControls();
		}
	}

	if (!os)
		Mouse::mouse->hide();
	gwin->center_view(gwin->get_main_actor()->get_tile());
}


int find_resolution(int w, int h, int s) {
	int res = 0;
	for (int i = 0; i < num_res; i++) {
		if (res_list[i].x == w && res_list[i].y == h && res_list[i].scale == s)
			res = i;
	}
	return res;
}

void increase_scaleval() {
	if (!cheat()) return;

	current_scaleval++;
	if (current_scaleval >= 9)
		current_scaleval = 1;
	set_scaleval(current_scaleval);
}

void decrease_scaleval() {
	if (!cheat()) return;

	current_scaleval--;
	if (current_scaleval < 1)
		current_scaleval = 8;
	set_scaleval(current_scaleval);
}

void set_scaleval(int new_scaleval) {
	int scaler = gwin->get_win()->get_scaler();
	bool fullscreen = gwin->get_win()->is_fullscreen();
	if (new_scaleval >= 1 && !fullscreen && scaler == Image_window::point && cheat.in_map_editor()) {
		current_scaleval = new_scaleval;
		bool share_settings;
		config->value("config/video/share_video_settings", share_settings, true);
		const string &vidStr = (fullscreen || share_settings) ?
		                       "config/video" : "config/video/window";
		int resx;
		int resy;
		config->value(vidStr + "/display/width", resx);
		config->value(vidStr + "/display/height", resy);

		// for Studio zooming we set game area to auto, fill quality to point,
		// fill mode to fill and increase/decrease the scale value
		gwin->resized(resx,
		              resy,
		              fullscreen,
		              0, 0,
		              current_scaleval, scaler,
		              Image_window::Fill,
		              Image_window::point);
	}
}

void make_screenshot(bool silent) {
	// TODO: Maybe some form of "My Pictures" on Windows.
	string savegamepath = get_system_path("<SAVEGAME>");
	size_t strsize = savegamepath.size() + 20;
	char *fn = new char[strsize];
	bool namefound = false;
	Effects_manager *eman = gwin->get_effects();

	// look for the next available exult???.pcx or .png file
	for (int i = 0; i < 1000 && !namefound; i++) {
		snprintf(fn, strsize, "%s/exult%03i." EXULT_IMAGE_SUFFIX, savegamepath.c_str(), i);
		FILE *f = fopen(fn, "rb");
		if (f) {
			fclose(f);
		} else {
			namefound = true;
		}
	}

	if (!namefound) {
		if (!silent) eman->center_text("Too many screenshots");
	} else {
		SDL_RWops *dst = SDL_RWFromFile(fn, "wb");

		if (gwin->get_win()->screenshot(dst)) {
			cout << "Screenshot saved in " << fn << endl;
			if (!silent) eman->center_text("Screenshot");
		} else {
			if (!silent) eman->center_text("Screenshot failed");
		}
	}
	delete [] fn;
}

void change_gamma(bool down) {
	double r;
	double g;
	double b;
	char text[256];
	const double delta = down ? 0.05 : -0.05;
	Image_window8::get_gamma(r, g, b);
	Image_window8::set_gamma(r + delta, g + delta, b + delta);
	gwin->get_pal()->apply(true);   // So new brightness applies.

	// Message
	Image_window8::get_gamma(r, g, b);
	snprintf(text, 256, "Gamma Set to R: %01.2f G: %01.2f B: %01.2f", r, g, b);
	gwin->get_effects()->center_text(text);

	int igam = std::lround(r * 10000);
	snprintf(text, 256, "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/red", text, true);

	igam = std::lround(b * 10000);
	snprintf(text, 256, "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/green", text, true);

	igam = std::lround(g * 10000);
	snprintf(text, 256, "%d.%04d", igam / 10000, igam % 10000);
	config->set("config/video/gamma/blue", text, true);
}

void BuildGameMap(BaseGameInfo *game, int mapnum) {
	// create 2048x2048 screenshots of the full Ultima 7 map.
	// WARNING!! Takes up lots of memory and diskspace!
	if (arg_buildmap >= 0) {
		int maplift = 16;
		switch (arg_buildmap) {
		case 0:
			maplift = 16;
			break;
		case 1:
			maplift = 10;
			break;
		case 2:
			maplift = 5;
			break;
		}
		int w;
		int h;
		int sc;
		int sclr;
		h = w = c_tilesize * c_tiles_per_schunk;
		sc = 1;
		sclr = Image_window::point;
		Image_window8::set_gamma(1, 1, 1);
		Image_window::FillMode fillmode = Image_window::Fit;

		//string    fullscreenstr;      // Check config. for fullscreen mode.
		//config->value("config/video/fullscreen",fullscreenstr,"no");
		// set windowed mode
		//config->set("config/video/fullscreen","no",false);
		gwin = new Game_window(w, h, false, w, h, sc, sclr, fillmode, sclr);
		// restore original fullscreen setting
		//config->set("config/video/fullscreen",fullscreenstr,true);
		Audio::Init();
		current_res = find_resolution(w, h, sc);
		Game::create_game(game);
		gwin->init_files(false); //init, but don't show plasma
		gwin->get_map()->init();// +++++Got to clean this up.
		gwin->set_map(mapnum);
		gwin->get_pal()->set(0);
		string savegamepath = get_system_path("<SAVEGAME>");
		for (int x = 0; x < c_num_chunks / c_chunks_per_schunk; x++) {
			for (int y = 0; y < c_num_chunks / c_chunks_per_schunk; y++) {
				gwin->paint_map_at_tile(0, 0, w, h, x * c_tiles_per_schunk, y * c_tiles_per_schunk, maplift);
				size_t strsize = savegamepath.size() + 20;
				char *fn = new char[strsize];
				snprintf(fn, strsize, "%s/u7map%02x." EXULT_IMAGE_SUFFIX, savegamepath.c_str(),(12*y)+x);
				SDL_RWops *dst = SDL_RWFromFile(fn, "wb");
				cerr << x << "," << y << ": ";
				gwin->get_win()->screenshot(dst);
			}
		}
		cout << "--buildmap saved the map screenshots in " << savegamepath << endl;
		Audio::Destroy();
		exit(0);
	}
}

/*
 *  Most of the game setable video configuration stuff is stored here so
 *  it isn't duplicated all over the place. fullscreen is determined
 *  before coming here. config->write_back() is done after (if needed).
 *  video_init does save before trying to open the Game_window just in case
 */
void setup_video(bool fullscreen, int setup_video_type, int resx, int resy,
                 int gw , int gh, int scaleval, int scaler,
                 Image_window::FillMode fillmode, int fill_scaler) {
	string fmode_string;
	string sclr;
	string scalerName;
	string fillScalerName;
	bool video_init = false;
	bool set_config = false;
	bool change_gwin = false;
	bool menu_init = false;
	bool read_config = false;
	if (setup_video_type == VIDEO_INIT)
		read_config = video_init = set_config = true;
	else if (setup_video_type == TOGGLE_FULLSCREEN)
		read_config = change_gwin = true;
	else if (setup_video_type == MENU_INIT)
		read_config = menu_init = true;
	else if (setup_video_type == SET_CONFIG)
		set_config = true;
	bool share_settings;
	config->value("config/video/share_video_settings", share_settings, true);
	const string vidStr((fullscreen || share_settings) ?
	                       "config/video" : "config/video/window");
	bool high_dpi;
	config->value("config/video/highdpi", high_dpi, true);
	if (read_config) {
#ifdef DEBUG
		cout << "Reading video menu adjustable configuration options" << endl;
#endif
		// Default resolution is now 320x240 with 2x scaling
		int w = 320;
		int h = 240;
#if defined(__IPHONEOS__) || defined(ANDROID)
		SDL_DisplayMode dispmode;
		if (SDL_GetDesktopDisplayMode(0, &dispmode) == 0) {
			w = dispmode.w;
			h = dispmode.h;
		}
		int sc = 1;
		string default_scaler = "point";
		string default_fill_scaler = "point";
		string default_fmode = "Fill";
		fullscreen = true;
		config->set("config/video/force_bpp", 32, true);
#else
		int sc = 2;
		string default_scaler = "point";
		string default_fill_scaler = "point";
		string default_fmode = "Fit";
#endif
		string fill_scaler_str;
		if (video_init) {
			// Convert from old video dims to new
			if (config->key_exists("config/video/width")) {
				config->value("config/video/width", resx, w);
				config->remove("config/video/width", false);
			} else
				resx = w;
			if (config->key_exists("config/video/height")) {
				config->value("config/video/height", resy, h);
				config->remove("config/video/height", false);
			} else
				resy = h;
		} else {
			resx = w;
			resy = h;
		}
		config->value(vidStr + "/scale_method", sclr, default_scaler.c_str());
		config->value(vidStr + "/scale", scaleval, sc);
		scaler = Image_window::get_scaler_for_name(sclr.c_str());
		// Ensure a default scaler if a wrong scaler name is set
		if (scaler == Image_window::NoScaler)
			scaler = Image_window::get_scaler_for_name(default_scaler.c_str());
		// Ensure proper values for scaleval based on scaler.
		if (scaler == Image_window::Hq3x || scaler == Image_window::_3xBR)
			scaleval = 3;
		else if (scaler == Image_window::Hq4x || scaler == Image_window::_4xBR)
			scaleval = 4;
		else if (scaler != Image_window::point &&
		         scaler != Image_window::interlaced &&
		         scaler != Image_window::bilinear)
			scaleval = 2;
		config->value(vidStr + "/display/width", resx, 1024);
		config->value(vidStr + "/display/height", resy, 768);
		config->value(vidStr + "/game/width", gw, 320);
		config->value(vidStr + "/game/height", gh, 200);
		SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, high_dpi ? "0" : "1");
		config->value(vidStr + "/fill_mode", fmode_string, default_fmode);
		fillmode = Image_window::string_to_fillmode(fmode_string.c_str());
		if (fillmode == 0)
			fillmode = Image_window::AspectCorrectFit;
		config->value(vidStr + "/fill_scaler", fill_scaler_str, default_fill_scaler);
		fill_scaler = Image_window::get_scaler_for_name(fill_scaler_str.c_str());
		if (fill_scaler == Image_window::NoScaler)
			fill_scaler = Image_window::bilinear;
	}
	if (set_config) {
		scalerName = Image_window::get_name_for_scaler(scaler);
		fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		Image_window::fillmode_to_string(fillmode, fmode_string);
#ifdef DEBUG
		cout << "Setting video menu adjustable configuration options " <<
		     resx << " resX, " << resy <<
		     " resY, " << gw << " gameW, " << gh << " gameH, " << scaleval <<
		     " scale, " << scalerName << " scaler, " << fmode_string <<
		     " fill mode, " << fillScalerName << " fill scaler, " <<
		     (fullscreen ? "full screen" : "window") << endl;
#endif
		config->set((vidStr + "/display/width").c_str(), resx, false);
		config->set((vidStr + "/display/height").c_str(), resy, false);
		config->set((vidStr + "/game/width").c_str(), gw, false);
		config->set((vidStr + "/game/height").c_str(), gh, false);
		config->set((vidStr + "/scale").c_str(), scaleval, false);
		config->set((vidStr + "/scale_method").c_str(), scalerName , false);
		config->set((vidStr + "/fill_mode").c_str(), fmode_string, false);
		config->set((vidStr + "/fill_scaler").c_str(), fillScalerName, false);
		config->set("config/video/highdpi", high_dpi ?
		            "yes" : "no", false);
	}
	if (video_init) {
#ifdef DEBUG
		cout << "Initializing Game_window to " << resx << " resX, " << resy <<
		     " resY, " << gw << " gameW, " << gh << " gameH, " << scaleval <<
		     " scale, " << scalerName << " scaler, " << fmode_string <<
		     " fill mode, " << fillScalerName << " fill scaler, " <<
		     (fullscreen ? "full screen" : "window") << endl;
#endif
		config->set("config/video/share_video_settings", share_settings ?
		            "yes" : "no", false);
		config->write_back();
		gwin = new Game_window(resx, resy, fullscreen, gw , gh, scaleval, scaler,
		                       fillmode, fill_scaler);
		// Ensure proper clipping:
		gwin->get_win()->clear_clip();
		// is find_resolution outdated?
		current_res = find_resolution(resx, resy, scaleval);
	} else if (change_gwin) {
#ifdef DEBUG
		if (!set_config) {
			Image_window::fillmode_to_string(fillmode, fmode_string);
			scalerName = Image_window::get_name_for_scaler(scaler);
			fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		}
		cout << "Changing Game_window to " << resx << " resX, " << resy <<
		     " resY, " << gw << " gameW, " << gh << " gameH, " << scaleval <<
		     " scale, " << scalerName << " scaler, " << fmode_string <<
		     " fill mode, " << fillScalerName << " fill scaler, " <<
		     (fullscreen ? "full screen" : "window") << endl;
#endif
		gwin->resized(resx, resy, fullscreen, gw, gh, scaleval, scaler,
		              fillmode, fill_scaler);
	}
	if (menu_init) {
#ifdef DEBUG
		Image_window::fillmode_to_string(fillmode, fmode_string);
		scalerName = Image_window::get_name_for_scaler(scaler);
		fillScalerName = Image_window::get_name_for_scaler(fill_scaler);
		cout << "Initializing video options menu settings " << resx << " resX, " <<
		     resy << " resY, " << gw << " gameW, " << gh << " gameH, " << scaleval <<
		     " scale, " << scalerName << " scaler, " << fmode_string <<
		     " fill mode, " << fillScalerName << " fill scaler, " <<
		     (fullscreen ? "full screen" : "window") << endl;
#endif
		VideoOptions_gump *videoGump = VideoOptions_gump::get_instance();
		videoGump->set_scaling(scaleval - 1);
		videoGump->set_scaler(scaler);
		videoGump->set_resolution(resx << 16 | resy);
		videoGump->set_game_resolution(gw << 16 | gh);
		videoGump->set_fill_scaler(fill_scaler == Image_window::bilinear ? 1 : 0);
		videoGump->set_fill_mode(fillmode);
	}
}

#ifdef USE_EXULTSTUDIO

/*
 *  Show a grid being dragged.
 */

static void Move_grid(
    int x, int y,           // Mouse coords. within window.
    int prevx, int prevy,       // Prev. coords, or -1.
    bool ireg,          // A single IREG object?
    int xtiles, int ytiles,     // Dimension of grid to show.
    int tiles_right, int tiles_below// # tiles to show to right of and
    //   below (x, y).
) {
	//int scale = gwin->get_win()->get_scale();
	//x /= scale;           // Watch for scaled window.
	//y /= scale;
	gwin->get_win()->screen_to_game(x, y, false, x, y);

	int lift = cheat.get_edit_lift();
	x += lift * 4 - 1;      // Take lift into account, round.
	y += lift * 4 - 1;
	int tx = x / c_tilesize;    // Figure tile on ground.
	int ty = y / c_tilesize;
	tx += tiles_right;
	ty += tiles_below;
	if (prevx != -1) {      // See if moved to a new tile.
		gwin->get_win()->screen_to_game(prevx, prevy, false, prevx, prevy);
		prevx += lift * 4 - 1;  // Take lift into account, round.
		prevy += lift * 4 - 1;
		int ptx = prevx / c_tilesize;
		int pty = prevy / c_tilesize;
		ptx += tiles_right;
		pty += tiles_below;
		if (tx == ptx && ty == pty)
			return;     // Will be in same tile.
		// Repaint over old area.
		const int pad = 8;
		TileRect r((ptx - xtiles + 1)*c_tilesize - pad,
		           (pty - ytiles + 1)*c_tilesize - pad,
		           xtiles * c_tilesize + 2 * pad,
		           ytiles * c_tilesize + 2 * pad);
		r = gwin->clip_to_win(r);
		gwin->add_dirty(r);
		gwin->paint_dirty();
	}
	// First see if it's a gump.
	if (ireg && gwin->get_gump_man()->find_gump(x, y))
		return;         // Skip if so.
	tx -= xtiles - 1;       // Get top-left of footprint.
	ty -= ytiles - 1;
	// Let's try a green outline.
	int pix = Shape_manager::get_instance()->get_special_pixel(
	              POISON_PIXEL);
	Image_window8 *win = gwin->get_win();
	win->set_clip(0, 0, win->get_game_width(), win->get_game_height());
	for (int Y = 0; Y <= ytiles; Y++)
		win->fill8(pix, xtiles * c_tilesize, 1,
		           tx * c_tilesize, (ty + Y)*c_tilesize);
	for (int X = 0; X <= xtiles; X++)
		win->fill8(pix, 1, ytiles * c_tilesize,
		           (tx + X)*c_tilesize, ty * c_tilesize);
	win->clear_clip();
	gwin->set_painted();
}

/*
 *  Show where a shape dragged from a shape-chooser will go.
 *  ALSO, this is called with shape==-1 to just force a repaint.
 */

static void Move_dragged_shape(
    int shape, int frame,       // What to create, OR -1 to just
    //   repaint window.
    int x, int y,           // Mouse coords. within window.
    int prevx, int prevy,       // Prev. coords, or -1.
    bool show           // Blit window.
) {
	if (shape == -1) {
		gwin->set_all_dirty();
		return;
	}
	const Shape_info &info = ShapeID::get_info(shape);
	// Get footprint in tiles.
	int xtiles = info.get_3d_xtiles(frame);
	int ytiles = info.get_3d_ytiles(frame);
	int sclass = info.get_shape_class();
	// Is it an ireg (changeable) obj?
	bool ireg = (sclass != Shape_info::unusable &&
	             sclass != Shape_info::building);
	Move_grid(x, y, prevx, prevy, ireg, xtiles, ytiles, 0, 0);
	if (show)
		gwin->show();
}

#ifdef _WIN32
/*
 *  Show where a shape dragged from a shape-chooser will go.
 */

static void Move_dragged_combo(
    int xtiles, int ytiles,     // Dimensions in tiles.
    int tiles_right,        // Tiles right of & below hot-spot.
    int tiles_below,
    int x, int y,           // Mouse coords. within window.
    int prevx, int prevy,       // Prev. coords, or -1.
    bool show           // Blit window.
) {
	Move_grid(x, y, prevx, prevy, false, xtiles, ytiles, tiles_right,
	          tiles_below);
	if (show)
		gwin->show();
}
#endif

/*
 *  Create an object as moveable (IREG) or fixed.
 */

static Game_object_shared Create_object(
    int shape, int frame,       // What to create.
    bool &ireg          // Rets. TRUE if ireg (moveable).
) {
	const Shape_info &info = ShapeID::get_info(shape);
	int sclass = info.get_shape_class();
	// Is it an ireg (changeable) obj?
	ireg = (sclass != Shape_info::unusable &&
	        sclass != Shape_info::building);
	Game_object_shared newobj;
	if (ireg)
		newobj = gwin->get_map()->create_ireg_object(
		             info, shape, frame, 0, 0, 0);
	else
		newobj = gwin->get_map()->create_ifix_object(shape, frame);
	return newobj;
}

/*
 *  Drop a shape dragged from a shape-chooser via drag-and-drop.
 */

static void Drop_dragged_shape(
    int shape, int frame,   // What to create.
    int x, int y            // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) // Get into editing mode.
		cheat.toggle_map_editor();
	cheat.clear_selected();     // Remove old selected.
	gwin->get_map()->set_map_modified();
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	ShapeID sid(shape, frame);
	if (gwin->skip_lift == 0) { // Editing terrain?
		int tx = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
		int ty = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
		int cx = tx / c_tiles_per_chunk;
		int cy = ty / c_tiles_per_chunk;
		Map_chunk *chunk = gwin->get_map()->get_chunk(cx, cy);
		Chunk_terrain *ter = chunk->get_terrain();
		tx %= c_tiles_per_chunk;
		ty %= c_tiles_per_chunk;
		ShapeID curid = ter->get_flat(tx, ty);
		if (sid.get_shapenum() != curid.get_shapenum() ||
		        sid.get_framenum() != curid.get_framenum()) {
			ter->set_flat(tx, ty, sid);
			Game_map::set_chunk_terrains_modified();
			gwin->set_all_dirty();  // ++++++++For now.++++++++++
		}
		return;
	}
	Shape_frame *sh = sid.get_shape();
	if (!sh || !sh->is_rle())   // Flats are only for terrain.
		return;         // Shouldn't happen.
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Create shape (" << shape << '/' << frame << ')' <<
	     endl;
	bool ireg;          // Create object.
	Game_object_shared newobj = Create_object(shape, frame, ireg);
	Dragging_info drag(newobj);
	drag.drop(x, y, true);      // (Dels if it fails.)
}

/*
 *  Drop a chunk dragged from a chunk-chooser via drag-and-drop.
 */

static void Drop_dragged_chunk(
    int chunknum,           // Index in 'u7chunks'.
    int x, int y            // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) // Get into editing mode.
		cheat.toggle_map_editor();
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Set chunk (" << chunknum << ')' << endl;
	// Need chunk-coordinates.
	int tx = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
	int ty = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
	int cx = tx / c_tiles_per_chunk;
	int cy = ty / c_tiles_per_chunk;
	gwin->get_map()->set_chunk_terrain(cx, cy, chunknum);
	gwin->paint();
}

/*
 *  Drop a npc dragged from a npc-chooser via drag-and-drop.
 */

static void Drop_dragged_npc(
    int npcnum,
    int x, int y            // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) // Get into editing mode.
		cheat.toggle_map_editor();
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	cout << "Last drag pos: (" << x << ", " << y << ')' << endl;
	cout << "Set npc (" << npcnum << ')' << endl;
	Actor *npc = gwin->get_npc(npcnum);
	if (!npc)
		return;
	Game_object_shared safe_npc = npc->shared_from_this();
	Game_object_shared npckeep;
	if (npc->is_pos_invalid())  // Brand new?
		npc->clear_flag(Obj_flags::dead);
	else
		npc->remove_this(&npckeep); // Remove if already on map.
	Dragging_info drag(safe_npc);
	if (drag.drop(x, y))
		npc->set_unused(false);
	gwin->paint();
}

/*
 *  Drop a combination object dragged from ExultStudio.
 */

void Drop_dragged_combo(
    int cnt,            // # shapes.
    U7_combo_data *combo,       // The shapes.
    int x, int y            // Mouse coords. within window.
) {
	if (!cheat.in_map_editor()) // Get into editing mode.
		cheat.toggle_map_editor();
	cheat.clear_selected();     // Remove old selected.
	gwin->get_win()->screen_to_game(x, y, false, x, y);
	int at_lift = cheat.get_edit_lift();
	x += at_lift * 4 - 1;   // Take lift into account, round.
	y += at_lift * 4 - 1;
	// Figure tile at mouse pos.
	int tx = (gwin->get_scrolltx() + x / c_tilesize) % c_num_tiles;
	int ty = (gwin->get_scrollty() + y / c_tilesize) % c_num_tiles;
	for (int i = 0; i < cnt; i++) {
		// Drop each shape.
		U7_combo_data &elem = combo[i];
		// Figure new tile coord.
		int ntx = (tx + elem.tx) % c_num_tiles;
		int nty = (ty + elem.ty) % c_num_tiles;
		int ntz = at_lift + elem.tz;
		if (ntz < 0)
			ntz = 0;
		ShapeID sid(elem.shape, elem.frame);
		if (gwin->skip_lift == 0) { // Editing terrain?
			int cx = ntx / c_tiles_per_chunk;
			int cy = nty / c_tiles_per_chunk;
			Map_chunk *chunk = gwin->get_map()->get_chunk(cx, cy);
			Chunk_terrain *ter = chunk->get_terrain();
			ntx %= c_tiles_per_chunk;
			nty %= c_tiles_per_chunk;
			ter->set_flat(ntx, nty, sid);
			Game_map::set_chunk_terrains_modified();
			continue;
		}
		bool ireg;      // Create object.
		Game_object_shared newobj = Create_object(elem.shape,
		                                    elem.frame, ireg);
		newobj->set_invalid();  // Not in world.
		newobj->move(ntx, nty, ntz);
		// Add to selection.
		cheat.append_selected(newobj.get());
	}
	gwin->set_all_dirty();      // For now, until we clear out grid.
}

#endif
