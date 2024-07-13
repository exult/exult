/*
Copyright (C) 2000-2022  The Exult Team

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
#	include <config.h>
#endif

#include "Configuration.h"
#include "exult_constants.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::pair;
using std::string;
using std::vector;

enum DoOps {
	DoAdd,
	DoRem,
	DoGet
};

using DoListPair = pair<DoOps, vector<string>>;

using DoList = vector<DoListPair>;

DoList         dolist;
Configuration* config = new Configuration();
string         config_file_name;
bool           verbose = false;    // dump verbose output to cerr

/* #include <cstd_usage_function> */
void usage(unsigned int i) {
	cout << "cmanip - simple commandline, conf/ file manipulator" << endl
#ifdef HAVE_CONFIG_H
		 << "    compiled with " << PACKAGE << " " << VERSION << endl
#endif
		 << endl
		 << "usage:" << endl
		 << "\tcmanip <conffile> [option [parameters]] ..." << endl
		 << endl
		 << "options:" << endl
		 << "\t[-a | add | create | mk | new | modify] <key> <value>" << endl
		 << "\t\t\t- adds the <value> to the <key> in the <conffile>" << endl
		 << "\t[-r | rem | remove | rm | del] <key>" << endl
		 << "\t\t\t- removes the <key> from the <conffile>" << endl
		 << "\t[-x | get | extract | rd | see | read] <key>" << endl
		 << "\t\t\t- extracts the value set for <key> from the <conffile> or "
			"\"unknown\""
		 << endl
		 << "\t[-v | verbose]\t- print verbose output to stderr" << endl
		 << endl
		 << "examples:" << endl
		 << "\tcmanip exult.cfg add config/foo stuff rem config/bar" << endl
		 << "\tcmanip exult.cfg new config/foo \"more stuff\" add config/bar "
			"useless"
		 << endl
		 << "\tcmanip exult.cfg -v -r config/foo del config/bar" << endl;
	exit(i);
}

/* Turn the various command line parameters into operations that can be handled
 * by process_ops */
void read_params(const int argc, char* argv[]) {
	config_file_name = argv[1];

	// "I'd appreciate your input"...
	for (int i = 2; i < argc; i++) {
		const string s(argv[i]);

		/* Adds the value (argv[i+2]) to the key (argv[i+1]) in the conf file.
		 */
		if ((s == "add") || (s == "create") || (s == "mk") || (s == "new")
			|| (s == "-a") || (s == "modify")) {
			if (i + 2 >= argc) {
				cout << "error: insufficient parameters supplied for '" << s
					 << "'" << endl;
				usage(1);
			}

			DoListPair dlp;
			dlp.first = DoAdd;
			dlp.second.emplace_back(argv[i + 1]);
			dlp.second.emplace_back(argv[i + 2]);
			dolist.push_back(dlp);
			i += 2;
		}
		/* Removes the key (argv[i+1]) from the conf file.
			FIXME: Currently only sets it to 'null', since there is no 'remove'
		   ability in Configuration. */
		else if (
				(s == "rem") || (s == "remove") || (s == "rm") || (s == "mk")
				|| (s == "del") || (s == "-r")) {
			if (i + 1 >= argc) {
				cout << "error: insufficient parameters supplied for '" << s
					 << "'" << endl;
				usage(1);
			}

			DoListPair dlp;
			dlp.first = DoRem;
			dlp.second.emplace_back(argv[i + 1]);
			dolist.push_back(dlp);
			i++;
		}
		/* Added by Artaxerxes. Extracts the values for a given path. Do not
		   change anything. Just read.*/
		else if (
				(s == "extract") || (s == "read") || (s == "rd") || (s == "see")
				|| (s == "get") || (s == "-x")) {
			if (i + 1 >= argc) {
				cout << "error: insufficient parameters supplied for '" << s
					 << "'" << endl;
				usage(1);
			}

			DoListPair dlp;
			dlp.first = DoGet;
			dlp.second.emplace_back(argv[i + 1]);
			dolist.push_back(dlp);
			i++;
		}
		/* Just turns on 'verbose' mode. Nothing of particular intrest. */
		else if ((s == "-v") || (s == "verbose")) {
			verbose = true;
		} else {
			cout << "error: unknown parameter \"" << s << "\"" << endl;
		}
	}
}

/* Walk over the operations list (dolist) and execute each operation in the
   order it was listed on the command line */
void process_ops() {
	for (auto i = dolist.begin(); i != dolist.end(); i++) {
		if (i->first == DoAdd) {
			assert(i->second.size() == 2);
			if (verbose) {
				string s;
				assert(config != nullptr);
				config->value(i->second[0], s, "---nil---");
				cerr << "Original value of " << i->second[0] << " was " << s
					 << endl;
			}

			assert(config != nullptr);
			config->set(i->second[0], i->second[1], false);

			if (verbose) {
				cerr << "Added " << i->second[1] << " to " << i->second[0]
					 << endl;
			}
		}
		if (i->first == DoRem) {
			assert(i->second.size() == 1);
			if (verbose) {
				string s;
				assert(config != nullptr);
				config->value(i->second[0], s, "---nil---");
				cerr << "Original value was " << i->second[0] << " was " << s
					 << endl;
			}

			assert(config != nullptr);
			config->set(i->second[0], "", false);

			if (verbose) {
				cerr << "Removed " << i->second[0] << endl;
			}
		}
		if (i->first == DoGet) {
			assert(i->second.size() == 1);
			if (verbose) {
				string s;
				assert(config != nullptr);
				config->value(i->second[0], s, "unknown");
				cerr << "Return value for " << i->second[0] << " is " << s
					 << endl;
			}

			assert(config != nullptr);
			// config->set(i->second[0], "", false);
			string s;
			config->value(i->second[0], s, "unknown");
			cout << s << endl;
		}
	}
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		usage(0);
	}

	read_params(argc, argv);

	if (verbose) {
		cerr << "Operations:" << endl;
		for (auto& i : dolist) {
			cerr << '\t'
				 << ((i.first == DoAdd)
							 ? "add"
							 : ((i.first == DoRem)
										? "rem"
										: ((i.first == DoGet) ? "get"
															  : "unknown")))
				 << '\t';
			for (auto& j : i.second) {
				cerr << j << '\t';
			}
			cerr << endl;
		}
	}

	assert(config != nullptr);
	config->read_config_file(config_file_name);

	process_ops();

	assert(config != nullptr);
	config->write_back();

	return 0;
}
