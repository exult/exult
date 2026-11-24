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
#	include <config.h>
#endif

#include "ucexpr.h"

#include "basic_block.h"
#include "ignore_unused_variable_warning.h"
#include "opcodes.h"
#include "ucclass.h"
#include "ucfun.h"
#include "ucloc.h"
#include "ucsym.h"

#include <algorithm>
#include <cstdio>
#include <limits>

using std::vector;

Uc_array_info Uc_array_info::concat(Uc_array_info left, Uc_array_info right) {
	Uc_array_info result{left.is_script() || right.is_script() ? Kind::script : Kind::array, {}};
	result.contains_repeat = left.contains_repeat || right.contains_repeat;
	if (left.length && right.length && *left.length <= std::numeric_limits<int>::max() - *right.length) {
		result.length = *left.length + *right.length;
	}
	return result;
}

Uc_array_info Uc_array_info::merge(Uc_array_info left, Uc_array_info right) {
	Uc_array_info result;
	result.contains_repeat = left.contains_repeat || right.contains_repeat;
	if (left.kind == right.kind) {
		result.kind = left.kind;
	} else if (left.is_array() && right.is_array()) {
		result.kind = Kind::array;
	}
	if (left.length == right.length) {
		result.length = left.length;
	}
	return result;
}

// Aliases share the facts of their underlying variable, including alias chains.
static Uc_var_symbol* Array_variable(Uc_var_symbol* var) {
	while (var->get_sym() != var) {
		var = static_cast<Uc_var_symbol*>(var->get_sym());
	}
	return var;
}

void Analyze_array_shapes(Basic_block* entry) {
	std::map<Basic_block*, Uc_array_state> incoming;
	std::vector<Basic_block*>              pending{entry};
	std::vector<Basic_block*>              order{entry};
	incoming[entry] = {};
	for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
		auto* block = pending[cursor];
		auto  state = incoming[block];
		for (const auto& action : block->array_actions) {
			action(state, false);
		}
		for (auto* next : {block->get_taken(), block->get_ntaken()}) {
			if (!next) {
				continue;
			}
			auto found = incoming.find(next);
			if (found == incoming.end()) {
				incoming[next] = state;
				order.push_back(next);
				pending.push_back(next);
				continue;
			}
			auto merged = found->second;
			for (auto it = merged.begin(); it != merged.end();) {
				auto other = state.find(it->first);
				if (other == state.end()) {
					it = merged.erase(it);
				} else {
					it->second = Uc_array_info::merge(it->second, other->second);
					++it;
				}
			}
			if (merged != found->second) {
				found->second = std::move(merged);
				pending.push_back(next);
			}
		}
	}
	// Emit diagnostics and resolve script offsets only after the fixed point.
	for (auto* block : order) {
		auto state = incoming[block];
		for (const auto& action : block->array_actions) {
			action(state, true);
		}
	}
}

Uc_array_info Uc_var_expression::array_info(const Uc_array_state& state) const {
	auto found = state.find(Array_variable(var));
	return found != state.end() ? found->second : Uc_array_info{};
}

Uc_array_info Uc_binary_expression::array_info(const Uc_array_state& state) const {
	if (opcode == UC_ARRA) {
		return Uc_array_info::concat(left->array_info(state), right->array_info(state));
	}
	if (opcode == UC_ADD) {
		// Addition preserves the other operand when one value is undefined.
		// Unless a scalar is known, an unknown operand can therefore allow an
		// array to pass through unchanged.
		const auto l = left->array_info(state);
		const auto r = right->array_info(state);
		if (l.kind != Uc_array_info::Kind::scalar && r.kind != Uc_array_info::Kind::scalar
			&& (l.kind == Uc_array_info::Kind::unknown || r.kind == Uc_array_info::Kind::unknown)) {
			return {};
		}
	}
	return Uc_array_info::scalar();
}

bool Uc_binary_expression::split_script_repeat(Uc_expression*& count, Uc_expression*& body, Uc_location& location) {
	if (opcode != UC_MUL) {
		return false;
	}
	count = left;
	body  = right;
	location = right_location.value_or(static_cast<const Uc_location&>(*right));
	return true;
}

std::optional<int> Uc_repeated_script_expression::script_slots(const Uc_array_state& state) const {
	auto size = body->script_slots(state);
	if (!size || *size > std::numeric_limits<int>::max() - tail_size) {
		return {};
	}
	return *size + tail_size;
}

