/*
 *  Copyright (C) 2000-2013  The Exult Team
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

#include <iostream>
#include <cstdlib>
#include "args.h"

using std::cerr;
using std::endl;
using std::string;
using std::strtol;
using std::strtoul;

void    Args::declare(const char *s, bool *b, bool defval) {
	string  ss;

	ss = s;

	Opts o;
	o.option = ss;
	o.bval = b;
	o.dbval = defval;
	o.valuetype = Opts::type_bool;
	options.push_back(o);
}

void    Args::declare(const char *s, string *b, const char *defval) {
	string  ss;

	ss = s;

	Opts o;
	o.option = ss;
	o.sval = b;
	o.dsval = defval ? defval : "";
	*o.sval = defval ? defval : "";
	o.valuetype = Opts::type_string;
	options.push_back(o);
}

void    Args::declare(const char *s, int *b, int defval) {
	string  ss;

	ss = s;

	Opts o;
	o.option = ss;
	o.ival = b;
	o.dival = defval;
	*o.ival = defval;
	o.valuetype = Opts::type_int;
	options.push_back(o);
}

void    Args::declare(const char *s, uint32 *b, uint32 defval) {
	string  ss;

	ss = s;

	Opts o;
	o.option = ss;
	o.uval = b;
	o.duval = defval;
	*o.uval = defval;
	o.valuetype = Opts::type_unsigned;
	options.push_back(o);
}

void    Args::process(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		for (unsigned int j = 0; j < options.size() && i < argc; j++) {
			switch (options[j].valuetype) {
			case Opts::no_type:
				continue;
			case Opts::type_bool:
				if (options[j].option == argv[i])
					*(options[j].bval) = options[j].dbval;
				break;
			case Opts::type_string: {
				if (options[j].option == argv[i]) {
					// We want the _next_ argument
					if (++i >= argc) {
						cerr << "Data not specified for argument '" << options[j].option << "'. Using default." << endl;
						break;
					}
					*(options[j].sval) = argv[i];
				}
				break;
			}
			case Opts::type_int: {
//					char buf[64];
				if (options[j].option == argv[i]) {
					// We want the _next_ argument
					if (++i >= argc) {
						cerr << "Data not specified for argument '" << options[j].option << "'. Using default." << endl;
						break;
					}
					*(options[j].ival) = strtol(argv[i], nullptr, 10);
				}
				break;
			}
			case Opts::type_unsigned: {
//					char buf[64];
				if (options[j].option == argv[i]) {
					// We want the _next_ argument
					if (++i >= argc) {
						cerr << "Data not specified for argument '" << options[j].option << "'. Using default." << endl;
						break;
					}
					*(options[j].uval) = strtoul(argv[i], nullptr, 10);
				}
				break;
			}
			}
		}
	}

}
