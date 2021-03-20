/*
 *  utils.cc - Common utility routines.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
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

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <map>
#include <list>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __IPHONEOS__
#  include "ios_utils.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h> // For mkdir and chdir
#endif

#include <cassert>
#include <exception>
#include "exceptions.h"
#include "utils.h"
#include "fnames.h"
#include "ignore_unused_variable_warning.h"

#if defined(MACOSX) || defined(__IPHONEOS__)
#  include <CoreFoundation/CoreFoundation.h>
#  include <sys/param.h> // for MAXPATHLEN
#endif

using std::cerr;
using std::string;
using std::ios;

// Function prototypes

static void switch_slashes(string &name);
static bool base_to_uppercase(string &str, int count);


// Wrap a few functions
inline int stat(const std::string &file_name, struct stat *buf) {
	return stat(file_name.c_str(), buf);
}

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

static string remove_trailing_slash(const string &value) {
	string new_path = value;
	if (is_path_separator(new_path.back())) {
		std::cerr << "Warning, trailing slash in path: \"" << new_path << "\"" << std::endl;
		new_path.resize(new_path.size() - 1);
	}

	return new_path;
}

void add_system_path(const string &key, const string &value) {
	if (!value.empty()) {
		if (value.find(key) != string::npos) {
			std::cerr << "Error: system path '" << key
			          << "' is being defined in terms of itself: '"
			          << value << "'." << std::endl;
			exit(1);
		} else
			path_map[key] = remove_trailing_slash(value);
	} else {
		clear_system_path(key);
	}
}

void clone_system_path(const string &new_key, const string &old_key) {
	if (is_system_path_defined(old_key)) {
		path_map[new_key] = path_map[old_key];
	} else {
		clear_system_path(new_key);
	}
}

void clear_system_path(const string &key) {
	auto iter = path_map.find(key);
	if (iter != path_map.end())
		path_map.erase(iter);
}

/*
 *  Has a path been entered?
 */
bool is_system_path_defined(const string &path) {
	return path_map.find(path) != path_map.end();
}

/*
 *  Convert an exult path (e.g. "<DATA>/exult.flx") into a system path
 */

