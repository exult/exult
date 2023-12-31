/**
 ** Ucstmt.cc - Usecode compiler statements.
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

#include "ignore_unused_variable_warning.h"
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cstdio>
#include <cassert>

#include "ucstmt.h"
#include "ucexpr.h"
#include "ucsym.h"
#include "opcodes.h"
#include "utils.h"
#include "ucfun.h"
#include "basic_block.h"

using std::vector;
using std::map;
using std::string;

int Uc_converse_statement::nest = 0;

/*
 *  Delete.
 */

Uc_block_statement::~Uc_block_statement(
) {
	// Delete all the statements.
	for (auto *statement : statements)
		delete statement;
}

/*
 *  Generate code.
 */

void Uc_block_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	for (auto *stmt : statements) {
		stmt->gen(fun, blocks, curr, end, labels, start, exit);
	}
}


/*
 *  Delete.
 */

Uc_assignment_statement::~Uc_assignment_statement(
) {
	delete target;
	delete value;
}


/*
 *  Generate code.
 */

void Uc_assignment_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, blocks, end, labels, start, exit);
	value->gen_value(curr);     // Get value on stack.
	target->gen_assign(curr);
}

/*
 *  Generate code.
 */

void Uc_if_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (if_stmt == nullptr && else_stmt == nullptr) // Optimize whole block away.
		return;
	// The basic block for the if code.
	auto *if_block = new Basic_block();
	// The basic block after the if/else blocks.
	auto *past_if = new Basic_block();
	blocks.push_back(if_block);
	int ival = 0;
	const bool const_expr = expr == nullptr || expr->eval_const(ival);
	if (expr == nullptr || (const_expr && ival == 0)) {
		// IF body unreachable except by GOTO statements.
		// Skip IF block.
		curr->set_targets(UC_JMP, past_if);
	} else if (const_expr && ival != 0) {
		// ELSE block unreachable except by GOTO statements.
		// Fall-through to IF block.
		curr->set_targets(UC_INVALID, if_block);
	} else {
		// Gen test code & JNE.
		expr->gen_value(curr);
		curr->set_targets(UC_JNE, if_block);
	}
	if (if_stmt != nullptr) {
		if_stmt->gen(fun, blocks, if_block, end, labels, start, exit);
	}
	if (else_stmt != nullptr) {
		// The basic block for the else code.
		auto *else_block = new Basic_block();
		blocks.push_back(else_block);
		if (expr == nullptr) {  // Go directly to else block instead.
			curr->set_taken(else_block);
		} else if (ival == 0) { // Only for JNE.
			curr->set_ntaken(else_block);
		}
		// JMP past ELSE code.
		if_block->set_targets(UC_JMP, past_if);
		// Generate else code.
		else_stmt->gen(fun, blocks, else_block, end, labels, start, exit);
		else_block->set_taken(past_if);
	} else {
		if (!const_expr) {    // Need to go to past-if block too.
			curr->set_ntaken(past_if);
		}
		if_block->set_targets(UC_INVALID, past_if);
	}
	blocks.push_back(curr = past_if);
}

/*
 *  Delete.
 */

Uc_if_statement::~Uc_if_statement(
) {
	delete expr;
	delete if_stmt;
	delete else_stmt;
}

/*
 *  Generate code.
 */

void Uc_trycatch_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	static int cnt = 0;
	if (!try_stmt) // Optimize whole block away.
		return;
	// The basic block for the try code.
	auto *try_block = new Basic_block();
	// The basic block for the catch code.
	auto *catch_block = new Basic_block();
	// The basic block after the try/catch blocks.
	auto *past_trycatch = new Basic_block();
	// Gen start opcode for try block.
	curr->set_targets(UC_TRYSTART, try_block, catch_block);
	// Generate code for try block
	blocks.push_back(try_block);
	try_stmt->gen(fun, blocks, try_block, end, labels, start, exit);
	WriteOp(try_block, UC_TRYEND);
	// JMP past CATCH code.
	try_block->set_targets(UC_JMP, past_trycatch);
	// Generate a temp variable for error if needed
	if (!catch_var) {
		char buf[50];
		snprintf(buf, sizeof(buf), "_tmperror_%d", cnt++);
		// Create a 'tmp' variable.
		catch_var = fun->add_symbol(buf, true);
		assert(catch_var != nullptr);
	}
	// Generate error variable assignment (push is handled by abort/throw)
	blocks.push_back(catch_block);
	catch_var->gen_assign(catch_block);
	// Do we have anything else to generate on catch block?
	if (catch_stmt) {
		// Generate catch code.
		catch_stmt->gen(fun, blocks, catch_block, end, labels, start, exit);
		catch_block->set_taken(past_trycatch);
	}
	blocks.push_back(curr = past_trycatch);
}

