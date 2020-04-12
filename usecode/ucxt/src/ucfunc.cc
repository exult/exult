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

#include "ucdata.h"
#include "ucfunc.h"
#include <set>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include "files/utils.h"
#include "usecode/ucsymtbl.h"
#include "headers/ignore_unused_variable_warning.h"
#include "headers/ios_state.hpp"

#include "ops.h"

#ifdef DEBUG_UCXT
#define DEBUG_INDENT
#define DEBUG_PARSE
#define DEBUG_PARSE2
#define DEBUG_PARSE2a
#define DEBUG_READ
#define DEBUG_PRINT
#define DEBUG_READ_PAIR(X, Y) cout << '\t' << X << '\t' << Y << endl;
#else
#undef DEBUG_INDENT
#undef DEBUG_PARSE
#undef DEBUG_PARSE2
#undef DEBUG_READ
#undef DEBUG_PRINT
#define DEBUG_READ_PAIR(X, Y)
#endif

using std::ostream;
using std::ifstream;
using std::string;
using std::vector;
using std::map;
using std::endl;
using std::pair;
using std::ios;
using std::streampos;
using std::cout;
using std::setw;
using std::less;

const string UCFunc::VARPREFIX = "var";
const string UCFunc::CLASSVARPREFIX = "cvar";
const string UCFunc::STATICPREFIX = "lsvar";
const string UCFunc::GLOBALSTATICPREFIX = "gsvar";
const string UCFunc::THISVAR = "this";
const string UCFunc::VARNAME = "var";
const string UCFunc::STATICNAME = "static var";
const string UCFunc::NORETURN = "void";
const string UCFunc::SHAPENUM = "shape#(";
const string UCFunc::OBJECTNUM = "object#(";
std::map<unsigned int, std::string> UCFunc::FlagMap;

int UCFunc::num_global_statics = 0;

string demunge_ocstring(UCFunc &ucf, const FuncMap &funcmap, const string &asmstr, const map<unsigned int, string> &intrinsics, const UCc &op, bool ucs_output, Usecode_symbol_table *symtbl);

/* Assumption the 'var's are in their 'zeroed' state on initialization,
   unless something else is assigned to them. */

ostream &tab_indent(const unsigned int indent, ostream &o) {
#ifdef DEBUG_INDENT
	o << indent;
#endif

	switch (indent) {
	case 0:
		break;
	case 1:
		o << '\t';
		break;
	case 2:
		o << "\t\t";
		break;
	case 3:
		o << "\t\t\t";
		break;
	case 4:
		o << "\t\t\t\t";
		break;
	case 5:
		o << "\t\t\t\t\t";
		break;
	default:
		for (unsigned int i = 0; i < indent; ++i) o << '\t';
		break;
	}
	return o;
}

/* Outputs the short function data 'list' format, returns true upon success */
bool UCFunc::output_list(ostream &o, unsigned int funcno, const UCOptions &options) {
	boost::io::ios_flags_saver flags(o);
	boost::io::ios_fill_saver fill(o);
	o << "#" << std::setbase(10) << std::setw(4) << funcno << std::setbase(16) << ": "
	  << (return_var ? '&' : ' ')
	  << std::setw(4) << _funcid    << "H  "
	  << std::setw(8) << _offset    << "  "
	  << std::setw(4) << _funcsize  << "  "
	  << std::setw(4) << _datasize  << "  "
	  << std::setw(4) << codesize() << "  ";

	if (options.ucdebug)
		o << _data.find(0)->second;

	o << endl;

	return true;
}

/* Outputs the usecode-script formatted usecode, returns true upon success */
bool UCFunc::output_ucs(ostream &o, const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, const UCOptions &options, int new_indent, Usecode_symbol_table *symtbl) {
	unsigned int indent = new_indent;

	if (!_externs.empty()) tab_indent(indent, o) << "// externs" << endl;
	// output the 'externs'
	for (vector<unsigned int>::iterator e = _externs.begin(); e != _externs.end(); ++e) {
		FuncMap::const_iterator fmp = funcmap.find(*e);
		output_ucs_funcname(tab_indent(indent, o) << "extern ", funcmap, *e, fmp->second.num_args, symtbl) << ';' << endl;
	}

	if (!_externs.empty()) o << endl;

	// output the function name
	output_ucs_funcname(tab_indent(indent, o), funcmap, _funcid, _num_args, symtbl) << endl;
	// start of func
	tab_indent(indent++, o) << '{' << endl;
	std::map<unsigned int, std::string>::iterator it;

	for (unsigned int i = _num_args; i < _num_args + _num_locals; i++) {
		tab_indent(indent, o) << VARNAME << ' ';
		it = _varmap.find(i);
		if (it != _varmap.end())
			o << it->second;
		else
			o << VARPREFIX << std::setw(4) << i;
		o << ';' << endl;
	}

	for (int i = 0; i < _num_statics; i++)
		tab_indent(indent, o) << STATICNAME << ' ' << STATICPREFIX << std::setw(4) << i << ';' << endl;

	if (_num_locals > 0) o << endl;

	output_ucs_data(o, funcmap, intrinsics, options, indent, symtbl);

	tab_indent(--indent, o) << '}' << endl;

	return true;
}

/* outputs the general 'function name' in long format. For function
    declarations and externs */
