/**
 ** Ucstmt.h - Usecode compiler statements.
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

#ifndef INCL_UCSTMT
#define INCL_UCSTMT

#include "ignore_unused_variable_warning.h"
#include "ucloc.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class Uc_expression;
class Uc_call_expression;
class Uc_array_expression;
class Uc_function;
class Uc_var_symbol;
class Uc_del_expression;
class Basic_block;

struct Uc_loop_data {
	Basic_block* start;
	Basic_block* exit;
	std::string  name;

	bool operator==(const std::string& nm) const {
		return name == nm;
	}
};

class Uc_loop_data_stack {
	using Data = std::vector<Uc_loop_data>;
	Data loop_data{
			{nullptr, nullptr, ""}
    };

public:
	void push(Basic_block* start, Basic_block* exit, std::string name) {
		loop_data.push_back(Uc_loop_data{start, exit, std::move(name)});
	}

	void pop() {
		if (loop_data.empty()) {
			throw std::runtime_error("Loop data stack underflow!");
		}
		loop_data.pop_back();
	}

	const Uc_loop_data& back() {
		if (loop_data.empty()) {
			throw std::runtime_error("Loop data stack underflow!");
		}
		return loop_data.back();
	}

	struct find_result {
		Data::const_reverse_iterator iter;
		bool                         found;

		bool has_value() const {
			return found;
		}

		explicit operator bool() const {
			return has_value();
		}

		bool operator!() const {
			return !has_value();
		}

		const Uc_loop_data& unsafe_get() const {
			return *iter;
		}

		const Uc_loop_data& get() const {
			if (!has_value()) {
				throw std::runtime_error("Iterator dereferenced without value!");
			}
			return unsafe_get();
		}
	};

	find_result find(const std::string& needle) const {
		if (needle.empty()) {
			return {loop_data.crbegin(), true};
		}
		auto iter = std::find(loop_data.crbegin(), loop_data.crend(), needle);
		return {iter, iter != loop_data.crend()};
	}
};

/*
 *  A statement:
 */
class Uc_statement : public Uc_location {
public:
	virtual ~Uc_statement() = default;
	// Generate code.
	virtual void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue)
			= 0;

	// Whether statement is a fallthrough statement or ends in one.
	virtual bool falls_through() const {
		return false;
	}

	virtual bool set_loop_name(const std::string& nm) {
		ignore_unused_variable_warning(nm);
		return false;
	}
};

/*
 *  A group of statements.
 */
class Uc_block_statement : public Uc_statement {
	std::vector<Uc_statement*> statements;

public:
	~Uc_block_statement() override;

	void add(Uc_statement* stmt) {
		statements.push_back(stmt);
	}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;

	bool falls_through() const override {
		return !statements.empty() && statements.back()->falls_through();
	}

	bool set_loop_name(const std::string& nm) override;
};

/*
 *  An assignment statement:
 */
class Uc_assignment_statement : public Uc_statement {
	Uc_expression *target, *value;

public:
	Uc_assignment_statement(Uc_expression* t, Uc_expression* v) : target(t), value(v) {}

	~Uc_assignment_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  An IF statement:
 */
class Uc_if_statement : public Uc_statement {
	Uc_expression* expr;    // What to test.
	// What to execute:
	Uc_statement *if_stmt, *else_stmt;

public:
	Uc_if_statement(Uc_expression* e, Uc_statement* t, Uc_statement* f) : expr(e), if_stmt(t), else_stmt(f) {}

	~Uc_if_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  A try/catch statement:
 */
class Uc_trycatch_statement : public Uc_statement {
	Uc_var_symbol* catch_var{};
	Uc_statement*  try_stmt;
	Uc_statement*  catch_stmt{};

public:
	Uc_trycatch_statement(Uc_statement* t) : try_stmt(t) {}

	void set_catch_variable(Uc_var_symbol* v) {
		catch_var = v;
	}

	void set_catch_statement(Uc_statement* s) {
		catch_stmt = s;
	}

	~Uc_trycatch_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  A generic breakable statement:
 */
class Uc_breakable_statement : public Uc_statement {
protected:
	std::string   name;       // Loop name, or empty for unnamed.
	Uc_statement* stmt;       // What to execute.
	Uc_statement* nobreak;    // What to execute after normal finish.
public:
	Uc_breakable_statement(Uc_statement* s, Uc_statement* nb) : stmt(s), nobreak(nb) {}

