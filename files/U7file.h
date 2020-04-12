/*
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef U7FILE_H
#define U7FILE_H

#include <fstream>
#include <string>
#include <utility>
#include "databuf.h"
#include "U7obj.h"
#include "exceptions.h"
#include "common_types.h"

/**
 *  The U7file class is an abstract data object which is
 *  the basis of "file" reading classes for flex, table, iff
 *  and "flat" (single object) file formats.
 */
class U7file {
protected:
	/// This is used to uniquely identify the data object.
	/// For files, this is {"path/filename", -1}.
	/// For objects in multi-object files, this is
	/// {"path/filename", index}.
	File_spec identifier;
	/// Pointer to the DataSource which will be used by
	/// derived classes.
	std::unique_ptr<IDataSource> data;
	/// Causes file/buffer information to be read. Or will do,
	/// when it is implemented for derived classes.
	virtual void index_file() {}
	struct Reference {
		size_t  offset;
		size_t  size;
	};
	virtual Reference get_object_reference(uint32) const {
		return Reference{0, 0};
	}

public:
	/// Initializes from a file spec. Needed by some constructors.
	/// @param spec A file specification.
	explicit U7file(File_spec spec)
		: identifier(std::move(spec)), data(nullptr) {}
	/// Deletes the data source. The destructors of derived classes
	/// should delete anything else that is needed by the datasource
	/// (e.g. buffers) but NOT the datadource.
	virtual ~U7file() noexcept = default;
	U7file(const U7file&) = delete;
	U7file& operator=(const U7file&) = delete;
	U7file(U7file&&) = default;
	U7file& operator=(U7file&&) = default;

	virtual size_t number_of_objects() = 0;
	/**
	 *  Reads the desired object from the file.
	 *  @param objnum   Number of object to read.
	 *  @param len  Receives the length of the object, or zero in any failure.
	 *  @return Buffer created with new[] containing the object data or
	 *  null on any failure.
	 */
	std::unique_ptr<unsigned char[]> retrieve(uint32 objnum, std::size_t &len) {
		if (!data || objnum >= number_of_objects()) {
			len = 0;
			return nullptr;
		}
		Reference ref = get_object_reference(objnum);
		data->seek(ref.offset);
		if (!data->good()) {
			len = 0;
			return nullptr;
		}
		len = ref.size;
		return data->readN(len);
	}

	virtual const char *get_archive_type() = 0;

	/**
	 *  Reads the desired object from the file.
	 *  @param objnum   Number of object to read.
	 *  @return IBufferDataSource wrapping object data. If any error
	 *  has occurred, calling good() on the returned data source will
	 *  return false.
	 */
	IBufferDataSource retrieve(uint32 objnum) {
		std::size_t len;
		auto buffer = retrieve(objnum, len);
		return IBufferDataSource(std::move(buffer), len);
	}
};

/**
 *  This template class implements a basic framework for loading
 *  data from a file through a stream.
 *
 *  The template parameter class T should be derived from U7file.
 */
template <class T>
class U7DataFile final : public T {
public:
	/// This constructor treats the identifier as a file name and
	/// opens the file if it exists. It also creates and initializes
	/// the data source, or sets it to null if the file is not there.
	/// @param spec Name of file to open. Ignores the index portion.
	explicit U7DataFile(const File_spec &spec)
		: T(spec) {
		this->data = std::make_unique<IFileDataSource>(this->identifier.name);
		if (this->data->good()) {
			this->index_file();
		}
	}
};

/**
 *  This template class implements a basic framework for loading
 *  data from a memory buffer. This class TAKES OWNERSHIP of the
 *  buffer and deletes it at the end.
 *
 *  The template parameter class T should be derived from U7file.
 */
template <class T>
class U7DataBuffer final : public T {
public:
	/// Creates and initializes the data source from the data source.
	/// @param spec Unique identifier for this data object.
	/// @param dt   Unique pointer to IExultDataSource that we shoud use.
	U7DataBuffer(const File_spec &spec, std::unique_ptr<IExultDataSource> dt)
		: T(spec) {
		this->data = std::move(dt);
		this->index_file();
	}
	/// Creates and initializes the data source from the buffer.
	/// Takes ownership of the parameters and deletes them when done,
	/// so callers should NOT delete them.
	/// @param spec Unique identifier for this data object.
	/// @param buf  Buffer to read from. The class deletes the buffer
	/// at the end. Can be null if l also is.
	/// @param l    Length of data in the buffer.
	U7DataBuffer(const File_spec &spec, const char *buf, unsigned int l)
		: T(spec) {
		this->data = std::make_unique<IExultDataSource>(buf, l);
		this->index_file();
	}
	/// Creates and initializes the data source from the specified
	/// file/number pair.
	/// @param spec Unique identifier for this data object. The 'name'
	/// member **MUST** be a valid file name.
	explicit U7DataBuffer(const File_spec &spec)
		: T(spec) {
		this->data = std::make_unique<IExultDataSource>(spec.name, spec.index);
		this->index_file();
	}
};

// Not yet.
#if 0
/**
 *  This class stores data from each file.
 */
class File_data {
protected:
	/// The file this refers to. Should NOT be deleted!
	U7file *file;
	/// Whether the file comes from patch dir.
	bool patch;
	/// Number of objects in the file.
	int cnt;

public:
	explicit File_data(const File_spec &spec);
	File_data(const File_data &other) noexcept = default;
	bool from_patch() const {
		return patch;
	}
	size_t number_of_objects() const {
		return cnt;
	}
	char *retrieve(uint32 objnum, std::size_t &len, bool &pt) {
		pt = patch;
		len = 0;
		return file ? file->retrieve(objnum, len) : 0;
	}
	const char *get_archive_type() {
		return file ? file->get_archive_type() : "NONE";
	}
};

/**
 *  Abstract file loader, adapted for loading different resources
 *  from different "files".
 */
class U7multifile {
protected:
	/// This is the list of contained files.
	std::vector<File_data> files;

public:
	U7multifile(const U7multifile&) = delete;
	U7multifile& operator=(const U7multifile&) = delete;
	explicit U7multifile(const File_spec &spec);
	U7multifile(const File_spec &spec0, const File_spec &spec1);
	U7multifile(const File_spec &spec0, const File_spec &spec1,
	            const File_spec &spec2);
	U7multifile(const std::vector<File_spec> &specs);

	size_t number_of_objects() const;
	char *retrieve(uint32 objnum, std::size_t &len, bool &patch) const;
};
#endif

#endif
