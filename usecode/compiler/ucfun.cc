/**
 ** Ucfun.cc - Usecode compiler function.
 **
 ** Written: 1/2/01 - JSF
 **/

/*
Copyright (C) 2000 The Exult Team

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
#include <string>
#include <vector>
#include <map>
#include <set>
#include "ucfun.h"
#include "ucstmt.h"
#include "utils.h"
#include "opcodes.h"
#include "ucexpr.h"         /* Needed only for Write2(). */
#include "ucsymtbl.h"
#include "basic_block.h"
#include "array_size.h"

using std::strlen;
using std::memcpy;
using std::string;
using std::vector;
using std::map;
using std::pair;

Uc_scope Uc_function::globals(nullptr);   // Stores intrinic symbols.
vector<Uc_intrinsic_symbol *> Uc_function::intrinsics;
int Uc_function::num_global_statics = 0;
int Uc_function::add_answer = -1, Uc_function::remove_answer = -1,
                 Uc_function::push_answers = -1, Uc_function::pop_answers = -1,
                              Uc_function::show_face = -1, Uc_function::remove_face = -1,
                                           Uc_function::get_item_shape = -1, Uc_function::get_usecode_fun = -1;
Uc_function::Intrinsic_type Uc_function::intrinsic_type =
    Uc_function::unset;

/*
 *  Create function, and add to global symbol table.
 */

Uc_function::Uc_function(
    Uc_function_symbol *p,
    Uc_scope *parent
) : top(parent), proto(p), cur_scope(&top), num_parms(0),
	num_locals(0), num_statics(0), text_data(nullptr), text_data_size(0),
	statement(nullptr) {
	add_global_function_symbol(proto, parent);// Add prototype to globals.
	const std::vector<Uc_var_symbol *> &parms = proto->get_parms();
	// Add backwards.
	for (std::vector<Uc_var_symbol *>::const_reverse_iterator it = parms.rbegin();
	        it != parms.rend(); ++it)
		add_symbol(*it);
	num_parms = num_locals;     // Set counts.
	num_locals = 0;
}

/*
 *  Delete.
 */

Uc_function::~Uc_function(
) {
	delete statement;
	delete proto;
	labels.clear();
}

/*
 *  Add a new variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_symbol(
    char *nm
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_var_symbol(nm, num_parms + num_locals++);
	cur_scope->add(var);
	return var;
}

/*
 *  Add a new variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_symbol(
    char *nm,
    Uc_class *c
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_class_inst_symbol(nm, c, num_parms + num_locals++);
	cur_scope->add(var);
	return var;
}

/*
 *  Add a new variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_symbol(
    char *nm,
    Uc_struct_symbol *s
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_struct_var_symbol(nm, s, num_parms + num_locals++);
	cur_scope->add(var);
	return var;
}

/*
 *  Add a new variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_symbol(
    Uc_var_symbol *var
) {
	if (cur_scope->is_dup(var->get_name()))
		return nullptr;
	// Create & assign slot.
	var->set_offset(num_parms + num_locals++);
	cur_scope->add(var);
	return var;
}

/*
 *  Add an alias to variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_alias(
    char *nm,
    Uc_var_symbol *var
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_alias_symbol *alias = new Uc_alias_symbol(nm, var);
	cur_scope->add(alias);
	return alias;
}

/*
 *  Add an alias to variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_alias(
    char *nm,
    Uc_var_symbol *v,
    Uc_struct_symbol *struc
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = static_cast<Uc_var_symbol *>(v->get_sym());
	Uc_alias_symbol *alias = new Uc_struct_alias_symbol(nm, var, struc);
	cur_scope->add(alias);
	return alias;
}

/*
 *  Add an alias to variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_var_symbol *Uc_function::add_alias(
    char *nm,
    Uc_var_symbol *v,
    Uc_class *c
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_var_symbol *var = static_cast<Uc_var_symbol *>(v->get_sym());
	Uc_alias_symbol *alias = new Uc_class_alias_symbol(nm, var, c);
	cur_scope->add(alias);
	return alias;
}

/*
 *  Add a new static variable to the current scope.
 */