void Uc_repeated_script_expression::gen_value(Basic_block* out) {
	// The left operand is evaluated before calls in the repeat tail. Preserve
	// its proven length even if those calls invalidate facts about shared vars.
	offset->capture_state(out);
	Uc_binary_expression::gen_value(out);
}

Uc_array_info Uc_array_expression::array_info(const Uc_array_state& state) const {
	Uc_array_info result{Uc_array_info::Kind::array, 0};
	result.contains_repeat = contains_repeat;
	for (const auto* expr : exprs) {
		if (expr) {
			const auto info = expr->array_info(state);
			result          = Uc_array_info::concat(result, info);
		}
	}
	// An ordinary array literal does not acquire script identity from its elements.
	result.kind = Uc_array_info::Kind::array;
	return result;
}

std::optional<int> Uc_array_expression::script_slots(const Uc_array_state& state) const {
	int size = 0;
	for (const auto* expr : exprs) {
		if (!expr) {
			continue;
		}
		auto slots = expr->script_slots(state);
		if (!slots || size > std::numeric_limits<int>::max() - *slots) {
			return {};
		}
		size += *slots;
	}
	return size;
}

Uc_array_info Uc_script_expression::array_info(const Uc_array_state& state) const {
	auto info = body->array_info(state);
	if (!expanded) {
		info.kind = Uc_array_info::Kind::script;
	}
	return info;
}

std::optional<int> Uc_script_expression::script_slots(const Uc_array_state& state) const {
	if (!expanded) {
		return body->script_slots(state);
	}
	auto info = body->array_info(state);
	return info.is_script() ? info.length : std::optional<int>(0);
}

void Uc_script_expression::gen_value(Basic_block* out) {
	if (expanded) {
		out->array_actions.emplace_back([this](Uc_array_state& state, bool diagnose) {
			if (!diagnose) {
				return;
			}
			const auto info = body->array_info(state);
			if (!info.is_script()) {
				error("script dereference requires a value tracked as a script");
			} else if (info.contains_repeat) {
				error("cannot dereference a script containing a repeat inside another repeat");
			}
		});
	}
	body->gen_value(out);
}

static void Check_script_concat(const Uc_location& loc, Uc_array_info left, Uc_array_info right) {
	if (left.is_script() != right.is_script()) {
		loc.warning(Uc_location::Warning::script_mismatch, "concatenating a script with a value not tracked as a script");
	}
}

Uc_array_info Uc_append_expression::array_info(const Uc_array_state& state) const {
	auto result = left->array_info(state);
	for (const auto* expr : right->get_exprs()) {
		result = Uc_array_info::concat(result, expr->array_info(state));
	}
	return result;
}

void Uc_append_expression::gen_value(Basic_block* out) {
	out->array_actions.emplace_back([this](Uc_array_state& state, bool diagnose) {
		if (!diagnose) {
			return;
		}
		auto result = left->array_info(state);
		for (const auto* expr : right->get_exprs()) {
			auto info = expr->array_info(state);
			Check_script_concat(*this, result, info);
			result = Uc_array_info::concat(result, info);
		}
	});
	Uc_array_expression::gen_value(out);
}

void Uc_script_offset_expression::capture_state(Basic_block* out) {
	captured_state = std::make_shared<Uc_array_state>();
	out->array_actions.emplace_back([snapshot = captured_state](Uc_array_state& state, bool) {
		*snapshot = state;
	});
}

void Uc_script_offset_expression::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHI);
	WriteOpParam2(out, 0);
	auto* instruction = out->last_instruction();
	out->array_actions.emplace_back([this, instruction, snapshot = captured_state](Uc_array_state& state, bool diagnose) {
		if (!diagnose) {
			return;
		}
		int size = 0;
		for (const auto* expr : body) {
			auto slots = expr->script_slots(snapshot ? *snapshot : state);
			if (!slots) {
				expr->error("cannot repeat script: array length is unknown");
				return;
			}
			if (size > std::numeric_limits<int>::max() - *slots) {
				expr->error("script array length exceeds the supported range");
				return;
			}
			size += *slots;
		}
		instruction->set_integer(-size);
	});
}

/*
 *  Default.  Just push the one value.
 *
 *  Output: # pushed
 */

int Uc_expression::gen_values(Basic_block* out) {
	gen_value(out);    // Gen. result on stack.
	return 1;
}

/*
 *  Default assignment generation.
 */

