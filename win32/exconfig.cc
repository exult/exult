/*
 *  Copyright (C) 2000-2011  Ryan Nunn
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
 *  EXCONFIG is a dll that is used by the InstallShield Wizard to get and set the
 *  Black Gate and Serpent Isle paths.
 *
 */

#include "exconfig.h"
#include "Configuration.h"
#include "utils.h"
#include <string>
#include <cstring>
#include <cstdio>
#include "fnames.h"

#define MAX_STRLEN  512

#ifdef _DEBUF
#define MessageBoxDebug(a,b,c,d) MessageBox(a,b,c,d);
#else
#define MessageBoxDebug(a,b,c,d)
#endif

const char *config_defaults[] = {
	"config/disk/game/blackgate/keys",  "(default)",
	"config/disk/game/serpentisle/keys",    "(default)",
	"config/audio/enabled",         "yes",
	"config/audio/effects/enabled",     "yes",
	"config/audio/effects/convert",     "gs",
	"config/audio/midi/enabled",        "yes",
	"config/audio/midi/convert",        "gm",
	"config/audio/midi/reverb/enabled", "no",
	"config/audio/midi/reverb/level",   "0",
	"config/audio/midi/chorus/enabled", "no",
	"config/audio/midi/chorus/level",   "0",
	"config/audio/midi/volume_curve",   "1.000000",
	"config/audio/speech/enabled",      "yes",
	"config/gameplay/cheat",        "yes",
	"config/gameplay/skip_intro",       "no",
	"config/gameplay/skip_splash",      "no",
	"config/video/width",           "320",
	"config/video/height",          "200",
	"config/video/scale",           "2",
	"config/video/scale_method",        "2xSaI",
	"config/video/fullscreen",      "no",
	"config/video/disable_fades",       "no",
	"config/video/gamma/red",       "1.00",
	"config/video/gamma/green",     "1.00",
	"config/video/gamma/blue",      "1.00",
	0
};

const char *BASE_Files[] = {
	SHAPES_VGA,
	FACES_VGA,
	GUMPS_VGA,
	FONTS_VGA,
	INITGAME,
	USECODE,
	0
};

const char *BG_Files[] = {
	ENDGAME,
	0
};

const char *SI_Files[] = {
	PAPERDOL,
	0
};

//
// Path Helper class
//
class Path {
	struct Directory {
		char        name[256];
		Directory   *next;

		Directory() : next(0) {
			name[0] = 0;
		}
	};

	bool        network;// Networked?
	char        drive;  // Drive letter (if set)
	Directory   *dirs;  // Directories

	void RemoveAll();
	int Addit(const char *p);
public:

	Path() : network(false), drive(0), dirs(0) { }
	Path(const char *p) : network(false), drive(0), dirs(0) {
		AddString(p);
	}
	~Path();

	void RemoveLast() {
		Addit("..");
	}
	void AddString(const char *p);
	void GetString(char *p, int max_strlen = MAX_STRLEN);

};

// Destructor
Path::~Path() {
	RemoveAll();
	network = false;
	drive = 0;
}

void Path::RemoveAll() {
	Directory *d = dirs;
	while (d) {
		Directory *next = d->next;
		delete d;
		d = next;
	}
	dirs = 0;
}

int Path::Addit(const char *p) {
	Directory *d = dirs;
	Directory *prev = 0;
	Directory *prevprev = 0;
	int i;

	// Check for . and ..

	// Create new
	if (!d) {
		dirs = d = new Directory;
	} else {
		while (d->next) {
			prevprev = d;
			d = d->next;
		}
		d->next = new Directory;
		prev = d;
		d = d->next;
	}

	for (i = 0; p[i] != 0 && p[i] != '\\' && p[i] != '/'; i++)
		d->name[i] = p[i];

	d->name[i] = 0;

	// Skip all 'slashes'
	while (p[i] && (p[i] == '\\' || p[i] == '/')) i++;

	// Check for . and ..
	if (!std::strcmp(d->name, ".")) {
		delete d;
		prev->next = 0;
	} else if (!std::strcmp(d->name, "..")) {
		delete d;
		delete prev;
		prevprev->next = 0;

	}

	return i;
}

// Add path
void Path::AddString(const char *p) {
	int len = std::strlen(p);

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

	} else if (p[0] && p[1] == ':') { // Drive
		RemoveAll();
		drive = *p;
		network = false;
		p += 2;
	}

	// Skip all slashes
	while (*p && (*p == '\\' || *p == '/')) p++;

	while (*p) {
		p += Addit(p);
	}
}

void Path::GetString(char *p, int max_strlen) {
	p[0] = 0;

	if (network) std::strncat(p, "\\\\", max_strlen);
	else if (drive) _snprintf(p, max_strlen, "%c:\\", drive);
	else std::strncat(p, "\\", max_strlen);

	Directory *d = dirs;
	while (d) {
		std::strncat(p, d->name, max_strlen);
		d = d->next;
		if (d) std::strncat(p, "\\", max_strlen);
	}

}


//             //
// The exports //
//             //

