/**
 ** data_utils.h: Generic data template functions.
 **/

/*
Copyright (C) 2009 Exult Team

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

#ifndef INCL_DATA_UTILS
#define INCL_DATA_UTILS 1

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "exult_constants.h"
#include "utils.h"
#include "U7obj.h"
#include "databuf.h"
#include "msgfile.h"
#include "fnames.h"
#include "ignore_unused_variable_warning.h"

/*
 *  Generic vector data handler routines.
 *  They all assume that the template class has the following
 *  operators defined:
 *      (1) operator< (which must be a strict weak order)
 *      (2) operator==
 *      (3) operator!=
 *      (4) operator= that sets modified flag if needed.
 *  They also assume that the vector is totally ordered
 *  with the operator< -- using these functions will ensure
 *  that this is the case.
 */
template <typename T>
void add_vector_info(const T &inf, std::vector<T> &vec) {
	// Find using operator<.
	auto it = std::lower_bound(vec.begin(), vec.end(), inf);
	if (it == vec.end() || *it != inf)  // Not found.
		vec.insert(it, inf);    // Add new.
	else {  // Already exists.
		bool st = it->have_static();
		bool inv = it->is_invalid();
		it->set(inf);   // Replace information.
		if (st && inv && !inf.is_invalid() &&
		        !inf.from_patch() && !inf.was_modified())
			it->set_modified(false);
	}
}
template <typename T>
void copy_vector_info(const std::vector<T> &from, std::vector<T> &to) {
	if (from.size()) {
		to.resize(from.size());
		std::copy(from.begin(), from.end(), to.begin());
	} else
		to.clear();
}
template <typename T>
std::vector<T> &set_vector_info(bool tf, std::vector<T> &vec) {
	invalidate_vector(vec);
	if (!tf)
		clean_vector(vec);
	return vec;
}
template <typename T>
void invalidate_vector(std::vector<T> &vec) {
	for (auto& elem : vec)
		elem.set_invalid(true);
}

template <typename T>
void clean_vector(std::vector<T> &vec) {
	vec.erase(std::remove_if(vec.begin(), vec.end(),
		[](T& elem) {
			return elem.is_invalid() && !elem.have_static();
		}
		), vec.end());
}

template <class T, typename U>
static const T *Search_vector_data_single_wildcard(
    const std::vector<T> &vec,
    int src,
    U T::*dat
) {
	if (vec.empty())    // Not found.
		return nullptr;
	T inf;
	inf.*dat = src;
	// Try finding exact match first.
	auto it = std::lower_bound(vec.begin(), vec.end(), inf);
	if (it == vec.end())    // Nowhere to be found.
		return nullptr;
	else if (*it == inf && !it->is_invalid())   // Have it already.
		return &*it;
	// Try wildcard shape.
	inf.*dat = -1;
	it = std::lower_bound(it, vec.end(), inf);
	if (it == vec.end() || *it != inf   // It just isn't there...
	        || it->is_invalid())   // ... or it is invalid.
		return nullptr;
	else    // At last!
		return &*it;
}

template <class T>
static const T *Search_vector_data_double_wildcards(
    const std::vector<T> &vec,
    int frame, int quality,
    short T::*fr, short T::*qual
) {
	if (vec.empty())
		return nullptr;   // No name.
	T inf;
	inf.*fr = frame;
	inf.*qual = quality;
	// Try finding exact match first.
	auto it = std::lower_bound(vec.begin(), vec.end(), inf);
	if (it == vec.end())    // Nowhere to be found.
		return nullptr;
	else if (*it == inf && !it->is_invalid())   // Have it already.
		return &*it;
	// We only have to search forward for a match.
	if (quality != -1) {
		if ((*it).*fr == frame) {
			// Maybe quality is to blame. Try wildcard quality.
			inf.*qual = -1;
			it = std::lower_bound(it, vec.end(), inf);
			if (it == vec.end())    // Nowhere to be found.
				return nullptr;
			else if (*it == inf && !it->is_invalid())   // We got it!
				return &*it;
		}
		// Maybe frame is to blame? Try search for specific
		// quality with wildcard frame.
		inf.*qual = quality;
		inf.*fr = -1;
		it = std::lower_bound(it, vec.end(), inf);
		if (it == vec.end())    // Nowhere to be found.
			return nullptr;
		else if (*it == inf && !it->is_invalid())   // We got it!
			return &*it;
		inf.*qual = -1;
	}
	// *Still* haven't found it. Last try: wildcard frame *and* quality.
	inf.*fr = -1;
	it = std::lower_bound(it, vec.end(), inf);
	if (it == vec.end() || *it != inf || *it != inf // It just isn't there...
	        || it->is_invalid())   // ... or it is invalid.
		return nullptr;
	else    // At last!
		return &*it;
}