void Uc_expression::gen_assign(Basic_block* out) {
	ignore_unused_variable_warning(out);
	error("expression is not assignable");
}

/*
 *  Need a variable whose value is this expression.
 */

Uc_var_symbol* Uc_expression::need_var(Basic_block* out, Uc_function* fun) {
	static int cnt = 0;
	char       buf[50];
	snprintf(buf, sizeof(buf), "_tmpval_%d", cnt++);
	// Create a 'tmp' variable.
	Uc_var_symbol* var = fun->add_symbol(buf, true);
	if (var == nullptr) {
		return nullptr;    // Shouldn't happen.  Err. reported.
	}
	gen_value(out);    // Want to assign this value to it.
	var->gen_assign(out);
	out->array_actions.emplace_back([this, var](Uc_array_state& state, bool) {
		state[Array_variable(var)] = array_info(state);
	});
	return var;
}

/*
 *  Concatenate expression into an array.
 */

void Uc_expression::add_to(Uc_array_expression* arr) {
	arr->add(this);
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_expression::eval_const(
		int& val    // Value returned here.
) {
	val = 0;
	return false;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_var_expression::gen_value(Basic_block* out) {
	if (!var->gen_value(out)) {
		error("'%s' cannot be used as a value", var->get_name());
	}
}

/*
 *  Generate assignment to this variable.
 */

void Uc_var_expression::gen_assign(Basic_block* out) {
	if (!var->gen_assign(out)) {
		error("cannot assign to '%s'", var->get_name());
	}
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_fun_name_expression::is_object_function(bool error) const {
	if (fun->get_function_type() != Uc_function_symbol::utility_fun) {
		return 0;
	}
	if (error) {
		Uc_location::error("function '%s' must be declared with 'shape#' or 'object#' for a script call", fun->get_name());
	}
	return 1;
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */
bool Uc_fun_name_expression::eval_const(int& val) {
	val = fun->get_usecode_num();
	return true;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_fun_name_expression::gen_value(Basic_block* out) {
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

void Uc_arrayelem_expression::gen_value(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);    // Want index on stack.
	WriteOp(out, UC_AIDX);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_arrayelem_expression::gen_assign(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);      // Want index on stack.
	WriteOp(out, UC_POPARR);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
	track_assignment(out);
}

void Uc_arrayelem_expression::track_assignment(Basic_block* out) {
	out->array_actions.emplace_back([this](Uc_array_state& state, bool) {
		auto& info = state[Array_variable(array)];
		int   position;
		if (!index->eval_const(position) || position > 32767 || position < -32768) {
			info.length.reset();
		} else if (position <= 0) {
			return;    // The VM ignores writes at nonpositive indices.
		} else if (info.length) {
			info.length = std::max(*info.length, position);
		}
		if (!info.is_script()) {
			info.kind = Uc_array_info::Kind::array;
		}
	});
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_static_arrayelem_expression::gen_value(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);     // Want index on stack.
	WriteOp(out, UC_AIDXS);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_static_arrayelem_expression::gen_assign(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);       // Want index on stack.
	WriteOp(out, UC_POPARRS);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
	track_assignment(out);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_class_arrayelem_expression::gen_value(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);       // Want index on stack.
	WriteOp(out, UC_AIDXTHV);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
}

/*
 *  Generate assignment to this variable.
 */

void Uc_class_arrayelem_expression::gen_assign(Basic_block* out) {
	if (!index || !array) {
		return;
	}
	index->gen_value(out);         // Want index on stack.
	WriteOp(out, UC_POPARRTHV);    // Opcode, var #.
	WriteOpParam2(out, array->get_offset());
	track_assignment(out);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_flag_expression::gen_value(Basic_block* out) {
	int ival;
	if (flag->eval_const(ival)) {
		WriteOp(out, UC_PUSHF);    // Opcode, flag #.
		WriteOpParam2(out, ival);
	} else {
		flag->gen_value(out);
		WriteOp(out, UC_PUSHFVAR);    // Opcode
	}
}

/*
 *  Generate assignment to this variable.
 */

void Uc_flag_expression::gen_assign(Basic_block* out) {
	int ival;
	if (flag->eval_const(ival)) {
		WriteOp(out, UC_POPF);    // Opcode, flag #.
		WriteOpParam2(out, ival);
	} else {
		flag->gen_value(out);
		WriteOp(out, UC_POPFVAR);    // Opcode
	}
}

inline bool Uc_var_expression::is_struct() const {
	return var->get_struct() != nullptr;
}

inline Uc_struct_symbol* Uc_var_expression::get_struct() const {
	return var->get_struct();
}

inline int Uc_var_expression::is_object_function(bool error) const {
	Uc_location::Context diagnostic_context(*this);
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

int Uc_var_expression::get_string_offset() {
	return var->get_string_offset();
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_binary_expression::gen_value(Basic_block* out) {
	if (opcode == UC_ARRA) {
		out->array_actions.emplace_back([this](Uc_array_state& state, bool diagnose) {
			if (diagnose) {
				Check_script_concat(*this, left->array_info(state), right->array_info(state));
			}
		});
	}
	int ival;
	if (eval_const(ival)) {
		auto* iexpr = new Uc_int_expression(ival, intop);
		iexpr->gen_value(out);
		delete iexpr;
	} else {
		left->gen_value(out);     // First the left.
		right->gen_value(out);    // Then the right.
		WriteOp(out, opcode);
	}
}

/*
 *  Evaluate constant.
 *
 *  Output: true if successful, with result returned in 'val'.
 */

bool Uc_binary_expression::eval_const(
		int& val    // Value returned here.
) {
	int val1;
	int val2;    // Get each side.
	if (!left->eval_const(val1) || !right->eval_const(val2)) {
		val = 0;
		return false;
	}
	switch (opcode) {
	case UC_ADD:
		val = usecode_int32(static_cast<uint32_t>(val1) + static_cast<uint32_t>(val2));
		return true;
	case UC_SUB:
		val = usecode_int32(static_cast<uint32_t>(val1) - static_cast<uint32_t>(val2));
		return true;
	case UC_MUL:
		val = usecode_int32(static_cast<uint32_t>(val1) * static_cast<uint32_t>(val2));
		return true;
	case UC_DIV:
		if (!val2) {
			right->error("division by zero in constant expression");
			return false;
		}
		if (val1 == INT32_MIN && val2 == -1) {
			error("integer overflow in constant division");
			return false;
		}
		val = val1 / val2;
		return true;
	case UC_MOD:
		if (!val2) {
			right->error("division by zero in constant expression");
			return false;
		}
		if (val1 == INT32_MIN && val2 == -1) {
			error("integer overflow in constant remainder");
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
		error("operator is not supported in an integer constant expression");
		return false;
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_unary_expression::gen_value(Basic_block* out) {
	int ival;
	if (eval_const(ival)) {
		auto* iexpr = new Uc_int_expression(ival);
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
		int& val    // Value returned here.
) {
	int val1;    // Get each side.
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
		error("operator is not supported in an integer constant expression");
		return false;
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_int_expression::gen_value(Basic_block* out) {
	WriteOp(out, opcode);
	if (opcode == UC_PUSHB) {
		WriteOpParam1(out, value);
	} else if (opcode == UC_PUSHI) {
		WriteOpParam2(out, value);
	} else {
		WriteOpParam4(out, value);
	}
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_int_expression::is_object_function(bool error) const {
	if (value < 0) {
		if (error) {
			Uc_location::error("function ID %d is negative; script calls require a nonnegative ID", value);
		}
		return 2;
	} else if (value < 0x800) {
		return 0;    // This is always an object/shape function.
	}

	Uc_function_symbol* sym = Uc_function_symbol::search_num(value);
	if (!sym) {
		return -1;    // Can't determine.
	} else if (sym->get_function_type() == Uc_function_symbol::utility_fun) {
		if (error) {
			Uc_location::error(
					"function '%s' (ID %d) must be declared with 'shape#' or 'object#' for a script call", sym->get_name(), value);
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
		int& val    // Value returned here.
) {
	val = value;
	return true;
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_bool_expression::gen_value(Basic_block* out) {
	if (tf) {
		WriteOp(out, UC_PUSHTRUE);
	} else {
		WriteOp(out, UC_PUSHFALSE);
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_event_expression::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHEVENTID);
}

/*
 *  Generate assignment to this variable.
 */

void Uc_event_expression::gen_assign(Basic_block* out) {
	WriteOp(out, UC_POPEVENTID);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_item_expression::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHITEMREF);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_choice_expression::gen_value(Basic_block* out) {
	WriteOp(out, UC_PUSHCHOICE);
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_string_expression::gen_value(Basic_block* out) {
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

void Uc_string_prefix_expression::gen_value(Basic_block* out) {
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

int Uc_string_prefix_expression::get_string_offset() {
	if (offset < 0) {    // First time?
		// Look up & print errors.
		offset = fun->find_string_prefix(*this, prefix.c_str());
	}
	return offset;
}

/*
 *  Delete a list of expressions.
 */

Uc_array_expression::~Uc_array_expression() {
	for (auto* expr : exprs) {
		delete expr;
	}
}

/*
 *  Concatenate another expression, or its values if an array, onto this.
 *  If the expression is an array, it's deleted after its elements are
 *  taken.
 */

void Uc_array_expression::concat(Uc_expression* e) {
	e->add_to(this);
}

/*
 *  Concatenate array into another array, and destroys the current array
 *  is destroyed and should not be used after this function is called.
 */

void Uc_array_expression::add_to(Uc_array_expression* arr) {
	if (contains_repeat) {
		arr->mark_repeat();
	}
	arr->add(exprs);
	exprs.clear();    // Don't want to delete elements.
	delete this;      // But this array is history.
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_array_expression::gen_value(Basic_block* out) {
	const int actual = Uc_array_expression::gen_values(out);
	WriteOp(out, UC_ARRC);
	WriteOpParam2(out, actual);
}

/*
 *  Push all values onto the stack.  This is also called for parm. lists.
 *
 *  Output: # pushed
 */

int Uc_array_expression::gen_values(Basic_block* out) {
	int actual = 0;    // (Just to be safe.)
	// Push backwards, so #0 pops first.
	for (auto it = exprs.rbegin(); it != exprs.rend(); ++it) {
		Uc_expression* expr = *it;
		if (expr) {
			actual++;
			expr->gen_value(out);
		}
	}
	return actual;
}

inline bool Uc_call_expression::is_struct() const {
	return sym && sym->get_struct() != nullptr;
}

inline Uc_struct_symbol* Uc_call_expression::get_struct() const {
	return sym ? sym->get_struct() : nullptr;
}

inline bool Uc_call_expression::is_class() const {
	return sym && sym->get_cls() != nullptr;
}

inline Uc_class* Uc_call_expression::get_cls() const {
	return sym ? sym->get_cls() : nullptr;
}

/*
 *  Returns 1 if the integer corresponds to an object/shape function,
 *  0 if not or -1 if it was not possible to determine.
 */

int Uc_call_expression::is_object_function(bool error) const {
	auto* fun = dynamic_cast<Uc_intrinsic_symbol*>(sym);
	if (!fun) {
		return -1;    // Can't determine.
	}

	if (fun == Uc_function::get_get_usecode_fun()) {
		return 0;    // It is.
	} else if (fun == Uc_function::get_get_item_shape()) {
		// *Could* be, if not a high shape.
		// Let's say it is, but issue a warning.
		if (error && Uc_location::get_shapefun_warn()) {
			Uc_location::warning(
					Uc_location::Warning::shape_to_function, "a shape number is a function ID only below 0x400; use "
															 "UI_get_usecode_fun instead");
		}
		return -2;
	}
	// For now, no other intrinsics return a valid fun ID.
	if (error) {
		Uc_location::error("result of intrinsic '%s' cannot be used as a script function ID", fun->get_name());
	}
	return 3;
}

/*
 *  Generate code to check if the passed params are in the correct number
 *  and of the correct types.
 */

void Uc_call_expression::check_params() {
	auto* fun = dynamic_cast<Uc_function_symbol*>(sym);
	if (!fun) {
		// Intrinsics; do nothing for now.
		return;
	}
	const auto&  protoparms  = fun->get_parms();
	const auto&  callparms   = parms->get_exprs();
	const size_t ignore_this = fun->get_method_num() >= 0 ? 1 : 0;
	const size_t expected    = protoparms.size() - ignore_this;
	if (callparms.size() != expected) {
		error("%s arguments to '%s': expected %zu, got %zu", callparms.size() > expected ? "too many" : "too few", sym->get_name(),
			  expected, callparms.size());
		fun->note("function declared here");
		return;
	}
	for (size_t i = 0; i < expected; ++i) {
		Uc_expression* expr     = callparms[i];
		Uc_var_symbol* var      = protoparms[i + ignore_this];
		auto*          cls      = dynamic_cast<Uc_class_inst_symbol*>(var);
		bool           mismatch = false;
		if (expr->is_class()) {
			if (!cls) {
				expr->error(
						"argument %zu to '%s' requires 'var', but has class type '%s'", i + 1, sym->get_name(),
						expr->get_cls()->get_name());
				mismatch = true;
			} else if (!expr->get_cls()->is_class_compatible(cls->get_cls()->get_name())) {
				expr->error(
						"argument %zu to '%s' requires class '%s', but has class type '%s'", i + 1, sym->get_name(),
						cls->get_cls()->get_name(), expr->get_cls()->get_name());
				mismatch = true;
			}
		} else if (cls) {
			expr->error(
					"argument %zu to '%s' requires class '%s', but has type 'var'", i + 1, sym->get_name(),
					cls->get_cls()->get_name());
			mismatch = true;
		}
		if (mismatch) {
			var->note("passing argument to parameter '%s' here", var->get_name());
		}
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

static void Forget_shared_arrays(Uc_array_state& state, bool) {
	// Calls can change persistent variables and class members, but cannot
	// assign the caller's locals (Usecode values have value semantics).
	for (auto it = state.begin(); it != state.end();) {
		if (it->first->is_static() || it->first->get_sym_type() == Uc_symbol::Member_var) {
			it = state.erase(it);
		} else {
			++it;
		}
	}
}

void Uc_call_expression::gen_value(Basic_block* out) {
	Uc_location::Context diagnostic_context(*this);
	if (ind) {                                            // Indirect?
		const size_t parmcnt = parms->gen_values(out);    // Want to push parm. values.
		if (!itemref) {
			Uc_item_expression item;
			item.gen_value(out);
		} else {
			itemref->gen_value(out);
		}
		ind->gen_value(out);    // Function #.
		if (parmcnt) {
			WriteOp(out, UC_CALLINDEX);
			WriteOpParam1(out, parmcnt);
		} else {
			WriteOp(out, UC_CALLIND);
		}
		out->array_actions.emplace_back(Forget_shared_arrays);
		return;
	}
	if (!sym) {
		return;    // Already failed once.
	}
	if (!sym->gen_call(out, function, original, itemref, parms, return_value, meth_scope)) {
		error("called object '%s' is not a function or intrinsic", sym->get_name());
		sym = nullptr;    // Avoid repeating error if in loop.
	}
	out->array_actions.emplace_back(Forget_shared_arrays);
}

void Uc_class_expression::gen_value(Basic_block* out) {
	if (!var->gen_value(out)) {
		error("cannot assign to '%s'", var->get_name());
	}
}

inline Uc_class* Uc_class_expression::get_cls() const {
	return var->get_cls();
}

inline Uc_class* Uc_new_expression::get_cls() const {
	return var->get_cls();
}

void Uc_class_expression::gen_assign(Basic_block* out) {
	if (!var->gen_assign(out)) {
		error("cannot assign to '%s'", var->get_name());
	}
}

/*
 *  Ensure that the correct number of arguments are pushed to constructor.
 */
Uc_new_expression::Uc_new_expression(Uc_var_symbol* v, Uc_array_expression* p) : Uc_class_expression(v), parms(p) {
	Uc_class* cls          = var->get_cls();
	const int pushed_parms = parms->get_exprs().size();
	if (cls->get_num_vars() > pushed_parms) {
		const int missing = cls->get_num_vars() - pushed_parms;
		warning(Uc_location::Warning::constructor_arguments,
				"%d constructor argument%s missing for class '%s'; initializing remaining members to zero", missing,
				(missing > 1) ? "s" : "", cls->get_name());
	} else if (cls->get_num_vars() < pushed_parms) {
		error("too many constructor arguments for class '%s': expected %d, got %d", cls->get_name(), cls->get_num_vars(),
			  pushed_parms);
	}
	// Ensure that all data members get initialized.
	for (int i = pushed_parms; i < cls->get_num_vars(); i++) {
		parms->add(new Uc_int_expression(0));
	}
}

/*
 *  Generate code to evaluate expression and leave result on stack.
 */

void Uc_new_expression::gen_value(Basic_block* out) {
	(void)parms->gen_values(out);
	Uc_class* cls = var->get_cls();
	WriteOp(out, UC_CLSCREATE);
	WriteOpParam2(out, cls->get_num());
}

/*
 *  Generate code to delete class.
 */

void Uc_del_expression::gen_value(Basic_block* out) {
	cls->gen_value(out);
	WriteOp(out, UC_CLASSDEL);
}
