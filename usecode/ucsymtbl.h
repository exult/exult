/*
 *  ucsymtbl.h - Usecode symbol table
 *
 *  Copyright (C) 2006-2022  The Exult Team
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

#ifndef UCSYMTBL_H
#define UCSYMTBL_H

#include "common_types.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#ifdef __clang__
// Working around this clang bug with pragma push/pop:
// https://github.com/clangd/clangd/issues/1167
static_assert(true);
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif
constexpr const uint32 UCSYMTBL_MAGIC0 = 0xffffffffu;
constexpr const uint32 UCSYMTBL_MAGIC1
		= (static_cast<uint32>('U') << 24) + (static_cast<uint32>('C') << 16)
		  + (static_cast<uint32>('S') << 8) + 'Y';
#ifdef __clang__
#	pragma GCC diagnostic pop
#endif

class Usecode_class_symbol;
class Usecode_symbol_table;

class Usecode_symbol {
public:
	enum Symbol_kind {
		fun_defined = 1,
		fun_externed,
		fun_extern_defined,    // External, but fun. # was given.
		class_scope,
		table_scope,
		shape_fun,
		object_fun
	};

private:
	friend class Usecode_symbol_table;
	friend class Usecode_scope_symbol;
	std::string name;
	Symbol_kind kind;
	int         value;    // Function #.
	int         extra;    // Extra symbol info.
public:
	Usecode_symbol(const char* nm, Symbol_kind k, int v, int e = -1)
			: name(nm), kind(k), value(v), extra(e) {}

	virtual ~Usecode_symbol() = default;

	const char* get_name() const {
		return name.c_str();
	}

	Symbol_kind get_kind() const {
		return kind;
	}

	int get_val() const {
		return value;
	}

	int get_extra() const {
		return extra;
	}
};

class Usecode_scope_symbol : public Usecode_symbol {
public:
	using Syms_vector = std::vector<Usecode_symbol*>;

private:
	Syms_vector                        symbols;    // All symbols.
	std::vector<Usecode_class_symbol*> classes;    // Just the classes.
	using Name_table       = std::map<std::string, Usecode_symbol*>;
	using Val_table        = std::map<int, Usecode_symbol*>;
	using Class_name_table = std::map<std::string, Usecode_class_symbol*>;
	using Shape_table      = std::map<int, int>;
	Name_table       by_name;
	Val_table        by_val;
	Class_name_table class_names;
	Shape_table      shape_funs;
	void             setup_by_name(int start = 0);
	void             setup_by_val(int start = 0);
	void             setup_class_names(int start = 0);

public:
	Usecode_scope_symbol(
			const char* nm = "_usecode_", Symbol_kind k = table_scope,
			int v = -1)
			: Usecode_symbol(nm, k, v) {}

	~Usecode_scope_symbol() override;
	void            read(std::istream& in);
	void            write(std::ostream& out);
	void            add_sym(Usecode_symbol* sym);
	Usecode_symbol* operator[](const char* nm);
	Usecode_symbol* operator[](int val);

	Usecode_class_symbol* get_class(int n) {
		return static_cast<unsigned>(n) < classes.size() ? classes[n] : nullptr;
	}

	int get_num_classes() const {
		return static_cast<int>(classes.size());
	}

	Usecode_class_symbol* get_class(const char* nm);
	int                   get_high_shape_fun(int val);
	bool                  is_object_fun(int val);

	const Syms_vector& get_symbols() {
		return symbols;
	}
};

class Usecode_class_symbol : public Usecode_scope_symbol {
	using Ints_vector = std::vector<int>;
	Ints_vector methods;     // List of method usecode #'s.
	int         num_vars;    // # of class variables.
public:
	Usecode_class_symbol(const char* nm, Symbol_kind k, int v, int nvars = 0)
			: Usecode_scope_symbol(nm, k, v), num_vars(nvars) {}

	void add_method_num(int val) {
		methods.push_back(val);
	}

	int get_method_id(int i) {
		return (i >= 0 && static_cast<unsigned>(i) < methods.size())
					   ? methods[i]
					   : -1;
	}

	int get_num_vars() {
		return num_vars;
	}

	int get_num_methods() {
		return static_cast<int>(methods.size());
	}

	void read(std::istream& in);
	void write(std::ostream& out);
};

/*
 *  This class represents a symbol table read in from a Usecode file.  This
 *  is an Exult construct; ie, U7's 'usecode' file doesn't have a symbol-
 *  table.
 */
class Usecode_symbol_table : public Usecode_scope_symbol {
public:
	static bool has_symbol_table(std::istream& in);
};

#endif
