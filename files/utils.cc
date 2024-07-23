/*
 *  utils.cc - Common utility routines.
 *
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
#	include <config.h>
#endif

#include "utils.h"

#include "exceptions.h"
#include "fnames.h"
#include "ignore_unused_variable_warning.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <vector>

#ifdef SDL_PLATFORM_IOS
#	include "ios_utils.h"
#endif

#ifdef _WIN32
#	include <direct.h>    // For mkdir and chdir
#	include <shlobj.h>
#	include <windows.h>
#	include <io.h>
#endif

#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
#	include <CoreFoundation/CoreFoundation.h>
#	include <sys/param.h>    // for MAXPATHLEN
#endif

#ifdef ANDROID
#	include <SDL3/SDL_system.h>
#endif

using std::ios;
using std::string;

// Function prototypes

static void switch_slashes(string& name);
static bool base_to_uppercase(string& str, int count);

// Global factories for instantiating file streams
static U7IstreamFactory istream_factory
		= [](const char* s, std::ios_base::openmode mode) {
			  return std::make_unique<std::ifstream>(s, mode);
		  };

static U7OstreamFactory ostream_factory
		= [](const char* s, std::ios_base::openmode mode) {
			  return std::make_unique<std::ofstream>(s, mode);
		  };

// Ugly hack for supporting different paths

static std::map<string, string> path_map;
static std::map<string, string> stored_path_map;

void store_system_paths() {
	stored_path_map = path_map;
}

void reset_system_paths() {
	path_map = stored_path_map;
}

static bool is_path_separator(char cc) {
#ifdef _WIN32
	return cc == '/' || cc == '\\';
#else
	return cc == '/';
#endif
}

static string remove_trailing_slash(const string& value) {
	string new_path = value;
	if (is_path_separator(new_path.back())) {
#ifdef EXTRA_DEBUG
		std::cerr << "Warning, trailing slash in path: \"" << new_path << "\""
				  << std::endl;
#endif
		new_path.resize(new_path.size() - 1);
	}

	return new_path;
}

void add_system_path(const string& key, const string& value) {
	if (!value.empty()) {
		if (value.find(key) != string::npos) {
			std::cerr << "Error: system path '" << key
					  << "' is being defined in terms of itself: '" << value
					  << "'." << std::endl;
			exit(1);
		} else {
			path_map[key] = remove_trailing_slash(value);
		}
	} else {
		clear_system_path(key);
	}
}

void clone_system_path(const string& new_key, const string& old_key) {
	if (is_system_path_defined(old_key)) {
		path_map[new_key] = path_map[old_key];
	} else {
		clear_system_path(new_key);
	}
}

void clear_system_path(const string& key) {
	auto iter = path_map.find(key);
	if (iter != path_map.end()) {
		path_map.erase(iter);
	}
}

/*
 *  Has a path been entered?
 */
bool is_system_path_defined(const string& path) {
	return path_map.find(path) != path_map.end();
}

/*
 *  Convert an exult path (e.g. "<DATA>/exult.flx") into a system path
 */

