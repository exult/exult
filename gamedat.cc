/*
 *  gamedat.cc - Create gamedat files from a savegame.
 *
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

#ifdef _WIN32
#include <io.h>
#endif

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if defined(XWIN)
#include <sys/stat.h>
#endif

#include <sstream>
#include "exceptions.h"
#include "fnames.h"
#include "gamewin.h"
#include "gameclk.h"
#include "gamemap.h"
#include "utils.h"
#include "gump_utils.h"
#include "game.h"
#include "Flex.h"
#include "databuf.h"
#include "Newfile_gump.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "party.h"
#include "version.h"
#include "array_size.h"

// Zip file support
#ifdef HAVE_ZIP_SUPPORT
#include "files/zip/unzip.h"
#include "files/zip/zip.h"
#endif

#include <cstddef>
#ifndef offsetof    // Broken <cstddef>? Just in case...
#   define offsetof(type, field) reinterpret_cast<long>(&(static_cast<type *>(0)->field))
#endif

using std::cout;
using std::stringstream;
using std::istream;
using std::ostream;
using std::string;
using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::ostream;
using std::size_t;
using std::strchr;
using std::strcmp;
using std::strncpy;
using std::time_t;
using std::tm;
using std::localtime;
using std::time;

// Save game compression level
extern int save_compression;

/*
 *  Write files from flex assuming first 13 characters of
 *  each flex object are an 8.3 filename.
 */
void Game_window::restore_flex_files(
    IDataSource &in,
    const char *basepath
) {
	in.seek(0x54);          // Get to where file count sits.
	const int numfiles = in.read4();
	in.seek(0x80);          // Get to file info.
	// Read pos., length of each file.
	long *finfo = new long[2 * numfiles];
	int i;
	for (i = 0; i < numfiles; i++) {
		finfo[2 * i] = in.read4();  // The position, then the length.
		finfo[2 * i + 1] = in.read4();
	}
	const int baselen = strlen(basepath);
	for (i = 0; i < numfiles; i++) { // Now read each file.
		// Get file length.
		size_t len = finfo[2 * i + 1];
		if (len <= 13)
			continue;
		len -= 13;
		in.seek(finfo[2 * i]);  // Get to it.
		char fname[50];     // Set up name.
		strcpy(fname, basepath);
		in.read(&fname[baselen], 13);
		size_t namelen = strlen(fname);
		// Watch for names ending in '.'.
		if (fname[namelen - 1] == '.')
			fname[namelen - 1] = 0;
		// Now read the file.
		char *buf = new char[len];
		in.read(buf, len);
		if (!memcmp(&fname[baselen], "map", 3)) {
			// Multimap directory entry.
			// Just for safety, we will force-terminate the filename
			// at an appropriate position.
			namelen = baselen + 5;
			fname[namelen] = 0;

			IBufferDataSource ds(buf, len);
			if (!Flex::is_flex(&ds)) {
				// Save is most likely corrupted. Ignore the file but keep
				// reading the savegame.
				std::cerr << "Error reading flex: file '" << fname
				          << "' is not a valid flex file. This probably means a corrupt save game." << endl;
			} else {
				// fname should be a path hare.
				U7mkdir(fname, 0755);
				// Append trailing slash:
				fname[namelen] = '/';
				fname[namelen + 1] = 0;
				restore_flex_files(ds, fname);
			}
			continue;
		}
		auto pOut = U7open_out(fname);
		if (!pOut)
			abort("Error opening '%s'.", fname);
		auto& out = *pOut;
		out.write(buf, len);    // Then write it out.
		delete [] buf;
		if (!out.good())
			abort("Error writing '%s'.", fname);
		cycle_load_palette();
	}
	delete [] finfo;
}

// In gamemgr/modmgr.cc because it is also needed by ES.
string get_game_identity(const char *savename, const string &title);

/*
 *  Write out the gamedat directory from a saved game.
 *
 *  Output: Aborts if error.
 */

