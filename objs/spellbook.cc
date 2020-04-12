/**
 ** Spellbook.cc - Spellbook object.
 **
 ** Written: 10/1/98 - JSF
 **/

/*
Copyright (C) 2000-2013 The Exult Team.

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

#include "spellbook.h"
#include "gamewin.h"
#include "gamemap.h"
#include "utils.h"
#include "game.h"
#include "Gump_manager.h"
#include "databuf.h"
#include "ucsched.h"
#include "actors.h"
#include "cheat.h"
#include "ucmachine.h"
#include "frflags.h"
#include "usefuns.h"
#include "ignore_unused_variable_warning.h"

using std::memcpy;
using std::ostream;

// ++++TAG: De-hard-code reagent shape and costs.
const int REAGENTS = 842;       // Shape #.

/*
 *  Flags for required reagents.  Bits match frame #.
 */
const int bp = 1;           // Black pearl.
const int bm = 2;           // Blood moss.
const int ns = 4;           // Nightshade.
const int mr = 8;           // Mandrake root.
const int gr = 16;          // Garlic.
const int gn = 32;          // Ginseng.
const int ss = 64;          // Spider silk.
const int sa = 128;         // Sulphuras ash.
const int bs = 256;         // Blood spawn.
const int sc = 512;         // Serpent scales.
const int wh = 1024;            // Worm's hart.

// Black Gate:
unsigned short Spellbook_object::bg_reagents[9 * 8] = {
	0, 0, 0, 0, 0, 0, 0, 0,     // Linear spells require no reagents.
	// Circle 1:
	gr | gn | mr, gr | gn, ns | ss, gr | ss, sa | ss, sa, ns, gr | gn,
	// Circle 2:
	bm | sa, bp | mr, bp | sa, mr | sa, gr | gn | mr, gr | gn | sa, bp | bm | mr,
	bm | ns | mr | sa | bp | ss,
	// Circle 3:
	gr | ns | sa, gr | gn | ss, ns | mr | bm, sa | gn | gr | mr, ns | ss, ns | mr,
	bp | ns | bm, ns | ss | bp,
	// Circle 4:
	ss | mr, bp | sa | mr, mr | bp | bm, gr | mr | ns | sa, mr | bp | bm, bm | sa,
	bm | mr | ns | ss | sa, bm | sa,
	// Circle 5:
	bp | ns | ss, mr | gr | bm, gr | bp | sa | ss, bm | bp | mr | sa, gn | ss | mr | gr,
	ns | bm, bp | sa | ss, gn | ns | ss,
	// Circle 6:
	gr | mr | ns, sa | ss | bm | gn | ns | mr, bp | mr | ss | sa, sa | bp | bm, mr | ns | sa | bm,
	ns | ss | bp, gn | ss | bp, bm | sa | mr,
	// Circle 7:
	mr | ss, bp | ns | sa, bm | bp | mr | ss | sa, bp | mr | ss | sa, bm | mr | ns | sa,
	bp | ns | ss | mr, bp | gn | mr, gr | gn | mr | ss,
	// Circle 8:
	bp | bm | gr | gn | mr | ns | ss | sa, bm | mr | ns | sa, gr | gn | mr | ns | bm, mr | ns | bm | bp,
	gr | gn | ss | sa, bm | gr | mr, bp | mr | ns, bm | gr | mr
};
// Serpent Isle:
unsigned short Spellbook_object::si_reagents[9 * 8] = {
	// Circle 1:
	gr | gn | mr, gr | gn, ns | ss, gr | ss, sa | ss, sa, ns, bp | bm | mr,
	// Circle 2:
	gr | gn, bm | sa, ns | sa, bp | sa | wh, mr | sa, gr | gn | ss, gr | gn | mr, gr | gn | sa,
	// Circle 3:
	gr | gn | wh, gr | ns | sa, bp | mr, bp | gr, gr | gn | mr | sa, ns | ss, bp | ns | ss, bp | mr | sa | sa,
	// Circle 4:
	bm | mr, gr | ss, mr | sa, sa | bm | gr | mr | ss | sc, gr | mr | ns | sa, bm | sa, bp | ss, bm | sa,
	// Circle 5:
	mr | ss, bp | gr | ss | sa, bm | bp | mr | sa, gr | gn | mr | ss, bm | ns, gn | ns | ss, sa | bm | mr | ns | ss,
	bp | gr | mr | sa,
	// Circle 6:
	bp | ns | ss, gr | mr | ns, gr | mr | ns, bp | wh | ss | sa, bp | wh | mr | ss | sa,
	bm | bp | wh | sa, bm | gn | sa, mr | sa | ss | sc,
	// Circle 7:
	bp | mr | ss | sa, bm | mr | ns | sa, gr | gn, bp | gn | mr, bm | ns | sa, gr | gn | mr | ss,
	bp | bm | mr | ss, bp | mr | sa,
	// Circle 8:
	wh | ss, bs | bp | ns | sa, bm | bp | mr | ss | sa, bm | bp | mr, bm | gr | ss | wh | sc,
	bm | bp | gr | ss | wh | sc, gr | mr | sa, bp | bs | mr | ns,
	// Circle 9:
	bm | mr | ns | sa, bm | bs | gr | gn | mr | ns, bp | bm | mr | ns, bm | bs | bp | ns | sa,
	bp | gr | mr | ss | sa, bm | gr | mr | ss, bm | gr | mr, ns | sa | wh | sc | bs
};

