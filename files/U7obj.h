/*
 *  U7obj.h - Generic file reader object.
 *
 *  Copyright (C) 2008  The Exult Team
 *
 *  Based on code by Dancer A.L Vesperman
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

#ifndef U7OBJ_H
#define U7OBJ_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common_types.h"

/**
 *  This structure condenses a file name and an object number in
 *  one place. It is meant to unify opening files and memory buffers.
 */
struct File_spec {
	/// This is a file name, always.
	std::string name;
	/// If -1, this indicates that we are reading from the file itself.
	/// If >= 0, this means we want to read from the object with the
	/// given index inside the file.
	int index{-1};
	File_spec() = default;
	/// Constructs a File_spec from a c-string.
	File_spec(const char *n, int i = -1)
		: name(n), index(i) {}
	/// Constructs a File_spec from a string.
	File_spec(std::string n, int i = -1)
		: name(std::move(n)), index(i) {}
	bool operator<(const File_spec &other) const {
		int cmp = name.compare(other.name);
		return cmp < 0 || (cmp == 0 && index < other.index);
	}
};

/**
 *  This class loads a file of undefined type and retrieves
 *  a given object contained in said file.
 */
class U7object {
protected:
	/// Name of desired file.
	File_spec identifier;
	/// Number of object to retrieve.
	int objnumber;

public:
	/// Setups to load an object from a buffer or file.
	/// @param spec Specification of the data source.
	/// @param objnum   Index of object in the data object.
	/// @param p    Optional, defaults to false. If true, indicates
	/// the object comes from the patch dir.
	U7object(File_spec spec, int objnum)
		: identifier(std::move(spec)), objnumber(objnum)
	{  }
	virtual ~U7object() noexcept = default;
	U7object(const U7object&) = delete;
	U7object& operator=(const U7object&) = delete;
	U7object(U7object&&) = default;
	U7object& operator=(U7object&&) = default;

	File_spec get_identifier() const {
		return identifier;
	}
	// TODO: This may need to be overriden by U7multiobject, in case the
	// patching files have more objects than the base.
	size_t number_of_objects();
	virtual std::unique_ptr<unsigned char[]> retrieve(std::size_t &len) const;
};

/**
 *  This class loads an object from several data sources, going
 *  from last to first source when determining the correct object.
 *  The intended use is to reduce code duplication for something
 *  which should be patchable but is not.
 *  To make it clear: it is a multi-source, EXCLUSIVE object loader;
 *  it does NOT "merge" the objects from the different sources,
 *  it picks a single source from which the object is loaded.
 *  An 'object' it loads may be an object in a file or an object
 *  in a buffer.
 */
class U7multiobject : public U7object {
private:
	std::unique_ptr<unsigned char[]> buffer;
	size_t length;
	void set_object(const std::vector<U7object> &objects);

public:
	U7multiobject(const File_spec &file0, int objnum);
	U7multiobject(const File_spec &file0, const File_spec &file1, int objnum);
	U7multiobject(const File_spec &file0, const File_spec &file1,
	              const File_spec &file2, int objnum);
	U7multiobject(const File_spec &file0, const File_spec &file1,
	              const File_spec &file2, const File_spec &file3, int objnum);
	U7multiobject(const std::vector<File_spec> &files, int objnum);
	~U7multiobject() noexcept final = default;
	U7multiobject(const U7multiobject&) = delete;
	U7multiobject& operator=(const U7multiobject&) = delete;
	U7multiobject(U7multiobject&&) = default;
	U7multiobject& operator=(U7multiobject&&) = default;

	std::unique_ptr<unsigned char[]> retrieve(std::size_t &len) const final;
};

#endif
