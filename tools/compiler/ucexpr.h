/**
 ** Ucexpr.h - Expressions for Usecode compiler.
 **
 ** Written: 1/0/01 - JSF
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

#ifndef INCL_UCEXPR
#define INCL_UCEXPR 1

#include "ignore_unused_variable_warning.h"
#include "opcodes.h"
#include "ucloc.h"

#include <string>
#include <vector>

class Uc_symbol;
class Uc_var_symbol;
class Uc_class_inst_symbol;
class Uc_struct_symbol;
class Uc_function;
class Uc_function_symbol;
class Uc_class;
class Basic_block;
class Uc_array_expression;

/*
 *  Base class for expressions.
 */
class Uc_expression : public Uc_location {
public:
	// Use current location.
	Uc_expression()          = default;
	virtual ~Uc_expression() = default;
	// Gen. code to put result on stack.
	virtual void gen_value(Basic_block* out) = 0;
	// Gen code to push value(s).
	virtual int gen_values(Basic_block* out);
	// Gen. to assign from stack.
	virtual void gen_assign(Basic_block* out);

	virtual int get_string_offset() {    // Get offset in text_data.
		return -1;
	}

	// Get/create var == this.
	virtual Uc_var_symbol* need_var(Basic_block* out, Uc_function* fun);
	// Evaluate constant.
	virtual bool eval_const(int& val);

	virtual bool is_class() const {
		return false;
	}

	virtual bool is_struct() const {
		return false;
	}

	virtual int get_type() const {
		return 0;
	}

	virtual Uc_var_symbol* get_var() const {
		return nullptr;
	}

	virtual Uc_class* get_cls() const {
		return nullptr;
	}

	virtual Uc_struct_symbol* get_struct() const {
		return nullptr;
	}

	virtual int is_object_function(bool error = true) const {
		ignore_unused_variable_warning(error);
		return -1;
	}

	virtual void set_is_obj_fun(int s) {
		ignore_unused_variable_warning(s);
	}

	virtual void add_to(Uc_array_expression* arr);
};

/*
 *  A variable.
 */
class Uc_var_expression : public Uc_expression {
protected:
	Uc_var_symbol* var;

public:
	// Use current location.
	Uc_var_expression(Uc_var_symbol* v) : var(v) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
	int  get_string_offset() override;    // Get offset in text_data.

	Uc_var_symbol* need_var(Basic_block*, Uc_function*) override {
		return var;
	}

	Uc_var_symbol* get_var() const override {
		return var;
	}

	bool              is_struct() const override;
	Uc_struct_symbol* get_struct() const override;
	int               is_object_function(bool error = true) const override;
	void              set_is_obj_fun(int s) override;
	int               get_type() const override;
};

/*
 *  A function name, as used in a script.
 */
class Uc_fun_name_expression : public Uc_expression {
	Uc_function_symbol* fun;

public:
	// Use current location.
	Uc_fun_name_expression(Uc_function_symbol* f) : fun(f) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	int  is_object_function(bool error = true) const override;
	bool eval_const(int& val) override;
};

/*
 *  An array element.
 */
class Uc_arrayelem_expression : public Uc_expression {
protected:
	Uc_var_symbol* array;
	Uc_expression* index;

public:
	Uc_arrayelem_expression(Uc_var_symbol* a, Uc_expression* i)
			: array(a), index(i) {}

	~Uc_arrayelem_expression() override {
		delete index;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
};

/*
 *  An array element of an static array.
 */
class Uc_static_arrayelem_expression : public Uc_arrayelem_expression {
public:
	Uc_static_arrayelem_expression(Uc_var_symbol* a, Uc_expression* i)
			: Uc_arrayelem_expression(a, i) {}

	~Uc_static_arrayelem_expression() override {
		delete index;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
};

/*
 *  An array element of a class member array.
 */
class Uc_class_arrayelem_expression : public Uc_arrayelem_expression {
public:
	Uc_class_arrayelem_expression(Uc_var_symbol* a, Uc_expression* i)
			: Uc_arrayelem_expression(a, i) {}

