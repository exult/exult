/*
 *  modmgr.cc - Mod manager for Exult.
 *
 *  Copyright (C) 2006-2025  The Exult Team
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
#	include <config.h>
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
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef HAVE_ZIP_SUPPORT
#	include "files/zip/unzip.h"
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

	if (!mod_title.empty()) {
		mod_path_tag
				+= ("_" + to_uppercase(static_cast<const string>(mod_title)));
	}

	clone_system_path("<GAMEDAT>", "<" + mod_path_tag + "_GAMEDAT>");
	clone_system_path("<SAVEGAME>", "<" + mod_path_tag + "_SAVEGAME>");

	if (is_system_path_defined("<" + mod_path_tag + "_PATCH>")) {
		clone_system_path("<PATCH>", "<" + mod_path_tag + "_PATCH>");
	} else {
		clear_system_path("<PATCH>");
	}

	if (is_system_path_defined("<" + mod_path_tag + "_SOURCE>")) {
		clone_system_path("<SOURCE>", "<" + mod_path_tag + "_SOURCE>");
	} else {
		clear_system_path("<SOURCE>");
	}

	if (type != EXULT_MENU_GAME) {
		U7mkdir("<SAVEGAME>", 0755);    // make sure savegame directory exists
		U7mkdir("<GAMEDAT>", 0755);     // make sure gamedat directory exists
	}
}

static inline void ReplaceMacro(
		string& path, const string& srch, const string& repl) {
	const string::size_type pos = path.find(srch);
	if (pos != string::npos) {
		path.replace(pos, srch.length(), repl);
	}
}

// ModInfo: class that manages one mod's information
ModInfo::ModInfo(
		Exult_Game game, Game_Language lang, const string& name,
		const string& mod, const string& path, bool exp, bool sib, bool ed,
		const string& cfg)
		: BaseGameInfo(
				  game, lang, name, mod, path, "", exp, sib, false, ed, ""),
		  configfile(cfg), compatible(false) {
	const Configuration modconfig(configfile, "modinfo");

	string config_path;
	string default_dir;

	config_path = "mod_info/mod_title";
	default_dir = mod;
	modconfig.value(config_path, mod_title, default_dir.c_str());

	config_path = "mod_info/display_string";
	default_dir = "Description missing!";
	modconfig.value(config_path, menustring, default_dir.c_str());

	config_path = "mod_info/required_version";
	string modversion;
	modconfig.value(config_path, modversion, "");
	compatible = is_mod_compatible(modversion);

	const string tagstr(to_uppercase(static_cast<const string>(mod_title)));
	const string system_path_tag(path_prefix + "_" + tagstr);
	const string mods_dir("<" + path_prefix + "_MODS>");
	const string data_directory(mods_dir + "/" + mod_title);
	const string mods_save_dir("<" + path_prefix + "_SAVEGAME>/mods");
	const string savedata_directory(mods_save_dir + "/" + mod_title);
	const string mods_macro("__MODS__");
	const string mod_path_macro("__MOD_PATH__");

	// Read codepage first.
	config_path = "mod_info/codepage";
	default_dir = "ASCII";    // Ultima VII 7-bit ASCII code page.
	modconfig.value(config_path, codepage, default_dir.c_str());

	// Where game data is. This is defaults to a non-writable location because
	// mods_dir does too.
	config_path = "mod_info/patch";
	default_dir = data_directory + "/patch";
	string patchdir;
	modconfig.value(config_path, patchdir, default_dir.c_str());
	ReplaceMacro(patchdir, mods_macro, mods_dir);
	ReplaceMacro(patchdir, mod_path_macro, data_directory);
	add_system_path(
			"<" + system_path_tag + "_PATCH>", get_system_path(patchdir));
	// Where usecode source is found; defaults to same as patch.
	config_path = "mod_info/source";
	string sourcedir;
	modconfig.value(config_path, sourcedir, default_dir.c_str());
	ReplaceMacro(sourcedir, mods_macro, mods_dir);
	ReplaceMacro(sourcedir, mod_path_macro, data_directory);
	add_system_path(
			"<" + system_path_tag + "_SOURCE>", get_system_path(sourcedir));

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
	add_system_path(
			"<" + system_path_tag + "_GAMEDAT>", get_system_path(gamedatdir));
	U7mkdir(gamedatdir.c_str(), 0755);

	config_path = "mod_info/savegame_path";
	string savedir;
	modconfig.value(config_path, savedir, savedata_directory.c_str());
	// Path 'macros' for relative paths:
	ReplaceMacro(savedir, mods_macro, mods_save_dir);
	ReplaceMacro(savedir, mod_path_macro, savedata_directory);
	add_system_path(
			"<" + system_path_tag + "_SAVEGAME>", get_system_path(savedir));
	U7mkdir(savedir.c_str(), 0755);

	config_path = "mod_info/skip_splash";
	string skip_splash_str;
	has_force_skip_splash = modconfig.key_exists(config_path);
	if (has_force_skip_splash) {
		modconfig.value(config_path, skip_splash_str);
		force_skip_splash
				= (skip_splash_str == "yes" || skip_splash_str == "true");
	} else {
		force_skip_splash = false;
	}

	config_path = "mod_info/menu_endgame";
	string menu_endgame_str;
	has_menu_endgame = modconfig.key_exists(config_path);
	if (has_menu_endgame) {
		modconfig.value(config_path, menu_endgame_str);
		menu_endgame
				= (menu_endgame_str == "yes" || menu_endgame_str == "true");
	} else {
		menu_endgame = false;
	}

	config_path = "mod_info/menu_credits";
	string menu_credits_str;
	has_menu_credits = modconfig.key_exists(config_path);
	if (has_menu_credits) {
		modconfig.value(config_path, menu_credits_str);
		menu_credits
				= (menu_credits_str == "yes" || menu_credits_str == "true");
	} else {
		menu_credits = false;
	}

	config_path = "mod_info/menu_quotes";
	string menu_quotes_str;
	has_menu_quotes = modconfig.key_exists(config_path);
	if (has_menu_quotes) {
		modconfig.value(config_path, menu_quotes_str);
		menu_quotes = (menu_quotes_str == "yes" || menu_quotes_str == "true");
	} else {
		menu_quotes = false;
	}

	config_path = "mod_info/force_digital_music";
	string force_digital_music_str;
	has_force_digital_music = modconfig.key_exists(config_path);
	if (has_force_digital_music) {
		modconfig.value(config_path, force_digital_music_str);
		force_digital_music
				= (force_digital_music_str == "yes"
				   || force_digital_music_str == "true");
	} else {
		force_digital_music = false;
	}

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
	cout << "setting " << cfgname
		 << " Force Skip Splash to: " << (force_skip_splash ? "yes" : "no")
		 << endl;
	cout << "setting " << cfgname
		 << " Endgame Menu to: " << (menu_endgame ? "yes" : "no") << endl;
	cout << "setting " << cfgname
		 << " Credits Menu to: " << (menu_credits ? "yes" : "no") << endl;
	cout << "setting " << cfgname
		 << " Quotes Menu to: " << (menu_quotes ? "yes" : "no") << endl;
	cout << "forcing " << cfgname
		 << " Digital Music to: " << (force_digital_music ? "yes" : "no")
		 << endl;
#endif
}

bool ModInfo::is_mod_compatible(const std::string& modversion) {
	// 0.0.00R  or no version number is considered not compatible
	if (modversion.empty() || modversion == "0.0.00R") {
		return false;
	}

	const char* ptrmod = modversion.c_str();
	const char* ptrver = VERSION;
	char*       eptrmod;
	char*       eptrver;
	int         modver = strtol(ptrmod, &eptrmod, 0);
	int         exver  = strtol(ptrver, &eptrver, 0);

	// Comparing major version number:
	if (modver > exver) {
		return false;
	} else if (modver == exver) {
		modver = strtol(eptrmod + 1, &eptrmod, 0);
		exver  = strtol(eptrver + 1, &eptrver, 0);
		// Comparing minor version number:
		if (modver > exver) {
			return false;
		} else if (modver == exver) {
			modver = strtol(eptrmod + 1, &eptrmod, 0);
			exver  = strtol(eptrver + 1, &eptrver, 0);
			// Comparing revision number:
			if (modver > exver) {
				return false;
			} else if (modver == exver) {
				const string mver(to_uppercase(eptrmod));
				const string ever(to_uppercase(eptrver));
				// Release vs CVS:
				if (mver == "CVS" && ever == "R") {
					return false;
				}
			}
		}
	}
	return true;
}

string get_game_identity(
		const char* savename, IDataSource* ds, const string& title) {
	char* game_identity = nullptr;
	if (!ds) {
		return title;
	}
	if (!Flex::is_flex(ds))
#ifdef HAVE_ZIP_SUPPORT
	{
		unzFile unzipfile = unzOpen(ds);
		if (unzipfile) {
			// Find IDENTITY, ignoring case.
			if (unzLocateFile(unzipfile, "identity", 2) != UNZ_OK) {
				unzClose(unzipfile);
				return "*";    // Old game.  Return wildcard.
			} else {
				unz_file_info file_info;
				unzGetCurrentFileInfo(
						unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr,
						0);
				game_identity = new char[file_info.uncompressed_size + 1];

				if (unzOpenCurrentFile(unzipfile) != UNZ_OK) {
					unzClose(unzipfile);
					delete[] game_identity;
					throw file_read_exception(savename);
				}
				unzReadCurrentFile(
						unzipfile, game_identity, file_info.uncompressed_size);
				if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
					// 0-delimit.
					game_identity[file_info.uncompressed_size] = 0;
				}
			}
			unzClose(unzipfile);
		}
	}
#else
		return title.c_str();
#endif
	else {
		ds->seek(0x54);    // Get to where file count sits.
		const size_t numfiles = ds->read4();
		ds->seek(0x80);    // Get to file info.
		// Read pos., length of each file.
		auto finfo = std::make_unique<uint32[]>(2 * numfiles);
		for (size_t i = 0; i < numfiles; i++) {
			finfo[2 * i]     = ds->read4();    // The position, then the length.
			finfo[2 * i + 1] = ds->read4();
		}
		for (size_t i = 0; i < numfiles; i++) {    // Now read each file.
			// Get file length.
			size_t len = finfo[2 * i + 1];
			if (len <= 13) {
				continue;
			}
			len -= 13;
			ds->seek(finfo[2 * i]);    // Get to it.
			char fname[14] = {0};      // Set up name.
			ds->read(fname, 13);
			if (!strcmp("identity", fname)) {
				game_identity = new char[len + 1];
				ds->read(game_identity, len);
				game_identity[len] = 0;
				break;
			}
		}
	}
	if (!game_identity) {
		return title;
	}
	// Truncate identity
	char* ptr = game_identity;
	for (; (*ptr != 0x1a && *ptr != 0x0d); ptr++)
		;
	*ptr      = 0;
	string id = game_identity;
	delete[] game_identity;
	return id;
}

/*
 *  Return string from IDENTITY in a savegame.
 *	Also needed by ES.
 *
 *  Output: identity if found.
 *      "" if error (or may throw exception).
 *      "*" if older savegame.
 */
