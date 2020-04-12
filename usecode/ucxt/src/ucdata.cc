/*
 *  Copyright (C) 2001-2013  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <set>
#include <vector>
#include <sstream>
#include <stdexcept>
#include "ucdata.h"
#include "ops.h"
#include "files/utils.h"
#include "usecode/ucsymtbl.h"
#include "headers/ios_state.hpp"

using std::cout;
using std::setw;
using std::endl;
using std::vector;
using std::setbase;
using std::setfill;
using std::string;
using std::ostream;

const string CLASSNAME = "class";

UCData::UCData() = default;

UCData::~UCData() {
	_file.close();
	for (unsigned int i = 0; i < _funcs.size(); i++)
		delete _funcs[i];
	delete _symtbl;
}

void UCData::parse_params(const unsigned int argc, char **argv) {
	/* Parse command line */
	for (unsigned int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-si") == 0) options._game = UCOptions::GAME_SI;
		else if (strcmp(argv[i], "-bg") == 0) options._game = UCOptions::GAME_BG;
		else if (strcmp(argv[i], "-ss") == 0) options._game = UCOptions::GAME_SS;
		else if (strcmp(argv[i], "-fov") == 0) options._game = UCOptions::GAME_FOV;
		else if (strcmp(argv[i], "-sib") == 0) options._game = UCOptions::GAME_SIB;
		else if (strcmp(argv[i], "-u8") == 0) options._game = UCOptions::GAME_U8;

		else if (strcmp(argv[i], "-a") == 0) options.mode_all = true;

		else if (strcmp(argv[i], "-nc") == 0) options.noconf  = true;
		else if (strcmp(argv[i], "-ro") == 0) options.rawops  = true;
		else if (strcmp(argv[i], "-ac") == 0) options.autocomment = true;
		else if (strcmp(argv[i], "-uc") == 0) options.uselesscomment = true;
		else if (strcmp(argv[i], "-v") == 0) options.verbose = true;
		else if (strcmp(argv[i], "-vv") == 0) {
			options.verbose = options.very_verbose = true;
		} else if (strcmp(argv[i], "-dbg") == 0) options.ucdebug = true;
		else if (strcmp(argv[i], "-ext32") == 0) options.force_ext32 = true;
		else if (strcmp(argv[i], "-b") == 0) options.basic = true;

		else if (strcmp(argv[i], "-fl") == 0) options.output_list = true;
		else if (strcmp(argv[i], "-fa") == 0) options.output_asm  = true;
		else if (strcmp(argv[i], "-fz") == 0) options.output_ucs  = true;
		else if (strcmp(argv[i], "-fs") == 0) options.output_ucs  = true;
		else if (strcmp(argv[i], "-ff") == 0) options.output_flag = true;
		else if (strcmp(argv[i], "-ftt") == 0) options.output_trans_table = true;

		else if (strcmp(argv[i], "--extern-header") == 0) options.output_extern_header = true;

		else if (argv[i][0] != '-') {
			char *stopstr;
			/* Disassembly mode */
			unsigned int search_func = static_cast<unsigned int>(strtoul(argv[i], &stopstr, 16));
			if (stopstr - argv[i] < static_cast<int>(strlen(argv[i])))
				/* Invalid number */
			{ /* Do Nothing */
			} else {
				search_funcs.push_back(search_func);
				if (options.verbose) cout << "Disassembling Function: " << search_func << endl;
				options.mode_dis = true;
			}
		} else if ((string(argv[i]).size() > 2) && string(argv[i]).substr(0, 2) == "-o") {
			_output_redirect = string(argv[i]).substr(2, string(argv[i]).size() - 2);
			if (options.verbose) cout << "Outputting to filename: " << _output_redirect << endl;
		} else if ((string(argv[i]).size() > 2) && string(argv[i]).substr(0, 2) == "-i") {
			_input_usecode_file = string(argv[i]).substr(2, string(argv[i]).size() - 2);
			if (options.verbose) cout << "Inputting from file: " << _input_usecode_file << endl;
		} else if ((string(argv[i]).size() > 2) && string(argv[i]).substr(0, 2) == "-g") {
			_global_flags_file = string(argv[i]).substr(2, string(argv[i]).size() - 2);
			if (options.verbose) cout << "Reading flag names from file: " << _global_flags_file << endl;
		} else {
			cout << "unsupported parameter " << argv[i] << " detected. countinuing." << endl;
		}
	}
}

