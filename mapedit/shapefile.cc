/**
 ** Information about a shapes file.
 **
 ** Written: 1/23/02 - JSF
 **/

/*
Copyright (C) 2002-2022 The Exult Team

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
#	include <config.h>
#endif

#include "shapefile.h"

#include "Flex.h"
#include "chunklst.h"
#include "combo.h"
#include "exceptions.h"
#include "ignore_unused_variable_warning.h"
#include "npclst.h"
#include "paledit.h"
#include "shapegroup.h"
#include "shapelst.h"
#include "shapevga.h"
#include "studio.h"
#include "u7drag.h"
#include "utils.h"

#include <algorithm>

using std::cerr;
using std::endl;
using std::ofstream;
using std::string;
using std::unique_ptr;
using std::vector;

/*
 *  Delete file and groups.
 */

Shape_file_info::~Shape_file_info() {
	delete groups;
	delete browser;
}

/*
 *  Get the main browser for this file, or create it.
 *
 *  Output: ->browser.
 */

Object_browser* Shape_file_info::get_browser(
		Shape_file_info* vgafile, unsigned char* palbuf) {
	if (browser) {
		return browser;    // Okay.
	}
	browser = create_browser(vgafile, palbuf, nullptr);
	// Add a reference (us).
	(g_object_ref)(browser->get_widget());
	return browser;
}

/*
 *  Cleanup.
 */

Image_file_info::~Image_file_info() {
	delete ifile;
}

/*
 *  Create a browser for our data.
 */

Object_browser* Image_file_info::create_browser(
		Shape_file_info* vgafile,    // THE 'shapes.vga' file.
		unsigned char*   palbuf,     // Palette for displaying.
		Shape_group*     g           // Group, or 0.
) {
	auto* chooser = new Shape_chooser(ifile, palbuf, 400, 64, g, this);
	// Fonts?  Show 'A' as the default.
	if (strcasecmp(basename.c_str(), "fonts.vga") == 0) {
		chooser->set_framenum0('A');
	}
	if (this == vgafile) {    // Main 'shapes.vga' file?
		chooser->set_shapes_file(
				static_cast<Shapes_vga_file*>(vgafile->get_ifile()));
	}
	return chooser;
}

/*
 *  Write out if modified.  May throw exception.
 */

void Image_file_info::flush() {
	if (!modified) {
		return;
	}
	modified               = false;
	const int      nshapes = ifile->get_num_shapes();
	int            shnum;    // First read all entries.
	vector<Shape*> shapes(nshapes);
	for (shnum = 0; shnum < nshapes; shnum++) {
		shapes[shnum] = ifile->extract_shape(shnum);
	}
	string filestr("<PATCH>/");    // Always write to 'patch'.
	filestr += basename;
	// !flex means single-shape.
	try {
		write_file(filestr.c_str(), shapes.data(), nshapes, !ifile->is_flex());
	} catch (exult_exception& e) {
		ignore_unused_variable_warning(e);
		EStudio::Alert("Error writing '%s'", filestr.c_str());
		return;
	}
	// Tell Exult to reload this file.
	unsigned char  buf[Exult_server::maxlength];
	unsigned char* ptr = &buf[0];
	little_endian::Write2(ptr, ifile->get_u7drag_type());
	ExultStudio* studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::reload_shapes, buf, ptr - buf);
}

/*
 *  Revert to what's on disk.
 *
 *  Output: True (meaning we support 'revert').
 */

bool Image_file_info::revert() {
	if (modified) {
		ifile->load(pathname.c_str());
		modified = false;
	}
	return true;
}

/*
 *  Write a shape file.  (Note:  static method.)
 *  May print an error.
 */

