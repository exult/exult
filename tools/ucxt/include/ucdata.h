/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#ifndef UCDATA_H
#define UCDATA_H

#include "ucfunc.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

class Usecode_symbol_table;

class UCData {
public:
	UCData();
	~UCData();

	void dump_unknown_opcodes();
	void dump_unknown_intrinsics();

	void parse_params(const unsigned int argc, char** argv);
	void open_usecode(const std::string& filename);
	void load_globals(std::ostream& o);
	void load_funcs(std::ostream& o);
	void analyse_classes();

	void disassamble(std::ostream& o);
	void dump_flags(std::ostream& o);
	void output_extern_header(std::ostream& o);

	std::string output_redirect() const {
		return _output_redirect;
	}

	std::string input_usecode_file() const {
		return _input_usecode_file;
	}

	bool fail() const {
		auto& _file = *_pFile;
		return _file.fail();
	}

	const std::map<unsigned int, UCFuncSet>& funcmap() {
		return _funcmap;
	}

	const UCOptions& opt() {
		return options;
	}

	UCOptions options;

private:
	void file_open(const std::string& filename);

	void file_seek_start() {
		auto& _file = *_pFile;
		_file.seekg(0, std::ios::beg);
	}

	void file_seek_end() {
		auto& _file = *_pFile;
		_file.seekg(0, std::ios::end);
	}

	std::unique_ptr<std::istream> _pFile;

	std::string _output_redirect;
	std::string _input_usecode_file;
	std::string _global_flags_file;

	unsigned int _funcid;

	std::vector<UCc> _codes;

	std::vector<UCFunc*>  _funcs;
	Usecode_symbol_table* _symtbl = nullptr;

	/* Just a quick mapping between funcs and basic data on them.
	   Just something we can quickly pass to the parsing functions
	   so we don't have to give them an entire function to play with. */
	FuncMap _funcmap;

	/* Usecode class inheritance map. */
	using InheritMap = std::map<Usecode_class_symbol*, Usecode_class_symbol*>;
	InheritMap _clsmap;

	long                      _search_opcode    = -1;
	long                      _search_intrinsic = -1;
	std::vector<unsigned int> search_funcs;
};

#endif