string get_game_identity(const char* savename, const string& title) {
	if (!U7exists(savename)) {
		return title;
	}

	IFileDataSource ds(savename);

	return get_game_identity(savename, &ds, title);
}

// ModManager: class that manages a game's modlist and paths
ModManager::ModManager(
		const string& name, const string& menu, bool needtitle, bool silent) {
	cfgname   = name;
	mod_title = "";

	// We will NOT trust config with these values.
	// We MUST NOT use path tags at this point yet!
	string       game_path;
	string       static_dir;
	const string base_cfg_path("config/disk/game/" + cfgname);
	{
		string default_dir;
		string config_path;

		// ++++ These path settings are for that game data which requires only
		// ++++ read access. They default to a subdirectory of:
		// ++++     *nix: /usr/local/share/exult or /usr/share/exult
		// ++++     macOS: /Library/Application Support/Exult
		// ++++     Windows: program path.

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
		default_dir = "ASCII";    // Ultima VII 7-bit ASCII code page.
		config->value(config_path, codepage, default_dir.c_str());

		// And edit flag.
		string cfgediting;
		config_path = base_cfg_path + "/editing";
		default_dir = "no";    // Not editing.
		config->value(config_path, cfgediting, default_dir.c_str());
		editing = (cfgediting == "yes");
	}

	if (!silent) {
		cout << "Looking for '" << cfgname << "' at '" << game_path << "'... ";
	}
	const string initgam_path(static_dir + "/initgame.dat");
	found = U7exists(initgam_path);

	std::string id = static_identity
			= get_game_identity(initgam_path.c_str(), cfgname);
	if (found) {
		if (static_identity != "ULTIMA7" && static_identity != "FORGE"
			&& static_identity != "SERPENT ISLE"
			&& static_identity != "SILVER SEED") {
			static_identity = "DEVEL GAME";
		}
		if (!silent) {
			if (id == CFG_DEMO_NAME) {
				cout << "found DEMO game with identity '" << static_identity
					 << "'" << endl;
			} else {
				cout << "found game with identity '" << static_identity << "'"
					 << endl;
			}
		}
	} else if (U7exists(game_path)) {    // New game still under development.
		if (cfgname != CFG_BG_NAME && cfgname != CFG_FOV_NAME
			&& cfgname != CFG_SIB_NAME && cfgname != CFG_SI_NAME
			&& cfgname != CFG_SS_NAME) {
			static_identity = "DEVEL GAME";
			if (!silent) {
				cout << "found game with identity '" << static_identity << "'"
					 << endl;
			}
		} else {
			if (!silent) {
				cout << "but it wasn't there." << endl;
			}
			return;
		}
	} else {
		if (!silent) {
			cout << "but it wasn't there." << endl;
		}
		return;
	}

	const string mainshp     = static_dir + "/mainshp.flx";
	const uint32 crc         = crc32(mainshp.c_str(), true);
	auto         unknown_crc = [crc](const char* game) {
        cerr << "Warning: Unknown CRC for mainshp.flx: 0x" << std::hex << crc
             << std::dec << std::endl;
        cerr << "Note: Guessing hacked " << game << std::endl;
	};
	string new_title;
	if (static_identity == "ULTIMA7") {
		type      = BLACK_GATE;
		expansion = false;
		sibeta    = false;
		switch (crc) {
		case 0x36af707f:
			// French BG
			language    = Game_Language::FRENCH;
			path_prefix = to_uppercase(CFG_BG_FR_NAME);
			if (needtitle) {
				new_title = CFG_BG_FR_TITLE;
			}
			break;
		case 0x157ca514:
			// German BG
			language    = Game_Language::GERMAN;
			path_prefix = to_uppercase(CFG_BG_DE_NAME);
			if (needtitle) {
				new_title = CFG_BG_DE_TITLE;
			}
			break;
		case 0x6d7b7323:
			// Spanish BG
			language    = Game_Language::SPANISH;
			path_prefix = to_uppercase(CFG_BG_ES_NAME);
			if (needtitle) {
				new_title = CFG_BG_ES_TITLE;
			}
			break;
		default:
			unknown_crc("Black Gate");
			[[fallthrough]];
		case 0xafc35523:
			// English BG
			language    = Game_Language::ENGLISH;
			path_prefix = to_uppercase(CFG_BG_NAME);
			if (needtitle) {
				new_title = CFG_BG_TITLE;
			}
			break;
		}
	} else if (static_identity == "FORGE") {
		type      = BLACK_GATE;
		language  = Game_Language::ENGLISH;
		expansion = true;
		sibeta    = false;
		if (crc != 0x8a74c26b) {
			unknown_crc("Forge of Virtue");
		}
		path_prefix = to_uppercase(CFG_FOV_NAME);
		if (needtitle) {
			new_title = CFG_FOV_TITLE;
		}
	} else if (static_identity == "SERPENT ISLE") {
		type      = SERPENT_ISLE;
		expansion = false;
		switch (crc) {
		case 0x96f66a7a:
			// Spanish SI
			language    = Game_Language::SPANISH;
			path_prefix = to_uppercase(CFG_SI_ES_NAME);
			if (needtitle) {
				new_title = CFG_SI_ES_TITLE;
			}
			sibeta = false;
			break;
		case 0xdbdc2676:
			// SI Beta
			language    = Game_Language::ENGLISH;
			path_prefix = to_uppercase(CFG_SIB_NAME);
			if (needtitle) {
				new_title = CFG_SIB_TITLE;
			}
			sibeta = true;
			break;
		default:
			unknown_crc("Serpent Isle");
			[[fallthrough]];
		case 0xf98f5f3e:
			// English SI
			language    = Game_Language::ENGLISH;
			path_prefix = to_uppercase(CFG_SI_NAME);
			if (needtitle) {
				new_title = CFG_SI_TITLE;
			}
			sibeta = false;
			break;
		}
	} else if (static_identity == "SILVER SEED") {
		type      = SERPENT_ISLE;
		language  = Game_Language::ENGLISH;
		expansion = true;
		sibeta    = false;
		if (crc != 0x3e18f9a0) {
			unknown_crc("Silver Seed");
		}
		path_prefix = to_uppercase(CFG_SS_NAME);
		if (needtitle) {
			new_title = CFG_SS_TITLE;
		}
	} else if (static_identity == "DEVEL GAME") {
		type      = EXULT_DEVEL_GAME;
		language  = Game_Language::ENGLISH;
		expansion = false;
		sibeta    = false;
		if (id == CFG_DEMO_NAME) {
			path_prefix = to_uppercase(CFG_DEMO_NAME);
			if (needtitle) {
				new_title = CFG_DEMO_TITLE;
			}
		} else {
			path_prefix = "DEVEL" + to_uppercase(name);
			new_title   = menu;    // To be safe.
		}
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
		string patch_dir;
		string mods_dir;
		string src_dir;
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
		default_dir = game_path + "/source";
		config->value(config_path.c_str(), src_dir, default_dir.c_str());
		add_system_path("<" + path_prefix + "_SOURCE>", src_dir);

#ifdef DEBUG_PATHS
		if (!silent) {
			cout << "path prefix of " << cfgname << " is: " << path_prefix
				 << endl;
			cout << "setting " << cfgname
				 << " static directory to: " << static_dir << endl;
			cout << "setting " << cfgname
				 << " patch directory to: " << patch_dir << endl;
			cout << "setting " << cfgname
				 << " modifications directory to: " << mods_dir << endl;
			cout << "setting " << cfgname << " source directory to: " << src_dir
				 << endl;
		}
#endif
	}

	get_game_paths(game_path);
	gather_mods();
}