string get_system_path(const string& path) {
	string            new_path = path;
	string::size_type pos;
	string::size_type pos2;

	pos  = new_path.find('>');
	pos2 = new_path.find('<');
	// If there is no separator, return the path as is
	int cnt = 10;
	while (pos != string::npos && pos2 == 0 && cnt-- > 0) {
		pos += 1;
		// See if we can translate this prefix
		const string syspath = new_path.substr(0, pos);
		if (is_system_path_defined(syspath)) {
			string new_prefix = path_map[syspath];
			new_prefix += new_path.substr(pos);
			new_path.swap(new_prefix);
			pos  = new_path.find('>');
			pos2 = new_path.find('<');
		} else {
#ifdef DEBUG
			std::cerr << "Unrecognized system path '" << syspath
					  << "' in path '" << path << "'." << std::endl;
#endif
			break;
		}
	}
	if (cnt <= 0) {
		std::cerr << "Could not convert path '" << path
				  << "' into a filesystem path, due to mutually recursive "
					 "system paths."
				  << std::endl;
		std::cerr << "Expansion resulted in '" << new_path << "'." << std::endl;
		exit(1);
	}

	switch_slashes(new_path);
#ifdef _WIN32
	if (new_path.back() == '/' || new_path.back() == '\\') {
#	ifdef EXTRA_DEBUG
		std::cerr << "Warning, trailing slash in path: \"" << new_path << "\""
				  << std::endl;
#	endif
		new_path += '.';
	}
#	ifdef NO_WIN32_PATH_SPACES
	pos                    = new_path.find('*');
	pos2                   = new_path.find('?');
	string::size_type pos3 = new_path.find(' ');
	// pos and pos2 will equal each other if neither is found
	// and we'll only convert to short if a space is found
	// really, we don't need to do this, but hey, you never know
	if (pos == pos2 && pos3 != string::npos) {
		int num_chars = GetShortPathName(new_path.c_str(), nullptr, 0);
		if (num_chars > 0) {
			std::string short_path(num_chars + 1);
			GetShortPathName(new_path.c_str(), &short_path[0], num_chars + 1);
			new_path = std::move(short_path);
		}
		// else std::cerr << "Warning unable to get short path for: " <<
		// new_path << std::endl;
	}
#	endif
#endif
	return new_path;
}

/*
 *  Convert a buffer to upper-case.
 *
 *  Output: ->original buffer, changed to upper case.
 */

void to_uppercase(string& str) {
	for (auto& chr : str) {
		chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
	}
}

string to_uppercase(const string& str) {
	string s(str);
	to_uppercase(s);
	return s;
}

/*
 *  Convert just the last 'count' parts of a filename to uppercase.
 *  returns false if there are less than 'count' parts
 */

static bool base_to_uppercase(string& str, int count) {
	if (count <= 0) {
		return true;
	}

	int todo = count;
	// Go backwards.
	string::reverse_iterator X;
	for (X = str.rbegin(); X != str.rend(); ++X) {
		// Stop at separator.
		if (*X == '/' || *X == '\\' || *X == ':') {
			todo--;
		}
		if (todo <= 0) {
			break;
		}

		*X = static_cast<char>(std::toupper(static_cast<unsigned char>(*X)));
	}
	if (X == str.rend()) {
		todo--;    // start of pathname counts as separator too
	}

	// false if it didn't reach 'count' parts
	return todo <= 0;
}

static void switch_slashes(string& name) {
#ifdef _WIN32
	for (char& X : name) {
		if (X == '/') {
			X = '\\';
		}
	}
#else
	ignore_unused_variable_warning(name);
	// do nothing
#endif
}

void U7set_istream_factory(U7IstreamFactory factory) {
	istream_factory = std::move(factory);
}

void U7set_ostream_factory(U7OstreamFactory factory) {
	ostream_factory = std::move(factory);
}

/*
 *  Open a file for input,
 *  trying the original name (lower case), and the upper case version
 *  of the name.
 *
 *  Output: 0 if couldn't open.
 */

std::unique_ptr<std::istream> U7open_in(
		const char* fname,     // May be converted to upper-case.
		bool        is_text    // Should the file be opened in text mode
) {
	std::ios_base::openmode mode = std::ios::in;
	if (!is_text) {
		mode |= std::ios::binary;
	}
	string                        name           = get_system_path(fname);
	int                           uppercasecount = 0;
	std::unique_ptr<std::istream> in;
	do {
		try {
			// std::cout << "trying: " << name << std::endl;
			in = istream_factory(name.c_str(), mode);
		} catch (std::exception&) {
		}
		if (in && in->good() && !in->fail()) {
			// std::cout << "got it!" << std::endl;
			return in;    // found it!
		}
	} while (base_to_uppercase(name, ++uppercasecount));

	// file not found.
	throw file_open_exception(get_system_path(fname));
	return nullptr;
}