/*
 *  Delete.
 */

Uc_trycatch_statement::~Uc_trycatch_statement(
) {
	delete catch_var;
	delete try_stmt;
	delete catch_stmt;
}

/*
 *  Delete.
 */

Uc_breakable_statement::~Uc_breakable_statement(
) {
	delete stmt;
}

/*
 *  Generate code.
 */

void Uc_breakable_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (stmt == nullptr && nobreak == nullptr) {
		// Optimize whole statement away.
		return;
	}
	// Basic block for no-break statements
	auto* nobreak_block = new Basic_block();
	// Basic block past statement body.
	auto* past_block = new Basic_block();
	if (stmt) {
		stmt->gen(fun, blocks, curr, end, labels, nobreak_block, past_block);
	}
	curr->set_taken(nobreak_block);

	// Generate no-break block.
	blocks.push_back(nobreak_block);
	if (nobreak != nullptr) {
		auto* next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		nobreak_block = next_block;
	}
	// Fallthrough from no-break block to past-while (break) block
	nobreak_block->set_taken(past_block);
	blocks.push_back(curr = past_block);
}

/*
 *  Delete.
 */

Uc_while_statement::~Uc_while_statement(
) {
	delete expr;
	delete stmt;
}

/*
 *  Generate code.
 */

void Uc_while_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (stmt == nullptr && nobreak == nullptr) {
		// Optimize whole loop away.
		return;
	}
	// The start of a loop is a jump target and needs
	// a new basic block.
	auto *while_top = new Basic_block();
	// Need new block past a JNE (the test) or JMP.
	auto *while_block = new Basic_block();
	// Basic block for no-break statements
	auto* nobreak_block = new Basic_block();
	// Basic block past while body.
	auto *past_while = new Basic_block();
	// Fall-through to WHILE top.
	curr->set_taken(while_top);
	blocks.push_back(while_top);
	blocks.push_back(while_block);
	if (!expr)
		// While body unreachable except through GOTO statements.
		// Skip WHILE body by default.
		while_top->set_targets(UC_JMP, nobreak_block);
	else {
		// Gen test code.
		expr->gen_value(while_top);
		// Link WHILE top to WHILE body and past-WHILE blocks.
		while_top->set_targets(UC_JNE, while_block, nobreak_block);
	}
	// Generate while body.
	if (stmt != nullptr) {
		stmt->gen(fun, blocks, while_block, end, labels, while_top, past_while);
	}

	// JMP back to top.
	while_block->set_targets(UC_JMP, while_top);

	// Generate no-break block.
	blocks.push_back(nobreak_block);
	if (nobreak != nullptr) {
		auto* next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		nobreak_block = next_block;
	}
	// Fallthrough from no-break block to past-while (break) block
	nobreak_block->set_taken(past_while);
	blocks.push_back(curr = past_while);
}


/*
 *  Delete.
 */

Uc_dowhile_statement::~Uc_dowhile_statement(
) {
	delete expr;
	delete stmt;
}

/*
 *  Generate code.
 */

void Uc_dowhile_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (stmt == nullptr && nobreak == nullptr) {
		// Optimize whole loop away.
		return;
	}
	// The start of a loop is a jump target and needs
	// a new basic block.
	auto *do_block = new Basic_block();
	curr->set_taken(do_block);
	blocks.push_back(do_block);
	// Need new block for test as it is a jump target.
	auto *do_test = new Basic_block();
	// Gen test code.
	expr->gen_value(do_test);
	// Basic block for no-break statements
	auto* nobreak_block = new Basic_block();
	// Basic block past while body.
	auto *past_do = new Basic_block();
	// Jump back to top.
	auto *do_jmp = new Basic_block();
	do_jmp->set_targets(UC_JMP, do_block);
	do_test->set_targets(UC_JNE, do_jmp, nobreak_block);
	// Generate while body.
	if (stmt != nullptr) {
		stmt->gen(fun, blocks, do_block, end, labels, do_test, past_do);
	}
	do_block->set_targets(UC_INVALID, do_test);

	blocks.push_back(do_test);
	blocks.push_back(do_jmp);

	// Generate no-break block.
	blocks.push_back(nobreak_block);
	if (nobreak != nullptr) {
		auto* next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		nobreak_block = next_block;
	}
	// Fallthrough from no-break block to past-while (break) block
	nobreak_block->set_taken(past_do);
	blocks.push_back(curr = past_do);
}