void Uc_function::add_static(
    char *nm
) {
	if (cur_scope->is_dup(nm))
		return;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_var_symbol(nm, num_statics++);
	cur_scope->add(var);
}

/*
 *  Add a new static struct variable to the current scope.
 */

void Uc_function::add_static(
    char *nm,
    Uc_struct_symbol *type
) {
	if (cur_scope->is_dup(nm))
		return;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_struct_var_symbol(nm, num_statics++, type);
	cur_scope->add(var);
}

/*
 *  Add a new static class to the current scope.
 */

void Uc_function::add_static(
    char *nm,
    Uc_class *c
) {
	if (cur_scope->is_dup(nm))
		return;
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_class_symbol(nm, c, num_statics++);
	cur_scope->add(var);
}

/*
 *  Add a new string constant to the current scope.
 */

Uc_symbol *Uc_function::add_string_symbol(
    char *nm,
    char *text
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_symbol *sym = new Uc_string_symbol(nm, add_string(text));
	cur_scope->add(sym);
	return sym;
}

/*
 *  Add a new integer constant variable to the current scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_symbol *Uc_function::add_int_const_symbol(
    char *nm,
    int value,
    int opcode
) {
	if (cur_scope->is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_const_int_symbol *var = new Uc_const_int_symbol(nm, value, static_cast<UsecodeOps>(opcode));
	cur_scope->add(var);
	return var;
}

/*
 *  Add a new integer constant variable to the global scope.
 *
 *  Output: New sym, or nullptr if already declared.
 */

Uc_symbol *Uc_function::add_global_int_const_symbol(
    char *nm,
    int value,
    int opcode
) {
	if (globals.is_dup(nm))
		return nullptr;
	// Create & assign slot.
	Uc_const_int_symbol *var = new Uc_const_int_symbol(nm, value, static_cast<UsecodeOps>(opcode));
	globals.add(var);
	return var;
}

/*
 *  Add a global static.
 */

void Uc_function::add_global_static(
    char *nm
) {
	if (globals.is_dup(nm))
		return;
	num_global_statics++;       // These start with 1.
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_var_symbol(nm,
	        -num_global_statics);
	globals.add(var);
}

/*
 *  Add a global static.
 */

void Uc_function::add_global_static(
    char *nm,
    Uc_struct_symbol *type
) {
	if (globals.is_dup(nm))
		return;
	num_global_statics++;       // These start with 1.
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_struct_var_symbol(nm,
	        -num_global_statics, type);
	globals.add(var);
}

/*
 *  Add a global static class.
 */

void Uc_function::add_global_static(
    char *nm,
    Uc_class *c
) {
	if (globals.is_dup(nm))
		return;
	num_global_statics++;       // These start with 1.
	// Create & assign slot.
	Uc_var_symbol *var = new Uc_static_class_symbol(nm, c,
	        -num_global_statics);
	globals.add(var);
}

/*
 *  Add a string to the data area.
 *
 *  Output: offset of string.
 */

int Uc_function::add_string(
    char *text
) {
	// Search for an existing string.
	std::map<std::string, int>::const_iterator exist = text_map.find(text);
	if (exist != text_map.end())
		return (*exist).second;
	int offset = text_data_size;    // This is where it will go.
	int textlen = strlen(text) + 1; // Got to include ending null.
	char *new_text_data = new char[text_data_size + textlen];
	if (text_data_size)     // Copy over old.
		memcpy(new_text_data, text_data, text_data_size);
	// Append new.
	memcpy(new_text_data + text_data_size, text, textlen);
	delete [] text_data;
	text_data = new_text_data;
	text_data_size += textlen;
	text_map[text] = offset;    // Store map entry.
	return offset;
}

/*
 *  Find the (unique) string for a given prefix.
 *
 *  Output: Offset of string.  Error printed if more than one.
 *      0 if not found, with error printed.
 */

