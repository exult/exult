/*
 *  modmgr.cc - Mod manager for Exult.
 *
 *  Copyright (C) 2006-2022  The Exult Team
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

#include "modmgr.h"

#include "Configuration.h"
#include "Flex.h"
#include "crc.h"
#include "databuf.h"
#include "exult_constants.h"
#include "fnames.h"
#include "listfiles.h"
#include "utils.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef HAVE_ZIP_SUPPORT
#  include "files/zip/unzip.h"
#  include "files/zip/zip.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::string;
using std::vector;

// BaseGameInfo: Generic information and functions common to mods and games
void BaseGameInfo::setup_game_paths() {
	// Make aliases to the current game's paths.
	clone_system_path("<STATIC>", "<" + path_prefix + "_STATIC>");
	clone_system_path("<MODS>", "<" + path_prefix + "_MODS>");

	string mod_path_tag = path_prefix;

	if (!mod_title.empty())
		mod_path_tag += ("_" + to_uppercase(static_cast<const string>(mod_title)));

	clone_system_path("<GAMEDAT>", "<" + mod_path_tag + "_GAMEDAT>");
	clone_system_path("<SAVEGAME>", "<" + mod_path_tag + "_SAVEGAME>");

	if (is_system_path_defined("<" + mod_path_tag + "_PATCH>"))
		clone_system_path("<PATCH>", "<" + mod_path_tag + "_PATCH>");
	else
		clear_system_path("<PATCH>");

	if (is_system_path_defined("<" + mod_path_tag + "_SOURCE>"))
		clone_system_path("<SOURCE>", "<" + mod_path_tag + "_SOURCE>");
	else
		clear_system_path("<SOURCE>");

	if (type != EXULT_MENU_GAME) {
		U7mkdir("<SAVEGAME>", 0755);    // make sure savegame directory exists
		U7mkdir("<GAMEDAT>", 0755);     // make sure gamedat directory exists
	}
}

static inline void ReplaceMacro(
    string &path,
    const string &srch,
    const string &repl
) {
	string::size_type pos = path.find(srch);
	if (pos != string::npos)
		path.replace(pos, srch.length(), repl);
}

// ModInfo: class that manages one mod's information
ModInfo::ModInfo(
    Exult_Game game,
	Game_Language lang,
    const string &name,
    const string &mod,
    const string &path,
    bool exp,
    bool sib,
    bool ed,
    const string &cfg
) : BaseGameInfo(game, lang, name, mod, path, "", exp, sib, false, ed, ""),
    configfile(cfg), compatible(false) {
	Configuration modconfig(configfile, "modinfo");

	string config_path;
	string default_dir;

	config_path = "mod_info/mod_title";
	default_dir = mod;
	string modname;
	modconfig.value(config_path, modname, default_dir.c_str());
	mod_title = modname;

	config_path = "mod_info/display_string";
	default_dir = "Description missing!";
	string menustr;
	modconfig.value(config_path, menustr, default_dir.c_str());
	menustring = menustr;

	config_path = "mod_info/required_version";
	default_dir = "0.0.00R";
	string modversion;
	modconfig.value(config_path, modversion, default_dir.c_str());
	if (modversion == default_dir)
		// Required version is missing; assume the mod to be incompatible
		compatible = false;
	else {
		const char *ptrmod = modversion.c_str();
		const char *ptrver = VERSION;
		char *eptrmod;
		char *eptrver;
		int modver = strtol(ptrmod, &eptrmod, 0);
		int exver = strtol(ptrver, &eptrver, 0);

		// Assume compatibility:
		compatible = true;
		// Comparing major version number:
		if (modver > exver)
			compatible = false;
		else if (modver == exver) {
			modver = strtol(eptrmod + 1, &eptrmod, 0);
			exver = strtol(eptrver + 1, &eptrver, 0);
			// Comparing minor version number:
			if (modver > exver)
				compatible = false;
			else if (modver == exver) {
				modver = strtol(eptrmod + 1, &eptrmod, 0);
				exver = strtol(eptrver + 1, &eptrver, 0);
				// Comparing revision number:
				if (modver > exver)
					compatible = false;
				else if (modver == exver) {
					string mver(to_uppercase(eptrmod));
					string ever(to_uppercase(eptrver));
					// Release vs CVS:
					if (mver == "CVS" && ever == "R")
						compatible = false;
				}
			}
		}
	}

	string tagstr(to_uppercase(static_cast<const string>(mod_title)));
	string system_path_tag(path_prefix + "_" + tagstr);
	string mods_dir("<" + path_prefix + "_MODS>");
	string data_directory(mods_dir + "/" + mod_title);
	string mods_save_dir("<" + path_prefix + "_SAVEGAME>/mods");
	string savedata_directory(mods_save_dir + "/" + mod_title);
	string mods_macro("__MODS__");
	string mod_path_macro("__MOD_PATH__");

	// Read codepage first.
	config_path = "mod_info/codepage";
	default_dir = "CP437";  // DOS code page.
	modconfig.value(config_path, codepage, default_dir.c_str());

	// Where game data is. This is defaults to a non-writable location because
	// mods_dir does too.
	config_path = "mod_info/patch";
	default_dir = data_directory + "/patch";
	string patchdir;
	modconfig.value(config_path, patchdir, default_dir.c_str());
	ReplaceMacro(patchdir, mods_macro, mods_dir);
	ReplaceMacro(patchdir, mod_path_macro, data_directory);
	add_system_path("<" + system_path_tag + "_PATCH>", get_system_path(patchdir));
	// Where usecode source is found; defaults to same as patch.
	config_path = "mod_info/source";
	string sourcedir;
	modconfig.value(config_path, sourcedir, default_dir.c_str());
	ReplaceMacro(sourcedir, mods_macro, mods_dir);
	ReplaceMacro(sourcedir, mod_path_macro, data_directory);
	add_system_path("<" + system_path_tag + "_SOURCE>", get_system_path(sourcedir));

	U7mkdir(mods_save_dir.c_str(), 0755);
	U7mkdir(savedata_directory.c_str(), 0755);

	// The following paths default to user-writable locations.
	config_path = "mod_info/gamedat_path";
	default_dir = savedata_directory + "/gamedat";
	string gamedatdir;
	modconfig.value(config_path, gamedatdir, default_dir.c_str());
	// Path 'macros' for relative paths:
	ReplaceMacro(gamedatdir, mods_macro, mods_save_dir);
	ReplaceMacro(gamedatdir, mod_path_macro, savedata_directory);
	add_system_path("<" + system_path_tag + "_GAMEDAT>", get_system_path(gamedatdir));
	U7mkdir(gamedatdir.c_str(), 0755);

	config_path = "mod_info/savegame_path";
	string savedir;
	modconfig.value(config_path, savedir, savedata_directory.c_str());
	// Path 'macros' for relative paths:
	ReplaceMacro(savedir, mods_macro, mods_save_dir);
	ReplaceMacro(savedir, mod_path_macro, savedata_directory);
	add_system_path("<" + system_path_tag + "_SAVEGAME>", get_system_path(savedir));
	U7mkdir(savedir.c_str(), 0755);

#ifdef DEBUG_PATHS
	cout << "path prefix of " << cfgname << " mod " << mod_title
	     << " is: " << system_path_tag << endl;
	cout << "setting " << cfgname
	     << " gamedat directory to: " << get_system_path(gamedatdir) << endl;
	cout << "setting " << cfgname
	     << " savegame directory to: " << get_system_path(savedir) << endl;
	cout << "setting " << cfgname
	     << " patch directory to: " << get_system_path(patchdir) << endl;
	cout << "setting " << cfgname
	     << " source directory to: " << get_system_path(sourcedir) << endl;
#endif
}

/*
 *  Return string from IDENTITY in a savegame.
 *	Also needed by ES.
 *
 *  Output: identity if found.
 *      "" if error (or may throw exception).
 *      "*" if older savegame.
 */