ostream &UCFunc::output_ucs_funcname(ostream &o, const FuncMap &funcmap,
                                     unsigned int funcid, unsigned int numargs,
                                     Usecode_symbol_table *symtbl) {
	ignore_unused_variable_warning(symtbl);
	FuncMap::const_iterator fmp = funcmap.find(funcid);
	// do we return a variable
	if (fmp->second.return_var) o << VARNAME << ' ';
	else o << NORETURN << ' ';

	// output the "function name"
	// TODO: Probably want to grab this from a file in the future...
	//o << demunge_ocstring(*this, funcmap, "%f1", intrinsics, ucc, true, symtbl)
	if (!fmp->second.funcname.empty()) {
		if (fmp->second.funcname[0] == '&')
			o << fmp->second.funcname.substr(1, fmp->second.funcname.size() - 1);
		else
			o << fmp->second.funcname;
	} else
		o << "Func" << std::setw(4) << funcid;

	// output the "function number"
	o << " ";
	if (fmp->second.kind == Usecode_symbol::shape_fun)
		o << SHAPENUM;
	else if (fmp->second.kind == Usecode_symbol::object_fun)
		o << OBJECTNUM;
	o << "0x" << funcid;
	if (funcid < 0x800)
		o << ")";
	// output ObCurly braces
	o << " (";
	std::map<unsigned int, std::string>::iterator it;

	for (unsigned int i = (fmp->second.class_fun ? 1 : 0); i < numargs; i++) {
		o << VARNAME << ' ';
		it = fmp->second.varmap.find(numargs - i - 1);
		if (it != fmp->second.varmap.end())
			o << it->second;
		else
			o << VARPREFIX << std::setw(4) << i;
		o << ((i == numargs - 1) ? "" : ", ");
	}

	o << ")";

	return o;
}

ostream &UCFunc::output_ucs_funcname(ostream &o, const FuncMap &funcmap, Usecode_symbol_table *symtbl) {
	return output_ucs_funcname(o, funcmap, _funcid, _num_args, symtbl);
}

void UCFunc::output_ucs_data(ostream &o, const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, const UCOptions &options, unsigned int indent, Usecode_symbol_table *symtbl) {
	vector<unsigned int> labeltmp(1);
	for (vector<GotoSet>::iterator i = gotoset.begin(); i != gotoset.end(); ++i) {
		// we don't want to output the first "jump" (the start of the function)
		if (i != gotoset.begin()) {
			labeltmp[0] = i->offset();
			tab_indent(indent - 1, o) << demunge_ocstring(*this, funcmap, "label%f*_%1:", intrinsics, UCc(labeltmp), true, symtbl) << endl;
		}

		for (GotoSet::iterator j = (*i)().begin(); j != (*i)().end(); ++j) {
			const UCc &ucc = *(j->first);

			if (options.uselesscomment)
				tab_indent(indent, o) << "// Offset: " << std::setw(4) << ucc._offset << endl;

			output_ucs_opcode(o, funcmap, opcode_table_data, ucc, intrinsics, indent, symtbl);
		}
		//if(i!=gotoset.begin()) --indent; //decrement it again to skip the label statement.

	}
}

void UCFunc::output_ucs_opcode(ostream &o, const FuncMap &funcmap, const vector<UCOpcodeData> &optab, const UCc &op, const map<unsigned int, string> &intrinsics, unsigned int indent, Usecode_symbol_table *symtbl) {
	tab_indent(indent, o) << demunge_ocstring(*this, funcmap, optab[op._id].ucs_nmo, intrinsics, op, true, symtbl);
	if (!optab[op._id].flag_nosemicolon)
		o << ';';
	o << endl;

#ifdef DEBUG_PRINT
	for (vector<UCc *>::const_iterator i = op._popped.begin(); i != op._popped.end(); ++i) {
		if ((*i)->_popped.size())
			output_ucs_opcode(o, funcmap, opcode_table_data, **i, intrinsics, indent + 1, symtbl);
		else
//			tab_indent(indent+1, o) << demunge_ocstring(*this, funcmap, optab[(*i)->_id].ucs_nmo, **i, symtbl) << endl;
			tab_indent(indent + 1, o) << optab[(*i)->_id].ucs_nmo << endl;
	}
#endif
}

void UCFunc::output_ucs_node(ostream &o, const FuncMap &funcmap, UCNode *ucn, const map<unsigned int, string> &intrinsics, unsigned int indent, const UCOptions &options, Usecode_symbol_table *symtbl) {
	if (!ucn->nodelist.empty()) tab_indent(indent, o) << '{' << endl;

	if (ucn->ucc != nullptr)
		output_asm_opcode(tab_indent(indent, o), funcmap, opcode_table_data, intrinsics, *(ucn->ucc), options, symtbl);

	if (!ucn->nodelist.empty())
		for (vector<UCNode *>::iterator i = ucn->nodelist.begin(); i != ucn->nodelist.end(); ++i) {
			//tab_indent(indent, o);
			output_ucs_node(o, funcmap, *i, intrinsics, indent + 1, options, symtbl);
		}

	// end of func
	if (!ucn->nodelist.empty()) tab_indent(indent, o) << '}' << endl;
}

/* Just a quick function to remove all the ucc structured flagged as removable */
inline void gc_gotoset(vector<GotoSet> &gotoset) {
	for (vector<GotoSet>::iterator i = gotoset.begin(); i != gotoset.end(); ++i) {
		i->gc();
#ifdef DEBUG_GOTOSET
		cout << "----" << endl;
#endif
	}
}

void UCFunc::parse_ucs(const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, const UCOptions &options, Usecode_symbol_table *symtbl) {
	for (vector<UCc>::iterator i = _opcodes.begin(); i != _opcodes.end(); ++i)
		node.nodelist.push_back(new UCNode(&(*i)));

	parse_ucs_pass1(node.nodelist);
	parse_ucs_pass2(gotoset, funcmap, intrinsics, symtbl);
	gc_gotoset(gotoset);

	if (!options.basic) {
		parse_ucs_pass3(gotoset, intrinsics);
	}

#ifdef DEBUG_PARSE2
	for (vector<GotoSet>::iterator i = gotoset.begin(); i != gotoset.end(); ++i) {
		cout << std::setw(4) << i->offset() << endl;

		for (GotoSet::iterator j = (*i)().begin(); j != (*i)().end(); ++j) {
			cout << '\t' << std::setw(4) << j->first->_offset << '\t' << j->first->_id << endl;
		}
	}
#endif
}

/* Pass 1 turns the 1-dimentional vector of opcodes, into a 2-dimentional array
   consisting of all the opcodes within two 'goto target offsets'. */