void Image_file_info::write_file(
		const char* pathname,    // Full path.
		Shape**     shapes,      // List of shapes to write.
		int         nshapes,     // # shapes.
		bool        single       // Don't write a FLEX file.
) {
	OFileDataSource out(pathname);    // May throw exception.
	if (single) {
		if (nshapes) {
			shapes[0]->write(out);
		}
		out.flush();
		return;
	}
	Flex_writer writer(out, "Written by ExultStudio", nshapes);
	// Write all out.
	for (int shnum = 0; shnum < nshapes; shnum++) {
		if (shapes[shnum]->get_modified() || shapes[shnum]->get_from_patch()) {
			writer.write_object(shapes[shnum]);
		} else {
			writer.empty_object();
		}
	}
}

/*
 *  Cleanup.
 */

Chunks_file_info::~Chunks_file_info() {}

/*
 *  Create a browser for our data.
 */

Object_browser* Chunks_file_info::create_browser(
		Shape_file_info* vgafile,    // THE 'shapes.vga' file.
		unsigned char*   palbuf,     // Palette for displaying.
		Shape_group*     g           // Group, or 0.
) {
	// Must be 'u7chunks' (for now).
	return new Chunk_chooser(vgafile->get_ifile(), *file, palbuf, 400, 64, g);
}

/*
 *  Write out if modified.  May throw exception.
 */

void Chunks_file_info::flush() {
	if (!modified) {
		return;
	}
	modified = false;
	cerr << "Chunks should be stored by Exult" << endl;
}

/*
 *  Create a browser for our data.
 */

Object_browser* Npcs_file_info::create_browser(
		Shape_file_info* vgafile,    // THE 'shapes.vga' file.
		unsigned char*   palbuf,     // Palette for displaying.
		Shape_group*     g           // Group, or 0.
) {
	return new Npc_chooser(vgafile->get_ifile(), palbuf, 400, 64, g, this);
}

/*
 *  Read in an NPC from Exult.
 */
bool Npcs_file_info::read_npc(unsigned num) {
	if (num > npcs.size()) {
		npcs.resize(num + 1);
	}
	ExultStudio*           studio        = ExultStudio::get_instance();
	int                    server_socket = studio->get_server_socket();
	unsigned char          buf[Exult_server::maxlength];
	Exult_server::Msg_type id;
	unsigned char*         ptr;
	const unsigned char*   newptr;
	newptr = ptr = &buf[0];
	little_endian::Write2(ptr, num);
	if (!studio->send_to_server(Exult_server::npc_info, buf, ptr - buf)
		|| !Exult_server::wait_for_response(server_socket, 100)
		|| Exult_server::Receive_data(server_socket, id, buf, sizeof(buf)) == -1
		|| id != Exult_server::npc_info
		|| little_endian::Read2(newptr) != num) {
		return false;
	}

	npcs[num].shapenum = little_endian::Read2(newptr);    // -1 if unused.
	if (npcs[num].shapenum >= 0) {
		npcs[num].unused = Read1(newptr) != 0;
		const string utf8name(
				convertToUTF8(reinterpret_cast<const char*>(newptr)));
		npcs[num].name = utf8name;
	} else {
		npcs[num].unused = true;
		npcs[num].name   = "";
	}
	return true;
}

/*
 *  Get Exult's list of NPC's.
 */

