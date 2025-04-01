/**
 ** Ucfun.h - Usecode compiler function.
 **
 ** Written: 1/2/01 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef INCL_UCFUN
#define INCL_UCFUN

#include "opcodes.h"
#include "ucsym.h"

#include <iosfwd>
#include <set>

class Uc_location;
class Uc_statement;

/*
 *  This represents a usecode function:
 */
class Uc_function : public Uc_design_unit {
	static Uc_scope globals;    // For finding intrinsics, funs.
	// Intrinsics, indexed by number:
	static std::vector<Uc_intrinsic_symbol*> intrinsics;
	// Some intrinsic numbers:
	static int add_answer, remove_answer, push_answers, pop_answers, show_face,
			remove_face, get_item_shape, get_usecode_fun;
	static int          num_global_statics;
	Uc_scope            top;            // Top-level scope.
	Uc_function_symbol* proto;          // Function declaration.
	Uc_scope*           cur_scope;      // Current scope.
	int                 num_parms;      // # parameters.
	int                 num_locals;     // Counts locals.
	int                 num_statics;    // Counts local statics.
	// Links to called functions:
	std::vector<Uc_function_symbol*> links;
	std::set<std::string>            labels;
	char*                            text_data;    // All strings.
	int                              text_data_size;
	// Map string to its offset.
	std::map<std::string, int> text_map;
	Uc_statement*              statement;    // Statement(s) in function.
public:
	enum Intrinsic_type {
		unset,
		bg,    // Black gate.
		si,    // Serpent isle.
		sib    // Serpent Isle Beta.
	};

private:
	static Intrinsic_type intrinsic_type;

public:
	Uc_function(Uc_function_symbol* p, Uc_scope* parent = nullptr);
	~Uc_function() override;
	static void set_intrinsics();

	static void setup_intrinsics() {    // Init. the 1st time.
		if (intrinsics.empty()) {
			set_intrinsics();
		}
	}

	static void set_intrinsic_type(Intrinsic_type ty) {
		intrinsic_type = ty;
		if (ty != Uc_function::unset) {
			set_intrinsics();
		}
	}

	void set_statement(Uc_statement* s) {
		statement = s;
	}

	const char* get_name() {
		return proto->get_name();
	}

	bool is_externed() {
		return proto->is_externed();
	}

	bool is_inherited() {
		return proto->is_inherited();
	}

	void set_inherited() {
		proto->set_inherited();
	}

	int get_usecode_num() {
		return proto->get_usecode_num();
	}

	void set_method_num(int n) {
		proto->set_method_num(n);
	}

	int get_method_num() {
		return proto->get_method_num();
	}

	bool has_ret() const {
		return proto->has_ret();
	}

	Uc_function_symbol::Function_ret get_ret_type() const {
		return proto->get_ret_type();
	}

	Uc_class* get_cls() const {
		return proto->get_cls();
	}

	Uc_struct_symbol* get_struct() const {
		return proto->get_struct();
	}

	Uc_function_symbol::Function_kind get_function_type() const {
		return proto->get_function_type();
	}

	Uc_scope* get_parent() {
		return cur_scope->get_parent();
	}

	void push_scope() {    // Start a new scope.
		cur_scope = cur_scope->add_scope();
	}

	void pop_scope() {    // End scope.
		cur_scope = cur_scope->get_parent();
	}

	Uc_symbol* search(const char* nm);
	Uc_symbol* search_up(const char* nm);

	static Uc_symbol* search_globals(const char* nm) {
		return globals.search(nm);
	}

	static Uc_intrinsic_symbol* get_intrinsic(int i) {
		setup_intrinsics();
		return (i >= 0 && static_cast<unsigned>(i) < intrinsics.size())
					   ? intrinsics[i]
					   : nullptr;
	}

	static Uc_intrinsic_symbol* get_add_answer() {
		return get_intrinsic(add_answer);
	}

	static Uc_intrinsic_symbol* get_remove_answer() {
		return get_intrinsic(remove_answer);
	}

	static Uc_intrinsic_symbol* get_push_answers() {
		return get_intrinsic(push_answers);
	}

	static Uc_intrinsic_symbol* get_pop_answers() {
		return get_intrinsic(pop_answers);
	}

	static Uc_intrinsic_symbol* get_show_face() {
		return get_intrinsic(show_face);
	}

	static Uc_intrinsic_symbol* get_remove_face() {
		return get_intrinsic(remove_face);
	}

	static Uc_intrinsic_symbol* get_get_usecode_fun() {
		return get_intrinsic(get_usecode_fun);
	}

	static Uc_intrinsic_symbol* get_get_item_shape() {
		return get_intrinsic(get_item_shape);
	}

	Uc_var_symbol* add_symbol(
			const char* nm, bool bind_offset);    // Add var. to current scope.
	Uc_var_symbol* add_symbol(
			const char* nm, Uc_class* c,
			bool bind_offset);    // Add var. to current scope.
	Uc_var_symbol* add_symbol(
			const char* nm, Uc_struct_symbol* s,
			bool bind_offset);    // Add var. to current scope.
	Uc_var_symbol* add_symbol(
			Uc_var_symbol* var);    // Add var. to current scope.
	// Add alias to current scope.
	Uc_var_symbol* add_alias(char* nm, Uc_var_symbol* var);
	// Add class alias to current scope.
	Uc_var_symbol* add_alias(char* nm, Uc_var_symbol* var, Uc_class* c);
	// Add struct alias to current scope.
	Uc_var_symbol* add_alias(char* nm, Uc_var_symbol* var, Uc_struct_symbol* s);
	void           add_static(char* nm);    // Add static var. to current scope.
	void           add_static(
					  char*             nm,
					  Uc_struct_symbol* type);    // Add static struct to current scope.
	void add_static(
			char* nm, Uc_class* c);    // Add static cls. to current scope.

	int add_function_symbol(
			Uc_function_symbol* fun, Uc_scope* parent = nullptr) {
		return cur_scope->add_function_symbol(fun, parent);
	}

	static int add_global_function_symbol(
			Uc_function_symbol* fun, Uc_scope* parent = nullptr) {
		return globals.add_function_symbol(fun, parent);
	}

	static void add_global_class_symbol(Uc_class_symbol* c) {
		globals.add(c);
	}

	static void add_global_struct_symbol(Uc_struct_symbol* s) {
		globals.add(s);
	}

	// Add string constant.
	Uc_symbol* add_string_symbol(char* nm, char* text);
	// Add int constant.
	Uc_symbol* add_int_const_symbol(char* nm, int value, int opcode = UC_PUSHI);
	static Uc_symbol* add_global_int_const_symbol(
			char* nm, int val, int opcode = UC_PUSHI);
	static void add_global_static(char* nm);
	static void add_global_static(char* nm, Uc_struct_symbol* type);
	static void add_global_static(char* nm, Uc_class* c);
	int         add_string(char* text);
	int         find_string_prefix(Uc_location& loc, const char* text);

	// Start/end loop.
	void add_label(char* nm) {
		labels.insert(nm);
	}

	bool search_label(char* nm) {
		return labels.find(nm) != labels.end();
	}

	int             link(Uc_function_symbol* fun);
	void            gen(std::ostream& out) override;    // Generate Usecode.
	Usecode_symbol* create_sym() override;
};

#endif
