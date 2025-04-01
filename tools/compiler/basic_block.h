/**
 ** basic_block.cc - Basic block analysis for usecode compiler.
 **
 ** Written: 2/7/08
 **/

/*
Copyright (C) 2008-2022 The Exult Team

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

#ifndef INCL_BASICBLOCK
#define INCL_BASICBLOCK 1

#include "common_types.h"
#include "opcodes.h"
#include "ucexpr.h"

#include <iomanip>
#include <iostream>
#include <set>
#include <vector>

class Basic_block;

/*
 *  Class representing a single usecode instruction and its
 *  parameters, if any; for jump instructions, this does NOT
 *  include the instruction offset.
 */

class Opcode {
protected:
	UsecodeOps        opcode;
	std::vector<char> params;
	bool              is_jump;
	bool              op32bit = false;

public:
	Opcode(UsecodeOps op) : opcode(op) {
		switch (opcode) {
		case UC_LOOPTOP:
		case UC_LOOPTOPS:
		case UC_LOOPTOPTHV:
		case UC_LOOPTOP32:
		case UC_LOOPTOPS32:
		case UC_LOOPTOPTHV32:
			is_jump = true;
			params.reserve(8);
			break;
		case UC_CMPS:
		case UC_DEFAULT:
		case UC_CMPS32:
		case UC_DEFAULT32:
			is_jump = true;
			params.reserve(2);
			break;
		case UC_CONVERSE:
		case UC_JMP:
		case UC_JNE:
		case UC_TRYSTART:
		case UC_CONVERSE32:
		case UC_JMP32:
		case UC_JNE32:
		case UC_TRYSTART32:
			is_jump = true;
			break;
		case UC_CALLINDEX:
		case UC_PUSHB:
			is_jump = false;
			params.reserve(1);
			break;
		case UC_ADDSI:
		case UC_ADDSV:
		case UC_AIDX:
		case UC_AIDXS:
		case UC_AIDXTHV:
		case UC_ARRC:
		case UC_CALL:
		case UC_CALLE:
		case UC_CALLM:
		case UC_CALLO:
		case UC_CLSCREATE:
		case UC_DBGLINE:
		case UC_POP:
		case UC_POPARR:
		case UC_POPARRS:
		case UC_POPARRTHV:
		case UC_POPF:
		case UC_POPSTATIC:
		case UC_POPTHV:
		case UC_PUSH:
		case UC_PUSHF:
		case UC_PUSHI:
		case UC_PUSHS:
		case UC_PUSHSTATIC:
		case UC_PUSHTHV:
			is_jump = false;
			params.reserve(2);
			break;
		case UC_CALLI:
		case UC_CALLIS:
			is_jump = false;
			params.reserve(3);
			break;
		case UC_CALLMS:
		case UC_DBGFUNC:
		case UC_ADDSI32:
		case UC_CALL32:
		case UC_CALLE32:
		case UC_PUSHI32:
		case UC_PUSHS32:
			is_jump = false;
			params.reserve(4);
			break;
		case UC_DBGFUNC32:
			is_jump = false;
			params.reserve(6);
			break;
		default:
			is_jump = false;
			break;
		}
	}

	UsecodeOps get_opcode() const {
		return opcode;
	}

	void WriteParam1(unsigned short val) {
		Write1(params, val);
	}

	void WriteParam2(unsigned short val) {
		Write2(params, val);
	}

	void WriteParam4(unsigned int val) {
		Write4(params, val);
	}

private:
	uint32 get_real_opcode() const {
		if (op32bit) {
			return static_cast<uint32>(opcode) | 0x80u;
		}
		return opcode;
	}

public:
	void write(std::vector<char>& out) {
		Write1(out, get_real_opcode());
		out.insert(out.end(), params.begin(), params.end());
	}
#ifdef DEBUG    // For debugging.
	void write_text() const {
		std::cout << std::setw(2) << std::setfill('0') << get_real_opcode()
				  << ' ';
		for (const char param : params) {
			std::cout << std::setw(2) << std::setfill('0')
					  << static_cast<int>(static_cast<unsigned char>(param))
					  << ' ';
		}
		if (is_jump) {
			std::cout << "<offset>";
		}
	}
#endif
	size_t get_size() const {    // Total size, including offset.
		const size_t size = 1 + params.size();
		if (is_32bit()) {
			return size + 4;    // +4 for 32-bit offset.
		}
		if (is_jump) {
			return size + 2;    // +2 for 16-bit offset.
		}
		return size;
	}