void UCFunc::parse_ucs_pass1(vector<UCNode *> &nodes) {
	vector<unsigned int> jumps;

	// collect jump references
	for (unsigned int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->ucc != nullptr) {
			unsigned int isjump = 0;
			for (vector<pair<unsigned int, unsigned int> >::iterator op = opcode_jumps.begin(); op != opcode_jumps.end(); ++op)
				if (op->first == nodes[i]->ucc->_id) {
					isjump = op->second;
					break;
				}

			if (isjump != 0) {
				assert(nodes[i]->ucc->_params_parsed.size() >= isjump);
				jumps.push_back(nodes[i]->ucc->_params_parsed[isjump - 1]);
			}
		}
	}

	gotoset.push_back(GotoSet());

	for (unsigned int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->ucc != nullptr) {
			if (count(jumps.begin(), jumps.end(), nodes[i]->ucc->_offset)) {
				gotoset.push_back(nodes[i]->ucc);
			} else
				gotoset.back().add(nodes[i]->ucc);
		}
	}
}

/* In Pass 2 we convert our 2-dimensional 'GotoSet' array into an array with
   each UCc, having it's parameters sitting in it's UCc::_popped vector. Elements
   that are parameters are flagged for removal (Gotoset::()[i]->second=true) from
   the original GotoSet. */
void UCFunc::parse_ucs_pass2(vector<GotoSet> &gotoset, const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, Usecode_symbol_table *symtbl) {
	for (vector<GotoSet>::iterator i = gotoset.begin(); i != gotoset.end(); ++i) {
		parse_ucs_pass2a((*i)().rbegin(), (*i)(), 0, funcmap, intrinsics, symtbl);
	}
}

