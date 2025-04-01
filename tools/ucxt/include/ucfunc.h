/*
 *  Copyright (C) 2001-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef UCFUNC_H
#define UCFUNC_H

#include "ucc.h"
#include "usecode/ucsymtbl.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

class UCFuncSet {
public:
	UCFuncSet(
			unsigned int new_funcid, unsigned int new_num_args,
			bool new_return_var, bool new_aborts, bool new_class_fun,
			const std::string&                   new_funcname,
			Usecode_symbol::Symbol_kind          new_kind,
			std::map<unsigned int, std::string>& new_varmap)
			: funcid(new_funcid), num_args(new_num_args),
			  return_var(new_return_var), aborts(new_aborts),
			  class_fun(new_class_fun), funcname(new_funcname), kind(new_kind),
			  varmap(new_varmap) {}

	unsigned int funcid;        // the id of the function
	unsigned int num_args;      // the number of arguments
	bool         return_var;    // if true, the function returns a variable
	bool         aborts;        // true if the function may not return
	bool         class_fun;     // true if this is a class member function
	std::string  funcname;      // the name of the function, if it has one
	Usecode_symbol::Symbol_kind          kind;      // Type of function.
	std::map<unsigned int, std::string>& varmap;    // Variable names
};

using FuncMap     = std::map<unsigned int, UCFuncSet>;
using FuncMapPair = std::pair<unsigned int, UCFuncSet>;

// #define DEBUG_GOTOSET
const unsigned int SIZEOF_USHORT = 2;

class FlagData {
public:
	enum {
		SETFLAG = false,
		GETFLAG = true
	};

	/* SETFLAG=UCC_POPF, GETFLAG=UCC_PUSHF */
	enum {
		POP  = false,
		PUSH = true
	};

	FlagData(
			const long func, const unsigned int offset, const unsigned int flag,
			const bool access)
			: _func(func), _offset(offset), _flag(flag), _access(access) {}

	long func() const {
		return _func;
	}

	unsigned int offset() const {
		return _offset;
	}

	unsigned int flag() const {
		return _flag;
	}

	bool access() const {
		return _access;
	}

private:
	long         _func;
	unsigned int _offset;
	unsigned int _flag;
	bool         _access;
};

class SortFlagDataLessFlag {
public:
	bool operator()(const FlagData& fd1, const FlagData& fd2) const {
		return (fd1.flag() < fd2.flag())   ? true
			   : (fd1.flag() > fd2.flag()) ? false
			   : (fd1.func() < fd2.func()) ? true
			   : (fd1.func() > fd2.func()) ? false
										   : (fd1.offset() < fd2.offset());
	}

	bool operator()(const FlagData* fd1, const FlagData* fd2) const {
		return (fd1->flag() < fd2->flag())   ? true
			   : (fd1->flag() > fd2->flag()) ? false
			   : (fd1->func() < fd2->func()) ? true
			   : (fd1->func() > fd2->func()) ? false
											 : (fd1->offset() < fd2->offset());
	}
};

class SortFlagDataLessFunc {
public:
	bool operator()(const FlagData& fd1, const FlagData& fd2) const {
		return (fd1.func() < fd2.func())   ? true
			   : (fd1.func() > fd2.func()) ? false
			   : (fd1.flag() < fd2.flag()) ? true
			   : (fd1.flag() > fd2.flag()) ? false
										   : (fd1.offset() < fd2.offset());
	}

	bool operator()(const FlagData* fd1, const FlagData* fd2) const {
		return (fd1->func() < fd2->func())   ? true
			   : (fd1->func() > fd2->func()) ? false
			   : (fd1->flag() < fd2->flag()) ? true
			   : (fd1->flag() > fd2->flag()) ? false
											 : (fd1->offset() < fd2->offset());
	}
};

class UCNode;

class UCNode {
public:
	UCNode(UCc* newucc = nullptr) : ucc(newucc) {}

	UCc*                 ucc;
	std::vector<UCNode*> nodelist;
};

#include "ops.h"

class GotoSet {
public:
	GotoSet() = default;

	GotoSet(const unsigned int offset, UCc* ucc) : _offset(offset) {
		add(ucc);
	}

	GotoSet(UCc* ucc) : _offset(ucc->_offset) {
		add(ucc);
	}

	std::vector<std::pair<UCc*, bool>>& operator()() {
		return _uccs;
	}

	UCc& operator[](const unsigned int i) {
		return *(_uccs[i].first);
	}

	unsigned int size() const {
		return static_cast<unsigned int>(_uccs.size());
	}

	void add(UCc* ucc, bool gc = false) {
		_uccs.emplace_back(ucc, gc);
	}

	/* Just a quick function to remove all the Uccs in _uccs marked for
	   garbage collection. */
	void gc() {
		for (auto j = _uccs.begin(); j != _uccs.end();) {
#ifdef DEBUG_GOTOSET
			std::cout << "OP: " << opcode_table_data[j->first->_id].ucs_nmo
					  << '\t' << j->second;
#endif
			if (j->second) {
				/* ok, we need to take into consideration that the
				   iterator we're removing might ==_uccs.begin() */
				bool begin = false;
				if (j == _uccs.begin()) {
					begin = true;
				}

#ifdef DEBUG_GOTOSET
				std::cout << "\tremoved";
#endif

				auto rem(j);

				if (!begin) {
					--j;
				}

				_uccs.erase(rem);

				if (begin) {
					j = _uccs.begin();
				} else {
					++j;
				}

				if (j == _uccs.end()) {
					std::cout << "POTENTIAL PROBLEM" << std::endl;
				}
			} else {
				++j;
			}

#ifdef DEBUG_GOTOSET
			std::cout << std::endl;
#endif
		}
	}