/*
 *  Generic data handler routine.
 */
template <typename T>
T *set_info(bool tf, T *&pt) {
	if (!tf) {
		delete pt;
		pt = nullptr;
	} else if (!pt)
		pt = new T();
	return pt;
}

/*
 *  Get # entries of binary data file (with Exult extension).
 */
inline int Read_count(std::istream &in) {
	int cnt = Read1(in);    // How the originals did it.
	if (cnt == 255)
		cnt = Read2(in);    // Exult extension.
	return cnt;
}

/*
 *  Write # entries of binary data file (with Exult extension).
 */
inline void Write_count(
    std::ofstream &out,
    int cnt
) {
	if (cnt >= 255) {
		// Exult extension.
		out.put(static_cast<char>(255));
		Write2(out, cnt);
	} else
		out.put(static_cast<char>(cnt));
}

/*
 *  Generic base data-agnostic reader class.
 */
class Base_reader {
protected:
	bool haveversion;
	virtual void read_data(std::istream &in, size_t index, int version,
	                       bool patch, Exult_Game game, bool binary) {
		ignore_unused_variable_warning(in, index, version, patch, game, binary);
	}
	// Binary data file.
	void read_binary_internal(std::istream &in, bool patch, Exult_Game game) {
		int vers = 0;
		if (haveversion)
			vers = Read1(in);
		size_t cnt = Read_count(in);
		for (size_t j = 0; j < cnt; j++)
			read_data(in, j, vers, patch, game, true);
	}
public:
	Base_reader(bool h)
		:   haveversion(h)
	{  }
	virtual ~Base_reader() = default;
	// Text data file.
	void parse(std::vector<std::string> &strings, int version, bool patch, Exult_Game game) {
		for (size_t j = 0; j < strings.size(); j++) {
			if (!strings[j].empty()) {
				std::istringstream strin(strings[j], std::ios::in);
				read_data(strin, j, version, patch, game, false);
			}
		}
		strings.clear();
	}
	// Binary data file.
	void read(const char *fname, bool patch, Exult_Game game) {
		if (!U7exists(fname))
			return;
		std::ifstream fin;
		U7open(fin, fname);
		read_binary_internal(fin, patch, game);
		fin.close();
	}
	// Binary resource file.
	void read(Exult_Game game, int resource) {
		// Only for BG and SI.
		if (game != BLACK_GATE && game != SERPENT_ISLE)
			return;
		/*  ++++ Not because of ES.
		snprintf(buf, 50, "config/%s", fname);
		str_int_pair resource = game->get_resource(buf);
		U7object txtobj(resource.str, resource.num);
		*/
		bool bg = game == BLACK_GATE;
		const char *flexfile =
		    bg ? BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX)
		    : BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);
		U7object txtobj(flexfile, resource);
		std::size_t len;
		auto txt = txtobj.retrieve(len);
		std::string databuf(reinterpret_cast<char*>(txt.get()), len);
		std::istringstream strin(databuf, std::ios::in | std::ios::binary);
		read_binary_internal(strin, false, game);
	}
};

class ID_reader_functor {
public:
	int operator()(std::istream &in, int index, int version, bool binary) {
		ignore_unused_variable_warning(index, version);
		return binary ? Read2(in) : ReadInt(in);
	}
};

/*
 *  Post-read data transformation functors.
 */
template <class Info>
class Null_functor {
public:
	void operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(in, info, version, patch, game);
	}
};

template <int flag, class Info>
class Patch_flags_functor {
public:
	void operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(in, version, game);
		if (patch)
			info.frompatch_flags |= flag;
	}
};

/*
 *  Generic functor-based reader class for maps.
 */
template < class Info, class Functor, class Transform = Null_functor<Info>,
         class ReadID = ID_reader_functor >
class Functor_multidata_reader : public Base_reader {
protected:
	std::map<int, Info> &info;
	Functor reader;
	Transform postread;
	ReadID idread;
	void read_data(std::istream &in, size_t index, int version,
	               bool patch, Exult_Game game, bool binary) override {
		int id = idread(in, index, version, binary);
		if (id >= 0) {
			Info &inf = info[id];
			reader(in, version, patch, game, inf);
			postread(in, version, patch, game, inf);
		}
	}
public:
	Functor_multidata_reader(std::map<int, Info> &nfo, bool h = false)
		:   Base_reader(h), info(nfo)
	{  }
};