string get_game_identity(const char *savename, const string &title) {
	char *game_identity = nullptr;
	if (!U7exists(savename))
		return title;
	if (!Flex::is_flex(savename))
#ifdef HAVE_ZIP_SUPPORT
	{
		unzFile unzipfile = unzOpen(get_system_path(savename).c_str());
		if (unzipfile) {
			// Find IDENTITY, ignoring case.
			if (unzLocateFile(unzipfile, "identity", 2) != UNZ_OK) {
				unzClose(unzipfile);
				return "*";      // Old game.  Return wildcard.
			} else {
				unz_file_info file_info;
				unzGetCurrentFileInfo(unzipfile, &file_info, nullptr,
				                      0, nullptr, 0, nullptr, 0);
				game_identity = new char[file_info.uncompressed_size + 1];

				if (unzOpenCurrentFile(unzipfile) != UNZ_OK) {
					unzClose(unzipfile);
					delete [] game_identity;
					throw file_read_exception(savename);
				}
				unzReadCurrentFile(unzipfile, game_identity,
				                   file_info.uncompressed_size);
				if (unzCloseCurrentFile(unzipfile) == UNZ_OK)
					// 0-delimit.
					game_identity[file_info.uncompressed_size] = 0;
			}
		}
	}
#else
		return title.c_str();
#endif
	else {
		IFileDataSource in(savename);

		in.seek(0x54);          // Get to where file count sits.
		size_t numfiles = in.read4();
		in.seek(0x80);          // Get to file info.
		// Read pos., length of each file.
		auto finfo = std::make_unique<uint32[]>(2 * numfiles);
		for (size_t i = 0; i < numfiles; i++) {
			finfo[2 * i] = in.read4();  // The position, then the length.
			finfo[2 * i + 1] = in.read4();
		}
		for (size_t i = 0; i < numfiles; i++) { // Now read each file.
			// Get file length.
			size_t len = finfo[2 * i + 1];
			if (len <= 13)
				continue;
			len -= 13;
			in.seek(finfo[2 * i]);  // Get to it.
			char fname[50];     // Set up name.
			in.read(fname, 13);
			if (!strcmp("identity", fname)) {
				game_identity = new char[len];
				in.read(game_identity, len);
				break;
			}
		}
	}
	if (!game_identity)
		return title;
	// Truncate identity
	char *ptr = game_identity;
	for (; (*ptr != 0x1a && *ptr != 0x0d); ptr++)
		;
	*ptr = 0;
	string id = game_identity;
	delete [] game_identity;
	return id;
}