	~Uc_class_arrayelem_expression() override {
		delete index;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
};

/*
 *  Global flag.
 */
class Uc_flag_expression : public Uc_expression {
	Uc_expression* flag;

public:
	Uc_flag_expression(Uc_expression* f) : flag(f) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
};

/*
 *  Binary expressions.
 */
class Uc_binary_expression : public Uc_expression {
	UsecodeOps     opcode;          // Should be the UC_<opcode>
	Uc_expression *left, *right;    // Operands to add, sub, etc.
	UsecodeOps intop;    // If we want to use pushb or pushi32 instead of pushi.
public:
	Uc_binary_expression(
			UsecodeOps o, Uc_expression* l, Uc_expression* r,
			UsecodeOps iop = UC_PUSHI)
			: opcode(o), left(l), right(r), intop(iop) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Evaluate constant.
	bool eval_const(int& val) override;
};

/*
 *  Unary expressions.
 */
class Uc_unary_expression : public Uc_expression {
	UsecodeOps     opcode;    // Should be the UC_<opcode>
	Uc_expression* operand;

public:
	Uc_unary_expression(UsecodeOps o, Uc_expression* r)
			: opcode(o), operand(r) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	bool eval_const(int& val) override;
};

/*
 *  Integer value.
 */
class Uc_int_expression : public Uc_expression {
	int        value;
	UsecodeOps opcode;

public:
	Uc_int_expression(int v, UsecodeOps op = UC_PUSHI) : opcode(op) {
		if (opcode == UC_PUSHB) {
			value = static_cast<char>(v & 0xff);
		} else if (opcode == UC_PUSHI) {
			value = static_cast<short>(v & 0xffff);
		} else {
			value = static_cast<int>(v & 0xffffffff);
		}
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Evaluate constant.
	bool eval_const(int& val) override;
	int  is_object_function(bool error = true) const override;
};

/*
 *  Boolean value.
 */
class Uc_bool_expression : public Uc_expression {
	bool tf;

public:
	Uc_bool_expression(bool t) : tf(t) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;

	bool eval_const(int& val) override {
		val = static_cast<int>(tf);
		return true;
	}
};

/*
 *  Eventid (a special int variable passed to each function):
 */
class Uc_event_expression : public Uc_expression {
public:
	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen. to assign from stack.
	void gen_assign(Basic_block* out) override;
};

/*
 *  Item (a special ptr. variable passed to each function):
 */
class Uc_item_expression : public Uc_expression {
public:
	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
};

/*
 *  String value.
 */
class Uc_string_expression : public Uc_expression {
	int offset;    // Offset in function's data area.
public:
	Uc_string_expression(int o) : offset(o) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;

	int get_string_offset() override {    // Get offset in text_data.
		return offset;
	}
};

/*
 *  String value given by a prefix (i.e. "Jo"* for "Job").
 */
class Uc_string_prefix_expression : public Uc_expression {
	Uc_function* fun;       // Needed to look up prefix.
	std::string  prefix;    // What to look up.
	int          offset;    // Offset in function's data area.
							//   This is -1 if not found yet.
public:
	Uc_string_prefix_expression(Uc_function* f, char* pre)
			: fun(f), prefix(pre), offset(-1) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	int  get_string_offset() override;    // Get offset in text_data.
};

/*
 *  Last selected user choice.
 */
class Uc_choice_expression : public Uc_expression {
public:
	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
};

/*
 *  A concatenation, which generates an array:
 */
class Uc_array_expression : public Uc_expression {
	std::vector<Uc_expression*> exprs;

public:
	Uc_array_expression() = default;

	Uc_array_expression(Uc_expression* e0) {
		add(e0);    // Create with 1st expression.
	}

	Uc_array_expression(Uc_expression* e0, Uc_expression* e1) {
		add(e0);
		add(e1);
	}

	~Uc_array_expression() override;

	void add(Uc_expression* e) {    // Append an expression.
		exprs.push_back(e);
	}

	void add(std::vector<Uc_expression*>& vec) {    // Append an expression.
		exprs.insert(exprs.cend(), vec.begin(), vec.end());
	}

	void clear() {    // Remove, but DON'T delete, elems.
		exprs.clear();
	}

	void concat(Uc_expression* e);    // Concat e's elements onto this.

	const std::vector<Uc_expression*>& get_exprs() {
		return exprs;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
	// Gen code to push value(s).
	int  gen_values(Basic_block* out) override;
	void add_to(Uc_array_expression* arr) override;
};

/*
 *  A function or intrinsic call.
 */
class Uc_call_expression : public Uc_expression {
	Uc_symbol*     sym;         // Function or intrinsic.
	Uc_expression* ind;         // For indirect call (sym == nullptr).
	bool           original;    // Call original function instead of
	//   the one from 'patch'.
	Uc_expression*       itemref;    // Non-null for CALLE.
	Uc_array_expression* parms;
	Uc_function*         function;        // May need function this is in.
	bool                 return_value;    // True for a function (to return
	//   its value).
	Uc_class* meth_scope;

public:
	Uc_call_expression(
			Uc_symbol* s, Uc_array_expression* prms, Uc_function* fun,
			bool orig = false)
			: sym(s), ind(nullptr), original(orig), itemref(nullptr),
			  parms(prms), function(fun), return_value(true),
			  meth_scope(nullptr) {}

