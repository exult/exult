/*
 *  modmgr.h - Mod manager for Exult.
 *
 *  Copyright (C) 2006  The Exult Team
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

#ifndef MODMGR_H
#define MODMGR_H

#include <vector>
#include <string>
#include "exult_constants.h"
#include "exceptions.h"
#include "Configuration.h"

extern Configuration *config;

class BaseGameInfo {
protected:
	Exult_Game type = NONE;  // Game type
	std::string cfgname;     // What the game is called in Exult.cfg
	std::string path_prefix; // System path prefix for the game/mod.
	std::string mod_title;   // Internal mod name, the mod's title
	std::string menustring;  // Text displayed in mods menu
	bool expansion = false;          // For FoV/SS ONLY.
	bool sibeta = false;             // For beta version of SI.
	bool found = false;              // If the game/mod is found.
	bool editing = false;            // Game is being edited and may have missing files.
	std::string codepage = "CP437";  // Game/mod codepage (mainly for ES).
public:
	BaseGameInfo() = default;
	BaseGameInfo(const Exult_Game ty, const char *cf, const char *mt,
	             const char *pt, const char *ms, bool exp, bool sib, bool f, bool ed,
	             const char *cp)
		: type(ty), cfgname(cf), path_prefix(pt), mod_title(mt), menustring(ms),
		  expansion(exp), sibeta(sib), found(f), editing(ed), codepage(cp)
	{  }
	BaseGameInfo(const BaseGameInfo &) = default;
	virtual ~BaseGameInfo() = default;

	const std::string get_cfgname() const {
		return cfgname;
	}
	const std::string get_path_prefix() const {
		return path_prefix;
	}
	const std::string get_mod_title() const {
		return mod_title;
	}
	const std::string get_menu_string() const {
		return menustring;
	}
	Exult_Game get_game_type() const {
		return type;
	}
	const std::string get_codepage() const {
		return codepage;
	}
	// For FoV/SS ONLY.
	bool have_expansion() const {
		return expansion;
	}
	bool is_si_beta() const {
		return sibeta;
	}
	bool is_there() const {
		return found;
	}
	bool being_edited() const {
		return editing;
	}
	void set_cfgname(const std::string &name) {
		cfgname = name;
	}
	void set_path_prefix(const std::string &pt) {
		path_prefix = pt;
	}
	void set_mod_title(const std::string &mod) {
		mod_title = mod;
	}
	void set_menu_string(const std::string &menu) {
		menustring = menu;
	}
	void set_game_type(Exult_Game game) {
		type = game;
	}
	// For FoV/SS ONLY.
	void set_expansion(bool tf) {
		expansion = tf;
	}
	void set_si_beta(bool tf) {
		sibeta = tf;
	}
	void set_found(bool tf) {
		found = tf;
	}
	void set_editing(bool tf) {
		editing = tf;
	}
	void set_codepage(const std::string &cp) {
		codepage = cp;
	}

	void setup_game_paths();

	virtual bool get_config_file(Configuration *&cfg, std::string &root) = 0;
};

class ModInfo : public BaseGameInfo {
protected:
	bool compatible;
	std::string configfile;
public:
	ModInfo(Exult_Game game, const std::string &name, const std::string &mod,
	        const std::string &path, bool exp, bool sib, bool ed, const std::string &cfg);

	bool is_mod_compatible() const {
		return compatible;
	}

	bool get_config_file(Configuration *&cfg, std::string &root) override {
		cfg = new Configuration(configfile, "modinfo");
		root = "mod_info/";
		return true;
	}
};

class ModManager : public BaseGameInfo {
protected:
	std::vector<ModInfo> modlist;
public:
	ModManager(const std::string &name, const std::string &menu, bool needtitle,
	           bool silent = false);
	ModManager() = default;

	std::vector<ModInfo> &get_mod_list() {
		return modlist;
	}
	ModInfo *find_mod(const std::string &name);
	int find_mod_index(const std::string &name);

	bool has_mods() const {
		return modlist.size() > 0;
	}
	ModInfo *get_mod(int i) {
		if (i >= 0 && static_cast<unsigned>(i) < modlist.size())
			return &(modlist[i]);
		return nullptr;
	}
	BaseGameInfo *get_mod(const std::string &name, bool checkversion = true);
	void add_mod(const std::string &mod, const std::string &modconfig);

	void get_game_paths(const std::string &game_path);
	void gather_mods();

	bool get_config_file(Configuration *&cfg, std::string &root) override {
		cfg = config;
		root = "config/disk/game/" + cfgname + "/";
		return false;
	}
};

class GameManager {
	UNREPLICATABLE_CLASS(GameManager)
protected:
	// -> to original games.
	ModManager *bg;
	ModManager *fov;
	ModManager *si;
	ModManager *ss;
	ModManager *sib;
	std::vector<ModManager> games;
	void print_found(ModManager *game, const char *flex,
	                 const char *title, const char *cfgname, const char *basepath,
	                 bool silent);
public:
	GameManager(bool silent = false);

	std::vector<ModManager> &get_game_list() {
		return games;
	}
	int get_game_count() const {
		return games.size();
	}
	ModManager *get_game(int i) {
		if (i >= 0 && static_cast<unsigned>(i) < games.size())
			return &(games[i]);
		return nullptr;
	}
	bool is_bg_installed() const {
		return bg != nullptr;
	}
	bool is_fov_installed() const {
		return fov != nullptr;
	}
	bool is_si_installed() const {
		return si != nullptr;
	}
	bool is_ss_installed() const {
		return ss != nullptr;
	}
	bool is_sib_installed() const {
		return sib != nullptr;
	}
	ModManager *find_game(const std::string &name);
	ModManager *get_bg() {
		return bg ? bg : fov;
	}
	ModManager *get_fov() {
		return fov;
	}
	ModManager *get_si() {
		return si ? si : ss;
	}
	ModManager *get_ss() {
		return ss;
	}
	ModManager *get_sib() {
		return sib;
	}
	int find_game_index(const std::string &name);
	void add_game(const std::string &name, const std::string &menu);
};

#endif
