/*
 *  Copyright (C) 2000-2005  Ryan Nunn
 *  Copyright (C) 2008-2022  The Exult Team
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

/*
 *  What is this?
 *
 *  EXCONFIG is a dll that is used by the InstallShield Wizard to get and set
 * the Black Gate and Serpent Isle paths.
 *
 */

#include "exconfig.h"

#include "Configuration.h"
#include "fnames.h"
#include "headers/ignore_unused_variable_warning.h"
#include "utils.h"

#include <array>
#include <cstring>
#include <map>
#include <string>

#define MAX_STRLEN 512

#ifdef _DEBUF
#	define MessageBoxDebug(a, b, c, d) MessageBox(a, b, c, d);
#else
#	define MessageBoxDebug(a, b, c, d)
#endif

static const std::map<const char*, const char*> config_defaults{
		{  "config/disk/game/blackgate/keys", "(default)"},
		{"config/disk/game/serpentisle/keys", "(default)"},
		{			 "config/audio/enabled",       "yes"},
		{     "config/audio/effects/enabled",       "yes"},
		{     "config/audio/effects/convert",        "gs"},
		{        "config/audio/midi/enabled",       "yes"},
		{        "config/audio/midi/convert",        "gm"},
		{ "config/audio/midi/reverb/enabled",        "no"},
		{   "config/audio/midi/reverb/level",         "0"},
		{ "config/audio/midi/chorus/enabled",        "no"},
		{   "config/audio/midi/chorus/level",         "0"},
		{   "config/audio/midi/volume_curve",  "1.000000"},
		{      "config/audio/speech/enabled",       "yes"},
		{			"config/gameplay/cheat",       "yes"},
		{       "config/gameplay/skip_intro",        "no"},
		{      "config/gameplay/skip_splash",        "no"},
		{			   "config/video/width",       "320"},
		{			  "config/video/height",       "200"},
		{			   "config/video/scale",         "2"},
		{        "config/video/scale_method",     "2xSaI"},
		{		  "config/video/fullscreen",        "no"},
		{       "config/video/disable_fades",        "no"},
		{		   "config/video/gamma/red",      "1.00"},
		{		 "config/video/gamma/green",      "1.00"},
		{		  "config/video/gamma/blue",      "1.00"}
};

constexpr static const std::array BASE_Files{SHAPES_VGA, FACES_VGA, GUMPS_VGA,
											 FONTS_VGA,  INITGAME,  USECODE};

constexpr static const std::array BG_Files{ENDGAME};

constexpr static const std::array SI_Files{PAPERDOL};

extern "C" {
__declspec(dllexport) int __cdecl VerifySIDirectory(char* path);

__declspec(dllexport) int __cdecl VerifyBGDirectory(char* path);
}

void strcat_safe(char* dest, const char* src, size_t size_dest) {
	size_t cur = std::strlen(dest);
	if (cur >= size_dest) {
		return;
	}
	size_t remaining = size_dest - cur;

	std::strncpy(dest + cur, src, remaining - 1);
	dest[size_dest - 1] = 0;
}

//
// Path Helper class
//
class Path {
	struct Directory {
		char name[256];    // I hope no one needs path components this long
		Directory* next{};

		Directory() {
			name[0] = 0;
			next    = nullptr;
		}
	};

	bool       network = false;      // Networked?
	char       drive   = 0;          // Drive letter (if set)
	Directory* dirs    = nullptr;    // Directories

	void RemoveAll();
	int  Addit(const char* p);

public:
	Path() = default;

	Path(const char* p) {
		AddString(p);
	}

	~Path();

	void RemoveLast() {
		Addit("..");
	}

	void AddString(const char* p);
	void GetString(char* p, int max_strlen = MAX_STRLEN);
};

// Destructor
Path::~Path() {
	RemoveAll();
	network = false;
	drive   = 0;
}

void Path::RemoveAll() {
	Directory* d = dirs;
	while (d) {
		Directory* next = d->next;
		delete d;
		d = next;
	}
	dirs = nullptr;
}

int Path::Addit(const char* p) {
	Directory* d        = dirs;
	Directory* prev     = nullptr;
	Directory* prevprev = nullptr;
	size_t     i;

	// Check for . and ..

	// Create new
	if (!d) {
		dirs = d = new Directory;
	} else {
		while (d->next) {
			prevprev = d;
			d        = d->next;
		}
		d->next = new Directory;
		prev    = d;
		d       = d->next;
	}

	for (i = 0;
		 p[i] != 0 && p[i] != '\\' && p[i] != '/' && i < std::size(d->name) - 1;
		 i++) {
		d->name[i] = p[i];
	}

	d->name[i] = 0;

	if (i == std::size(d->name) - 1) {
		// 'if there is no separator where we stopped the path component is too
		// long so discard everything after what we copied
		if (p[i] && (p[i] != '\\' && p[i] != '/')) {
			return std::strlen(p);
		}
	}

	// Skip all 'slashes'
	while (p[i] && (p[i] == '\\' || p[i] == '/')) {
		i++;
	}

	// Check for . and ..
	if (!std::strcmp(d->name, ".")) {
		delete d;
		prev->next = nullptr;
	} else if (!std::strcmp(d->name, "..")) {
		delete d;
		delete prev;
		if (prevprev) {
			prevprev->next = nullptr;

			// if there is no prevprev we've gone back upto the root
		} else {
			dirs = nullptr;
		}
	}

	return i;
}

