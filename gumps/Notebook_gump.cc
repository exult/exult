/*
Copyright (C) 2000-2022 The Exult Team

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

#include "Notebook_gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "exult_flx.h"
#include "game.h"
#include "gamewin.h"
#include "gameclk.h"
#include "actors.h"
#include "SDL_events.h"
#include "Configuration.h"
#include "msgfile.h"
#include "fnames.h"
#include "cheat.h"
#include "U7file.h"
#include "Gump_manager.h"
#include "exult.h"
#include "touchui.h"

using std::ofstream;
using std::ifstream;
using std::ostream;
using std::endl;
using std::string;
using std::cout;
using std::vector;

vector<One_note *> Notebook_gump::notes;
bool Notebook_gump::initialized = false;    // Set when read in.
bool Notebook_gump::initialized_auto_text = false;
Notebook_gump *Notebook_gump::instance = nullptr;
vector<Notebook_top> Notebook_gump::page_info;
vector<string> Notebook_gump::auto_text;

/*
 *  Defines in 'gumps.vga':
 */
#define LEFTPAGE  (GAME_BG ? 44 : 39)   // At top-left of left page.
#define RIGHTPAGE  (GAME_BG ? 45 : 40)  // At top-right of right page.

const int font = 4;         // Small black.
const int vlead = 1;            // Extra inter-line spacing.
#if defined(__IPHONEOS__) || defined(ANDROID)
const int pagey = 7;           // Top of text area of page.
#else
const int pagey = 10;           // Top of text area of page.
#endif
const int lpagex = 36, rpagex = 174;    // X-coord. of text area of page.

class One_note {
	int day = 0, hour = 0, minute = 0;      // Game time when note was written.
	int tx = 0, ty = 0;         // Tile coord. where written.
	string text;                // Text, 0-delimited.
	int gflag = -1;             // >=0 if created automatically when
	//   the global flag was set.
	bool is_new = false;        // Newly created at cur. time/place.
public:
	friend class Notebook_gump;
	One_note() = default;
	void set_time(int d, int h, int m) {
		day = d;
		hour = h;
		minute = m;
	}
	void set_loc(int x, int y) {
		tx = x;
		ty = y;
	}
	void set_text(const string &txt) {
		text = txt;
	}
	void set_gflag(int gf) {
		gflag = gf;
	}
	void set(int d, int h, int m, int x, int y, const string &txt, int gf = -1, bool isnew = false) {
		set_time(d, h, m);
		set_loc(x, y);
		set_text(txt);
		gflag = gf;
		is_new = isnew;
	}
	One_note(int d, int h, int m, int x, int y, const string &txt = "", int gf = -1, bool isnew = false) {
		set(d, h, m, x, y, txt, gf, isnew);
	}
	// Insert text.
	void insert(int chr, int offset) {
		text.insert(offset, 1, chr);
	}
	// Delete text.
	bool del(int offset) {
		if (offset < 0 || static_cast<size_t>(offset) >= text.length()) {
			return false;
		} else {
			text.erase(offset, 1);
			return true;
		}
	}
	void write(ostream &out);       // Write out as XML.
};

/*
 *  Write out as XML.
 */

void One_note::write(
    ostream &out
) {
	out << "<note>" << endl;
	out << "<time> " << day << ':' << hour << ':' << minute <<
	    " </time>" << endl;
	out << "<place> " << tx << ':' << ty << " </place>" << endl;
	if (gflag >= 0)
		out << "<gflag> " << gflag << " </gflag>" << endl;
	out << "<text>" << endl;
	// Encode entities (prevents crashes on load with ampersands).
	out << encode_entity(text);
	out << endl << "</text>" << endl;
	out << "</note>" << endl;
}

/*
 *  Get left/right text area.
 */

inline TileRect Get_text_area(bool right, bool startnote) {
	const int ninf = 12;        // Space for note info.
	if (!startnote)
		return right ? TileRect(rpagex, pagey, 122, 130)
		       : TileRect(lpagex, pagey, 122, 130);
	else
		return right ? TileRect(rpagex, pagey + ninf, 122, 130 - ninf)
		       : TileRect(lpagex, pagey + ninf, 122, 130 - ninf);
}