/*
 *  Open a file for output,
 *  trying the original name (lower case), and the upper case version
 *  of the name.
 *
 *  Output: 0 if couldn't open.
 */

std::unique_ptr<std::ostream> U7open_out(
		const char* fname,     // May be converted to upper-case.
		bool        is_text    // Should the file be opened in text mode
) {
	std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
	if (!is_text) {
		mode |= std::ios::binary;
	}
	string name = get_system_path(fname);

	std::unique_ptr<std::ostream> out;

	int uppercasecount = 0;
	do {
		out = ostream_factory(name.c_str(), mode);
		if (out && out->good()) {
			return out;    // found it!
		}
	} while (base_to_uppercase(name, ++uppercasecount));

	// file not found.
	throw file_open_exception(get_system_path(fname));
	return nullptr;
}

DIR* U7opendir(const char* fname    // May be converted to upper-case.
) {
	string name           = get_system_path(fname);
	int    uppercasecount = 0;

	do {
		DIR* dir = opendir(name.c_str());    // Try to open
#ifdef ANDROID
		// TODO: If SDL ever adds directories to rwops use it instead

		if (!dir) {
			string internalpath
					= SDL_GetAndroidInternalStoragePath() + ("/" + name);
			dir = opendir(internalpath.c_str());
		}
#endif
		if (dir) {
			return dir;    // found it!
		}
	} while (base_to_uppercase(name, ++uppercasecount));
	return nullptr;
}

/*
 *  Remove a file taking care of paths etc.
 *
 */

void U7remove(const char* fname    // May be converted to upper-case.
) {
	string name = get_system_path(fname);

#if defined(_WIN32) && defined(UNICODE)
	const char* n     = name.c_str();
	int         nLen  = std::strlen(n) + 1;
	LPTSTR      lpszT = (LPTSTR)alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	DeleteFile(lpszT);
#else

	int uppercasecount = 0;
	do {
		std::unique_ptr<std::istream> in;
		try {
			if (istream_factory) {
				in = istream_factory(name.c_str(), std::ios_base::in);
			} else {
				in = std::make_unique<std::ifstream>(
						name.c_str(), std::ios_base::in);
			}
		} catch (std::exception&) {
		}
		if (in && in->good() && !in->fail()) {
			in.reset();
			std::remove(name.c_str());
		}
	} while (base_to_uppercase(name, ++uppercasecount));
	std::remove(name.c_str());
#endif
}

/*
 *  Open a "static" game file by first looking in <PATCH>, then
 *  <STATIC>.
 *  Output: 0 if couldn't open. We do NOT throw exceptions.
 */

std::unique_ptr<std::istream> U7open_static(
		const char* fname,     // May be converted to upper-case.
		bool        is_text    // Should file be opened in text mode
) {
	string name;

	name = string("<PATCH>/") + fname;
	try {
		auto in = U7open_in(name.c_str(), is_text);
		if (in) {
			return in;
		}
	} catch (std::exception&) {
	}
	name = string("<STATIC>/") + fname;
	try {
		auto in = U7open_in(name.c_str(), is_text);
		if (in) {
			return in;
		}
	} catch (std::exception&) {
	}
	return nullptr;
}

/*
 *  See if a file exists.
 */

bool U7exists(const char* fname    // May be converted to upper-case.
) {
	try {
		// First check if we can open it as a file.
		if (U7open_in(fname)) {
			return true;
		}
	} catch (std::exception&) {
	}
	try {
		// If not, try to open it as a directory.
		auto* dir = U7opendir(fname);
		if (dir) {
			closedir(dir);
			return true;
		}
	} catch (std::exception&) {
	}
	return false;
}

/*
 *  Create a directory
 */