/*
 *  Get usecode function for a given spell:
 */
int Get_usecode(int spell) {
	return BaseSpellsUsecode + spell;
}

/*
 *  Test for "Ring" of Reagants.
 */

bool Spellbook_object::has_ring(
    Actor *act
) {
	return act->check_gear_powers(Frame_flags::infinite_reagents);
}

/*
 *  Create a spellbook from Ireg data.
 */

Spellbook_object::Spellbook_object(
    int shapenum, int framenum,
    unsigned int shapex, unsigned int shapey,
    unsigned int lft,
    unsigned char *c,       // Circle spell flags.
    unsigned char bmark     // Spell bookmark.
) : Ireg_game_object(shapenum, framenum,
	                     shapex, shapey, lft), bookmark(bmark == 255 ? -1 : bmark) {
	memcpy(circles, c, sizeof(circles));
	// Point to reagent table.
	reagents = GAME_SI ? si_reagents : bg_reagents;
}

/*
 *  Add a spell.
 *
 *  Output: 0 if already have it, 1 if successful.
 */

int Spellbook_object::add_spell(
    int spell           // 0-71
) {
	int circle = spell / 8;
	int num = spell % 8;    // # within circle.
	if (circles[circle] & (1 << num))
		return 0;       // Already have it.
	circles[circle] |= (1 << num);
	return 1;
}
/*
 *  Remove all spells.
 */

void Spellbook_object::clear_spells() {
	memset(circles, 0, sizeof(circles));
}

/*
 *  Remove a spell.
 *
 *  Output: 1 if had spell, 0 if not.
 */

int Spellbook_object::remove_spell(
    int spell           // 0-71
) {
	int circle = spell / 8;
	int num = spell % 8;    // # within circle.
	if ((circles[circle] & (1 << num)) == 0)
		return 0;       // Already does not have it.
	circles[circle] ^= (1 << num);
	return 1;
}

/*
 *  Can we do a given spell?
 */