void UCData::open_usecode(const string &filename) {
	file_open(filename);

	if (fail())
		return;
}

void UCData::disassamble(ostream &o) {
	boost::io::ios_flags_saver flags(o);
	boost::io::ios_fill_saver fill(o);
	load_globals(o);
	load_funcs(o);
	analyse_classes();

	if (options.verbose) {
		for (vector<unsigned int>::iterator i = search_funcs.begin(); i != search_funcs.end(); ++i)
			o << "Looking for function number " << setw(8) << (*i) << endl;
		o << endl;
	}

	if (options.output_list)
		o << "Function       offset    size  data  code" << (options.ucdebug ? " funcname" : "") << endl;

	if (options.output_trans_table)
		o << "<trans>" << endl;

	if (options.output_ucs && UCFunc::num_global_statics > 0) {
		o << "// Global static variables" << endl;
		for (int i = 1; i <= UCFunc::num_global_statics; i++)
			o << UCFunc::STATICNAME << ' ' << UCFunc::GLOBALSTATICPREFIX << std::setw(4) << i << ';' << endl;
		o << endl;
	}

	Usecode_class_symbol *cls = nullptr;
	bool _foundfunc = false; //did we find and print the function?
	for (unsigned int i = 0; i < _funcs.size(); i++) {
		if (options.mode_all || (options.mode_dis && count(search_funcs.begin(), search_funcs.end(), _funcs[i]->_funcid))) {
			_foundfunc = true;
			bool _func_printed = false; // to test if we've actually printed a function ouput

			if (options.output_list)
				_func_printed = _funcs[i]->output_list(o, i, options);

			if (options.output_ucs) {
				int indent = 0;
				if (cls != _funcs[i]->_cls) {
					if (cls) {
						o << "}" << endl << endl;
					}
					cls = _funcs[i]->_cls;
					if (cls) {
						o << CLASSNAME << " " << cls->get_name();
						// Does the class inherit from another?
						InheritMap::const_iterator it = _clsmap.find(cls);
						int initvar = 0;
						if (it != _clsmap.end()) {
							// Yes; print base class.
							o << " : " << it->second->get_name();
							initvar = it->second->get_num_vars();
						}
						o << endl << "{" << endl;
						indent = 1;
						for (int i = initvar; i < cls->get_num_vars(); i++)
							tab_indent(indent, o) << UCFunc::VARNAME << ' ' << UCFunc::CLASSVARPREFIX << std::setw(4) << i << ';' << endl;
					}
				} else if (cls) {
					indent = 1;
				}
				_funcs[i]->parse_ucs(_funcmap, uc_intrinsics, options, _symtbl);
				_func_printed = _funcs[i]->output_ucs(o, _funcmap, uc_intrinsics, options, indent, _symtbl);
				if (!cls)
					o << endl;
				//_func_printed=true;
			}

			if (options.output_trans_table) {
				_func_printed = _funcs[i]->output_tt(o);
				//_func_printed=true;
			}

			// if we haven't printed one by now, we'll print an asm output.
			if (options.output_asm || (!_func_printed))
				_funcs[i]->output_asm(o, _funcmap, uc_intrinsics, options, _symtbl);
		}
	}

	if (!_foundfunc)
		o << "Function not found." << endl;

	if (search_funcs.empty())
		o << "Functions: " << _funcs.size() << endl;

	if (options.output_list)
		o << endl << "Functions: " << setbase(10) << _funcs.size() << endl;

	if (options.output_trans_table)
		o << "</>" << endl;

	o << endl;
}

/* FIXME: Need to remove the hard coded opcode numbers (0x42 and 0x43) and replace them
    with 'variables' in the opcodes.txt file, that signify if it's a pop/push and a flag */