void ModManager::gather_mods() {
	modlist.clear();    // Just to be on the safe side.

	FileList     filenames;
	const string pathname("<" + path_prefix + "_MODS>");

	// If the dir doesn't exist, leave at once.
	if (!U7exists(pathname)) {
		return;
	}

	U7ListFiles(pathname + "/*.cfg", filenames, true);
	const int num_mods = filenames.size();

	if (num_mods > 0) {
		modlist.reserve(num_mods);
		for (int i = 0; i < num_mods; i++) {
#if 0
			// std::filesystem isn't reliably available yet, but this is much cleaner
			std::filesystem::path modcfg(filenames[i]);
			auto modtitle = modcfg.stem();
#else
			const auto& filename = filenames[i];
			auto        pathend  = filename.find_last_of("/\\") + 1;
			auto        modtitle = filename.substr(
                    pathend, filename.length() - pathend - strlen(".cfg"));
#endif
			modlist.emplace_back(
					type, language, cfgname, modtitle, path_prefix, expansion,
					sibeta, editing, filenames[i]);
		}
	}
}

int ModManager::InstallModZip(
		std::string& zipfilename, ModManager* game_override,
		GameManager* gamemanager) {
	unz_file_info           fileinfo;
	std::unique_ptr<char[]> buffer;
	size_t                  buffer_size = 0;
	std::string             filepath;

	if (!U7exists(zipfilename)) {
		std::cerr << "InstallMod: Mod zipfile \"" << zipfilename
				  << "\" not found." << std::endl;
		return -1;
	}
	IFileDataSource ds(zipfilename);

	unzFile unzipfile = unzOpen(&ds);
	if (!unzipfile) {
		std::cerr << "InstallMod: Unable to open \"" << zipfilename
				  << "\" as a zip file" << std::endl;
		return -2;
	}

	std::unordered_set<std::string> cfgs;
	std::unordered_set<std::string> dirs;

	if (unzGoToFirstFile(unzipfile) != UNZ_OK) {
		std::cerr << "InstallMod: Error calling unzGoToFirstFile" << std::endl;
		return -3;
	}
	do {
		if (unzGetCurrentFileInfo(
					unzipfile, &fileinfo, filepath, nullptr, 0, nullptr, 0)
			!= UNZ_OK) {
			std::cerr << "InstallMod: Error calling unzGetCurrentFileInfo"
					  << std::endl;
			return -3;
		}

		// is it a config file in the zip's root?
		if (filepath.size() > 4 && filepath[filepath.size() - 4] == '.'
			&& std::tolower(filepath[filepath.size() - 3]) == 'c'
			&& std::tolower(filepath[filepath.size() - 2]) == 'f'
			&& std::tolower(filepath[filepath.size() - 1]) == 'g'
			&& filepath.find('/') == std::string::npos) {
			cfgs.insert(filepath);
		}

		// is it a directory?
		if (filepath.size() > 1 && filepath[filepath.size() - 1] == '/') {
			dirs.insert(filepath.substr(0, filepath.size() - 1));
		}
	} while (unzGoToNextFile(unzipfile) == UNZ_OK);

	if (cfgs.empty()) {
		std::cerr << "InstallMod: No cfg file found in mod. Unable to install"
				  << std::endl;
		return -4;
	}

	for (auto& cfgname : cfgs) {
		// Get the mod data directory name
		std::string moddir = cfgname;
		moddir.resize(moddir.size() - 4);
		std::cout << "InstallMod: Found cfg file \"" << cfgname
				  << "\" in modzip. " << std::endl;

		if (dirs.find(moddir) == dirs.end()) {
			std::cerr << "InstallMod: Directory \"" << moddir
					  << "\" for mod not found in zipfile. Unable to install "
					  << std::endl;
			return -5;
		}

		// load the cfgfile and make sure it is supported by this version of
		// exult.
		Configuration modconfig("", "modinfo");
		int           error = unzLocateFile(unzipfile, cfgname.c_str(), 2);

		if (error == UNZ_OK) {
			error = unzGetCurrentFileInfo(
					unzipfile, &fileinfo, nullptr, 0, nullptr, 0, nullptr, 0);
		}
		if (error == UNZ_OK) {
			// Allocate a string for the config file
			std::string configstring(fileinfo.uncompressed_size + 1, 0);

			if (error == UNZ_OK) {
				error = unzOpenCurrentFile(unzipfile);
			}
			if (error == UNZ_OK) {
				error = unzReadCurrentFile(
						unzipfile, configstring.data(),
						fileinfo.uncompressed_size + 1);
				if (error > 0
					&& unsigned(error) == fileinfo.uncompressed_size) {
					configstring.resize(fileinfo.uncompressed_size);
					error = UNZ_OK;
				}
			}

			if (error == UNZ_OK) {
				error = unzCloseCurrentFile(unzipfile);
			}

			if (error != UNZ_OK) {
				std::cerr << "InstallMod: Error unzipping mod cfg" << std::endl;
				return -6;
			}

			if (!modconfig.read_config_string(configstring)) {
				std::cerr << "InstallMod: Error parsing mod cfg" << std::endl;
				return -7;
			}
		}

		string modversion;
		modconfig.value("mod_info/required_version", modversion, "");
		if (modversion.empty()) {
			std::cerr << "InstallMod: Warning Mod is missing required_version "
						 "value in mod cfg. This mod will be installed but may "
						 "not appear in the mod menu."
					  << std::endl;
		} else if (!ModInfo::is_mod_compatible(modversion)) {
			std::cerr << "InstallMod: Warning Mod is not compatible with this "
						 "version of exult. Exult v"
					  << modversion
					  << " is required. This mod will be installed but may not "
						 "appear in the mod menu."
					  << std::endl;
		}

		std::string patch_path;
		modconfig.value("mod_info/patch", patch_path, "__MOD_PATH__/patch");
		ReplaceMacro(patch_path, "__MOD_PATH__", moddir);
		if (patch_path.empty()) {
			std::cerr << "InstallMod: Didn't find patch path in mod cfg"
					  << std::endl;
			return -8;
		}

		// try to load initgame.dat in zip file to get identity
		std::string initgame_path = patch_path + "/initgame.dat";
		std::string identity;
		// Get identity string from cfg if it is there
		modconfig.value("mod_info/game_identity", identity, "");
		if (unzLocateFile(unzipfile, initgame_path.c_str(), 2) == UNZ_OK) {
			error = unzGetCurrentFileInfo(
					unzipfile, &fileinfo, nullptr, 0, nullptr, 0, nullptr, 0);
			if (buffer_size < fileinfo.uncompressed_size) {
				try {
					buffer = std::make_unique<char[]>(
							buffer_size = fileinfo.uncompressed_size);
				} catch (std::bad_alloc&) {
					std::cerr << "ExtractZip: Failed to allocate "
								 "extract buffer "
								 "of size "
							  << fileinfo.uncompressed_size << " for file "
							  << initgame_path << std::endl;

					return -9;
				}
			}
			if (error == UNZ_OK) {
				error = unzOpenCurrentFile(unzipfile);
			}
			if (error == UNZ_OK) {
				error = unzReadCurrentFile(
						unzipfile, buffer.get(), fileinfo.uncompressed_size);
				if (error > 0
					&& unsigned(error) == fileinfo.uncompressed_size) {
					error = UNZ_OK;
				}
			}
			if (error == UNZ_OK) {
				error = unzCloseCurrentFile(unzipfile);

				if (error != UNZ_OK) {
					std::cerr << "InstallMod: Error unzipping "
								 "mod initgame. Not installing mod"
							  << std::endl;
					return -10;
				}

				IBufferDataView ds(buffer.get(), fileinfo.uncompressed_size);

				identity = get_game_identity(
						initgame_path.c_str(), &ds, identity);
			}
		}

		// decode identity and get game

		ModManager* base_game          = nullptr;
		bool        expansion_required = false;
		bool        have_unexpanded    = false;
		std::string game_title{"Unknown Game"};

		if (identity == "ULTIMA7") {
			base_game  = gamemanager->get_bg();
			game_title = CFG_BG_TITLE;
		} else if (identity == "FORGE") {
			base_game          = gamemanager->get_fov();
			have_unexpanded    = gamemanager->get_bg() != nullptr;
			expansion_required = true;
			game_title         = CFG_FOV_TITLE;
		} else if (identity == "SERPENT ISLE") {
			base_game  = gamemanager->get_si();
			game_title = CFG_SI_TITLE;
		} else if (identity == "SILVER SEED") {
			base_game          = gamemanager->get_ss();
			have_unexpanded    = gamemanager->get_si() != nullptr;
			expansion_required = true;
			game_title         = CFG_SS_TITLE;
		} else if (identity == "DEVEL GAME") {
			base_game  = gamemanager->get_devel();
			game_title = CFG_DEMO_TITLE;
		}

		if (base_game) {
			game_title = base_game->get_menu_string();
		}
		// Get rid of line feed in the title and replace with ": "
		auto pos_lf = game_title.find('\n');
		if (pos_lf != std::string::npos) {
			game_title.replace(pos_lf, 1, ": ");
		}
		if (!base_game && !game_override) {
			std::cerr << "InstallMod: Correct Game for mod not found. \""
					  << game_title
					  << "\" required. This mod will not be installed. "
					  << std::endl;

			// Return a different code if the expension was required and not
			// found
			if (expansion_required && have_unexpanded) {
				return -11;
			} else {
				return -12;
			}
		} else {
			std::cout << "InstallMod: Mod wants to install to game: "
					  << game_title << std::endl;
		}

		if (game_override) {
			std::cout << "InstallMod: Installing mod to Game : "
					  << game_override->get_menu_string() << std::endl;
			if (expansion_required && !game_override->have_expansion()) {
				std::cout << "InstallMod: Warning: trying to install a mod "
							 "that wants an expansion into a game that does "
							 "not have the expansion"
						  << std::endl;
			}
			if (base_game && base_game != game_override) {
				std::cout << "InstallMod: Warning: This mod is being installed "
							 "to a different game than it expects. Mod wont be "
							 "playable"
						  << std::endl;
			}

			base_game = game_override;
		}

		std::string game_mod_path
				= "<" + base_game->get_path_prefix() + "_MODS>";

		U7mkdir(game_mod_path.c_str());

		// Delete mod patch directory and cfg
		U7remove((game_mod_path + "/" + cfgname).c_str());
		U7rmdir((game_mod_path + "/" + patch_path).c_str(), true);

		// Get list of game's static files ro fixup the filename caseing of
		// themod's files to match te game's files

		FileList filelist;
		U7ListFiles(
				("<" + base_game->get_path_prefix() + "_STATIC>/*").c_str(),
				filelist, true);
		std::unordered_map<std::string, std::string> lowercase_map;
		for (auto filename : filelist) {
			filename          = get_filename_from_path(filename);
			std::string lower = filename;
			for (char& c : lower) {
				c = std::tolower(c);
			}

			lowercase_map[lower] = filename;
		}

		// Extract the files for this mod

		error = unzGoToFirstFile(unzipfile);
		if (error == UNZ_OK) {
			do {
				error = unzGetCurrentFileInfo(
						unzipfile, &fileinfo, filepath, nullptr, 0, nullptr, 0);

				if (error != UNZ_OK) {
					break;
				}

				std::string directory{get_directory_from_path(filepath)};
				std::string directory_lower{directory};
				for (char& c : directory_lower) {
					c = std::tolower(c);
				}
				// Get the top level dir name
				std::string_view topleveldir = directory;
				for (;;) {
					auto d = get_directory_from_path(topleveldir);
					if (d.empty()) {
						break;
					} else {
						topleveldir = d;
					}
				}

				// Skip any files not from this mod
				if (topleveldir != moddir && filepath != cfgname) {
					continue;
				}

				// Is the file in the patch dir (ignores map directories)
				if (directory == patch_path) {
					//

					// Fixup filename casing
					auto filename = get_filename_from_path(filepath);
					if (filename.size()) {
						std::string lower{filename};

						for (char& c : lower) {
							c = std::tolower(c);
						}

						auto found = lowercase_map.find(lower);
						if (found != lowercase_map.end()) {
							if (filename != found->second) {
								std::cout << "InstallMod: Mod file \""
										  << filepath
										  << "\" being renamed to \""
										  << found->second << "\"" << std::endl;
								filepath = directory + "/" + found->second;
							}
						}
					}
				}

				std::string outpath = game_mod_path + "/" + filepath;

				// create directories as required
				U7mkdir((game_mod_path + "/" + directory).c_str(), 0755, true);

				// Extract the file
				if (filepath[filepath.size() - 1] != '/') {
					if (error == UNZ_OK) {
						error = unzOpenCurrentFile(unzipfile);
					}
					if (error == UNZ_OK) {
						if (buffer_size < fileinfo.uncompressed_size) {
							try {
								buffer = std::make_unique<char[]>(
										buffer_size
										= fileinfo.uncompressed_size);
							} catch (std::bad_alloc&) {
								std::cerr << "ExtractZip: Failed to allocate "
											 "extract buffer "
											 "of size "
										  << fileinfo.uncompressed_size
										  << " for file " << filepath
										  << std::endl;

								return -15;
							}
						}

						error = unzReadCurrentFile(
								unzipfile, buffer.get(),
								fileinfo.uncompressed_size);
						if (error > 0
							&& unsigned(error) == fileinfo.uncompressed_size) {
							error = UNZ_OK;
						}
					}
					if (error == UNZ_OK) {
						error = unzCloseCurrentFile(unzipfile);
					}

					if (error != UNZ_OK) {
						std::cerr
								<< "InstallMod: error trying to extract file \""
								<< filepath << "\" from mod zip" << std::endl;
						return -16;
					}
					// Write out the buffer
					std::unique_ptr<std::ostream> outfile;

					try {
						outfile = U7open_out(outpath.c_str());
					} catch (exult_exception&) {
						std::cerr << "InstallMod: exception trying open file \""
								  << get_system_path(outpath)
								  << "\" for writing" << std::endl;
						return -16;
					}
					outfile->write(buffer.get(), fileinfo.uncompressed_size);
					if (outfile->fail()) {
						std::cerr << "InstallMod: error trying to write "
									 "file \""
								  << get_system_path(outpath) << "\""
								  << std::endl;

						return -17;
					}
				}
			} while ((error = unzGoToNextFile(unzipfile)) == UNZ_OK);
			if (error != UNZ_END_OF_LIST_OF_FILE) {
				std::cerr << "InstallMod: error extracting files "
						  << " from mod zip" << std::endl;
				return -18;
			}
			std::cout << "InstallMod: \"" << moddir
					  << "\" sucessfully installed into game: "
					  << base_game->get_menu_string() << std::endl;
		}
	}
	unzClose(unzipfile);

	return 0;
}

