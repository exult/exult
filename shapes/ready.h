/*
 *  ready.h - Information from the 'ready.dat' file.
 *
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

#ifndef INCL_READY
#define INCL_READY 1

/*
 *  Internal Exult types describing how a shape may be worn.
 */
enum Ready_type_Exult {
	head        = 0x00,
	backpack    = 0x01,
	belt        = 0x02,
	lhand       = 0x03,
	lfinger     = 0x04,
	legs        = 0x05,
	feet        = 0x06,
	rfinger     = 0x07,    // Only for usecode or for alternate slot.
	rhand       = 0x08,
	torso       = 0x09,
	amulet      = 0x0a,
	quiver      = 0x0b,
	back_2h     = 0x0c,    // Only for usecode or for alternate slot
	back_shield = 0x0d,    // Only for usecode or for alternate slot
	earrings    = 0x0e,
	cloak       = 0x0f,
	gloves      = 0x10,
	ucont       = 0x11,
	// The following four entries are not actual spots, but mark an object as
	// filling multiple spots of the given type.
	both_hands = 0x12,    // Uses both hands.
	lrgloves   = 0x13,    // Uses gloves spot, fills ring.
	neck       = 0x14,    // Uses amulet spot, fills cloak.
	// Used for alternate slot only:
	scabbard = 0x15,    // Uses belt spot, fills back_2h, back_shield.
	// Marks as firing triple crossbow bolts:
	triple_bolts = 0x16,
	// Used for paperdolling only:
	cloak_clasp  = 102,
	invalid_spot = 0xff
};

// Black Gate Ready types
enum Ready_type_BG {
	backpack_bg     = 0x00,
	lhand_bg        = 0x01,
	rhand_bg        = 0x02,    // Only for usecode or for alternate slot
	belt_bg         = 0x03,    // Only for usecode or for alternate slot
	neck_bg         = 0x04,
	torso_bg        = 0x05,
	lfinger_bg      = 0x06,
	rfinger_bg      = 0x07,    // Only for usecode or for alternate slot
	quiver_bg       = 0x08,
	head_bg         = 0x09,
	legs_bg         = 0x0a,
	feet_bg         = 0x0b,
	ucont_bg        = 0x0c,    // Porting from SI
	cloak_bg        = 0x0d,    // Porting from SI
	gloves_bg       = 0x0e,    // Porting from SI
	triple_bolts_bg = 0x0f,
	earrings_bg     = 0x10,    // Porting from SI
	back_shield_bg  = 0x11,    // Porting from SI
	tongs_bg        = 0x12,    // Silently converted to lhand_bg
	back_2h_bg      = 0x13,    // Porting from SI
	both_hands_bg   = 0x14,
	lrgloves_bg     = 0x15,
	amulet_bg       = 0x16,
	scabbard_bg     = 0x17    // Only for alternate slot
};

// Serpent Isle Ready types
enum Ready_type_SI {
	rhand_si        = 0x00,
	lhand_si        = 0x01,
	cloak_si        = 0x02,
	amulet_si       = 0x03,
	head_si         = 0x04,
	gloves_si       = 0x05,
	ucont_si        = 0x06,
	rfinger_si      = 0x07,    // Only for usecode or for alternate slot
	lfinger_si      = 0x08,
	earrings_si     = 0x09,
	quiver_si       = 0x0a,
	belt_si         = 0x0b,
	torso_si        = 0x0c,
	feet_si         = 0x0d,
	legs_si         = 0x0e,
	backpack_si     = 0x0f,
	back_shield_si  = 0x10,    // Only for usecode or for alternate slot
	back_2h_si      = 0x11,    // Only for usecode or for alternate slot
	triple_bolts_si = 0x12,    // Porting from BG
	both_hands_si   = 0x14,
	lrgloves_si     = 0x15,    // Porting from BG
	neck_si         = 0x16,
	scabbard_si     = 0x17    // Only for alternate slot
};

/*
 *  Convert to BG ready spot # from ours (or invalid_spot if not found).
 */