int U7mkdir(
		const char* dirname,    // May be converted to upper-case.
		int         mode) {
	string name = get_system_path(dirname);
	// remove any trailing slashes
	const string::size_type pos = name.find_last_not_of('/');
	if (pos != string::npos) {
		name.resize(pos + 1);
	}
#if defined(_WIN32) && defined(UNICODE)
	const char* n     = name.c_str();
	int         nLen  = std::strlen(n) + 1;
	LPTSTR      lpszT = (LPTSTR)alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	ignore_unused_variable_warning(mode);
	return CreateDirectory(lpszT, nullptr);
#elif defined(_WIN32)
	ignore_unused_variable_warning(mode);
	return mkdir(name.c_str());
#else
	return mkdir(name.c_str(), mode);    // Create dir. if not already there.
#endif
}

#ifdef _WIN32
class shell32_wrapper {
protected:
	HMODULE hLib;
	using SHGetFolderPathFunc = HRESULT(WINAPI*)(
			HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags,
			LPTSTR pszPath);
	SHGetFolderPathFunc SHGetFolderPath;

	/*
	// Will leave this for someone with Vista/W7 to implement.
	using SHGetKnownFolderPathFunc = HRESULT (WINAPI*) (
		REFKNOWNFOLDERID rfid,
		DWORD dwFlags,
		HANDLE hToken,
		PWSTR *ppszPath
	);
	SHGetKnownFolderPathFunc SHGetKnownFolderPath;
	*/

	template <typename Dest>
	Dest get_function(const char* func) {
		static_assert(
				sizeof(Dest) == sizeof(decltype(GetProcAddress(hLib, func))),
				"sizeof(FARPROC) is not equal to sizeof(Dest)!");
		Dest    fptr;
		FARPROC optr = GetProcAddress(hLib, func);
		std::memcpy(&fptr, &optr, sizeof(optr));
		return fptr;
	}

public:
	shell32_wrapper() {
		hLib = LoadLibrary("shell32.dll");
		if (hLib != nullptr) {
			SHGetFolderPath
					= get_function<SHGetFolderPathFunc>("SHGetFolderPathA");
			/*
			SHGetKnownFolderPath =
			get_function<SHGetKnownFolderPathFunc>("SHGetKnownFolderPath");
			*/
		} else {
			SHGetFolderPath = nullptr;
			// SHGetKnownFolderPath = nullptr;
		}
	}

	~shell32_wrapper() {
		FreeLibrary(hLib);
	}

	string Get_local_appdata() {
		/*  Not yet.
		if (SHGetKnownFolderPath != nullptr)
			{
			}
		else */
		if (SHGetFolderPath != nullptr) {
			CHAR    szPath[MAX_PATH];
			HRESULT code = SHGetFolderPath(
					nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, szPath);
			if (code == E_INVALIDARG) {
				return string();
			} else if (code == S_FALSE) {    // E_FAIL for Unicode version.
				// Lets try creating it through the API flag:
				code = SHGetFolderPath(
						nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
						nullptr, 0, szPath);
			}
			if (code == E_INVALIDARG) {
				return string();
			} else if (code == S_OK) {
				return string(reinterpret_cast<const char*>(szPath));
			}
			// We don't have a folder yet at this point. This means we have
			// a truly ancient version of Windows.
			// Just to be sure, we fall back to the old behaviour.
			// Is anyone still needing this?
		}
		return string();
	}
};

#	ifdef USE_CONSOLE
void redirect_output(const char* prefix) {
	ignore_unused_variable_warning(prefix);
}

void cleanup_output(const char* prefix) {
	ignore_unused_variable_warning(prefix);
}
#	else
static std::string Get_home();

struct unsynch_from_stdio {
	const bool old_synch_status = std::ostream::sync_with_stdio(false);

	~unsynch_from_stdio() noexcept {
		std::ostream::sync_with_stdio(old_synch_status);
	}
};

bool stdout_redirected = false;
bool stderr_redirected = false;