	bool is_32bit() const {    // Only matters for jumps.
		return op32bit;
	}

	void set_32bit() {    // Only matters for jumps.
		if (is_jump) {
			op32bit = true;
		}
	}

	bool is_return() const {
		return opcode == UC_RET || opcode == UC_RET2 || opcode == UC_RETV
			   || opcode == UC_RETZ;
	}

	bool is_abort() const {
		return opcode == UC_ABRT || opcode == UC_THROW;
	}
};

/*
 *  A class representing a basic block. A basic block is a group of
 *  statements/instructions with the following properties:
 *      (1) There is only one entrance to the block;
 *      (2) There is only one exit from the block.
 *  Thus, when a basic block is being executed, whenever the first
 *  instruction of the block is executed, all the instructions of
 *  of that block will also be executed.
 *
 *  A basic block must be created when
 *      (1) The given instruction is a jump target;
 *      (2) the previous block reached its end by means of a
 *          branching instruction or a return.
 */

class Basic_block {
	friend void PopOpcode(Basic_block* dest);
	friend void WriteOp(Basic_block* dest, UsecodeOps val);
	friend void WriteOpParam1(Basic_block* dest, unsigned short val);
	friend void WriteOpParam2(Basic_block* dest, unsigned short val);
	friend void WriteOpParam4(Basic_block* dest, unsigned int val);
	friend void WriteJumpParam2(Basic_block* dest, unsigned short val);
	friend void WriteJumpParam4(Basic_block* dest, unsigned int val);

protected:
	std::set<Basic_block*> predecessors;    // Blocks that came before this.
	int index = 0;    // Block index in the fun_blocks array *or*
	// -1 for the starting ("phantom") block.

	Basic_block* taken
			= nullptr;    // For conditional jumps, block taken to by true
	// condition (fall-through); for unconditional jumps, block
	// to jump to; for all others, the next (fall-through) block.
	int taken_index = -1;    // Index in the fun_blocks array, filled after
	// cleaning up unreachable blocks.
	Basic_block* ntaken
			= nullptr;    // For conditional jumps, block taken to by false
	// condition (jump); for all others, this is zero.
	int ntaken_index = -1;    // Index in the fun_blocks array, filled after
	// cleaning up unreachable blocks.

	Opcode* jmp_op = nullptr;    // 0 for no instruction (fall-through) to taken
	// block; otherwise, one of:
	//  conditional jumps:
	//      UC_JNE, UC_CMPS, UC_CONVERSE, UC_LOOPTOP, UC_LOOPTOPS,
	//      UC_LOOPTOPTHV, UC_TRYSTART, UC_DEFAULT
	//  unconditional jumps:
	//      UC_JMP
	// or the 32-bit versions of these instructions.

	std::vector<Opcode*> instructions;    // The instructions of the block
	bool                 reachable = false;

public:
	Basic_block() {
		instructions.reserve(100);
	}

	Basic_block(
			int ind, Basic_block* t = nullptr, Basic_block* n = nullptr,
			UsecodeOps ins = UC_INVALID)
			: index(ind), taken(t), ntaken(n), jmp_op(new Opcode(ins)) {
		if (index != -1) {
			instructions.reserve(100);
		}
	}

	~Basic_block() {
		predecessors.clear();
		for (auto* op : instructions) {
			delete op;
		}
		instructions.clear();
		delete jmp_op;
	}

	int get_index() const {
		return index;
	}

	void set_index(int ind) {
		index = ind;
	}

	UsecodeOps get_opcode() const {
		return jmp_op != nullptr ? jmp_op->get_opcode() : UC_INVALID;
	}

	void clear_jump() {
		delete jmp_op;
		jmp_op = nullptr;
	}

	UsecodeOps get_last_instruction() const {
		return !instructions.empty() ? instructions.back()->get_opcode()
									 : UC_INVALID;
	}

	void set_32bit_jump() {
		if (jmp_op != nullptr) {
			jmp_op->set_32bit();
		}
	}

	bool is_32bit_jump() {
		return jmp_op != nullptr ? jmp_op->is_32bit() : false;
	}

	bool does_not_jump() const {
		return jmp_op == nullptr;
	}