int Uc_function::find_string_prefix(
    Uc_location &loc,       // For printing errors.
    const char *text
) {
	int len = strlen(text);
	// Find 1st entry >= text.
	std::map<std::string, int>::const_iterator exist =
	    text_map.lower_bound(text);
	if (exist == text_map.end() ||
	        strncmp(text, (*exist).first.c_str(), len) != 0) {
		char *buf = new char[len + 100];
		sprintf(buf, "Prefix '%s' matches no string in this function",
		        text);
		loc.error(buf);
		delete [] buf;
		return 0;
	}
	std::map<std::string, int>::const_iterator next = exist;
	++next;
	if (next != text_map.end() &&
	        strncmp(text, (*next).first.c_str(), len) == 0) {
		char *buf = new char[len + 100];
		sprintf(buf, "Prefix '%s' matches more than one string", text);
		loc.error(buf);
		delete [] buf;
	}
	return (*exist).second;     // Return offset.
}


/*
 *  Lookup/add a link to an external function.
 *
 *  Output: Link offset.
 */

int Uc_function::link(
    Uc_function_symbol *fun
) {
	for (std::vector<Uc_function_symbol *>::const_iterator it = links.begin();
	        it != links.end(); ++it)
		if (*it == fun)     // Found it?  Return offset.
			return it - links.begin();
	int offset = links.size();  // Going to add it.
	links.push_back(fun);
	return offset;
}

static int Remove_dead_blocks(
    vector<Basic_block *> &blocks
) {
	int niter = 0;
	while (true) {
		niter++;
		size_t i = 0;
		int nremoved = 0;
		while (i + 1 < blocks.size()) {
			Basic_block *block = blocks[i];
			bool remove = false;
			if (!block->is_reachable()) {
				// Remove unreachable block.
				block->unlink_descendants();
				block->unlink_predecessors();
				remove = true;
			} else if (block->is_empty_block() && block->is_forced_target()) {
				// Link predecessors directly to decendants.
				// May link a block to the initial block, or
				// may link the initial and ending blocks.
				block->link_through_block();
				remove = true;
			}
			if (remove) {
				++nremoved;
				vector<Basic_block *>::iterator it = blocks.begin() + i;
				blocks.erase(it);
				delete block;
				continue;
			}
			i++;
		}
		if (!nremoved)
			break;
	}
	return niter;
}