/*
 *  Delete.
 */

Uc_infinite_loop_statement::~Uc_infinite_loop_statement(
) {
	delete stmt;
}

/*
 *  Generate code.
 */

void Uc_infinite_loop_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (stmt == nullptr && nobreak == nullptr) {
		// Optimize whole loop away.
		return;
	}
	const size_t num_end = end->predecessor_count();
	// The start of a loop is a jump target and needs
	// a new basic block.
	auto *loop_top = new Basic_block();
	curr->set_taken(loop_top);
	blocks.push_back(loop_top);
	// Local copy.
	Basic_block *loop_body = loop_top;
	// Basic block past loop body.
	auto *past_loop = new Basic_block();
	// Generate loop body.
	if (stmt) {
		stmt->gen(fun, blocks, loop_body, end, labels, loop_top, past_loop);
	}
	// Jump back to top.
	loop_body->set_targets(UC_JMP, loop_top);

	if (past_loop->is_orphan() && num_end == end->predecessor_count()) {
		// Need to do this *before* setting fallthrough for nobreak statements.
		error("Error: Infinite loop without any 'abort', 'break', or 'return' statements");
	}

	if (nobreak != nullptr) {
		// Basic block for no-break statements
		auto *nobreak_block = new Basic_block();
		blocks.push_back(nobreak_block);
		auto *next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		next_block->set_taken(past_loop);

		if (nobreak_block->is_orphan()) {
			if (next_block == nobreak_block) {
				warning("No statements in 'nobreak' block will never execute");
			} else {
				// There may be a label somewhere in the block.
				if (!nobreak_block->is_empty_block()) {
					warning("Some statements in 'nobreak' block will never execute");
				}
			}
		}
		nobreak_block = next_block;
	}

	blocks.push_back(curr = past_loop);
}

/*
 *  Generate code.
 */
void array_enum_statement::gen(
    Uc_function *fun,
    std::vector<Basic_block *> &blocks,
    Basic_block *&curr,
    Basic_block *end,
    std::map<std::string,
    Basic_block *> &labels,
    Basic_block *start,
    Basic_block *exit
) {
	ignore_unused_variable_warning(fun, blocks, end, labels, start, exit);
	// Start of loop.
	WriteOp(curr, UC_LOOP);
}

/*
 *  Finish up creation.
 */

void Uc_arrayloop_statement_base::finish(
    Uc_function *fun
) {
	char buf[100];
	if (index == nullptr) {       // Create vars. if not given.
		snprintf(buf, sizeof(buf), "_%s_index", array->get_name());
		index = fun->add_symbol(buf, true);
	}
	if (array_len == nullptr) {
		snprintf(buf, sizeof(buf), "_%s_size", array->get_name());
		array_len = fun->add_symbol(buf, true);
	}
}

/*
 *  Generate code.
 */

void Uc_arrayloop_statement_base::gen_check(
    Basic_block *for_top,
    Basic_block *loop_body,
    Basic_block *past_loop
) {
	UsecodeOps opcode;
	if (array->is_static()) {
		opcode = UC_LOOPTOPS;
	} else if (array->get_sym_type() == Uc_symbol::Member_var) {
		opcode = UC_LOOPTOPTHV;
	} else {
		opcode = UC_LOOPTOP;
	}
	for_top->set_targets(opcode, loop_body, past_loop);
	WriteJumpParam2(for_top, index->get_offset());  // Counter, total-count variables.
	WriteJumpParam2(for_top, array_len->get_offset());
	WriteJumpParam2(for_top, var->get_offset());    // Loop variable, than array.
	WriteJumpParam2(for_top, array->get_offset());
}

/*
 *  Delete.
 */

Uc_arrayloop_statement::~Uc_arrayloop_statement(
) {
	delete stmt;
}