/*
 *  Generic functor-based reader class.
 */
template <class Info, class Functor, class Transform = Null_functor<Info> >
class Functor_data_reader : public Base_reader {
protected:
	Info &info;
	Functor reader;
	Transform postread;
	void read_data(std::istream &in, size_t index, int version,
	               bool patch, Exult_Game game, bool binary) override {
		ignore_unused_variable_warning(index, binary);
		reader(in, version, patch, game, info);
		postread(in, version, patch, game, info);
	}
public:
	Functor_data_reader(Info &nfo, bool h = false)
		:   Base_reader(h), info(nfo)
	{  }
};

/*
 *  Data reader functors.
 */
template <typename T, class Info, T Info::*data>
class Text_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		info.*data = ReadInt(in);
		return true;
	}
};

template <typename T, class Info, T Info::*data1, T Info::*data2>
class Text_pair_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		info.*data1 = ReadInt(in, -1);
		info.*data2 = ReadInt(in, -1);
		return true;
	}
};

template <typename T, class Info, T Info::*data, int bit>
class Bit_text_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		// For backwards compatibility.
		bool biton = ReadInt(in, 1) != 0;
		if (biton)
			info.*data |= (static_cast<T>(1) << bit);
		else
			info.*data &= ~(static_cast<T>(1) << bit);
		return true;
	}
};

template <typename T, class Info, T Info::*data>
class Bit_field_text_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		int size = 8 * sizeof(T); // Bit count.
		int bit = 0;
		T flags = 0;
		while (in.good() && bit < size) {
			if (ReadInt(in) != 0)
				flags |= (static_cast<T>(1) << bit);
			else
				flags &= ~(static_cast<T>(1) << bit);
			bit++;
		}
		info.*data = flags;
		return true;
	}
};

template <typename T, class Info, T Info::*data, unsigned pad>
class Binary_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		in.read(reinterpret_cast<char *>(&(info.*data)), sizeof(T));
		if (pad)    // Skip some bytes.
			in.ignore(pad);
		return true;
	}
};

template < typename T1, typename T2, class Info,
         T1 Info::*data1, T2 Info::*data2, unsigned pad >
class Binary_pair_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		ignore_unused_variable_warning(version, patch, game);
		in.read(reinterpret_cast<char *>(&(info.*data1)), sizeof(T1));
		in.read(reinterpret_cast<char *>(&(info.*data2)), sizeof(T2));
		if (pad)    // Skip some bytes.
			in.ignore(pad);
		return true;
	}
};

template <typename T, class Info, T *Info::*data>
class Class_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		T *cls = new T();
		cls->set_patch(patch);  // Set patch flag.
		if (!cls->read(in, version, game)) {
			delete cls;
			return false;
		}
		T *&pt = info.*data;
		if (cls->is_invalid() && pt) {
			// 'Delete old' flag.
			delete pt;
			pt = nullptr;
			delete cls;
			return false;
		}
		if (!patch)     // This is a static data file.
			info.have_static_flags |= T::get_info_flag();
		// Delete old.
		delete pt;
		pt = cls;
		return true;
	}
};

template <typename T, class Info, std::vector<T> Info::*data>
class Vector_reader_functor {
public:
	bool operator()(std::istream &in, int version, bool patch,
	                Exult_Game game, Info &info) {
		T cls;
		if (!patch)
			cls.set_static(true);
		cls.set_patch(patch);
		cls.read(in, version, game);
		std::vector<T> &vec = info.*data;
		add_vector_info(cls, vec);
		return true;
	}
};

/*
 *  Reads text data file and parses it according to passed
 *  parser functions.
 */
