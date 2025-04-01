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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ops.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::map;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

#define MAX_NO_OPCODES 512
vector<UCOpcodeData> opcode_table_data(MAX_NO_OPCODES);

vector<pair<unsigned int, unsigned int>> opcode_jumps;

map<unsigned int, string> uc_intrinsics;

map<string, pair<unsigned int, bool>> type_size_map;

void ucxtInit::init(const Configuration& config, const UCOptions& options) {
	datadir = get_datadir(config, options);

	misc_data = "u7misc.data";
	misc_root = "misc";

	opcodes_data = "u7opcodes.data";
	opcodes_root = "opcodes";

	bg_intrinsics_data = "u7bgintrinsics.data";
	bg_intrinsics_root = "intrinsics";

	si_intrinsics_data = "u7siintrinsics.data";
	si_intrinsics_root = "intrinsics";

	sibeta_intrinsics_data = "u7sibetaintrinsics.data";
	sibeta_intrinsics_root = "intrinsics";

	if (options.verbose) {
		cout << "Initing misc..." << endl;
	}
	misc();

	if (options.verbose) {
		cout << "Initing opcodes..." << endl;
	}
	opcodes();

	if (options.verbose) {
		cout << "Initing intrinsics..." << endl;
	}
	if (options.game_bg() || options.game_fov()) {
		intrinsics(bg_intrinsics_data, bg_intrinsics_root);
	} else if (options.game_si() || options.game_ss()) {
		intrinsics(si_intrinsics_data, si_intrinsics_root);
	} else if (options.game_sib()) {
		intrinsics(sibeta_intrinsics_data, sibeta_intrinsics_root);
	}
}

string ucxtInit::get_datadir(
		const Configuration& config, const UCOptions& options) {
	string datadir;

	// just to handle if people are going to compile with makefile.unix,
	// unsupported, but occasionally useful
#ifdef HAVE_CONFIG_H
	if (!options.noconf) {
		config.value("config/ucxt/root", datadir, EXULT_DATADIR);
	}
#else
	if (!options.noconf) {
		config.value("config/ucxt/root", datadir, "data/");
	}
#endif

	if (!datadir.empty() && datadir[datadir.size() - 1] != '/'
		&& datadir[datadir.size() - 1] != '\\') {
		datadir += '/';
	}
	if (options.verbose) {
		cout << "datadir: " << datadir << endl;
	}

	return datadir;
}

void ucxtInit::misc() {
	Configuration miscdata(datadir + misc_data, misc_root);

	Configuration::KeyTypeList om;
	miscdata.getsubkeys(om, misc_root + "/offset_munge");

	Configuration::KeyTypeList st;
	miscdata.getsubkeys(st, misc_root + "/size_type");

	// For each size type (small/long/byte/etc.)
	for (auto& k : st) {
		bool munge_offset = false;

		const string tmpstr(k.first + "/");

		/* ... we need to find out if we should munge it's parameter
			that is, it's some sort of goto target (like offset) or such */
		for (auto& m : om) {
			if (m.first.size() - 1 == k.first.size()) {
				if (m.first == tmpstr) {
					//				if(m->first.compare(0, m->first.size()-1,
					// k->first, 0, k->first.size())==0)
					munge_offset = true;
				}
			}
		}

		// once we've got it, add it to the map
		const pair<unsigned int, bool> tsm_tmp(
				static_cast<unsigned int>(strtol(k.second.c_str(), nullptr, 0)),
				munge_offset);
		type_size_map.insert(
				pair<string, pair<unsigned int, bool>>(k.first, tsm_tmp));
	}
}

/* constructs the usecode tables from datafiles in the /ucxt hierachy */
void ucxtInit::opcodes() {
	Configuration opdata(datadir + opcodes_data, opcodes_root);

	vector<string> keys = opdata.listkeys(opcodes_root);

	for (auto& key : keys) {
		if (key[0] != '!') {
			Configuration::KeyTypeList ktl;

			opdata.getsubkeys(ktl, key);

			if (!ktl.empty()) {
				const unsigned int i = static_cast<unsigned int>(
						strtol(key.substr(key.find_first_of('0')).c_str(),
							   nullptr, 0));
				opcode_table_data[i] = UCOpcodeData(i, ktl);
			}
		}
	}

	/* Create an {opcode, parameter_index} array of all opcodes that
		execute a 'jump' statement */
	for (auto& op : opcode_table_data) {
		for (unsigned int i = 0; i < op.param_sizes.size(); i++) {
			if (op.param_sizes[i].second) {    // this is a calculated offset
				opcode_jumps.emplace_back(
						op.opcode, i + 1);    // parameters are stored as base 1
			}
		}
	}
}

void ucxtInit::intrinsics(
		const string& intrinsic_data, const string& intrinsic_root) {
	Configuration intdata(datadir + intrinsic_data, intrinsic_root);

	Configuration::KeyTypeList ktl;

	intdata.getsubkeys(ktl, intrinsic_root);

	for (auto& k : ktl) {
		uc_intrinsics.insert(pair<unsigned int, string>(
				static_cast<unsigned int>(strtol(k.first.c_str(), nullptr, 0)),
				k.second));
	}
}

/* To be depricated when I get the complex std::vector<std::string> splitter
 * online */
std::vector<std::string> qnd_ocsplit(const std::string& s) {
	assert((s[0] == '{') && (s[s.size() - 1] == '}'));

	std::vector<std::string> vs;
	std::string              tstr;

	for (const char i : s) {
		if (i == ',') {
			vs.push_back(tstr);
			tstr = "";
		} else if (i == '{' || i == '}') {
			/* nothing */
		} else {
			tstr += i;
		}
	}
	if (!tstr.empty()) {
		vs.push_back(tstr);
	}

	return vs;
}

std::vector<std::string> str2vec(const std::string& s) {
	std::vector<std::string> vs;
	unsigned int             lasti = 0;

	// if it's empty return null
	if (s.empty()) {
		return vs;
	}

	bool indquote = false;
	for (unsigned int i = 0; i < s.size(); i++) {
		if (s[i] == '"') {
			indquote = !indquote;
		} else if (isspace(static_cast<unsigned char>(s[i])) && (!indquote)) {
			if (lasti != i) {
				if ((s[lasti] == '"') && (s[i - 1] == '"')) {
					if ((lasti + 1) < (i - 1)) {
						vs.push_back(s.substr(lasti + 1, i - lasti - 2));
					}
				} else {
					vs.push_back(s.substr(lasti, i - lasti));
				}
			}

			lasti = i + 1;
		}
		if (i == s.size() - 1) {
			if ((s[lasti] == '"') && (s[i] == '"')) {
				if ((lasti + 1) < (i - 1)) {
					vs.push_back(s.substr(lasti + 1, i - lasti - 2));
				}
			} else {
				vs.push_back(s.substr(lasti, i - lasti + 1));
			}
		}
	}
	return vs;
}

void map_type_size(
		const std::vector<std::string>&             param_types,
		std::vector<std::pair<unsigned int, bool>>& param_sizes) {
	for (const auto& param_type : param_types) {
		auto tsm(type_size_map.find(param_type));
		if (tsm == type_size_map.end()) {
			cerr << "error: No size type `" << param_type << "`" << endl;
			assert(tsm != type_size_map.end());
		} else {
			param_sizes.emplace_back(tsm->second);
		}
	}
}
