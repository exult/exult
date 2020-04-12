/**
 ** Items.cc - Names of items.
 **
 ** Written: 11/5/98 - JSF
 **/

/*
Copyright (C) 1998  Jeffrey S. Freedman

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

// #include <iomanip>           /* Debugging */
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "items.h"
#include "utils.h"
#include "msgfile.h"
#include "fnames.h"
#include "exult_flx.h"
#include "data_utils.h"

using std::istream;
using std::ostream;
using std::ifstream;
using std::ofstream;
using std::stringstream;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

vector<string> item_names;          // Names of U7 items.
vector<string> text_msgs;           // Msgs. (0x400 - in text.flx).
vector<string> misc_names;          // Frames, etc (0x500 - 0x5ff/0x685 (BG/SI) in text.flx).

static inline int remap_index(bool remap, int index, bool sibeta) {
	if (!remap)
		return index;
	if (sibeta) {
		// Account for differences between SI Beta and SI Final when remapping
		// to SS indices.
		if (index >= 0x146)
			return index + 17;
		else if (index >= 0x135)
			return index + 16;
		else if (index >= 0x0f6)
			return index + 15;
		else if (index >= 0x0ea)
			return index + 14;
		else if (index >= 0x0e5)
			return index + 13;
		else if (index >= 0x0b1)
			return index + 11;
		else if (index >= 0x0ae)
			return index + 10;
		else if (index >= 0x0a2)
			return index + 9;
		else if (index >= 0x094)
			return index + 8;
		else if (index >= 0x08b)
			return index + 7;
		else if (index >= 0x07f)
			return index + 2;
		else
			return index;
	} else {
		if (index >= 0x0fa)
			return index + 11;
		else if (index >= 0x0b2)
			return index + 10;
		else if (index >= 0x0af)
			return index + 9;
		else if (index >= 0x094)
			return index + 8;
		else if (index >= 0x08b)
			return index + 7;
		else if (index >= 0x07f)
			return index + 2;
		else
			return index;
	}
}

static inline char const *get_text_internal(vector<string> const &src, unsigned num) {
	return src[num].c_str();
}

static inline void add_text_internal(vector<string> &src, unsigned num, char const *name) {
	if (num >= src.size()) {
		src.resize(num + 1);
	}
	src[num] = name;
}

/*
 *  Get how many item names there are.
 */

int get_num_item_names() {
	return item_names.size();
}

/*
 *  Get an item name.
 */
char const *get_item_name(unsigned num) {
	return get_text_internal(item_names, num);
}

/*
 *  Create an item name.
 */
void Set_item_name(unsigned num, char const *name) {
	add_text_internal(item_names, num, name);
}

/*
 *  Get how many text messages there are.
 */

int get_num_text_msgs() {
	return text_msgs.size();
}

/*
 *  Get a text message.
 */
char const *get_text_msg(unsigned num) {
	return get_text_internal(text_msgs, num);
}

/*
 *  Create a text message.
 */
void Set_text_msg(unsigned num, char const *msg) {
	add_text_internal(text_msgs, num, msg);
}

/*
 *  Get how many misc names there are.
 */

int get_num_misc_names() {
	return misc_names.size();
}

/*
 *  Get a misc name.
 */
char const *get_misc_name(unsigned num) {
	return get_text_internal(misc_names, num);
}

/*
 *  Create a misc name.
 */
void Set_misc_name(unsigned num, char const *name) {
	add_text_internal(misc_names, num, name);
}

/*
 *  Set up names of items.
 *
 *  Msg. names start at 0x400.
 *  Frame names start at entry 0x500 (reagents,medallions,food,etc.).
 */