	~Uc_breakable_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;

	bool set_loop_name(const std::string& nm) override {
		name = nm;
		return true;
	}
};

/*
 *  A WHILE statement:
 */
class Uc_while_statement : public Uc_breakable_statement {
	Uc_expression* expr;    // What to test.
public:
	Uc_while_statement(Uc_expression* e, Uc_statement* s, Uc_statement* nb) : Uc_breakable_statement(s, nb), expr(e) {}

	~Uc_while_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  A DO...WHILE statement:
 */
class Uc_dowhile_statement : public Uc_breakable_statement {
	Uc_expression* expr;    // What to test.
public:
	Uc_dowhile_statement(Uc_expression* e, Uc_statement* s, Uc_statement* nb) : Uc_breakable_statement(s, nb), expr(e) {}

	~Uc_dowhile_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  An infinite loop statement:
 */
class Uc_infinite_loop_statement : public Uc_breakable_statement {
public:
	Uc_infinite_loop_statement(Uc_statement* s, Uc_statement* nb) : Uc_breakable_statement(s, nb) {}

	~Uc_infinite_loop_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  enum() statement:
 */

class array_enum_statement : public Uc_statement {
public:
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Base for an array loop statement:
 */
class Uc_arrayloop_statement_base : public Uc_statement {
protected:
	std::string name;    // Loop name, or empty for unnamed.
private:
	Uc_var_symbol* var;            // Loop variable.
	Uc_var_symbol* array;          // Array to loop over.
	Uc_var_symbol* index{};        // Counter.
	Uc_var_symbol* array_len{};    // Symbol holding array size.
public:
	Uc_arrayloop_statement_base(Uc_var_symbol* v, Uc_var_symbol* a)
			: var(v), array(a)

	{}

	void set_index(Uc_var_symbol* i) {
		index = i;
	}

	void set_array_size(Uc_var_symbol* as) {
		array_len = as;
	}

	void gen_check(Basic_block* for_top, Basic_block* loop_body, Basic_block* past_loop);
	void finish(Uc_function* fun);    // Create tmps. if necessary.

	bool set_loop_name(const std::string& nm) override {
		name = nm;
		return true;
	}
};

/*
 *  An array loop statement:
 */
class Uc_arrayloop_statement : public Uc_arrayloop_statement_base {
	Uc_statement* stmt{};       // What to execute.
	Uc_statement* nobreak{};    // What to execute after normal finish.
public:
	Uc_arrayloop_statement(Uc_var_symbol* v, Uc_var_symbol* a) : Uc_arrayloop_statement_base(v, a) {}

	~Uc_arrayloop_statement() override;

	void set_statement(Uc_statement* s) {
		stmt = s;
	}

	void set_nobreak(Uc_statement* n) {
		nobreak = n;
	}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  An array loop attend statement:
 */
class Uc_arrayloop_attend_statement : public Uc_arrayloop_statement_base {
	std::string label;

public:
	Uc_arrayloop_attend_statement(Uc_var_symbol* v, Uc_var_symbol* a) : Uc_arrayloop_statement_base(v, a) {}

	void set_target(std::string target) {
		label = std::move(target);
	}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  An RETURN statement:
 */
class Uc_return_statement : public Uc_statement {
	Uc_expression* expr;    // What to return.  May be nullptr.
public:
	Uc_return_statement(Uc_expression* e = nullptr) : expr(e) {}

	~Uc_return_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  BREAK statement:
 */
class Uc_break_statement : public Uc_statement {
	// Name of loop to break, or empty for innermost.
	std::string loop_name;

public:
	Uc_break_statement() = default;

	Uc_break_statement(std::string nm) : loop_name(std::move(nm)) {}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  CONTINUE statement:
 */
class Uc_continue_statement : public Uc_statement {
	// Name of loop to continue, or empty for innermost.
	std::string loop_name;

public:
	Uc_continue_statement() = default;

	Uc_continue_statement(std::string nm) : loop_name(std::move(nm)) {}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  BREAK statement:
 */
class Uc_fallthrough_statement : public Uc_statement {
public:
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override {
		ignore_unused_variable_warning(fun, blocks, curr, end, labels, break_continue);
	}