/*
 *  Generate code.
 */

void Uc_arrayloop_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (stmt == nullptr && nobreak == nullptr) {
		// Nothing useful to do.
		return;
	}
	// Start of loop.
	WriteOp(curr, UC_LOOP);
	// The start of a loop is a jump target and needs
	// a new basic block.
	auto *for_top = new Basic_block();
	curr->set_taken(for_top);
	blocks.push_back(for_top);
	// Body of FOR loop.
	auto *for_body = new Basic_block();
	blocks.push_back(for_body);
	// Basic block for no-break statements
	auto* nobreak_block = new Basic_block();
	// Block immediately after FOR.
	auto *past_for = new Basic_block();
	gen_check(for_top, for_body, nobreak_block);

	// Generate FOR body.
	if (stmt != nullptr) {
		stmt->gen(fun, blocks, for_body, end, labels, for_top, past_for);
	}
	// Jump back to top.
	for_body->set_targets(UC_JMP, for_top);

	// Generate no-break block.
	blocks.push_back(nobreak_block);
	if (nobreak != nullptr) {
		auto* next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		nobreak_block = next_block;
	}
	// Fallthrough from no-break block to past-while (break) block
	nobreak_block->set_taken(past_for);

	blocks.push_back(curr = past_for);
}

/*
 *  Generate code.
 */

void Uc_arrayloop_attend_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, start, exit);
	auto it = labels.find(label);
	if (it == labels.end()) {
		char buf[255];
		snprintf(buf, sizeof(buf), "Undeclared label: '%s'", label.c_str());
		error(buf);
		return;
	}
	// The start of a loop is a jump target and needs
	// a new basic block.
	auto *for_top = new Basic_block();
	curr->set_taken(for_top);
	blocks.push_back(for_top);
	// Body of FOR loop.
	auto *for_body = new Basic_block();
	// Block immediately after FOR.
	auto *past_for = it->second;
	gen_check(for_top, for_body, past_for);
	blocks.push_back(curr = for_body);
}

/*
 *  Delete.
 */

Uc_return_statement::~Uc_return_statement(
) {
	delete expr;
}

/*
 *  Generate code.
 */

void Uc_return_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, labels, start, exit);
	if (expr) {         // Returning something?
		int ival;
		if (expr->eval_const(ival) && !ival)
			WriteOp(curr, UC_RETZ);
		else {
			expr->gen_value(curr);  // Put value on stack.
			WriteOp(curr, UC_RETV);
		}
	} else
		WriteOp(curr, UC_RET);
	curr->set_targets(UC_INVALID, end);
	curr = new Basic_block();
	blocks.push_back(curr);
}


/*
 *  Generate code.
 */

void Uc_break_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, labels, start);
	curr->set_targets(UC_JMP, exit);
	curr = new Basic_block();
	blocks.push_back(curr);
}


void Uc_continue_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, labels, exit);
	curr->set_targets(UC_JMP, start);
	curr = new Basic_block();
	blocks.push_back(curr);
}

void Uc_label_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, start, exit);
	auto it = labels.find(label);
	// Should never fail, but...
	assert(it != labels.end());
	Basic_block *block = it->second;
	assert(block != nullptr);
	if (!curr->is_jump_block() && !curr->ends_in_return() && !curr->ends_in_abort()) {
		curr->set_taken(block);
	}
	blocks.push_back(curr = block);

	// no code
}


void Uc_goto_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, start, exit);
	auto it = labels.find(label);
	if (it != labels.end()) {
		Basic_block *l = it->second;
		curr->set_targets(UC_JMP, l);
		curr = new Basic_block();
		blocks.push_back(curr);
	} else {
		char buf[255];
		snprintf(buf, sizeof(buf), "Undeclared label: '%s'", label.c_str());
		error(buf);
	}
}

/*
 *  Generate a call to an intrinsic with 0 or 1 parameter.
 */
static void Call_intrinsic(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Uc_intrinsic_symbol *intr,  // What to call.
    Uc_expression *parm0 = nullptr    // Parm, or null.
) {
	// Create parms. list.
	auto *parms = new Uc_array_expression;
	if (parm0)
		parms->add(parm0);
	auto *fcall = new Uc_call_expression(intr, parms, fun);
	Uc_call_statement fstmt(fcall);
	fstmt.gen(fun, blocks, curr, end, labels);
	parms->clear();         // DON'T want to delete parm0.
}