void Npcs_file_info::setup() {
	modified = false;
	npcs.resize(0);
	ExultStudio* studio        = ExultStudio::get_instance();
	int          server_socket = studio->get_server_socket();
	// Should get immediate answer.
	unsigned char          buf[Exult_server::maxlength];
	Exult_server::Msg_type id;
	int                    num_npcs;
	if (Send_data(server_socket, Exult_server::npc_unused) == -1
		|| !Exult_server::wait_for_response(server_socket, 100)
		|| Exult_server::Receive_data(server_socket, id, buf, sizeof(buf)) == -1
		|| id != Exult_server::npc_unused) {
		cerr << "Error sending data to server." << endl;
		return;
	}
	unsigned char*       ptr;
	const unsigned char* newptr = &buf[0];
	num_npcs                    = little_endian::Read2(newptr);
	// Validate num_npcs to prevent excessive allocation
	if (num_npcs > 1024) {    // Reasonable upper limit
		cerr << "Invalid NPC count received from server: " << num_npcs << endl;
		return;
	}
	npcs.resize(num_npcs);
	for (int i = 0; i < num_npcs; ++i) {
		newptr = ptr = &buf[0];
		little_endian::Write2(ptr, i);
		if (!studio->send_to_server(Exult_server::npc_info, buf, ptr - buf)
			|| !Exult_server::wait_for_response(server_socket, 100)
			|| Exult_server::Receive_data(server_socket, id, buf, sizeof(buf))
					   == -1
			|| id != Exult_server::npc_info
			|| little_endian::Read2(newptr) != i) {
			npcs.resize(0);
			cerr << "Error getting info for NPC #" << i << endl;
			return;
		}
		npcs[i].shapenum = little_endian::Read2(newptr);    // -1 if unused.
		if (npcs[i].shapenum >= 0) {
			npcs[i].unused = Read1(newptr) != 0;
			const string utf8name(
					convertToUTF8(reinterpret_cast<const char*>(newptr)));
			npcs[i].name = utf8name;
		} else {
			npcs[i].unused = true;
			npcs[i].name   = "";
		}
	}
}

/*
 *  Init.
 */

Flex_file_info::Flex_file_info(
		const char*       bnm,    // Basename,
		const char*       pnm,    // Full pathname,
		Flex*             fl,     // Flex file (we'll own it).
		Shape_group_file* g       // Group file (or 0).
		)
		: Shape_file_info(bnm, pnm, g), flex(fl), write_flat(false) {
	entries.resize(flex->number_of_objects());
	lengths.resize(entries.size());
}

/*
 *  Init. for single-palette.
 */

Flex_file_info::Flex_file_info(
		const char* bnm,    // Basename,
		const char* pnm,    // Full pathname,
		unsigned    size    // File size.
		)
		: Shape_file_info(bnm, pnm, nullptr), flex(nullptr), write_flat(true) {
	entries.resize(static_cast<size_t>(size > 0));
	lengths.resize(entries.size());
	if (size > 0) {    // Read in whole thing.
		IFileDataSource in(pnm);
		entries[0] = in.readN(size);
		lengths[0] = size;
	}
}

/*
 *  Cleanup.
 */

Flex_file_info::~Flex_file_info() {
	delete flex;
}

/*
 *  Get i'th entry.
 */

unsigned char* Flex_file_info::get(unsigned i, size_t& len) {
	if (i < entries.size()) {
		if (!entries[i]) {    // Read it if necessary.
			entries[i] = flex->retrieve(i, len);
			lengths[i] = len;
		}
		len = lengths[i];
		return entries[i].get();
	} else {
		return nullptr;
	}
}

/*
 *  Set i'th entry.
 */

void Flex_file_info::set(
		unsigned i,
		unique_ptr<unsigned char[]>
				newentry,    // Allocated data that we'll own.
		int     entlen       // Length.
) {
	if (i > entries.size()) {
		return;
	}
	if (i == entries.size()) {    // Appending?
		entries.push_back(std::move(newentry));
		lengths.push_back(entlen);
	} else {
		entries[i] = std::move(newentry);
		lengths[i] = entlen;
	}
}

/*
 *  Swap the i'th and i+1'th entries.
 */

void Flex_file_info::swap(unsigned i) {
	assert(i < entries.size() - 1);
	std::swap(entries[i], entries[i + 1]);
	std::swap(lengths[i], lengths[i + 1]);
}

/*
 *  Remove i'th entry.
 */

void Flex_file_info::remove(unsigned i) {
	assert(i < entries.size());
	entries.erase(entries.begin() + i);
	lengths.erase(lengths.begin() + i);
}

/*
 *  Create a browser for our data.
 */

