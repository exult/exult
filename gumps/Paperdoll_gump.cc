/*
Copyright (C) 2000-2013 The Exult Team

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

#include "game.h"
#include "gamewin.h"
#include "Gump_button.h"
#include "misc_buttons.h"
#include "Paperdoll_gump.h"
#include "contain.h"
#include "actors.h"
#include "objiter.h"
#include "cheat.h"
#include "miscinf.h"
#include "shapeid.h"
#include "ready.h"
#include "ammoinf.h"
#include "npcdollinf.h"
#include "objdollinf.h"
#include "weaponinf.h"
#include "ignore_unused_variable_warning.h"

#include <cstdio>

using std::size_t;

//#define SHOW_USECODE_CONTAINER
//#define SHOW_NONREADIED_OBJECTS

/*
 *
 *  SERPENT ISLE PAPERDOLL GUMP
 *
 */

/*
 *  Statics:
 */

// Paperdoll is completely different to Actor
short Paperdoll_gump::diskx = 123, Paperdoll_gump::disky = 137;
short Paperdoll_gump::heartx = 97, Paperdoll_gump::hearty = 137;
short Paperdoll_gump::combatx = 51, Paperdoll_gump::combaty = 142;
short Paperdoll_gump::cstatx = 72, Paperdoll_gump::cstaty = 137;
short Paperdoll_gump::cmodex = 75, Paperdoll_gump::cmodey = 140;
short Paperdoll_gump::halox = 123, Paperdoll_gump::haloy = 120;

short Paperdoll_gump::coords[36] = {
	76, 20,     /* head */  115, 27,    /* back */
	105, 65,    /* belt */  38, 55,     /* lhand */
	55, 62,     /* lfinger */   80, 80,     /* legs */
	84, 105,    /* feet */  90, 50,     /* rfinger */
	117, 55,    /* rhand */ 45, 35,     /* torso */
	44, 26,     /* neck */  69, 59,     /* ammo */
	59, 19,     /* back2 */ 94, 20,     /* back 3 (shield) */
	76, 26,     /* ears */  76, 33,     /* cloak */
	73, 53,     /* gloves */    0, 0        /* usecode container */
};
short Paperdoll_gump::coords_blue[36] = {
	76, 20,     /* head */  64, 27,     /* back */
	54, 56,     /* belt */  30, 58,     /* lhand */
	55, 62,     /* lfinger */   80, 80,     /* legs */
	84, 105,    /* feet */  90, 50,     /* rfinger */
	68, 50,     /* rhand */ 45, 37,     /* torso */
	22, 26,     /* neck */  69, 59,     /* ammo */
	59, 19,     /* back2 */ 94, 20,     /* back 3 (shield) */
	76, 26,     /* ears */  76, 33,     /* cloak */
	73, 53,     /* gloves */    0, 0        /* usecode container */
};
short Paperdoll_gump::coords_hot[36] = {
	76, 20,     /* head */  94, 27,     /* back */
	92,  61,    /* belt */  38, 55,     /* lhand */
	55, 62,     /* lfinger */   80, 80,     /* legs */
	84, 105,    /* feet */  90, 50,     /* rfinger */
	117, 55,    /* rhand */ 83, 43,     /* torso */
	76, 41,     /* neck */  69, 59,     /* ammo */
	59, 19,     /* back2 */ 94, 20,     /* back 3 (shield) */
	76, 26,     /* ears */  76, 33,     /* cloak */
	73, 53,     /* gloves */    0, 0        /* usecode container */
};



//
// Paperdoll Coords
//

short Paperdoll_gump::bodyx = 46, Paperdoll_gump::bodyy = 33;
short Paperdoll_gump::headx = 46, Paperdoll_gump::heady = 22;

short Paperdoll_gump::beltfx = 58, Paperdoll_gump::beltfy = 52;
short Paperdoll_gump::neckfx = 46, Paperdoll_gump::neckfy = 47;
short Paperdoll_gump::beltmx = 57, Paperdoll_gump::beltmy = 55;
short Paperdoll_gump::neckmx = 46, Paperdoll_gump::neckmy = 44;

short Paperdoll_gump::legsx = 57, Paperdoll_gump::legsy = 66;
short Paperdoll_gump::feetx = 46, Paperdoll_gump::feety = 99;

