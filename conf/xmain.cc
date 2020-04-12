/*
Copyright (C) 2000  Dancer A.L Vesperman

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
#include "Configuration.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include "common_types.h"
#include "utils.h"

using std::string;

using std::cout;
using std::cerr;
using std::endl;

Configuration *config;

void    dump_stringvec(std::vector<std::string> &vs, int expect = -2) {
	size_t  n;
	cout << "vs is " << vs.size() << " entries" << endl;
	for (n = 0; n < vs.size(); n++)
		cout << n << " : " << vs[n] << endl;
	if (expect == -2)
		return;
	assert(static_cast<int>(vs.size()) == expect);
}

void    test1() {
	cerr << "Reading config file: ./config.xml" << endl;

	config->read_config_file("./config.xml");

	cerr << "Config file read." << endl;

	int n;
	std::string r;

	cerr << "Dumping config to stdout." << endl;

	config->dump(cout, "\t");
	cout << endl;

	cerr << "Testing: config/audio/midi/device" << endl;

	std::string test_device("config/audio/midi/device");
	config->value(test_device, n, -1);
	cout << "Returned from reference, \"" << test_device << "\". Got '" << n << "'" << endl;
	assert(n == 5);

	std::string test_enabled("config/audio/midi/enabled");
	config->value(test_enabled, r, "--nil--");
	cout << "Returned from reference, \"" << test_enabled << "\". Got '" << r << "'" << endl;
	assert(r == "yes");

	std::string test_spaces("config/disk/u7path_with_spaces");
	config->value(test_spaces, r, "--nil--");
	cout << "Returned from reference, \"" << test_spaces << "\". Got '" << r << "'" << endl;
	assert(r == "d:\\ultima series\\ultima vii - the serpent isle");

	config->set("config/something/something/else", "wibble", false);

	std::string out = config->dump();
	cout << out << endl;

	std::vector<std::string> vs;

	vs = config->listkeys("config");
	dump_stringvec(vs, 6);

	vs = config->listkeys("config/audio");
	dump_stringvec(vs, 4);

	vs = config->listkeys("config/something", false);
	dump_stringvec(vs, 1);

	vs = config->listkeys("config/somenonexistantthing");
	dump_stringvec(vs, 0);

	config->clear();
	//cout << endl << config->dump() << endl;
	assert(config->dump() == "<config>\n</config>\n");

	Configuration config_slash(string(""), string("root"));
	//cout << endl << config_slash.dump() << endl;
	assert(config_slash.dump() == "<root>\n</root>\n");

	config->clear("foo");
	assert(config->dump() == "<foo>\n</foo>\n");

	Configuration confnew("./config.xml", "config");

	Configuration::KeyTypeList ktl;
	string basekey("config/audio");
	confnew.getsubkeys(ktl, basekey);

	cout << endl;

	for (Configuration::KeyTypeList::iterator i = ktl.begin(); i != ktl.end(); i++)
		cout << "Key:\t" << i->first << endl << "Value:\t" << i->second << endl;
	assert(ktl.size() == 7);

}

void test2() {
	setup_program_paths();
	config->read_config_file("exult.cfg");
	config->dump(cout, "\t") << endl;
}

int main(int argc, char *argv[]) {
	config = new Configuration();

	cerr << "Starting tests..." << endl;
	test1();
	if (argc > 1)
		if (strcmp(argv[1], "--exult-cfg") == 0)
			test2();

	return 0;
}