	bool ends_in_return() const {
		return !instructions.empty() && instructions.back()->is_return();
	}

	bool ends_in_abort() const {
		return !instructions.empty() && instructions.back()->is_abort();
	}

	size_t get_jump_size() const {
		if (jmp_op == nullptr) {
			return 0;    // Fall-through block or terminating block
		}
		return jmp_op->get_size();
	}

	size_t get_block_size() const {
		size_t size = get_jump_size();
		for (auto* op : instructions) {
			size += op->get_size();
		}
		return size;
	}

	bool is_jump_block() const {
		return jmp_op != nullptr ? (jmp_op->get_opcode() == UC_JMP) : false;
	}

	bool is_simple_jump_block() const {
		return is_jump_block() && instructions.empty();
	}

	bool is_conditionaljump_block() const {
		// instructions shouldn't be empty in this case.
		return jmp_op != nullptr ? (jmp_op->get_opcode() == UC_JNE) : false;
	}

	bool is_array_loop_block() const {
		if (jmp_op == nullptr) {
			return false;
		}
		const auto opcode = jmp_op->get_opcode();
		return opcode == UC_LOOPTOP || opcode == UC_LOOPTOPS
			   || opcode == UC_LOOPTOPTHV;
	}

	bool is_converse_case_block() const {
		if (jmp_op == nullptr) {
			return false;
		}
		const auto opcode = jmp_op->get_opcode();
		return opcode == UC_CMPS || opcode == UC_DEFAULT;
	}

	bool is_fallthrough_block() const {
		return jmp_op == nullptr && taken != nullptr;
	}

	bool is_empty_block() const {
		return jmp_op == nullptr && (instructions.empty());
	}

	bool is_end_block() const {
		return jmp_op == nullptr && taken != nullptr && (taken->index == -1);
	}

	bool is_forced_target() const {
		for (auto* block : predecessors) {
			if (!block->is_jump_block() && !block->is_fallthrough_block()) {
				return false;
			}
		}
		return true;
	}

	UsecodeOps get_return_opcode() const {
		return ends_in_return() ? instructions.back()->get_opcode()
								: UC_INVALID;
	}

	bool is_simple_return_block() const {
		return instructions.size() == 1 && instructions.back()->is_return();
	}

	bool is_simple_abort_block() const {
		return is_end_block() && instructions.size() == 1
			   && instructions.back()->is_abort();
	}

	bool is_childless() const {
		return taken == nullptr && ntaken == nullptr;
	}

	bool no_parents() const {
		return predecessors.empty();
	}

	bool is_orphan() const {
		return index >= 0 && no_parents();
	}

	bool is_reachable() const {
		return reachable;
	}

	void mark_reachable() {
		if (reachable) {
			return;
		}
		reachable = true;
		if (taken != nullptr) {
			taken->mark_reachable();
		}
		if (ntaken != nullptr) {
			ntaken->mark_reachable();
		}
	}

	Basic_block* get_taken() {
		return taken;
	}

	Basic_block* get_ntaken() {
		return ntaken;
	}

	int get_taken_index() {
		return taken_index;
	}

	int get_ntaken_index() {
		return ntaken_index;
	}

	bool has_single_predecessor() const {
		return predecessors.size() == 1;
	}

	size_t predecessor_count() const {
		return predecessors.size();
	}

	void set_taken(Basic_block* dest) {
		if (dest != nullptr) {
			dest->predecessors.insert(this);
		}
		if (taken != nullptr) {
			taken->predecessors.erase(this);
		}
		taken = dest;
	}

	void set_ntaken(Basic_block* dest) {
		if (dest != nullptr) {
			dest->predecessors.insert(this);
		}
		if (ntaken != nullptr) {
			ntaken->predecessors.erase(this);
		}
		ntaken = dest;
	}

	void set_targets(
			UsecodeOps op, Basic_block* t = nullptr, Basic_block* n = nullptr) {
		clear_jump();
		if (op != UC_INVALID) {
			jmp_op = new Opcode(op);
		}
		set_taken(t);
		set_ntaken(n);
	}

	void unlink_descendants() {
		if (taken != nullptr) {
			taken->predecessors.erase(this);
			taken = nullptr;
		}
		if (ntaken != nullptr) {
			ntaken->predecessors.erase(this);
			ntaken = nullptr;
		}
	}

