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

#ifndef OPCODES_H
#define OPCODES_H

#include "Configuration.h"
#include "ucc.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

std::vector<std::string> qnd_ocsplit(const std::string& s);

void map_type_size(
		const std::vector<std::string>&             param_types,
		std::vector<std::pair<unsigned int, bool>>& param_sizes);

static inline std::string strip_backticks(const std::string& s) {
	if (s.size() == 2 && s[0] == '`' && s[1] == '`') {
		return std::string();
	} else if (s[0] == '`' && s[s.size() - 1] == '`') {
		return s.substr(1, s.size() - 2);
	}
	return s;
}

/* Just a basic class to tie together half a dozen initialisation routines. */
class ucxtInit {
public:
	ucxtInit(const std::string& new_ucxtdata = std::string("ucxt.data"))
			: ucxtdata(new_ucxtdata) {}

	void init(const Configuration& config, const UCOptions& options);

private:
	std::string get_datadir(
			const Configuration& config, const UCOptions& options);
	void misc();
	void opcodes();
	void intrinsics(
			const std::string& intrinsic_data,
			const std::string& intrinsic_root);

	const std::string ucxtdata;
	std::string       datadir;

	std::string opcodes_data;
	std::string opcodes_root;

	std::string misc_data;
	std::string misc_root;

	std::string bg_intrinsics_data;
	std::string bg_intrinsics_root;

	std::string si_intrinsics_data;
	std::string si_intrinsics_root;

	std::string sibeta_intrinsics_data;
	std::string sibeta_intrinsics_root;
};

class UCOpcodeData {
public:
	UCOpcodeData() = default;

	UCOpcodeData(unsigned int op, const Configuration::KeyTypeList& ktl)
			: opcode(op) {
		for (const auto& k : ktl) {
			switch (k.first[0]) {
			case 'a':
				if (k.first == "asm_nmo") {
					asm_nmo = strip_backticks(k.second);
				} else if (k.first == "asm_comment") {
					asm_comment = strip_backticks(k.second);
				} else if (k.first == "abort/") {
					flag_abort = true;
				}
				break;
			case 'c':
				if (k.first == "call_effect/") {
					flag_call_effect = true;
				}
				break;
			case 'd':
				if (k.first == "debug/") {
					flag_debug = true;
				}
				break;
			case 'f':
				if (k.first == "function_effect/") {
					flag_function_effect = true;
				}
				break;
			case 'i':
				if (k.first == "indent_inc/") {
					flag_indent_inc = true;
				} else if (k.first == "indent_dec/") {
					flag_indent_dec = true;
				} else if (k.first == "indent_tmpinc/") {
					flag_indent_tmpinc = true;
				} else if (k.first == "indent_tmpdec/") {
					flag_indent_tmpdec = true;
				}
				break;
			case 'l':
				if (k.first == "loop/") {
					flag_loop = true;
				}
				break;
			case 'm':
				if (k.first == "method_effect/") {
					flag_method_effect = true;
				}
				break;
			case 'n':
				if (k.first == "name") {
					name = strip_backticks(k.second);
				} else if (k.first == "num_bytes") {
					num_bytes = static_cast<unsigned int>(
							strtol(k.second.c_str(), nullptr, 0));
				} else if (k.first == "num_pop") {
					num_pop = static_cast<unsigned int>(
							strtol(k.second.c_str(), nullptr, 0));
				} else if (k.first == "num_push") {
					num_push = static_cast<unsigned int>(
							strtol(k.second.c_str(), nullptr, 0));
				} else if (k.first == "nosemicolon/") {
					flag_nosemicolon = true;
				} else if (k.first == "new_effect/") {
					flag_new_effect = true;
				} else if (k.first == "not_param/") {
					flag_not_param = true;
				}
				break;
			case 'p':
				if (k.first == "param_types") {
					param_types = qnd_ocsplit(k.second);
				} else if (k.first == "paren/") {
					flag_paren = true;
				}
				break;
			case 'r':
				if (k.first == "return/") {
					flag_return = true;
				}
				break;
			case 's':
				if (k.first == "staticref/") {
					flag_staticref = true;
				}
				break;
			case 'u':
				if (k.first == "ucs_nmo") {
					ucs_nmo = strip_backticks(k.second);
				}
				break;
			case '!':    // ignore, it's a comment or something.
				break;
			default:
				std::cerr << "invalid key `" << k.first << "` value `"
						  << k.second << "`" << std::endl;
			}
		}
		map_type_size(param_types, param_sizes);
	}

	void dump(std::ostream& o) const {
		o << "opcode: " << opcode << std::endl;
		o << "name: " << name << std::endl;
		o << "asm_nmo: " << asm_nmo << std::endl;
		o << "asm_comment: " << asm_comment << std::endl;
		o << "ucs_nmo: " << ucs_nmo << std::endl;
		o << "num_bytes: " << num_bytes << std::endl;
		o << "param_types: ";
		for (const auto& param_type : param_types) {
			o << param_type << ',';
		}
		o << std::endl;
		o << "num_pop: " << num_pop << std::endl;
		o << "num_push: " << num_push << std::endl;
		o << "flag_call_effect: " << flag_call_effect << std::endl;
		o << "flag_return: " << flag_return << std::endl;
		o << "flag_paren: " << flag_paren << std::endl;
		o << "flag_indent_inc: " << flag_indent_inc << std::endl;
		o << "flag_indent_dec: " << flag_indent_dec << std::endl;
		o << "flag_indent_tmpinc: " << flag_indent_tmpinc << std::endl;
		o << "flag_indent_tmpdec: " << flag_indent_tmpdec << std::endl;
		o << "flag_debug: " << flag_debug << std::endl;
		o << "flag_nosemicolon: " << flag_nosemicolon << std::endl;
		o << "flag_abort: " << flag_abort << std::endl;
		o << "flag_staticref: " << flag_staticref << std::endl;
		o << "flag_loop: " << flag_loop << std::endl;
		o << "flag_new_effect: " << flag_new_effect << std::endl;
		o << "flag_method_effect: " << flag_method_effect << std::endl;
		o << "flag_function_effect: " << flag_function_effect << std::endl;
		o << "flag_not_param: " << flag_not_param << std::endl;
	}

	unsigned int opcode = 0;
	std::string  name;
	std::string  asm_nmo;
	std::string  asm_comment;
	std::string  ucs_nmo;
	unsigned int num_bytes = 0;

	unsigned int num_pop              = 0;
	unsigned int num_push             = 0;
	bool         flag_call_effect     = false;
	bool         flag_return          = false;
	bool         flag_paren           = false;
	bool         flag_indent_inc      = false;
	bool         flag_indent_dec      = false;
	bool         flag_indent_tmpinc   = false;
	bool         flag_indent_tmpdec   = false;
	bool         flag_debug           = false;
	bool         flag_nosemicolon     = false;
	bool         flag_abort           = false;
	bool         flag_staticref       = false;
	bool         flag_loop            = false;
	bool         flag_new_effect      = false;
	bool         flag_method_effect   = false;
	bool         flag_function_effect = false;
	bool         flag_not_param       = false;

	std::vector<std::string> param_types;
	// values caluclated from param_types
	std::vector<std::pair<unsigned int, bool>>
			param_sizes;    // .first==size of parameter in bytes
							// .second==whether to treat it as a relative offset
							// and calculate for it
};

extern std::vector<UCOpcodeData>                          opcode_table_data;
extern std::vector<std::pair<unsigned int, unsigned int>> opcode_jumps;

extern std::map<unsigned int, std::string> uc_intrinsics;

#endif
