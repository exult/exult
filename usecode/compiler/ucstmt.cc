/**
 ** Ucstmt.cc - Usecode compiler statements.
 **
 ** Written: 1/2/01 - JSF
 **/

/*
Copyright (C) 2000-2013 The Exult Team

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
	for (std::vector<Uc_statement *>::const_iterator it = statements.begin();
	        it != statements.end(); ++it)
		delete(*it);
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
	for (std::vector<Uc_statement *>::const_iterator it = statements.begin();
	        it != statements.end(); ++it) {
		Uc_statement *stmt = *it;
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
	if (!if_stmt && !else_stmt) // Optimize whole block away.
		return;
	// The basic block for the if code.
	Basic_block *if_block = new Basic_block();
	// The basic block after the if/else blocks.
	Basic_block *past_if = new Basic_block();
	blocks.push_back(if_block);
	int ival;
	bool const_expr = !expr || expr->eval_const(ival);
	if (!expr)
		// IF body unreachable except by GOTO statements.
		// Skip IF block.
		curr->set_targets(UC_JMP, past_if);
	else if (ival)
		// ELSE block unreachable except by GOTO statements.
		// Fall-through to IF block.
		curr->set_targets(UC_INVALID, if_block);
	else {
		// Gen test code & JNE.
		expr->gen_value(curr);
		curr->set_targets(UC_JNE, if_block);
	}
	if (if_stmt)
		if_stmt->gen(fun, blocks, if_block, end, labels, start, exit);
	if (else_stmt) {
		// The basic block for the else code.
		Basic_block *else_block = new Basic_block();
		blocks.push_back(else_block);
		if (!expr)  // Go directly to else block instead.
			curr->set_taken(else_block);
		else if (!ival) // Only for JNE.
			curr->set_ntaken(else_block);
		// JMP past ELSE code.
		if_block->set_targets(UC_JMP, past_if);
		// Generate else code.
		else_stmt->gen(fun, blocks, else_block, end, labels, start, exit);
		else_block->set_taken(past_if);
	} else {
		if (!const_expr)    // Need to go to past-if block too.
			curr->set_ntaken(past_if);
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
	Basic_block *try_block = new Basic_block();
	// The basic block for the catch code.
	Basic_block *catch_block = new Basic_block();
	// The basic block after the try/catch blocks.
	Basic_block *past_trycatch = new Basic_block();
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
		sprintf(buf, "_tmperror_%d", cnt++);
		// Create a 'tmp' variable.
		catch_var = fun->add_symbol(buf);
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
	ignore_unused_variable_warning(start, exit);
	if (!stmt)  // Optimize whole statement away.
		return;
	Basic_block *past_block = new Basic_block();
	stmt->gen(fun, blocks, curr, end, labels, past_block, past_block);
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
	ignore_unused_variable_warning(start, exit);
	if (!stmt)  // Optimize whole loop away.
		return;
	// The start of a loop is a jump target and needs
	// a new basic block.
	Basic_block *while_top = new Basic_block();
	// Basic block past while body.
	Basic_block *past_while = new Basic_block();
	// Need new block past a JNE (the test) or JMP.
	Basic_block *while_block = new Basic_block();
	// Fall-through to WHILE top.
	curr->set_taken(while_top);
	blocks.push_back(while_top);
	blocks.push_back(while_block);
	if (!expr)
		// While body unreachable except through GOTO statements.
		// Skip WHILE body by default.
		while_top->set_targets(UC_JMP, past_while);
	else {
		// Gen test code.
		expr->gen_value(while_top);
		// Link WHILE top to WHILE body and past-WHILE blocks.
		while_top->set_targets(UC_JNE, while_block, past_while);
	}
	// Generate while body.
	stmt->gen(fun, blocks, while_block, end, labels, while_top, past_while);

	// JMP back to top.
	while_block->set_targets(UC_JMP, while_top);
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
	ignore_unused_variable_warning(start, exit);
	if (!stmt)  // Optimize whole loop away.
		return;
	// The start of a loop is a jump target and needs
	// a new basic block.
	Basic_block *do_block = new Basic_block();
	curr->set_taken(do_block);
	blocks.push_back(do_block);
	// Need new block for test as it is a jump target.
	Basic_block *do_test = new Basic_block();
	// Gen test code.
	expr->gen_value(do_test);
	// Basic block past while body.
	Basic_block *past_do = new Basic_block();
	// Jump back to top.
	Basic_block *do_jmp = new Basic_block();
	do_jmp->set_targets(UC_JMP, do_block);
	do_test->set_targets(UC_JNE, do_jmp, past_do);
	// Generate while body.
	stmt->gen(fun, blocks, do_block, end, labels, do_test, past_do);
	do_block->set_targets(UC_INVALID, do_test);

	blocks.push_back(do_test);
	blocks.push_back(do_jmp);
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
	ignore_unused_variable_warning(start, exit);
	if (!stmt)  // Optimize whole loop away.
		return;
	// The start of a loop is a jump target and needs
	// a new basic block.
	Basic_block *loop_top = new Basic_block();
	curr->set_taken(loop_top);
	blocks.push_back(loop_top);
	// Local copy.
	Basic_block *loop_body = loop_top;
	// Basic block past loop body.
	Basic_block *past_loop = new Basic_block();
	// Generate loop body.
	stmt->gen(fun, blocks, loop_body, end, labels, loop_top, past_loop);
	// Jump back to top.
	loop_body->set_targets(UC_JMP, loop_top);

	blocks.push_back(curr = past_loop);
	if (past_loop->is_orphan())
		warning("No 'break' statements will be executed for infinite loop");
}

/*
 *  Delete.
 */