/*
 *  A 'page-turner' button.
 */
class Notebook_page_button : public Gump_button {
	int leftright;          // 0=left, 1=right.
public:
	Notebook_page_button(Gump *par, int px, int py, int lr)
		: Gump_button(par, lr ? RIGHTPAGE : LEFTPAGE, px, py),
		  leftright(lr)
	{  }
	// What to do when 'clicked':
	bool activate(int button = 1) override;
	bool push(int button) override {
		ignore_unused_variable_warning(button);
		return false;
	}
	void unpush(int button) override {
		ignore_unused_variable_warning(button);
	}
};

/*
 *  Handle click.
 */

bool Notebook_page_button::activate(
    int button
) {
	if (button != 1) return false;
	static_cast<Notebook_gump *>(parent)->change_page(leftright ? 1 : -1);
	return true;
}

/*
 *  Read in notes the first time.
 */

void Notebook_gump::initialize(
) {
	initialized = true;
	read();
}

/*
 *  Clear out.
 */

void Notebook_gump::clear(
) {
	while (!notes.empty()) {
		One_note *note = notes.back();
		delete note;
		notes.pop_back();
	}
	page_info.clear();
	initialized = false;
}


/*
 *  Add a new note at the current time/place.
 */

void Notebook_gump::add_new(
    const string &text,
    int gflag
) {
	Game_clock *clk = gwin->get_clock();
	Tile_coord t = gwin->get_main_actor()->get_tile();
	auto *note = new One_note(clk->get_day(), clk->get_hour(),
	                              clk->get_minute(), t.tx, t.ty, text, gflag);
	note->is_new = true;
	notes.push_back(note);
}

/*
 *  Create notebook gump.
 */

Notebook_gump::Notebook_gump(
) : Gump(nullptr,
	EXULT_FLX_NOTEBOOK_SHP, SF_EXULT_FLX) {
#if defined(__IPHONEOS__) || defined(ANDROID)
	//on iOS the Notebook gump needs to be aligned with the top
	set_pos(5, -2);
#endif
	handles_kbd = true;
	cursor.offset = 0;
	cursor.x = cursor.y = -1;
	cursor.line = cursor.nlines = 0;
	// (Obj. area doesn't matter.)
	set_object_area(TileRect(36, 10, 100, 100), 7, 40);
	if (page_info.empty())
		page_info.emplace_back(0, 0);
	// Where to paint page marks:
	const int lpagex = 35;
	const int rpagex = 298;
	const int lrpagey = 12;
	leftpage = new Notebook_page_button(this, lpagex, lrpagey, 0);
	rightpage = new Notebook_page_button(this, rpagex, lrpagey, 1);
	add_new("");          // Add new note to end.
}

Notebook_gump *Notebook_gump::create(
) {
	if (!initialized)
		initialize();
	if (!instance) {
		instance = new Notebook_gump;
		if (touchui != nullptr) {
			touchui->hideGameControls();
			if (!SDL_IsTextInputActive())
				SDL_StartTextInput();
		}
	}
	return instance;
}

/*
 *  Cleanup.
 */
Notebook_gump::~Notebook_gump(
) {
	if (!notes.empty()) {
		// Check for empty 'new' note.
		One_note *note = notes.back();
		if (note->is_new && !note->text.length()) {
			notes.pop_back();
			delete note;
		} else
			note->is_new = false;
	}
	delete leftpage;
	delete rightpage;
	if (this == instance)
		instance = nullptr;
	if (touchui != nullptr) {
		Gump_manager *gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode())
			touchui->showGameControls();
		if (SDL_IsTextInputActive())
			SDL_StopTextInput();
	}
}

/*
 *  Paint a page and find where its text ends.
 *
 *  Output: True if finished entire note.
 *      Cursor.x is possibly set.
 */