	unsigned int offset() const {
		return _offset;
	}

private:
	unsigned int                       _offset = 0;
	std::vector<std::pair<UCc*, bool>> _uccs;
};

class UCOpcodeData;

class UCFunc {
public:
	UCFunc() = default;

	bool output_list(
			std::ostream& o, unsigned int funcno, const UCOptions& options);

	bool output_ucs(
			std::ostream& o, const FuncMap& funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, int new_indent,
			Usecode_symbol_table* symtbl);
	std::ostream& output_ucs_funcname(
			std::ostream& o, const FuncMap& funcmap,
			Usecode_symbol_table* symtbl);
	std::ostream& output_ucs_funcname(
			std::ostream& o, const FuncMap& funcmap, unsigned int funcid,
			unsigned int numargs, Usecode_symbol_table* symtbl);
	void output_ucs_node(
			std::ostream& o, const FuncMap& funcmap, UCNode* ucn,
			const std::map<unsigned int, std::string>& intrinsics,
			unsigned int indent, const UCOptions& options,
			Usecode_symbol_table* symtbl);
	void output_ucs_data(
			std::ostream& o, const FuncMap& funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, unsigned int indent,
			Usecode_symbol_table* symtbl);
	void output_ucs_opcode(
			std::ostream& o, const FuncMap& funcmap,
			const std::vector<UCOpcodeData>& optab, const UCc& op,
			const std::map<unsigned int, std::string>& intrinsics,
			unsigned int indent, Usecode_symbol_table* symtbl);

	void parse_ucs(
			const FuncMap&                             funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, Usecode_symbol_table* symtbl);
	void parse_ucs_pass1(std::vector<UCNode*>& nodes);
	void parse_ucs_pass2(
			std::vector<GotoSet>& gotoset, const FuncMap& funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, Usecode_symbol_table* symtbl);
	std::vector<UCc*> parse_ucs_pass2a(
			std::vector<std::pair<UCc*, bool>>::reverse_iterator current,
			std::vector<std::pair<UCc*, bool>>& vec, int opsneeded,
			const FuncMap&                             funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, Usecode_symbol_table* symtbl);
	void parse_ucs_pass3(
			std::vector<GotoSet>&                      gotoset,
			const std::map<unsigned int, std::string>& intrinsics);

	bool output_asm(
			std::ostream& o, const FuncMap& funcmap,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCOptions& options, Usecode_symbol_table* symtbl);
	void output_asm_data(std::ostream& o);
	void output_asm_opcode(
			std::ostream& o, const FuncMap& funcmap,
			const std::vector<UCOpcodeData>&           optab,
			const std::map<unsigned int, std::string>& intrinsics,
			const UCc& op, const UCOptions& options,
			Usecode_symbol_table* symtbl);
	void output_raw_opcodes(std::ostream& o, const UCc& op);

	bool output_tt(std::ostream& o);

	//	private:

	std::vector<GotoSet> gotoset;

	std::streampos _offset   = 0;    // offset to start of function
	unsigned int   _funcid   = 0;    // the id of the function
	unsigned int   _funcsize = 0;    // the size of the function (bytes)
	std::streampos _bodyoffset
			= 0;    // the file position after the header is read

	unsigned int _datasize = 0;    // the size of the data block

	std::map<unsigned int, std::string> _data;
	std::map<unsigned int, std::string> _varmap;
	// contains the entire data segment in offset from start of segment, and
	// string data pairs

	std::streampos _codeoffset
			= 0;    // the offset to the start of the code segment

	unsigned int _num_args    = 0;    // the number of arguments
	unsigned int _num_locals  = 0;    // the number of local variables
	unsigned int _num_externs = 0;    // the number of external function id's
	int          _num_statics
			= 0;    // the number of static variables defined in the function
	std::vector<unsigned int> _externs;    // the external function id's

	std::vector<UCc> _opcodes;

	bool         return_var = false;    // does the function return a variable?
	bool         aborts     = false;    // true if the function may not return.
	bool         debugging_info   = false;
	unsigned int debugging_offset = 0;
	std::string  funcname;
	Usecode_symbol*       _sym = nullptr;
	Usecode_class_symbol* _cls = nullptr;    // Class member functions

	bool ext32 = false;    // is this function an extended function?

	unsigned int codesize() const {
		return _funcsize - _datasize;
	}

	UCNode node;

	// A few static variables.
	static int num_global_statics;    // the number of global static variables
	static const std::string                   VARPREFIX;
	static const std::string                   CLASSVARPREFIX;
	static const std::string                   STATICPREFIX;
	static const std::string                   GLOBALSTATICPREFIX;
	static const std::string                   THISVAR;
	static const std::string                   VARNAME;
	static const std::string                   STATICNAME;
	static const std::string                   NORETURN;
	static const std::string                   SHAPENUM;
	static const std::string                   OBJECTNUM;
	static std::map<unsigned int, std::string> FlagMap;
};

void readbin_U7UCFunc(
		std::istream& f, UCFunc& ucf, const UCOptions& options,
		Usecode_symbol_table* symtbl);
void readbin_U8UCFunc(std::istream& f, UCFunc& ucf);

std::ostream& tab_indent(const unsigned int indent, std::ostream& o);
#endif
