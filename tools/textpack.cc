/**
 ** Textpack.cc - Convert text file to a Flex file.
 **
 ** Written: 2/14/2002
 **/

/*
 *  Copyright (C) 2002  The Exult Team
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

#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include "Flex.h"
#include "utils.h"
#include "exceptions.h"
#include "msgfile.h"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::exit;
using std::ifstream;
using std::ofstream;
using std::istream;
using std::ostream;
using std::size_t;
using std::string;
using std::vector;

/*
 *  Read in a Flex file where each entry is a 0-delimited text string.
 */

static void Read_flex(
    const char *filename,       // File to read.
    vector<string> &strings     // Strings are stored here.
) {
	FlexFile in(filename);      // May throw exception.
	int cnt = in.number_of_objects();
	strings.resize(cnt);
	for (int i = 0; i < cnt; i++) {
		size_t len;
		auto ptr = in.retrieve(i, len);
		if (len) {     // Not empty?
			strings[i] = reinterpret_cast<char*>(ptr.get());
		}
	}
}

/*
 *  Write out a Flex file where each entry is a 0-delimited text string.
 */

static void Write_flex(
    const char *filename,       // File to write.
    const char *title,          // For the header.
    vector<string> &strings     // Okay if some are null.
) {
	OFileDataSource out(filename);      // May throw exception.
	Flex_writer writer(out, title, strings.size());
	for (auto& str : strings) {
		if (!str.empty())
			writer.write_object(str.c_str(), str.size() + 1);
		else
			writer.empty_object();
	}
}

/*
 *  Write out text, with each line in the form "nnn:sssss", where nnn is
 *  the Flex entry #, and anything after the ':' is the string.
 *  NOTES:  Null entry #'s will be skipped in the output.
 *      Max. text length is 1024.
 */

static void Write_text(
    ostream &out,
    vector<string> &strings     // Strings to write.
) {
	out << "# Written by Exult Textpack tool" << endl;
	int cnt = strings.size();
	for (int i = 0; i < cnt; i++) {
		string &text = strings[i];
		if (text.empty())
			continue;
		if (text.size() + 1 > 1024) {
			cerr << "Text in entry " << i << " is too long"
			     << endl;
			exit(1);
		}
		out << i << ':' << text << endl;
	}
	out.flush();
}

/*
 *  Print usage and exit.
 */

static void Usage() {
	cerr << "Usage: textpack -[x|c] flexfile [textfile]" << endl <<
	     "    Missing [textfile] => stdin/stdout" << endl;
	exit(1);
}

/*
 *  Create or extract from Flex files consisting of text entries.
 */

int main(
    int argc,
    char **argv
) {
	if (argc < 3 || argv[1][0] != '-')
		Usage();        // (Exits.)
	char *flexname = argv[2];
	vector<string> strings;     // Text stored here.
	switch (argv[1][1]) {   // Which function?
	case 'c':           // Create Flex.
		if (argc >= 4) {    // Text filename given?
			ifstream in;    // Open as text.
			try {
				U7open(in, argv[3], true);
			} catch (exult_exception &e) {
				cerr << e.what() << endl;
				exit(1);
			}
			if (Read_text_msg_file(in, strings) == -1)
				exit(1);
		} else          // Default to stdin.
			if (Read_text_msg_file(cin, strings) == -1)
				exit(1);
		try {
			Write_flex(flexname, "Flex created by Exult", strings);
		} catch (exult_exception &e) {
			cerr << e.what() << endl;
			exit(1);
		}
		break;
	case 'x':           // Extract to text.
		try {
			Read_flex(flexname, strings);
		} catch (exult_exception &e) {
			cerr << e.what() << endl;
			exit(1);
		}
		if (argc >= 4) {    // Text file given?
			ofstream out;
			try {
				U7open(out, argv[3],  true);
			} catch (exult_exception &e) {
				cerr << e.what() << endl;
				exit(1);
			}
			Write_text(out, strings);
		} else
			Write_text(cout, strings);
		break;
	default:
		Usage();
	}
	return 0;
}