bool Notebook_gump::paint_page(
    TileRect const &box,           // Display box rel. to gump.
    One_note *note,         // Note to print.
    int &offset,            // Starting offset into text.  Updated.
    int pagenum
) {
	bool find_cursor = (note == notes[curnote] && cursor.x < 0);
	if (offset == 0) {      // Print note info. at start.
		char buf[60];
		const char *ampm = "am";
		int h = note->hour;
		if (h >= 12) {
			h -= 12;
			ampm = "pm";
		}
		snprintf(buf, sizeof(buf), "Day %d, %02d:%02d%s",
		         note->day, h ? h : 12, note->minute, ampm);
#if defined(__IPHONEOS__) || defined(ANDROID)
		const int fontnum = 4;
		const int yoffset = 0;
#else
		const int fontnum = 2;
		const int yoffset = 4;
#endif
		sman->paint_text(fontnum, buf, x + box.x, y + pagey);
		//when cheating show location of entry (in dec - could use sextant postions)
		if (cheat()) {
			snprintf(buf, sizeof(buf), "%d, %d", note->tx, note->ty);
			sman->paint_text(4, buf, x + box.x + 80, y + pagey - yoffset);
		}
		// Use bright green for automatic text.
		gwin->get_win()->fill8(sman->get_special_pixel(
		                           note->gflag >= 0 ? POISON_PIXEL : CHARMED_PIXEL),
		                       box.w, 1, x + box.x, y + box.y - 3);
	}
	const char *str = note->text.c_str() + offset;
	cursor.offset -= offset;
	int endoff = sman->paint_text_box(font, str, x + box.x,
	                                  y + box.y, box.w, box.h, vlead,
	                                  false, false, -1, find_cursor ? &cursor : nullptr);
	cursor.offset += offset;
	if (endoff > 0) {       // All painted?
		// Value returned is height.
		str = note->text.c_str() + note->text.length();
	} else              // Out of room.
		str += -endoff;
	if (find_cursor && cursor.x >= 0) {
		gwin->get_win()->fill8(sman->get_special_pixel(POISON_PIXEL),
		                       1,
		                       sman->get_text_height(font), cursor.x - 1, cursor.y - 1);
		curpage = pagenum;
	}
	offset = str - note->text.c_str();  // Return offset past end.
	// Watch for exactly filling page.
	return endoff > 0 && endoff < box.h;
}

/*
 *  Change page.
 */

void Notebook_gump::change_page(
    int delta
) {
	int topleft = curpage & ~1;
	if (delta > 0) {
		int nxt = topleft + 2;
		if (nxt >= static_cast<int>(page_info.size()))
			return;
		curpage = nxt;
		curnote = page_info[curpage].notenum;
		updnx = cursor.offset = 0;
	} else if (delta < 0) {
		if (topleft == 0)
			return;
		curpage = topleft - 2;
		curnote = page_info[curpage].notenum;
		updnx = cursor.offset = 0;
	}
	paint();
}

/*
 *  Is a given screen point on one of our buttons?  If not, we try to
 *  set cursor pos. within the text.
 *
 *  Output: ->button if so.
 */

Gump_button *Notebook_gump::on_button(
    int mx, int my          // Point in window.
) {
	Gump_button *btn = Gump::on_button(mx, my);
	if (btn)
		return btn;
	else if (leftpage->on_button(mx, my))
		return leftpage;
	else if (rightpage->on_button(mx, my))
		return rightpage;
	int topleft = curpage & ~1;
	int notenum = page_info[topleft].notenum;
	if (notenum < 0)
		return nullptr;
	int offset = page_info[topleft].offset;
	TileRect box = Get_text_area(false, offset == 0);  // Left page.
	One_note *note = notes[notenum];
	int coff = sman->find_cursor(font, note->text.c_str() + offset, x + box.x,
	                             y + box.y, box.w, box.h, mx, my, vlead);
	if (coff >= 0) {        // Found it?
		curpage = topleft;
		curnote = page_info[curpage].notenum;
		cursor.offset = offset + coff;
		paint();
		updnx = cursor.x - x - lpagex;
		if (!SDL_IsTextInputActive())
			SDL_StartTextInput();
	} else {
		offset += -coff;        // New offset.
		if (offset >= static_cast<int>(note->text.length())) {
			if (notenum == static_cast<int>(notes.size()) - 1)
				return nullptr;   // No more.
			note = notes[++notenum];
			offset = 0;
		}
		box = Get_text_area(true, offset == 0); // Right page.
		box.shift(x, y);        // Window area.
		coff = box.has_point(mx, my) ?
		       sman->find_cursor(font, note->text.c_str() + offset, box.x,
		                         box.y, box.w, box.h, mx, my, vlead)
		       : -1;
		if (coff >= 0) {        // Found it?
			curpage = curpage | 1;
			curnote = page_info[curpage].notenum;
			cursor.offset = offset + coff;
			paint();
			updnx = cursor.x - x - rpagex;
			if (!SDL_IsTextInputActive())
				SDL_StartTextInput();
		}
	}
	return nullptr;
}

