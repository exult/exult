/*
 *  Copyright (C) 2000-2013  The Exult Team
 *
 *  Original file by Dancer A.L Vesperman
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

#ifndef FLEX_H
#define FLEX_H

#include <vector>
#include <string>
#include <cstring>
#include <iosfwd>
#include "common_types.h"
#include "U7file.h"
#include "exceptions.h"

struct Flex_header {
	static constexpr const uint32 FLEX_MAGIC1 = 0xffff1a00U;
	static constexpr const uint32 FLEX_MAGIC2 = 0x000000ccU;
	static constexpr const uint32 EXULT_FLEX_MAGIC2 = 0x0000cc00U;
	static constexpr const uint32 FLEX_TITLE_LEN = 80;
	static constexpr const uint32 FLEX_HEADER_LEN = 128;
	static constexpr const uint32 FLEX_HEADER_PADDING = 9;
	static constexpr const uint32 FLEX_VERSION_MASK = 0xffU;
	/// Exult has an extended flex file format to support
	/// extended shape numbers in IFIX objects.
	enum Flex_vers {
	    orig = 0,       ///<    Original file version.
	    exult_v2 = 1    ///<    Exult extension for IFIX objects.
	};
	char    title[FLEX_TITLE_LEN];
	uint32  magic1;
	uint32  count;
	uint32  magic2;
	uint32  padding[FLEX_HEADER_PADDING];
	Flex_vers get_vers() const {
		return (magic2 & ~FLEX_VERSION_MASK) == EXULT_FLEX_MAGIC2 ?
		       static_cast<Flex_vers>(magic2 & FLEX_VERSION_MASK) : orig;
	}
	bool read(IDataSource *in);
	static void write(ODataSource *out, const char *title, size_t count,
	                  Flex_vers vers = orig);
	static bool is_flex(IDataSource *in);
};
/**
 *  The Flex class is an data reader which reads data in the flex
 *  file format. The actual data need not be in a file, however.
 */
class Flex : public U7file {
public:

protected:
	Flex_header hdr;
	std::vector<Reference> object_list;
	size_t start_pos;

	void index_file() override;
	Reference get_object_reference(uint32 objnum) const override {
		return object_list[objnum];
	}

public:
	/// Basic constructor.
	/// @param spec File name and object index pair.
	explicit Flex(const File_spec &spec)
		: U7file(spec)
	{  }

	/// Inspect the version of a flex file.
	/// @return The flex version.
	Flex_header::Flex_vers get_vers() const {
		return hdr.get_vers();
	}
	/// Gets the number of objects in the flex.
	/// @return Number of objects.
	size_t number_of_objects() override {
		return object_list.size();
	}
	size_t get_entry_info(uint32 objnum, size_t &len);
	const char *get_archive_type() override {
		return "FLEX";
	}
	static bool is_flex(IDataSource *in) {
		return Flex_header::is_flex(in);
	}
	static bool is_flex(const std::string& fname);
	void IndexFlexFile();
};

using FlexFile = U7DataFile<Flex>;
using FlexBuffer = U7DataBuffer<Flex>;

/*
 *  This is for writing out a whole Flex file.
 */
class Flex_writer {
	OStreamDataSource& dout;       // Or this, if non-0.
	const size_t count;           // # entries.
	const size_t start_pos;
	size_t cur_start;         // Start of cur. entry being written.
	std::unique_ptr<uint8[]> table;           // Table of offsets & lengths.
	uint8 *tptr;            // ->into table.
	void finish_object();   // Finished writing out a section.

public:
	Flex_writer(OStreamDataSource& o, const char *title, size_t cnt,
	            Flex_header::Flex_vers vers = Flex_header::orig);
	Flex_writer(const Flex_writer&) noexcept = delete;
	Flex_writer& operator=(const Flex_writer&) noexcept = delete;
	Flex_writer(Flex_writer&&) noexcept = default;
	Flex_writer& operator=(Flex_writer&&) noexcept = delete;
	~Flex_writer();
	std::string base_name(std::string fullname) {
		// Remove trailing (back)slash, if any
		if (fullname.back() == '/' || fullname.back() == '\\') {
			fullname.pop_back();
		}
		// Get actual basename
		auto pos = fullname.find_last_of("/\\");
		if (pos == std::string::npos) {
			return fullname;
		}
		return fullname.substr(pos + 1);
	}
	void write_name(const std::string& fullname) {
		std::string name = base_name(fullname);
		name.resize(8+1+3, 0);	// DOS filename
		dout.write(name);
	}
	void write_object(const File_spec &spec) {
		IFileDataSource ds(spec);
		write_object(ds);
	}
	void write_object(IDataSource& ds) {
		ds.copy_to(dout);
		finish_object();
	}
	void write_object(const void* data, size_t len) {
		dout.write(data, len);
		finish_object();
	}
	template <typename Object, typename... Ts>
	auto write_object(Object& obj, Ts&&... ts) -> decltype(obj.write(dout, std::forward<Ts>(ts)...), std::declval<void>()) {
		obj.write(dout, std::forward<Ts>(ts)...);
		finish_object();
	}
	template <typename Object, typename... Ts>
	auto write_object(Object* obj, Ts&&... ts) -> decltype(obj->write(dout, std::forward<Ts>(ts)...), std::declval<void>()) {
		obj->write(dout, std::forward<Ts>(ts)...);
		finish_object();
	}
	template <typename Writer, typename... Ts>
	auto write_object(Writer&& write, Ts&&... ts) -> decltype(write(dout, std::forward<Ts>(ts)...), std::declval<void>()) {
		std::forward<Writer>(write)(dout, std::forward<Ts>(ts)...);
		finish_object();
	}
	template <typename... Ts>
	void write_file(const File_spec &spec, Ts&&... ts) {
		write_name(spec.name);
		write_object(std::forward<Ts>(ts)...);
	}
	void empty_object() {
		finish_object();
	}
	void flush();
};

#endif
