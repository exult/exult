/**
 ** Ucsym.h - Usecode compiler symbol table.
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

#ifndef INCL_UCSYM
#define INCL_UCSYM

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ignore_unused_variable_warning.h"
#include "opcodes.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

class Uc_array_expression;
class Uc_expression;
class Uc_function;
class Usecode_symbol;
class Uc_class;
class Uc_struct_symbol;
class Uc_scope;
class Basic_block;

inline bool is_int_32bit(int val) {
	const int high = val >> 16;
	return high != -1 && high != 0;
}

inline bool is_sint_32bit(int val) {
	return val < -32768 || val > 32767;
}

/*
 *  For comparing names:
 */
class String_compare {
public:
	bool operator()(const char* const& x, const char* const& y) const;
};

/*
 *  A formal parameter or local symbol within a function.
 */
class Uc_symbol {
protected:
	std::string name;    // This will be the key.
public:
	enum Sym_types {
		Generic = 0,
		Constant,
		String,
		Variable,
		// Struct,
		Class,        // Class *instances*.
		Member_var    // Class member variable.
					  // Member_struct
	};
	friend class Uc_scope;

	Uc_symbol(const char* nm) : name(nm) {}

	virtual ~Uc_symbol() = default;

	const char* get_name() {
		return name.c_str();
	}

	// Gen. code to put result on stack.
	virtual int gen_value(Basic_block* out);
	// Gen. to assign from stack.
	virtual int gen_assign(Basic_block* out);
	// Generate function/procedure call.
	virtual int gen_call(
			Basic_block* out, Uc_function* fun, bool orig, Uc_expression* item,
			Uc_array_expression* parms, bool retvalue,
			Uc_class* scope_vtbl = nullptr);

	virtual int get_string_offset() {    // Get offset in text_data.
		return -1;
	}

	// Return var/int expression.
	virtual Uc_expression* create_expression();

	virtual Uc_class* get_cls() const {
		return nullptr;
	}

	virtual Uc_struct_symbol* get_struct() const {
		return nullptr;
	}

	virtual bool is_static() const {
		return false;
	}

	virtual int get_sym_type() const {
		return Generic;
	}

	virtual Uc_symbol* get_sym() {
		return this;
	}
};

/*
 *  A variable (untyped) that can be assigned to.
 */
class Uc_var_symbol : public Uc_symbol {
protected:
	int offset;    // Within function.  Locals follow
	//   formal parameters.
	int is_obj_fun;

public:
	friend class Uc_scope;

	Uc_var_symbol(const char* nm, int off, int obj = -1)
			: Uc_symbol(nm), offset(off), is_obj_fun(obj) {}

	int get_offset() {
		return offset;
	}

	void set_offset(int off) {
		offset = off;
	}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	int gen_assign(Basic_block* out) override;
	// Return var/int expression.
	Uc_expression* create_expression() override;
	virtual int    is_object_function(bool error = true) const;

	virtual void set_is_obj_fun(int s) {
		is_obj_fun = s;
	}

	int get_sym_type() const override {
		return Uc_symbol::Variable;
	}
};

/*
 *  A struct variable (weakly typed) that can be assigned to.
 *  Basically, a wrapper around an usecode array.
 */
class Uc_struct_var_symbol : public Uc_var_symbol {
protected:
	Uc_struct_symbol* type;

public:
	friend class Uc_scope;

	Uc_struct_var_symbol(const char* nm, Uc_struct_symbol* t, int off)
			: Uc_var_symbol(nm, off), type(t) {}

	/*
		int get_sym_type() const override
			{ return Uc_symbol::Struct; }
	*/
	Uc_struct_symbol* get_struct() const override {
		return type;
	}
};

/*
 *  A class instance, a typed variable which can be assigned to.
 */
class Uc_class_inst_symbol : public Uc_var_symbol {
protected:
	Uc_class* cls;

public:
	Uc_class_inst_symbol(const char* nm, Uc_class* c, int off)
			: Uc_var_symbol(nm, off), cls(c) {}

	Uc_expression* create_expression() override;

	Uc_class* get_cls() const override {
		return cls;
	}

	int get_sym_type() const override {
		return Uc_symbol::Class;
	}
};

/*
 *  A static (persistent) variable.
 */
class Uc_static_var_symbol : public Uc_var_symbol {
public:
	Uc_static_var_symbol(const char* nm, int off) : Uc_var_symbol(nm, off) {}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	int gen_assign(Basic_block* out) override;

	bool is_static() const override {
		return true;
	}
};