// ModManager: class that manages a game's modlist and paths
ModManager::ModManager(const string &name, const string &menu, bool needtitle,
                       bool silent) {
	cfgname = name;
	mod_title = "";

	// We will NOT trust config with these values.
	// We MUST NOT use path tags at this point yet!
	string game_path;
	string static_dir;
	string base_cfg_path("config/disk/game/" + cfgname);
	{
		string default_dir;
		string config_path;

		// ++++ These path settings are for that game data which requires only
		// ++++ read access. They default to a subdirectory of:
		// ++++     *nix: /usr/local/share/exult or /usr/share/exult
		// ++++     MacOS X: /Library/Application Support/Exult
		// ++++     Windows, MacOS: program path.

		// <path> setting: default is "$gameprefix".
		config_path = base_cfg_path + "/path";
		default_dir = get_system_path("<GAMEHOME>") + "/" + cfgname;
		config->value(config_path.c_str(), game_path, default_dir.c_str());

		// <static_path> setting: default is "$game_path/static".
		config_path = base_cfg_path + "/static_path";
		default_dir = game_path + "/static";
		config->value(config_path.c_str(), static_dir, default_dir.c_str());

		// Read codepage too.
		config_path = base_cfg_path + "/codepage";
		default_dir = "CP437";  // DOS code page.
		config->value(config_path, codepage, default_dir.c_str());

		// And edit flag.
		string cfgediting;
		config_path = base_cfg_path + "/editing";
		default_dir = "no";     // Not editing.
		config->value(config_path, cfgediting, default_dir.c_str());
		editing = (cfgediting == "yes");
	}

	if (!silent)
		cout << "Looking for '" << cfgname << "' at '" << game_path << "'... ";
	string initgam_path(static_dir + "/initgame.dat");
	found = U7exists(initgam_path);

	string static_identity;
	if (found) {
		static_identity = get_game_identity(initgam_path.c_str(), cfgname);
		if (!silent)
			cout << "found game with identity '" << static_identity << "'" << endl;
	} else { // New game still under development.
		static_identity = "DEVEL GAME";
		if (!silent)
			cout << "but it wasn't there." << endl;
	}

	const string mainshp = static_dir + "/mainshp.flx";
	const uint32 crc = crc32(mainshp.c_str());
	auto unknown_crc = [crc](const char *game) {
		cerr << "Warning: Unknown CRC for mainshp.flx: 0x"
				<< std::hex << crc << std::dec << std::endl;
		cerr << "Note: Guessing hacked " << game << std::endl;
	};
	string new_title;
	if (static_identity == "ULTIMA7") {
		type = BLACK_GATE;
		expansion = false;
		sibeta = false;
		switch (crc) {
		case 0x36af707f:
			// French BG
			language = FRENCH;
			path_prefix = to_uppercase(CFG_BG_FR_NAME);
			if (needtitle)
				new_title = CFG_BG_FR_TITLE;
			break;
		case 0x157ca514:
			// German BG
			language = GERMAN;
			path_prefix = to_uppercase(CFG_BG_DE_NAME);
			if (needtitle)
				new_title = CFG_BG_DE_TITLE;
			break;
		case 0x6d7b7323:
			// Spanish BG
			language = SPANISH;
			path_prefix = to_uppercase(CFG_BG_ES_NAME);
			if (needtitle)
				new_title = CFG_BG_ES_TITLE;
			break;
		default:
			unknown_crc("Black Gate");
			// FALLTHROUGH
		case 0xafc35523:
			// English BG
			language = ENGLISH;
			path_prefix = to_uppercase(CFG_BG_NAME);
			if (needtitle)
				new_title = CFG_BG_TITLE;
			break;
		};
	} else if (static_identity == "FORGE") {
		type = BLACK_GATE;
		language = ENGLISH;
		expansion = true;
		sibeta = false;
		if (crc != 0x8a74c26b) {
			unknown_crc("Forge of Virtue");
		}
		path_prefix = to_uppercase(CFG_FOV_NAME);
		if (needtitle)
			new_title = CFG_FOV_TITLE;
	} else if (static_identity == "SERPENT ISLE") {
		type = SERPENT_ISLE;
		expansion = false;
		switch (crc) {
		case 0x96f66a7a:
			// Spanish SI
			language = SPANISH;
			path_prefix = to_uppercase(CFG_SI_ES_NAME);
			if (needtitle)
				new_title = CFG_SI_ES_TITLE;
			sibeta = false;
			break;
		case 0xdbdc2676:
			// SI Beta
			language = ENGLISH;
			path_prefix = to_uppercase(CFG_SIB_NAME);
			if (needtitle)
				new_title = CFG_SIB_TITLE;
			sibeta = true;
			break;
		default:
			unknown_crc("Serpent Isle");
			// FALLTHROUGH
		case 0xf98f5f3e:
			// English SI
			language = ENGLISH;
			path_prefix = to_uppercase(CFG_SI_NAME);
			if (needtitle)
				new_title = CFG_SI_TITLE;
			sibeta = false;
			break;
		};
	} else if (static_identity == "SILVER SEED") {
		type = SERPENT_ISLE;
		language = ENGLISH;
		expansion = true;
		sibeta = false;
		if (crc != 0x3e18f9a0) {
			unknown_crc("Silver Seed");
		}
		path_prefix = to_uppercase(CFG_SS_NAME);
		if (needtitle)
			new_title = CFG_SS_TITLE;
	} else {
		type = EXULT_DEVEL_GAME;
		language = ENGLISH;
		expansion = false;
		sibeta = false;
		path_prefix = "DEVEL" + to_uppercase(name);
		new_title = menu;   // To be safe.
	}

	// If the "default" path selected above is already taken, then use a unique
	// one based on the exult.cfg entry instead.
	if (is_system_path_defined("<" + path_prefix + "_STATIC>")) {
		path_prefix = to_uppercase(name);
	}

	menustring = needtitle ? new_title : menu;
	// NOW we can store the path.
	add_system_path("<" + path_prefix + "_PATH>", game_path);
	add_system_path("<" + path_prefix + "_STATIC>", static_dir);

	{
		string src_dir;
		string patch_dir;
		string mods_dir;
		string default_dir;
		string config_path;

		// <mods> setting: default is "$game_path/mods".
		config_path = base_cfg_path + "/mods";
		default_dir = game_path + "/mods";
		config->value(config_path.c_str(), mods_dir, default_dir.c_str());
		add_system_path("<" + path_prefix + "_MODS>", mods_dir);

		// <patch> setting: default is "$game_path/patch".
		config_path = base_cfg_path + "/patch";
		default_dir = game_path + "/patch";
		config->value(config_path.c_str(), patch_dir, default_dir.c_str());
		add_system_path("<" + path_prefix + "_PATCH>", patch_dir);

		// <source> setting: default is "$game_path/source".
		config_path = base_cfg_path + "/source";
		config->value(config_path.c_str(), patch_dir, default_dir.c_str());
		add_system_path("<" + path_prefix + "_SOURCE>", src_dir);
#ifdef DEBUG_PATHS
		if (!silent) {
			cout << "path prefix of " << cfgname
			     << " is: " << path_prefix << endl;
			cout << "setting " << cfgname
			     << " static directory to: " << static_dir << endl;
			cout << "setting " << cfgname
			     << " patch directory to: " << patch_dir << endl;
			cout << "setting " << cfgname
			     << " modifications directory to: " << mods_dir << endl;
			cout << "setting " << cfgname
			     << " source directory to: " << src_dir << endl;
		}
#endif
	}

	get_game_paths(game_path);
	gather_mods();
}