/*
 *  Paint notebook.
 */

void Notebook_gump::paint(
) {
	Gump::paint();
	if (curpage > 0)        // Not the first?
		leftpage->paint();
	int topleft = curpage & ~1;
	int notenum = page_info[topleft].notenum;
	if (notenum < 0)
		return;
	int offset = page_info[topleft].offset;
	One_note *note = notes[notenum];
	cursor.x = -1;
	// Paint left page.
	if (paint_page(Get_text_area(false, offset == 0),
	               note, offset, topleft)) {
		// Finished note?
		if (notenum == static_cast<int>(notes.size()) - 1)
			return;
		++notenum;
		note = notes[notenum];
		offset = 0;
	}
	if (topleft + 1 >= static_cast<int>(page_info.size()))  // Store right-page info.
		page_info.resize(topleft + 2);
	page_info[topleft + 1].notenum = notenum;
	page_info[topleft + 1].offset = offset;
	// Paint right page.
	if (paint_page(Get_text_area(true, offset == 0),
	               note, offset, topleft + 1)) {
		// Finished note?
		if (notenum == static_cast<int>(notes.size()) - 1)
			return;     // No more.
		++notenum;
		offset = 0;
	}
	rightpage->paint();
	int nxt = topleft + 2;      // For next pair of pages.
	if (nxt >= static_cast<int>(page_info.size()))
		page_info.resize(nxt + 1);
	page_info[nxt].notenum = notenum;
	page_info[nxt].offset = offset;
}

/*
 *  Move to end of prev. page.
 */

void Notebook_gump::prev_page(
) {
	if (!curpage)
		return;
	Notebook_top &pinfo = page_info[curpage];
	--curpage;
	curnote = page_info[curpage].notenum;
	if (!pinfo.offset)      // Going to new note?
		cursor.offset = notes[curnote]->text.length();
	else
		cursor.offset = pinfo.offset - 1;
}

/*
 *  Move to next page.
 */

void Notebook_gump::next_page(
) {
	if (curpage >= static_cast<int>(page_info.size()))
		return;
	++curpage;
	Notebook_top &pinfo = page_info[curpage];
	curnote = pinfo.notenum;
	cursor.offset = pinfo.offset;   // Start of page.
}

/*
 *  See if on last/first line of current page.
 *  Note:   These assume paint() was done, so cursor is correct.
 */

bool Notebook_gump::on_last_page_line(
) {
	return cursor.line == cursor.nlines - 1;
}

bool Notebook_gump::on_first_page_line(
) {
	return cursor.line == 0;
}

/*
 *  Handle down/up arrows.
 */

void Notebook_gump::down_arrow(
) {
	int offset = page_info[curpage].offset;
	TileRect box = Get_text_area((curpage % 2) != 0, offset == 0);
	int ht = sman->get_text_height(font);
	if (on_last_page_line()) {
		if (curpage >= static_cast<int>(page_info.size()) - 1)
			return;
		next_page();
		paint();
		offset = page_info[curpage].offset;
		box = Get_text_area((curpage % 2) != 0, offset == 0);
		cursor.y = y + box.y - ht;
	}
	box.shift(x, y);        // Window coords.
	int mx = box.x + updnx + 1;
	int my = cursor.y + ht + ht / 2;
	int notenum = page_info[curpage].notenum;
	One_note *note = notes[notenum];
	int coff = sman->find_cursor(font, note->text.c_str() + offset, box.x,
	                             box.y, box.w, box.h, mx, my, vlead);
	if (coff >= 0) {        // Found it?
		cursor.offset = offset + coff;
		paint();
	}
}