static void Setup_item_names(
    istream &items,
    istream &msgs,
    bool si,
    bool expansion,
    bool sibeta
) {
	vector<string> msglist;
	int first_msg;          // First in exultmsg.txt.  Should
	//   follow those in text.flx.
	int total_msgs = 0;
	int num_item_names = 0;
	int num_text_msgs = 0;
	int num_misc_names = 0;

	items.seekg(0x54);
	int flxcnt = Read4(items);
	first_msg = num_item_names = flxcnt;
	if (flxcnt > 0x400) {
		num_item_names = 0x400;
		num_text_msgs = flxcnt - 0x400;
		if (flxcnt > 0x500) {
			num_text_msgs = 0x100;
			num_misc_names = flxcnt - 0x500;
			int last_name; // Discard all starting from this.
			if (si)
				last_name = 0x686;
			else
				last_name = 0x600;
			if (flxcnt > last_name) {
				num_misc_names = last_name - 0x500;
				flxcnt = last_name;
			}
		}
		total_msgs = num_text_msgs;
	}
	if (msgs.good()) {
		// Exult msgs. too?
		first_msg = Read_text_msg_file(msgs, msglist);
		if (first_msg >= 0) {
			first_msg -= 0x400;
			if (first_msg < num_text_msgs) {
				cerr << "Exult msg. # " << first_msg <<
				     " conflicts with 'text.flx'" << endl;
				first_msg = num_text_msgs;
			}
			total_msgs = static_cast<int>(msglist.size() - 0x400);
		} else
			first_msg = num_text_msgs;
	}
	item_names.resize(num_item_names);
	text_msgs.resize(total_msgs);
	misc_names.resize(num_misc_names);
	// Hack alert: move SI misc_names around to match those of SS.
	bool doremap = si && (!expansion || sibeta);
	if (doremap)
		flxcnt -= 17;   // Just to be safe.
	int i;
	for (i = 0; i < flxcnt; i++) {
		items.seekg(0x80 + i * 8);
		int itemoffs = Read4(items);
		if (!itemoffs)
			continue;
		int itemlen = Read4(items);
		items.seekg(itemoffs);
		char *newitem = new char[itemlen];
		items.read(newitem, itemlen);
		if (i < num_item_names)
			item_names[i] = newitem;
		else if (i - num_item_names < num_text_msgs) {
			if (sibeta && (i - num_item_names) >= 0xd2)
				text_msgs[i - num_item_names + 1] = newitem;
			else
				text_msgs[i - num_item_names] = newitem;
		} else
			misc_names[remap_index(doremap,
			                       i - num_item_names - num_text_msgs,
			                       sibeta)] = newitem;
		delete [] newitem;
	}
	for (i = first_msg; i < total_msgs; i++)
		text_msgs[i] = msglist[i + 0x400];
}

#define SHAPES_SECT  "shapes"
#define MSGS_SECT    "msgs"
#define MISC_SECT    "miscnames"

/*
 *  This sets up item names and messages from Exult's new file,
 *  "textmsgs.txt".
 */

static void Setup_text(
    istream &txtfile,       // All text.
    istream &exultmsg
) {
	// Start by reading from exultmsg
	vector<string> msglist;
	int first_msg;
	first_msg = Read_text_msg_file(exultmsg, msglist);
	unsigned total_msgs = static_cast<int>(msglist.size() - 0x400);
	if (first_msg >= 0) {
		first_msg -= 0x400;
	}
	text_msgs.resize(total_msgs);
	for (unsigned i = first_msg; i < total_msgs; i++)
		text_msgs[i] = msglist[i + 0x400];
	// Now read in textmsg.txt
	Read_text_msg_file(txtfile, item_names, SHAPES_SECT);
	Read_text_msg_file(txtfile, text_msgs, MSGS_SECT);
	Read_text_msg_file(txtfile, misc_names, MISC_SECT);
}

/*
 *  Setup item names and text messages.
 */

void Setup_text(bool si, bool expansion, bool sibeta) {
	Free_text();
	bool is_patch = is_system_path_defined("<PATCH>");
	// Always read from exultmsg.txt
	// TODO: allow multilingual exultmsg.txt files.
	istream *exultmsg;
	if (is_patch && U7exists(PATCH_EXULTMSG)) {
		ifstream *exultmsgfile = new ifstream();
		exultmsg = exultmsgfile;
		U7open(*exultmsgfile, PATCH_EXULTMSG, true);
	} else {
		stringstream *exultmsgbuf = new stringstream();
		exultmsg = exultmsgbuf;
		const char *msgs = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
		U7object txtobj(msgs, EXULT_FLX_EXULTMSG_TXT);
		size_t len;
		auto txt = txtobj.retrieve(len);
		if (txt && len > 0) {
			exultmsgbuf->str(string(reinterpret_cast<char*>(txt.get()), len));
		}
	}

	// Exult new-style messages?
	if (is_patch && U7exists(PATCH_TEXTMSGS)) {
		ifstream txtfile;
		U7open(txtfile, PATCH_TEXTMSGS, true);
		Setup_text(txtfile, *exultmsg);
	} else if (U7exists(TEXTMSGS)) {
		ifstream txtfile;
		U7open(txtfile, TEXTMSGS, true);
		Setup_text(txtfile, *exultmsg);
	} else {
		ifstream textflx;
		if (is_patch && U7exists(PATCH_TEXT))
			U7open(textflx, PATCH_TEXT);
		else
			U7open(textflx, TEXT_FLX);
		Setup_item_names(textflx, *exultmsg, si, expansion, sibeta);
	}
	delete exultmsg;
}

/*
 *  Free memory.
 */

static void Free_text_list(
    vector<string> &items
) {
	items.clear();
}

void Free_text(
) {
	Free_text_list(item_names);
	Free_text_list(text_msgs);
	Free_text_list(misc_names);
}

/*
 *  Write out new-style Exult text file.
 */

void Write_text_file(
) {
	ofstream out;

	U7open(out, PATCH_TEXTMSGS, true);  // (It's a text file.)
	out << "Exult " << VERSION << " text message file." <<
	    "  Written by ExultStudio." << endl;
	Write_msg_file_section(out, SHAPES_SECT, item_names);
	Write_msg_file_section(out, MSGS_SECT, text_msgs);
	Write_msg_file_section(out, MISC_SECT, misc_names);
	out.close();
}