void ModManager::gather_mods() {
	modlist.clear();    // Just to be on the safe side.

	FileList filenames;
	string   pathname("<" + path_prefix + "_MODS>");

	// If the dir doesn't exist, leave at once.
	if (!U7exists(pathname))
		return;

	U7ListFiles(pathname + "/*.cfg", filenames);
	int num_mods = filenames.size();

	if (num_mods > 0) {
		modlist.reserve(num_mods);
		for (int i = 0; i < num_mods; i++) {
#if 0
			// std::filesystem isn't reliably available yet, but this is much cleaner
			std::filesystem::path modcfg(filenames[i]);
			auto modtitle = modcfg.stem();
#else
			auto filename = filenames[i];
			auto pathend  = filename.find_last_of('/') + 1;
			auto modtitle = filename.substr(
					pathend, filename.length() - pathend - strlen(".cfg"));
#endif
			modlist.emplace_back(
					type, language, cfgname, modtitle, path_prefix, expansion,
					sibeta, editing, filenames[i]);
		}
	}
}

ModInfo *ModManager::find_mod(const string &name) {
	for (auto& mod : modlist)
		if (mod.get_mod_title() == name)
			return &mod;
	return nullptr;
}

int ModManager::find_mod_index(const string &name) {
	for (size_t i = 0; i < modlist.size(); i++)
		if (modlist[i].get_mod_title() == name)
			return i;
	return -1;
}

