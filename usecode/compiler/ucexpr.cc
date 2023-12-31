/**
 ** Ucexpr.cc - Expressions for Usecode compiler.
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <cstdio>
#include "ucexpr.h"
#include "ucsym.h"
#include "utils.h"
#include "opcodes.h"
#include "ucfun.h"
#include "ucclass.h"
#include "ucloc.h"
#include "basic_block.h"
#include "ignore_unused_variable_warning.h"

using std::vector;

/*
 *  Default.  Just push the one value.
 *
 *  Output: # pushed
 */

int Uc_expression::gen_values(
    Basic_block *out
) {
	gen_value(out);         // Gen. result on stack.
	return 1;
}

/*
 *  Default assignment generation.
 */

void Uc_expression::gen_assign(
    Basic_block *out
) {
	ignore_unused_variable_warning(out);
	error("Can't assign to this expression");
}

/*
 *  Need a variable whose value is this expression.
 */

Uc_var_symbol *Uc_expression::need_var(
    Basic_block *out,
    Uc_function *fun
) {
	static int cnt = 0;
	char buf[50];
	snprintf(buf, sizeof(buf), "_tmpval_%d", cnt++);
	// Create a 'tmp' variable.
	Uc_var_symbol *var = fun->add_symbol(buf, true);
	if (var == nullptr) {
		return nullptr;       // Shouldn't happen.  Err. reported.
	}
	gen_value(out);         // Want to assign this value to it.
	var->gen_assign(out);
	return var;
}

/*
 *  Concatenate expression into an array.
 */