template <class Parser>
static void Read_text_data_file(
    const char *fname,          // Name of file to read, sans extension
    Parser *parsers[],          // What to use to parse data.
    const char *sections[],     // The names of the sections
    int numsections,            // Number of sections
    bool editing,
    Exult_Game game,
    int resource
) {
	int static_version = 1;
	int patch_version = 1;
	std::vector<std::vector<std::string> > static_strings;
	std::vector<std::vector<std::string> > patch_strings;
	char buf[50];
	if (game == BLACK_GATE || game == SERPENT_ISLE) {
		/*  ++++ Not because of ES.
		snprintf(buf, 50, "config/%s", fname);
		str_int_pair resource = game->get_resource(buf);
		U7object txtobj(resource.str, resource.num);
		*/
		bool bg = game == BLACK_GATE;
		const char *flexfile =
		    bg ? BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX)
		    : BUNDLE_CHECK(BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX);
		IExultDataSource ds(flexfile, resource);
		static_version = Read_text_msg_file_sections(&ds,
		                 static_strings, sections, numsections);
	} else {
		try {
			snprintf(buf, 50, "<STATIC>/%s.txt", fname);
			std::ifstream in;
			U7open(in, buf, false);
			static_version = Read_text_msg_file_sections(in,
			                 static_strings, sections, numsections);
			in.close();
		} catch (std::exception &e) {
			if (!editing) {
				for (int i = 0; i < numsections; i++)
					delete parsers[i];
				throw;
			}
			static_strings.resize(numsections);
		}
	}
	patch_strings.resize(numsections);
	snprintf(buf, 50, "<PATCH>/%s.txt", fname);
	if (U7exists(buf)) {
		std::ifstream in;
		U7open(in, buf, false);
		patch_version = Read_text_msg_file_sections(in, patch_strings,
		                sections, numsections);
		in.close();
	}
	for (int i = 0; i < numsections; i++) {
		parsers[i]->parse(static_strings[i], static_version, false, game);
		parsers[i]->parse(patch_strings[i], patch_version, true, game);
	}
	static_strings.clear();
	patch_strings.clear();
}

/*
 *  Generic base data-agnostic writer class.
 */
class Base_writer {
protected:
	const char *name;
	const int version;
	int cnt;
	virtual int check_write() {
		return 0;
	}
	virtual void write_data(std::ostream &out, Exult_Game game) {
		ignore_unused_variable_warning(out, game);
	}
public:
	Base_writer(const char *s, int v = -1)
		:   name(s), version(v), cnt(-1)
	{  }
	virtual ~Base_writer() = default;
	int check() {
		// Return cached value, if any.
		return cnt > -1 ? cnt : cnt = check_write();
	}
	void write_text
	(
	    std::ostream &out,
	    Exult_Game game
	) {
		if (cnt <= 0)   // Nothing to do.
			return;
		// Section is not empty.
		out << "%%section " << name << std::endl;
		write_data(out, game);
		out << "%%endsection" << std::endl;
	}
	void write_binary
	(
	    Exult_Game game
	) {
		if (cnt <= 0)   // Nothing to do.
			return;
		std::ofstream fout; // Open file.
		U7open(fout, name);
		if (version >= 0)   // container.dat has version #.
			fout.put(version);
		Write_count(fout, cnt); // Object count, with Exult extension.
		write_data(fout, game);
		fout.close();
	}
};

/*
 *  Generic functor-based writer class for maps.
 */
template <class Info, class Functor>
class Functor_multidata_writer : public Base_writer {
protected:
	std::map<int, Info> &info;
	const int numshapes;
	Functor writer;
	int check_write() override {
		int num = 0;
		for (auto& kvpair : info)
			if (writer(kvpair.second))
				num++;
		return num;
	}
	void write_data(std::ostream &out, Exult_Game game) override {
		for (auto& kvpair : info)
			if (writer(kvpair.second))
				writer(out, kvpair.first, game, kvpair.second);
	}
public:
	Functor_multidata_writer(const char *s, std::map<int, Info> &nfo,
	                         int n, int v = -1)
		:   Base_writer(s, v), info(nfo), numshapes(n) {
		check();
	}
};

/*
 *  Generic functor-based writer class.
 */
template <class Info, class Functor>
class Functor_data_writer : public Base_writer {
protected:
	Info &info;
	Functor writer;
	int check_write() override {
		return writer(info) ? 1 : 0;
	}
	void write_data(std::ostream &out, Exult_Game game) override {
		if (writer(info))
			writer(out, -1, game, info);
	}
public:
	Functor_data_writer(const char *s, Info &nfo, int v = -1)
		:   Base_writer(s, v), info(nfo) {
		check();
	}
};


/*
 *  Base flag checker functor.
 */
template <int flag, class Info>
class Flag_check_functor {
public:
	bool operator()(Info &info) {
		return (info.modified_flags | info.frompatch_flags) & flag;
	}
};

inline void WriteIndex(std::ostream &out, int index) {
	if (index >= 0)
		WriteInt(out, index);
}

/*
 *  Data checker and writer functors.
 */