void ModManager::add_mod(const string &mod, const string &modconfig) {
	modlist.emplace_back(type, language, cfgname, mod, path_prefix,
	                          expansion, sibeta, editing, modconfig);
	store_system_paths();
}


// Checks the game 'gam' for the presence of the mod given in 'arg_modname'
// and checks the mod's compatibility. If the mod exists and is compatible with
// Exult, returns a reference to the mod; otherwise, returns the mod's parent game.
// Outputs error messages is the mod is not found or is not compatible.
BaseGameInfo *ModManager::get_mod(const string &name, bool checkversion) {
	ModInfo *newgame = nullptr;
	if (has_mods())
		newgame = find_mod(name);
	if (newgame) {
		if (checkversion && !newgame->is_mod_compatible()) {
			cerr << "Mod '" << name << "' is not compatible with this version of Exult." << endl;
			return nullptr;
		}
	}
	if (!newgame)
		cerr << "Mod '" << name << "' not found." << endl;
	return newgame;
}

/*
 *  Calculate paths for the given game, using the config file and
 *  falling back to defaults if necessary.  These are stored in
 *  per-game system_path entries, which are then used later once the
 *  game is selected.
 */
void ModManager::get_game_paths(const string &game_path) {
	string saveprefix(get_system_path("<SAVEHOME>") + "/" + cfgname);
	string default_dir;
	string config_path;
	string gamedat_dir;
	string static_dir;
	string savegame_dir;
	string base_cfg_path("config/disk/game/" + cfgname);

	// ++++ All of these are directories with read/write requirements.
	// ++++ They default to a directory in the current user's profile,
	// ++++ with Win9x and old MacOS (possibly others) being exceptions.

	// Usually for Win9x:
	if (saveprefix == ".")
		saveprefix = game_path;

	// <savegame_path> setting: default is "$dataprefix".
	config_path = base_cfg_path + "/savegame_path";
	default_dir = saveprefix;
	config->value(config_path.c_str(), savegame_dir, default_dir.c_str());
	add_system_path("<" + path_prefix + "_SAVEGAME>", savegame_dir);
	U7mkdir(savegame_dir.c_str(), 0755);

	// <gamedat_path> setting: default is "$dataprefix/gamedat".
	config_path = base_cfg_path + "/gamedat_path";
	default_dir = saveprefix + "/gamedat";
	config->value(config_path.c_str(), gamedat_dir, default_dir.c_str());
	add_system_path("<" + path_prefix + "_GAMEDAT>", gamedat_dir);
	U7mkdir(gamedat_dir.c_str(), 0755);

#ifdef DEBUG_PATHS
	cout << "setting " << cfgname
	     << " gamedat directory to: " << gamedat_dir << endl;
	cout << "setting " << cfgname
	     << " savegame directory to: " << savegame_dir << endl;
#endif
}