short Paperdoll_gump::handsx = 68, Paperdoll_gump::handsy = 44;
short Paperdoll_gump::rhandx = 68, Paperdoll_gump::rhandy = 44;
short Paperdoll_gump::lhandx = 34, Paperdoll_gump::lhandy = 65;

short Paperdoll_gump::ahandx = 28, Paperdoll_gump::ahandy = 59;
short Paperdoll_gump::ammox = 28, Paperdoll_gump::ammoy = 59;

short Paperdoll_gump::backfx = 68, Paperdoll_gump::backfy = 28;
short Paperdoll_gump::back2fx = 34, Paperdoll_gump::back2fy = 22;
short Paperdoll_gump::backmx = 68, Paperdoll_gump::backmy = 22;
short Paperdoll_gump::back2mx = 35, Paperdoll_gump::back2my = 22;
short Paperdoll_gump::shieldfx = 56, Paperdoll_gump::shieldfy = 25;
short Paperdoll_gump::shieldmx = 57, Paperdoll_gump::shieldmy = 22;


/*
 *  Find the index of the closest 'spot' to a mouse point.
 *
 *  Output: Index, or -1 if unsuccessful.
 */

int Paperdoll_gump::find_closest(
    int mx, int my,         // Mouse point in window.
    int only_empty          // Only allow empty spots.
) {
	mx -= x;
	my -= y;       // Get point rel. to us.
	long closest_squared = 1000000; // Best distance squared.
	int closest = -1;       // Best index.
	int spot;

	Actor *npc = container->as_actor();

	for (size_t i = 0; i < sizeof(coords_hot) / (2 * sizeof(coords_hot[0])); i++) {
		spot = i;

		int dx = mx - coords_hot[spot * 2];
		int dy = my - coords_hot[spot * 2 + 1];
		long dsquared = dx * dx + dy * dy;

		// Map slots occupied by multi-slot items to the filled slot.
		if ((i == back_shield || i == back_2h) && npc->is_scabbard_used())
			spot = belt;
		else if (i == cloak && npc->is_neck_used())
			spot = amulet;
		else if (i == rhand && npc->is_two_handed())
			spot = lhand;
		else if ((i == rfinger || i == gloves) && npc->is_two_fingered())
			spot = lfinger;

		// Better than prev and free if required.?
		if (dsquared < closest_squared && !(only_empty && container->get_readied(spot))) {
			closest_squared = dsquared;
			closest = spot;
		}
	}

	return closest;
}

/*
 *  Create the gump display for an actor.
 */

Paperdoll_gump::Paperdoll_gump(
    Container_game_object *cont,    // Container it represents.
    int initx, int inity,       // Coords. on screen.
    int shnum           // Shape #.
) : Gump(cont, initx, inity, 123, SF_PAPERDOL_VGA) {
	ignore_unused_variable_warning(shnum);
	set_object_area(Rectangle(26, 0, 104, 140), 6, 145);

	// Create Heart button
	heart_button = new Heart_button(this, heartx, hearty);
	Actor *actor = cont->as_actor();
	// Create Cstats button or Halo and Cmode
	if (Game::get_game_type() == BLACK_GATE) {
		if (actor->get_npc_num() == 0)
			halo_button = new Halo_button(this, halox, haloy,
			                              actor);
		else
			halo_button = new Halo_button(this, diskx, disky,
			                              actor);

		cmode_button = new Combat_mode_button(this, cmodex, cmodey,
		                                      actor);
		cstats_button = nullptr;
	} else {
		cstats_button = new Cstats_button(this, cstatx, cstaty);
		halo_button = nullptr;
		cmode_button = nullptr;
	}



	// If Avatar create Disk Button
	if (actor->get_npc_num() == 0)
		disk_button = new Disk_button(this, diskx, disky);
	else
		disk_button = nullptr;


	// If Avatar create Combat Button
	if (actor->get_npc_num() == 0)
		combat_button = new Combat_button(this, combatx, combaty);
	else
		combat_button = nullptr;


	// Put all the objects in the right place
	for (size_t i = 0; i < sizeof(coords) / 2 * sizeof(coords[0]); i++) {
		Game_object *obj = container->get_readied(i);
		if (obj)
			set_to_spot(obj, i);
	}
}

/*
 *  Delete actor display.
 */