static int Optimize_jumps(
    vector<Basic_block *> &blocks,
    bool returns
) {
	int niter = 0;
	while (true) {
		niter++;
		size_t i = 0;
		int nremoved = 0;
		while (i + 1 < blocks.size()) {
			Basic_block *block = blocks[i];
			Basic_block *aux = block->get_taken();
			bool remove = false;
			if (block->is_jump_block()) {
				// Unconditional jump block.
				if (aux->is_simple_jump_block()) {
					// Double-jump. Merge the jumps in a single one.
					block->set_taken(aux->get_taken());
					++nremoved;
					continue;
				} else if (aux->is_end_block()) {
					// Jump to end-block.
					if (aux->is_empty_block()) {
						// No return opcode in end block; add one.
						WriteOp(block, returns ? UC_RETZ : UC_RET);
						remove = true;
					} else if (aux->is_simple_return_block()) {
						// Copy return opcode from end block.
						WriteOp(block, aux->get_return_opcode());
						remove = true;
					} else if (aux->is_simple_abort_block()) {
						// Copy abort code.
						WriteOp(block, UC_ABRT);
						remove = true;
					}
					if (remove) {
						// Set destination to end block.
						block->set_targets(UC_INVALID, aux->get_taken());
						++nremoved;
						continue;
					}
				}
			} else if (aux == blocks[i + 1]) {
				if (block->is_fallthrough_block() || block->is_jump_block()) {
					// Fall-through block followed by block which descends
					// from current block or jump immediatelly followed
					// by its target.
					if (aux->has_single_predecessor()) {
						// Child block has single ancestor.
						// Merge it with current block.
						block->merge_taken();
						remove = true;
					} else if (block->get_opcode() != -1) {
						// Optimize the jump away.
						++nremoved;
						block->clear_jump();
						continue;
					}
				} else if (i + 2 < blocks.size()
				           && block->is_conditionaljump_block()
				           && aux->is_simple_jump_block()
				           && aux->has_single_predecessor()
				           && block->get_ntaken() == blocks[i + 2]) {
					// Conditional jump followed by jump block which
					// descends solely from current block.
					// Reverse condition.
					UsecodeOps opcode = block->get_last_instruction();
					switch (opcode) {
					case UC_CMPGT:
						opcode = UC_CMPLE;
						break;
					case UC_CMPLT:
						opcode = UC_CMPGE;
						break;
					case UC_CMPGE:
						opcode = UC_CMPLT;
						break;
					case UC_CMPLE:
						opcode = UC_CMPGT;
						break;
					case UC_CMPNE:
						opcode = UC_CMPEQ;
						break;
					case UC_CMPEQ:
						opcode = UC_CMPNE;
						break;
					case UC_NOT:
						break;
					default:
						opcode = UC_INVALID;
						break;
					}
					if (opcode == UC_INVALID)
						WriteOp(block, UC_NOT);
					else {
						PopOpcode(block);
						if (opcode != UC_NOT)
							WriteOp(block, opcode);
					}
					// Set destinations.
					block->set_taken(block->get_ntaken());
					block->set_ntaken(aux->get_taken());
					remove = true;
				}
			} else if (block->is_end_block() && (aux = blocks[i + 1])->is_end_block()
			           && block->ends_in_return() && aux->is_simple_return_block()
			           && block->get_return_opcode() == aux->get_return_opcode()) {
				block->set_taken(aux);
				PopOpcode(block);
				++nremoved;
				continue;
			}
			if (remove) {
				++nremoved;
				aux->unlink_descendants();
				aux->unlink_predecessors();
				vector<Basic_block *>::iterator it = blocks.begin() + i + 1;
				blocks.erase(it);
				delete aux;
				continue;
			}
			i++;
		}
		if (!nremoved)
			break;
	}
	return niter;
}

static inline int Compute_locations(
    vector<Basic_block *> &blocks,
    vector<int> &locs
) {
	locs.reserve(blocks.size());
	locs.push_back(0);  // First block is at zero.
	// Get locations.
	Basic_block *back = blocks.back();
	for (vector<Basic_block *>::iterator it = blocks.begin();
	        *it != back; ++it)
		locs.push_back(locs.back() + (*it)->get_block_size());
	return locs.back() + back->get_block_size();
}

static inline int Compute_jump_distance(
    Basic_block *block,
    vector<int> &locs
) {
	int dest;
	if (block->is_jump_block())
		dest = block->get_taken_index();
	else
		dest = block->get_ntaken_index();
	return locs[dest] - (locs[block->get_index()] + block->get_block_size());
}

static int Set_32bit_jump_flags(
    vector<Basic_block *> &blocks
) {
	int niter = 0;
	while (true) {
		niter++;
		int nchanged = 0;
		vector<int> locs;
		Compute_locations(blocks, locs);
		// Determine base distances and which are 32-bit.
		for (vector<Basic_block *>::iterator it = blocks.begin();
		        it != blocks.end(); ++it) {
			Basic_block *block = *it;
			// If the jump is already 32-bit, or if there is
			// no jump (just a fall-through), then there is
			// nothing to do.
			if (block->does_not_jump() || block->is_32bit_jump())
				continue;
			if (is_sint_32bit(Compute_jump_distance(block, locs))) {
				nchanged++;
				block->set_32bit_jump();
			}
		}
		if (!nchanged)
			break;
	}
	return niter;
}

/*
 *  Generate Usecode.
 */