// Pulled from exult_studio.cc.
void redirect_output(const char* prefix) {
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
	DWORD  stdout_type   = GetFileType(stdout_handle);
	DWORD  stderr_type   = GetFileType(stderr_handle);
	if (stdout_type != FILE_TYPE_UNKNOWN && stderr_type != FILE_TYPE_UNKNOWN) {
		// If we already have valid destinations for both stdout and stderr,
		// we don't need to do anything. This happens, for example, on MSYS2
		// in MinTTY.
		return;
	}
	auto attach_console = []() {
		// If we already have a console, quit.
		if (GetConsoleWindow() != nullptr) {
			return true;
		}
		// Attach to the parent process console.
		if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
			return true;
		}
#		ifdef DEBUG
		return AllocConsole() != 0;
#		else
		return false;
#		endif
	};
	auto redirect_stream = [](FILE* stream, const char* device, HANDLE handle,
							  DWORD& type, int mode, size_t size) {
		// Only redirect if the stream is not already being redirected.
		if (handle != INVALID_HANDLE_VALUE && type == FILE_TYPE_UNKNOWN) {
			// Flush the output in case anything is queued
			fflush(stream);
			FILE* new_stream = freopen(device, "w", stream);
			if (new_stream != nullptr) {
				setvbuf(stream, nullptr, mode, size);
				return true;
			}
			type = GetFileType(handle);
		}
		return false;
	};
	auto redirect_stdout = [&](const char* device) {
		// Line buffered
		return redirect_stream(
				stdout, device, stdout_handle, stdout_type, _IOLBF, BUFSIZ);
	};
	auto redirect_stderr = [&](const char* device) {
		// No buffering
		return redirect_stream(
				stderr, device, stderr_handle, stderr_type, _IONBF, 0);
	};
	const unsynch_from_stdio resyncher;
	if (attach_console()) {
		// If we are in something like Windows Terminal, we need to connect to
		// the parent terminal and manually redirect stdout/stderr to the
		// console. Note: in powershell or cmd.exe, this will give the
		// impression of being stuck when exult/ES exits because they return to
		// prompt immediately after exult/ES start, and this gets lost in the
		// stdout/stderr output.
		// Another possibility is to compile as a console application. We will
		// always have an attached terminal in this case, even when starting
		// from Windows Explorer. This console can be destroyed, but it will
		// cause it to flash into view, then disappear right away.
		// TODO: Figure out a way to make Exult/ES "return" the terminal.
		// If both calls succeeded in redirecting output to the terminal, we can
		// bail out early.
		if (redirect_stdout("CONOUT$") && redirect_stderr("CONOUT$")) {
			return;
		}
		// Otherwise, fall through to redirect whichever ones failed to a file.
	}

	// Starting from GUI, or from cmd.exe, we will need to redirect the output.
	// Paths to the output files.
	const string folderPath = Get_home() + "/" + prefix;
	const string stdoutPath = folderPath + "out.txt";
	stdout_redirected       = redirect_stdout(stdoutPath.c_str());
	const string stderrPath = folderPath + "err.txt";
	stderr_redirected       = redirect_stderr(stderrPath.c_str());
}

void cleanup_output(const char* prefix) {
	const string folderPath           = Get_home() + "/" + prefix;
	auto         clear_empty_redirect = [&](FILE* stream, const char* suffix) {
        // Get the Win32 HANDLE Of the stream
        HANDLE handle
                = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream)));
        fflush(stream);
        // Only try to delete the stream if it is an actual file
        if (handle != INVALID_HANDLE_VALUE
            && GetFileType(handle) == FILE_TYPE_DISK && ftell(stream) == 0) {
            fclose(stream);
            const string stream_path = folderPath + suffix;
            remove(stream_path.c_str());
        }
	};
	if (stdout_redirected) {
		clear_empty_redirect(stdout, "out.txt");
	}
	if (stderr_redirected) {
		clear_empty_redirect(stderr, "err.txt");
	}
	if (GetConsoleWindow() != nullptr) {
		FreeConsole();
	}
}
#	endif    // USE_CONSOLE
#endif        // _WIN32

static std::string home_directory;

void U7set_home(std::string home) {
	home_directory = std::move(home);
}