string get_system_path(const string &path) {
	string new_path = path;
	string::size_type pos;
	string::size_type pos2;

	pos = new_path.find('>');
	pos2 = new_path.find('<');
	// If there is no separator, return the path as is
	int cnt = 10;
	while (pos != string::npos && pos2 == 0 && cnt-- > 0) {
		pos += 1;
		// See if we can translate this prefix
		string syspath = new_path.substr(0, pos);
		if (is_system_path_defined(syspath)) {
			string new_prefix = path_map[syspath];
			new_prefix += new_path.substr(pos);
			new_path.swap(new_prefix);
			pos = new_path.find('>');
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
		          << "' into a filesystem path, due to mutually recursive system paths." << std::endl;
		std::cerr << "Expansion resulted in '" << new_path << "'." << std::endl;
		exit(1);
	}

	switch_slashes(new_path);
#ifdef _WIN32
	if (new_path.back() == '/' || new_path.back() == '\\') {
		//std::cerr << "Trailing slash in path: \"" << new_path << "\"" << std::endl << "...compensating, but go complain to Colourless anyway" << std::endl;
		std::cerr << "Warning, trailing slash in path: \"" << new_path << "\"" << std::endl;
		new_path += '.';
	}
#ifdef NO_WIN32_PATH_SPACES
	pos = new_path.find('*');
	pos2 = new_path.find('?');
	string::size_type pos3 = new_path.find(' ');
	// pos and pos2 will equal each other if neither is found
	// and we'll only convert to short if a space is found
	// really, we don't need to do this, but hey, you never know
	if (pos == pos2 && pos3 != string::npos) {
		int num_chars = GetShortPathName(new_path.c_str(), nullptr, 0);
		if (num_chars > 0) {
			char *short_path = new char [num_chars + 1];
			GetShortPathName(new_path.c_str(), short_path, num_chars + 1);
			new_path = short_path;
			delete [] short_path;
		}
		//else std::cerr << "Warning unable to get short path for: " << new_path << std::endl;
	}
#endif
#endif
	return new_path;
}


/*
 *  Convert a buffer to upper-case.
 *
 *  Output: ->original buffer, changed to upper case.
 */

void to_uppercase(string &str) {
	for (auto& chr : str) {
		chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
	}
}

string to_uppercase(const string &str) {
	string s(str);
	to_uppercase(s);
	return s;
}

/*
 *  Convert just the last 'count' parts of a filename to uppercase.
 *  returns false if there are less than 'count' parts
 */

static bool base_to_uppercase(string &str, int count) {
	if (count <= 0) return true;

	int todo = count;
	// Go backwards.
	string::reverse_iterator X;
	for (X = str.rbegin(); X != str.rend(); ++X) {
		// Stop at separator.
		if (*X == '/' || *X == '\\' || *X == ':')
			todo--;
		if (todo <= 0)
			break;

		*X = static_cast<char>(std::toupper(static_cast<unsigned char>(*X)));
	}
	if (X == str.rend())
		todo--; // start of pathname counts as separator too

	// false if it didn't reach 'count' parts
	return todo <= 0;
}



static void switch_slashes(
    string &name
) {
#ifdef _WIN32
	for (char& X : name) {
		if (X == '/')
			X = '\\';
	}
#else
	ignore_unused_variable_warning(name);
	// do nothing
#endif
}

/*
 *  Open a file for input,
 *  trying the original name (lower case), and the upper case version
 *  of the name.
 *
 *  Output: 0 if couldn't open.
 */

bool U7open(
    std::ifstream &in,          // Input stream to open.
    const char *fname,          // May be converted to upper-case.
    bool is_text                // Should the file be opened in text mode
) {
	std::ios_base::openmode mode = std::ios::in;
	if (!is_text) mode |= std::ios::binary;
	string name = get_system_path(fname);
	int uppercasecount = 0;
	do {
		// We first "clear" the stream object. This is done to prevent
		// problems when re-using stream objects
		in.clear();
		try {
			//std::cout << "trying: " << name << std::endl;
			in.open(name.c_str(), mode);        // Try to open
		} catch (std::exception &)
		{}
		if (in.good() && !in.fail()) {
			//std::cout << "got it!" << std::endl;
			return true; // found it!
		}
	} while (base_to_uppercase(name, ++uppercasecount));

	// file not found.
	throw file_open_exception(get_system_path(fname));
	return false;
}

/*
 *  Open a file for output,
 *  trying the original name (lower case), and the upper case version
 *  of the name.
 *
 *  Output: 0 if couldn't open.
 */

bool U7open(
    std::ofstream &out,         // Output stream to open.
    const char *fname,          // May be converted to upper-case.
    bool is_text                // Should the file be opened in text mode
) {
	std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
	if (!is_text) mode |= std::ios::binary;
	string name = get_system_path(fname);

	// We first "clear" the stream object. This is done to prevent
	// problems when re-using stream objects
	out.clear();

	int uppercasecount = 0;
	do {
		out.open(name.c_str(), mode);       // Try to open
		if (out.good())
			return true; // found it!
		out.clear();    // Forget ye not
	} while (base_to_uppercase(name, ++uppercasecount));

	// file not found.
	throw file_open_exception(get_system_path(fname));
	return false;
}

DIR *U7opendir(
    const char *fname			// May be converted to upper-case.
) {
	string name = get_system_path(fname);
	int uppercasecount = 0;

	do {
		DIR *dir = opendir(name.c_str()); // Try to open
		if (dir)
			return dir; // found it!
	} while (base_to_uppercase(name, ++uppercasecount));
	return nullptr;
}

/*
 *  Remove a file taking care of paths etc.
 *
 */

void U7remove(
    const char *fname         // May be converted to upper-case.
) {
	string name = get_system_path(fname);

#if defined(_WIN32) && defined(UNICODE)
	const char *n = name.c_str();
	int nLen = std::strlen(n) + 1;
	LPTSTR lpszT = (LPTSTR) alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	DeleteFile(lpszT);
#else

	struct stat sbuf;

	int uppercasecount = 0;
	do {
		bool exists = (stat(name, &sbuf) == 0);
		if (exists) {
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

bool U7open_static(
    std::ifstream &in,      // Input stream to open.
    const char *fname,      // May be converted to upper-case.
    bool is_text            // Should file be opened in text mode
) {
	string name;

	name = string("<PATCH>/") + fname;
	try {
		if (U7open(in, name.c_str(), is_text))
			return true;
	} catch (std::exception &)
	{}
	name = string("<STATIC>/") + fname;
	try {
		if (U7open(in, name.c_str(), is_text))
			return true;
	} catch (std::exception &)
	{}
	return false;
}

/*
 *  See if a file exists.
 */

bool U7exists(
    const char *fname         // May be converted to upper-case.
) {
	string name = get_system_path(fname);
	struct stat sbuf;

	int uppercasecount = 0;
	do {
		bool exists = (stat(name, &sbuf) == 0);
		if (exists)
			return true; // found it!
	} while (base_to_uppercase(name, ++uppercasecount));

	// file not found
	return false;
}

/*
 *  Create a directory
 */

int U7mkdir(
    const char *dirname,    // May be converted to upper-case.
    int mode
) {
	string name = get_system_path(dirname);
	// remove any trailing slashes
	string::size_type pos = name.find_last_not_of('/');
	if (pos != string::npos)
		name.resize(pos + 1);
#if defined(_WIN32) && defined(UNICODE)
	const char *n = name.c_str();
	int nLen = std::strlen(n) + 1;
	LPTSTR lpszT = (LPTSTR) alloca(nLen * 2);
	MultiByteToWideChar(CP_ACP, 0, n, -1, lpszT, nLen);
	ignore_unused_variable_warning(mode);
	return CreateDirectory(lpszT, nullptr);
#elif defined(_WIN32)
	ignore_unused_variable_warning(mode);
	return mkdir(name.c_str());
#else
	return mkdir(name.c_str(), mode); // Create dir. if not already there.
#endif
}

#ifdef _WIN32
class shell32_wrapper {
protected:
	HMODULE hLib;
	using SHGetFolderPathFunc = HRESULT (WINAPI*)(
	    HWND hwndOwner,
	    int nFolder,
	    HANDLE hToken,
	    DWORD dwFlags,
	    LPTSTR pszPath
	);
	SHGetFolderPathFunc      SHGetFolderPath;
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
	Dest get_function(const char *func) {
		static_assert(sizeof(Dest) == sizeof(decltype(GetProcAddress(hLib, func))), "sizeof(FARPROC) is not equal to sizeof(Dest)!");
		Dest fptr;
		const FARPROC optr = GetProcAddress(hLib, func);
		std::memcpy(&fptr, &optr, sizeof(optr));
		return fptr;
	}

public:
	shell32_wrapper() {
		hLib = LoadLibrary("shell32.dll");
		if (hLib != nullptr) {
			SHGetFolderPath      = get_function<SHGetFolderPathFunc>("SHGetFolderPathA");
			/*
			SHGetKnownFolderPath = get_function<SHGetKnownFolderPathFunc>("SHGetKnownFolderPath");
			*/
		} else {
			SHGetFolderPath      = nullptr;
			//SHGetKnownFolderPath = nullptr;
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
			CHAR szPath[MAX_PATH];
			HRESULT code = SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA,
			                               nullptr, 0, szPath);
			if (code == E_INVALIDARG)
				return string();
			else if (code == S_FALSE)   // E_FAIL for Unicode version.
				// Lets try creating it through the API flag:
				code = SHGetFolderPath(nullptr,
				                       CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
				                       nullptr, 0, szPath);
			if (code == E_INVALIDARG)
				return string();
			else if (code == S_OK)
				return string(reinterpret_cast<const char *>(szPath));
			// We don't have a folder yet at this point. This means we have
			// a truly ancient version of Windows.
			// Just to be sure, we fall back to the old behaviour.
			// Is anyone still needing this?
		}
		return string();
	}
};

#ifdef USE_CONSOLE
void redirect_output(const char *prefix) {
	ignore_unused_variable_warning(prefix);
}
void cleanup_output(const char *prefix) {
	ignore_unused_variable_warning(prefix);
}
#else
std::string Get_home();

// Pulled from exult_studio.cc.
void redirect_output(const char *prefix) {
	if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_PIPE) {
		// If we are at a msys/msys2 shell, do not redirect the output, and
		// print it to console instead.
		return;
	}
	// Starting from GUI, or from cmd.exe, we will need to redirect the output.
	// It is possible to connect to the parent cmd.exe console using WinAPI
	// (namely, AttachConsole(ATTACH_PARENT_PROCESS)). However, cmd.exe detaches
	// the program since it (correctly) thinks we are a GUI application, and
	// returns to the prompt. Thus, when we exit, it seems like we are stuck.
	// Another possibility is to compile as a console application. We will
	// always have an attached terminal in this case, even when starting from
	// Windows Explorer. This console can be destroyed, but it will cause it
	// to flash into view, then disappear right away.

	// Flush the output in case anything is queued
	fclose(stdout);
	fclose(stderr);

	string folderPath = Get_home() + "/";

	string stdoutPath = folderPath + prefix + "out.txt";
	const char *stdoutfile = stdoutPath.c_str();

	// Redirect standard input and standard output
	FILE *newfp = freopen(stdoutfile, "w", stdout);
	if (newfp == nullptr) {
		// This happens on NT
#if !defined(stdout)
		stdout = fopen(stdoutfile, "w");
#else
		newfp = fopen(stdoutfile, "w");
		if (newfp)
			*stdout = *newfp;
#endif
	}

	string stderrPath = folderPath + prefix + "err.txt";
	const char *stderrfile = stderrPath.c_str();

	newfp = freopen(stderrfile, "w", stderr);
	if (newfp == nullptr) {
		// This happens on NT
#if !defined(stderr)
		stderr = fopen(stderrfile, "w");
#else
		newfp = fopen(stderrfile, "w");
		if (newfp)
			*stderr = *newfp;
#endif
	}
	setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);  // Line buffered
	setbuf(stderr, nullptr);                   // No buffering
}

void cleanup_output(const char *prefix) {
	string folderPath = Get_home() + "/";
	if (!ftell(stdout)) {
		fclose(stdout);
		string stdoutPath = folderPath + prefix + "out.txt";
		remove(stdoutPath.c_str());
	}
	if (!ftell(stderr)) {
		fclose(stderr);
		string stderrPath = folderPath + prefix + "err.txt";
		remove(stderrPath.c_str());
	}
}
#endif // USE_CONSOLE
#endif  // _WIN32

string Get_home() {
	std::string home_dir;
#ifdef _WIN32
#ifdef PORTABLE_EXULT_WIN32
	home_dir = ".";
#else
	if (get_system_path("<HOME>") == ".")
		home_dir = ".";
	else {
		shell32_wrapper shell32;
		home_dir = shell32.Get_local_appdata();
		if (!home_dir.empty())
			home_dir += "\\Exult";
		else
			home_dir = ".";
	}
#endif // PORTABLE_WIN32_EXULT
#else
	const char *home = nullptr;
	if ((home = getenv("HOME")) != nullptr)
		home_dir = home;
#endif
	return home_dir;
}

#if defined(MACOSX) || defined(__IPHONEOS__)
struct CFDeleter
{
	void operator()(CFTypeRef ptr) const
	{
		if (ptr) CFRelease(ptr);
	}
};
#endif

void setup_data_dir(
    const std::string &data_path,
    const char *runpath
) {
#if defined(MACOSX) || defined(__IPHONEOS__)
	// Can we try from the bundle?
	std::unique_ptr<std::remove_pointer<CFURLRef>::type, CFDeleter> fileUrl {std::move(CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle()))};
	if (fileUrl) {
		unsigned char buf[MAXPATHLEN];
		if (CFURLGetFileSystemRepresentation(fileUrl.get(), true, buf, sizeof(buf))) {
			string path(reinterpret_cast<const char *>(buf));
			path += "/data";
			add_system_path("<BUNDLE>", path);
			if (!U7exists(BUNDLE_EXULT_FLX))
				clear_system_path("<BUNDLE>");
		}
	}
#endif
#ifdef __IPHONEOS__
	if (is_system_path_defined("<BUNDLE>")) {
		// We have the flxfiles in the bundle, so lets use it.
		// But lets use <DATA> in the iTunes file sharing.
		string path(ios_get_documents_dir());
		path += "/data";
		add_system_path("<DATA>", path);
		return;
	}
#endif

	// First, try the cfg value:
	add_system_path("<DATA>", data_path);
	if (U7exists(EXULT_FLX))
		return;

	// Now, try default -- if warranted.
	if (data_path != EXULT_DATADIR) {
		add_system_path("<DATA>", EXULT_DATADIR);
		if (U7exists(EXULT_FLX))
			return;
	}

	// Try "data" subdirectory for current working directory:
	add_system_path("<DATA>", "data");
	if (U7exists(EXULT_FLX))
		return;

	// Try "data" subdirectory for exe directory:
	const char *sep = std::strrchr(runpath, '/');
	if (!sep) sep = std::strrchr(runpath, '\\');
	if (sep) {
		int plen = sep - runpath;
		std::string dpath(runpath, plen + 1);
		dpath += "data";
		add_system_path("<DATA>", dpath);
	} else
		add_system_path("<DATA>", "data");
	if (U7exists(EXULT_FLX))
		return;

	if (is_system_path_defined("<BUNDLE>")) {
		// We have the bundle, so lets use it. But lets also leave <DATA>
		// with a sensible default.
		add_system_path("<DATA>", data_path);
		return;
	}

	// We've tried them all...
	std::cerr << "Could not find 'exult.flx' anywhere." << std::endl;
	std::cerr << "Please make sure Exult is correctly installed," << std::endl;
	std::cerr << "and the Exult data path is specified in the configuration file." << std::endl;
	std::cerr << "(See the README file for more information)" << std::endl;
	exit(-1);
}