Paperdoll_gump::~Paperdoll_gump(
) {
	delete heart_button;
	delete disk_button;
	delete combat_button;
	delete cstats_button;
	delete halo_button;
	delete cmode_button;
}

/*
 *  Is a given screen point on one of our buttons?
 *
 *  Output: ->button if so.
 */

Gump_button *Paperdoll_gump::on_button(
    int mx, int my          // Point in window.
) {
	Gump_button *btn = Gump::on_button(mx, my);
	if (btn)
		return btn;
	else if (heart_button && heart_button->on_button(mx, my))
		return heart_button;
	else if (disk_button && disk_button->on_button(mx, my))
		return disk_button;
	else if (combat_button && combat_button->on_button(mx, my))
		return combat_button;
	else if (cstats_button && cstats_button->on_button(mx, my))
		return cstats_button;
	else if (halo_button && halo_button->on_button(mx, my))
		return halo_button;
	else if (cmode_button && cmode_button->on_button(mx, my))
		return cmode_button;
	return nullptr;
}

/*
 *  Add an object.
 *
 *  Output: false if cannot add it.
 */

bool Paperdoll_gump::add(
    Game_object *obj,
    int mx, int my,         // Screen location of mouse.
    int sx, int sy,         // Screen location of obj's hotspot.
    bool dont_check,        // Skip volume check.
    bool combine            // True to try to combine obj.  MAY
    //   cause obj to be deleted.
) {
	ignore_unused_variable_warning(sx, sy);
	do {
		Game_object *cont = find_object(mx, my);

		if (cont && cont->add(obj, false, combine))
			break;

		int index = find_closest(mx, my, 1);

		if (index != -1 && container->add_readied(obj, index))
			break;

		if (container->add(obj, dont_check, combine))
			break;

		return false;
	} while (false);

	// Put all the objects in the right place
	for (size_t i = 0; i < sizeof(coords) / 2 * sizeof(coords[0]); i++) {
		obj = container->get_readied(i);
		if (obj) set_to_spot(obj, i);
	}

	return true;
}

/*
 *  Set object's coords. to given spot.
 */

void Paperdoll_gump::set_to_spot(
    Game_object *obj,
    int index           // Spot index.
) {

	// Get shape.
	Shape_frame *shape = obj->get_shape();
	//if (!shape)           // Funny?  Try frame 0.
	//  shape = gwin->get_shape(obj->get_shapenum(), 0);
	if (!shape)
		return;

	// Height and width
	int w = shape->get_width();
	int h = shape->get_height();

	// Set object's position.
	obj->set_shape_pos(
	    spotx(index) + shape->get_xleft() - w / 2 - object_area.x,
	    spoty(index) + shape->get_yabove() - h / 2 - object_area.y);
}

/*
 *  Paint on screen.
 */

