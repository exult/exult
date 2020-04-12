
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <vector>
#include <fstream>
#include "array_size.h"

using std::cout;
using std::cerr;
using std::endl;
using std::ostream;
using std::string;
using std::setfill;
using std::setbase;
using std::setw;
using std::ios;
using std::ofstream;

#ifndef TO_STRING
#if defined __STDC__ && __STDC__
#define TO_STRING(x) #x
#else
#define TO_STRING(x) "x"
#endif
#endif

void gen_intrinsic_table(ofstream& o, std::string const table[], unsigned int len) {
	o << "<intrinsics>" << endl;
	for (unsigned int i = 0; i < len; i++) {
		o << "\t<0x" << setw(2) << i << "> " << table[i];
		if (table[i] == "UNKNOWN") {
			o << '_' << setw(2) << i;
		}
		o << " </>" << endl;
	}
	o << "</>" << endl;
}

void bg_out(const string &fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) std::string(TO_STRING(NAME))
	std::string bgut[] = {
#include "bgintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR

	gen_intrinsic_table(o, bgut, array_size(bgut));
	o.close();
}

void si_out(const string &fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) std::string(TO_STRING(NAME))
	std::string siut[] = {
#include "siintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR

	gen_intrinsic_table(o, siut, array_size(siut));

	o.close();
}

void sibeta_out(const string &fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) std::string(TO_STRING(NAME))
	std::string sibut[] = {
#include "sibetaintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR

	gen_intrinsic_table(o, sibut, array_size(sibut));

	o.close();
}

int main(int argc, char **argv) {
	if (argc != 4) {
		cout << "usage:" << endl
		     << "\thead2data <bg outputfile> <si outputfile> <si beta outputfile>" << endl
		     << endl
		     << "\tWhere the output files are the relative pathnames to the datafiles" << endl
		     << "\tto be output." << endl
		     << "\teg. head2data data/u7bgintrinsics.data data/u7siintrinsics.data data/u7sibetaintrinsics.data" << endl;
		return 1;
	}

	bg_out(string(argv[1]));
	si_out(string(argv[2]));
	sibeta_out(string(argv[3]));

	return 0;
}