// Add path
void Path::AddString(const char* p) {
	// Root dir of this drive
	if (*p == '\\' || *p == '/') {
		// Remove all the entires
		RemoveAll();

		p++;

		// Could be network drive
		if (*p == '\\') {
			network = true;
			p++;
		}
	} else if (p[0] && p[1] == ':') {    // Drive
		RemoveAll();
		drive   = *p;
		network = false;
		p += 2;
	}

	// Skip all slashes
	while (*p && (*p == '\\' || *p == '/')) {
		p++;
	}

	while (*p) {
		p += Addit(p);
	}
}

void Path::GetString(char* p, int max_strlen) {
	p[0] = 0;

	if (network) {
		strcat_safe(p, "\\\\", max_strlen);
	} else if (drive) {
		_snprintf(p, max_strlen, "%c:\\", drive);
	} else {
		strcat_safe(p, "\\", max_strlen);
	}

	Directory* d = dirs;
	while (d) {
		strcat_safe(p, d->name, max_strlen);
		d = d->next;
		if (d) {
			strcat_safe(p, "\\", max_strlen);
		}
	}
}

//             //
// The exports //
//             //

extern "C" {

//
// Get the Game paths from the config file
//
__declspec(dllexport) void __cdecl GetExultGamePaths(
		char* ExultDir, char* BGPath, char* SIPath, int MaxPath) {
	MessageBoxDebug(nullptr, ExultDir, "ExultDir", MB_OK);
	MessageBoxDebug(nullptr, BGPath, "BGPath", MB_OK);
	MessageBoxDebug(nullptr, SIPath, "SIPath", MB_OK);
	std::memset(BGPath, 0, MaxPath);
	std::memset(SIPath, 0, MaxPath);
	const int p_size = strlen(ExultDir) + strlen("/exult.cfg") + MAX_STRLEN;
	char*     p      = new char[p_size];

	// Get the complete path for the config file
	Path config_path(ExultDir);
	config_path.AddString("exult.cfg");
	config_path.GetString(p, p_size);
	setup_program_paths();

	static const char* si_pathdef = CFG_SI_NAME;
	static const char* bg_pathdef = CFG_BG_NAME;

	MessageBoxDebug(nullptr, ExultDir, p, MB_OK);

	try {
		// Open config file
		Configuration config;
		if (get_system_path("<CONFIG>") == ".") {
			config.read_config_file(p);
		} else {
			config.read_config_file("exult.cfg");
		}
		std::string dir;

		// SI Path
		config.value("config/disk/game/serpentisle/path", dir, si_pathdef);
		if (dir != si_pathdef) {
			Path si(ExultDir);
			si.AddString(dir.c_str());
			si.GetString(SIPath, MaxPath);
		} else {
			std::strncpy(SIPath, si_pathdef, MaxPath);
		}

		// BG Path
		config.value("config/disk/game/blackgate/path", dir, bg_pathdef);
		if (dir != bg_pathdef) {
			Path bg(ExultDir);
			bg.AddString(dir.c_str());
			bg.GetString(BGPath, MaxPath);
		} else {
			std::strncpy(BGPath, bg_pathdef, MaxPath);
		}
	} catch (...) {
		std::strncpy(BGPath, bg_pathdef, MaxPath);
		std::strncpy(SIPath, si_pathdef, MaxPath);
	}

	delete[] p;
}

//
// Check if a string is Null or Empty or Entirely whitespace
//
bool IsNullEmptyOrWhitespace(const char* s) {
	// if it is not null check for non white space charactere
	if (s) {
		for (; *s; ++s) {
			// if any charaxter is not space and not blank return false;
			if (!std::isspace(*s) && !std::isblank(*s)) {
				return false;
			}
		}
	}
	// string is null or has no non whitespace characters
	return true;
}

//
// Set Game paths in the config file
//

__declspec(dllexport) void __cdecl SetExultGamePaths(
		char* ExultDir, char* BGPath, char* SIPath) {
	MessageBoxDebug(nullptr, ExultDir, "ExultDir", MB_OK);
	MessageBoxDebug(nullptr, BGPath, "BGPath", MB_OK);
	MessageBoxDebug(nullptr, SIPath, "SIPath", MB_OK);

	const int p_size = strlen(ExultDir) + strlen("/exult.cfg") + MAX_STRLEN;
	char*     p      = new char[p_size];

	Path config_path(ExultDir);
	config_path.AddString("exult.cfg");
	config_path.GetString(p, p_size);
	setup_program_paths();

	MessageBoxDebug(nullptr, p, "WriteConfig: p", MB_OK);

	try {
		// Open config file
		Configuration config;
		if (get_system_path("<CONFIG>") == ".") {
			config.read_config_file(p);
		} else {
			config.read_config_file("exult.cfg");
		}
		char existing_bgpath[MAX_PATH];
		char existing_sipath[MAX_PATH];
		GetExultGamePaths(ExultDir, existing_bgpath, existing_sipath, MAX_PATH);

		// Set BG Path
		MessageBoxDebug(nullptr, p, "WriteConfig: BG", MB_OK);

		// Only write new path if it is actually valid or existing value is
		// invalid
		if (!IsNullEmptyOrWhitespace(BGPath)
			&& (VerifyBGDirectory(BGPath)
				|| !VerifyBGDirectory(existing_bgpath))) {
			config.set("config/disk/game/blackgate/path", BGPath, true);
			// Only write default if there isn't already a value set.
		} else if (!config.key_exists("config/disk/game/blackgate/path")) {
			config.set("config/disk/game/blackgate/path", "blackgate", true);
		}
		// Set SI Path
		MessageBoxDebug(nullptr, p, "WriteConfig: SI", MB_OK);
		// Only write new path if it is actually valid or existing value is
		// invalid
		if (!IsNullEmptyOrWhitespace(SIPath)
			&& (VerifySIDirectory(SIPath)
				|| !VerifySIDirectory(existing_sipath))) {
			config.set("config/disk/game/serpentisle/path", SIPath, true);
			// Only write default if there isn't already a value set.
		} else if (!config.key_exists("config/disk/game/serpentisle/path")) {
			config.set(
					"config/disk/game/serpentisle/path", "serpentisle", true);
		}
		// Set Defaults
		std::string s;
		for (const auto& [key, value] : config_defaults) {
			config.value(key, s, "");
			if (s.empty()) {
				config.set(key, value, true);
			}
		}

		// Fix broken SI SFX stuff
		config.value("config/disk/game/serpentisle/waves", s, "");

		const char* si_sfx = s.c_str();
		int         slen   = std::strlen(si_sfx);
		slen -= std::strlen("jmsfxsi.flx");

		if (slen >= 0 && !std::strcmp(si_sfx + slen, "jmsfxsi.flx")) {
			char* fixed = new char[slen + 1];
			std::strncpy(fixed, si_sfx, slen);
			fixed[slen] = 0;
			s           = fixed;
			s += "jmsisfx.flx";

			config.set("config/disk/game/serpentisle/waves", s, true);
		}

		config.write_back();
	} catch (...) {
	}

	delete[] p;
}

//
// Verify the BG Directory contains the right stuff
//
__declspec(dllexport) int __cdecl VerifyBGDirectory(char* path) {
	if (IsNullEmptyOrWhitespace(path)) {
		return false;
	}
	const std::string s(path);
	add_system_path("<STATIC>", s + "/static");

	// Check all the BASE files
	for (const auto* file : BASE_Files) {
		if (!U7exists(file)) {
			return false;
		}
	}

	// Check all the IFIX files
	// for (i = 0; i < 144; i++) {
	// 	char num[4];
	// 	std::snprintf(num, sizeof(num), "%02X", i);

	// 	std::string s(U7IFIX);
	// 	s += num;

	// 	if (!U7exists(s)) {
	// 		return false;
	// 	}
	// }

	// Check all the BG files
	for (const auto* file : BG_Files) {
		if (!U7exists(file)) {
			return false;
		}
	}

	return true;
}

//
// Verify the SI Directorys contains the right stuff
//
__declspec(dllexport) int __cdecl VerifySIDirectory(char* path) {
	if (IsNullEmptyOrWhitespace(path)) {
		return false;
	}
	const std::string s(path);
	add_system_path("<STATIC>", s + "/static");

	// Check all the BASE files
	for (const auto* file : BASE_Files) {
		if (!U7exists(file)) {
			return false;
		}
	}

	// Check all the IFIX files
	// for (i = 0; i < 144; i++) {
	// 	char num[4];
	// 	std::snprintf(num, sizeof(num), "%02X", i);

	// 	std::string s(U7IFIX);
	// 	s += num;

	// 	if (!U7exists(s)) {
	// 		return false;
	// 	}
	// }

	// Check all the SI files
	for (const auto* file : SI_Files) {
		if (!U7exists(file)) {
			return false;
		}
	}

	return true;
}
}

BOOL APIENTRY DllMain(
		HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	ignore_unused_variable_warning(hModule, lpReserved);
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