void Paperdoll_gump::paint(
) {
	const Game_object *obj;

	// Paint Objects
	Rectangle box = object_area;    // Paint objects inside.
	box.shift(x, y);        // Set box to screen location.

	paint_shape(x, y);

	// Paint red "checkmark".
	paint_elems();

	// Get the information required about ourself
	Actor *actor = container->as_actor();
	const Paperdoll_npc *info = actor->get_info().get_npc_paperdoll();

	if (!info) {
		const Shape_info &inf = ShapeID::get_info(actor->get_sexed_coloured_shape());
		info = inf.get_npc_paperdoll();
	}
	if (!info) {
		const Shape_info &inf = ShapeID::get_info(actor->get_shape_real());
		info = inf.get_npc_paperdoll_safe(actor->get_type_flag(Actor::tf_sex));
	}

	// Spots that are female/male specific
	int shieldx;
	int shieldy;
	int back2x;
	int back2y;
	int backx;
	int backy;
	int neckx;
	int necky;
	int beltx;
	int belty;

	if (actor->get_type_flag(Actor::tf_sex) || info->is_npc_female()) {
		// Set the female spots
		shieldx = shieldfx;
		shieldy = shieldfy;
		back2x = back2fx;
		back2y = back2fy;
		backx = backfx;
		backy = backfy;
		neckx = neckfx;
		necky = neckfy;
		beltx = beltfx;
		belty = beltfy;
	} else {            // Set the male spots
		shieldx = shieldmx;
		shieldy = shieldmy;
		back2x = back2mx;
		back2y = back2my;
		backx = backmx;
		backy = backmy;
		neckx = neckmx;
		necky = neckmy;
		beltx = beltmx;
		belty = beltmy;
	}

	// Now paint. Order is very specific


	if (actor->is_scabbard_used()) {
		paint_object(box, info, belt,            shieldx, shieldy, 0, back_shield);
		paint_object(box, info, belt,            back2x,  back2y,  0, back_2h);
	} else {
		paint_object(box, info, back_shield,     shieldx, shieldy);
		paint_object(box, info, back_2h,         back2x,  back2y);
	}
	paint_object(box, info, backpack,        backx,  backy);
	if (actor->is_neck_used())
		paint_object(box, info, amulet,          bodyx,   bodyy,   0, cloak);
	else
		paint_object(box, info, cloak,           bodyx,   bodyy);
	paint_body(box, info);
	paint_object(box, info, legs,            legsx,  legsy);
	paint_object(box, info, feet,            feetx,  feety);
	paint_object(box, info, quiver,          ammox,  ammoy,    0, -1);
	paint_object(box, info, torso,           bodyx,  bodyy);
	paint_belt(box, info);
	paint_head(box, info);
	if (actor->is_neck_used()) {
		obj = container->get_readied(amulet);
		const Paperdoll_item *item1;
		const Paperdoll_item *item2;
		if (obj) {
			const Shape_info &inf = obj->get_info();
			item1 = inf.get_item_paperdoll(obj->get_framenum(), cloak);
			item2 = inf.get_item_paperdoll(obj->get_framenum(), cloak_clasp);
		} else
			item1 = item2 = nullptr;
		if (!item1 && !item2)
			paint_object(box, info, amulet,          neckx,  necky);
	} else
		paint_object(box, info, amulet,          neckx,  necky);
	if (!actor->is_scabbard_used())
		paint_object(box, info, belt,            beltx,  belty);
	paint_arms(box, info);
	paint_object_arms(box, info, torso,           bodyx,  bodyy,    1, torso);
	paint_object(box, info, earrings,        headx,  heady);
	paint_object(box, info, head,            headx,  heady);
	if (actor->is_neck_used())
		paint_object(box, info, amulet,          bodyx,  bodyy,    0, cloak_clasp);
	else
		paint_object(box, info, cloak,           bodyx,  bodyy,    0, cloak_clasp);
	paint_object_arms(box, info, rfinger,         lhandx, lhandy,   0);
	if (actor->is_two_fingered()) {
		obj = container->get_readied(lfinger);
		const Paperdoll_item *item1;
		if (obj) {
			const Shape_info &inf = obj->get_info();
			item1 = inf.get_item_paperdoll(obj->get_framenum(), gloves);
		} else
			item1 = nullptr;
		if (!item1)
			paint_object_arms(box, info, lfinger,         rhandx, rhandy);
		else
			paint_object_arms(box, info, lfinger,         handsx, handsy,   0, gloves);
	} else
		paint_object_arms(box, info, lfinger,         rhandx, rhandy,   0);
	paint_object_arms(box, info, gloves,          handsx, handsy,   0);
	paint_object(box, info, lhand,           lhandx, lhandy);
	paint_object(box, info, quiver,          ahandx, ahandy,   2, -1);
	paint_object(box, info, rhand,           rhandx, rhandy);

	// if debugging show usecode container
#ifdef SHOW_USECODE_CONTAINER
	paint_object(box, info, ucont,  20,      20);
#endif

#ifdef SHOW_NONREADIED_OBJECTS
	Game_object *itm;
	Object_iterator iter(actor->get_objects());
	while ((itm = iter.get_next()) != nullptr)
		if (actor->find_readied(itm) == -1)
			itm->paint();
#endif


	// Paint buttons.
	if (heart_button) heart_button->paint();
	if (disk_button) disk_button->paint();
	if (combat_button) combat_button->paint();
	if (cstats_button) cstats_button->paint();
	if (halo_button) halo_button->paint();
	if (cmode_button) cmode_button->paint();

	// Show weight.
	int max_weight = actor->get_max_weight();
	int weight = actor->get_weight() / 10;
	char text[20];
	if (gwin->failed_copy_protection())
		snprintf(text, 6, "Oink!");
	else
		snprintf(text, 20, "%d/%d", weight, max_weight);
	int twidth = sman->get_text_width(2, text);
	sman->paint_text(2, text, x + 84 - (twidth / 2), y + 114);
}

