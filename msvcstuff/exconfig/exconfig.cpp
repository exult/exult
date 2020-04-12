/*
 *  Copyright (C) 2000-2002  Ryan Nunn
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

#include "stdafx.h"
#include "exconfig.h"
#include "Configuration.h"
#include <string>

#define MAX_STRLEN  512

#ifdef _DEBUF
#define MessageBoxDebug(a,b,c,d) MessageBox(a,b,c,d);
#else
#define MessageBoxDebug(a,b,c,d)
#endif

char *config_defaults[] = {
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
	void GetString(char p[MAX_STRLEN]);

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

	for (int i = 0; p[i] != 0 && p[i] != '\\' && p[i] != '/'; i++)
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

void Path::GetString(char p[MAX_STRLEN]) {
	p[0] = 0;

	if (network) std::strncat(p, "\\\\", MAX_STRLEN);
	else if (drive) _snprintf(p, MAX_STRLEN, "%c:\\", drive);
	else std::strncat(p, "\\", MAX_STRLEN);

	Directory *d = dirs;
	while (d) {
		std::strncat(p, d->name, MAX_STRLEN);
		d = d->next;
		if (d) std::strncat(p, "\\", MAX_STRLEN);
	}

}
// The exports

#ifdef __cplusplus
extern "C" {
#endif

	EXCONFIG_API LONG APIENTRY GetPaths(HWND hwnd, LPLONG lpIValue, LPSTR lpszValue) {
		Path    path(lpszValue);

		char p[MAX_STRLEN];

		path.AddString("exult.cfg");
		path.GetString(p);

		char *si_pathdef = CFG_SI_NAME;
		char *bg_pathdef = CFG_BG_NAME;

		MessageBoxDebug(hwnd, lpszValue, p, MB_OK);

		try {
			//chdir (lpszValue);
			Configuration config;
			config.read_config_file(p);

			std::string data_directory;

			config.value("config/disk/game/serpentisle/path", data_directory, si_pathdef);
			if (data_directory != si_pathdef) {
				path.~Path();
				path.AddString(lpszValue);
				path.AddString(data_directory.c_str());
				path.GetString(p);
			} else {
				std::strncpy(p, si_pathdef, MAX_STRLEN);
			}

			config.value("config/disk/game/blackgate/path", data_directory, bg_pathdef);
			if (data_directory != bg_pathdef) {
				path.~Path();
				path.AddString(lpszValue);
				path.AddString(data_directory.c_str());
				path.GetString(lpszValue);
			} else {
				std::strncpy(lpszValue, bg_pathdef, MAX_STRLEN);
			}

			std::strncat(lpszValue, "\n", MAX_STRLEN);
			std::strncat(lpszValue, p, MAX_STRLEN);

			MessageBoxDebug(hwnd, lpszValue, data_directory.c_str(), MB_OK);
		} catch (...) {
			std::strncpy(lpszValue, bg_pathdef, MAX_STRLEN);
			std::strncat(lpszValue, "\n", MAX_STRLEN);
			std::strncat(lpszValue, si_pathdef, MAX_STRLEN);
		}
		return 0;
	}

	EXCONFIG_API LONG APIENTRY WriteConfig(HWND hwnd, LPLONG lpIValue, LPSTR lpszValue) {
		MessageBoxDebug(hwnd, lpszValue, "WriteConfig", MB_OK);

		int len = std::strlen(lpszValue);
		char *p = new char [len + 1];
		int i, j, bgstart, sistart;

		for (i = 0; i < len && lpszValue[i] != '\n'; i++) p[i] = lpszValue[i];
		p[i] = 0;
		bgstart = i + 1;

		Path    path(p);

		path.AddString("exult.cfg");
		path.GetString(p);
		MessageBoxDebug(hwnd, p, "WriteConfig: p", MB_OK);

		try {
			//chdir (lpszValue);
			Configuration config;
			config.read_config_file(p);

			for (j = 0, i = bgstart; i < len && lpszValue[i] != '\n'; j++, i++) p[j] = lpszValue[i];
			p[j] = 0;
			sistart = i + 1;
			if (j > 1 && p[j - 1] == '\\') p[j - 1] = 0;

			MessageBoxDebug(hwnd, p, "WriteConfig: BG", MB_OK);
			config.set("config/disk/game/blackgate/path", p, true);

			for (j = 0, i = sistart ; i < len && lpszValue[i]; j++, i++) p[j] = lpszValue[i];
			p[j] = 0;
			if (j > 1 && p[j - 1] == '\\') p[j - 1] = 0;

			MessageBoxDebug(hwnd, p, "WriteConfig: SI", MB_OK);
			config.set("config/disk/game/serpentisle/path", p, true);

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
		return 0;
	}

	EXCONFIG_API LONG APIENTRY LaunchApp(HWND hwnd, LPLONG lpIValue, LPSTR lpszValue) {
		Path            path(lpszValue);
		char            p[MAX_STRLEN];
		PROCESS_INFORMATION pi;
		STARTUPINFO     si;

		path.RemoveLast();
		path.GetString(p);

		std::memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);

		MessageBoxDebug(hwnd, lpszValue, "LaunchApp", MB_OK);

		CreateProcess(nullptr, lpszValue, nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr,
		              *lpIValue == 1 ? p : nullptr, &si, &pi);

		return 0;
	}

#ifdef __cplusplus
}
#endif

BOOL APIENTRY DllMain(HANDLE hModule,
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