ModInfo* ModManager::find_mod(const string& name) {
	for (auto& mod : modlist) {
		if (mod.get_mod_title() == name) {
			return &mod;
		}
	}
	return nullptr;
}

int ModManager::find_mod_index(const string& name) {
	for (size_t i = 0; i < modlist.size(); i++) {
		if (modlist[i].get_mod_title() == name) {
			return i;
		}
	}
	return -1;
}

void ModManager::add_mod(const string& mod, const string& modconfig) {
	modlist.emplace_back(
			type, language, cfgname, mod, path_prefix, expansion, sibeta,
			editing, modconfig);
	store_system_paths();
}

// Checks the game 'gam' for the presence of the mod given in 'arg_modname'
// and checks the mod's compatibility. If the mod exists and is compatible with
// Exult, returns a reference to the mod; otherwise, returns the mod's parent
// game. Outputs error messages if the mod is not found or is not compatible.
BaseGameInfo* ModManager::get_mod(const string& name, bool checkversion) {
	ModInfo* newgame = nullptr;
	if (has_mods()) {
		newgame = find_mod(name);
	}
	if (newgame) {
		if (checkversion && !newgame->is_mod_compatible()) {
			cerr << "Mod '" << name
				 << "' is not compatible with this version of Exult." << endl;
			return nullptr;
		}
	}
	if (!newgame) {
		cerr << "Mod '" << name << "' not found." << endl;
	}
	return newgame;
}