void Game_window::restore_gamedat(
    const char *fname       // Name of savegame file.
) {
	// Check IDENTITY.
	string id = get_game_identity(fname, Game::get_gametitle());
	const string static_identity = get_game_identity(INITGAME, Game::get_gametitle());
	// Note: "*" means an old game.
	if (id.empty() || (id[0] != '*' && static_identity != id)) {
		std::string msg("Wrong identity '");
		msg += id;
		msg += "'.  Open anyway?";
		if (!Yesno_gump::ask(msg.c_str())) {
			return;
		}
	}
	// Check for a ZIP file first
#ifdef HAVE_ZIP_SUPPORT
	if (restore_gamedat_zip(fname))
		return;
#endif

	// Display red plasma during load...
	setup_load_palette();

	U7mkdir("<GAMEDAT>", 0755);     // Create dir. if not already there. Don't
	// use GAMEDAT define cause that's got a
	// trailing slash
	IFileDataSource in(fname);
	if (!in.good()) {
		if (!Game::is_editing())    // Ok if map-editing.
			throw file_read_exception(fname);
		std::cerr << "Warning (map-editing): Couldn't open '" <<
		          fname << "'" << endl;
		return;
	}

	U7remove(USEDAT);
	U7remove(USEVARS);
	U7remove(U7NBUF_DAT);
	U7remove(NPC_DAT);
	U7remove(MONSNPCS);
	U7remove(FLAGINIT);
	U7remove(GWINDAT);
	U7remove(IDENTITY);
	U7remove(GSCHEDULE);
	U7remove("<STATIC>/flags.flg");
	U7remove(GSCRNSHOT);
	U7remove(GSAVEINFO);
	U7remove(GNEWGAMEVER);
	U7remove(GEXULTVER);
	U7remove(KEYRINGDAT);
	U7remove(NOTEBOOKXML);

	cout.flush();

	restore_flex_files(in, GAMEDAT);

	cout.flush();

	load_palette_timer = 0;
}

/*
 *  Write out the gamedat directory from a saved game.
 *
 *  Output: Aborts if error.
 */

void Game_window::restore_gamedat(
    int num             // 0-9, currently.
) {
	char fname[50];         // Set up name.
	snprintf(fname, sizeof(fname), SAVENAME, num,
	         Game::get_game_type() == BLACK_GATE ? "bg" :
	         Game::get_game_type() == SERPENT_ISLE ? "si" : "dev");
	restore_gamedat(fname);
}

/*
 *  List of 'gamedat' files to save (in addition to 'iregxx'):
 */
static const char *bgsavefiles[] = {
	GSCRNSHOT,  GSAVEINFO,  // MUST BE FIRST!!
	IDENTITY,           // MUST BE #2
	GEXULTVER,  GNEWGAMEVER,
	NPC_DAT,    MONSNPCS,
	USEVARS,    USEDAT,
	FLAGINIT,   GWINDAT,
	GSCHEDULE,  NOTEBOOKXML
};
static const int bgnumsavefiles = array_size(bgsavefiles);

static const char *sisavefiles[] = {
	GSCRNSHOT,  GSAVEINFO,  // MUST BE FIRST!!
	IDENTITY,           // MUST BE #2
	GEXULTVER,  GNEWGAMEVER,
	NPC_DAT,    MONSNPCS,
	USEVARS,    USEDAT,
	FLAGINIT,   GWINDAT,
	GSCHEDULE,  KEYRINGDAT,
	NOTEBOOKXML
};
static const int sinumsavefiles = array_size(sisavefiles);

static void SavefileFromDataSource(
    Flex_writer &flex,
    IDataSource &source, // read from here
    const char *fname   // store data using this filename
) {
	flex.write_file(fname, source);
}

/*
 *  Save a single file into an IFF repository.
 */

static void Savefile(
    Flex_writer &flex,
    const char *fname           // Name of file to save.
) {
	IFileDataSource source(fname);
	if (!source.good()) {
		if (Game::is_editing())
			return;   // Newly developed game.
		throw file_read_exception(fname);
	}
	SavefileFromDataSource(flex, source, fname);
}

inline static void save_gamedat_chunks(
    Game_map *map,
    Flex_writer &flex) {
	for (int schunk = 0; schunk < 12 * 12; schunk++) {
		char iname[128];
		//Check to see if the ireg exists before trying to
		//save it; prevents crash when creating new maps
		//for existing games
		if (U7exists(map->get_schunk_file_name(U7IREG, schunk, iname)))
			Savefile(flex, iname);
		else
			flex.empty_object();	// TODO: Get rid of this by making it redundant.
	}
}

/*
 *  Save 'gamedat' into a given file.
 *
 *  Output: 0 if error (reported).
 */