	void link_predecessors() {
		for (auto* block : predecessors) {
			if (block->taken == this) {
				block->taken_index = index;
			}
			if (block->ntaken == this) {
				block->ntaken_index = index;
			}
		}
	}

	void unlink_predecessors() {
		for (auto* block : predecessors) {
			if (block->taken == this) {
				block->taken       = nullptr;
				block->taken_index = -1;
			}
			if (block->ntaken == this) {
				block->ntaken       = nullptr;
				block->ntaken_index = -1;
			}
		}
		predecessors.clear();
	}

	void link_through_block() {
		for (auto* pred : predecessors) {
			// Do NOT use set_taken, set_ntaken!
			if (pred->taken == this) {
				if (taken != nullptr) {    // Check almost unneeded.
					taken->predecessors.insert(pred);
				}
				pred->taken = taken;
			}
			if (pred->ntaken == this) {
				if (ntaken != nullptr) {
					ntaken->predecessors.insert(pred);
					pred->ntaken = ntaken;
				} else if (taken != nullptr) {
					taken->predecessors.insert(pred);
					pred->ntaken = taken;
				} else {
					pred->ntaken = nullptr;
				}
			}
		}
		predecessors.clear();
		unlink_descendants();
	}

	void merge_taken() {
		Basic_block* safetaken = taken;
		instructions.insert(
				instructions.end(), safetaken->instructions.begin(),
				safetaken->instructions.end());
		delete jmp_op;
		jmp_op = safetaken->jmp_op;
		set_taken(safetaken->taken);
		set_ntaken(safetaken->ntaken);
		// Prevent these from being deleted.
		safetaken->instructions.clear();
		safetaken->jmp_op = nullptr;
	}

	void write(std::vector<char>& out) {
		for (auto* op : instructions) {
			op->write(out);
		}
		if (jmp_op != nullptr) {
			jmp_op->write(out);
		}
	}
#ifdef DEBUG    // For debugging.
	void check() const {
		std::cout << std::hex << std::setw(8) << std::setfill('0') << this
				  << '\t' << std::setw(8) << std::setfill('0') << taken << '\t'
				  << std::setw(8) << std::setfill('0') << ntaken << '\t';
		for (const auto* instruction : instructions) {
			instruction->write_text();
		}
		if (jmp_op != nullptr) {
			jmp_op->write_text();
		} else {
			std::cout << "<no jump>";
		}
		std::cout << std::endl;
	}
#endif
};

/*
 *  Removes last opcode from the instruction block.
 */

inline void PopOpcode(Basic_block* dest) {
	if (dest->instructions.empty()) {
		return;
	}
	Opcode* op = dest->instructions.back();
	dest->instructions.pop_back();
	delete op;
}

/*
 *  Write an opcode value to the end of the instruction block
 *  and marks it as the last opcode.
 */

inline void WriteOp(Basic_block* dest, UsecodeOps val) {
	auto* op = new Opcode(val);
	dest->instructions.push_back(op);
}

/*
 *  Write a 1-byte value to the end of the jump parameters.
 */

inline void WriteOpParam1(Basic_block* dest, unsigned short val) {
	if (dest->instructions.empty()) {
		return;
	}
	dest->instructions.back()->WriteParam1(val);
}

/*
 *  Write a 2-byte value to the end of the jump parameters.
 */

inline void WriteOpParam2(Basic_block* dest, unsigned short val) {
	if (dest->instructions.empty()) {
		return;
	}
	dest->instructions.back()->WriteParam2(val);
}

/*
 *  Write a 4-byte value to the end of the jump parameters.
 */

inline void WriteOpParam4(Basic_block* dest, unsigned int val) {
	if (dest->instructions.empty()) {
		return;
	}
	dest->instructions.back()->WriteParam4(val);
}

/*
 *  Write a 2-byte value to the end of the jump parameters.
 */

inline void WriteJumpParam2(Basic_block* dest, unsigned short val) {
	if (dest->jmp_op == nullptr) {
		return;
	}
	dest->jmp_op->WriteParam2(val);
}

/*
 *  Write a 4-byte value to the end of the jump parameters.
 */

inline void WriteJumpParam4(Basic_block* dest, unsigned int val) {
	if (dest->jmp_op == nullptr) {
		return;
	}
	dest->jmp_op->WriteParam4(val);
}

#endif
