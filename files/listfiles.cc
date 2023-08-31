/*
Copyright (C) 2001-2022 The Exult Team

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
#  include <config.h>
#endif

#include <cstdlib>
#include <cctype>
#include <cstdio>

#include <string>
#include <cstring>
#include <iostream>

using std::string;

#include "utils.h"
#include "listfiles.h"

// TODO: If SDL ever adds directory traversal to rwops, update U7ListFiles() to
//       use it.


// System Specific Code for Windows
#if defined(_WIN32)

// Need this for _findfirst, _findnext, _findclose
#include <windows.h>
#include <tchar.h>

int U7ListFiles(const std::string &mask, FileList &files) {
	const string    path(get_system_path(mask));
	const TCHAR     *lpszT;
	WIN32_FIND_DATA fileinfo;
	HANDLE          handle;
	char            *stripped_path;
	int             i;
	int             nLen;
	int             nLen2;

#ifdef UNICODE
	const char *name = path.c_str();
	nLen = strlen(name) + 1;
	LPTSTR lpszT2 = static_cast<LPTSTR>(_alloca(nLen * 2));
	lpszT = lpszT2;
	MultiByteToWideChar(CP_ACP, 0, name, -1, lpszT2, nLen);
#else
	lpszT = path.c_str();
#endif

	handle = FindFirstFile(lpszT, &fileinfo);

	stripped_path = new char [path.length() + 1];
	strcpy(stripped_path, path.c_str());

	for (i = strlen(stripped_path) - 1; i; i--)
		if (stripped_path[i] == '\\' || stripped_path[i] == '/')
			break;

	if (stripped_path[i] == '\\' || stripped_path[i] == '/')
		stripped_path[i + 1] = 0;


#ifdef DEBUG
	std::cerr << "U7ListFiles: " << mask << " = " << path << std::endl;
#endif

	// Now search the files
	if (handle != INVALID_HANDLE_VALUE) {
		do {
			nLen = std::strlen(stripped_path);
			nLen2 = _tcslen(fileinfo.cFileName) + 1;
			char *filename = new char [nLen + nLen2];
			strcpy(filename, stripped_path);
#ifdef UNICODE
			WideCharToMultiByte(CP_ACP, 0, fileinfo.cFileName, -1, filename + nLen, nLen2, nullptr, nullptr);
#else
			std::strcat(filename, fileinfo.cFileName);
#endif

			files.push_back(filename);
#ifdef DEBUG
			std::cerr << filename << std::endl;
#endif
			delete [] filename;
		} while (FindNextFile(handle, &fileinfo));
	}

	if (GetLastError() != ERROR_NO_MORE_FILES) {
		LPTSTR lpMsgBuf;
		char *str;
		FormatMessage(
		    FORMAT_MESSAGE_ALLOCATE_BUFFER |
		    FORMAT_MESSAGE_FROM_SYSTEM |
		    FORMAT_MESSAGE_IGNORE_INSERTS,
		    nullptr,
		    GetLastError(),
		    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		    reinterpret_cast<LPTSTR>(&lpMsgBuf),
		    0,
		    nullptr
		);
#ifdef UNICODE
		nLen2 = _tcslen(lpMsgBuf) + 1;
		str = static_cast<char *>(_alloca(nLen));
		WideCharToMultiByte(CP_ACP, 0, lpMsgBuf, -1, str, nLen2, nullptr, nullptr);
#else
		str = lpMsgBuf;
#endif
		std::cerr << "Error while listing files: " << str << std::endl;
		LocalFree(lpMsgBuf);
	}

#ifdef DEBUG
	std::cerr << files.size() << " filenames" << std::endl;
#endif

	delete [] stripped_path;
	FindClose(handle);
	return 0;
}

#else   // This system has glob.h

#include <glob.h>

#ifdef ANDROID
#include <SDL3/SDL_system.h>
#endif


static int U7ListFilesImp(const std::string &path, FileList &files) {
	glob_t globres;
	int err = glob(path.c_str(), GLOB_NOSORT, nullptr, &globres);

	switch (err) {
	case 0:   //OK
		for (size_t i = 0; i < globres.gl_pathc; i++) {
			files.push_back(globres.gl_pathv[i]);
		}
		globfree(&globres);
		return 0;
	case 3:   //no matches
		return 0;
	default:  //error
		std::cerr << "Glob error " << err << std::endl;
		return err;
	}
}

int U7ListFiles(const std::string &mask, FileList &files) {
    string path(get_system_path(mask));
    int result = U7ListFilesImp(path, files);
#ifdef ANDROID
    // TODO: If SDL ever adds directory traversal to rwops use it instead of
    // glob() so that we pick up platform-specific paths and behaviors like
    // this.
    if (result != 0) {
        result = U7ListFilesImp(SDL_AndroidGetInternalStoragePath() + ("/" + path), files);
    }
#endif
    return result;
}

#endif