/*
 *  Calculate paths for the given game, using the config file and
 *  falling back to defaults if necessary.  These are stored in
 *  per-game system_path entries, which are then used later once the
 *  game is selected.
 */
void ModManager::get_game_paths(const string& game_path) {
	string       saveprefix(get_system_path("<SAVEHOME>") + "/" + cfgname);
	string       default_dir;
	string       config_path;
	string       gamedat_dir;
	string       savegame_dir;
	const string base_cfg_path("config/disk/game/" + cfgname);

	// ++++ All of these are directories with read/write requirements.
	// ++++ They default to a directory in the current user's profile,
	// ++++ with Win9x and old MacOS (possibly others) being exceptions.

	// Usually for Win9x:
	if (saveprefix == ".") {
		saveprefix = game_path;
	}

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
	cout << "setting " << cfgname << " gamedat directory to: " << gamedat_dir
		 << endl;
	cout << "setting " << cfgname << " savegame directory to: " << savegame_dir
		 << endl;
#endif
}

// GameManager: class that manages the installed games
GameManager::GameManager(bool silent) {
	games.clear();
	bg = fov = si = ss = sib = dev = nullptr;

	// Search for games defined in exult.cfg:
	const string              config_path("config/disk/game");
	string                    game_title;
	const std::vector<string> gamestrs = config->listkeys(config_path, false);
	std::vector<string>       checkgames;
	checkgames.reserve(
			gamestrs.size()
			+ 6);    // +6 in case the six below are not in the cfg.
	// The original games plus expansions.
	checkgames.emplace_back(CFG_BG_NAME);
	checkgames.emplace_back(CFG_FOV_NAME);
	checkgames.emplace_back(CFG_SI_NAME);
	checkgames.emplace_back(CFG_SS_NAME);
	checkgames.emplace_back(CFG_SIB_NAME);
	checkgames.emplace_back(CFG_DEMO_NAME);

	for (const auto& gamestr : gamestrs) {
		if (gamestr != CFG_BG_NAME && gamestr != CFG_FOV_NAME
			&& gamestr != CFG_SI_NAME && gamestr != CFG_SS_NAME
			&& gamestr != CFG_SIB_NAME && gamestr != CFG_DEMO_NAME) {
			checkgames.push_back(gamestr);
		}
	}

	games.reserve(checkgames.size());
	int bgind  = -1;
	int fovind = -1;
	int siind  = -1;
	int ssind  = -1;
	int sibind = -1;
	int devind = -1;

	for (const auto& gameentry : checkgames) {
		// Load the paths for all games found:
		string base_title = gameentry;
		to_uppercase(base_title);
		base_title += "\nMissing Title";
		string base_conf = config_path;
		base_conf += '/';
		base_conf += gameentry;
		base_conf += "/title";
		config->value(base_conf, game_title, base_title.c_str());
		const bool need_title = game_title == base_title;
		// This checks static identity and sets game type.
		const ModManager game
				= ModManager(gameentry, game_title, need_title, silent);
		if (!game.being_edited() && !game.is_there()) {
			continue;
		}
		if (game.get_game_type() == BLACK_GATE) {
			if (game.have_expansion()) {
				if (fovind == -1) {
					fovind = games.size();
				}
			} else if (bgind == -1) {
				bgind = games.size();
			}
		} else if (game.get_game_type() == SERPENT_ISLE) {
			if (game.is_si_beta()) {
				if (sibind == -1) {
					sibind = games.size();
				}
			} else if (game.have_expansion()) {
				if (ssind == -1) {
					ssind = games.size();
				}
			} else if (siind == -1) {
				siind = games.size();
			}
		} else if (game.get_game_type() == EXULT_DEVEL_GAME) {
			if (devind == -1) {
				devind = games.size();
			}
		}

		games.push_back(game);
	}

	if (bgind >= 0) {
		bg = &(games[bgind]);
	}
	if (fovind >= 0) {
		fov = &(games[fovind]);
	}
	if (siind >= 0) {
		si = &(games[siind]);
	}
	if (ssind >= 0) {
		ss = &(games[ssind]);
	}
	if (sibind >= 0) {
		sib = &(games[sibind]);
	}

	if (devind >= 0) {
		dev = &(games[devind]);
	}

	// Sane defaults.
	add_system_path("<ULTIMA7_STATIC>", ".");
	add_system_path("<SERPENT_STATIC>", ".");
	print_found(
			bg, "exult_bg.flx", "Black Gate", CFG_BG_NAME, "ULTIMA7", silent);
	print_found(
			fov, "exult_bg.flx", "Forge of Virtue", CFG_FOV_NAME, "ULTIMA7",
			silent);
	print_found(
			si, "exult_si.flx", "Serpent Isle", CFG_SI_NAME, "SERPENT", silent);
	print_found(
			ss, "exult_si.flx", "Silver Seed", CFG_SS_NAME, "SERPENT", silent);
	store_system_paths();
}