void Game_window::save_gamedat(
    const char *fname,          // File to create.
    const char *savename            // User's savegame name.
) {
	// First check for compressed save game
#ifdef HAVE_ZIP_SUPPORT
	if (save_compression > 0 && save_gamedat_zip(fname, savename))
		return;
#endif

	// setup correct file list
	const int numsavefiles = (Game::get_game_type() == BLACK_GATE) ?
	                   bgnumsavefiles : sinumsavefiles;
	const char **savefiles = (Game::get_game_type() == BLACK_GATE) ?
	                         bgsavefiles : sisavefiles;

	OFileDataSource out(fname);
	int count = numsavefiles;   // Count up #files to write.
	count += 12 * 12 - 1; // First map outputs IREG's directly to
	// gamedat flex, while all others have a flex
	// of their own contained in gamedat flex.
	for (auto *map : maps)
		if (map)
			count++;
	// Use samename for title.
	Flex_writer flex(out, savename, count);
	int i;              // Start with listed files.
	for (i = 0; i < numsavefiles; i++) {
		Savefile(flex, savefiles[i]);
	}
	// Now the Ireg's.
	for (auto *map : maps) {
		if (!map)
			continue;
		if (!map->get_num())
			// Map 0 is a special case.
			save_gamedat_chunks(map, flex);
		else {
			// Multimap directory entries. Each map is stored in their
			// own flex file contained inside the general gamedat flex.
			char dname[128];
			// Need to have read/write access here.
			std::stringstream outbuf(std::ios::in | std::ios::out | std::ios::binary);
			OStreamDataSource outds(&outbuf);
			{
				Flex_writer flexbuf(outds,
			                        map->get_mapped_name(GAMEDAT, dname), 12 * 12);
				// Save chunks to memory flex
				save_gamedat_chunks(map, flexbuf);
			}
			outbuf.seekg(0);
			IStreamDataSource inds(&outbuf);
			SavefileFromDataSource(flex, inds, dname);
		}
	}
}

/*
 *  Save to one of the numbered savegame files (and update save_names).
 *
 *  Output: false if error (reported).
 */

void Game_window::save_gamedat(
    int num,            // 0-9, currently.
    const char *savename            // User's savegame name.
) {
	char fname[50];         // Set up name.
	snprintf(fname, sizeof(fname), SAVENAME, num,
	         Game::get_game_type() == BLACK_GATE ? "bg" :
	         Game::get_game_type() == SERPENT_ISLE ? "si" : "dev");
	save_gamedat(fname, savename);
	if (num >= 0 && num < 10) {
		delete [] save_names[num];  // Update name
		save_names[num] = newstrdup(savename);
	}
}

/*
 *  Read in the saved game names.
 */
void Game_window::read_save_names(
) {
	for (unsigned int i = 0; i < array_size(save_names); i++) {
		char fname[50];     // Set up name.
		snprintf(fname, sizeof(fname), SAVENAME, static_cast<int>(i),
		         GAME_BG ? "bg" : (GAME_SI ? "si" : "dev"));
		try {
			auto pIn = U7open_in(fname);
			if (!pIn) {        // Okay if file not there.
			    save_names[i] = newstrdup("");
			    continue;
			}
			auto& in = *pIn;
			char buf[0x50];     // It's at start of file.
			memset(buf, 0, sizeof(buf));
			in.read(buf, sizeof(buf) - 1);
			if (in.good())      // Okay if file not there.
				save_names[i] = newstrdup(buf);
			else
				save_names[i] = newstrdup("");
		} catch (const file_exception & /*f*/) {
			save_names[i] = newstrdup("");
		}
	}
}