/*
 *  A struct variable (weakly typed) that can be assigned to.
 *  Basically, a wrapper around an usecode array.
 */
class Uc_static_struct_var_symbol : public Uc_static_var_symbol {
protected:
	Uc_struct_symbol* type;

public:
	friend class Uc_scope;

	Uc_static_struct_var_symbol(const char* nm, int off, Uc_struct_symbol* t)
			: Uc_static_var_symbol(nm, off), type(t) {}

	/*
		int get_sym_type() const override
			{ return Uc_symbol::Struct; }
	*/
	Uc_struct_symbol* get_struct() const override {
		return type;
	}
};

/*
 *  A static (persistent) class.
 */
class Uc_static_class_symbol : public Uc_static_var_symbol {
protected:
	Uc_class* cls;

public:
	Uc_static_class_symbol(const char* nm, Uc_class* c, int off)
			: Uc_static_var_symbol(nm, off), cls(c) {}

	Uc_expression* create_expression() override;

	Uc_class* get_cls() const override {
		return cls;
	}

	int get_sym_type() const override {
		return Uc_symbol::Class;
	}
};

/*
 *  A symbol alias.
 */
class Uc_alias_symbol : public Uc_var_symbol {
protected:
	Uc_var_symbol* var;

public:
	friend class Uc_scope;

	Uc_alias_symbol(const char* nm, Uc_var_symbol* v)
			: Uc_var_symbol(nm, v->get_offset()), var(v) {}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override {
		return var->gen_value(out);
	}

	// Gen. to assign from stack.
	int gen_assign(Basic_block* out) override {
		return var->gen_assign(out);
	}

	// Return var/int expression.
	bool is_static() const override {
		return var->is_static();
	}

	int is_object_function(bool error = true) const override {
		ignore_unused_variable_warning(error);
		return var->is_object_function();
	}

	void set_is_obj_fun(int s) override {
		var->set_is_obj_fun(s);
	}

	Uc_symbol* get_sym() override {
		return var;
	}

	int get_sym_type() const override {
		return Uc_symbol::Variable;
	}

	Uc_struct_symbol* get_struct() const override {
		return nullptr;
	}

	Uc_class* get_cls() const override {
		return nullptr;
	}
};

/*
 *  A symbol alias with a struct type.
 */
class Uc_struct_alias_symbol : public Uc_alias_symbol {
protected:
	Uc_struct_symbol* type;

public:
	friend class Uc_scope;

	Uc_struct_alias_symbol(
			const char* nm, Uc_var_symbol* v, Uc_struct_symbol* t)
			: Uc_alias_symbol(nm, v), type(t) {}

	/*
		int get_sym_type() const override
			{ return Uc_symbol::Struct; }
	*/
	Uc_struct_symbol* get_struct() const override {
		return type;
	}
};

/*
 *  A symbol alias with a struct type.
 */
class Uc_class_alias_symbol : public Uc_alias_symbol {
protected:
	Uc_class* cls;

public:
	friend class Uc_scope;

	Uc_class_alias_symbol(const char* nm, Uc_var_symbol* v, Uc_class* c)
			: Uc_alias_symbol(nm, v), cls(c) {}

	int get_sym_type() const override {
		return Uc_symbol::Class;
	}

	Uc_class* get_cls() const override {
		return cls;
	}

	int is_object_function(bool error = true) const override {
		ignore_unused_variable_warning(error);
		return false;
	}

	void set_is_obj_fun(int s) override {
		ignore_unused_variable_warning(s);
	}
};

/*
 *  A class.
 */
class Uc_class_symbol : public Uc_symbol {
	Uc_class* cls;

public:
	Uc_class_symbol(const char* nm, Uc_class* c) : Uc_symbol(nm), cls(c) {}

	static Uc_class_symbol* create(char* nm, Uc_class* c);

	Uc_class* get_cls() const override {
		return cls;
	}
};

/*
 *  This represents a usecode struct (vaguely like in C++).
 *  Performs no checks whatsoever about the size of assigned
 *  usecode variable -- this is left to Exult.
 */
class Uc_struct_symbol : public Uc_symbol {
	using Var_map = std::map<std::string, int>;
	Var_map vars;
	int     num_vars;    // # member variables.
public:
	Uc_struct_symbol(const char* nm) : Uc_symbol(nm), num_vars(0) {}

	~Uc_struct_symbol() override;

	int add(const char* nm) {
		if (is_dup(nm)) {
			return 0;
		}
		// Add struct variable.
		vars[nm] = ++num_vars;
		return num_vars;
	}