Object_browser* Flex_file_info::create_browser(
		Shape_file_info* vgafile,    // THE 'shapes.vga' file.
		unsigned char*   palbuf,     // Palette for displaying.
		Shape_group*     g           // Group, or 0.
) {
	const char* bname = basename.c_str();
	if (strcasecmp(bname, "palettes.flx") == 0
		|| strcasecmp(".pal", bname + strlen(bname) - 4) == 0) {
		return new Palette_edit(this);
	}
	return new Combo_chooser(vgafile->get_ifile(), this, palbuf, 400, 64, g);
}

/*
 *  Write out if modified.  May throw exception.
 */

void Flex_file_info::flush() {
	if (!modified) {
		return;
	}
	modified      = false;
	const int cnt = entries.size();
	size_t    len;
	int       i;
	for (i = 0; i < cnt; i++) {    // Make sure all are read.
		if (!entries[i]) {
			get(i, len);
		}
	}
	string filestr("<PATCH>/");    // Always write to 'patch'.
	filestr += basename;
	OFileDataSource ds(filestr.c_str());    // Throws exception on failure
	if (cnt <= 1 && write_flat) {           // Write flat file.
		if (cnt) {
			ds.write(entries[0].get(), lengths[0]);
		}
		return;
	}
	Flex_writer writer(ds, "Written by ExultStudio", cnt);
	// Write all out.
	for (int i = 0; i < cnt; i++) {
		writer.write_object(entries[i].get(), lengths[i]);
	}
}

/*
 *  Revert to what's on disk.
 *
 *  Output: True (meaning we support 'revert').
 */

bool Flex_file_info::revert() {
	if (!modified) {
		return true;
	}
	modified = false;
	int cnt  = entries.size();
	for (int i = 0; i < cnt; i++) {
		entries[i].reset();
		lengths[i] = 0;
	}
	if (flex) {
		cnt = flex->number_of_objects();
		entries.resize(cnt);
		lengths.resize(entries.size());
	} else {    // Single palette.
		IFileDataSource in(pathname);
		const int       sz = in.getSize();
		cnt                = sz > 0 ? 1 : 0;
		entries.resize(cnt);
		lengths.resize(entries.size());
		if (cnt) {
			entries[0] = in.readN(sz);
			lengths[0] = sz;
		}
	}
	return true;
}

/*
 *  Delete set's entries.
 */

Shape_file_set::~Shape_file_set() {
	for (auto* file : files) {
		delete file;
	}
}

/*
 *  This routines tries to create files that don't yet exist.
 *
 *  Output: true if successful or if we don't need to create it.
 */

static bool Create_file(
		const char*   basename,    // Base file name.
		const string& pathname     // Full name.
) {
	try {
		const int namelen = strlen(basename);
		if (strcasecmp(".flx", basename + namelen - 4) == 0) {
			// We can create an empty flx.
			OFileDataSource   out(pathname.c_str());    // May throw exception.
			const Flex_writer writer(out, "Written by ExultStudio", 0);
			return true;
		} else if (strcasecmp(".pal", basename + namelen - 4) == 0) {
			// Empty 1-palette file.
			U7open_out(pathname.c_str());    // May throw exception.
			return true;
		} else if (strcasecmp("npcs", basename) == 0) {
			return true;    // Don't need file.
		}
	} catch (exult_exception& e) {
		ignore_unused_variable_warning(e);
		EStudio::Alert("Error writing '%s'", pathname.c_str());
	}
	return false;    // Might add more later.
}

/*
 *  Create a new 'Shape_file_info', or return existing one.
 *
 *  Output: ->file info, or 0 if error.
 */

