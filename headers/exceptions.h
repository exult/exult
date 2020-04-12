/*
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

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <cerrno>
#include <stdexcept>
#include <string>
#include <utility>

/*
 *  Base class of all our exceptions, providing a storage for the error message
 */

class   exult_exception : public std::exception {
	std::string  what_;
	int errno_;
public:
	explicit exult_exception(std::string what_arg): what_(std::move(what_arg)), errno_(errno) {  }
	const char *what() const noexcept override {
		return what_.c_str();
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
	explicit quit_exception(int result = 0): exult_exception("Quit"), result_(result) {  }
	int get_result() const {
		return result_;
	}
};

/*
 *  Classes which should not be replicatable throw an replication_exception
 */

class replication_exception : public exult_exception {
public:
	explicit replication_exception(std::string what_arg): exult_exception(std::move(what_arg)) {  }
};

// Some handy macros which you can use to make a class non-replicable
#define UNREPLICATABLE_CLASS(NAME)  NAME(const NAME &) = delete; \
	NAME &operator=(const NAME &) = delete; \
	NAME(NAME &&) = delete; \
	NAME &operator=(NAME &&) = delete;



/*
 *  File errors
 */

class file_exception : public exult_exception {
public:
	explicit file_exception(std::string what_arg): exult_exception(std::move(what_arg)) {  }
};

class   file_open_exception : public file_exception {
	static const std::string  prefix_;
public:
	explicit file_open_exception(const std::string &file): file_exception("Error opening file " + file) {  }
};

class   file_write_exception : public file_exception {
	static const std::string  prefix_;
public:
	explicit file_write_exception(const std::string &file): file_exception("Error writing to file " + file) {  }
};

class   file_read_exception : public file_exception {
	static const std::string  prefix_;
public:
	explicit file_read_exception(const std::string &file): file_exception("Error reading from file " + file) {  }
};

class   wrong_file_type_exception : public file_exception {
public:
	explicit wrong_file_type_exception(const std::string &file, const std::string &type): file_exception("File " + file + " is not of type " + type) {  }
};


/*
 *  Exception that gets fired when the user aborts something
 */
class UserBreakException {
};

#endif