string Get_home() {
	if (!home_directory.empty()) {
		return home_directory;
	}
#ifdef _WIN32
#	ifdef PORTABLE_EXULT_WIN32
	home_directory = ".";
#	else
	if (is_system_path_defined("<HOME>") && get_system_path("<HOME>") == ".") {
		home_directory = ".";
	} else {
		shell32_wrapper shell32;
		home_directory = shell32.Get_local_appdata();
		if (!home_directory.empty()) {
			home_directory += "\\Exult";
		} else {
			home_directory = ".";
		}
	}
#	endif    // PORTABLE_WIN32_EXULT
#else 
#	ifdef ANDROID
	const char *home = SDL_GetAndroidInternalStoragePath();
	home_directory = home;
#	else
	const char* home = nullptr;
	if ((home = getenv("HOME")) != nullptr) {
		home_directory = home;
	}
#	endif
#endif
	return home_directory;
}

#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
struct CFDeleter {
	void operator()(CFTypeRef ptr) const {
		if (ptr) {
			CFRelease(ptr);
		}
	}
};

void setup_app_bundle_resource() {
	// setup the location of the resource folder inside the app bundle
	std::unique_ptr<std::remove_pointer<CFURLRef>::type, CFDeleter> bundleUrl{
			std::move(CFBundleCopyBundleURL(CFBundleGetMainBundle()))};
	if (bundleUrl) {
		unsigned char bundlebuf[MAXPATHLEN];
		if (CFURLGetFileSystemRepresentation(
					bundleUrl.get(), true, bundlebuf, sizeof(bundlebuf))) {
			string bundlepath(reinterpret_cast<const char*>(bundlebuf));
#	ifdef MACOSX
			bundlepath += "/Contents/Info.plist";
#	else
			bundlepath += "/Info.plist";
#	endif
			if (U7exists(bundlepath)) {
				std::unique_ptr<std::remove_pointer<CFURLRef>::type, CFDeleter>
						fileUrl{std::move(CFBundleCopyResourcesDirectoryURL(
								CFBundleGetMainBundle()))};
				if (fileUrl) {
					unsigned char buf[MAXPATHLEN];
					if (CFURLGetFileSystemRepresentation(
								fileUrl.get(), true, buf, sizeof(buf))) {
						string path(reinterpret_cast<const char*>(buf));
						add_system_path("<APP_BUNDLE_RES>", path);
					}
				}
			}
		}
	}
}
#endif

void setup_data_dir(const std::string& data_path, const char* runpath) {
#if defined(MACOSX) || defined(SDL_PLATFORM_IOS)
	// Can we try from the bundle?
	setup_app_bundle_resource();
	if (is_system_path_defined("<APP_BUNDLE_RES>")) {
		std::string path = get_system_path("<APP_BUNDLE_RES>");
		path += "/data";
		add_system_path("<BUNDLE>", path);
		if (!U7exists(BUNDLE_EXULT_FLX)) {
			clear_system_path("<BUNDLE>");
		}
	}
#endif
#ifdef SDL_PLATFORM_IOS
	if (is_system_path_defined("<BUNDLE>")) {
		// We have the flxfiles in the bundle, so lets use it.
		// But lets use <DATA> in the iTunes file sharing.
		string path(IOSGetDocumentsDir());
		path += "/data";
		add_system_path("<DATA>", path);
		return;
	}
#endif
#ifdef ANDROID
	// We always have the APK with the bundled flx files
	add_system_path("<BUNDLE>", "data");
	// for optional data use the InternalStoragePath data folder
	const char *androidpath = SDL_GetAndroidInternalStoragePath();
	string path(androidpath);
	path += "/data";
	add_system_path("<DATA>", path);
	return;
#endif

	// First, try the cfg value:
	add_system_path("<DATA>", data_path);
	if (U7exists(EXULT_FLX)) {
		return;
	}

	// Now, try default -- if warranted.
	if (data_path != EXULT_DATADIR) {
		add_system_path("<DATA>", EXULT_DATADIR);
		if (U7exists(EXULT_FLX)) {
			return;
		}
	}

	// Due to SDL_IOStream internally using SDL_OpenFPFromBundleOrFallback in
	// OSX (which looks for the file inside the bundle before looking in CWD),
	// there is no reason ever to check CWD if we found the bundle path above.
	if (!is_system_path_defined("<BUNDLE>")) {
		// Try "data" subdirectory for current working directory:
		add_system_path("<DATA>", "data");
		if (U7exists(EXULT_FLX)) {
			return;
		}
	}

	// Try "data" subdirectory for exe directory:
	const char* sep = std::strrchr(runpath, '/');
	if (!sep) {
		sep = std::strrchr(runpath, '\\');
	}
	if (sep) {
		const int   plen = sep - runpath;
		std::string dpath(runpath, plen + 1);
		dpath += "data";
		add_system_path("<DATA>", dpath);
	} else {
		add_system_path("<DATA>", "data");
	}
	if (U7exists(EXULT_FLX)) {
		return;
	}

	if (is_system_path_defined("<BUNDLE>")) {
		// We have the bundle, so lets use it. But lets also leave <DATA>
		// with a sensible default.
		add_system_path("<DATA>", data_path);
		return;
	}

	// We've tried them all...
	std::cerr << "Could not find 'exult.flx' anywhere." << std::endl;
	std::cerr << "Please make sure Exult is correctly installed," << std::endl;
	std::cerr
			<< "and the Exult data path is specified in the configuration file."
			<< std::endl;
	std::cerr << "(See the README file for more information)" << std::endl;
	exit(-1);
}