Shape_file_info* Shape_file_set::create(
		const char* basename    // Like 'shapes.vga'.
) {
	// Already have it open?
	for (auto* file : files) {
		if (strcasecmp(file->basename.c_str(), basename) == 0) {
			return file;    // Found it.
		}
	}
	// Look in 'static', 'patch'.
	const string sstr    = string("<STATIC>/") + basename;
	const string pstr    = string("<PATCH>/") + basename;
	const char*  spath   = sstr.c_str();
	const char*  ppath   = pstr.c_str();
	const bool   sexists = U7exists(spath);
	bool         pexists = U7exists(ppath);
	if (!sexists && !pexists) {    // Neither place.  Try to create.
		if (!(pexists = Create_file(basename, ppath))) {
			return nullptr;
		}
	}
	// Use patch file if it exists.
	const char* fullname = pexists ? ppath : spath;
	string      group_name(basename);    // Create groups file.
	group_name += ".grp";
	auto* groups = new Shape_group_file(group_name.c_str());
	if (strcasecmp(basename, "shapes.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname,
				new Shapes_vga_file(spath, U7_SHAPE_SHAPES, ppath), groups));
	} else if (strcasecmp(basename, "gumps.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname, new Vga_file(spath, U7_SHAPE_GUMPS, ppath),
				groups));
	} else if (strcasecmp(basename, "faces.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname, new Vga_file(spath, U7_SHAPE_FACES, ppath),
				groups));
	} else if (strcasecmp(basename, "sprites.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname,
				new Vga_file(spath, U7_SHAPE_SPRITES, ppath), groups));
	} else if (strcasecmp(basename, "paperdol.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname,
				new Vga_file(spath, U7_SHAPE_PAPERDOL, ppath), groups));
	} else if (strcasecmp(basename, "fonts.vga") == 0) {
		return append(new Image_file_info(
				basename, fullname, new Vga_file(spath, U7_SHAPE_FONTS, ppath),
				groups));
	} else if (strcasecmp(basename, "u7chunks") == 0) {
		auto file = U7open_in(fullname);
		return append(new Chunks_file_info(
				basename, fullname, std::move(file), groups));
	} else if (strcasecmp(basename, "npcs") == 0) {
		return append(new Npcs_file_info(basename, fullname, groups));
	} else if (
			strcasecmp(basename, "combos.flx") == 0
			|| strcasecmp(basename, "palettes.flx") == 0) {
		return append(new Flex_file_info(
				basename, fullname, new FlexFile(fullname), groups));
	} else if (strcasecmp(".pal", basename + strlen(basename) - 4) == 0) {
		// Single palette?
		auto pIn = U7open_in(fullname);
		if (!pIn) {
			cerr << "Error opening palette file '" << fullname << "'.\n";
			return nullptr;
		}
		auto& in = *pIn;
		in.seekg(0, std::ios::end);    // Figure size.
		const int sz = in.tellg();
		delete groups;
		return append(new Flex_file_info(basename, fullname, sz));
	} else {    // Not handled above?
		// Get image file for this path.
		auto* ifile = new Vga_file(spath, U7_SHAPE_UNK, ppath);
		if (ifile->is_good()) {
			return append(
					new Image_file_info(basename, fullname, ifile, groups));
		} else {
			delete groups;
		}
		delete ifile;
	}
	cerr << "Error opening image file '" << basename << "'.\n";
	return nullptr;
}

/*
 *  Locates the NPC browser.
 *
 *  Output: ->file info, or nullptr if not found.
 */

Shape_file_info* Shape_file_set::get_npc_browser() {
	for (auto* file : files) {
		if (strcasecmp(file->basename.c_str(), "npcs") == 0) {
			return file;    // Found it.
		}
	}
	return nullptr;    // Doesn't exist yet.
}

/*
 *  Write any modified image files.
 */

void Shape_file_set::flush() {
	for (auto* file : files) {
		file->flush();
	}
}

/*
 *  Any files modified?
 */

bool Shape_file_set::is_modified() {
	return std::any_of(files.cbegin(), files.cend(), [](auto* file) {
		return file->modified;
	});
}