	void merge_struct(Uc_struct_symbol* other);

	const char* get_name() const {
		return name.c_str();
	}

	int get_num_vars() const {
		return num_vars;
	}

	int search(const char* nm) {
		auto it = vars.find(nm);
		if (it == vars.end()) {
			return -1;
		} else {
			return it->second;
		}
	}

	bool is_dup(const char* nm);    // Already declared?
};

/*
 *  A class member variable.
 */
class Uc_class_var_symbol : public Uc_var_symbol {
public:
	Uc_class_var_symbol(const char* nm, int off) : Uc_var_symbol(nm, off) {}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	int gen_assign(Basic_block* out) override;

	int get_sym_type() const override {
		return Uc_symbol::Member_var;
	}
};

/*
 *  A class member struct variable.
 */
class Uc_class_struct_var_symbol : public Uc_class_var_symbol {
protected:
	Uc_struct_symbol* type;

public:
	friend class Uc_scope;

	Uc_class_struct_var_symbol(const char* nm, Uc_struct_symbol* t, int off)
			: Uc_class_var_symbol(nm, off), type(t) {}

	// int get_sym_type() const override
	//   { return Uc_symbol::Member_struct; }
	Uc_struct_symbol* get_struct() const override {
		return type;
	}
};

/*
 *  A constant integer variable.
 */
class Uc_const_int_symbol : public Uc_symbol {
	int        value;
	UsecodeOps opcode;

public:
	Uc_const_int_symbol(const char* nm, int v, UsecodeOps op = UC_PUSHI)
			: Uc_symbol(nm), opcode(op) {
		if (opcode == UC_PUSHB) {
			value = static_cast<char>(v & 0xff);
		} else if (opcode == UC_PUSHI) {
			value = static_cast<short>(v & 0xffff);
		} else {
			value = static_cast<int>(v & 0xffffffff);
		}
	}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override;
	// Return var/int expression.
	Uc_expression* create_expression() override;

	int get_value() const {
		return value;
	}

	UsecodeOps get_opcode() const {
		return opcode;
	}

	int get_sym_type() const override {
		return Uc_symbol::Constant;
	}
};

/*
 *  A (constant) string.  The offset is within the usecode function's
 *  text_data field.
 */
class Uc_string_symbol : public Uc_symbol {
	int offset;    // In function's text_data.
public:
	Uc_string_symbol(const char* nm, int off) : Uc_symbol(nm), offset(off) {}

	// Gen. code to put result on stack.
	int gen_value(Basic_block* out) override;

	int get_string_offset() override {    // Get offset in text_data.
		return offset;
	}

	// Return var/int expression.
	Uc_expression* create_expression() override;

	int get_sym_type() const override {
		return Uc_symbol::String;
	}
};

/*
 *  An intrinsic symbol:
 */
class Uc_intrinsic_symbol : public Uc_symbol {
	int intrinsic_num;    // Intrinsic #.
	int num_parms;        // # parms. +++++Not used/set yet.
public:
	Uc_intrinsic_symbol(const char* nm, int n)
			: Uc_symbol(nm), intrinsic_num(n), num_parms(0) {}

	int get_intrinsic_num() {
		return intrinsic_num;
	}

	size_t get_num_parms() {    // ++++Not valid yet.
		return num_parms;
	}

	// Generate function/procedure call.
	int gen_call(
			Basic_block* out, Uc_function* fun, bool orig, Uc_expression* item,
			Uc_array_expression* parms, bool retvalue,
			Uc_class* scope_vtbl = nullptr) override;
};

/*
 *  A function-prototype symbol:
 */
class Uc_function_symbol : public Uc_symbol {
	static int last_num;    // Last 'usecode_num', so we can
	//   assign automatically.
	static bool new_auto_num;    // Prevents autoassigning of
								 //   function numbers if function already
								 //   has a number of its own.
public:
	enum Function_kind {
		utility_fun,
		shape_fun,
		object_fun
	};

	enum Function_ret {
		no_ret = 0,
		var_ret,
		struct_ret,
		class_ret
	};

	union Ret_symbol {
		Uc_struct_symbol* str;
		Uc_class*         cls;
	};