/*
 *  Generate code.
 */
void Uc_endconv_statement::gen(
    Uc_function *fun,
    std::vector<Basic_block *> &blocks,
    Basic_block *&curr,
    Basic_block *end,
    std::map<std::string,
    Basic_block *> &labels,
    Basic_block *start,
    Basic_block *exit
) {
	ignore_unused_variable_warning(fun, blocks, end, labels, start, exit);
	WriteOp(curr, UC_CONVERSELOC);
}

/*
 *  Generate code.
 */

void Uc_converse_case_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (!remove && statements == nullptr) {
		// Optimize whole case away.
		return;
	}

	// New basic block for CASE body.
	auto *case_body = new Basic_block();
	blocks.push_back(case_body);
	// Past CASE body.
	auto *past_case = new Basic_block();

	gen_check(curr, case_body, past_case);

	if (remove) {    // Remove answer?
		gen_remove(fun, blocks, case_body, end, labels);
	}
	if (statements != nullptr) {
		// Generate statement's code.
		statements->gen(fun, blocks, case_body, end, labels, start, exit);
	}
	// Jump back to converse top.
	if (!falls_through()) {
		case_body->set_targets(UC_JMP, start);
	} else {
		case_body->set_taken(past_case);
	}
	blocks.push_back(curr = past_case);
}

/*
 *  Generate code.
 */

int Uc_converse_strings_case_statement::gen_check(
    Basic_block *curr,         // Active block; where we write the check
    Basic_block *case_body,    // Case body, for check fallthrough
    Basic_block *past_case     // Block after the case statement
) {
	for (auto it = string_offset.rbegin();
			it != string_offset.rend(); ++it) {
		// Push strings on stack; *it should always be >= 0.
		if (is_int_32bit(*it)) {
			WriteOp(curr, UC_PUSHS32);
			WriteOpParam4(curr, *it);
		} else {
			WriteOp(curr, UC_PUSHS);
			WriteOpParam2(curr, *it);
		}
	}
	// # strings on stack.
	curr->set_targets(UC_CMPS, case_body, past_case);
	WriteJumpParam2(curr, string_offset.size());
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_strings_case_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	if (string_offset.size() > 1) {
		auto *strlist = new Uc_array_expression();
		for (const int it : string_offset) {
			auto *str = new Uc_string_expression(it);
			strlist->add(str);
		}
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(), strlist);
	} else if (!string_offset.empty()) {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(),
		          new Uc_string_expression(string_offset[0]));
	} else {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(),
		          new Uc_choice_expression());
	}
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_variable_case_statement::gen_check(
		Basic_block *curr,         // Active block; where we write the check
		Basic_block *case_body,    // Case body, for check fallthrough
		Basic_block *past_case     // Block after the case statement
) {
	if (variable != nullptr) {
		variable->gen_value(curr);
		// # strings on stack.
		curr->set_targets(UC_CMPS, case_body, past_case);
		WriteJumpParam2(curr, 1);
		return 1;
	}
	return 0;
}

/*
 *  Generate code.
 */

int Uc_converse_variable_case_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	if (variable != nullptr) {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		         Uc_function::get_remove_answer(),
		         variable);
		return 1;
	}
	Call_intrinsic(fun, blocks, case_body, end, labels,
	          Uc_function::get_remove_answer(),
	          new Uc_choice_expression());
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_default_case_statement::gen_check(
    Basic_block *curr,         // Active block; where we write the check
    Basic_block *case_body,    // Case body, for check fallthrough
    Basic_block *past_case     // Block after the case statement
) {
	// Writing 1 to match audition usecode, but this could be anything.
	curr->set_targets(UC_DEFAULT, case_body, past_case);
	WriteJumpParam2(curr, 1);
	return 1;
}

int Uc_converse_default_case_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	ignore_unused_variable_warning(fun, blocks, case_body, end, labels);
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_always_case_statement::gen_check(
    Basic_block *curr,         // Active block; where we write the check
    Basic_block *case_body,    // Case body, for check fallthrough
    Basic_block *past_case     // Block after the case statement
) {
	ignore_unused_variable_warning(past_case);
	curr->set_taken(case_body);
	return 1;
}