void Game_window::write_saveinfo() {
	int save_count = 1;

	{
		IFileDataSource ds(GSAVEINFO);
		if (ds.good()) {
			ds.skip(10);    // Skip 10 bytes.
			save_count += ds.read2();
		}
	}

	const int party_size = party_man->get_count() + 1;

	{
		OFileDataSource out(GSAVEINFO);   // Open file; throws an exception - Don't care

		const time_t t = time(nullptr);
		tm *timeinfo = localtime(&t);

		// This order must match struct SaveGame_Details

		// Time that the game was saved
		out.write1(timeinfo->tm_min);
		out.write1(timeinfo->tm_hour);
		out.write1(timeinfo->tm_mday);
		out.write1(timeinfo->tm_mon + 1);
		out.write2(timeinfo->tm_year + 1900);

		// The Game Time that the save was done at
		out.write1(clock->get_minute());
		out.write1(clock->get_hour());
		out.write2(clock->get_day());

		out.write2(save_count);
		out.write1(party_size);

		out.write1(0);          // Unused

		out.write1(timeinfo->tm_sec);   // 15

		// Packing for the rest of the structure
		for (size_t j = offsetof(SaveGame_Details, reserved0);
		        j < sizeof(SaveGame_Details); j++)
			out.write1(0);

		for (int i = 0; i < party_size ; i++) {
			Actor *npc;
			if (i == 0)
				npc = main_actor;
			else
				npc = get_npc(party_man->get_member(i - 1));

			char name[18];
			const std::string namestr = npc->get_npc_name();
			strncpy(name, namestr.c_str(), 18);
			out.write(name, 18);
			out.write2(npc->get_shapenum());

			out.write4(npc->get_property(Actor::exp));
			out.write4(npc->get_flags());
			out.write4(npc->get_flags2());

			out.write1(npc->get_property(Actor::food_level));
			out.write1(npc->get_property(Actor::strength));
			out.write1(npc->get_property(Actor::combat));
			out.write1(npc->get_property(Actor::dexterity));
			out.write1(npc->get_property(Actor::intelligence));
			out.write1(npc->get_property(Actor::magic));
			out.write1(npc->get_property(Actor::mana));
			out.write1(npc->get_property(Actor::training));

			out.write2(npc->get_property(Actor::health));
			out.write2(npc->get_shapefile());

			// Packing for the rest of the structure
			for (size_t j = offsetof(SaveGame_Party, reserved1);
			        j < sizeof(SaveGame_Party); j++)
				out.write1(0);
		}
	}

	{
		// Save Shape
		std::unique_ptr<Shape_file> map = create_mini_screenshot();
		OFileDataSource out(GSCRNSHOT);     // Open file; throws an exception - Don't care
		map->save(&out);
	}

	{
		// Current Exult version
		auto out_stream = U7open_out(GEXULTVER);     // Open file; throws an exception - Don't care
		if (out_stream)
			getVersionInfo(*out_stream);
	}

	// Exult version that started this game
	if (!U7exists(GNEWGAMEVER)) {
		OFileDataSource out(GNEWGAMEVER);
		const string unkver("Unknown");
		out.write(unkver);
	}
}


void Game_window::read_saveinfo(IDataSource *in,
                                SaveGame_Details *&details,
                                SaveGame_Party  *&party) {
	int i;
	details = new SaveGame_Details;

	// This order must match struct SaveGame_Details
	// Time that the game was saved
	details->real_minute = in->read1();
	details->real_hour = in->read1();
	details->real_day = in->read1();
	details->real_month = in->read1();
	details->real_year = in->read2();


	// The Game Time that the save was done at
	details->game_minute = in->read1();
	details->game_hour = in->read1();
	details->game_day = in->read2();

	details->save_count = in->read2();
	details->party_size = in->read1();

	details->unused = in->read1();  // Unused

	details->real_second = in->read1(); // 15

	// Packing for the rest of the structure
	in->skip(sizeof(SaveGame_Details) - offsetof(SaveGame_Details, reserved0));

	party = new SaveGame_Party[details->party_size];
	for (i = 0; i < 8 && i < details->party_size ; i++) {
		in->read(party[i].name, 18);
		party[i].shape = in->read2();

		party[i].exp = in->read4();
		party[i].flags = in->read4();
		party[i].flags2 = in->read4();

		party[i].food = in->read1();
		party[i].str = in->read1();
		party[i].combat = in->read1();
		party[i].dext = in->read1();
		party[i].intel = in->read1();
		party[i].magic = in->read1();
		party[i].mana = in->read1();
		party[i].training = in->read1();

		party[i].health = in->read2();
		party[i].shape_file = in->read2();

		// Packing for the rest of the structure
		in->skip(sizeof(SaveGame_Party) - offsetof(SaveGame_Party, reserved1));
	}
}