void UCData::dump_flags(ostream &o) {
	if (!options.game_u7()) {
		o << "This option only works for U7:BG, U7:FoV, U7:SI Beta, U7:SI and U7:SS" << endl;
		return;
	}
	load_funcs(o);

	if (options.verbose) cout << "Finding flags..." << endl;
	vector<FlagData> flags;

	// *BLEH* ugly!
	for (vector<UCFunc *>::iterator func = _funcs.begin(); func != _funcs.end(); ++func)
		for (vector<UCc>::iterator op = (*func)->_opcodes.begin(); op != (*func)->_opcodes.end(); ++op) {
			if (op->_id == 0x42)
				flags.push_back(FlagData((*func)->_funcid, op->_offset, op->_params_parsed[0], FlagData::GETFLAG));
			else if (op->_id == 0x43)
				flags.push_back(FlagData((*func)->_funcid, op->_offset, op->_params_parsed[0], FlagData::SETFLAG));
		}

	o << "Number of flags found: " << setbase(10) << flags.size() << endl << endl;

	// output per function
	{
		sort(flags.begin(), flags.end(), SortFlagDataLessFunc());

		o << setbase(16) << setfill('0');
		int currfunc = -1;
		for (unsigned int i = 0; i < flags.size(); i++) {
			if (currfunc != flags[i].func()) {
				o << "Function: " << setw(4) << flags[i].func() << endl;
				currfunc = static_cast<int>(flags[i].func());
				o << "              flag  offset" << endl;
			}

			o << "        ";
			if (flags[i].access() == FlagData::GETFLAG)
				o << "push  ";
			else if (flags[i].access() == FlagData::SETFLAG)
				o << "pop   ";
			o << setw(4) << flags[i].flag()   << "  "
			  << setw(4) << flags[i].offset() << endl;
		}
	}

	// output per flag
	{
		sort(flags.begin(), flags.end(), SortFlagDataLessFlag());

		o << setbase(16) << setfill('0');
		unsigned int currflag = static_cast<unsigned int>(-1);
		for (unsigned int i = 0; i < flags.size(); i++) {
			if (currflag != flags[i].flag()) {
				o << "Flag: " << setw(4) << flags[i].flag() << endl;
				currflag = flags[i].flag();
				o << "              func  offset" << endl;
			}

			o << "        ";
			if (flags[i].access() == FlagData::GETFLAG)
				o << "push  ";
			else if (flags[i].access() == FlagData::SETFLAG)
				o << "pop   ";
			o << setw(4) << flags[i].func()   << "  "
			  << setw(4) << flags[i].offset() << endl;
		}
	}
}

void UCData::file_open(const string &filename) {
	/* Open a usecode file */
	try {
		_file.clear();
		U7open(_file, filename.c_str(), false);
	} catch (const std::exception &err) {
		_file.setstate(std::ifstream::failbit);
	}
}

#undef LOAD_SPEED_TEST

#ifdef LOAD_SPEED_TEST
#include "tools/dbgutils.h"
#endif

void UCData::load_globals(ostream &o) {
	if (_global_flags_file.empty())
		return;
	try {
		std::ifstream gflags;
		U7open(gflags, _global_flags_file.c_str(), false);
		std::map<unsigned int, std::string>& FlagMap = UCFunc::FlagMap;
		std::set<std::string> flags;
		unsigned int ii = 0;
		o << "enum GlobalFlags {" << endl;
		std::string flagname;
		std::getline(gflags, flagname, '\0');
		bool first = true;
		boost::io::ios_flags_saver oflags(o);
		boost::io::ios_fill_saver fill(o);
		o << setbase(16) << setfill('0');
		while (gflags.good()) {
			if (!flagname.empty()) {
				if (flagname[0] == '$')
					flagname.erase(0, 1);
				if (flags.find(flagname) != flags.end()) {
					std::stringstream snum;
					snum << ii;
					flagname += snum.str();
				}
				FlagMap[ii] = flagname;
				if (!first)
					o << ',' << endl;
				else
					first = false;
				o << '\t' << flagname << " = 0x" << setw(4) << ii;
			}
			ii++;
			std::getline(gflags, flagname, '\0');
		};
		o << endl << "};" << endl << endl;
	} catch (const std::exception &err) {
		cout << "error. failed to open " << _global_flags_file << ". exiting." << endl;
		exit(1);
	}
}