	bool falls_through() const override {
		return true;
	}
};

/*
 *  a LABEL statement:
 */
class Uc_label_statement : public Uc_statement {
	std::string label;

public:
	Uc_label_statement(char* nm) : label(nm) {}

	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;

	const std::string& get_label() const {
		return label;
	}
};

/*
 *  a GOTO statement:
 */
class Uc_goto_statement : public Uc_statement {
	std::string label;

public:
	Uc_goto_statement(char* nm) : label(nm) {}

	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  An ENDCONV statement:
 */
class Uc_endconv_statement : public Uc_statement {
public:
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Conversation CASE statement:
 */
class Uc_converse_case_statement : public Uc_statement {
	Uc_statement* statements;    // Execute these.
	bool          remove;        // True to remove answer.
public:
	Uc_converse_case_statement(Uc_statement* stmts, bool rem) : statements(stmts), remove(rem) {}

	~Uc_converse_case_statement() override {
		delete statements;
	}

	virtual int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) = 0;
	virtual int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels)
			= 0;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;

	bool falls_through() const override {
		return statements != nullptr && statements->falls_through();
	}
};

/*
 *  Conversation CASE statement with a list of strings:
 */
class Uc_converse_strings_case_statement : public Uc_converse_case_statement {
	std::vector<int> string_offset;    // Offset of string to compare.
public:
	Uc_converse_strings_case_statement(std::vector<int> soff, Uc_statement* stmts, bool rem)
			: Uc_converse_case_statement(stmts, rem), string_offset(std::move(soff)) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  Conversation CASE statement with a variable:
 */
class Uc_converse_variable_case_statement : public Uc_converse_case_statement {
	Uc_expression* variable;    // Alternative to pushing string_offset
public:
	Uc_converse_variable_case_statement(Uc_expression* v, Uc_statement* stmts, bool rem)
			: Uc_converse_case_statement(stmts, rem), variable(v) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  Conversation DEFAULT statement:
 */
class Uc_converse_default_case_statement : public Uc_converse_case_statement {
public:
	Uc_converse_default_case_statement(Uc_statement* stmts) : Uc_converse_case_statement(stmts, false) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  Conversation ALWAYS statement:
 */
class Uc_converse_always_case_statement : public Uc_converse_case_statement {
public:
	Uc_converse_always_case_statement(Uc_statement* stmts) : Uc_converse_case_statement(stmts, false) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;

	bool falls_through() const override {
		return true;
	}
};

/*
 *  Conversation CASE ATTEND statement:
 */
class Uc_converse_case_attend_statement : public Uc_statement {
	std::string label;
	bool        remove;    // True to remove answer.
public:
	Uc_converse_case_attend_statement(std::string target, bool rem) : label(std::move(target)), remove(rem) {}

	~Uc_converse_case_attend_statement() override                                            = default;
	virtual int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) = 0;
	virtual int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels)
			= 0;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;

	bool falls_through() const override {
		return false;
	}
};

/*
 *  Conversation CASE ATTEND statement with a list of strings:
 */
class Uc_converse_strings_case_attend_statement : public Uc_converse_case_attend_statement {
	std::vector<int> string_offset;    // Offset of string to compare.
public:
	Uc_converse_strings_case_attend_statement(std::vector<int> soff, std::string target, bool rem)
			: Uc_converse_case_attend_statement(std::move(target), rem), string_offset(std::move(soff)) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  Conversation CASE ATTEND statement with a variable:
 */
class Uc_converse_variable_case_attend_statement : public Uc_converse_case_attend_statement {
	Uc_expression* variable;    // Alternative to pushing string_offset
public:
	Uc_converse_variable_case_attend_statement(Uc_expression* v, std::string target, bool rem)
			: Uc_converse_case_attend_statement(std::move(target), rem), variable(v) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  Conversation DEFAULT ATTEND statement:
 */
class Uc_converse_default_case_attend_statement : public Uc_converse_case_attend_statement {
	int value;

public:
	Uc_converse_default_case_attend_statement(std::string target, int v)
			: Uc_converse_case_attend_statement(std::move(target), false), value(v) {}