vector<UCc *> UCFunc::parse_ucs_pass2a(vector<pair<UCc *, bool> >::reverse_iterator current, vector<pair<UCc *, bool> > &vec, int opsneeded, const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, Usecode_symbol_table *symtbl) {
	vector<UCc *> vucc;
	int opsfound = 0;

#ifdef DEBUG_PARSE2
	output_asm_opcode(tab_indent(4, cout), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

	for (; vec.rend() != current; current++) {
#ifdef DEBUG_PARSE2
		output_asm_opcode(tab_indent(3, cout), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

		if (!current->second) {
			if ((opcode_table_data[current->first->_id].num_pop != 0) || (opcode_table_data[current->first->_id].flag_call_effect) ||
			        (opcode_table_data[current->first->_id].flag_new_effect) || (opcode_table_data[current->first->_id].flag_method_effect) ||
			        (opcode_table_data[current->first->_id].flag_function_effect)) {
				//if(opcode_table_data[current->first->_id].num_pop<0x7F)
				{
#ifdef DEBUG_PARSE2
					output_asm_opcode(tab_indent(3, cout << "0x" << std::setw(2) << current->first->_id << "-"), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
					tab_indent(3, cout << "0x" << std::setw(2) << current->first->_id << "-") << opcode_table_data[current->first->_id].num_pop << endl;
#endif

					int num_args = 0;

					if (opcode_table_data[current->first->_id].flag_new_effect) {
						assert(symtbl);
#ifdef DEBUG_PARSE2a
						cout << "NEW EFFECT: " << current->first->_params_parsed[0] << '\t';
#endif

						unsigned int offset = current->first->_params_parsed[0];
						Usecode_class_symbol *cls = symtbl->get_class(offset);
						assert(cls);
						num_args = cls->get_num_vars();

#ifdef DEBUG_PARSE2a
						cout << num_args << endl;
#endif
					} else if (opcode_table_data[current->first->_id].flag_method_effect) {
						assert(symtbl);
#ifdef DEBUG_PARSE2a
						cout << "NEW METHOD: " << current->first->_params_parsed[0] << '\t';
#endif
						if (current->first->_params_parsed.size() == 1) {
							// FIXME: Hack for partial UC_CALLM support.
							// TODO: Implement static data flow analysis so type of class can be
							// recovered so this opcode works better.
							// Means: get as many parameters as can be gotten from the stack.
							num_args = -1;

						} else {
							assert(current->first->_params_parsed.size() >= 2);
#ifdef DEBUG_PARSE2a
							cout << current->first->_params_parsed[1] << '\t';
#endif

							unsigned int offset = current->first->_params_parsed[1];
							Usecode_class_symbol *cls = symtbl->get_class(offset);
							assert(cls);
							int method = cls->get_method_id(current->first->_params_parsed[0]);
							assert(method >= 0);
							FuncMap::const_iterator fmp = funcmap.find(method);
							assert(fmp != funcmap.end());
							num_args = fmp->second.num_args;

						}
#ifdef DEBUG_PARSE2a
						if (num_args >= 0)
							cout << num_args << endl;
						else
							cout << "all" << endl;
#endif
					} else if (opcode_table_data[current->first->_id].num_pop > 0x7F) {
#ifdef DEBUG_PARSE2a
						cout << "CALL EFFECT: " << opcode_table_data[current->first->_id].num_pop << '\t';
#endif

						unsigned int offset = 0x100 - opcode_table_data[current->first->_id].num_pop;
						num_args = current->first->_params_parsed[(offset & 0x1f) - 1] + (offset >> 5);

#ifdef DEBUG_PARSE2a
						cout << num_args << endl;
#endif

					} else if (opcode_table_data[current->first->_id].flag_function_effect) {
						assert(!current->first->_params_parsed.empty());
						FuncMap::const_iterator fmp = funcmap.find(current->first->_params_parsed[0]);
						assert(fmp != funcmap.end());
#ifdef DEBUG_PARSE2
						cout << "CALL:     " << fmp->second.funcid << '\t' << fmp->second.num_args << endl;
#endif

						num_args = fmp->second.num_args + opcode_table_data[current->first->_id].num_pop;
					} else if (opcode_table_data[current->first->_id].flag_call_effect) {
						assert(!current->first->_params_parsed.empty());
						assert(_externs.size() >= current->first->_params_parsed[0]);
						FuncMap::const_iterator fmp = funcmap.find(_externs[current->first->_params_parsed[0]]);
						assert(fmp != funcmap.end());
#ifdef DEBUG_PARSE2
						cout << "CALL EXTERN:" << fmp->second.funcid << '\t' << fmp->second.num_args << endl;
#endif

						num_args = fmp->second.num_args;
					} else {
#ifdef DEBUG_PARSE2
						cout << "Non-CALL: \t" << opcode_table_data[current->first->_id].num_pop << endl;
#endif
						num_args = opcode_table_data[current->first->_id].num_pop;
					}

					// FIXME: Hack for partial UC_CALLM support.
					if (num_args != 0) {
						/* save the 'current' value as the return value and increment it so it's
						   pointing at the 'next' current value */
						vector<pair<UCc *, bool> >::reverse_iterator ret(current);

						ret->first->_popped = parse_ucs_pass2a(++current, vec, num_args, funcmap, intrinsics, symtbl);
						unsigned nargs = static_cast<unsigned>(num_args);
						if (num_args < 0 || nargs <= ret->first->_popped.size()) {
							ret->first->_missing_args = 0;
						} else {
							ret->first->_missing_args = nargs - ret->first->_popped.size();
						}

						assert(current != ret);

						--current;

						assert(current == ret);
#ifdef DEBUG_PARSE2a
						output_asm_opcode(tab_indent(1, cout), *this, funcmap, opcode_table_data, intrinsics, *(ret->first), symtbl);

						for (vector<UCc *>::iterator i = ret->first->_popped.begin(); i != ret->first->_popped.end(); ++i)
							output_asm_opcode(tab_indent(2, cout), *this, funcmap, opcode_table_data, intrinsics, **i, symtbl);
#endif
					}
				}
			}
			if ((opsneeded != 0) && (!current->second)) {
				if (opcode_table_data[current->first->_id].flag_not_param) {
					continue;
				}
				// if it's a 'push' opcode and we need items to return that we've popped off the stack...
				else if (opcode_table_data[current->first->_id].num_push != 0) {
#ifdef DEBUG_PARSE2
					output_asm_opcode(tab_indent(4, cout << "P-"), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

					opsfound += opcode_table_data[current->first->_id].num_push;
					vucc.push_back(current->first);
					current->second = true;
				}
				// if it's a call to a class method that returns a variable...
				else if (opcode_table_data[current->first->_id].flag_method_effect) {
					if (current->first->_params_parsed.size() == 1) {
						// FIXME: Hack for partial UC_CALLM support.
						// TODO: Try to come up with a way to get this information. Currently, we are
						// trusting UCC output to have asked for a return value only if needed.
						opsfound += 1;
						vucc.push_back(current->first);
						current->second = true;
					} else {
						unsigned int offset = current->first->_params_parsed[1];
						Usecode_class_symbol *cls = symtbl->get_class(offset);
						int method = cls->get_method_id(current->first->_params_parsed[0]);
						FuncMap::const_iterator fmp = funcmap.find(method);
						assert(fmp != funcmap.end());
						if (fmp->second.return_var) {
#ifdef DEBUG_PARSE2
							output_asm_opcode(tab_indent(4, cout << "C-"), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

							opsfound += 1;
							vucc.push_back(current->first);
							current->second = true;
						}
					}
				}
				// if it's a call to a function that returns a variable...
				else if (opcode_table_data[current->first->_id].flag_function_effect) {
					FuncMap::const_iterator fmp = funcmap.find(current->first->_params_parsed[0]);
					assert(fmp != funcmap.end());

					if (fmp->second.return_var) {
#ifdef DEBUG_PARSE2
						output_asm_opcode(tab_indent(4, cout << "C-"), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

						opsfound += 1;
						vucc.push_back(current->first);
						current->second = true;
					}
				}
				// if it's a call to an extern function that returns a variable...
				else if (opcode_table_data[current->first->_id].flag_call_effect) {
					FuncMap::const_iterator fmp = funcmap.find(_externs[current->first->_params_parsed[0]]);
					assert(fmp != funcmap.end());

					if (fmp->second.return_var) {
#ifdef DEBUG_PARSE2
						output_asm_opcode(tab_indent(4, cout << "C-"), *this, funcmap, opcode_table_data, intrinsics, *(current->first));
#endif

						opsfound += 1;
						vucc.push_back(current->first);
						current->second = true;
					}
				}

				// if we've found all the ops we were searching for, return them
				if (opsneeded != 0 && !current->second) {
					if (opsneeded >= 0 && opsfound < opsneeded) {
						cout << "DID NOT FIND ALL OPCODE PARAMETERS." << endl;
					}
					while (++current != vec.rend() && current->second) {
						current->second = false;
					}
					return vucc;
				} else if (opsneeded >= 0 && opsfound >= opsneeded) {
					return vucc;
				}
			}
		}
	}

	if (!vucc.empty()) cout << "DID NOT FIND ALL OPCODE PARAMETERS." << endl;
	return vucc;
}

/* The 'optimisation' phase. Attempting to remove as many goto...labels as possible. */
void UCFunc::parse_ucs_pass3(vector<GotoSet> &gotoset, const map<unsigned int, string> &intrinsics) {
	ignore_unused_variable_warning(gotoset, intrinsics);
}

bool UCFunc::output_tt(std::ostream &o) {
	o << "\t<0x" << setw(4) << _funcid << ">" << endl;

	for (map<unsigned int, string, less<unsigned int> >::iterator i = _data.begin(); i != _data.end(); ++i) {
		o << "\t\t<0x" << setw(4) << i->first << ">" << endl
		  << "\t\t`" << i->second << "`" << endl
		  << "\t\t</>" << endl;
	}
	o << "\t</>" << endl;

	return true;
}

/* calculates the relative offset jump location, used in opcodes jmp && jne */
inline int calc16reloffset(const UCc &op, unsigned int param) {
	return op._offset + static_cast<sint16>(param & 0xFFFF) + 1 + static_cast<int>(op._params.size());
}

/* calculates the relative offset jump location, used in opcodes jmp && jne */
inline int calc32reloffset(const UCc &op, unsigned int param) {
	return op._offset + static_cast<sint32>(param) + 1 + static_cast<int>(op._params.size());
}

void ucc_parse_parambytes(UCc &ucop, const UCOpcodeData &otd) {
	unsigned int first = 0;

	for (vector<pair<unsigned int, bool> >::const_iterator s = otd.param_sizes.begin(); s != otd.param_sizes.end(); ++s) {
		//cout << ucop._id << '\t' << ucop._params.size() << endl;

		assert(first < ucop._params.size());

		unsigned int ssize = s->first;
		bool offset_munge = s->second;

		assert(ssize != 0);

		if (ssize == 1)
			ucop._params_parsed.push_back(static_cast<unsigned int>(ucop._params[first++]));
		else if (ssize == 2)
			if (offset_munge) {
				unsigned int calcvar = static_cast<unsigned int>(ucop._params[first++]);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 8);
				unsigned int reloffset = calc16reloffset(ucop, calcvar);
				ucop._params_parsed.push_back(reloffset);
				ucop._jump_offsets.push_back(reloffset);
			} else {
				unsigned int calcvar = static_cast<unsigned int>(ucop._params[first++]);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 8);
				ucop._params_parsed.push_back(calcvar);
			}
		else if (ssize == 4)
			if (offset_munge) {
				unsigned int calcvar = static_cast<unsigned int>(ucop._params[first++]);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 8);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 16);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 24);
				unsigned int reloffset = calc32reloffset(ucop, calcvar);
				ucop._params_parsed.push_back(reloffset);
				ucop._jump_offsets.push_back(reloffset);
			} else {
				unsigned int calcvar = static_cast<unsigned int>(ucop._params[first++]);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 8);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 16);
				calcvar += ((static_cast<unsigned int>(ucop._params[first++])) << 24);
				ucop._params_parsed.push_back(calcvar);
			}
		else
			assert(false); // just paranoia.
	}
}

