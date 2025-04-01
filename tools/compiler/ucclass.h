/**
 ** Ucclass.h - Usecode compiler classes.
 **
 ** Written: 8/26/06 - JSF
 **/

/*
Copyright (C) 2006-2022 The Exult Team

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

#ifndef INCL_UCCLASS
#define INCL_UCCLASS

#include "ucsym.h"

#include <iosfwd>

class Uc_function;

/*
 *  This represents a usecode class (vaguely like in C++).
 */
class Uc_class : public Uc_design_unit {
	static int                last_num;
	int                       num;    // Unique ID for class.
	std::string               name;
	Uc_scope                  scope;
	int                       num_vars;    // # member variables.
	std::vector<Uc_function*> methods;
	Uc_class*                 base_class;    // For inheritance.
public:
	Uc_class(char* nm);
	Uc_class(char* nm, Uc_class* base);

	Uc_scope* get_scope() {
		return &scope;
	}

	int get_num() {
		return num;
	}

	Uc_var_symbol* add_symbol(char* nm);    // Add class variable.
	Uc_var_symbol* add_symbol(
			char* nm, Uc_struct_symbol* s);    // Add class struct.
	// Add alias to current scope.
	Uc_var_symbol* add_alias(char* nm, Uc_var_symbol* var);
	// Add struct alias to current scope.
	Uc_var_symbol* add_alias(char* nm, Uc_var_symbol* var, Uc_struct_symbol* s);
	void           add_method(Uc_function* m);
	void           gen(std::ostream& out) override;    // Generate Usecode.
	Usecode_symbol* create_sym() override;

	bool is_class() const override {
		return true;
	}

	const char* get_name() const {
		return name.c_str();
	}

	int get_num_vars() const {
		return num_vars;
	}

	bool is_class_compatible(const char* nm);
};

#endif