int Uc_converse_always_case_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	ignore_unused_variable_warning(fun, blocks, case_body, end, labels);
	return 1;
}

/*
 *  Generate code.
 */

void Uc_converse_case_attend_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(start, exit);
	auto it = labels.find(label);
	if (it == labels.end()) {
		char buf[255];
		snprintf(buf, sizeof(buf), "Undeclared label: '%s'", label.c_str());
		error(buf);
		return;
	}
	// New basic block for CASE body.
	auto *case_body = new Basic_block();
	// Past CASE body.
	auto *past_case = it->second;
	gen_check(curr, case_body, past_case);

	if (remove) {    // Remove answer?
		gen_remove(fun, blocks, case_body, end, labels);
	}
	blocks.push_back(curr = case_body);
}

/*
 *  Generate code.
 */

int Uc_converse_strings_case_attend_statement::gen_check(
    Basic_block *curr,         // Active block; where we write the check
    Basic_block *case_body,    // Case body, for check fallthrough
    Basic_block *past_case     // Block after the case statement
) {
	for (auto it = string_offset.rbegin();
			it != string_offset.rend(); ++it) {
		// Push strings on stack; *it should always be >= 0.
		if (is_int_32bit(*it)) {
			WriteOp(curr, UC_PUSHS32);
			WriteOpParam4(curr, *it);
		} else {
			WriteOp(curr, UC_PUSHS);
			WriteOpParam2(curr, *it);
		}
	}
	// # strings on stack.
	curr->set_targets(UC_CMPS, case_body, past_case);
	WriteJumpParam2(curr, string_offset.size());
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_strings_case_attend_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	if (string_offset.size() > 1) {
		auto *strlist = new Uc_array_expression();
		for (const int it : string_offset) {
			auto *str = new Uc_string_expression(it);
			strlist->add(str);
		}
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(), strlist);
	} else if (!string_offset.empty()) {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(),
		          new Uc_string_expression(string_offset[0]));
	} else {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		          Uc_function::get_remove_answer(),
		          new Uc_choice_expression());
	}
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_variable_case_attend_statement::gen_check(
		Basic_block *curr,         // Active block; where we write the check
		Basic_block *case_body,    // Case body, for check fallthrough
		Basic_block *past_case     // Block after the case statement
) {
	if (variable != nullptr) {
		variable->gen_value(curr);
		// # strings on stack.
		curr->set_targets(UC_CMPS, case_body, past_case);
		WriteJumpParam2(curr, 1);
		return 1;
	}
	return 0;
}

/*
 *  Generate code.
 */

int Uc_converse_variable_case_attend_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	if (variable != nullptr) {
		Call_intrinsic(fun, blocks, case_body, end, labels,
		         Uc_function::get_remove_answer(),
		         variable);
		return 1;
	}
	Call_intrinsic(fun, blocks, case_body, end, labels,
	          Uc_function::get_remove_answer(),
	          new Uc_choice_expression());
	return 1;
}

/*
 *  Generate code.
 */

int Uc_converse_default_case_attend_statement::gen_check(
    Basic_block *curr,         // Active block; where we write the check
    Basic_block *case_body,    // Case body, for check fallthrough
    Basic_block *past_case     // Block after the case statement
) {
	// Writing 1 to match audition usecode, but this could be anything.
	curr->set_targets(UC_DEFAULT, case_body, past_case);
	WriteJumpParam2(curr, value);
	return 1;
}

int Uc_converse_default_case_attend_statement::gen_remove(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *case_body,             // Active block; will usually be *changed*.
    Basic_block *end,                   // Fictitious exit block for function.
    map<string, Basic_block *> &labels  // Label map for goto statements.
) {
	ignore_unused_variable_warning(fun, blocks, case_body, end, labels);
	return 1;
}

/*
 *  Initialize.
 */

Uc_converse_statement::Uc_converse_statement(
    Uc_expression *a,
    Uc_block_statement *p,
    std::vector<Uc_statement *> *cs,
    bool n,
    Uc_statement *nb
) : answers(a), preamble(p), nobreak(nb), cases(*cs), nestconv(n) {
}

/*
 *  Delete.
 */

Uc_converse_statement::~Uc_converse_statement(
) {
	delete answers;
	for (auto *it : cases)
		delete it;
}

/*
 *  Generate code.
 */