/* prints the "assembler" output of the usecode. returns true if successful*/
bool UCFunc::output_asm(ostream &o, const FuncMap &funcmap, const map<unsigned int, string> &intrinsics, const UCOptions &options, Usecode_symbol_table *symtbl) {
	if (options.verbose) cout << "Printing function..." << endl;

	o << "\t; Function at file offset " << std::setw(8) << _offset << "H" << endl;
	o << "\t.funcnumber  " << std::setw(4) << _funcid << "H" << endl;
	if (ext32) o << "\t.ext32" << endl;
	o << "\t.data" << endl;
	o << "\t.msize       " << ((ext32) ? setw(8) : setw(4)) << _funcsize << "H" << endl;
	o << "\t.dsize       " << ((ext32) ? setw(8) : setw(4)) << _datasize << "H" << endl;

	if (debugging_info)
		o << "\t  .dbgoffset " << std::setw(4) << debugging_offset << "H" << endl;

	if (!_data.empty())
		output_asm_data(o);

	o << "\t; Code segment at file offset " << std::setw(8) << _codeoffset << "H" << endl;
	o << "\t.code" << endl;
	o << "\t.argc        " << std::setw(4) << _num_args << "H" << endl;
	o << "\t.localc      " << std::setw(4) << _num_locals << "H" << endl;
	o << "\t.externsize  " << std::setw(4) << _externs.size() << "H" << endl;

	for (std::vector<unsigned int>::iterator i = _externs.begin(); i != _externs.end(); ++i)
		o << '\t' << "  .extern    " << std::setw(4) << *i << "H" << endl;

	for (vector<UCc>::iterator op = _opcodes.begin(); op != _opcodes.end(); ++op)
		output_asm_opcode(o, funcmap, opcode_table_data, intrinsics, *op, options, symtbl);

	return true;
}

void UCFunc::output_asm_data(ostream &o) {
	static const unsigned int nochars = 60;
	// limit of about 60 chars to a line, wrap to the next line if longer then this...
	for (map<unsigned int, string, less<unsigned int> >::iterator i = _data.begin(); i != _data.end(); ++i) {
		for (unsigned int j = 0; j < i->second.size(); j++) {
			if (j == 0)
				o << "L" << setw(4) << i->first << ":";
			if ((j != 0) && !(j % nochars))
				o << "'" << endl;
			if (!(j % nochars))
				o << "\tdb\t'";

			o << i->second[j];
		}
		o << "'" << endl;
		o << "\tdb\t00" << endl;
	}
}

void UCFunc::output_raw_opcodes(ostream &o, const UCc &op) {
	// chars in opcode
	o << ' ' << std::setw(2) << static_cast<unsigned int>(op._id);
	if (!op._params.empty()) cout << ' ';

	for (unsigned int i = 0; i < op._params.size(); i++) {
		o << std::setw(2) << static_cast<unsigned int>(op._params[i]);
		if (i != op._params.size())
			o << ' ';
	}

	// seperator
	size_t numsep = op._params.size();
	//cout << endl << numsep << endl;
	if (numsep > 6)
		o << endl << "\t\t\t";
	else if (numsep > 5)
		o << " ";
	else if (numsep > 2)
		o << "\t";
	else
		o << "\t\t";
}

void UCFunc::output_asm_opcode(ostream &o, const FuncMap &funcmap, const vector<UCOpcodeData> &optab, const map<unsigned int, string> &intrinsics, const UCc &op, const UCOptions &options, Usecode_symbol_table *symtbl) {
	// offset
	o << std::setw(4) << op._offset << ':';

	if (options.rawops) output_raw_opcodes(o, op);
	else            o << '\t';

	o << demunge_ocstring(*this, funcmap, optab[op._id].asm_nmo, intrinsics, op, false, symtbl);

	if (options.autocomment)
		o << demunge_ocstring(*this, funcmap, optab[op._id].asm_comment, intrinsics, op, false, symtbl);

	o << endl;
}