void Uc_expression::add_to(Uc_array_expression *arr) {
	arr->add(this);
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_expression::eval_const(
    int &val            // Value returned here.
) {
	val = 0;
	return false;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_var_expression::gen_value(
    Basic_block *out
) {
	if (!var->gen_value(out)) {
		char buf[150];
		snprintf(buf, sizeof(buf), "Can't use value of '%s'", var->get_name());
		error(buf);
	}
}

/*
 *  Generate assignment to this variable.
 */

void Uc_var_expression::gen_assign(
    Basic_block *out
) {
	if (!var->gen_assign(out)) {
		char buf[150];
		snprintf(buf, sizeof(buf), "Can't assign to '%s'", var->get_name());
		error(buf);
	}
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_fun_name_expression::is_object_function(bool error) const {
	if (fun->get_function_type() != Uc_function_symbol::utility_fun)
		return 0;
	else {
		if (error) {
			char buf[180];
			snprintf(buf, sizeof(buf), "'%s' must be 'shape#' or 'object#'",
			        fun->get_name());
			Uc_location::yyerror(buf);
		}
		return 1;
	}
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */
bool Uc_fun_name_expression::eval_const(
    int &val
) {
	val = fun->get_usecode_num();
	return true;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_fun_name_expression::gen_value(
    Basic_block *out
) {
	const int funid = fun->get_usecode_num();
	if (fun->has_high_id()) {
		WriteOp(out, UC_PUSHI32);
		WriteOpParam4(out, funid);
	} else {
		WriteOp(out, UC_PUSHI);
		WriteOpParam2(out, funid);
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_arrayelem_expression::gen_value(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_AIDX);  // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_arrayelem_expression::gen_assign(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_POPARR);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_static_arrayelem_expression::gen_value(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_AIDXS); // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_static_arrayelem_expression::gen_assign(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_POPARRS);   // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_class_arrayelem_expression::gen_value(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_AIDXTHV);   // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_class_arrayelem_expression::gen_assign(
    Basic_block *out
) {
	if (!index || !array)
		return;
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_POPARRTHV); // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_flag_expression::gen_value(
    Basic_block *out
) {
	int ival;
	if (flag->eval_const(ival)) {
		WriteOp(out, UC_PUSHF); // Opcode, flag #.
		WriteOpParam2(out, ival);
	} else {
		flag->gen_value(out);
		WriteOp(out, UC_PUSHFVAR);  // Opcode
	}
}

/*
 *  Generate assignment to this variable.
 */

void Uc_flag_expression::gen_assign(
    Basic_block *out
) {
	int ival;
	if (flag->eval_const(ival)) {
		WriteOp(out, UC_POPF);  // Opcode, flag #.
		WriteOpParam2(out, ival);
	} else {
		flag->gen_value(out);
		WriteOp(out, UC_POPFVAR);   // Opcode
	}
}

inline bool Uc_var_expression::is_struct() const {
	return var->get_struct() != nullptr;
}

inline Uc_struct_symbol *Uc_var_expression::get_struct() const {
	return var->get_struct();
}

inline int Uc_var_expression::is_object_function(bool error) const {
	return var->is_object_function(error);
}

inline void Uc_var_expression::set_is_obj_fun(int s) {
	var->set_is_obj_fun(s);
}

int Uc_var_expression::get_type() const {
	return var->get_sym_type();
}


/*
 *  Get offset in function's text_data.
 *
 *  Output: Offset.
 */

int Uc_var_expression::get_string_offset(
) {
	return var->get_string_offset();
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_binary_expression::gen_value(
    Basic_block *out
) {
	int ival;
	if (eval_const(ival)) {
		auto *iexpr = new Uc_int_expression(ival, intop);
		iexpr->gen_value(out);
		delete iexpr;
	} else {
		left->gen_value(out);       // First the left.
		right->gen_value(out);      // Then the right.
		WriteOp(out, opcode);
	}
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_binary_expression::eval_const(
    int &val            // Value returned here.
) {
	int val1;
	int val2;         // Get each side.
	if (!left->eval_const(val1) || !right->eval_const(val2)) {
		val = 0;
		return false;
	}
	switch (opcode) {
	case UC_ADD:
		val = val1 + val2;
		return true;
	case UC_SUB:
		val = val1 - val2;
		return true;
	case UC_MUL:
		val = val1 * val2;
		return true;
	case UC_DIV:
		if (!val2) {
			error("Division by 0");
			return false;
		}
		val = val1 / val2;
		return true;
	case UC_MOD:
		if (!val2) {
			error("Division by 0");
			return false;
		}
		val = val1 % val2;
		return true;
	case UC_CMPGT:
		val = val1 > val2;
		return true;
	case UC_CMPLT:
		val = val1 < val2;
		return true;
	case UC_CMPGE:
		val = val1 >= val2;
		return true;
	case UC_CMPLE:
		val = val1 <= val2;
		return true;
	case UC_CMPNE:
		val = val1 != val2;
		return true;
	case UC_CMPEQ:
		val = val1 == val2;
		return true;
	case UC_AND:
		val = val1 && val2;
		return true;
	case UC_OR:
		val = val1 || val2;
		return true;
	case UC_ARRA:
		val = 0;
		return false;
	default:
		val = 0;
		error("This operation not supported for integer constants");
		return false;
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_unary_expression::gen_value(
    Basic_block *out
) {
	int ival;
	if (eval_const(ival)) {
		auto *iexpr = new Uc_int_expression(ival);
		iexpr->gen_value(out);
		delete iexpr;
	} else {
		operand->gen_value(out);
		WriteOp(out, opcode);
	}
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_unary_expression::eval_const(
    int &val            // Value returned here.
) {
	int val1;           // Get each side.
	if (!operand->eval_const(val1)) {
		val = 0;
		return false;
	}
	switch (opcode) {
	case UC_NOT:
		val = !val1;
		return true;
	default:
		val = 0;
		error("This operation not supported for integer constants");
		return false;
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_int_expression::gen_value(
    Basic_block *out
) {
	WriteOp(out, opcode);
	if (opcode == UC_PUSHB)
		WriteOpParam1(out, value);
	else if (opcode == UC_PUSHI)
		WriteOpParam2(out, value);
	else
		WriteOpParam4(out, value);
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_int_expression::is_object_function(bool error) const {
	char buf[150];
	if (value < 0) {
		if (error) {
			snprintf(buf, sizeof(buf), "Invalid fun. ID (%d): can't call negative function", value);
			Uc_location::yyerror(buf);
		}
		return 2;
	} else if (value < 0x800)
		return 0;   // This is always an object/shape function.

	Uc_function_symbol *sym = Uc_function_symbol::search_num(value);
	if (!sym)
		return -1;  // Can't determine.
	else if (sym->get_function_type() == Uc_function_symbol::utility_fun) {
		if (error) {
			snprintf(buf, sizeof(buf),
			        "'%s' (fun. ID %d)  must be 'shape#' or 'object#'",
			        sym->get_name(), value);
			Uc_location::yyerror(buf);
		}
		return 1;
	}
	return 0;
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_int_expression::eval_const(
    int &val            // Value returned here.
) {
	val = value;
	return true;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_bool_expression::gen_value(
    Basic_block *out
) {
	if (tf)
		WriteOp(out, UC_PUSHTRUE);
	else
		WriteOp(out, UC_PUSHFALSE);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_event_expression::gen_value(
    Basic_block *out
) {
	WriteOp(out, UC_PUSHEVENTID);
}

/*
 *  Generate assignment to this variable.
 */

void Uc_event_expression::gen_assign(
    Basic_block *out
) {
	WriteOp(out, UC_POPEVENTID);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_item_expression::gen_value(
    Basic_block *out
) {
	WriteOp(out, UC_PUSHITEMREF);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_choice_expression::gen_value(
    Basic_block *out
) {
	WriteOp(out, UC_PUSHCHOICE);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_string_expression::gen_value(
    Basic_block *out
) {
	if (is_int_32bit(offset)) {
		WriteOp(out, UC_PUSHS32);
		WriteOpParam4(out, offset);
	} else {
		WriteOp(out, UC_PUSHS);
		WriteOpParam2(out, offset);
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_string_prefix_expression::gen_value(
    Basic_block *out
) {
	if (is_int_32bit(get_string_offset())) {
		WriteOp(out, UC_PUSHS32);
		WriteOpParam4(out, offset);
	} else {
		WriteOp(out, UC_PUSHS);
		WriteOpParam2(out, offset);
	}
}

/*
 *  Get offset in function's text_data.
 *
 *  Output: Offset.
 */

int Uc_string_prefix_expression::get_string_offset(
) {
	if (offset < 0)         // First time?
		// Look up & print errors.
		offset = fun->find_string_prefix(*this, prefix.c_str());
	return offset;
}

/*
 *  Delete a list of expressions.
 */

Uc_array_expression::~Uc_array_expression(
) {
	for (auto *expr : exprs)
		delete expr;
}

/*
 *  Concatenate another expression, or its values if an array, onto this.
 *  If the expression is an array, it's deleted after its elements are
 *  taken.
 */

void Uc_array_expression::concat(
    Uc_expression *e
) {
	e->add_to(this);
}

/*
 *  Concatenate array into another array, and destroys the current array
 *  is destroyed and should not be used after this function is called.
 */

void Uc_array_expression::add_to(Uc_array_expression *arr) {
	arr->add(exprs);
	exprs.clear();   // Don't want to delete elements.
	delete this;     // But this array is history.
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_array_expression::gen_value(
    Basic_block *out
) {
	const int actual = Uc_array_expression::gen_values(out);
	WriteOp(out, UC_ARRC);
	WriteOpParam2(out, actual);
}

/*
 *  Push all values onto the stack.  This is also called for parm. lists.
 *
 *  Output: # pushed
 */

int Uc_array_expression::gen_values(
    Basic_block *out
) {
	int actual = 0;         // (Just to be safe.)
	// Push backwards, so #0 pops first.
	for (auto it =
	            exprs.rbegin(); it != exprs.rend(); ++it) {
		Uc_expression *expr = *it;
		if (expr) {
			actual++;
			expr->gen_value(out);
		}
	}
	return actual;
}

inline bool Uc_call_expression::is_struct() const {
	return sym->get_struct() != nullptr;
}

inline Uc_struct_symbol *Uc_call_expression::get_struct() const {
	return sym->get_struct();
}

inline bool Uc_call_expression::is_class() const {
	return sym->get_cls() != nullptr;
}

inline Uc_class *Uc_call_expression::get_cls() const {
	return sym->get_cls();
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_call_expression::is_object_function(bool error) const {
	auto *fun = dynamic_cast<Uc_intrinsic_symbol *>(sym);
	if (!fun)
		return -1;  // Can't determine.

	char buf[150];
	if (fun == Uc_function::get_get_usecode_fun())
		return 0;   // It is.
	else if (fun == Uc_function::get_get_item_shape()) {
		// *Could* be, if not a high shape.
		// Let's say it is, but issue a warning.
		if (error) {
			snprintf(buf, sizeof(buf), "Shape # is equal to fun. ID only for shapes < 0x400; use UI_get_usecode_fun instead");
			Uc_location::yywarning(buf);
		}
		return -2;
	}
	// For now, no other intrinsics return a valid fun ID.
	if (error) {
		snprintf(buf, sizeof(buf), "Return of intrinsic '%s' is not fun. ID", fun->get_name());
		Uc_location::yyerror(buf);
	}
	return 3;
}

/*
 *  Generate code to check if the passed params are in the correct number
 *  and of the correct types.
 */

void Uc_call_expression::check_params() {
	auto *fun = dynamic_cast<Uc_function_symbol *>(sym);
	if (!fun) {
		// Intrinsics; do nothing for now.
		return;
	}
	const vector<Uc_var_symbol *> &protoparms = fun->get_parms();
	const vector<Uc_expression *> &callparms = parms->get_exprs();
	const unsigned long ignore_this = fun->get_method_num() >= 0 ? 1 : 0;
	const unsigned long parmscnt = callparms.size() + ignore_this;
	if (parmscnt != protoparms.size()) {
		char buf[150];
		const unsigned long protoparmcnt = protoparms.size() - ignore_this;
		snprintf(buf, sizeof(buf),
		        "# parms. passed (%lu) doesn't match '%s' count (%lu)",
		        parmscnt - ignore_this, sym->get_name(), protoparmcnt);
		Uc_location::yyerror(buf);
		return;
	}
	for (unsigned long i = ignore_this; i < parmscnt; i++) {
		Uc_expression *expr = callparms[i - ignore_this];
		Uc_var_symbol *var = protoparms[i];
		auto *cls =
		    dynamic_cast<Uc_class_inst_symbol *>(var);
		char buf[180];
		if (expr->is_class()) {
			if (!cls) {
				snprintf(buf, sizeof(buf),
				        "Error in parm. #%lu: cannot convert class to non-class", i + 1);
				Uc_location::yyerror(buf);
			} else if (!expr->get_cls()->is_class_compatible(
			               cls->get_cls()->get_name())) {
				snprintf(buf, sizeof(buf),
				        "Error in parm. #%lu: class '%s' cannot be converted into class '%s'",
				        i + 1, expr->get_cls()->get_name(),
				        cls->get_cls()->get_name());
				Uc_location::yyerror(buf);
			}
		} else {
			if (cls) {
				snprintf(buf, sizeof(buf),
				        "Error in parm. #%lu: cannot convert non-class into class", i + 1);
				Uc_location::yyerror(buf);
			}
		}
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_call_expression::gen_value(
    Basic_block *out
) {
	if (ind) {          // Indirect?
		const size_t parmcnt = parms->gen_values(out);    // Want to push parm. values.
		if (!itemref) {
			Uc_item_expression item;
			item.gen_value(out);
		} else
			itemref->gen_value(out);
		ind->gen_value(out);    // Function #.
		if (parmcnt) {
			WriteOp(out, UC_CALLINDEX);
			WriteOpParam1(out, parmcnt);
		} else
			WriteOp(out, UC_CALLIND);
		return;
	}
	if (!sym)
		return;         // Already failed once.
	if (!sym->gen_call(out, function, original, itemref,
	                   parms, return_value, meth_scope)) {
		char buf[150];
		snprintf(buf, sizeof(buf), "'%s' isn't a function or intrinsic",
		        sym->get_name());
		sym = nullptr;        // Avoid repeating error if in loop.
		error(buf);
	}
}

void Uc_class_expression::gen_value(
    Basic_block *out
) {
	if (!var->gen_value(out)) {
		char buf[150];
		snprintf(buf, sizeof(buf), "Can't assign to '%s'", var->get_name());
		error(buf);
	}
}

inline Uc_class *Uc_class_expression::get_cls() const {
	return var->get_cls();
}

inline Uc_class *Uc_new_expression::get_cls() const {
	return var->get_cls();
}

void Uc_class_expression::gen_assign(
    Basic_block *out
) {
	if (!var->gen_assign(out)) {
		char buf[150];
		snprintf(buf, sizeof(buf), "Can't assign to '%s'", var->get_name());
		error(buf);
	}
}

/*
 *  Ensure that the correct number of arguments are pushed to constructor.
 */
Uc_new_expression::Uc_new_expression(
    Uc_var_symbol *v,
    Uc_array_expression *p
)
	: Uc_class_expression(v), parms(p) {
	Uc_class *cls = var->get_cls();
	const int pushed_parms = parms->get_exprs().size();
	if (cls->get_num_vars() > pushed_parms) {
		char buf[180];
		const int missing = cls->get_num_vars() - pushed_parms;
		snprintf(buf, sizeof(buf), "%d argument%s missing in constructor of class '%s'",
		        missing, (missing > 1) ? "s" : "", cls->get_name());
		yywarning(buf);
	} else if (cls->get_num_vars() < pushed_parms) {
		char buf[180];
		snprintf(buf, sizeof(buf), "Too many arguments in constructor of class '%s'",
		        cls->get_name());
		yyerror(buf);
	}
	// Ensure that all data members get initialized.
	for (int i = pushed_parms; i < cls->get_num_vars(); i++)
		parms->add(new Uc_int_expression(0));
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_new_expression::gen_value(
    Basic_block *out
) {
	(void)parms->gen_values(out);
	Uc_class *cls = var->get_cls();
	WriteOp(out, UC_CLSCREATE);
	WriteOpParam2(out, cls->get_num());
}

/*
 *  Generate code to delete class.
 */

void Uc_del_expression::gen_value(
    Basic_block *out
) {
	cls->gen_value(out);
	WriteOp(out, UC_CLASSDEL);
}