// GameManager: class that manages the installed games
GameManager::GameManager(bool silent) {
	games.clear();
	bg = fov = si = ss = sib = nullptr;

	// Search for games defined in exult.cfg:
	string config_path("config/disk/game");
	string game_title;
	std::vector<string> gamestrs = config->listkeys(config_path, false);
	std::vector<string> checkgames;
	checkgames.reserve(checkgames.size()+5);	// +5 in case the four below are not in the cfg.
	// The original games plus expansions.
	checkgames.emplace_back(CFG_BG_NAME);
	checkgames.emplace_back(CFG_FOV_NAME);
	checkgames.emplace_back(CFG_SI_NAME);
	checkgames.emplace_back(CFG_SS_NAME);
	checkgames.emplace_back(CFG_SIB_NAME);

	for (auto& gamestr : gamestrs) {
		if (gamestr != CFG_BG_NAME && gamestr != CFG_FOV_NAME &&
		    gamestr != CFG_SI_NAME && gamestr != CFG_SS_NAME &&
		    gamestr != CFG_SIB_NAME)
			checkgames.push_back(gamestr);
	}

	games.reserve(checkgames.size());
	int bgind = -1;
	int fovind = -1;
	int siind = -1;
	int ssind = -1;
	int sibind = -1;

	for (const auto& gameentry : checkgames) {
		// Load the paths for all games found:
		string base_title = gameentry;
		string new_title;
		to_uppercase(base_title);
		base_title += "\nMissing Title";
		string base_conf = config_path;
		base_conf += '/';
		base_conf += gameentry;
		base_conf += "/title";
		config->value(base_conf, game_title, base_title.c_str());
		bool need_title = game_title == base_title;
		// This checks static identity and sets game type.
		ModManager game = ModManager(gameentry, game_title, need_title, silent);
		if (!game.being_edited() && !game.is_there())
			continue;
		if (game.get_game_type() == BLACK_GATE) {
			if (game.have_expansion()) {
				if (fovind == -1)
					fovind = games.size();
			} else if (bgind == -1)
				bgind = games.size();
		} else if (game.get_game_type() == SERPENT_ISLE) {
			if (game.is_si_beta()) {
				if (sibind == -1)
					sibind = games.size();
			} else if (game.have_expansion()) {
				if (ssind == -1)
					ssind = games.size();
			} else if (siind == -1)
				siind = games.size();
		}

		games.push_back(game);
	}

	if (bgind >= 0)
		bg = &(games[bgind]);
	if (fovind >= 0)
		fov = &(games[fovind]);
	if (siind >= 0)
		si = &(games[siind]);
	if (ssind >= 0)
		ss = &(games[ssind]);
	if (sibind >= 0)
		sib = &(games[sibind]);

	// Sane defaults.
	add_system_path("<ULTIMA7_STATIC>", ".");
	add_system_path("<SERPENT_STATIC>", ".");
	print_found(bg, "exult_bg.flx", "Black Gate", CFG_BG_NAME, "ULTIMA7", silent);
	print_found(fov, "exult_bg.flx", "Forge of Virtue", CFG_FOV_NAME, "ULTIMA7", silent);
	print_found(si, "exult_si.flx", "Serpent Isle", CFG_SI_NAME, "SERPENT", silent);
	print_found(ss, "exult_si.flx", "Silver Seed", CFG_SS_NAME, "SERPENT", silent);
	store_system_paths();
}