void Uc_converse_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	if (cases.empty() && nobreak == nullptr && preamble == nullptr) {
		// Nothing to do; optimize whole block away.
		return;
	}
	if (nest++ > 0 || nestconv) {
		// Not the outermost?
		// Generate a 'push_answers()'.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_push_answers());
	}
	if (answers != nullptr) {            // Add desired answers.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_add_answer(), answers);
	}
	// The start of a CONVERSE loop is a jump target and needs
	// a new basic block.
	auto *conv_top = new Basic_block();
	curr->set_taken(conv_top);
	blocks.push_back(conv_top);
	// Need new block as it is past a jump.
	auto *conv_body = new Basic_block();
	blocks.push_back(conv_body);
	// Basic block for no-break statements
	auto* nobreak_block = new Basic_block();
	// Block past the CONVERSE loop.
	auto *past_conv = new Basic_block();
	WriteOp(past_conv, UC_CONVERSELOC);
	conv_top->set_targets(UC_CONVERSE, conv_body, nobreak_block);
	// Generate loop body.
	if (preamble != nullptr) {
		preamble->gen(fun, blocks, conv_body, end, labels, conv_top, past_conv);
	}
	for (auto *it : cases) {
		it->gen(fun, blocks, conv_body, end, labels, conv_top, past_conv);
	}
	// Jump back to top.
	conv_body->set_targets(UC_JMP, conv_top);

	// Generate no-break block.
	blocks.push_back(nobreak_block);
	if (nobreak != nullptr) {
		auto* next_block = nobreak_block;
		nobreak->gen(fun, blocks, next_block, end, labels, start, exit);
		nobreak_block = next_block;
	}
	// Fallthrough from no-break block to past-while (break) block
	nobreak_block->set_taken(past_conv);
	blocks.push_back(curr = past_conv);

	if (--nest > 0 || nestconv) {
		// Not the outermost?
		// Generate a 'pop_answers()'.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_pop_answers());
	}
}

/*
 *  Initialize.
 */

Uc_converse_attend_statement::Uc_converse_attend_statement(
    std::string target
) : label(std::move(target)) {
}

/*
 *  Generate code.
 */

void Uc_converse_attend_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, end, start, exit);
	// The start of a CONVERSE loop is a jump target and needs
	// a new basic block.
	auto it = labels.find(label);
	if (it == labels.end()) {
		char buf[255];
		snprintf(buf, sizeof(buf), "Undeclared label: '%s'", label.c_str());
		error(buf);
		return;
	}
	auto *conv_top = new Basic_block();
	// Fallthrough from curr to conv_top.
	curr->set_taken(conv_top);
	blocks.push_back(conv_top);
	auto *conv_body = new Basic_block();
	Basic_block *past_conv = it->second;
	// Branch to label or fallthrough to body.
	curr->set_targets(UC_CONVERSE, conv_body, past_conv);
	// Add body and make it current basic block.
	blocks.push_back(curr = conv_body);
}

/*
 *  Delete.
 */

Uc_switch_expression_case_statement::~Uc_switch_expression_case_statement(
) {
	delete check;
}

/*
 *  Generate code.
 */

int Uc_switch_expression_case_statement::gen_check(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *case_block     // Pointer to the case statements.
) {
	ignore_unused_variable_warning(fun, end, labels);
	check->gen_value(curr);
	WriteOp(curr, UC_CMPNE);
	auto *block = new Basic_block();
	curr->set_targets(UC_JNE, block, case_block);
	blocks.push_back(curr = block);
	return 1;
}

/*
 *  Generate code.
 */

void Uc_switch_case_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	statements->gen(fun, blocks, curr, end, labels, start, exit);
}

/*
 *  Delete.
 */

Uc_switch_statement::~Uc_switch_statement(
) {
	delete cond;
	for (auto *it : cases)
		delete it;
}

/*
 *  Initialize.
 */

Uc_switch_statement::Uc_switch_statement(
    Uc_expression *v,
    std::vector<Uc_statement *> *cs
)
	: cond(v), cases(*cs) {
	bool has_default = false;
	for (auto *it : cases) {
		auto *stmt =
		    static_cast<Uc_switch_case_statement *>(it);
		if (stmt->is_default()) {
			if (has_default) {
				char buf[255];
				snprintf(buf, sizeof(buf), "switch statement already has a default case.");
				error(buf);
			} else
				has_default = true;
		}
	}
}