#ifdef __cplusplus
extern "C" {
#endif

//
// Get the Game paths from the config file
//
	__declspec(dllexport) void __stdcall GetExultGamePaths(char *ExultDir, char *BGPath, char *SIPath, int MaxPath) {
		MessageBoxDebug(nullptr, ExultDir, "ExultDir", MB_OK);
		MessageBoxDebug(nullptr, BGPath, "BGPath", MB_OK);
		MessageBoxDebug(nullptr, SIPath, "SIPath", MB_OK);

		int p_size = strlen(ExultDir) + strlen("/exult.cfg") + MAX_STRLEN;
		char *p = new char[p_size];

		// Get the complete path for the config file
		Path config_path(ExultDir);
		config_path.AddString("exult.cfg");
		config_path.GetString(p, p_size);
		setup_program_paths();

		const static char *si_pathdef = CFG_SI_NAME;
		const static char *bg_pathdef = CFG_BG_NAME;

		MessageBoxDebug(nullptr, ExultDir, p, MB_OK);

		try {
			// Open config file
			Configuration config;
			if (get_system_path("<CONFIG>") == ".")
				config.read_config_file(p);
			else
				config.read_config_file("exult.cfg");

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

		delete [] p;
	}

//
// Set Game paths in the config file
//
	__declspec(dllexport) void __stdcall SetExultGamePaths(char *ExultDir, char *BGPath, char *SIPath) {
		MessageBoxDebug(nullptr, ExultDir, "ExultDir", MB_OK);
		MessageBoxDebug(nullptr, BGPath, "BGPath", MB_OK);
		MessageBoxDebug(nullptr, SIPath, "SIPath", MB_OK);

		int i;

		int p_size = strlen(ExultDir) + strlen("/exult.cfg") + MAX_STRLEN;
		char *p = new char[p_size];

		Path config_path(ExultDir);
		config_path.AddString("exult.cfg");
		config_path.GetString(p, p_size);
		setup_program_paths();

		MessageBoxDebug(nullptr, p, "WriteConfig: p", MB_OK);

		try {
			// Open config file
			Configuration config;
			if (get_system_path("<CONFIG>") == ".")
				config.read_config_file(p);
			else
				config.read_config_file("exult.cfg");

			// Set BG Path
			MessageBoxDebug(nullptr, p, "WriteConfig: BG", MB_OK);
			config.set("config/disk/game/blackgate/path", BGPath, true);

			// Set SI Path
			MessageBoxDebug(nullptr, p, "WriteConfig: SI", MB_OK);
			config.set("config/disk/game/serpentisle/path", SIPath, true);

			// Set Defaults
			std::string s;
			for (i = 0; config_defaults[i]; i += 2) {
				config.value(config_defaults[i], s, "");
				if (s.empty()) config.set(config_defaults[i], config_defaults[i + 1], true);
			}

			// Fix broken SI SFX stuff
			config.value("config/disk/game/serpentisle/waves", s, "");

			const char *si_sfx = s.c_str();
			int slen = std::strlen(si_sfx);
			slen -= std::strlen("jmsfxsi.flx");

			if (slen >= 0 && !std::strcmp(si_sfx + slen, "jmsfxsi.flx")) {
				char *fixed = new char[slen + 1];
				std::strncpy(fixed, si_sfx, slen);
				fixed[slen] = 0;
				s = fixed;
				s += "jmsisfx.flx";

				config.set("config/disk/game/serpentisle/waves", s, true);
			}

			config.write_back();
		} catch (...) {
		}

		delete [] p;
	}

//
// Verify the BG Directory contains the right stuff
//
	__declspec(dllexport) int __stdcall VerifyBGDirectory(char *path) {
		int i;

		std::string s(path);
		add_system_path("<STATIC>", s + "/static");

		// Check all the BASE files
		for (i = 0; BASE_Files[i]; i++) if (!U7exists(BASE_Files[i])) return false;

		// Check all the IFIX files
		//for (i = 0; i < 144; i++)
		//{
		//  char num[4];
		//  std::sprintf(num, "%02X", i);
		//
		//  string s(U7IFIX);
		//  s += num;
		//
		//  if (!U7exists(s.c_str()))
		//      return false;
		//}

		// Check all the BG files
		for (i = 0; BG_Files[i]; i++) if (!U7exists(BG_Files[i])) return false;

		return true;
	}

//
// Verify the SI Directorys contains the right stuff
//
	__declspec(dllexport) int __stdcall VerifySIDirectory(char *path) {
		int i;

		std::string s(path);
		add_system_path("<STATIC>", s + "/static");

		// Check all the BASE files
		for (i = 0; BASE_Files[i]; i++) if (!U7exists(BASE_Files[i])) return false;

		// Check all the IFIX files
		//for (i = 0; i < 144; i++)
		//{
		//  char num[4];
		//  std::sprintf(num, "%02X", i);
		//
		//  string s(U7IFIX);
		//  s += num;
		//
		//  if (!U7exists(s.c_str()))
		//      return false;
		//}

		// Check all the SI files
		for (i = 0; SI_Files[i]; i++) if (!U7exists(SI_Files[i]))   return false;

		return true;
	}


#ifdef __cplusplus
}
#endif

BOOL APIENTRY DllMain(HINSTANCE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
                     ) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