static string Get_config_dir(const string& home_dir) {
#ifdef __IPHONEOS__
	ignore_unused_variable_warning(home_dir);
	return ios_get_documents_dir();
#elif defined(MACOSX)
	string config_dir(home_dir);
	config_dir += "/Library/Preferences";
	return config_dir;
#else
	return home_dir;
#endif
}

static string Get_savehome_dir(const string& home_dir, const string& config_dir) {
#ifdef __IPHONEOS__
	ignore_unused_variable_warning(home_dir);
	string savehome_dir(config_dir);
	savehome_dir += "/save";
	return savehome_dir;
#elif defined(MACOSX)
	ignore_unused_variable_warning(config_dir);
	string savehome_dir(home_dir);
	savehome_dir += "/Library/Application Support/Exult";
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

static string Get_gamehome_dir(const string& home_dir, const string& config_dir) {
#ifdef __IPHONEOS__
	ignore_unused_variable_warning(home_dir);
	string gamehome_dir(config_dir);
	gamehome_dir += "/game";
	return gamehome_dir;
#elif defined(MACOSX)
	ignore_unused_variable_warning(home_dir, config_dir);
	return "/Library/Application Support/Exult";
#elif defined(XWIN)
	ignore_unused_variable_warning(home_dir, config_dir);
	return EXULT_DATADIR;
#else
	ignore_unused_variable_warning(home_dir, config_dir);
	return ".";
#endif
}

void setup_program_paths() {
	string home_dir(Get_home());
	string config_dir(Get_config_dir(home_dir));
	string savehome_dir(Get_savehome_dir(home_dir, config_dir));
	string gamehome_dir(Get_gamehome_dir(home_dir, config_dir));

	if (get_system_path("<HOME>") != ".")
		add_system_path("<HOME>", home_dir);
	add_system_path("<CONFIG>", config_dir);
	add_system_path("<SAVEHOME>", savehome_dir);
	add_system_path("<GAMEHOME>", gamehome_dir);
	U7mkdir("<HOME>", 0755);
	U7mkdir("<CONFIG>", 0755);
	U7mkdir("<SAVEHOME>", 0755);
#ifdef MACOSX
	// setup APPBUNDLE needed to launch Exult Studio from one
	std::unique_ptr<std::remove_pointer<CFURLRef>::type, CFDeleter> fileUrl {std::move(CFBundleCopyBundleURL(CFBundleGetMainBundle()))};
	if (fileUrl) {
		unsigned char buf[MAXPATHLEN];
		if (CFURLGetFileSystemRepresentation(fileUrl.get(), true, buf, sizeof(buf))) {
			string path(reinterpret_cast<const char *>(buf));
			add_system_path("<APPBUNDLE>", path);
			// test whether it is actually an app bundle
			path += "/Contents/Info.plist";
			if (!U7exists(path))
				clear_system_path("<APPBUNDLE>");
		}
	}
#endif
}

/*
 *  Change the current directory
 */

int U7chdir(
    const char *dirname // May be converted to upper-case.
) {
	return chdir(dirname);
}

/*
 *  Copy a file. May throw an exception.
 */
void U7copy(
    const char *src,
    const char *dest
) {
	std::ifstream in;
	std::ofstream out;
	try {
		U7open(in, src);
		U7open(out, dest);
	} catch (exult_exception &e) {
		in.close();
		out.close();
		throw;
	}
	size_t bufsize = 0x8000;
	char *buf = new char[0x8000];
	in.seekg(0, ios::end);      // Get filesize.
	size_t filesize = in.tellg();
	in.seekg(0, ios::beg);
	while (filesize > 0) {      // Copy.
		size_t toread = bufsize < filesize ? bufsize : filesize;
		in.read(buf, toread);
		out.write(buf, toread);
		filesize -= toread;
	}
	out.flush();
	delete [] buf;
	bool inok = in.good();
	bool outok = out.good();
	in.close();
	out.close();
	if (!inok)
		throw file_read_exception(src);
	if (!outok)
		throw file_write_exception(dest);
}

/*
 *  Take log2 of a number.
 *
 *  Output: Log2 of n (0 if n==0).
 */

int Log2(
    uint32 n
) {
	int result = 0;
	for (n = n >> 1; n; n = n >> 1)
		result++;
	return result;
}

/*
 *  Get the highest power of 2 <= given number.
 *  From http://aggregate.org/MAGIC/#Log2%20of%20an%20Integer
 *
 *  Output: Integer containing the highest bit of input.
 */

uint32 msb32(uint32 x) {
	x |= (x >>  1);
	x |= (x >>  2);
	x |= (x >>  4);
	x |= (x >>  8);
	x |= (x >> 16);
	return x & ~(x >> 1);
}

/*
 *  Get the lowest power of 2 >= given number.
 *
 *  Output: First power of 2 >= input (0 if n==0).
 */

int fgepow2(uint32 n) {
	uint32 l = msb32(n);
	return l < n ? (l << 1) : l;
}

/*
 *  Replacement for non-standard strdup function.
 */

char *newstrdup(const char *s) {
	if (!s)
		throw std::invalid_argument("nullptr pointer passed to newstrdup");
	char *ret = new char[std::strlen(s) + 1];
	std::strcpy(ret, s);
	return ret;
}

/*
 *  Build a file name with the map directory before it; ie,
 *      Get_mapped_name("<GAMEDAT>/ireg, 3, to) will store
 *          "<GAMEDAT>/map03/ireg".
 */

char *Get_mapped_name(
    const char *from,
    int num,
    char *to
) {
	if (num == 0)
		strcpy(to, from);   // Default map.
	else {
		const char *sep = strrchr(from, '/');
		assert(sep != nullptr);
		size_t len = sep - from;
		memcpy(to, from, len);  // Copy dir.
		strcpy(to + len, MULTIMAP_DIR);
		len = strlen(to);
		constexpr static const char hexLUT[] = "0123456789abcdef";
		to[len] = hexLUT[num / 16];
		to[len + 1] = hexLUT[num % 16];
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
    int start,          // Start here.
    int maxtry          // Max. # to try.
) {
	char fname[128];

	for (int i = start; maxtry; --maxtry, ++i) {
		if (U7exists(Get_mapped_name("<STATIC>/", i, fname)) ||
		        U7exists(Get_mapped_name("<PATCH>/", i, fname)))
			return i;
	}
	return -1;
}