static inline bool Get_ammo_frame(
    Game_object *obj,
    Container_game_object *container,
    int &frame
) {
	const Game_object *check = container->get_readied(lhand);
	if (check) {
		const Weapon_info *winf = check->get_info().get_weapon_info();
		// frame == 2 for ammo held in hand, 0 for ammo in quiver.
		if (!winf)
			return frame != 2;
		const Ammo_info *ainf = obj->get_info().get_ammo_info();
		int family = ainf ? ainf->get_family_shape() : obj->get_shapenum();
		bool infamily = winf->get_ammo_consumed() == family;
		if (frame == 2 && !infamily)
			return false;
		else if (!frame)
			frame++;
	} else if (frame == 2)  // No weapon means no ammo in hand.
		return false;
	return true;
}

/*
 *  Paint a generic object on screen
 */

void Paperdoll_gump::paint_object(
    const Rectangle &box,       // box
    const Paperdoll_npc *info,        // info
    int spot,           // belt
    int sx, int sy,         // back2x, back2y
    int frame,          // 0
    int itemtype            // back2h_spot
) {
	Game_object *obj = container->get_readied(spot);
	if (!obj) return;

	int old_it = itemtype;
	if (itemtype == -1) itemtype = spot;

	const Paperdoll_item *item = obj->get_info().get_item_paperdoll(obj->get_framenum(), itemtype);
	if (!item || item->get_paperdoll_baseframe() == -1 ||
	        item->get_paperdoll_shape() == -1) {
		if ((old_it != -1 && !item) || (spot == quiver && frame == 2)) return;
		//if (!obj->get_tx() && !obj->get_ty()) return;

		set_to_spot(obj, spot);

		int shnum = Shapeinfo_lookup::GetBlueShapeData(spot);
		ShapeID s(shnum, 0, SF_GUMPS_VGA);

		s.paint_shape(box.x + coords_blue[spot * 2],
		              box.y + coords_blue[spot * 2 + 1]);
		int ox = box.x + obj->get_tx();
		int oy = box.y + obj->get_ty();
		obj->paint_shape(ox, oy);
		if (cheat.is_selected(obj))
			// Outline selected obj.
			obj->ShapeID::paint_outline(ox, oy, HIT_PIXEL);

		return;
	} else if (spot == quiver && !Get_ammo_frame(obj, container, frame))
		return;

	int f = item->get_paperdoll_frame(frame);
	if (item->is_gender_based() &&
	        (!container->as_actor()->get_type_flag(Actor::tf_sex) &&
	         !info->is_npc_female()))
		f++;

	ShapeID s(item->get_paperdoll_shape(), f, SF_PAPERDOL_VGA);
	s.paint_shape(box.x + sx, box.y + sy, item->is_translucent());
	if (cheat.is_selected(obj)) // Outline selected obj.
		s.paint_outline(box.x + sx, box.y + sy, HIT_PIXEL);
}

/*
 *  Paint with arms frame
 */
void Paperdoll_gump::paint_object_arms(
    const Rectangle &box,
    const Paperdoll_npc *info,
    int spot,
    int sx, int sy,
    int start,
    int itemtype
) {
	paint_object(box, info, spot, sx, sy, start + get_arm_type(), itemtype);
}

/*
 *  Paint the body
 */
void Paperdoll_gump::paint_body(
    const Rectangle &box,
    const Paperdoll_npc *info
) {
	ShapeID s(info->get_body_shape(), info->get_body_frame(), SF_PAPERDOL_VGA);
	s.paint_shape(box.x + bodyx, box.y + bodyy, info->is_translucent());
}

/*
 *  Paint the belt
 */
void Paperdoll_gump::paint_belt(
    const Rectangle &box,
    const Paperdoll_npc *info
) {
	ShapeID s(10, 0, SF_PAPERDOL_VGA);
	if (!container->as_actor()->get_type_flag(Actor::tf_sex) &&
	        !info->is_npc_female())
		s.set_frame(1);
	s.paint_shape(box.x + beltmx, box.y + beltmy, info->is_translucent());
}

/*
 *  Paint the head
 */