void Uc_function::gen(
    std::ostream &out
) {
	map<string, Basic_block *> label_blocks;
	for (std::set<string>::iterator it = labels.begin();
	        it != labels.end(); ++it)
		// Fill up label <==> basic block map.
		label_blocks.insert(pair<string, Basic_block *>(*it, new Basic_block()));
	Basic_block *initial = new Basic_block(-1);
	Basic_block *endblock = new Basic_block(-1);
	vector<Basic_block *> fun_blocks;
	fun_blocks.reserve(300);
	Basic_block *current = new Basic_block();
	initial->set_taken(current);
	fun_blocks.push_back(current);
	if (statement)
		statement->gen(this, fun_blocks, current, endblock, label_blocks);
	assert(initial->no_parents() && endblock->is_childless());
	while (!fun_blocks.empty() && fun_blocks.back()->no_parents()) {
		Basic_block *blk = fun_blocks.back();
		fun_blocks.pop_back();
		delete blk;
	}
	// Mark all blocks reachable from initial block.
	initial->mark_reachable();
	if (!fun_blocks.empty() && !fun_blocks.back()->is_end_block())
		fun_blocks.back()->set_targets(UC_INVALID, endblock);
	// Labels map is no longer needed.
	for (map<string, Basic_block *>::iterator it = label_blocks.begin();
	        it != label_blocks.end(); ++it) {
		Basic_block *label = it->second;
		if (!label->is_reachable()) {
			// Label can't be reached from the initial block.
			// Remove it from map and unlink references to it.
			label->unlink_descendants();
			label->unlink_predecessors();
			it->second = nullptr;
			delete label;
		}
	}
	label_blocks.clear();
	// First round of optimizations.
	Remove_dead_blocks(fun_blocks);
	// Second round of optimizations.
	Optimize_jumps(fun_blocks, proto->has_ret());
	// Third round of optimizations.
	Remove_dead_blocks(fun_blocks);
	// Set block indices.
	for (size_t i = 0; i < fun_blocks.size(); i++) {
		Basic_block *block = fun_blocks[i];
		block->set_index(i);
		block->link_predecessors();
	}
	vector<char> code;      // Generate code here first.
	if (!fun_blocks.empty()) {
		// Mark blocks for 32-bit usecode jump sizes.
		Set_32bit_jump_flags(fun_blocks);
		vector<int> locs;
		// Get locations.
		int size = Compute_locations(fun_blocks, locs) + 1;

		code.reserve(size);
		// Output code.
		for (vector<Basic_block *>::iterator it = fun_blocks.begin();
		        it != fun_blocks.end(); ++it) {
			Basic_block *block = *it;
			block->write(code);
			if (block->does_not_jump())
				continue;   // Not a jump.
			int dist = Compute_jump_distance(block, locs);
			if (is_sint_32bit(dist))
				Write4(code, dist);
			else
				Write2(code, dist);
		}
	}

	if (fun_blocks.empty() || !fun_blocks.back()->ends_in_return()) {
		// Always end with a RET or RTS if a return opcode
		// is not the last opcode in the function.
		if (proto->has_ret())
			// Function specifies a return value.
			// When in doubt, return zero by default.
			code.push_back(UC_RETZ);
		else
			code.push_back(UC_RET);
	}

	// Free up the blocks.
	for (vector<Basic_block *>::iterator it = fun_blocks.begin();
	        it != fun_blocks.end(); ++it)
		delete *it;
	fun_blocks.clear();
	delete initial;
	delete endblock;
	int codelen = code.size();  // Get its length.
	int num_links = links.size();
	// Total: text_data_size + data +
	//   #args + #locals + #links + links +
	//   codelen.
	int totallen =  2 + text_data_size + 2 + 2 + 2 + 2 * num_links + codelen;

	// Special cases.
	bool need_ext_header = (proto->get_usecode_num() == 0xffff) ||
	                       (proto->get_usecode_num() == 0xfffe);

	// Function # first.
	if (is_int_32bit(totallen) || proto->has_high_id() || need_ext_header) {
		totallen += 2;  // Extra space for text_data_size.
		if (proto->has_high_id()) {
			Write2(out, 0xfffe);
			Write4(out, proto->get_usecode_num());
		} else {
			Write2(out, 0xffff);
			Write2(out, proto->get_usecode_num());
		}
		Write4(out, totallen);
		Write4(out, text_data_size);
	} else {
		Write2(out, proto->get_usecode_num());
		Write2(out, totallen);
		Write2(out, text_data_size);
	}
	// Now data.
	out.write(text_data, text_data_size);
	// Counts.
	Write2(out, num_parms +
	       (get_function_type() != Uc_function_symbol::utility_fun));
	Write2(out, num_locals);
	Write2(out, num_links);
	// Write external links.
	for (std::vector<Uc_function_symbol *>::const_iterator it =
	            links.begin(); it != links.end(); ++it)
		Write2(out, (*it)->get_usecode_num());
	char *ucstr = &code[0];     // Finally, the code itself.
	out.write(ucstr, codelen);
	out.flush();
}