template <int flag, typename T, class Info, T Info::*data>
class Text_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		out << ":";
		WriteIndex(out, index);
		WriteInt(out, info.*data, true);
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template <int flag, typename T, class Info, T Info::*data1, T Info::*data2>
class Text_pair_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		out << ":";
		WriteIndex(out, index);
		WriteInt(out, info.*data1);
		WriteInt(out, info.*data2, true);
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template <int flag, typename T, class Info, T Info::*data, int bit>
class Bit_text_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		bool val = ((info.*data) & (static_cast<T>(1) << bit));
		out << ":";
		WriteIndex(out, index);
		WriteInt(out, val, true);
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template <int flag, typename T, class Info, T Info::*data>
class Bit_field_text_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		out << ":";
		WriteIndex(out, index);
		int size = 8 * sizeof(T) - 1; // Bit count.
		int bit = 0;
		T flags = info.*data;
		while (bit < size) {
			out << static_cast<bool>((flags & (static_cast<T>(1) << bit)) != 0) << '/';
			bit++;
		}
		out << static_cast<bool>((flags & (static_cast<T>(1) << size)) != 0) << std::endl;
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template <int flag, typename T, class Info, T Info::*data, int pad>
class Binary_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		Write2(out, index);
		out.write(reinterpret_cast<char *>(&(info.*data)), sizeof(T));
		for (int i = 0; i < pad; i++)
			out.put(0);
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template < int flag, typename T1, typename T2, class Info,
         T1 Info::*data1, T2 Info::*data2, int pad >
class Binary_pair_writer_functor {
	Flag_check_functor<flag, Info> check;
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		ignore_unused_variable_warning(game);
		Write2(out, index);
		out.write(reinterpret_cast<char *>(&(info.*data1)), sizeof(T1));
		out.write(reinterpret_cast<char *>(&(info.*data2)), sizeof(T2));
		for (int i = 0; i < pad; i++)
			out.put(0);
	}
	bool operator()(Info &info) {
		return check(info);
	}
};

template <typename T, class Info, T *Info::*data>
class Class_writer_functor {
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		T *cls = info.*data;
		if (!cls) { // Write 'remove' block.
			if (!T::is_binary)  // Text entry.
				out << ':' << index << "/-255" << std::endl;
			else if (T::entry_size >= 3) {
				// T::entry_size should be >= 3!
				// For stupid compilers...
				const size_t nelems = T::entry_size >= 3 ? T::entry_size : 1;
				auto *buf = new unsigned char[nelems];
				unsigned char *ptr = buf;
				Write2(ptr, index);
				if (T::entry_size >= 4)
					memset(ptr, 0, T::entry_size - 3);
				buf[T::entry_size - 1] = 0xff;
				out.write(reinterpret_cast<const char *>(buf), T::entry_size);
				delete [] buf;
			}
		} else
			cls->write(out, index, game);
	}
	bool operator()(Info &info) {
		T *cls = info.*data;
		if (!cls)
			return (info.have_static_flags & T::get_info_flag()) != 0;
		return (cls->need_write());
	}
};

template <typename T, class Info, std::vector<T> Info::*data>
class Vector_writer_functor {
	bool need_write(T &inf) const {
		return inf.need_write() || (inf.is_invalid() && inf.have_static());
	}
public:
	void operator()(std::ostream &out, int index, Exult_Game game, Info &info) {
		std::vector<T> &vec = info.*data;
		for (auto& elem : vec)
			if (need_write(elem))
				elem.write(out, index, game);
	}
	bool operator()(Info &info) {
		std::vector<T> &vec = info.*data;
		if (vec.empty())    // Nothing to do.
			return false;
		for (auto& elem : vec)
			if (need_write(elem))
				return true;
		return false;
	}
};

/*
 *  Writes text data file according to passed writer functions.
 */
template <class Writer>
static void Write_text_data_file(
    const char *fname,      // Name of file to read, sans extension
    Writer *writers[],      // What to use to write data.
    int numsections,        // Number of sections
    int version,
    Exult_Game game
) {
	int cnt = 0;
	for (int i = 0; i < numsections; i++)
		cnt += writers[i]->check();
	if (!cnt) {
		// Nothing to do but delete the writers.
		for (int i = 0; i < numsections; i++)
			delete writers[i];
		return;
	}
	std::ofstream out;
	char buf[50];
	snprintf(buf, 50, "<PATCH>/%s.txt", fname);
	U7open(out, buf, true); // (It's a text file.)
	out << "#\tExult " << VERSION << " text message file."
	    << "\tWritten by ExultStudio." << std::endl;
	out << "%%section version" << std::endl
	    << ":" << version << std::endl
	    << "%%endsection" << std::endl;
	for (int i = 0; i < numsections; i++) {
		writers[i]->write_text(out, game);
		delete writers[i];
	}
	out.close();
}

#endif