	Uc_call_expression(
			Uc_expression* i, Uc_array_expression* prms, Uc_function* fun)
			: sym(nullptr), ind(i), original(false), itemref(nullptr),
			  parms(prms), function(fun), return_value(true),
			  meth_scope(nullptr) {}

	~Uc_call_expression() override {
		delete parms;
		delete itemref;
	}

	void set_itemref(Uc_expression* iexpr) {
		itemref = iexpr;
	}

	void set_no_return() {
		return_value = false;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;

	void set_call_scope(Uc_class* s) {
		meth_scope = s;
	}

	void              check_params();
	bool              is_class() const override;
	Uc_class*         get_cls() const override;
	bool              is_struct() const override;
	Uc_struct_symbol* get_struct() const override;
	int               is_object_function(bool error = true) const override;
};

class Uc_class_expression : public Uc_var_expression {
public:
	Uc_class_expression(Uc_var_symbol* v) : Uc_var_expression(v) {}

	void gen_value(Basic_block* out) override;
	void gen_assign(Basic_block* out) override;

	Uc_var_symbol* need_var(Basic_block*, Uc_function*) override {
		return nullptr;
	}

	bool is_class() const override {
		return true;
	}

	Uc_class* get_cls() const override;

	bool is_struct() const override {
		return false;
	}

	Uc_struct_symbol* get_struct() const override {
		return nullptr;
	}
};

/*
 *  Class 'new'.
 */
class Uc_new_expression : public Uc_class_expression {
protected:
	Uc_array_expression* parms;    // Parameters passed to constructor.
public:
	Uc_new_expression(Uc_var_symbol* v, Uc_array_expression* p);

	~Uc_new_expression() override {
		delete parms;
	}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;

	bool is_class() const override {
		return true;
	}

	Uc_class* get_cls() const override;
};

/*
 *  Class 'delete'.
 */
class Uc_del_expression : public Uc_expression {
	Uc_class_inst_symbol* cls;

public:
	Uc_del_expression(Uc_class_inst_symbol* v) : cls(v) {}

	// Gen. code to put result on stack.
	void gen_value(Basic_block* out) override;
};

/*
 *  Write a byte value to the end/position of a character stream.
 */

inline void Write1(std::vector<char>& out, unsigned short val) {
	out.push_back(static_cast<char>(val & 0xff));
}

inline void Write1(std::vector<char>& out, int pos, unsigned short val) {
	out[pos] = static_cast<char>(val & 0xff);
}

/*
 *  Write a 2-byte value to the end/position of a character stream.
 */

inline void Write2(std::vector<char>& out, unsigned short val) {
	out.push_back(static_cast<char>(val & 0xff));
	out.push_back(static_cast<char>((val >> 8) & 0xff));
}

inline void Write2(std::vector<char>& out, int pos, unsigned short val) {
	out[pos]     = static_cast<char>(val & 0xff);
	out[pos + 1] = static_cast<char>((val >> 8) & 0xff);
}

inline unsigned short Read2(const std::vector<char>& in, int pos) {
	unsigned short val = in[pos];
	val |= in[pos + 1] << 8;
	return val;
}

/*
 *  Write a 4-byte value to the end/position of a character stream.
 */

inline void Write4(std::vector<char>& out, unsigned int val) {
	out.push_back(static_cast<char>(val & 0xff));
	out.push_back(static_cast<char>((val >> 8) & 0xff));
	out.push_back(static_cast<char>((val >> 16) & 0xff));
	out.push_back(static_cast<char>((val >> 24) & 0xff));
}

inline void Write4(std::vector<char>& out, int pos, unsigned int val) {
	out[pos]     = static_cast<char>(val & 0xff);
	out[pos + 1] = static_cast<char>((val >> 8) & 0xff);
	out[pos + 2] = static_cast<char>((val >> 16) & 0xff);
	out[pos + 3] = static_cast<char>((val >> 24) & 0xff);
}

inline unsigned int Read4(const std::vector<char>& in, int pos) {
	unsigned int val = in[pos];
	val |= in[pos + 1] << 8;
	val |= in[pos + 2] << 16;
	val |= in[pos + 3] << 24;
	return val;
}
#endif