inline unsigned int charnum2uint(const char c) {
	switch (c) {
	case '1':
		return 1;
	case '2':
		return 2;
	case '3':
		return 3;
	case '4':
		return 4;
	case '5':
		return 5;
	case '6':
		return 6;
	case '7':
		return 7;
	case '8':
		return 8;
	case '9':
		return 9;
	default:
		return 0;
	}
	return 0; // can't happen
}

template <int radix, int width>
static void print_number(char c, std::stringstream &str, const vector<unsigned int> &params) {
	unsigned int t = charnum2uint(c);
	if (t != 0) {
		assert(params.size() >= t);
		if (radix != 16)
			str << std::setbase(radix) << params[t - 1] << std::setbase(16);
		else
			str << std::setw(width) << params[t - 1];
	}
}

string demunge_ocstring(UCFunc &ucf, const FuncMap &funcmap, const string &asmstr, const map<unsigned int, string> &intrinsics, const UCc &op, bool ucs_output, Usecode_symbol_table *symtbl) {
	std::stringstream str;
	str << std::setfill('0') << std::setbase(16);
	str.setf(ios::uppercase);
	size_t  len = asmstr.length();
	const vector<unsigned int> &params = op._params_parsed;

	if (len == 0) return string(); // for the degenerate case

	unsigned int i = 0; // istr index

	if (ucs_output && opcode_table_data[op._id].flag_paren) str << '(';

	while (i < len) {
		char c = asmstr[i];

		switch (c) {
		case '\\':
			i++;
			c = asmstr[i];
			switch (c) {
			case '\\':
				str << '\\';
				break;
			case 'n':
				str << '\n';
				break;
			case 't':
				str << '\t';
				break;
			case '\'':
				str << '\'';
				break;
			case '"':
				str << '\"';
				break;
			case 'b':  // bell is invalid
			default:   // we'll silently drop errors... it's the only "clean" way
				str << '\\' << c;
			}
			break;
		case '%': {
			i++;
			c = asmstr[i];

			switch (c) {
			case '%':
				str << '%';
				break;
			case 'b': {
				// if it's a "byte" print 2 hex digits, and get the next char
				i++;
				c = asmstr[i];
				print_number<16, 2>(c, str, params);
				break;
			}
			case 's': {
				// if it's a "short" print 4 hex digits, and get the next char
				i++;
				c = asmstr[i];
				print_number<16, 4>(c, str, params);
				break;
			}
			case 'n': {
				// if it's a "int" print 8 hex digits, and get the next char
				i++;
				c = asmstr[i];
				print_number<16, 8>(c, str, params);
				break;
			}
			case 'l': {
				// if it's a "long" print 16 hex digits, and get the next char
				i++;
				c = asmstr[i];
				print_number<16, 16>(c, str, params);
				break;
			}
			case 'd': {
				// if we want to output the 'decimal' value rather then the default hex
				i++;
				c = asmstr[i];
				print_number<10, 0>(c, str, params);
				break;
			}
			case 't': {
				// if it's the character representation of a text data string we want
				bool commentformat = false;
				i++;
				c = asmstr[i];

				// if we only want to output the 'short' format of the text (comment format)
				if (c == 'c') {
					commentformat = true;
					i++;
					c = asmstr[i];
				}

				unsigned int t = charnum2uint(c);

				assert(params.size() >= t);
				assert(t != 0);
				string s = ucf._data.find(params[t - 1])->second;

				if (commentformat)
					if (s.size() > 17) s = s.substr(0, 17) + string("...");

				// escape the appropriate characters...
				// we'll only do it in the 'full' text output for the moment.
				if (!commentformat)
					for (string::size_type i = 0; i < s.size(); i++)
						if ((s[i] == '\"') || (s[i] == '\\')) {
							s.insert(i, "\\");
							++i;
						}

				str << s;
				break;
			}
			case 'i': {
				// if it's the intrinsic name we want
				i++;
				c = asmstr[i];
				unsigned int t = charnum2uint(c);

				assert(params.size() >= t);
				assert(t != 0);
				string s = intrinsics.find(params[t - 1])->second;
				str << s;
				break;
			}
			case 'f': {
				// if it's external function name we want
				i++;
				c = asmstr[i];
				string funcname;
				int funcid;

				if (c == '*') {
					funcname = ucf.funcname;
					funcid = ucf._funcid;
				} else if (c == '#') {
					i++;
					c = asmstr[i];
					unsigned int t = charnum2uint(c);

					assert(params.size() >= t);
					assert(t != 0);

					FuncMap::const_iterator fmp = funcmap.find(params[t - 1]);
					funcname = fmp->second.funcname;
					funcid = params[t - 1];
				} else if (c == 'm') {
					assert(symtbl != nullptr);
					i++;
					c = asmstr[i];
					unsigned int t = charnum2uint(c);

					assert(params.size() >= t);
					assert(t != 0);

					Usecode_class_symbol *cls = symtbl->get_class(params[t - 1]);
					assert(cls);

					i++;
					c = asmstr[i];
					t = charnum2uint(c);

					assert(params.size() >= t);
					assert(t != 0);

					funcid = cls->get_method_id(params[t - 1]);
					assert(funcid >= 0);
					FuncMap::const_iterator fmp = funcmap.find(funcid);
					assert(fmp != funcmap.end());
					funcname = cls->get_name();
					funcname += "::";
					funcname += fmp->second.funcname;
				} else {
					unsigned int t = charnum2uint(c);

					assert(ucf._externs.size() >= t);
					assert(t != 0);
					assert(!params.empty());

					FuncMap::const_iterator fmp = funcmap.find(ucf._externs[params[t - 1]]);
					funcname = fmp->second.funcname;
					funcid = ucf._externs[params[t - 1]];
				}
				if (!funcname.empty()) {
					if (funcname[0] == '&')
						str << funcname.substr(1, funcname.size() - 1);
					else
						str << funcname;
				} else
					str << "Func" << std::setw(4) << funcid;
				break;
			}
			case 'p': {
				// if it's the character representation of a text data string we want
				i++;
				c = asmstr[i];
				unsigned int t = charnum2uint(c);

				// If followed by comma means start at first popped parameter; if
				// followed by plus sign, start on the parameter specified by the
				// following digit.
				if ((t == 0) && (c == ',' || c == '+')) {
					vector<UCc *>::const_iterator it = op._popped.begin();
					if (c == '+') {
						i++;
						c = asmstr[i];
						unsigned int n = charnum2uint(c);
						assert(n != 0);
						it = it + (n - 1);
					}

					for (; it != op._popped.end();) {
						str << demunge_ocstring(ucf, funcmap, opcode_table_data[(*it)->_id].ucs_nmo, intrinsics, **it, ucs_output, symtbl);
						if (++it != op._popped.end() || op._missing_args != 0)
							str << ", ";
					}
					for (unsigned ii = 0; ii < op._missing_args; ) {
						str << "0x0000/*stack underflow*/";
						if (++ii < op._missing_args)
							str << ", ";
					}
				}

				if (t != 0) {
					if (t > op._popped.size())
						str << "0x0000/*stack underflow*/";
					else {
						UCc &ucc(*op._popped[t - 1]);
						str << demunge_ocstring(ucf, funcmap, opcode_table_data[ucc._id].ucs_nmo, intrinsics, ucc, ucs_output, symtbl);
					}
				}
				break;
			}
			case 'v': {
				// If it is a static variable reference, want to output correct reference.
				assert(!params.empty());
				i++;
				c = asmstr[i];
				if (c == 's') {
					i++;
					c = asmstr[i];
					unsigned int t = charnum2uint(c);
					assert(t != 0);
					assert(params.size() >= t);
					int paramval = static_cast<sint16>(params[t - 1]);
					if (paramval < 0)
						str << UCFunc::GLOBALSTATICPREFIX << std::setw(4) << -paramval;
					else
						str << UCFunc::STATICPREFIX << std::setw(4) << paramval;
				} else if (c == 'c') {
					i++;
					c = asmstr[i];
					unsigned int t = charnum2uint(c);
					assert(t != 0);
					assert(params.size() >= t);
					int paramval = params[t - 1];
					str << UCFunc::CLASSVARPREFIX << std::setw(4) << paramval;
				} else {
					unsigned int t = charnum2uint(c);
					assert(t != 0);
					assert(params.size() >= t);
					// Small hack to make variable name equal to the
					// order in the function name, so that the function
					// call has the same order as parameter name.
					unsigned int paramval = params[t - 1];
					if (paramval < ucf._num_args)
						paramval = ucf._num_args - paramval - 1;
					if (paramval || !ucf._cls) {
						std::map<unsigned int, std::string>::iterator it;
						it = ucf._varmap.find(params[t - 1]);
						if (it != ucf._varmap.end())
							str << it->second;
						else
							str << UCFunc::VARPREFIX << std::setw(4) << paramval;
					} else
						str << UCFunc::THISVAR;
				}
				break;
			}
			case 'c': {
				// Want to output referenced class name.
				assert(symtbl);
				i++;
				c = asmstr[i];
				unsigned int t = charnum2uint(c);
				assert(t != 0);
				assert(params.size() >= t);
				unsigned int clsid = params[t - 1];
				Usecode_class_symbol *cls = symtbl->get_class(clsid);
				assert(cls);
				str << cls->get_name();
				break;
			}
			case 'g': {
				// Global flag reference
				i++;
				c = asmstr[i];
				unsigned int t = charnum2uint(c);
				assert(t != 0);
				assert(params.size() >= t);
				std::map<unsigned int, std::string>::iterator it;
				it = UCFunc::FlagMap.find(params[t - 1]);
				if (it != UCFunc::FlagMap.end()) {
					str << it->second;
				} else {
					str << "0x";
					print_number<16, 4>(c, str, params);
				}
				break;
			}
			default:
				print_number<16, 4>(c, str, params);
				break;
			}
			break;
		}
		default:
			// it's just a character, leave it be
			str << c;
			break;
		}

		i++;
		if (i == asmstr.size())
			break;
	}

	if (ucs_output && opcode_table_data[op._id].flag_paren) str << ')';

	return str.str();
}