/*
 *  Generate code.
 */

void Uc_switch_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(exit);
	auto *var = new Uc_var_expression(cond->need_var(curr, fun));
	vector<Basic_block *> case_blocks;
	Basic_block *def_case = nullptr;
	for (auto *it : cases) {
		auto *stmt =
		    static_cast<Uc_switch_case_statement *>(it);
		if (stmt->is_default()) { // Store the default case iterator.
			def_case = new Basic_block();
			case_blocks.push_back(def_case);
		} else {
			// Generate the conditional jumps and the case basic blocks.
			var->gen_value(curr);
			// Case block.
			auto *case_code = new Basic_block();
			stmt->gen_check(fun, blocks, curr, end, labels, case_code);
			case_blocks.push_back(case_code);
		}
	}
	// All done with it.
	delete var;
	// Past SWITCH block.
	auto *past_switch = new Basic_block();
	// For all other cases, the default jump.
	if (def_case)
		curr->set_targets(UC_JMP, def_case);
	else
		curr->set_targets(UC_JMP, past_switch);
	for (size_t i = 0; i < cases.size(); i++) {
		auto *stmt =
		    static_cast<Uc_switch_case_statement *>(cases[i]);
		Basic_block *block = case_blocks[i];
		// Link cases (for fall-through).
		if (i > 0)
			curr->set_targets(UC_INVALID, block);
		blocks.push_back(curr = block);
		stmt->gen(fun, blocks, curr, end, labels, start, past_switch);
	}
	curr->set_targets(UC_INVALID, past_switch);
	blocks.push_back(curr = past_switch);
}

/*
 *  Generate code.
 */

void Uc_message_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(blocks, end, labels, start, exit);
	if (!msgs)
		return;
	const std::vector<Uc_expression *> &exprs = msgs->get_exprs();
	for (auto *msg : exprs) {
			// A known string?
		const int offset = msg->get_string_offset();
		if (offset >= 0) {
			if (is_int_32bit(offset)) {
				WriteOp(curr, UC_ADDSI32);
				WriteOpParam4(curr, offset);
			} else {
				WriteOp(curr, UC_ADDSI);
				WriteOpParam2(curr, offset);
			}
		} else {
			Uc_var_symbol *var = msg->need_var(curr, fun);
			if (var) {  // Shouldn't fail.
				WriteOp(curr, UC_ADDSV);
				WriteOpParam2(curr, var->get_offset());
			}
		}
	}
}

/*
 *  Generate code.
 */

void Uc_say_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	// Add the messages.
	Uc_message_statement::gen(fun, blocks, curr, end, labels, start, exit);
	WriteOp(curr, UC_SAY);      // Show on screen.
}

/*
 *  Create.
 */

Uc_call_statement::Uc_call_statement(
    Uc_call_expression *f
) : function_call(f) {
	if (function_call)
		function_call->set_no_return();
}

/*
 *  Delete.
 */

Uc_call_statement::~Uc_call_statement(
) {
	delete function_call;
}

/*
 *  Generate code.
 */

void Uc_call_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, blocks, end, labels, start, exit);
	function_call->gen_value(curr); // (We set 'no_return'.)
}

/*
 *  Generate code.
 */

void Uc_abort_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, labels, start, exit);
	if (expr) {
		expr->gen_value(curr);
		WriteOp(curr, UC_THROW);
	} else {
		WriteOp(curr, UC_ABRT);
	}
	curr->set_targets(UC_INVALID, end);
	curr = new Basic_block();
	blocks.push_back(curr);
}

/*
 *  Delete.
 */

Uc_abort_statement::~Uc_abort_statement(
) {
	delete expr;
}

/*
 *  Generate code.
 */

void Uc_delete_statement::gen(
    Uc_function *fun,
    vector<Basic_block *> &blocks,      // What we are producing.
    Basic_block *&curr,         // Active block; will usually be *changed*.
    Basic_block *end,           // Fictitious exit block for function.
    map<string, Basic_block *> &labels, // Label map for goto statements.
    Basic_block *start,         // Block used for 'continue' statements.
    Basic_block *exit           // Block used for 'break' statements.
) {
	ignore_unused_variable_warning(fun, blocks, end, labels, start, exit);
	if (!expr)
		return;
	expr->gen_value(curr);
}