	// Keep track of #'s used.
	using Sym_nums = std::map<int, Uc_function_symbol*>;

private:
	static Sym_nums nums_used;
	// Note:  offset = Usecode fun. #.
	std::vector<Uc_var_symbol*> parms;          // Parameters.
	int                         usecode_num;    // Usecode function #.
	int                         method_num;     // Index with class if a method.
	int                         shape_num;      // Shape # this function is for.
	bool                        externed;
	bool                        inherited;
	Function_ret                ret_type;
	Ret_symbol                  ret_sym;
	bool                        high_id;
	Function_kind               type;

public:
	friend class Uc_scope;
	Uc_function_symbol(
			const char* nm, int num, std::vector<Uc_var_symbol*>& p,
			int shp = 0, Function_kind kind = utility_fun);
	static Uc_function_symbol* create(
			char* nm, int num, std::vector<Uc_var_symbol*>& p,
			bool is_extern = false, Uc_scope* scope = nullptr,
			Function_kind kind = utility_fun);

	const std::vector<Uc_var_symbol*>& get_parms() {
		return parms;
	}

	int get_usecode_num() const {
		return usecode_num;
	}

	bool has_high_id() const {
		return high_id;
	}

	void set_method_num(int n) {
		method_num = n;
	}

	int get_method_num() {
		return method_num;
	}

	int get_shape_num() {
		return shape_num;
	}

	size_t get_num_parms() {
		return parms.size();
	}

	void clear_externed() {
		externed = false;
	}

	void set_externed() {
		externed = true;
	}

	bool is_externed() {
		return externed;
	}

	void set_inherited() {
		inherited = true;
	}

	bool is_inherited() {
		return inherited;
	}

	// Return var/int expression.
	Uc_expression* create_expression() override;
	// Generate function/procedure call.
	int gen_call(
			Basic_block* out, Uc_function* fun, bool orig, Uc_expression* item,
			Uc_array_expression* parms, bool retvalue,
			Uc_class* scope_vtbl = nullptr) override;

	static void set_last_num(int n) {
		last_num     = n;
		new_auto_num = true;
	}

	static Uc_function_symbol* search_num(int ucnum) {
		auto it = nums_used.find(ucnum);
		if (it == nums_used.end()) {    // Unused?  That's good.
			return nullptr;
		}
		return it->second;
	}

	virtual bool has_ret() const {
		return ret_type != no_ret;
	}

	virtual Function_ret get_ret_type() const {
		return ret_type;
	}

	virtual void set_ret_type(bool var) {
		ret_type = var ? var_ret : no_ret;
	}

	virtual void set_ret_type(Uc_class* c) {
		ret_type    = class_ret;
		ret_sym.cls = c;
	}

	virtual void set_ret_type(Uc_struct_symbol* s) {
		ret_type    = struct_ret;
		ret_sym.str = s;
	}

	Uc_class* get_cls() const override {
		return ret_type == class_ret ? ret_sym.cls : nullptr;
	}

	Uc_struct_symbol* get_struct() const override {
		return ret_type == struct_ret ? ret_sym.str : nullptr;
	}

	virtual Function_kind get_function_type() const {
		return type;
	}
};

/*
 *  A 'scope' in the symbol table:
 */
class Uc_scope {
	Uc_scope* parent;    // ->parent.
	// For finding syms. by name.
	using Sym_map = std::map<const char*, Uc_symbol*, String_compare>;
	Sym_map                symbols;
	std::vector<Uc_scope*> scopes;    // Scopes within.
public:
	Uc_scope(Uc_scope* p) : parent(p) {}

	~Uc_scope();

	Uc_scope* get_parent() {
		return parent;
	}

	Uc_symbol* search(const char* nm) {    // Look in this scope.
		auto it = symbols.find(nm);
		if (it == symbols.end()) {
			return nullptr;
		} else {
			return it->second;
		}
	}

	// Search upwards through scopes.
	Uc_symbol* search_up(const char* nm);

	void add(Uc_symbol* sym) {    // Add (does NOT check for dups.)
		symbols[sym->name.c_str()] = sym;
	}

	Uc_scope* add_scope() {    // Create new scope.
		auto* newscope = new Uc_scope(this);
		scopes.push_back(newscope);
		return newscope;
	}

	// Add a function decl.
	int add_function_symbol(
			Uc_function_symbol* fun, Uc_scope* parent = nullptr);
	bool is_dup(const char* nm);    // Already declared?
};

/*
 *  Abstract top-level code (functions, classes).
 */
class Uc_design_unit {
public:
	virtual ~Uc_design_unit()                      = default;
	virtual void            gen(std::ostream& out) = 0;    // Generate Usecode.
	virtual Usecode_symbol* create_sym()           = 0;

	virtual bool is_class() const {
		return false;
	}
};

#endif