static string Get_config_dir(const string& home_dir) {
#ifdef SDL_PLATFORM_IOS
	ignore_unused_variable_warning(home_dir);
	return IOSGetDocumentsDir();
#elif defined(MACOSX)
	string config_dir(home_dir);
	config_dir += "/Library/Preferences";
	return config_dir;
#elif defined(SDL_PLATFORM_HAIKU)
	string config_dir(home_dir);
	config_dir += "/config/settings/exult";
	return config_dir;
#else
	return home_dir;
#endif
}

static string Get_savehome_dir(
		const string& home_dir, const string& config_dir) {
#ifdef SDL_PLATFORM_IOS
	ignore_unused_variable_warning(home_dir);
	string savehome_dir(config_dir);
	savehome_dir += "/save";
	return savehome_dir;
#elif defined(MACOSX)
	ignore_unused_variable_warning(config_dir);
	string savehome_dir(home_dir);
	savehome_dir += "/Library/Application Support/Exult";
	return savehome_dir;
#elif defined(SDL_PLATFORM_HAIKU)
	ignore_unused_variable_warning(config_dir);
	string savehome_dir(home_dir);
	savehome_dir += "/config/settings/exult";
	return savehome_dir;
#elif defined(XWIN)
	ignore_unused_variable_warning(config_dir);
	string savehome_dir(home_dir);
	savehome_dir += "/.exult";
	return savehome_dir;
#else
	ignore_unused_variable_warning(config_dir);
	return home_dir;
#endif
}

static string Get_gamehome_dir(
		const string& home_dir, const string& config_dir) {
#ifdef SDL_PLATFORM_IOS
	ignore_unused_variable_warning(home_dir);
	string gamehome_dir(config_dir);
	gamehome_dir += "/game";
	return gamehome_dir;
#elif defined(MACOSX)
	ignore_unused_variable_warning(home_dir, config_dir);
	return "/Library/Application Support/Exult";
#elif defined(SDL_PLATFORM_HAIKU)
	ignore_unused_variable_warning(config_dir);
	string gamehome_dir(home_dir);
	gamehome_dir += "/config/non-packaged/data/exult";
	return gamehome_dir;
#elif defined(XWIN)
	ignore_unused_variable_warning(home_dir, config_dir);
	return EXULT_DATADIR;
#else
	ignore_unused_variable_warning(home_dir, config_dir);
	return ".";
#endif
}