void GameManager::print_found(
		ModManager* game, const char* flex, const char* title,
		const char* cfgname, const char* basepath, bool silent) {
	char   path[50];
	string cfgstr(cfgname);
	to_uppercase(cfgstr);
	snprintf(path, sizeof(path), "<%s_STATIC>/", cfgstr.c_str());

	if (game == nullptr) {
		if (!silent) {
			cout << title << "   : not found (" << get_system_path(path) << ")"
				 << endl;
		}
		return;
	}
	if (!silent) {
		cout << title << "   : found" << endl;
	}
	// This stores the BG/SI static paths (preferring the expansions)
	// for easier support of things like multiracial avatars in BG.
	char staticpath[50];
	snprintf(path, sizeof(path), "<%s_STATIC>", cfgstr.c_str());
	snprintf(staticpath, sizeof(staticpath), "<%s_STATIC>", basepath);
	clone_system_path(staticpath, path);
	if (silent) {
		return;
	}
	if (is_system_path_defined("<BUNDLE>")) {
		snprintf(path, sizeof(path), "<BUNDLE>/%s", flex);
		if (U7exists(path)) {
			cout << flex << " : found" << endl;
			return;
		}
	}
	snprintf(path, sizeof(path), "<DATA>/%s", flex);
	if (U7exists(path)) {
		cout << flex << " : found" << endl;
	} else {
		cout << flex << " : not found (" << get_system_path(path) << ")"
			 << endl;
	}
}

ModManager* GameManager::find_game(const string& name) {
	for (auto& game : games) {
		if (game.get_cfgname() == name) {
			return &game;
		}
	}
	return nullptr;
}

int GameManager::find_game_index(const string& name) {
	for (size_t i = 0; i < games.size(); i++) {
		if (games[i].get_cfgname() == name) {
			return i;
		}
	}
	return -1;
}

void GameManager::add_game(const string& name, const string& menu) {
	games.emplace_back(name, menu, false);
	store_system_paths();
}