void Paperdoll_gump::paint_head(
    const Rectangle &box,
    const Paperdoll_npc *info
) {
	const Game_object *obj = container->get_readied(head);

	const Paperdoll_item *item = nullptr;
	if (obj)
		item = obj->get_info().get_item_paperdoll(
		           obj->get_framenum(), head);

	int f;
	if (item && item->get_spot_frame())
		f = info->get_head_frame_helm();
	else
		f = info->get_head_frame();

	ShapeID s(info->get_head_shape(), f, SF_PAPERDOL_VGA);
	s.paint_shape(box.x + headx, box.y + heady, info->is_translucent());
}

/*
 *  Paint the arms
 */
void Paperdoll_gump::paint_arms(
    const Rectangle &box,
    const Paperdoll_npc *info
) {
	int frnum = info->get_arms_frame(get_arm_type());
	ShapeID s(info->get_arms_shape(), frnum, SF_PAPERDOL_VGA);
	s.paint_shape(box.x + bodyx, box.y + bodyy, info->is_translucent());
}


/*
 *  Gets which arm frame to use
 */

int Paperdoll_gump::get_arm_type() {
	const Game_object *obj = container->get_readied(lhand);
	if (!obj)
		return 0;   // Nothing in hand; normal arms.
	const Shape_info &inf = obj->get_info();
	if (inf.get_ready_type() != both_hands)
		return 0;   // Only two-handed weapons change arms.

	const Paperdoll_item *item = inf.get_item_paperdoll(obj->get_framenum(), lhand);
	return item ? item->get_spot_frame() : 0;
}


/*
 *  Find object a screen point is on.
 *
 *  Output: Object found, or null.
 */