bool Spellbook_object::can_do_spell(
    Actor *act,
    int spell
) {
	if (cheat.in_wizard_mode())
		return true;        // Cheating.
	int circle = spell / 8;     // Circle spell is in.
	unsigned char cflags = circles[circle];
	if ((cflags & (1 << (spell % 8))) == 0)
		return false;       // We don't have that spell.
	int mana = act->get_property(Actor::mana);
	// ++++TAG: Need to de-hard-code cost.
	int cost = circle + (GAME_SI ? 1 : 0);
	int level = act->get_level();
	if ((mana < cost) || (level < circle))
		// Not enough mana or not yet at required level?
		return false;
	if (has_ring(act))      // Ring of reagents (SI)?
		return true;
	// Figure what we used.
	unsigned short flags = reagents[spell];
	// Go through bits.
	for (int r = 0; flags; r++, flags = flags >> 1)
		// Need 1 of each required reagent.
		if ((flags & 1) &&
		        act->count_objects(REAGENTS, c_any_qual, r) == 0)
			return false;   // Missing.
	return true;
}

/*
 *  Perform a spell.
 *
 *  Output: False if unsuccessful.
 */

bool Spellbook_object::do_spell(
    Actor *act,
    int spell,
    bool can_do,            // Already checked.
    bool in_combat          // Being used in combat.
) {
	if (can_do || can_do_spell(act, spell)) {
		int circle = spell / 8; // Figure/subtract mana.
		if (cheat.in_wizard_mode())
			circle = 0;
		int mana = act->get_property(Actor::mana);
		// ++++TAG: Need to de-hard-code cost.
		act->set_property(Actor::mana, mana - circle - (GAME_SI ? 1 : 0));
		// Figure what we used.
		unsigned short flags = reagents[spell];

		if (!cheat.in_wizard_mode() && !has_ring(act)) {
			// Go through bits.
			for (int r = 0; flags; r++, flags = flags >> 1)
				// Remove 1 of each required reagent.
				if (flags & 1)
					act->remove_quantity(1,
					                     REAGENTS, c_any_qual, r);
		}
		execute_spell(act, spell, in_combat);
		return true;
	}
	return false;
}

/*
 *  Perform the usecode for a spell.
 */

void Spellbook_object::execute_spell(
    Actor *act,
    int spell,
    bool in_combat          // Being used in combat.
) {
	act->begin_casting(859);    // ++++TAG: Need to de-hard-code.

	// We use intercept_item for spells cast from readied spellbook
	// while in combat.
	// First, save current.
	Game_object *oldtarg;
	Tile_coord *oldtile;
	ucmachine->save_intercept(oldtarg, oldtile);
	if (in_combat)  // Use caster's target if for combat.
		ucmachine->intercept_click_on_item(act->get_target());
	else    // Otherwise, disable intercept for gump casting.
		ucmachine->intercept_click_on_item(nullptr);

	ucmachine->call_usecode(Get_usecode(spell), act,
	                        in_combat ? Usecode_machine::weapon :
	                        Usecode_machine::double_click);

	// Restore previous intercept_item.
	ucmachine->restore_intercept(oldtarg, oldtile);
}

/*
 *  Show book when double-clicked.
 */

void Spellbook_object::activate(
    int event
) {
	ignore_unused_variable_warning(event);
	gumpman->add_gump(this, get_info().get_gump_shape());
}

/*
 *  Write out.
 */

void Spellbook_object::write_ireg(
    ODataSource *out
) {
	unsigned char buf[24];      // 18-byte entry.
	uint8 *ptr = write_common_ireg(18, buf);
	memcpy(ptr, &circles[0], 5);    // Store the way U7 does it.
	ptr += 5;
	*ptr++ = nibble_swap(get_lift());
	memcpy(ptr, &circles[5], 4);    // Rest of spell circles.
	ptr += 4;
	*ptr++ = 0;         // 3 unknowns.
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = bookmark >= 0 ? bookmark : 255;
	out->write(reinterpret_cast<char *>(buf), ptr - buf);
	// Write scheduled usecode.
	Game_map::write_scheduled(out, this);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Spellbook_object::get_ireg_size() {
	// These shouldn't ever happen, but you never know
	if (gumpman->find_gump(this) || Usecode_script::find(this))
		return -1;
	return 14 + get_common_ireg_size();
}
