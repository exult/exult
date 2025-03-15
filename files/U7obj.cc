/*
 *  U7obj.cc - Generic file reader object.
 *
 *  Copyright (C) 2008-2022  The Exult Team
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

#include "U7obj.h"

#include "U7file.h"
#include "U7fileman.h"

#include <algorithm>
#include <vector>

using std::make_unique;
using std::unique_ptr;

/**
 *  Uses U7FileManager to get an U7file for the desired file.
 *  @return The number of objects contained in the file or zero
 *  if any failure happened.
 */
size_t U7object::number_of_objects() {
	U7file* uf = U7FileManager::get_ptr()->get_file_object(identifier, true);
	return (uf != nullptr) ? uf->number_of_objects() : 0UL;
}

/**
 *  Uses U7FileManager to get an U7file for the desired file.
 *  @param len  Receives the size of desired object, if it exists
 *  in the file, or zero on any failure.
 *  @return Buffer created with new[] containing the contents of
 *  the object or null on any failure.
 */
unique_ptr<unsigned char[]> U7object::retrieve(size_t& len) const {
	U7file* uf = U7FileManager::get_ptr()->get_file_object(identifier, true);
	len        = 0;
	return (uf != nullptr) ? uf->retrieve(objnumber, len) : nullptr;
}

/**
 *  Finds out which file/buffer we will be using. Finds the first
 *  non-empty object in the reverse order in which the constructor
 *  parameters are entered.
 *  Pre-caches the buffer to return, sets the correct length and
 *  U7object information.
 *  @param objects  Vector containing U7objects we will test.
 */
void U7multiobject::set_object(tcb::span<U7object> objects) {
	for (const auto& obj : objects) {
		size_t len;
		auto   buf = obj.retrieve(len);
		// Only len > 0 means a valid object.
		if (buf && len > 0) {
			buffer = std::move(buf);    // Gets deleted with class.
			length = len;
			object.set_identifier(obj.get_identifier());
			break;
		}
	}
}

/**
 *  Single object constructor. Why bother?
 *  @param file0    Specification for first file/buffer.
 *  @param objnum   Object number we are looking for.
 */
U7multiobject::U7multiobject(const File_spec& file0, int objnum)
		: buffer(nullptr), length(0), object(file0, objnum) {
	size_t len;
	auto   buf = object.retrieve(len);
	// Only len > 0 means a valid object.
	if (buf && len > 0) {
		buffer = std::move(buf);    // Gets deleted with class.
		length = len;
	}
}

/**
 *  Two object constructor.
 *  @param file0    Specification for first file/buffer; usually <STATIC>.
 *  @param file1    Specification for second file/buffer; maybe <PATCH>.
 *  @param objnum   Object number we are looking for.
 */
U7multiobject::U7multiobject(
		const File_spec& file0, const File_spec& file1, int objnum)
		: buffer(nullptr), length(0), object(file0, objnum) {
	std::array objects{U7object(file1, objnum), U7object(file0, objnum)};
	set_object(objects);
}

/**
 *  Three object constructor.
 *  @param file0    Specification for first file/buffer; usually <STATIC>.
 *  @param file1    Specification for second file/buffer.
 *  @param file2    Specification for second file/buffer; maybe <PATCH>.
 *  @param objnum   Object number we are looking for.
 */
U7multiobject::U7multiobject(
		const File_spec& file0, const File_spec& file1, const File_spec& file2,
		int objnum)
		: buffer(nullptr), length(0), object(file0, objnum) {
	std::array objects{
			U7object(file2, objnum), U7object(file1, objnum),
			U7object(file0, objnum)};
	set_object(objects);
}

/**
 *  Four object constructor. Don't think we need those many, but...
 *  @param file0    Specification for first file/buffer; usually <STATIC>.
 *  @param file1    Specification for second file/buffer.
 *  @param file2    Specification for second file/buffer.
 *  @param file3    Specification for second file/buffer; maybe <PATCH>.
 *  @param objnum   Object number we are looking for.
 */
U7multiobject::U7multiobject(
		const File_spec& file0, const File_spec& file1, const File_spec& file2,
		const File_spec& file3, int objnum)
		: buffer(nullptr), length(0), object(file0, objnum) {
	std::array objects{
			U7object(file3, objnum), U7object(file2, objnum),
			U7object(file1, objnum), U7object(file0, objnum)};
	set_object(objects);
}

/**
 *  Overkill.
 *  @param files    Vector with all the file specifications.
 *  @param objnum   Object number we are looking for.
 */
U7multiobject::U7multiobject(const std::vector<File_spec>& files, int objnum)
		: buffer(nullptr), length(0), object("", objnum) {
	if (!files.empty()) {
		object.set_identifier(files[0]);
		std::vector<U7object> objects;
		objects.reserve(files.size());
		for (const auto& file : files) {
			objects.emplace_back(file, objnum);
		}
		set_object(objects);
	}
}

U7multiobject::U7multiobject(const U7multiobject& other)
		: buffer(nullptr), length(0), object(other.object) {
	buffer = make_unique<unsigned char[]>(other.length);
	std::copy_n(other.buffer.get(), other.length, buffer.get());
}

U7multiobject& U7multiobject::operator=(const U7multiobject& rhs) {
	U7multiobject tmp(rhs);
	std::swap(*this, tmp);
	return *this;
}

/**
 *  Uses U7FileManager to get an U7file for the desired file.
 *  @param len  Receives the size of desired object, if it exists
 *  in the file, or zero on any failure.
 *  @return Buffer created with new[] containing the contents of
 *  the object or null on any failure.
 */
unique_ptr<unsigned char[]> U7multiobject::retrieve(
		size_t& len, bool nullterminate) const {
	len = length;
	if (length == 0) {
		// This means we didn't find the object on construction.
		return nullptr;
	}
	auto buf = make_unique<unsigned char[]>(len + (nullterminate ? 1 : 0));
	std::copy_n(buffer.get(), len, buf.get());
	if (nullterminate) {
		buf[len] = 0;
	}
	return buf;
}