Game_object *Paperdoll_gump::find_object(
    int mx, int my          // Mouse pos. on screen.
) {


	// Check Objects
	Rectangle box = object_area;    // Paint objects inside.
	box.shift(x, y);        // Set box to screen location.
	mx -= box.x;
	my -= box.y;

	// Get the information required about ourself
	const Actor *actor = container->as_actor();
	const Paperdoll_npc *info = actor->get_info().get_npc_paperdoll();

	if (!info) {
		const Shape_info &inf = ShapeID::get_info(actor->get_sexed_coloured_shape());
		info = inf.get_npc_paperdoll();
	}
	if (!info) {
		const Shape_info &inf = ShapeID::get_info(actor->get_shape_real());
		info = inf.get_npc_paperdoll_safe(actor->get_type_flag(Actor::tf_sex));
	}

	int shieldx;
	int shieldy;
	int back2x;
	int back2y;
	int backx;
	int backy;
	int neckx;
	int necky;
	int beltx;
	int belty;

	if (actor->get_type_flag(Actor::tf_sex) || info->is_npc_female()) {
		shieldx = shieldfx;
		shieldy = shieldfy;
		back2x = back2fx;
		back2y = back2fy;
		backx = backfx;
		backy = backfy;
		neckx = neckfx;
		necky = neckfy;
		beltx = beltfx;
		belty = beltfy;
	} else {
		shieldx = shieldmx;
		shieldy = shieldmy;
		back2x = back2mx;
		back2y = back2my;
		backx = backmx;
		backy = backmy;
		neckx = neckmx;
		necky = neckmy;
		beltx = beltmx;
		belty = beltmy;
	}

	Game_object *obj;

	// if debugging show usecode container
#ifdef SHOW_USECODE_CONTAINER
	if ((obj = check_object(mx, my, info, ucont,  20,      20)) != nullptr)
		return obj;
#endif

	// Must be done in this order (reverse of rendering)
	if ((obj = check_object(mx, my, info, rhand,       rhandx,  rhandy)))
		return obj;
	if ((obj = check_object(mx, my, info, quiver,      ahandx,  ahandy, 2, -1)))
		return obj;
	if ((obj = check_object(mx, my, info, lhand,       lhandx,  lhandy)))
		return obj;
	if ((obj = check_object_arms(mx, my, info, gloves,      handsx, handsy, 0)))
		return obj;
	if (actor->is_two_fingered()) {
		obj = container->get_readied(lfinger);
		const Paperdoll_item *item1;
		if (obj) {
			const Shape_info &inf = obj->get_info();
			item1 = inf.get_item_paperdoll(obj->get_framenum(), gloves);
		} else
			item1 = nullptr;
		if (!item1 && (obj = check_object_arms(mx, my, info, lfinger,     rhandx, rhandy,   0)))
			return obj;
		else if ((obj = check_object_arms(mx, my, info, lfinger,     handsx, handsy, 0, gloves)))
			return obj;
	} else if ((obj = check_object_arms(mx, my, info, lfinger,     rhandx,  rhandy,  0)))
		return obj;

	if ((obj = check_object_arms(mx, my, info, rfinger,     lhandx,  lhandy,  0)))
		return obj;
	if (actor->is_neck_used()) {
		if ((obj = check_object(mx, my, info, amulet,      bodyx,   bodyy,   0, cloak_clasp)))
			return obj;
	} else {
		if ((obj = check_object(mx, my, info, cloak,       bodyx,   bodyy,   0, cloak_clasp)))
			return obj;
	}
	if ((obj = check_object(mx, my, info, head,        headx,   heady)))
		return obj;
	if ((obj = check_object(mx, my, info, earrings,    headx,   heady)))
		return obj;
	if ((obj = check_object_arms(mx, my, info, torso,       bodyx,   bodyy,  1, torso)))
		return obj;
	if (check_arms(mx, my, info))
		return nullptr;
	if (!actor->is_scabbard_used())
		if ((obj = check_object(mx, my, info, belt,        beltx,   belty)))
			return obj;
	if (actor->is_neck_used()) {
		obj = container->get_readied(amulet);
		const Paperdoll_item *item1;
		const Paperdoll_item *item2;
		if (obj) {
			const Shape_info &inf = obj->get_info();
			item1 = inf.get_item_paperdoll(obj->get_framenum(), cloak);
			item2 = inf.get_item_paperdoll(obj->get_framenum(), cloak_clasp);
		} else
			item1 = item2 = nullptr;
		if (!item1 && !item2 && (obj = check_object(mx, my, info, amulet,      neckx,   necky)))
			return obj;
	} else if ((obj = check_object(mx, my, info, amulet,      neckx,   necky)))
		return obj;
	if (check_head(mx, my, info))
		return nullptr;
	if (check_belt(mx, my, info))
		return nullptr;
	if ((obj = check_object(mx, my, info, torso,       bodyx,   bodyy)))
		return obj;
	if ((obj = check_object(mx, my, info, quiver,      ammox,   ammoy,   0, -1)))
		return obj;
	if ((obj = check_object(mx, my, info, feet,        feetx,   feety)))
		return obj;
	if ((obj = check_object(mx, my, info, legs,        legsx,   legsy)))
		return obj;
	if (check_body(mx, my, info))
		return nullptr;
	if (actor->is_neck_used()) {
		if ((obj = check_object(mx, my, info, amulet,      bodyx,   bodyy,   0, cloak)))
			return obj;
	} else {
		if ((obj = check_object(mx, my, info, cloak,       bodyx,   bodyy)))
			return obj;
	}
	if ((obj = check_object(mx, my, info, backpack,    backx,   backy)))
		return obj;
	if (actor->is_scabbard_used()) {
		if ((obj = check_object(mx, my, info, belt,        back2x,  back2y,  0, back_2h)))
			return obj;
		if ((obj = check_object(mx, my, info, belt,        shieldx, shieldy, 0, back_shield)))
			return obj;
	} else {
		if ((obj = check_object(mx, my, info, back_2h,     back2x,  back2y)))
			return obj;
		if ((obj = check_object(mx, my, info, back_shield, shieldx, shieldy)))
			return obj;
	}
	return nullptr;
}

/*
 *  Checks for a generic object on screen
 */