void readbin_U7UCFunc(
    ifstream &f,
    UCFunc &ucf,
    const UCOptions &options,
    Usecode_symbol_table *symtbl
) {
//	#define DEBUG_READ_PAIR(X, Y) cout << '\t' << X << '\t' << Y << endl;
	// offset to start of function
	ucf._offset = f.tellg();
	DEBUG_READ_PAIR("  Offset: ", ucf._offset);

	// Read Function Header
	ucf._funcid = Read2(f);

	if (options.very_verbose)
		cout << "\tReading Function: " << setw(4) << ucf._funcid << endl;

	DEBUG_READ_PAIR("  FuncID: ", ucf._funcid);

	if (ucf._funcid != 0xFFFF) {

		// This is the original usecode function header
		ucf._funcsize = Read2(f);
		DEBUG_READ_PAIR("  FuncSize: ", ucf._funcsize);

		// save body offset in case we need it
		ucf._bodyoffset = f.tellg();

		ucf._datasize = Read2(f);
		DEBUG_READ_PAIR("  DataSize: ", ucf._datasize);
	} else {
		// This is the ext32 extended usecode function header
		ucf.ext32 = true;
		ucf._funcid = Read2(f);

		if (options.very_verbose)
			cout << "\tReading Function: " << setw(4) << ucf._funcid << endl;

		DEBUG_READ_PAIR("  extFuncID: ", ucf._funcid);
		ucf._funcsize = Read4(f);
		DEBUG_READ_PAIR("  extFuncSize: ", ucf._funcsize);

		// save body offset in case we need it
		ucf._bodyoffset = f.tellg();

		ucf._datasize = Read4(f);
		DEBUG_READ_PAIR("  extDataSize: ", ucf._datasize);
	}

	if (symtbl) {
		Usecode_symbol *sym = (*symtbl)[ucf._funcid];
		Usecode_class_symbol *class_sym = nullptr;
		// No symbol? Try looking inside class definitions.
		if (!sym) {
			for (int ii = 0; ii < symtbl->get_num_classes(); ii++) {
				Usecode_class_symbol *cls = symtbl->get_class(ii);
				assert(cls != nullptr);
				sym = (*cls)[ucf._funcid];
				if (sym) {
					class_sym = cls;
					break;
				}
			}
		}
		// Did we find a symbol?
		if (sym) {
			if (class_sym) {
				ucf.funcname += sym->get_name();
			} else
				ucf.funcname = sym->get_name();
			ucf._sym = sym;
			ucf._cls = class_sym;
		}
	}

	// process ze data segment!
	{
		streampos pos = f.tellg(); // paranoia

		size_t off = 0;
		// Load all strings & their offsets
		while (off < ucf._datasize) {
			assert(!f.eof());

			string data;
			getline(f, data, static_cast<char>(0x00));
			ucf._data.insert(pair<unsigned int, string>(off, data));
			off += data.size() + 1;
		}
		f.seekg(pos, ios::beg); // paranoia
		f.seekg(ucf._datasize, ios::cur); // paranoia
	}

	// process code segment
	{
		//streampos start_of_code_seg = f.tellg(); // what's this used for?
		ucf._codeoffset = f.tellg();

		// get the number of arguments to the function
		ucf._num_args = Read2(f);

		if (options.output_ucs && ucf._funcid < 0x800)
			ucf._num_args--;

		// get the number of local variables
		ucf._num_locals = Read2(f);

		// get the number of external function numbers
		ucf._num_externs = Read2(f);

		// load the external function numbers
		for (unsigned int i = 0; i < ucf._num_externs; i++)
			ucf._externs.push_back(Read2(f));

		// ok, now to load the usecode
		unsigned int code_offset = 0;

		/* Here the '3+' comes from the sizeof(_datasize) + sizeof(_num_args)
		    + sizeof(_num_locals) + sizeof(_num_externs)
		    which are stored in the file as 16bit shorts, with the exception of
		    an ext32 function header, where the _datasize is a 32bit structure */
		unsigned int code_size = ucf._funcsize - ucf._datasize - ((4 + ucf._num_externs) * SIZEOF_USHORT);
		if (ucf.ext32) code_size -= 2;

		DEBUG_READ_PAIR("Code Size: ", code_size);

		while (code_offset < code_size) {
			assert(!f.eof());
			UCc ucop;

			ucop._offset = code_offset;

			ucop._id = Read1(f);
			code_offset++;

			const UCOpcodeData &otd = opcode_table_data[ucop._id];

			if (otd.opcode == 0x00) {
				cout << ucop._id << ' ' << code_offset << endl;
				assert(otd.opcode != 0x00);
			}

			//assert(((otd.asm_nmo.size()!=0) && (otd.ucs_nmo.size()!=0)));
			for (unsigned int i = 0; i < otd.num_bytes; i++)
				ucop._params.push_back(Read1(f));

			// parse the parameters
			ucc_parse_parambytes(ucop, otd);

			code_offset += otd.num_bytes;

			/* if we're an opcode that sets a return value, we need to mark the
			    function as one that returns a value */
			if (otd.flag_return)
				ucf.return_var = true;

			/* if we're an opcode that causes the function to abort, mark it as
			    such */
			if (otd.flag_abort)
				ucf.aborts = true;

			/* if we're a function debugging opcode, set the debuging flag, and
			    assign the variable name string offset */
			if (otd.flag_debug) {
				ucf.debugging_info = true;
				assert(ucop._params_parsed.size() >= 2);
				ucf.debugging_offset = ucop._params_parsed[1];
				// Don't override name from symbol table
				if (!ucf._sym) {
					std::stringstream str;
					str << ucf._data.find(ucop._params_parsed[0])->second << "_";
					str << std::setfill('0') << std::setbase(16);
					str.setf(ios::uppercase);
					str << std::setw(4) << ucf._funcid;
					ucf.funcname = str.str();
					std::map<unsigned int, std::string,
					         std::less<unsigned int> >::iterator it;
					it = ucf._data.find(ucf.debugging_offset);
					for (unsigned int i = 0;
					     i < ucf._num_args + ucf._num_locals && it != ucf._data.end();
					     ++it, i++) {
						std::string varname = it->second;
						if (!varname.empty()) {
							if (std::isdigit(static_cast<unsigned char>(varname[0])))
								varname = UCFunc::VARPREFIX + varname;
							else if (varname == "item")
								varname = "_item";
							ucf._varmap[i] = varname;
						}
					}
				}
			}

			/* if we're am opcode accessing statics, make sure to update the
			    appropriate static variable counter */
			if (otd.flag_staticref) {
				// TODO: Move variable reference index to u7opcodes.data
				int sref = static_cast<sint16>(ucop._params_parsed[otd.flag_loop ? 3 : 0]);
				if (sref < 0) {
					sref = -sref;
					if (sref > UCFunc::num_global_statics)
						UCFunc::num_global_statics = sref;
				} else {
					if (sref >= ucf._num_statics)
						ucf._num_statics = sref + 1;
				}
			}
			ucf._opcodes.push_back(ucop);

#ifdef DEBUG_READ
			cout << std::setw(4) << code_size << "\t" << std::setw(4) << code_offset << "\t" << std::setw(4) << (unsigned int)ucop._offset << "\t" << std::setw(2) << (unsigned int)ucop._id << "\t";
			for (unsigned int i = 0; i < ucop._params.size(); i++)
				cout << std::setw(2) << (unsigned int)ucop._params[i] << ',';
			cout << endl;
#endif
		}
	}
}

void readbin_U8UCFunc(ifstream &f, UCFunc &ucf) {
	ignore_unused_variable_warning(f, ucf);
}

