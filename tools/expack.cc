/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#include "Flex.h"
#include "U7file.h"
#include "U7fileman.h"
#include "U7obj.h"
#include "crc.h"
#include "databuf.h"
#include "exceptions.h"

#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using std::atoi;
using std::cerr;
using std::cout;
using std::endl;
using std::exit;
using std::ifstream;
using std::ofstream;
using std::size_t;
using std::string;
using std::vector;

enum Arch_mode {
	NOMODE,
	LIST,
	EXTRACT,
	CREATE,
	ADD,
	RESPONSE
};

/*
 *  Parse a number, and quit with an error msg. if not found.
 */

bool is_text_file(const string& fname) {
	const size_t len = fname.size();

	// only if the filename is greater than 4 chars
	return len > 4 && fname[len - 4] == '.'
		   && (fname[len - 3] == 't' || fname[len - 3] == 'T')
		   && (fname[len - 2] == 'x' || fname[len - 2] == 'X')
		   && (fname[len - 1] == 't' || fname[len - 1] == 'T');
}

bool is_null_entry(const string& fname) {
	const size_t len = fname.size();

	return len >= 4 && fname[len - 4] == 'N' && fname[len - 3] == 'U'
		   && fname[len - 2] == 'L' && fname[len - 1] == 'L';
}

void set_mode(Arch_mode& mode, Arch_mode new_mode) {
	if (mode != NOMODE) {
		cerr << "Error: cannot specify multiple modes" << endl;
		exit(1);
	} else {
		mode = new_mode;
	}
}

// Converts all .'s to _'s
void make_header_name(string& filename) {
	size_t i = filename.size();

	while (i--) {
		if (filename[i] == '.') {
			filename[i] = '_';
		} else if (
				filename[i] == '/' || filename[i] == '\\'
				|| filename[i] == ':') {
			break;
		}
	}
}

// Makes a name uppercase
void make_uppercase(string& name) {
	for (auto& chr : name) {
		chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
	}
}

// strips a path from a filename
void strip_path(string& filename) {
	int i = static_cast<int>(filename.size());

	while (i--) {
		if (filename[i] == '\\' || filename[i] == '/' || filename[i] == ':') {
			break;
		}
	}

	// Has a path
	if (i >= 0) {
		filename = filename.substr(i + 1);
	}
}

long get_file_size(string& fname) {
	if (is_null_entry(fname)) {
		return 0;    // an empty entry
	}

	try {
		auto pFin = U7open_in(fname, is_text_file(fname));
		if (!pFin) {
			cerr << "Failed to open " << fname << endl;
			return 0;
		}
		auto& fin = *pFin;
		// Lets avoid undefined behavior. See
		// http://cpp.indi.frih.net/blog/2014/09/how-to-read-an-entire-file-into-memory-in-cpp/
		fin.ignore(std::numeric_limits<std::streamsize>::max());
		return fin.gcount();
	} catch (const std::exception& err) {
		cerr << err.what() << endl;
		return 0;
	}
}

bool Write_Object(U7object& obj, const char* fname) {
	try {
		auto pOut = U7open_out(fname, false);
		if (!pOut) {
			cerr << "Failed to open " << fname << endl;
			return false;
		}
		auto&  out = *pOut;
		size_t l;
		auto   n = obj.retrieve(l);
		if (!n) {
			return false;
		}
		out.write(reinterpret_cast<char*>(n.get()), l);
	} catch (const std::exception& err) {
		cerr << err.what() << endl;
		return false;
	}
	return true;
}

// This getline() accepts any kind of endline on any platform
// (So it will accept windows linefeeds in linux, and vice versa)
void getline(ifstream& file, string& buf) {
	buf.clear();
	char c;
	file.get(c);

	while ((c >= ' ' || c == '\t') && file.good()) {
		buf += c;
		file.get(c);
	}

	while (file.peek() < ' ' && file.peek() != '\t' && file.good()) {
		file.get(c);
	}
}

