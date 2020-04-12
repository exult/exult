/*
 *  ucsymtbl.cc - Usecode symbol table
 *
 *  Copyright (C) 2006  The Exult Team
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

#include <iostream>
#include <cassert>
#include "ucsymtbl.h"
#include "utils.h"

using std::string;
using std::istream;
using std::ostream;

const int curvers = 0;

/*
 *  Cleanup.
 */
Usecode_scope_symbol::~Usecode_scope_symbol() {
	for (Syms_vector::iterator it = symbols.begin(); it != symbols.end();
	        ++it) {
		Usecode_symbol *sym = *it;
		delete sym;
	}
}

/*
 *  Read from a file.
 */
void Usecode_scope_symbol::read(istream &in) {
	int cnt = Read4(in);
	(void)Read4(in);    // Version.
	int oldsize = symbols.size();
	symbols.reserve(oldsize + cnt);
	for (int i = 0; i < cnt; ++i) {
		char nm[256];
		in.getline(nm, sizeof(nm), 0);
		Usecode_symbol::Symbol_kind kind =
		    static_cast<Usecode_symbol::Symbol_kind>(Read2(in));
		int val = Read4(in);
		Usecode_symbol *sym;
		if (kind == Usecode_symbol::class_scope) {
			Usecode_class_symbol *s = new Usecode_class_symbol(nm, kind, val);
			s->read(in);
			assert(static_cast<unsigned>(s->get_val()) == classes.size());
			classes.push_back(s);
			sym = s;
		} else {
			int shape = 0;
			// Shape function table:
			if (kind == Usecode_symbol::shape_fun) {
				shape = Read4(in);
				shape_funs[shape] = val;
			}
			sym = new Usecode_symbol(nm, kind, val, shape);
		}
		symbols.push_back(sym);
	}
	if (!by_name.empty())
		setup_by_name(oldsize);
	if (!by_val.empty())
		setup_by_val(oldsize);
	if (!class_names.empty())
		setup_class_names(oldsize);
}

/*
 *  Write to a file.
 */
void Usecode_scope_symbol::write(ostream &out) {
	Write4(out, symbols.size());
	Write4(out, curvers);
	for (Syms_vector::iterator it = symbols.begin(); it != symbols.end();
	        ++it) {
		Usecode_symbol *sym = *it;
		const char *nm = sym->get_name();
		out.write(nm, strlen(nm) + 1);
		Write2(out, static_cast<int>(sym->get_kind()));
		Write4(out, sym->get_val());
		if (sym->get_kind() == class_scope)
			static_cast<Usecode_class_symbol *>(sym)->write(out);
		else if (sym->get_kind() == shape_fun)
			Write4(out, sym->get_extra());
	}
}

/*
 *  Add a symbol.
 */
void Usecode_scope_symbol::add_sym(Usecode_symbol *sym) {
	int oldsize = symbols.size();
	symbols.push_back(sym);
	if (!by_name.empty())
		setup_by_name(oldsize);
	if (!by_val.empty())
		setup_by_val(oldsize);
}

/*
 *  Setup tables.
 */
void Usecode_scope_symbol::setup_by_name(int start) {
	for (Syms_vector::iterator it = symbols.begin() + start;
	        it != symbols.end(); ++it) {
		Usecode_symbol *sym = *it;
		by_name[sym->name] = sym;
	}
}
void Usecode_scope_symbol::setup_by_val(int start) {
	for (Syms_vector::iterator it = symbols.begin() + start;
	        it != symbols.end(); ++it) {
		Usecode_symbol *sym = *it;
		by_val[sym->value] = sym;
	}
}
void Usecode_scope_symbol::setup_class_names(int start) {
	for (std::vector<Usecode_class_symbol *>::iterator
	        it = classes.begin() + start;
	        it != classes.end(); ++it) {
		Usecode_class_symbol *sym = *it;
		class_names[sym->name] = sym;
	}
}

/*
 *  Lookup by name or by value.
 */
Usecode_symbol *Usecode_scope_symbol::operator[](const char *nm) {
	if (by_name.empty())
		setup_by_name();
	Name_table::iterator it = by_name.find(nm);
	if (it == by_name.end())
		return nullptr;
	else
		return (*it).second;
}

Usecode_symbol *Usecode_scope_symbol::operator[](int val) {
	if (by_val.empty())
		setup_by_val();
	Val_table::iterator it = by_val.find(val);
	if (it == by_val.end())
		return nullptr;
	else
		return (*it).second;
}

Usecode_class_symbol *Usecode_scope_symbol::get_class(const char *nm) {
	if (class_names.empty())
		setup_class_names();
	Class_name_table::iterator it = class_names.find(nm);
	if (it == class_names.end())
		return nullptr;
	else
		return (*it).second;
}

/*
 *  Lookup shape function.
 */
int Usecode_scope_symbol::get_high_shape_fun(int val) {
	if (shape_funs.empty())
		// Default to 'old style' high shape functions.
		return 0x1000 + (val - 0x400);
	Shape_table::iterator it = shape_funs.find(val);
	if (it == shape_funs.end())
		return -1;
	else
		return (*it).second;
}

/*
 *  See if the function requires an itemref.
 */
bool Usecode_scope_symbol::is_object_fun(int val) {
	if (by_val.empty())
		setup_by_val();
	Val_table::iterator it = by_val.find(val);
	// Symbol not found; default to original behavior
	if (it == by_val.end())
		return val < 0x800;
	Usecode_symbol *sym = (*it).second;
	return sym &&
	        (sym->get_kind() == shape_fun || sym->get_kind() == object_fun);
}

/*
 *  Read from a file.
 */
void Usecode_class_symbol::read(istream &in) {
	Usecode_scope_symbol::read(in);
	int num_methods = Read2(in);
	methods.resize(num_methods);
	for (int i = 0; i < num_methods; ++i)
		methods[i] = Read2(in);
	num_vars = Read2(in);
}

/*
 *  Write to a file.
 */
void Usecode_class_symbol::write(ostream &out) {
	Usecode_scope_symbol::write(out);
	int num_methods = methods.size();
	Write2(out, num_methods);
	for (Ints_vector::iterator it = methods.begin(); it != methods.end();
	        ++it)
		Write2(out, *it);
	Write2(out, num_vars);
}

bool Usecode_symbol_table::has_symbol_table(std::istream &in) {
	uint32 magic = Read4(in);   // Test for symbol table.
	if (magic == UCSYMTBL_MAGIC0 && (magic = Read4(in)) == UCSYMTBL_MAGIC1)
		return true;
	else {
		in.seekg(0);
		return false;
	}
}
