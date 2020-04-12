/**
 ** Ucclass.h - Usecode compiler classes.
 **
 ** Written: 8/26/06 - JSF
 **/

/*
Copyright (C) 2006-2013 The Exult Team

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdio>
#include <cassert>
#include <vector>
#include <cstring>

#include "ucclass.h"
#include "ucsymtbl.h"
#include "ucfun.h"
#include "ucloc.h"

using std::vector;

int Uc_class::last_num = -1;

/*
 *  Create.
 */

Uc_class::Uc_class(
    char *nm
) : name(nm), scope(nullptr), num_vars(0), base_class(nullptr) {
	num = ++last_num;
}

Uc_class::Uc_class(
    char *nm,
    Uc_class *base
) : name(nm), scope(&base->scope), num_vars(base->num_vars),
	methods(base->methods), base_class(base) {
	num = ++last_num;
	for (vector<Uc_function *>::iterator it = methods.begin();
	        it != methods.end(); ++it)
		(*it)->set_inherited();
}

/*
 *  Add a new class variable.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_class::add_symbol(
    char *nm
) {
	if (scope.is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_class_var_symbol(nm, num_vars++);
	scope.add(var);
	return var;
}

/*
 *  Add a new variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_class::add_symbol(
    char *nm,
    Uc_struct_symbol *s
) {
	if (scope.is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_class_struct_var_symbol(nm, s, num_vars++);
	scope.add(var);
	return var;
}

/*
 *  Add alias to class variable.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_class::add_alias(
    char *nm,
    Uc_var_symbol *var
) {
	if (scope.is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_alias_symbol *alias = new Uc_alias_symbol(nm, var);
	scope.add(alias);
	return alias;
}

/*
 *  Add alias to class variable.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_class::add_alias(
    char *nm,
    Uc_var_symbol *v,
    Uc_struct_symbol *struc
) {
	if (scope.is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = static_cast<Uc_var_symbol *>(v->get_sym());
	Uc_alias_symbol *alias = new Uc_struct_alias_symbol(nm, var, struc);
	scope.add(alias);
	return alias;
}

/*
 *  Add method.
 */

void Uc_class::add_method(
    Uc_function *m
) {
	// If this is a duplicate inherited function,
	// or an externed class method, override it.
	for (vector<Uc_function *>::iterator it = methods.begin();
	        it != methods.end(); ++it) {
		Uc_function *method = *it;
		if (!strcmp(m->get_name(), method->get_name())) {
			if (method->is_inherited() || method->is_externed()) {
				m->set_method_num(method->get_method_num());
				*it = m;
				return;
			} else {
				char buf[150];
				sprintf(buf, "Duplicate decl. of virtual member function '%s'.", m->get_name());
				Uc_location::yyerror(buf);
				return;
			}
		}
	}
	m->set_method_num(methods.size());
	methods.push_back(m);
}

/*
 *  Generate Usecode.
 */

void Uc_class::gen(
    std::ostream &out
) {
	vector<Uc_function *>::iterator it;
	for (it = methods.begin(); it != methods.end(); ++it) {
		Uc_function *m = *it;
		if (m->get_parent() == &scope)
			m->gen(out);    // Generate function if its ours.
	}
}

/*
 *  Create symbol for this function.
 */

Usecode_symbol *Uc_class::create_sym(
) {
	Usecode_class_symbol *cs = new Usecode_class_symbol(name.c_str(),
	        Usecode_symbol::class_scope, num, num_vars);
	vector<Uc_function *>::iterator it;
	for (it = methods.begin(); it != methods.end(); ++it) {
		Uc_function *m = *it;
		cs->add_sym(m->create_sym());
		cs->add_method_num(m->get_usecode_num());
	}
	return cs;
}

bool Uc_class::is_class_compatible(
    const char *nm
) {
	if (name == nm)
		return true;
	else
		return base_class ? base_class->is_class_compatible(nm) : false;
}