inline int Ready_spot_to_BG(int spot) {
	switch (spot) {
	case head:
		return head_bg;
	case backpack:
		return backpack_bg;
	case belt:
		return belt_bg;
	case lhand:
		return lhand_bg;
	case lfinger:
		return lfinger_bg;
	case legs:
		return legs_bg;
	case feet:
		return feet_bg;
	case rfinger:
		return rfinger_bg;
	case rhand:
		return rhand_bg;
	case torso:
		return torso_bg;
	case amulet:
		return amulet_bg;
	case quiver:
		return quiver_bg;
	case back_2h:
		return back_2h_bg;
	case back_shield:
		return back_shield_bg;
	case earrings:
		return earrings_bg;
	case cloak:
		return cloak_bg;
	case gloves:
		return gloves_bg;
	case ucont:
		return ucont_bg;
	case both_hands:
		return both_hands_bg;
	case lrgloves:
		return lrgloves_bg;
	case triple_bolts:
		return triple_bolts_bg;
	case scabbard:
		return scabbard_bg;
	case neck:
		return neck_bg;
	default:
		return invalid_spot;
	}
}

/*
 *  Convert to SI ready spot # from ours (or invalid_spot if not found).
 */

inline int Ready_spot_to_SI(int spot) {
	switch (spot) {
	case head:
		return head_si;
	case backpack:
		return backpack_si;
	case belt:
		return belt_si;
	case lhand:
		return lhand_si;
	case lfinger:
		return lfinger_si;
	case legs:
		return legs_si;
	case feet:
		return feet_si;
	case rfinger:
		return rfinger_si;
	case rhand:
		return rhand_si;
	case torso:
		return torso_si;
	case amulet:
		return amulet_si;
	case quiver:
		return quiver_si;
	case back_2h:
		return back_2h_si;
	case back_shield:
		return back_shield_si;
	case earrings:
		return earrings_si;
	case cloak:
		return cloak_si;
	case gloves:
		return gloves_si;
	case ucont:
		return ucont_si;
	case both_hands:
		return both_hands_si;
	case lrgloves:
		return lrgloves_si;
	case triple_bolts:
		return triple_bolts_si;
	case scabbard:
		return scabbard_si;
	case neck:
		return neck_si;
	default:
		return invalid_spot;
	}
}

/*
 *  Convert from BG ready spot # to ours (or invalid_spot if not found).
 */

inline int Ready_spot_from_BG(int spot) {
	switch (spot) {
	case head_bg:
		return head;
	case backpack_bg:
		return backpack;
	case belt_bg:
		return belt;
	case lhand_bg:
		return lhand;
	case lfinger_bg:
		return lfinger;
	case legs_bg:
		return legs;
	case feet_bg:
		return feet;
	case rfinger_bg:
		return rfinger;
	case rhand_bg:
		return rhand;
	case torso_bg:
		return torso;
	case amulet_bg:
		return amulet;
	case quiver_bg:
		return quiver;
	case back_2h_bg:
		return back_2h;
	case back_shield_bg:
		return back_shield;
	case earrings_bg:
		return earrings;
	case cloak_bg:
		return cloak;
	case gloves_bg:
		return gloves;
	case ucont_bg:
		return ucont;
	case both_hands_bg:
		return both_hands;
	case lrgloves_bg:
		return lrgloves;
	case triple_bolts_bg:
		return triple_bolts;
	case tongs_bg:
		return lhand;
	case scabbard_bg:
		return scabbard;
	case neck_bg:
		return neck;
	default:
		return invalid_spot;
	}
}

/*
 *  Convert from SI ready spot # to ours (or invalid_spot if not found).
 */

inline int Ready_spot_from_SI(int spot) {
	switch (spot) {
	case head_si:
		return head;
	case backpack_si:
		return backpack;
	case belt_si:
		return belt;
	case lhand_si:
		return lhand;
	case lfinger_si:
		return lfinger;
	case legs_si:
		return legs;
	case feet_si:
		return feet;
	case rfinger_si:
		return rfinger;
	case rhand_si:
		return rhand;
	case torso_si:
		return torso;
	case amulet_si:
		return amulet;
	case quiver_si:
		return quiver;
	case back_2h_si:
		return back_2h;
	case back_shield_si:
		return back_shield;
	case earrings_si:
		return earrings;
	case cloak_si:
		return cloak;
	case gloves_si:
		return gloves;
	case ucont_si:
		return ucont;
	case both_hands_si:
		return both_hands;
	case lrgloves_si:
		return lrgloves;
	case triple_bolts_si:
		return triple_bolts;
	case scabbard_si:
		return scabbard;
	case neck_si:
		return neck;
	default:
		return invalid_spot;
	}
}

#endif /* INCL_READY   */