Game_object *Paperdoll_gump::check_object(
    int mx, int my,
    const Paperdoll_npc *info,
    int spot,
    int sx, int sy,
    int frame,
    int itemtype
) {
	Game_object *obj = container->get_readied(spot);
	if (!obj) return nullptr;

	int old_it = itemtype;
	if (itemtype == -1) itemtype = spot;

	const Paperdoll_item *item = obj->get_info().get_item_paperdoll(obj->get_framenum(), itemtype);
	if (!item || item->get_paperdoll_baseframe() == -1 ||
	        item->get_paperdoll_shape() == -1) {
		if ((old_it != -1 && !item) || (spot == quiver && frame == 2)) return nullptr;

		if (!obj->get_tx() && !obj->get_ty()) set_to_spot(obj, spot);

		if (check_shape(mx - obj->get_tx(), my - obj->get_ty(),
		                obj->get_shapenum(), obj->get_framenum(), obj->get_shapefile())) {
			return obj;
		}

		return nullptr;
	} else if (spot == quiver && !Get_ammo_frame(obj, container, frame))
		return nullptr;


	int f = item->get_paperdoll_frame(frame);
	if (item->is_gender_based() && (!info->is_npc_female()
	                                && !container->as_actor()->get_type_flag(Actor::tf_sex))) f++;

	if (check_shape(mx - sx, my - sy, item->get_paperdoll_shape(), f, SF_PAPERDOL_VGA)) {
		Shape_frame *shape = obj->get_shape();
		int w = shape->get_width();
		int h = shape->get_height();
		// Set object's position.
		obj->set_shape_pos(mx + shape->get_xleft() - w / 2,
		                   my + shape->get_yabove() - h / 2);

		return obj;
	}

	return nullptr;
}

/*
 *  Checks for object with arms frame
 */
Game_object *Paperdoll_gump::check_object_arms(
    int mx, int my,
    const Paperdoll_npc *info,
    int spot,
    int sx, int sy,
    int start,
    int itemtype
) {
	return check_object(mx, my, info, spot, sx, sy, start + get_arm_type(), itemtype);
}

/*
 *  Checks for the body
 */
bool Paperdoll_gump::check_body(
    int mx, int my,
    const Paperdoll_npc *info
) {
	return check_shape(mx - bodyx, my - bodyy, info->get_body_shape(),
	                   info->get_body_frame(), SF_PAPERDOL_VGA);
}

/*
 *  Checks for the belt
 */
bool Paperdoll_gump::check_belt(
    int mx, int my,
    const Paperdoll_npc *info
) {
	if (info->is_npc_female() || container->as_actor()->get_type_flag(Actor::tf_sex))
		return check_shape(mx - beltfx, my - beltfy, 10, 0, SF_PAPERDOL_VGA);
	else
		return check_shape(mx - beltmx, my - beltmy, 10, 1, SF_PAPERDOL_VGA);

	return false;
}

/*
 *  Checks for the head
 */
bool Paperdoll_gump::check_head(
    int mx, int my,
    const Paperdoll_npc *info
) {
	const Game_object *obj = container->get_readied(head);

	const Paperdoll_item *item = nullptr;
	if (obj)
		item = obj->get_info().get_item_paperdoll(
		           obj->get_framenum(), head);

	int f;
	if (item && item->get_spot_frame())
		f = info->get_head_frame_helm();
	else
		f = info->get_head_frame();

	return check_shape(mx - headx, my - heady,
	                   info->get_head_shape(), f, SF_PAPERDOL_VGA);
}

/*
 *  Checks for the arms
 */
bool Paperdoll_gump::check_arms(
    int mx, int my,
    const Paperdoll_npc *info
) {
	int frnum = info->get_arms_frame(get_arm_type());
	return check_shape(mx - bodyx, my - bodyy,
	                   info->get_arms_shape(), frnum, SF_PAPERDOL_VGA);
}

/*
 *  Generic Shaper checking
 */
bool Paperdoll_gump::check_shape(
    int px, int py,
    int shape, int frame,
    ShapeFile file
) {
	ShapeID sid(shape, frame, file);
	Shape_frame *s = sid.get_shape();

	// If no shape, return
	if (!s) return false;

	Rectangle r = gwin->get_shape_rect(s, 0, 0);

	// If point not in rectangle, return
	if (!r.has_point(px, py)) return false;

	// If point not in shape, return
	if (!s->has_point(px, py)) return false;

	return true;
}

Container_game_object *Paperdoll_gump::find_actor(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	return container;
}

const Paperdoll_npc *Shape_info::get_npc_paperdoll_safe(bool sex) const {
	if (npcpaperdoll)
		return npcpaperdoll;
	int shape = sex ? Shapeinfo_lookup::GetFemaleAvShape() :
	            Shapeinfo_lookup::GetMaleAvShape();
	const Shape_info &inf = ShapeID::get_info(shape);
	return inf.get_npc_paperdoll();
}