#ifndef TO_STRING
#if defined __STDC__ && __STDC__
#define TO_STRING(x) #x
#else
#define TO_STRING(x) "x"
#endif
#endif

/*
 *  Tables of usecode intrinsics:
 */
#define USECODE_INTRINSIC_PTR(NAME) TO_STRING(UI_##NAME)

const char *bg_intrinsic_table[] = {
#include "../bgintrinsics.h"
};

const char *si_intrinsic_table[] = {
#include "../siintrinsics.h"
};

const char *sibeta_intrinsic_table[] = {
#include "../sibetaintrinsics.h"
};

/*
 *  Add one of the intrinsic tables to the 'intrinsics' scope.
 */

void Uc_function::set_intrinsics(
) {
	int cnt;
	const char **table;
	if (intrinsic_type == unset) {
		Uc_location::yywarning(
		    "Use '#game \"[blackgate|serpentisle|serpentbeta]\" to specify "
		    "intrinsics to use (default = blackgate).");
		intrinsic_type = bg;
	}
	if (intrinsic_type == bg) {
		table = bg_intrinsic_table;
		cnt = array_size(bg_intrinsic_table);
		add_answer = 5;
		remove_answer = 6;
		push_answers = 7;
		pop_answers = 8;
	} else if (intrinsic_type == si) {
		table = si_intrinsic_table;
		cnt = array_size(si_intrinsic_table);
		add_answer = 0xc;
		remove_answer = 0xd;
		push_answers = 0xe;
		pop_answers = 0xf;
	} else {
		table = sibeta_intrinsic_table;
		cnt = array_size(sibeta_intrinsic_table);
		add_answer = 0xc;
		remove_answer = 0xd;
		push_answers = 0xe;
		pop_answers = 0xf;
	}
	show_face = 3;
	remove_face = 4;
	intrinsics.resize(cnt);
	for (int i = 0; i < cnt; i++) {
		const char *nm = table[i];
		if (!strncmp(nm, "UI_get_usecode_fun", sizeof("UI_get_usecode_fun")))
			get_usecode_fun = i;
		else if (!strncmp(nm, "UI_get_item_shape", sizeof("UI_get_item_shape")))
			get_item_shape = i;
		Uc_intrinsic_symbol *sym = new Uc_intrinsic_symbol(nm, i);
		intrinsics[i] = sym;    // Store in indexed list.
		if (!globals.search(nm))
			// ++++Later, get num parms.
			globals.add(sym);
	}
}

/*
 *  Create symbol for this function.
 */

Usecode_symbol *Uc_function::create_sym(
) {
	Usecode_symbol::Symbol_kind kind = Usecode_symbol::fun_defined;
	// For now, all externs have their ID given.
	if (is_externed())
		kind = Usecode_symbol::fun_extern_defined;
	if (proto->get_function_type() == Uc_function_symbol::shape_fun)
		kind = Usecode_symbol::shape_fun;
	else if (proto->get_function_type() == Uc_function_symbol::object_fun)
		kind = Usecode_symbol::object_fun;
	return new Usecode_symbol(get_name(), kind, get_usecode_num(),
	                          proto->get_shape_num());
}