void Notebook_gump::up_arrow(
) {
	Notebook_top &pinfo = page_info[curpage];
	int ht = sman->get_text_height(font);
	int offset = pinfo.offset;
	int notenum = pinfo.notenum;
	if (on_first_page_line()) { // Above top.
		if (!curpage)
			return;
		prev_page();
		Notebook_top &pinfo2 = page_info[curpage];
		notenum = pinfo2.notenum;
		if (pinfo.notenum == notenum)   // Same note?
			cursor.offset = offset - 1;
		else
			cursor.offset = notes[notenum]->text.length();
		paint();
		offset = pinfo2.offset;
		cursor.y += ht / 2;     // Past bottom line.
	}
	TileRect box = Get_text_area((curpage % 2) != 0, offset == 0);
	box.shift(x, y);        // Window coords.
	int mx = box.x + updnx + 1;
	int my = cursor.y - ht / 2;
	One_note *note = notes[notenum];
	int coff = sman->find_cursor(font, note->text.c_str() + offset, box.x,
	                             box.y, box.w, box.h, mx, my, vlead);
	if (coff >= 0) {        // Found it?
		cursor.offset = offset + coff;
		paint();
	}
}

/*
 *  Handle keystroke.
 */
bool Notebook_gump::handle_kbd_event(
    void *vev
) {
	SDL_Event &ev = *static_cast<SDL_Event *>(vev);
	uint16 unicode = 0; // Unicode is way different in SDL2
	Gump_manager::translate_numpad(ev.key.keysym.sym, unicode, ev.key.keysym.mod);
	int chr = ev.key.keysym.sym;

	if (ev.type == SDL_KEYUP)
		return true;        // Ignoring key-up at present.
	if (ev.type != SDL_KEYDOWN)
		return false;
	if (curpage >= static_cast<int>(page_info.size()))
		return false;       // Shouldn't happen.
	Notebook_top &pinfo = page_info[curpage];
	One_note *note = notes[pinfo.notenum];
	switch (chr) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		note->insert('\n', cursor.offset);
		++cursor.offset;
		paint();        // (Not very efficient...)
		if (need_next_page()) {
			next_page();
			paint();
		}
		break;
	case SDLK_BACKSPACE:
		if (note->del(cursor.offset - 1)) {
			if (--cursor.offset < pinfo.offset && curpage % 2 == 0)
				prev_page();
			paint();
		}
		break;
	case SDLK_DELETE:
		if (note->del(cursor.offset))
			paint();
		break;
	case SDLK_LEFT:
		if (cursor.offset) {
			if (--cursor.offset < pinfo.offset && curpage % 2 == 0)
				prev_page();
			paint();
		}
		break;
	case SDLK_RIGHT:
		if (cursor.offset < static_cast<int>(note->text.length())) {
			++cursor.offset;
			paint();
			if (need_next_page()) {
				next_page();
				paint();
			}
		}
		break;
	case SDLK_UP:
		up_arrow();
		return true;            // Don't set updnx.
	case SDLK_DOWN:
		down_arrow();
		return true;            // Don't set updnx.
	case SDLK_HOME:
	case SDLK_END:
		// ++++++Finish.
		break;
	case SDLK_PAGEUP:
		change_page(-1);
		break;
	case SDLK_PAGEDOWN:
		change_page(1);
		break;
	default:
		if (chr < ' ')
			return false;       // Ignore other special chars.
		if (chr >= 256 || !isascii(chr))
			return false;
		if (ev.key.keysym.mod & (KMOD_SHIFT | KMOD_CAPS))
			chr = toupper(chr);
		note->insert(chr, cursor.offset);
		++cursor.offset;
		paint();        // (Not very efficient...)
		if (need_next_page()) {
			next_page();
			paint();
		}
		break;
	}
	updnx = cursor.x - x - ((curpage % 2) ? rpagex : lpagex);