int main(int argc, char** argv) {
	Arch_mode      mode = NOMODE;
	string         fname;
	string         hname;
	string         hprefix;
	vector<string> file_names;
	file_names.reserve(1200);

	if (argc > 2) {
		fname = argv[2];
		string type(argv[1]);
		if (type.size() == 2 && type[0] == '-') {
			switch (type[1]) {
			case 'i': {
				string path_prefix;

				std::unique_ptr<std::istream> pRespfile;
				const size_t                  slash = fname.rfind('/');
				if (slash != string::npos) {
					path_prefix = fname.substr(0, slash + 1);
				}
				set_mode(mode, RESPONSE);
				try {
					pRespfile = U7open_in(fname, true);
				} catch (const file_open_exception& e) {
					cerr << e.what() << endl;
					exit(1);
				}
				if (!pRespfile) {
					cerr << "Failed to open " << fname << endl;
					exit(1);
				}
				auto& respfile = *pRespfile;

				// Read the output file name
				string temp;
				getline(respfile, temp);
				fname = path_prefix + temp;

				// Header file name
				hprefix = temp;
				make_header_name(hprefix);
				hname = path_prefix + hprefix + ".h";
				strip_path(hprefix);
				make_uppercase(hprefix);

				unsigned int shnum   = 0;
				int          linenum = 2;
				while (respfile.good()) {
					getline(respfile, temp);
					if (!temp.empty()) {
						const char* ptr = temp.c_str();
						if (*ptr == ':') {
							ptr++;
							// Shape # specified.
							char*      eptr;
							const long num = strtol(ptr, &eptr, 0);
							if (eptr == ptr) {
								cerr << "Line " << linenum
									 << ": shapenumber not found. The correct "
										"format of a line with specified shape "
										"is ':shapenum:filename'."
									 << endl;
								exit(1);
							}
							shnum = static_cast<unsigned int>(num);
							ptr   = eptr;
							assert(*ptr == ':');
							ptr++;
						}
						const string temp2 = path_prefix + ptr;
						if (shnum >= file_names.size()) {
							file_names.resize(shnum + 1);
						}
						file_names[shnum] = temp2;
						shnum++;
						linenum++;
					}
				}
			} break;
			case 'l':
				set_mode(mode, LIST);
				break;
			case 'x':
				set_mode(mode, EXTRACT);
				break;
			case 'c': {
				for (int i = 0; i < argc - 3; i++) {
					file_names.emplace_back(argv[i + 3]);
				}
				set_mode(mode, CREATE);
				break;
			}
			case 'a':
				set_mode(mode, ADD);
				break;
			default:
				mode = NOMODE;
				break;
			}
		}
	}

	switch (mode) {
	case LIST: {
		if (argc != 3) {
			break;
		}
		U7FileManager* fm    = U7FileManager::get_ptr();
		U7file*        f     = fm->get_file_object(fname);
		const size_t   count = f->number_of_objects();
		cout << "Archive: " << fname << endl;
		cout << "Type: " << f->get_archive_type() << endl;
		cout << "Size: " << count << endl;
		cout << "-------------------------" << endl;
		for (size_t i = 0; i < count; i++) {
			size_t len;
			auto   buf = f->retrieve(static_cast<uint32>(i), len);
			cout << i << "\t" << len << endl;
		}
	} break;
	case EXTRACT: {
		constexpr const char ext[] = "u7o";
		if (argc == 4) {
			U7object            f(fname, atoi(argv[3]));
			const unsigned long nobjs = f.number_of_objects();
			const unsigned long n     = strtoul(argv[3], nullptr, 0);
			if (n >= nobjs) {
				cerr << "Obj. #(" << n << ") is too large.  ";
				cerr << "Flex size is " << nobjs << '.' << endl;
				exit(1);
			}
			char outfile[32];
			snprintf(outfile, sizeof(outfile), "%05lu.%s", n, ext);
			Write_Object(f, outfile);    // may throw!
		} else {
			U7FileManager* fm    = U7FileManager::get_ptr();
			U7file*        f     = fm->get_file_object(fname);
			const int      count = static_cast<int>(f->number_of_objects());
			for (int index = 0; index < count; index++) {
				U7object o(fname, index);
				char     outfile[32];
				snprintf(outfile, sizeof(outfile), "%05d.%s", index, ext);
				Write_Object(o, outfile);
			}
		}
	} break;
	case RESPONSE:
	case CREATE:
		try {
			OFileDataSource flex(fname.c_str());

			std::unique_ptr<std::ostream> pHeader;
			if (hname.empty()) {    // Need header name.
				hprefix = fname;
				make_header_name(hprefix);
				hname = hprefix + ".h";
				strip_path(hprefix);
				make_uppercase(hprefix);
			}
			try {
				pHeader = U7open_out(hname, true);
			} catch (const file_open_exception& e) {
				cerr << e.what() << endl;
				exit(1);
			}
			if (!pHeader) {
				cerr << "Failed to open " << hname << endl;
				exit(1);
			}
			auto& header = *pHeader;

			// The FLEX title
			Flex_writer writer(flex, "Exult Archive", file_names.size());

			// The beginning of the header
			string temp = fname;
			strip_path(temp);
			header << "// Header for \"" << temp << "\" Created by expack"
				   << std::endl
				   << std::endl;
			header << "// DO NOT MODIFY" << std::endl << std::endl;
			header << "#ifndef " << hprefix << "_INCLUDED" << std::endl;
			header << "#define " << hprefix << "_INCLUDED" << std::endl
				   << std::endl;

			// The files
			for (unsigned int i = 0; i < file_names.size(); i++) {
				const size_t fsize = file_names[i].empty()
											 ? 0
											 : get_file_size(file_names[i]);
				if (!file_names[i].empty() && fsize > 0) {
					IFileDataSource ifs(
							file_names[i].c_str(), is_text_file(file_names[i]));
					if (!ifs.good()) {
						cerr << "Error reading from file " << file_names[i]
							 << endl;
						exit(1);
					}
					writer.write_object(ifs);

					string hline = file_names[i];
					strip_path(hline);
					make_header_name(hline);
					make_uppercase(hline);
					header << "#define\t" << hprefix << "_" << hline << "\t\t"
						   << i << std::endl;
				} else {
					writer.empty_object();
				}
			}
			writer.flush();

			const uint32 crc32val = crc32(fname.c_str());
			header << std::endl << "#define\t" << hprefix << "_CRC32\t0x";
			header << std::hex << crc32val << std::dec << "U" << std::endl;

			header << std::endl << "#endif" << std::endl << std::endl;

		} catch (const file_open_exception& e) {
			cerr << e.what() << endl;
			exit(1);
		}
		break;
	default:
		cout << "Usage:" << endl
			 << argv[0] << " -[l|x|c] file [index]" << endl
			 << argv[0] << " -i indexfile" << endl;
		break;
	}
	return 0;
}
