/**
 ** Ucsym.cc - Usecode compiler symbol table.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ucsym.h"

#include "basic_block.h"
#include "ignore_unused_variable_warning.h"
#include "opcodes.h"
#include "ucclass.h"
#include "ucexpr.h"
#include "ucfun.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

using std::strcmp;

int                          Uc_function_symbol::last_num = -1;
Uc_function_symbol::Sym_nums Uc_function_symbol::nums_used;
bool                         Uc_function_symbol::new_auto_num = false;

/*
 *  Assign value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_symbol::gen_assign(Basic_block* out) {
	ignore_unused_variable_warning(out);
	return 0;
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_symbol::gen_value(Basic_block* out) {
	ignore_unused_variable_warning(out);
	return 0;
}

/*
 *  Generate function call.
 *
 *  Output: 0 if can't do this.
 */

int Uc_symbol::gen_call(
		Basic_block* out, Uc_function* fun,
		bool                 orig,         // Call original (not one from patch).
		Uc_expression*       itemref,      // Non-nullptr for CALLE.
		Uc_array_expression* parms,        // Parameter list.
		bool                 retvalue,     // True if a function.
		Uc_class*            scope_vtbl    // For method calls using a different scope.
) {
	ignore_unused_variable_warning(out, fun, orig, itemref, parms, retvalue, scope_vtbl);
	return 0;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_symbol::create_expression() {
	return nullptr;
}

/*
 *  Assign value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_var_symbol::gen_assign(Basic_block* out) {
	WriteOp(out, UC_POP);
	WriteOpParam2(out, offset);
	return 1;
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_var_symbol::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSH);
	WriteOpParam2(out, offset);
	return 1;
}

int Uc_var_symbol::is_object_function(bool print_error) const {
	if (print_error) {
		switch (is_obj_fun) {
		case -2:
			if (Uc_location::get_shapefun_warn()) {
				Uc_location::yywarning(
						Uc_location::Warning::shape_to_function, "a shape number is a function ID only below 0x400; use "
																 "UI_get_usecode_fun instead");
			}
			break;
		case 1:
			Uc_location::yyerror("variable '%s' holds a function not declared as 'shape#' or 'object#'", name.c_str());
			break;
		case 2:
			Uc_location::yyerror("variable '%s' holds a negative function ID", name.c_str());
			break;
		case 3:
			Uc_location::yyerror("intrinsic result is not a valid script function ID");
			break;
		}
	}
	return is_obj_fun;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_var_symbol::create_expression() {
	return new Uc_var_expression(this);
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_class_inst_symbol::create_expression() {
	return new Uc_class_expression(this);
}

/*
 *  Assign value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_static_var_symbol::gen_assign(Basic_block* out) {
	WriteOp(out, UC_POPSTATIC);
	WriteOpParam2(out, offset);
	return 1;
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_static_var_symbol::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHSTATIC);
	WriteOpParam2(out, offset);
	return 1;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_static_class_symbol::create_expression() {
	return new Uc_class_expression(this);
}

/*
 *  Check for a duplicate symbol and print an error.
 *
 *  Output: true if dup., with error printed.
 */

bool Uc_struct_symbol::is_dup(const char* nm) {
	const int index = search(nm);
	if (index >= 0) {    // Already declared?
		Uc_location::yyerror("redefinition of '%s'", nm);
		member_locations.at(nm).note("previous declaration is here");
		return true;
	}
	return false;
}

Uc_struct_symbol::~Uc_struct_symbol() = default;

void Uc_struct_symbol::merge_struct(Uc_struct_symbol* other) {
	if (!other) {
		return;    // The unknown struct type was already diagnosed.
	}
	for (auto& var : other->vars) {
		if (add(var.first.c_str())) {
			member_locations[var.first] = other->member_locations.at(var.first);
		}
	}
}

/*
 *  Create new class symbol and store in global table.
 */

Uc_class_symbol* Uc_class_symbol::create(char* nm, Uc_class* c) {
	Uc_symbol* sym = Uc_function::search_globals(nm);
	if (sym) {
		Uc_location::yyerror("redefinition of class '%s'", nm);
		sym->note("previous declaration is here");
	}
	auto* csym = new Uc_class_symbol(nm, c);
	Uc_function::add_global_class_symbol(csym);
	return csym;
}

/*
 *  Assign value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_class_var_symbol::gen_assign(Basic_block* out) {
	WriteOp(out, UC_POPTHV);
	WriteOpParam2(out, offset);
	return 1;
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_class_var_symbol::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHTHV);
	WriteOpParam2(out, offset);
	return 1;
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_const_int_symbol::gen_value(Basic_block* out) {
	WriteOp(out, opcode);
	if (opcode == UC_PUSHB) {
		WriteOpParam1(out, value);
	} else if (opcode == UC_PUSHI) {
		WriteOpParam2(out, value);
	} else {
		WriteOpParam4(out, value);
	}
	return 1;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_const_int_symbol::create_expression() {
	return new Uc_int_expression(value, opcode);
}

/*
 *  Generate code to push variable's value on stack.
 *
 *  Output: 0 if can't do this.
 */

int Uc_string_symbol::gen_value(Basic_block* out) {
	if (is_int_32bit(offset)) {
		WriteOp(out, UC_PUSHS32);
		WriteOpParam4(out, offset);
	} else {
		WriteOp(out, UC_PUSHS);
		WriteOpParam2(out, offset);
	}
	return 1;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_string_symbol::create_expression() {
	return new Uc_string_expression(offset);
}

/*
 *  Generate function call.
 *
 *  Output: 0 if can't do this.
 */

int Uc_intrinsic_symbol::gen_call(
		Basic_block* out, Uc_function* fun,
		bool                 orig,         // Call original (not one from patch).
		Uc_expression*       itemref,      // Non-nullptr for CALLE.
		Uc_array_expression* parms,        // Parameter list.
		bool                 retvalue,     // True if a function.
		Uc_class*            scope_vtbl    // For method calls using a different scope.
) {
	ignore_unused_variable_warning(fun, orig, scope_vtbl);
	int parmcnt = parms->gen_values(out);    // Want to push parm. values.
	if (itemref) {                           // Happens with 'method' call.
		itemref->gen_value(out);
		parmcnt++;
	}
	// ++++ parmcnt == num_parms.
	// Opcode depends on val. returned.
	WriteOp(out, retvalue ? UC_CALLIS : UC_CALLI);
	WriteOpParam2(out, intrinsic_num);    // Intrinsic # is 2 bytes.
	WriteOpParam1(out, parmcnt);          // Parm. count is 1.
	return 1;
}

/*
 *  Create new function.
 */

Uc_function_symbol::Uc_function_symbol(
		const char* nm,
		int         num,    // Function #, or -1 to assign
		//  1 + last_num.
		std::vector<Uc_var_symbol*>& p, int shp, Function_kind kind)
		: Uc_symbol(nm), parms(p), usecode_num(num), method_num(-1), shape_num(shp), externed(false), inherited(false),
		  ret_type(no_ret), high_id(false), type(kind) {
	high_id = is_int_32bit(usecode_num);
}

bool Uc_function_symbol::check_return_type(Function_ret type, Uc_class* cls, Uc_struct_symbol* structure) {
	if (id_declaration
		&& (type != id_declaration->ret_type || (type == class_ret && cls != id_declaration->ret_sym.cls)
			|| (type == struct_ret && structure != id_declaration->ret_sym.str))) {
		Uc_location::yyerror(
				"conflicting return type for function '%s': usecode ID 0x%x was previously declared as '%s'", get_name(),
				static_cast<unsigned int>(usecode_num), id_declaration->get_name());
		id_declaration->note("previous declaration is here");
		return_type_valid = false;
		return false;
	}
	if (return_declared
		&& (type != ret_type || (type == class_ret && cls != ret_sym.cls) || (type == struct_ret && structure != ret_sym.str))) {
		Uc_location::yyerror("conflicting return type for function '%s'", get_name());
		return_type_valid = false;
		note("previous declaration is here");
		return false;
	}
	return_declared = true;
	if (id_declaration && id_declaration_valid && warning_enabled(Warning::function_id_alias)) {
		Uc_location::yywarning(
				Warning::function_id_alias, "function '%s' shares usecode ID 0x%x with previously declared function '%s'",
				get_name(), static_cast<unsigned int>(usecode_num), id_declaration->get_name());
		id_declaration->note("previous declaration is here");
	}
	return true;
}

void Uc_function_symbol::set_ret_type(bool var) {
	const auto type = var ? var_ret : no_ret;
	if (check_return_type(type)) {
		ret_type = type;
	}
}

void Uc_function_symbol::set_ret_type(Uc_class* cls) {
	if (cls && check_return_type(class_ret, cls)) {
		ret_type    = class_ret;
		ret_sym.cls = cls;
	}
}

void Uc_function_symbol::set_ret_type(Uc_struct_symbol* structure) {
	if (structure && check_return_type(struct_ret, nullptr, structure)) {
		ret_type    = struct_ret;
		ret_sym.str = structure;
	}
}

/*
 *  Create new function symbol or return existing one (which could
 *  have been declared EXTERN).
 */

Uc_function_symbol* Uc_function_symbol::create(
		char* nm,
		int   num,    // Function #, or -1 to assign
		//  1 + last_num.
		std::vector<Uc_var_symbol*>& p, bool is_extern, Uc_scope* scope, Function_kind kind) {
	const auto kind_name = [](Function_kind type) {
		return type == utility_fun ? "utility" : type == shape_fun ? "shape#" : "object#";
	};
	// Checking num and kind for backward compatibility.
	if (num < 0) {
		// Treat as autonumber request.
		num = -1;
		if (kind == shape_fun) {
			Uc_location::yyerror("shape number cannot be negative");
		}
	} else if (num < 0x400) {
		if (kind == utility_fun) {
			Uc_location::yywarning(
					Uc_location::Warning::implicit_function_kind,
					"treating function '%s' as 'shape#' because its ID is below 0x400", nm);
			kind = shape_fun;
		}
	} else if (num < 0x800) {
		if (kind == utility_fun) {
			Uc_location::yywarning(
					Uc_location::Warning::implicit_function_kind,
					"treating function '%s' as 'object#' because its ID is between 0x400 and 0x7ff", nm);
			kind = object_fun;
		}
	}

	const int shp = (kind == shape_fun) ? num : -1;
	if (shp >= 0x400) {
		if (!new_auto_num && shp > INT32_MAX - 0xC00) {
			Uc_location::yyerror("shape number %d is too large to map to a function ID", shp);
			num = -1;
		} else {
			num = new_auto_num ? -1 : 0xC00 + shp;
		}
	} else if (shp != -1) {
		num = shp;
	}

	// Override function number if the function has been declared before this.
	auto* sym = dynamic_cast<Uc_function_symbol*>(scope ? scope->search(nm) : Uc_function::search_globals(nm));
	if (sym) {
		if (sym->get_function_type() != kind) {
			Uc_location::yyerror(
					"conflicting kind for function '%s': declared as '%s', previously '%s'", nm, kind_name(kind),
					kind_name(sym->get_function_type()));
			sym->note("previous declaration is here");
		} else if (scope) {
			if (!sym->is_inherited()) {
				Uc_location::yyerror("redefinition of function '%s'", nm);
				sym->note("previous declaration is here");
			}
		} else if (sym->is_externed() || is_extern) {
			if (sym->get_num_parms() == p.size()) {
				// If the new symbol is not externed, then the function
				// has been defined afterwards and we need to update
				// sym to not be extern anymore.
				if (!is_extern) {
					sym->clear_externed();
				}
				num = sym->get_usecode_num();
			} else {
				Uc_location::yyerror(
						"conflicting declaration of function '%s': expected %zu parameters, got %zu", nm, sym->get_num_parms(),
						p.size());
				sym->note("previous declaration is here");
				num = -1;
			}
		} else {
			Uc_location::yyerror("redefinition of function '%s'", nm);
			sym->note("previous declaration is here");
		}
	}

	if (num < 0 && !new_auto_num) {
		Uc_location::yywarning(
				Uc_location::Warning::autonumber, "function '%s' is automatically numbered without a #autonumber directive", nm);
	}

	if (num < 0 && last_num == INT32_MAX) {
		Uc_location::yyerror("automatic function ID exceeds the supported range");
		num = INT32_MAX;
	}
	const int ucnum = num >= 0 ? num : (last_num + 1);
	// Set last_num if the function doesn't
	// have a number:
	if (num < 0 ||
		// Or if we are using old-style autonumbers:
		(num >= 0 && !new_auto_num)) {
		last_num = ucnum;
	}
	// Keep track of #'s used.
	auto it = nums_used.find(ucnum);
	if (it == nums_used.end()) {    // Unused?  That's good.
		sym = new Uc_function_symbol(nm, ucnum, p, shp, kind);
		if (is_extern) {
			sym->set_externed();
		}
		nums_used[ucnum] = sym;
		if (!is_extern) {
			sym->definition = sym;
		}
		return sym;
	}
	auto* previous = it->second;
	if (!sym || sym->get_usecode_num() != ucnum) {
		sym = previous;
	}
	const bool alias = sym->name != nm;
	if (alias) {
		sym                 = new Uc_function_symbol(nm, ucnum, p, shp, kind);
		sym->id_declaration = previous;
		if (is_extern) {
			sym->set_externed();
		}
	}
	auto*       signature = sym->id_declaration ? sym->id_declaration : sym;
	std::string id_context;
	if (sym->id_declaration) {
		id_context = " (usecode ID 0x";
		char id[16];
		snprintf(id, sizeof(id), "%x", static_cast<unsigned int>(ucnum));
		id_context += id;
		id_context += " previously declared as '";
		id_context += signature->get_name();
		id_context += "')";
	}
	sym->id_declaration_valid = true;
	if (alias && (num < 0 || scope)) {
		Uc_location::yyerror(
				"function '%s' conflicts with previously declared function '%s' at usecode ID 0x%x", nm, previous->get_name(),
				static_cast<unsigned int>(ucnum));
		previous->note("previous declaration is here");
		sym->id_declaration_valid = false;
	}
	if (sym->id_declaration && kind != signature->get_function_type()) {
		Uc_location::yyerror(
				"conflicting kind for function '%s'%s: declared as '%s', previously '%s'", nm, id_context.c_str(), kind_name(kind),
				kind_name(signature->get_function_type()));
		signature->note("previous declaration is here");
		sym->id_declaration_valid = false;
	}
	if (!is_extern && previous->definition && previous->definition != sym) {
		Uc_location::yyerror(
				"redefinition of function '%s': usecode ID 0x%x is already defined as '%s'", nm, static_cast<unsigned int>(ucnum),
				previous->definition->get_name());
		previous->definition_location.note("previous definition is here");
		sym->id_declaration_valid = false;
	} else if (!is_extern) {
		previous->definition          = sym;
		previous->definition_location = Uc_location();
	}
	if (signature->get_num_parms() != p.size()) {
		Uc_location::yyerror(
				"conflicting declaration of function '%s'%s: expected %zu parameters, got %zu", nm, id_context.c_str(),
				signature->get_num_parms(), p.size());
		signature->note("previous declaration is here");
		sym->id_declaration_valid = false;
	} else {
		auto get_param_type = [](const Uc_var_symbol* var) {
			std::string type;
			if (var->get_sym_type() == Uc_symbol::Class) {
				type = "class<";
				type += var->get_cls()->get_name();
				type += ">";
			} else if (const auto* str = var->get_struct(); str != nullptr) {
				type = "struct<";
				type += str->get_name();
				type += ">";
			} else {
				type = "var";
			}
			return type;
		};
		const auto& parms = signature->get_parms();
		for (size_t i = 0; i < p.size(); i++) {
			if (p[i]->get_sym_type() != parms[i]->get_sym_type()
				|| (p[i]->get_sym_type() == Uc_symbol::Class && p[i]->get_cls() != parms[i]->get_cls())
				|| p[i]->get_struct() != parms[i]->get_struct()) {
				Uc_location::yyerror(
						"conflicting type for parameter %zu of function '%s'%s: expected '%s', got '%s'", i + 1, nm,
						id_context.c_str(), get_param_type(parms[i]).c_str(), get_param_type(p[i]).c_str());
				parms[i]->note("previous parameter declaration is here");
				sym->id_declaration_valid = false;
			}
		}
		if (!is_extern) {
			// Override symbol parameter names with those from the definition.
			sym->parms = p;
		}
	}
	return sym;
}

/*
 *  Create an expression with this value.
 */

Uc_expression* Uc_function_symbol::create_expression() {
	return new Uc_fun_name_expression(this);
}

/*
 *  Generate function call.
 *
 *  Output: 0 if can't do this.
 */

int Uc_function_symbol::gen_call(
		Basic_block* out, Uc_function* fun,
		bool                 orig,         // Call original (not one from patch).
		Uc_expression*       itemref,      // Non-nullptr for CALLE or method.
		Uc_array_expression* aparms,       // Actual parameter list.
		bool                 retvalue,     // True if a function.
		Uc_class*            scope_vtbl    // For method calls using a different scope.
) {
	size_t parmcnt = aparms->gen_values(out);    // Want to push parm. values.
	parmcnt += (method_num >= 0);                // Count 'this'.
	if (parmcnt != parms.size()) {
		Uc_location::yyerror("argument count mismatch for '%s': got %zu, expected %zu", get_name(), parmcnt, parms.size());
		note("function declared here");
	}
	// See if expecting a return value from a function that has none.
	if (retvalue && !has_ret()) {
		Uc_location::yyerror("void function '%s' does not return a value", get_name());
		note("function declared here");
	}
	if (orig) {
		if (!itemref) {
			Uc_item_expression item;
			item.gen_value(out);
		} else {
			itemref->gen_value(out);
		}
		WriteOp(out, UC_CALLO);
		WriteOpParam2(out, usecode_num);    // Use fun# directly.
	} else if (method_num >= 0) {           // Class method?
		// If no explicit obj., find 'this'.
		if (!itemref && fun->get_method_num() >= 0) {
			Uc_symbol* tsym = fun->search("this");
			if (tsym && dynamic_cast<Uc_var_symbol*>(tsym)) {
				itemref = tsym->create_expression();
			}
		}
		if (!itemref) {
			Uc_location::yyerror("member function '%s' requires a class instance", get_name());
		} else {
			itemref->gen_value(out);
		}
		if (scope_vtbl) {
			WriteOp(out, UC_CALLMS);
			WriteOpParam2(out, method_num);
			WriteOpParam2(out, scope_vtbl->get_num());
		} else {
			WriteOp(out, UC_CALLM);
			WriteOpParam2(out, method_num);
		}
	} else if (high_id) {
		if (itemref) {
			itemref->gen_value(out);
			WriteOp(out, UC_CALLE32);
		} else {
			WriteOp(out, UC_CALL32);
		}
		WriteOpParam4(out, usecode_num);    // Use fun# directly.
	} else if (itemref) {                   // Doing CALLE?  Push item onto stack.
		// The originals would need this.
		fun->link(this);
		itemref->gen_value(out);
		WriteOp(out, UC_CALLE);
		WriteOpParam2(out, usecode_num);    // Use fun# directly.
	} else {                                // Normal CALL.
		// Called function sets return.
		// Add to externs list.
		const int link = fun->link(this);
		WriteOp(out, UC_CALL);
		WriteOpParam2(out, link);
	}
	if (!retvalue && has_ret()) {
		// Function returns a value, but caller does not use it.
		// Generate the code to pop the result off the stack.
		static int cnt = 0;
		// Create a 'tmp' variable.
		std::string    tmp_name = "_tmpretval_" + std::to_string(cnt++);
		Uc_var_symbol* var      = fun->add_symbol(tmp_name.c_str(), true);
		if (!var) {
			return 0;    // Shouldn't happen.  Err. reported.
		}
		var->gen_assign(out);
	}
	return 1;
}

bool String_compare::operator()(const char* const& x, const char* const& y) const {
	return strcmp(x, y) < 0;
}

/*
 *  Delete.
 */

Uc_scope::~Uc_scope() {
	for (auto& symbol : symbols) {
		delete symbol.second;
	}
	for (auto* scope : scopes) {
		delete scope;
	}
}

/*
 *  Search upwards through scope.
 *
 *  Output: ->symbol if found, else 0.
 */

Uc_symbol* Uc_scope::search_up(const char* nm) {
	Uc_symbol* found = search(nm);    // First look here.
	if (found) {
		return found;
	}
	if (parent) {    // Look upwards.
		return parent->search_up(nm);
	} else {
		return nullptr;
	}
}

/*
 *  Add a function symbol.
 *
 *  Output: 0 if already there.  Errors reported.
 */

int Uc_scope::add_function_symbol(Uc_function_symbol* fun, Uc_scope* parent) {
	Uc_location::Context diagnostic_context(*fun);
	const char*          nm = fun->get_name();
	Uc_symbol*           found;    // Already here?
	if (parent) {
		found = parent->search(nm);
	} else {
		found = search(nm);
	}
	if (!found) {    // If not, that's good.
		if (parent) {
			parent->add(fun);
		} else {
			add(fun);
		}
		return 1;
	}
	auto* fun2 = dynamic_cast<Uc_function_symbol*>(found);
	if (fun2 == fun) {    // The case for an EXTERN.
		return 1;
	} else if (!fun2) {    // Non-function name.
		Uc_location::yyerror("redefinition of '%s'", nm);
	} else if (fun->get_num_parms() != fun2->get_num_parms()) {
		if (fun2->is_inherited()) {
			Uc_location::yyerror("declaration of member function '%s' conflicts with its base-class declaration", nm);
		} else {
			Uc_location::yyerror("conflicting declaration of '%s'", nm);
		}
	} else if (fun->usecode_num != fun2->usecode_num) {
		if (fun2->externed || fun->externed || fun2->is_inherited()) {
			if (!Uc_function_symbol::new_auto_num && Uc_function_symbol::last_num == fun->usecode_num) {
				--Uc_function_symbol::last_num;
			}
		} else {
			Uc_location::yyerror("declaration of '%s' has a different usecode ID", nm);
		}
	}
	found->note("previous declaration is here");
	return 0;
}

// Bounded adjacent-transposition distance for diagnostic suggestions. Avoid
// quadratic work on arbitrarily long malformed identifiers.
Uc_symbol* Uc_scope::find_similar(const char* nm, unsigned& distance) const {
	const std::string name(nm);
	const unsigned    limit = name.size() < 3 ? 0 : name.size() < 6 ? 1 : 2;
	distance                = limit + 1;
	if (!limit || name.size() > 64) {
		return nullptr;
	}
	Uc_symbol* best      = nullptr;
	bool       ambiguous = false;
	for (const Uc_scope* scope = this; scope; scope = scope->parent) {
		for (const auto& entry : scope->symbols) {
			auto*       symbol    = entry.second;
			const auto& candidate = symbol->name;
			if (symbol->error_placeholder || candidate.size() > name.size() + limit || name.size() > candidate.size() + limit) {
				continue;
			}
			std::vector<std::vector<unsigned>> d(name.size() + 1, std::vector<unsigned>(candidate.size() + 1));
			for (size_t i = 0; i <= name.size(); ++i) {
				d[i][0] = i;
			}
			for (size_t j = 0; j <= candidate.size(); ++j) {
				d[0][j] = j;
			}
			for (size_t i = 1; i <= name.size(); ++i) {
				for (size_t j = 1; j <= candidate.size(); ++j) {
					d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (name[i - 1] != candidate[j - 1])});
					if (i > 1 && j > 1 && name[i - 1] == candidate[j - 2] && name[i - 2] == candidate[j - 1]) {
						d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
					}
				}
			}
			const unsigned score = d.back().back();
			if (score < distance) {
				distance  = score;
				best      = symbol;
				ambiguous = false;
			} else if (score == distance && best && candidate != best->name) {
				ambiguous = true;
			}
		}
	}
	return ambiguous ? nullptr : best;
}

/*
 *  Check for a duplicate symbol and print an error.
 *
 *  Output: true if dup., with error printed.
 */

bool Uc_scope::is_dup(const char* nm) {
	Uc_symbol* sym = search(nm);
	if (sym) {    // Already in scope?
		Uc_location::yyerror("redefinition of '%s'", nm);
		sym->note("previous declaration is here");
		return true;
	}
	return false;
}