void setup_program_paths() {
	const string home_dir(Get_home());
	const string config_dir(Get_config_dir(home_dir));
	const string savehome_dir(Get_savehome_dir(home_dir, config_dir));
	const string gamehome_dir(Get_gamehome_dir(home_dir, config_dir));

	if (!is_system_path_defined("<HOME>") || get_system_path("<HOME>") != ".") {
		add_system_path("<HOME>", home_dir);
	}
	add_system_path("<CONFIG>", config_dir);
	add_system_path("<SAVEHOME>", savehome_dir);
	add_system_path("<GAMEHOME>", gamehome_dir);
	U7mkdir("<HOME>", 0755);
	U7mkdir("<CONFIG>", 0755);
	U7mkdir("<SAVEHOME>", 0755);
}

/*
 *  Change the current directory
 */

int U7chdir(const char* dirname    // May be converted to upper-case.
) {
	return chdir(dirname);
}

/*
 *  Copy a file. May throw an exception.
 */
void U7copy(const char* src, const char* dest) {
	std::unique_ptr<std::istream> pIn;
	std::unique_ptr<std::ostream> pOut;
	try {
		pIn  = U7open_in(src);
		pOut = U7open_out(dest);
	} catch (exult_exception&) {
		throw;
	}
	if (!pIn) {
		throw file_open_exception(src);
	}
	if (!pOut) {
		throw file_open_exception(dest);
	}
	auto& in  = *pIn;
	auto& out = *pOut;
	out << in.rdbuf();
	out.flush();
	const bool inok  = in.good();
	const bool outok = out.good();
	if (!inok) {
		throw file_read_exception(src);
	}
	if (!outok) {
		throw file_write_exception(dest);
	}
}

/*
 *  Take log2 of a number.
 *
 *  Output: Log2 of n (0 if n==0).
 */

int Log2(uint32 n) {
	int result = 0;
	for (n = n >> 1; n; n = n >> 1) {
		result++;
	}
	return result;
}

/*
 *  Get the highest power of 2 <= given number.
 *  From http://aggregate.org/MAGIC/#Log2%20of%20an%20Integer
 *
 *  Output: Integer containing the highest bit of input.
 */

uint32 msb32(uint32 x) {
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return x & ~(x >> 1);
}

/*
 *  Get the lowest power of 2 >= given number.
 *
 *  Output: First power of 2 >= input (0 if n==0).
 */

int fgepow2(uint32 n) {
	const uint32 l = msb32(n);
	return l < n ? (l << 1) : l;
}

/*
 *  Replacement for non-standard strdup function.
 */

char* newstrdup(const char* s) {
	if (!s) {
		throw std::invalid_argument("nullptr pointer passed to newstrdup");
	}
	char* ret = new char[std::strlen(s) + 1];
	std::strcpy(ret, s);
	return ret;
}

/*
 *  Build a file name with the map directory before it; ie,
 *      Get_mapped_name("<GAMEDAT>/ireg, 3, to) will store
 *          "<GAMEDAT>/map03/ireg".
 */

char* Get_mapped_name(const char* from, int num, char* to) {
	if (num == 0) {
		strcpy(to, from);    // Default map.
	} else {
		const char* sep = strrchr(from, '/');
		assert(sep != nullptr);
		size_t len = sep - from;
		memcpy(to, from, len);    // Copy dir.
		strcpy(to + len, MULTIMAP_DIR);
		len                                  = strlen(to);
		constexpr static const char hexLUT[] = "0123456789abcdef";
		to[len]                              = hexLUT[num / 16];
		to[len + 1]                          = hexLUT[num % 16];
		strcpy(to + len + 2, sep);
	}
	return to;
}

/*
 *  Find next existing map, starting with a given number.
 *
 *  Output: # found, or -1.
 */

int Find_next_map(
		int start,    // Start here.
		int maxtry    // Max. # to try.
) {
	char fname[128];

	for (int i = start; maxtry; --maxtry, ++i) {
		if (U7exists(Get_mapped_name("<STATIC>/", i, fname))
			|| U7exists(Get_mapped_name("<PATCH>/", i, fname))) {
			return i;
		}
	}
	return -1;
}