	int gen_check(Basic_block* curr, Basic_block* case_body, Basic_block* past_case) override;
	int gen_remove(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block* case_body, Basic_block* end,
			std::map<std::string, Basic_block*>& labels) override;
};

/*
 *  A CONVERSE2 statement provides a less wordy way to implement a
 *  conversation.  It provides for CASE entries for the comparisons, and
 *  also generates push/pop-answers so these can be nested.
 */
class Uc_converse_statement : public Uc_breakable_statement {
	static int                 nest;        // Keeps track of nesting.
	Uc_expression*             answers;     // Answers to add.
	std::vector<Uc_statement*> cases;       // What to execute.
	bool                       nestconv;    // Whether or not to force push/pop.
public:
	Uc_converse_statement(Uc_expression* a, Uc_block_statement* p, std::vector<Uc_statement*>* cs, bool n, Uc_statement* nb);
	~Uc_converse_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  A CONVERSE ATTEND statement.
 */
class Uc_converse_attend_statement : public Uc_statement {
	std::string label;

public:
	Uc_converse_attend_statement(std::string target);
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Switch CASE statement:
 */
class Uc_switch_case_statement : public Uc_statement {
	Uc_statement* statements;    // Execute these.
public:
	Uc_switch_case_statement(Uc_statement* stmts) : statements(stmts) {}

	~Uc_switch_case_statement() override {
		delete statements;
	}

	virtual bool is_default() const {
		return false;
	}

	// Generate code.
	virtual int gen_check(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Basic_block* case_block = nullptr)
			= 0;
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Switch CASE statement:
 */
class Uc_switch_default_case_statement : public Uc_switch_case_statement {
public:
	Uc_switch_default_case_statement(Uc_statement* stmts) : Uc_switch_case_statement(stmts) {}

	bool is_default() const override {
		return true;
	}

	// Generate code.
	int gen_check(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Basic_block* case_block = nullptr) override {
		ignore_unused_variable_warning(fun, blocks, curr, end, labels, case_block);
		return 1;
	}
};

/*
 *  Switch CASE statement:
 */
class Uc_switch_expression_case_statement : public Uc_switch_case_statement {
	Uc_expression* check;    // The case we are interested in.
public:
	Uc_switch_expression_case_statement(Uc_expression* c, Uc_statement* stmts) : Uc_switch_case_statement(stmts), check(c) {}

	~Uc_switch_expression_case_statement() override;
	// Generate code.
	int gen_check(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Basic_block* case_block = nullptr) override;
};

/*
 *  C++-style switch statement. Just a less wordy form for
 *  long if/else if blocks which check different values of
 *  the same variable.
 */
class Uc_switch_statement : public Uc_statement {
	Uc_expression*             cond;     // Var to check values of.
	std::vector<Uc_statement*> cases;    // What to execute.
public:
	Uc_switch_statement(Uc_expression* v, std::vector<Uc_statement*>* cs);
	~Uc_switch_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Add string to current message (for conversations).
 */
class Uc_message_statement : public Uc_statement {
	Uc_array_expression* msgs;

public:
	Uc_message_statement(Uc_array_expression* m) : msgs(m) {}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Print message on screen (in conversation).
 */
class Uc_say_statement : public Uc_message_statement {
public:
	Uc_say_statement(Uc_array_expression* m) : Uc_message_statement(m) {}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Call a function/intrinsic.
 */
class Uc_call_statement : public Uc_statement {
	Uc_call_expression* function_call;

public:
	Uc_call_statement(Uc_call_expression* f);
	~Uc_call_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Statement that represents an abort opcode.
 */
class Uc_abort_statement : public Uc_statement {
	Uc_expression* expr;

public:
	Uc_abort_statement(Uc_expression* e = nullptr) : expr(e) {}

	~Uc_abort_statement() override;
	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

/*
 *  Statement that just represents a single opcode.
 */
class Uc_delete_statement : public Uc_statement {
	Uc_del_expression* expr;

public:
	Uc_delete_statement(Uc_del_expression* e) : expr(e) {}

	// Generate code.
	void gen(
			Uc_function* fun, std::vector<Basic_block*>& blocks, Basic_block*& curr, Basic_block* end,
			std::map<std::string, Basic_block*>& labels, Uc_loop_data_stack& break_continue) override;
};

#endif