bool Game_window::get_saveinfo(int num, char *&name, std::unique_ptr<Shape_file> &map, SaveGame_Details *&details, SaveGame_Party  *&party) {
	char fname[50];         // Set up name.
	snprintf(fname, sizeof(fname), SAVENAME, num,
	         Game::get_game_type() == BLACK_GATE ? "bg" :
	         Game::get_game_type() == SERPENT_ISLE ? "si" : "dev");

	// First check for compressed save game
#ifdef HAVE_ZIP_SUPPORT
	if (get_saveinfo_zip(fname, name, map, details, party))
		return true;
#endif

	IFileDataSource in(fname);
	if (!in.good()) {
		throw file_read_exception(fname);
	}
	// in case of an error.
	// Always try to Read Name
	char buf[0x50];
	memset(buf, 0, sizeof(buf));
	in.read(buf, sizeof(buf) - 1);
	name = new char [strlen(buf) + 1];
	strcpy(name, buf);

	// Isn't a flex, can't actually read it
	if (!Flex::is_flex(&in)) return false;

	// Now get dir info
	in.seek(0x54);          // Get to where file count sits.
	const size_t numfiles = in.read4();
	in.seek(0x80);          // Get to file info.
	// Read pos., length of each file.
	auto finfo = std::make_unique<uint32[]>(2 * numfiles);
	for (size_t i = 0; i < numfiles; i++) {
		finfo[2 * i] = in.read4();  // The position, then the length.
		finfo[2 * i + 1] = in.read4();
	}

	// Always first two entires
	for (size_t i = 0; i < 2; i++) { // Now read each file.
		// Get file length.
		size_t len = finfo[2 * i + 1];
		if (len <= 13)
			continue;
		len -= 13;
		in.seek(finfo[2 * i]);  // Get to it.
		char fname[50];     // Set up name.
		strcpy(fname, GAMEDAT);
		in.read(&fname[sizeof(GAMEDAT) - 1], 13);
		const size_t namelen = strlen(fname);
		// Watch for names ending in '.'.
		if (fname[namelen - 1] == '.')
			fname[namelen - 1] = 0;

		if (!strcmp(fname, GSCRNSHOT)) {
			auto ds = in.makeSource(len);
			map = std::make_unique<Shape_file>(ds.get());
		} else if (!strcmp(fname, GSAVEINFO)) {
			read_saveinfo(&in, details, party);
		}

	}
	return true;
}

void Game_window::get_saveinfo(std::unique_ptr<Shape_file> &map, SaveGame_Details *&details, SaveGame_Party  *&party) {
	{
		IFileDataSource ds(GSAVEINFO);
		if (ds.good()) {
			read_saveinfo(&ds, details, party);
		} else {
			details = nullptr;
			party = nullptr;
		}
	}

	{
		IFileDataSource ds(GSCRNSHOT);
		if (ds.good()) {
			map = std::make_unique<Shape_file>(&ds);
		} else {
			map.reset();
		}
	}
}

// Zip file support
#ifdef HAVE_ZIP_SUPPORT

static const char *remove_dir(const char *fname) {
	const char *base = strchr(fname, '/');// Want the base name.
	if (!base)
		base = strchr(fname, '\\');
	if (base)
		return base + 1;

	return fname;
}