void GameManager::print_found(
    ModManager *game,
    const char *flex,
    const char *title,
    const char *cfgname,
    const char *basepath,
    bool silent
) {
	char path[50];
	string cfgstr(cfgname);
	to_uppercase(cfgstr);
	snprintf(path, sizeof(path), "<%s_STATIC>/", cfgstr.c_str());

	if (game == nullptr) {
		if (!silent)
			cout << title << "   : not found ("
			     << get_system_path(path) << ")" << endl;
		return;
	}
	if (!silent)
		cout << title << "   : found" << endl;
	// This stores the BG/SI static paths (preferring the expansions)
	// for easier support of things like multiracial avatars in BG.
	char staticpath[50];
	snprintf(path, sizeof(path), "<%s_STATIC>", cfgstr.c_str());
	snprintf(staticpath, sizeof(staticpath), "<%s_STATIC>", basepath);
	clone_system_path(staticpath, path);
	if (silent)
		return;
	snprintf(path, sizeof(path), "<DATA>/%s", flex);
	if (U7exists(path))
		cout << flex << " : found" << endl;
	else
		cout << flex << " : not found ("
		     << get_system_path(path)
		     << ")" << endl;
}

ModManager *GameManager::find_game(const string &name) {
	for (auto& game : games)
		if (game.get_cfgname() == name)
			return &game;
	return nullptr;
}

int GameManager::find_game_index(const string &name) {
	for (size_t i = 0; i < games.size(); i++)
		if (games[i].get_cfgname() == name)
			return i;
	return -1;
}

void GameManager::add_game(const string &name, const string &menu) {
	games.emplace_back(name, menu, false);
	store_system_paths();
}
