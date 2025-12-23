/**
 ** Information about a shapes file.
 **
 ** Written: 1/23/02 - JSF
 **/

#ifndef INCL_SHAPEFILE
#define INCL_SHAPEFILE 1

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

#include "ignore_unused_variable_warning.h"

#include <istream>
#include <memory>
#include <string>
#include <vector>

class Vga_file;
class Shape_group_file;
class Object_browser;
class Shape_group;
class Shape;
class Flex;

/*
 *  A shape file:
 */
class Shape_file_info {
protected:
	std::string       basename;    // Base filename.
	std::string       pathname;    // Full pathname.
	Shape_group_file* groups;      // Groups within ifile.
	bool              modified;    // Ifile was modified.
	Object_browser*   browser;     // Widget for seeing this file.
public:
	friend class Shape_file_set;

	// We will own files and groups.
	Shape_file_info(const char* bnm, const char* pnm, Shape_group_file* g)
			: basename(bnm), pathname(pnm), groups(g), modified(false),
			  browser(nullptr) {}

	virtual ~Shape_file_info();

	const char* get_basename() {
		return basename.c_str();
	}

	const char* get_pathname() {
		return pathname.c_str();
	}

	Shape_group_file* get_groups() {
		return groups;
	}

	void set_modified() {
		modified = true;
	}

	virtual Vga_file* get_ifile() {
		return nullptr;
	}

	// Call this to create group browser.
	virtual Object_browser* create_browser(
			Shape_file_info* vgafile, unsigned char* palbuf, Shape_group* g) {
		ignore_unused_variable_warning(vgafile, palbuf, g);
		return nullptr;
	}

	// Call for main browser.
	virtual Object_browser* get_browser(
			Shape_file_info* vgafile, unsigned char* palbuf);

	virtual std::istream* get_file() {
		return nullptr;
	}

	virtual Flex* get_flex() {
		return nullptr;
	}

	virtual void flush() {    // Write if modified.
		modified = false;
	}

	virtual bool revert() {
		return false;    // Means 'not supported'.
	}
};

/*
 *  Image file:
 */
class Image_file_info : public Shape_file_info {
	Vga_file* ifile;    // Contains the images.
public:
	// We will own ifile.
	Image_file_info(
			const char* bnm, const char* pnm, Vga_file* i, Shape_group_file* g)
			: Shape_file_info(bnm, pnm, g), ifile(i) {}

	~Image_file_info() override;

	Vga_file* get_ifile() override {
		return ifile;
	}

	Object_browser* create_browser(
			Shape_file_info* vgafile, unsigned char* palbuf,
			Shape_group* g = nullptr) override;
	void        flush() override;    // Write if modified.
	bool        revert() override;
	static void write_file(
			const char* pathname, Shape** shapes, int nshapes, bool single);
};

/*
 *  Chunks file:
 */
class Chunks_file_info : public Shape_file_info {
	std::unique_ptr<std::istream> file;    // For 'chunks'; ifile is nullptr.
public:
	// We will own file.
	Chunks_file_info(
			const char* bnm, const char* pnm, std::unique_ptr<std::istream> f,
			Shape_group_file* g)
			: Shape_file_info(bnm, pnm, g), file(std::move(f)) {}

	~Chunks_file_info() override;

	std::istream* get_file() override {
		return file.get();
	}

	Object_browser* create_browser(
			Shape_file_info* vgafile, unsigned char* palbuf,
			Shape_group* g = nullptr) override;
	void flush() override;    // Write if modified.
};

/*
 *  Info. needed for each NPC:
 */
class Estudio_npc {
	friend class Npc_chooser;
	friend class Npcs_file_info;
	short       shapenum = 0;
	bool        unused   = false;
	std::string name;

public:
};

/*
 *  NPC's aren't really in a file.  The info. comes from Exult.
 */
class Npcs_file_info : public Shape_file_info {
	std::vector<Estudio_npc> npcs;    // Shared NPC info.
public:
	// We will own file.
	Npcs_file_info(const char* bnm, const char* pnm, Shape_group_file* g)
			: Shape_file_info(bnm, pnm, g) {
		setup();
	}

	Object_browser* create_browser(
			Shape_file_info* vgafile, unsigned char* palbuf,
			Shape_group* g = nullptr) override;

	std::vector<Estudio_npc>& get_npcs() {
		return npcs;
	}

	void setup();
	bool read_npc(unsigned num);
};

/*
 *  Flex file (used for combos, palettes):
 */
class Flex_file_info : public Shape_file_info {
	Flex* flex;    // nullptr if just 1 entry.
	std::vector<std::unique_ptr<unsigned char[]>>
					 entries;       // Entries stored here.
	std::vector<int> lengths;       // Lengths here.
	bool             write_flat;    // Write flat file if just 1 entry.
public:
	// We will own flex.
	Flex_file_info(
			const char* bnm, const char* pnm, Flex* fl, Shape_group_file* g);
	// Create for single-palette.
	Flex_file_info(const char* bnm, const char* pnm, unsigned size);

	unsigned size() {    // Get # flex entries.
		return entries.size();
	}

	unsigned char* get(unsigned i, size_t& len);    // Get i'th entry.
	// Set i'th entry.
	void set(unsigned i, std::unique_ptr<unsigned char[]> newentry, int entlen);
	void swap(unsigned i);      // Swap entries i, i+1.
	void remove(unsigned i);    // Remove i'th entry.
	~Flex_file_info() override;
	Object_browser* create_browser(
			Shape_file_info* vgafile, unsigned char* palbuf,
			Shape_group* g = nullptr) override;

	Flex* get_flex() override {
		return flex;
	}

	void flush() override;    // Write if modified.
	bool revert() override;
};

/*
 *  A set of Shape_file's.
 */
class Shape_file_set {
	std::vector<Shape_file_info*> files;

	Shape_file_info* append(Shape_file_info* fi) {
		files.push_back(fi);
		return fi;
	}

public:
	~Shape_file_set();
	// Create, or return existing one.
	Shape_file_info* create(const char* basename);
	Shape_file_info* get_npc_browser();

	unsigned size() {
		return files.size();
	}

	Shape_file_info* operator[](int i) {
		return files[i];
	}

	void flush();          // Write if modified.
	bool is_modified();    // Any modified?
};

#endif
