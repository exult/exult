/*
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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <cerrno>
#include <stdexcept>
#include <string>
#include <utility>

/*
 *  Base class of all our exceptions, providing a storage for the error message
 */

class exult_exception : public std::exception {
	std::string what_;
	int         errno_;
	const char* sourcefile_;
	int         line_;

public:
	explicit exult_exception(
			std::string what_arg, const char* sourcefile = nullptr,
			int line = 0)
			: what_(std::move(what_arg)), errno_(errno),
			  sourcefile_(sourcefile), line_(line) {}

	const char* what() const noexcept override {
		return what_.c_str();
	}

	const char* sourcefile() const {
		return sourcefile_;
	}

	int line() const {
		return line_;
	}

	int get_errno() const {
		return errno_;
	}
};

/*
 *  A quit exception can be thrown to quit the program
 */

class quit_exception : public exult_exception {
	int result_;

public:
	explicit quit_exception(int result = 0)
			: exult_exception("Quit"), result_(result) {}

	int get_result() const {
		return result_;
	}
};

// Per analogiam to boost::noncopyable
class nonreplicatable {
protected:
	constexpr nonreplicatable()                        = default;
	nonreplicatable(const nonreplicatable&)            = delete;
	nonreplicatable(nonreplicatable&&)                 = delete;
	nonreplicatable& operator=(const nonreplicatable&) = delete;
	nonreplicatable& operator=(nonreplicatable&&)      = delete;
	~nonreplicatable()                                 = default;
};

/*
 *  File errors
 */

class file_exception : public exult_exception {
public:
	explicit file_exception(
			std::string what_arg, const char* sourcefile, int line)
			: exult_exception(std::move(what_arg), sourcefile, line) {}
};

class file_open_exception : public file_exception {
	static const std::string prefix_;

public:
	explicit file_open_exception(
			const std::string& file, const char* sourcefile, int line)
			: file_exception("Error opening file " + file, sourcefile, line) {}
};

class file_write_exception : public file_exception {
	static const std::string prefix_;

public:
	explicit file_write_exception(
			const std::string& file, const char* sourcefile, int line)
			: file_exception(
					  "Error writing to file " + file, sourcefile, line) {}
};

class file_read_exception : public file_exception {
	static const std::string prefix_;

public:
	explicit file_read_exception(
			const std::string& file, const char* sourcefile, int line)
			: file_exception(
					  "Error reading from file " + file, sourcefile, line) {}
};

class wrong_file_type_exception : public file_exception {
public:
	explicit wrong_file_type_exception(
			const std::string& file, const std::string& type,
			const char* sourcefile, int line)
			: file_exception(
					  "File " + file + " is not of type " + type, sourcefile,
					  line) {}
};

//
// Macros to automatically set Sourcefile and line arguments
//
#define file_exception(what_arg)  file_exception(what_arg, __FILE__, __LINE__)
#define file_open_exception(file) file_open_exception(file, __FILE__, __LINE__)
#define file_write_exception(file) \
	file_write_exception(file, __FILE__, __LINE__)

#define file_read_exception(file) file_read_exception(file, __FILE__, __LINE__)

#define wrong_file_type_exception(file, type) \
	wrong_file_type_exception(file, type, __FILE__, __LINE__)

/*
 *  Exception that gets fired when the user aborts something
 */
class UserBreakException {};

class UserSkipException : public UserBreakException {};

#endif
