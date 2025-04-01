
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "span.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

using std::cerr;
using std::cout;
using std::endl;
using std::ios;
using std::ofstream;
using std::ostream;
using std::setbase;
using std::setfill;
using std::setw;
using std::string;
using std::string_view;

using namespace std::string_view_literals;

void gen_intrinsic_table(
		ofstream& o, const tcb::span<const string_view>& table) {
	o << "<intrinsics>" << endl;
	for (size_t i = 0; i < table.size(); i++) {
		o << "\t<0x" << setw(2) << i << "> " << table[i];
		if (table[i] == "UNKNOWN") {
			o << '_' << setw(2) << i;
		}
		o << " </>" << endl;
	}
	o << "</>" << endl;
}

void bg_out(const string& fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) #NAME##sv
	constexpr static const std::array bgut{
#include "bgintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR
	gen_intrinsic_table(o, bgut);
	o.close();
}

void si_out(const string& fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) #NAME##sv
	constexpr static const std::array siut{
#include "siintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR

	gen_intrinsic_table(o, siut);

	o.close();
}

void sibeta_out(const string& fname) {
	ofstream o;
	o.open(fname.c_str());

	if (o.fail()) {
		cerr << "error: could not open `" << fname << "` for writing" << endl;
		exit(1);
	}

	o << setfill('0') << setbase(16);
	o.setf(ios::uppercase);

#define USECODE_INTRINSIC_PTR(NAME) #NAME##sv
	constexpr static const std::array sibut{
#include "sibetaintrinsics.h"
	};
#undef USECODE_INTRINSIC_PTR

	gen_intrinsic_table(o, sibut);

	o.close();
}

int main(int argc, char** argv) {
	if (argc != 4) {
		cout << "usage:" << endl
			 << "\thead2data <bg outputfile> <si outputfile> <si beta "
				"outputfile>"
			 << endl
			 << endl
			 << "\tWhere the output files are the relative pathnames to the "
				"datafiles"
			 << endl
			 << "\tto be output." << endl
			 << "\teg. head2data data/u7bgintrinsics.data "
				"data/u7siintrinsics.data data/u7sibetaintrinsics.data"
			 << endl;
		return 1;
	}

	bg_out(string(argv[1]));
	si_out(string(argv[2]));
	sibeta_out(string(argv[3]));

	return 0;
}
