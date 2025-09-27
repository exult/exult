/**
 ** Items.cc - Names of items.
 **
 ** Written: 11/5/98 - JSF
 **/

/*
Copyright (C) 1998  Jeffrey S. Freedman
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

#include "items.h"

#include "databuf.h"
#include "exceptions.h"
#include "exult_flx.h"
#include "fnames.h"
#include "msgfile.h"
#include "utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::cerr;
using std::endl;
using std::ifstream;
using std::istream;
using std::ofstream;
using std::ostream;
using std::string;
using std::stringstream;
using std::vector;

// Names of U7 items.
vector<string> item_names;
// Msgs. (0x400 - in text.flx).
vector<string> text_msgs;
// Frames, etc (0x500 - 0x5ff/0x685 (BG/SI) in text.flx).
vector<string> misc_names;

static inline int remap_index(bool remap, int index, bool sibeta) {
	if (!remap) {
		return index;
	}
	if (sibeta) {
		// Account for differences between SI Beta and SI Final when remapping
		// to SS indices.
		if (index >= 0x146) {
			return index + 17;
		} else if (index >= 0x135) {
			return index + 16;
		} else if (index >= 0x0f6) {
			return index + 15;
		} else if (index >= 0x0ea) {
			return index + 14;
		} else if (index >= 0x0e5) {
			return index + 13;
		} else if (index >= 0x0b1) {
			return index + 11;
		} else if (index >= 0x0ae) {
			return index + 10;
		} else if (index >= 0x0a2) {
			return index + 9;
		} else if (index >= 0x094) {
			return index + 8;
		} else if (index >= 0x08b) {
			return index + 7;
		} else if (index >= 0x07f) {
			return index + 2;
		} else {
			return index;
		}
	} else {
		if (index >= 0x0fa) {
			return index + 11;
		} else if (index >= 0x0b2) {
			return index + 10;
		} else if (index >= 0x0af) {
			return index + 9;
		} else if (index >= 0x094) {
			return index + 8;
		} else if (index >= 0x08b) {
			return index + 7;
		} else if (index >= 0x07f) {
			return index + 2;
		} else {
			return index;
		}
	}
}

static inline const char* get_text_internal(
		const vector<string>& src, unsigned num) {
	return src[num].c_str();
}

static inline void add_text_internal(
		vector<string>& src, unsigned num, const char* name) {
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
const char* get_item_name(unsigned num) {
	return get_text_internal(item_names, num);
}

/*
 *  Create an item name.
 */