void UCData::load_funcs(ostream &o) {
	if (options.game_u7() && Usecode_symbol_table::has_symbol_table(_file)) {
		delete _symtbl;
		if (options.verbose) o << "Loading symbol table..." << endl;
		_symtbl = new Usecode_symbol_table();
		_symtbl->read(_file);
	}

	if (options.verbose) o << "Loading functions..." << endl;

#ifdef LOAD_SPEED_TEST
	dbg::DateDiff dd;
	dbg::timeDateDiff(o);
	dd.start();
#endif

	bool eof = false;
	while (!eof) {
		UCFunc *ucfunc = new UCFunc();

		if (options.game_u7())
			readbin_U7UCFunc(_file, *ucfunc, options, _symtbl);
		else if (options.game_u8())
			readbin_U8UCFunc(_file, *ucfunc);
		else
			exit(-1); // can't happen

		/* if we're forcing ext32 on output, this is where we do so.
		    if we try doing it before reading it, it'll get corrupted. */
		if (options.force_ext32) ucfunc->ext32 = true;

		_funcs.push_back(ucfunc);
		{
			_file.get();
			eof = _file.eof();
			_file.unget();
		}
	}

#ifdef LOAD_SPEED_TEST
	dd.end();
	o << setbase(10) << setfill(' ');
	dd.print_start(o) << endl;
	dd.print_end(o) << endl;
	dd.print_diff(o) << endl;
	o << setbase(16) << setfill('0');
#endif

	if (options.verbose) o << "Creating function map..." << endl;

	for (vector<UCFunc *>::iterator i = _funcs.begin(); i != _funcs.end(); ++i) {
		int funcid = (*i)->_funcid;
		Usecode_symbol::Symbol_kind kind;
		if ((*i)->_sym)
			kind = (*i)->_sym->get_kind();
		else if (funcid < 0x400)
			kind = Usecode_symbol::shape_fun;
		else if (funcid < 0x800)
			kind = Usecode_symbol::object_fun;
		else
			kind = Usecode_symbol::fun_defined;
		_funcmap.insert(FuncMapPair((*i)->_funcid, UCFuncSet(funcid, (*i)->_num_args,
		                                                     (*i)->return_var, (*i)->aborts,
		                                                     (*i)->_cls != nullptr, (*i)->funcname,
		                                                     kind, (*i)->_varmap)));
	}
	/*  for(map<unsigned int, UCFuncSet>::iterator i=_funcmap.begin(); i!=_funcmap.end(); ++i)
	        o << i->first << "\t" << i->second.num_args << endl;*/
}

void UCData::analyse_classes() {
	if (!_symtbl)
		return;
	int nclasses = _symtbl->get_num_classes();
	// Class 0 can't inherit from any others.
	for (int i = 1; i < nclasses; i++) {
		Usecode_class_symbol *cls = _symtbl->get_class(i);
		// Can only inherit from previously-defined classes.
		for (int j = i - 1; j >= 0; j--) {
			Usecode_class_symbol *base = _symtbl->get_class(j);
			assert(base != nullptr);
			// If "base" has more variables or more methods than "cls", it can't be a base class of "cls".
			if (base->get_num_vars() > cls->get_num_vars() || base->get_num_methods() > cls->get_num_methods())
				continue;
			// Assume this is a base class.
			bool is_base = true;
			// Find nonmatching method. This will be the first method in UCC output.
			for (int k = 0; k < base->get_num_methods(); k++) {
				if (base->get_method_id(k) != cls->get_method_id(k)) {
					is_base = false;
					break;
				}
			}
			// All methods match?
			if (is_base) {
				// Yes. Insert in map then break (no multiple inheritance in UCC).
				_clsmap.insert(std::make_pair(cls, base));
				break;
			}
		}
	}
}

void UCData::output_extern_header(ostream &o) {
	if (!options.game_u7()) {
		o << "This option only works for U7:BG, U7:FoV, U7:SI beta, U7:SI and U7:SS" << endl;
		return;
	}
	load_funcs(o);

	for (vector<UCFunc *>::iterator func = _funcs.begin(); func != _funcs.end(); ++func) {
		//(*func)->output_ucs_funcname(o << "extern ", _funcmap, (*func)->_funcid, (*func)->_num_args, (*func)->return_var) << ';' << endl;
		(*func)->output_ucs_funcname(o << "extern ", _funcmap, _symtbl) << ';' << endl;
	}
}