#ifdef DEBUG
	std::cout << "updnx = " << updnx << std::endl;
//	std::cout << "Notebook chr: " << chr << std::endl;
#endif
	return true;
}

/*
 *  Automatically add a text entry when a usecode global flag is set.
 */

void Notebook_gump::add_gflag_text(
    int gflag,
    const string &text
) {
	if (!initialized)
		initialize();
	// See if already there.
	for (auto *note : notes)
		if (note->gflag == gflag)
			return;
	if (gwin->get_allow_autonotes())
		add_new(text, gflag);
}

/*
 *  Write it out.
 */

void Notebook_gump::write(
) {
	auto pOut = U7open_out(NOTEBOOKXML);
	if (!pOut)
		return;
	auto& out = *pOut;
	out << "<notebook>" << endl;
	if (initialized) {
		for (auto *note : notes)
			if (note->text.length() || !note->is_new)
				note->write(out);
	}
	out << "</notebook>" << endl;
}

/*
 *  Read it in from 'notebook.xml'.
 */

void Notebook_gump::read(
) {
	string root;
	Configuration conf;

	conf.read_abs_config_file(NOTEBOOKXML, root);
	string identstr;
	// not spamming the terminal with all the notes in normal play
#ifdef DEBUG
	conf.dump(cout, identstr);
#endif
	Configuration::KeyTypeList note_nds;
	string basekey = "notebook";
	conf.getsubkeys(note_nds, basekey);
	One_note *note = nullptr;
	for (const auto& notend : note_nds) {
		if (notend.first == "note") {
			note = new One_note();
			notes.push_back(note);
		} else if (notend.first == "note/time") {
			int d;
			int h;
			int m;
			sscanf(notend.second.c_str(), "%d:%d:%d", &d, &h, &m);
			if (note)
				note->set_time(d, h, m);
		} else if (notend.first == "note/place") {
			int x;
			int y;
			sscanf(notend.second.c_str(), "%d:%d", &x, &y);
			if (note)
				note->set_loc(x, y);
		} else if (notend.first == "note/text") {
			if (note)
				note->set_text(notend.second);
		} else if (notend.first == "note/gflag") {
			int gf;
			sscanf(notend.second.c_str(), "%d", &gf);
			if (note)
				note->set_gflag(gf);
		}
	}
}

/*
 *  Read in text to be inserted automatically when global flags are set.
 */

// read in from external file
void Notebook_gump::read_auto_text_file(const char *filename) {
	if (gwin->get_allow_autonotes()) {
		initialized_auto_text = true;
		std::unique_ptr<std::istream> pNotesfile;
		if (is_system_path_defined("<PATCH>") && U7exists(PATCH_AUTONOTES)) {
			cout << "Loading patch autonotes" << endl;
			pNotesfile = U7open_in(PATCH_AUTONOTES, true);
		} else {
			cout << "Loading autonotes from file " << filename << endl;
			pNotesfile = U7open_in(filename, true);
		}
		if (!pNotesfile)
			return;
		auto& notesfile = *pNotesfile;
		Read_text_msg_file(notesfile, auto_text);
	}
}

// read in from flx bundled file
void Notebook_gump::read_auto_text(
) {
	if (gwin->get_allow_autonotes()) {
		initialized_auto_text = true;
		if (is_system_path_defined("<PATCH>") && U7exists(PATCH_AUTONOTES)) {
			cout << "Loading patch autonotes" << endl;
			auto pNotesfile = U7open_in(PATCH_AUTONOTES, true);
			if (!pNotesfile)
				return;
			auto& notesfile = *pNotesfile;
			Read_text_msg_file(notesfile, auto_text);
		} else {
			const str_int_pair &resource = game->get_resource("config/autonotes");
			IExultDataSource buf(resource.str, resource.num);
			if (buf.good()) {
				cout << "Loading default autonotes" << endl;
				Read_text_msg_file(&buf, auto_text);
			}
		}
	}
}