Uc_arrayloop_statement::~Uc_arrayloop_statement(
) {
	delete stmt;
}

/*
 *  Finish up creation.
 */

void Uc_arrayloop_statement::finish(
    Uc_function *fun
) {
	char buf[100];
	if (!index) {       // Create vars. if not given.
		sprintf(buf, "_%s_index", array->get_name());
		index = fun->add_symbol(buf);
	}
	if (!array_size) {
		sprintf(buf, "_%s_size", array->get_name());
		array_size = fun->add_symbol(buf);
	}
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
	ignore_unused_variable_warning(start, exit);
	if (!stmt)
		return;         // Nothing useful to do.
	// Start of loop.
	WriteOp(curr, UC_LOOP);
	// The start of a loop is a jump target and needs
	// a new basic block.
	Basic_block *for_top = new Basic_block();
	curr->set_taken(for_top);
	blocks.push_back(for_top);
	// Body of FOR loop.
	Basic_block *for_body = new Basic_block();
	blocks.push_back(for_body);
	// Block immediatelly after FOR.
	Basic_block *past_for = new Basic_block();
	UsecodeOps opcode;
	if (array->is_static())
		opcode = UC_LOOPTOPS;
	else if (array->get_sym_type() == Uc_symbol::Member_var)
		opcode = UC_LOOPTOPTHV;
	else
		opcode = UC_LOOPTOP;
	for_top->set_targets(opcode, for_body, past_for);
	WriteJumpParam2(for_top, index->get_offset());// Counter, total-count variables.
	WriteJumpParam2(for_top, array_size->get_offset());
	WriteJumpParam2(for_top, var->get_offset());    // Loop variable, than array.
	WriteJumpParam2(for_top, array->get_offset());

	// Generate FOR body.
	stmt->gen(fun, blocks, for_body, end, labels, for_top, past_for);
	// Jump back to top.
	for_body->set_targets(UC_JMP, for_top);

	blocks.push_back(curr = past_for);
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
	map<string, Basic_block *>::iterator it = labels.find(label);
	// Should never fail, but...
	assert(it != labels.end());
	Basic_block *block = it->second;
	curr->set_taken(block);
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
	map<string, Basic_block *>::iterator it = labels.find(label);
	if (it != labels.end()) {
		Basic_block *l = it->second;
		curr->set_targets(UC_JMP, l);
		curr = new Basic_block();
		blocks.push_back(curr);
	} else {
		char buf[255];
		snprintf(buf, 255, "Undeclared label: '%s'", label.c_str());
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
	Uc_array_expression *parms = new Uc_array_expression;
	if (parm0)
		parms->add(parm0);
	Uc_call_expression *fcall = new Uc_call_expression(intr, parms, fun);
	Uc_call_statement fstmt(fcall);
	fstmt.gen(fun, blocks, curr, end, labels);
	parms->clear();         // DON'T want to delete parm0.
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
	if (!remove && !statements) // Optimize whole case away.
		return;

	for (vector<int>::reverse_iterator it = string_offset.rbegin();
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
	// New basic block for CASE body.
	Basic_block *case_body = new Basic_block();
	blocks.push_back(case_body);
	// Past CASE body.
	Basic_block *past_case = new Basic_block();
	if (is_default())
		curr->set_targets(UC_INVALID, case_body);
	else {
		curr->set_targets(UC_CMPS, case_body, past_case);
		WriteJumpParam2(curr, string_offset.size());    // # strings on stack.
	}

	if (remove) {       // Remove answer?
		if (string_offset.size() > 1) {
			Uc_array_expression *strlist = new Uc_array_expression();
			for (vector<int>::iterator it = string_offset.begin();
			        it != string_offset.end(); ++it) {
				Uc_string_expression *str = new Uc_string_expression(*it);
				strlist->add(str);
			}
			Call_intrinsic(fun, blocks, case_body, end, labels,
			               Uc_function::get_remove_answer(), strlist);
		} else if (!string_offset.empty())
			Call_intrinsic(fun, blocks, case_body, end, labels,
			               Uc_function::get_remove_answer(),
			               new Uc_string_expression(string_offset[0]));
		else
			Call_intrinsic(fun, blocks, case_body, end, labels,
			               Uc_function::get_remove_answer(),
			               new Uc_choice_expression());
	}
	if (statements)         // Generate statement's code.
		statements->gen(fun, blocks, case_body, end, labels, start, exit);
	// Jump back to converse top.
	case_body->set_targets(UC_JMP, start);
	blocks.push_back(curr = past_case);
}

/*
 *  Initialize.
 */

Uc_converse_statement::Uc_converse_statement(
    Uc_expression *a,
    std::vector<Uc_statement *> *cs,
    bool n
)
	: answers(a), cases(*cs), nestconv(n) {
	bool has_default = false;
	for (vector<Uc_statement *>::const_iterator it = cases.begin();
	        it != cases.end(); ++it) {
		Uc_converse_case_statement *stmt =
		    static_cast<Uc_converse_case_statement *>(*it);
		if (stmt->is_default()) {
			if (has_default) {
				char buf[255];
				snprintf(buf, 255, "converse statement already has a default case.");
				error(buf);
			} else
				has_default = true;
		}
	}
}

/*
 *  Delete.
 */

Uc_converse_statement::~Uc_converse_statement(
) {
	delete answers;
	for (std::vector<Uc_statement *>::const_iterator it = cases.begin();
	        it != cases.end(); ++it)
		delete(*it);
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
	ignore_unused_variable_warning(start, exit);
	if (cases.empty())  // Nothing to do; optimize whole block away.
		return;
	if (nest++ > 0 || nestconv)         // Not the outermost?
		// Generate a 'push_answers()'.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_push_answers());
	if (answers)            // Add desired answers.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_add_answer(), answers);
	// The start of a CONVERSE loop is a jump target and needs
	// a new basic block.
	Basic_block *conv_top = new Basic_block();
	curr->set_taken(conv_top);
	blocks.push_back(conv_top);
	// Need new block as it is past a jump.
	Basic_block *conv_body = new Basic_block();
	blocks.push_back(conv_body);
	// Block past the CONVERSE loop.
	Basic_block *past_conv = new Basic_block();
	WriteOp(past_conv, UC_CONVERSELOC);
	conv_top->set_targets(UC_CONVERSE, conv_body, past_conv);
	// Generate loop body.
	Uc_converse_case_statement *def = nullptr;
	for (std::vector<Uc_statement *>::const_iterator it = cases.begin();
	        it != cases.end(); ++it) {
		Uc_converse_case_statement *stmt =
		    static_cast<Uc_converse_case_statement *>(*it);
		if (stmt->is_default())
			def = stmt;
		else
			stmt->gen(fun, blocks, conv_body, end, labels, conv_top, past_conv);
	}
	if (def)
		def->gen(fun, blocks, conv_body, end, labels, conv_top, past_conv);
	// Jump back to top.
	conv_body->set_targets(UC_JMP, conv_top);
	blocks.push_back(curr = past_conv);

	if (--nest > 0 || nestconv)         // Not the outermost?
		// Generate a 'pop_answers()'.
		Call_intrinsic(fun, blocks, curr, end, labels,
		               Uc_function::get_pop_answers());
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
	Basic_block *block = new Basic_block();
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
	for (std::vector<Uc_statement *>::const_iterator it = cases.begin();
	        it != cases.end(); ++it)
		delete(*it);
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
	for (vector<Uc_statement *>::const_iterator it = cases.begin();
	        it != cases.end(); ++it) {
		Uc_switch_case_statement *stmt =
		    static_cast<Uc_switch_case_statement *>(*it);
		if (stmt->is_default()) {
			if (has_default) {
				char buf[255];
				snprintf(buf, 255, "switch statement already has a default case.");
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
	Uc_var_expression *var = new Uc_var_expression(cond->need_var(curr, fun));
	vector<Basic_block *> case_blocks;
	Basic_block *def_case = nullptr;
	for (size_t i = 0; i < cases.size(); i++) {
		Uc_switch_case_statement *stmt =
		    dynamic_cast<Uc_switch_case_statement *>(cases[i]);
		if (stmt->is_default()) { // Store the default case iterator.
			def_case = new Basic_block();
			case_blocks.push_back(def_case);
		} else {
			// Generate the conditional jumps and the case basic blocks.
			var->gen_value(curr);
			// Case block.
			Basic_block *case_code = new Basic_block();
			stmt->gen_check(fun, blocks, curr, end, labels, case_code);
			case_blocks.push_back(case_code);
		}
	}
	// All done with it.
	delete var;
	// Past SWITCH block.
	Basic_block *past_switch = new Basic_block();
	// For all other cases, the default jump.
	if (def_case)
		curr->set_targets(UC_JMP, def_case);
	else
		curr->set_targets(UC_JMP, past_switch);
	for (size_t i = 0; i < cases.size(); i++) {
		Uc_switch_case_statement *stmt =
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
	for (std::vector<Uc_expression *>::const_iterator it = exprs.begin();
	        it != exprs.end(); ++it) {
		Uc_expression *msg = *it;
		// A known string?
		int offset = msg->get_string_offset();
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