bool Game_window::get_saveinfo_zip(const char *fname, char *&name, std::unique_ptr<Shape_file> &map, SaveGame_Details *&details, SaveGame_Party  *&party) {
	// If a flex, so can't read it
	if (Flex::is_flex(fname)) return false;

	const std::string filestr = get_system_path(fname);
	unzFile unzipfile = unzOpen(filestr.c_str());
	if (!unzipfile) return false;

	// Name comes from comment
	char namebuf[0x50];
	if (unzGetGlobalComment(unzipfile, namebuf, 0x50) <= 0) strncpy(namebuf, "UNNAMED", 0x50);
	name = new char [strlen(namebuf) + 1];
	strcpy(name, namebuf);


	// Things we need
	unz_file_info file_info;
	char *buf = nullptr;

	// Get the screenshot first
	if (unzLocateFile(unzipfile, remove_dir(GSCRNSHOT), 2) == UNZ_OK) {
		unzGetCurrentFileInfo(unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
		buf = new char[file_info.uncompressed_size];

		unzOpenCurrentFile(unzipfile);
		unzReadCurrentFile(unzipfile, buf, file_info.uncompressed_size);
		if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
			IBufferDataSource ds(buf, file_info.uncompressed_size);
			map = std::make_unique<Shape_file>(&ds);
		} else {
			delete [] buf;
		}
	}

	// Now saveinfo
	if (unzLocateFile(unzipfile, remove_dir(GSAVEINFO), 2) == UNZ_OK) {
		unzGetCurrentFileInfo(unzipfile, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
		buf = new char[file_info.uncompressed_size];

		unzOpenCurrentFile(unzipfile);
		unzReadCurrentFile(unzipfile, buf, file_info.uncompressed_size);
		if (unzCloseCurrentFile(unzipfile) == UNZ_OK) {
			IBufferDataSource ds(buf, file_info.uncompressed_size);
			read_saveinfo(&ds, details, party);
		} else {
			delete [] buf;
		}
	}

	unzClose(unzipfile);

	return true;
}


// Level 2 Compression
bool Game_window::Restore_level2(
    void *uzf, const char *dirname, int dirlen
) {
	auto *unzipfile = static_cast<unzFile>(uzf);

	char oname[50];     // Set up name.
	char *oname2 = oname + sizeof(GAMEDAT) + dirlen - 1;
	strcpy(oname, dirname);
	oname2[0] = '/';
	oname2++;

	if (unzOpenCurrentFile(unzipfile) != UNZ_OK) {
		std::cerr << "Couldn't open current file" << std::endl;
		return false;
	}

	while (!unzeof(unzipfile)) {
		// Read Filename
		oname2[12] = 0;
		if (unzReadCurrentFile(unzipfile, oname2, 12) != 12) {
			std::cerr << "Couldn't read for filename" << std::endl;
			return false;
		}

		// Check to see if was are at the end of the list
		if (*oname2 == 0) break;

		// Get file length.
		unsigned char size_buffer[4];
		if (unzReadCurrentFile(unzipfile, size_buffer, 4) != 4) {
			std::cerr << "Couldn't read for size" << std::endl;
			return false;
		}
		const unsigned char *ptr = size_buffer;
		const int size = Read4(ptr);

		if (size) {
			// Watch for names ending in '.'.
			const int namelen = strlen(oname);
			if (oname[namelen - 1] == '.')
				oname[namelen - 1] = 0;


			// Now read the file.
			char *buf = new char[size];
			if (unzReadCurrentFile(unzipfile, buf, size) != size) {
				delete [] buf;
				std::cerr << "Couldn't read for buf" << std::endl;
				return false;
			}

			// Then write it out.
			auto pOut = U7open_out(oname);
			if (!pOut) {
				std::cerr << "couldn't open " << oname<< std::endl;
				return false;
			}
			auto& out = *pOut;
			out.write(buf, size);

			delete [] buf;
			if (!out.good()) {
				std::cerr << "out was bad" << std::endl;
				return false;
			}
			cycle_load_palette();
		}
	}

	return unzCloseCurrentFile(unzipfile) == UNZ_OK;
}


/*
 *  Write out the gamedat directory from a saved game.
 *
 *  Output: Aborts if error.
 */

bool Game_window::restore_gamedat_zip(
    const char *fname       // Name of savegame file.
) {
	// If a flex, so can't read it
	try {
		if (Flex::is_flex(fname)) return false;
	} catch (const file_exception & /*f*/) {
		return false;       // Ignore if not found.
	}
	// Display red plasma during load...
	setup_load_palette();
	const std::string filestr = get_system_path(fname);
	unzFile unzipfile = unzOpen(filestr.c_str());
	if (!unzipfile) return false;

	U7mkdir("<GAMEDAT>", 0755);     // Create dir. if not already there. Don't
	// use GAMEDAT define cause that's got a
	// trailing slash
	U7remove(USEDAT);
	U7remove(USEVARS);
	U7remove(U7NBUF_DAT);
	U7remove(NPC_DAT);
	U7remove(MONSNPCS);
	U7remove(FLAGINIT);
	U7remove(GWINDAT);
	U7remove(IDENTITY);
	U7remove(GSCHEDULE);
	U7remove("<STATIC>/flags.flg");
	U7remove(GSCRNSHOT);
	U7remove(GSAVEINFO);
	U7remove(GNEWGAMEVER);
	U7remove(GEXULTVER);
	U7remove(KEYRINGDAT);
	U7remove(NOTEBOOKXML);

	cout.flush();

	unz_global_info global;
	unzGetGlobalInfo(unzipfile, &global);

	// Now read each file.
	char oname[50];     // Set up name.
	char *oname2 = oname + sizeof(GAMEDAT) - 1;
	strcpy(oname, GAMEDAT);
	bool level2zip = false;

	do {
		unz_file_info   file_info;

		// For safer handling, better do it in two steps.
		unzGetCurrentFileInfo(unzipfile, &file_info,
		                      nullptr, 0,
		                      nullptr, 0,
		                      nullptr, 0);
		// Get the needed buffer size.
		const int filenamelen = file_info.size_filename;
		unzGetCurrentFileInfo(unzipfile, nullptr,
		                      oname2, filenamelen,
		                      nullptr, 0,
		                      nullptr, 0);
		oname2[filenamelen] = 0;

		// Get file length.
		const int len = file_info.uncompressed_size;
		if (len <= 0)
			continue;

		// Level 2 compression handling
		if (level2zip) {
			// Files for map # > 0; create dir first.
			U7mkdir(oname, 0755);
			// Put a final marker in the dir name.
			if (!Restore_level2(unzipfile, oname, filenamelen))
				abort("Error reading level2 from zip '%s'.", fname);
			continue;
		} else if (!std::strcmp("GAMEDAT", oname2)) {
			// Put a final marker in the dir name.
			if (!Restore_level2(unzipfile, oname, 0))
				abort("Error reading level2 from zip '%s'.", fname);
			// Flag that this is a level 2 save.
			level2zip = true;
			continue;
		}

		// Watch for names ending in '.'.
		const int namelen = strlen(oname);
		if (oname[namelen - 1] == '.')
			oname[namelen - 1] = 0;
		// Watch out for multimap games.
		for (size_t i = 0; i < strlen(oname2); i++) {
			//Doing it the right way this time.
			if (oname2[i] == '/') {
				//May need to create a mapxx directory here
				oname2[i] = 0;
				U7mkdir(oname, 0755);
				oname2[i] = '/';
			}
		}

		// Open the file in the zip
		if (unzOpenCurrentFile(unzipfile) != UNZ_OK)
			abort("Error opening current from zipfile '%s'.", fname);

		// Now read the file.
		char *buf = new char[len];
		if (unzReadCurrentFile(unzipfile, buf, len) != len)
			abort("Error reading current from zip '%s'.", fname);

		// now write it out.
		auto pOut = U7open_out(oname);
		if (!pOut)
		    abort("Error opening '%s'.", oname);
		auto& out = *pOut;
		out.write(buf, len);
		if (!out.good()) abort("Error writing to '%s'.", oname);

		// Close the file in the zip
		if (unzCloseCurrentFile(unzipfile) != UNZ_OK)
			abort("Error closing current in zip '%s'.", fname);
		delete [] buf;

		cycle_load_palette();
	} while (unzGoToNextFile(unzipfile) == UNZ_OK);

	unzClose(unzipfile);

	cout.flush();

	load_palette_timer = 0;

	return true;
}


// Level 1 Compression
static bool Save_level1(zipFile zipfile, const char *fname) {
	IFileDataSource ds(fname);
	if (!ds.good()) {
		if (Game::is_editing())
			return false;   // Newly developed game.
		throw file_read_exception(fname);
	}

	const size_t size = ds.getSize();
	const auto buf = ds.readN(size);

	zipOpenNewFileInZip(zipfile, remove_dir(fname), nullptr, nullptr, 0,
	                    nullptr, 0, nullptr, Z_DEFLATED, Z_BEST_COMPRESSION);

	zipWriteInFileInZip(zipfile, buf.get(), size);

	return zipCloseFileInZip(zipfile) == ZIP_OK;
}

// Level 2 Compression
static bool Begin_level2(zipFile zipfile, int mapnum) {
	char oname[8];      // Set up name.
	if (mapnum == 0) {
		strcpy(oname, "GAMEDAT");
		oname[7] = 0;
	} else {
		strcpy(oname, "map");
		constexpr static const char hexLUT[] = "0123456789abcdef";
		oname[3] = hexLUT[mapnum / 16];
		oname[4] = hexLUT[mapnum % 16];
		oname[5] = 0;
	}
	return zipOpenNewFileInZip(zipfile, oname, nullptr, nullptr, 0,
	                           nullptr, 0, nullptr, Z_DEFLATED, Z_BEST_COMPRESSION) == ZIP_OK;
}

static bool Save_level2(zipFile zipfile, const char *fname) {
	IFileDataSource ds(fname);
	if (!ds.good()) {
		if (Game::is_editing())
			return false;   // Newly developed game.
		throw file_read_exception(fname);
	}

	const size_t size = ds.getSize();
	char * const buf = new char[size < 13 ? 13 : size]; // We want at least 13 bytes

	// Filename first
	memset(buf, 0, 13);
	const char *fname2 = strrchr(fname, '/');
	if (!fname2) {
		fname2 = strchr(fname, '\\');
	}
	if (fname2) {
		fname2++;
	} else {
		fname2 = fname;
	}
	strncpy(buf, fname2, 13);
	int err = zipWriteInFileInZip(zipfile, buf, 12);

	// Size of the file
	if (err == ZIP_OK) {
		// Must be platform independant
		auto *ptr = reinterpret_cast<unsigned char*>(buf);
		Write4(ptr, size);
		err = zipWriteInFileInZip(zipfile, buf, 4);
	}

	// Now the actual file
	if (err == ZIP_OK) {
		ds.read(buf, size);
		err = zipWriteInFileInZip(zipfile, buf, size);
	}

	delete [] buf;

	return err == ZIP_OK;
}

static bool End_level2(zipFile zipfile) {
	uint32 zeros = 0;

	// Write a terminator (12 zeros)
	int err = zipWriteInFileInZip(zipfile, &zeros, 4);
	if (err == ZIP_OK) err = zipWriteInFileInZip(zipfile, &zeros, 4);
	if (err == ZIP_OK) err = zipWriteInFileInZip(zipfile, &zeros, 4);

	return zipCloseFileInZip(zipfile) == ZIP_OK && err == ZIP_OK;
}


bool Game_window::save_gamedat_zip(
    const char *fname,          // File to create.
    const char *savename            // User's savegame name.
) {
	char iname[128];
	// If no compression return
	if (save_compression < 1) return false;

	// setup correct file list
	const int numsavefiles = (Game::get_game_type() == BLACK_GATE) ?
	                   bgnumsavefiles : sinumsavefiles;
	const char **savefiles = (Game::get_game_type() == BLACK_GATE) ?
	                         bgsavefiles : sisavefiles;

	// Name
	{
		char title[0x50];
		memset(title, 0, 0x50);
		std::strncpy(title, savename, 0x50);
		auto out = U7open_out(fname);
		if (out)
			out->write(title, 0x50);
	}

	const std::string filestr = get_system_path(fname);
	zipFile zipfile = zipOpen(filestr.c_str(), 1);

	// Level 1 Compression
	if (save_compression != 2) {
		for (int i = 0; i < numsavefiles; i++)
			Save_level1(zipfile, savefiles[i]);

		// Now the Ireg's.
		for (auto *map : maps) {
			if (!map)
				continue;
			for (int schunk = 0; schunk < 12 * 12; schunk++) {
				//Check to see if the ireg exists before trying to
				//save it; prevents crash when creating new maps
				//for existing games
				if (U7exists(map->get_schunk_file_name(U7IREG,
				             schunk, iname)))
					Save_level1(zipfile, iname);
			}
		}
	}
	// Level 2 Compression
	else {
		// Keep saveinfo, screenshot, identity using normal compression
		// There are always files 0 - 2
		Save_level1(zipfile, GSCRNSHOT);
		Save_level1(zipfile, GSAVEINFO);
		Save_level1(zipfile, IDENTITY);

		// Start the GAMEDAT file.
		Begin_level2(zipfile, 0);

		for (int i = 3; i < numsavefiles; i++)
			Save_level2(zipfile, savefiles[i]);

		// Now the Ireg's.
		for (auto *map : maps) {
			if (!map)
				continue;
			if (map->get_num() != 0) {
				// Finish the open file (GAMEDAT or mapXX).
				End_level2(zipfile);
				// Start the next file (mapXX).
				Begin_level2(zipfile, map->get_num());
			}
			for (int schunk = 0; schunk < 12 * 12; schunk++)
				//Check to see if the ireg exists before trying to
				//save it; prevents crash when creating new maps
				//for existing games
				if (U7exists(map->get_schunk_file_name(U7IREG,
				             schunk, iname)))
					Save_level2(zipfile, iname);
		}

		End_level2(zipfile);
	}

	// ++++Better error system needed??
	if (zipClose(zipfile, savename) != ZIP_OK)
		throw file_write_exception(fname);

	return true;
}

#endif