void Set_item_name(unsigned num, const char* name) {
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
const char* get_text_msg(unsigned num) {
	return get_text_internal(text_msgs, num);
}

/*
 *  Create a text message.
 */
void Set_text_msg(unsigned num, const char* msg) {
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
const char* get_misc_name(unsigned num) {
	return get_text_internal(misc_names, num);
}

/*
 *  Create a misc name.
 */
void Set_misc_name(unsigned num, const char* name) {
	add_text_internal(misc_names, num, name);
}

/*
 *  Set up names of items.
 *
 *  Msg. names start at 0x400.
 *  Frame names start at entry 0x500 (reagents,medallions,food,etc.).
 */

static void Setup_item_names(
		IDataSource& items, std::vector<File_spec>& exultmsgs, bool si,
		bool expansion, bool sibeta) {
	vector<std::optional<string>> msglist;
	int            num_item_names = 0;
	int            num_text_msgs  = 0;
	int            num_misc_names = 0;
	int            flxcnt         = 0;
	int            i;

	if (items.good()) {
		items.seek(0x54);
		num_item_names = flxcnt = items.read4();
		if (flxcnt > 0x400) {
			num_item_names = 0x400;
			num_text_msgs  = flxcnt - 0x400;
			if (flxcnt > 0x500) {
				num_text_msgs  = 0x100;
				num_misc_names = flxcnt - 0x500;
				int last_name;    // Discard all starting from this.
				if (si) {
					last_name = 0x686;
				} else {
					last_name = 0x600;
				}
				if (flxcnt > last_name) {
					num_misc_names = last_name - 0x500;
					flxcnt         = last_name;
				}
			}
		}
	}
	for (const auto& exultmsgfs : exultmsgs) {
		IExultDataSource msgs(exultmsgfs, 0);
		if (msgs.good()) {
			// Exult msgs. too?
			Text_msg_file_reader reader(msgs);
			int first_msg = reader.get_global_section_strings(msglist);
			if (first_msg >= msg_file_start) {
				first_msg -= msg_file_start;
				if (first_msg < num_text_msgs) {
					cerr << "Exult msg. # " << first_msg
						 << " conflicts with 'text.flx'" << endl;
					first_msg = num_text_msgs;
				}
				const size_t total_msgs = msglist.size() - msg_file_start;
				text_msgs.resize(std::max(total_msgs, text_msgs.size()));
				for (unsigned i = first_msg; i < total_msgs; i++) {
					std::optional<string>& msg = msglist[i + msg_file_start];
					if (msg) {
						text_msgs[i] = msg.value();
					}
				}
			} else {
				first_msg = num_text_msgs;
			}
		}
	}
	item_names.resize(num_item_names);
	text_msgs.resize(std::max<size_t>(num_text_msgs, text_msgs.size()));
	misc_names.resize(num_misc_names);
	// Hack alert: move SI misc_names around to match those of SS.
	if (flxcnt) {
		const bool doremap = si && (!expansion || sibeta);
		if (doremap) {
			flxcnt -= 17;    // Just to be safe.
		}
		for (i = 0; i < flxcnt; i++) {
			items.seek(0x80 + (i * 8));
			const int itemoffs = items.read4();
			if (itemoffs == 0) {
				continue;
			}
			const int itemlen = items.read4();
			items.seek(itemoffs);
			string newitem;
			items.read(newitem, itemlen);
			if (i < num_item_names) {
				item_names[i] = std::move(newitem);
			} else if (i - num_item_names < num_text_msgs) {
				if (sibeta && (i - num_item_names) >= 0xd2) {
					text_msgs[i - num_item_names + 1] = std::move(newitem);
				} else {
					text_msgs[i - num_item_names] = std::move(newitem);
				}
			} else {
				const size_t new_index = remap_index(
						doremap, i - num_item_names - num_text_msgs, sibeta);
				misc_names[new_index] = std::move(newitem);
			}
		}
	}
}

#define SHAPES_SECT "shapes"
#define MSGS_SECT   "msgs"
#define MISC_SECT   "miscnames"

/*
 *  This sets up item names and messages from Exult's new file,
 *  "textmsgs.txt".
 */

static void Setup_text(
		IDataSource&            txtfile,    // All text.
		std::vector<File_spec>& exultmsgs) {
	// Start by reading from exultmsg
	for (auto exultmsgfs : exultmsgs) {
		vector<std::optional<string>> msglist;
		int              first_msg = 0;
		IExultDataSource exultmsg(exultmsgfs.name, exultmsgfs.index);
		if (exultmsg.good()) {
			{
				Text_msg_file_reader reader(exultmsg);
				first_msg = reader.get_global_section_strings(msglist);
			}

			//
			if (first_msg >= msg_file_start) {
				first_msg -= msg_file_start;
				const size_t total_msgs = msglist.size() - msg_file_start;
				text_msgs.resize(std::max(total_msgs, text_msgs.size()));
				for (unsigned i = first_msg; i < total_msgs; i++) {
					std::optional<string> &msg = msglist[i + msg_file_start];
					if (msg) {
						text_msgs[i] = msg.value();
					}
				}
			}
		}
	}
	// If no text mesages were loaded retry with the default exult ones
	if (text_msgs.empty()) {
		throw exult_exception(
				"Failed to load any messages from exultmsg", __FILE__,
				__LINE__);
	}

	// Now read in textmsg.txt
	if (txtfile.good()) {
		Text_msg_file_reader reader(txtfile);
		reader.get_section_strings(SHAPES_SECT, item_names);
		reader.get_section_strings(MSGS_SECT, text_msgs);
		reader.get_section_strings(MISC_SECT, misc_names);
	}
}

/*
 *  Setup item names and text messages.
 */

void Setup_text(bool si, bool expansion, bool sibeta, Game_Language language) {
	Free_text();
	const bool             is_patch = is_system_path_defined("<PATCH>");
	std::vector<File_spec> exultmsgs;
	exultmsgs.reserve(4);
	// Always read exultmsg.txt from exult.flx
	exultmsgs.push_back(File_spec(
			BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_EXULTMSG_TXT));
	// Then load the language specific exultmsg from exult.flx
	int         exultflx_msg_lang_index = -1;
	const char* patch_exultmsg_lang     = nullptr;
	switch (language) {
	case Game_Language::FRENCH:
		exultflx_msg_lang_index = EXULT_FLX_EXULTMSG_FR_TXT;
		patch_exultmsg_lang     = PATCH_EXULTMSG_FR;
		break;
	case Game_Language::GERMAN:
		exultflx_msg_lang_index = EXULT_FLX_EXULTMSG_DE_TXT;
		patch_exultmsg_lang     = PATCH_EXULTMSG_DE;
		break;
	case Game_Language::SPANISH:
		exultflx_msg_lang_index = EXULT_FLX_EXULTMSG_ES_TXT;
		patch_exultmsg_lang     = PATCH_EXULTMSG_ES;
		break;
		// English messages are always loaded so do not need to explicitly load
		// them here
	case Game_Language::ENGLISH:
	default:
		break;
	}
	// Then load the language specific exultmsg from exult.flx
	if (exultflx_msg_lang_index != -1) {
		exultmsgs.push_back(File_spec(
				BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX),
				exultflx_msg_lang_index));
	}

	if (is_patch) {
		// Then load the patch exultmsg then  finally load load the
		exultmsgs.push_back(File_spec(PATCH_EXULTMSG, 0));
		// finally load load the language specific patch
		if (patch_exultmsg_lang) {
			exultmsgs.push_back(File_spec(patch_exultmsg_lang, 0));
		}
	}

	// Exult new-style messages?
	if (is_patch && U7exists(PATCH_TEXTMSGS)) {
		IFileDataSource txtfile(PATCH_TEXTMSGS, true);
		if (!txtfile.good()) {
			return;
		}
		Setup_text(txtfile, exultmsgs);
	} else if (U7exists(TEXTMSGS)) {
		IFileDataSource txtfile(TEXTMSGS, true);
		if (!txtfile.good()) {
			return;
		}
		Setup_text(txtfile, exultmsgs);
	} else {
		IFileDataSource textflx = [&]() {
			if (is_patch && U7exists(PATCH_TEXT)) {
				return IFileDataSource(PATCH_TEXT);
			}
			return IFileDataSource(TEXT_FLX);
		}();

		Setup_item_names(textflx, exultmsgs, si, expansion, sibeta);
	}
}

/*
 *  Free memory.
 */

static void Free_text_list(vector<string>& items) {
	items.clear();
}

void Free_text() {
	Free_text_list(item_names);
	Free_text_list(text_msgs);
	Free_text_list(misc_names);
}

/*
 *  Write out new-style Exult text file.
 */

void Write_text_file() {
	auto pOut = U7open_out(PATCH_TEXTMSGS, true);    // (It's a text file.)
	if (!pOut) {
		return;
	}
	auto& out = *pOut;
	out << "#\tExult " << VERSION << " text message file."
		<< "\tWritten by ExultStudio." << std::endl;
	Write_msg_file_section(out, SHAPES_SECT, item_names);
	Write_msg_file_section(out, MSGS_SECT, text_msgs);
	Write_msg_file_section(out, MISC_SECT, misc_names);
}
