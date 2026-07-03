/*
Copyright (C) 2011-2024 The Exult Team

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

#include "ItemMenu_gump.h"

#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Scroll_gump.h"
#include "Text_button.h"
#include "actors.h"
#include "cheat.h"
#include "exult.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "items.h"
#include "ready.h"
#include "shapeinf.h"
#include "shapeinf/ammoinf.h"
#include "shapeinf/armorinf.h"
#include "shapeinf/weaponinf.h"

#include <sstream>
#include <string>

using Itemmenu_button = CallbackTextButton<Itemmenu_gump>;
using Itemmenu_object = CallbackTextButton<Itemmenu_gump, Game_object*>;
using ObjectParams    = Itemmenu_object::CallbackParams;

class Strings : public GumpStrings {
public:
	static auto ShowInventory() {
		return get_text_msg(0x650 - msg_file_start);
	}

	static auto Talkto() {
		return get_text_msg(0x651 - msg_file_start);
	}

	static auto ShowContents() {
		return get_text_msg(0x652 - msg_file_start);
	}

	static auto Interactwith() {
		return get_text_msg(0x653 - msg_file_start);
	}

	static auto Pickup() {
		return get_text_msg(0x654 - msg_file_start);
	}

	static auto Moveto() {
		return get_text_msg(0x655 - msg_file_start);
	}

	static auto Donothing() {
		return get_text_msg(0x656 - msg_file_start);
	}

	static auto Properties() {
		return get_text_msg(0x657 - msg_file_start);
	}
};

namespace {

	constexpr int potion_shape = 0x154;

	bool has_potion_properties(Game_object* object) {
		return object && object->get_shapenum() == potion_shape && (object->get_framenum() & 31) <= 9;
	}

	bool show_item_properties() {
		std::string enabled;
		config->value("config/gameplay/show_item_properties", enabled, "yes");
		return enabled == "yes";
	}

	const char* get_damage_type_name(int type) {
		switch (type) {
		case Weapon_data::normal_damage:
			return "normal";
		case Weapon_data::fire_damage:
			return "fire";
		case Weapon_data::magic_damage:
			return "magic";
		case Weapon_data::lightning_damage:
			return "lightning/poison/freezing";
		case Weapon_data::ethereal_damage:
			return "ethereal";
		case Weapon_data::sonic_damage:
			return "sonic";
		default:
			return "unknown";
		}
	}

	const char* get_weapon_kind_name(int uses) {
		switch (uses) {
		case Weapon_info::melee:
			return "melee";
		case Weapon_info::poor_thrown:
			return "poor thrown";
		case Weapon_info::good_thrown:
			return "good thrown";
		case Weapon_info::ranged:
			return "ranged";
		default:
			return "unknown";
		}
	}

	const char* get_ready_type_name(int ready) {
		switch (ready) {
		case head:
			return "head";
		case backpack:
			return "backpack";
		case belt:
			return "belt";
		case lhand:
			return "lhand";
		case lfinger:
			return "lfinger";
		case legs:
			return "legs";
		case feet:
			return "feet";
		case rfinger:
			return "rfinger";
		case rhand:
			return "rhand";
		case torso:
			return "torso";
		case amulet:
			return "amulet";
		case quiver:
			return "quiver";
		case back_2h:
			return "back_2h";
		case back_shield:
			return "back_shield";
		case earrings:
			return "earrings";
		case cloak:
			return "cloak";
		case gloves:
			return "gloves";
		case ucont:
			return "ucont";
		case both_hands:
			return "both_hands";
		case lrgloves:
			return "lrgloves";
		case neck:
			return "neck";
		case scabbard:
			return "scabbard";
		case triple_bolts:
			return "triple_bolts";
		default:
			return "unknown";
		}
	}

	void append_power(std::ostringstream& out, bool& first, unsigned char powers, int bit, const char* name) {
		if ((powers & bit) == 0) {
			return;
		}
		if (!first) {
			out << ", ";
		}
		out << name;
		first = false;
	}

	std::string get_power_names(unsigned char powers) {
		if (powers == 0) {
			return "none";
		}
		std::ostringstream out;
		bool               first = true;
		append_power(out, first, powers, Weapon_data::sleep, "sleep");
		append_power(out, first, powers, Weapon_data::charm, "charm");
		append_power(out, first, powers, Weapon_data::curse, "curse");
		append_power(out, first, powers, Weapon_data::poison, "poison");
		append_power(out, first, powers, Weapon_data::paralyze, "paralyze");
		append_power(out, first, powers, Weapon_data::magebane, "magebane");
		append_power(out, first, powers, Weapon_data::unknown, "unknown");
		append_power(out, first, powers, Weapon_data::no_damage, "no damage");
		return out.str();
	}

	const char* get_potion_name(int frame) {
		switch (frame) {
		case 0:
			return "Blue Potion";
		case 1:
			return "Yellow Potion";
		case 2:
			return "Red Potion";
		case 3:
			return "Green Potion";
		case 4:
			return "Orange Potion";
		case 5:
			return "Purple Potion";
		case 6:
			return "White Potion";
		case 7:
			return "Black Potion";
		case 8:
			return "Mana potion";
		case 9:
			return "Warmth potion";
		default:
			return "Unknown potion";
		}
	}

	const char* get_potion_effect(int frame) {
		switch (frame) {
		case 0:
			return "Makes the target fall asleep.";
		case 1:
			return "Restores 3-12 HP.";
		case 2:
			return "Cures poison, paralysis, sleep, charm, and curse.";
		case 3:
			return "Poisons the target.";
		case 4:
			return "Wakes a sleeping target.";
		case 5:
			return "Gives protection.";
		case 6:
			return "Creates light.";
		case 7:
			return "Makes the target invisible.";
		case 8:
			return "Restores up to 10 mana to the Avatar, capped by max mana.";
		case 9:
			return "Sets the target's temperature to normal.";
		default:
			return "Unknown.";
		}
	}

	std::string get_potion_description(Game_object* object) {
		const int   frame = object->get_framenum() & 31;
		std::string description = get_potion_name(frame);
		description += " - ";
		description += get_potion_effect(frame);
		return description;
	}

	const char* get_potion_duration(int frame) {
		switch (frame) {
		case 0:
			return "Until awakened, rest, or another clearing effect";
		case 1:
		case 2:
		case 4:
		case 8:
		case 9:
			return "Instant";
		case 3:
			return "Until cured, rest, or another clearing effect";
		case 5:
		case 7:
			return "Until rest or another clearing effect";
		case 6:
			return "About 10 game minutes";
		default:
			return "Unknown";
		}
	}

	void display_potion_properties(Game_object* object) {
		const int          frame = object->get_framenum() & 31;
		std::ostringstream text;
		text << get_potion_description(object) << "~";
		text << "Duration: " << get_potion_duration(frame) << "~";

		Game_window* gwin = Game_window::get_instance();
		Scroll_gump  scroll;
		scroll.add_text(text.str().c_str());
		scroll.paint();
		do {
			int x;
			int y;
			Get_click(x, y, Mouse::hand, nullptr, false, &scroll);
		} while (scroll.show_next_page());
		gwin->paint();
	}

	void display_weapon_properties(Game_object* object) {
		const Weapon_info* weapon = object->get_info().get_weapon_info();
		if (!weapon) {
			return;
		}
		const Shape_info&   info = object->get_info();
		std::ostringstream  text;
		text << "Name: " << object->get_name() << "~";
		text << "Damage: " << weapon->get_damage() << "~";
		text << "Range: " << weapon->get_range() << "~";
		text << "Speed: " << weapon->get_missile_speed() << "~";
		text << "XP: " << weapon->get_base_xp_value() << "~";
		text << "Damage Type: " << get_damage_type_name(weapon->get_damage_type()) << "~";
		text << "Powers: " << get_power_names(weapon->get_powers()) << "~";
		text << "Kind of Weapon: " << get_weapon_kind_name(weapon->get_uses()) << "~";
		text << "Weight: " << info.get_weight() << "~";
		text << "Volume: " << info.get_volume() << "~";
		text << "ReadyType: " << get_ready_type_name(info.get_ready_type()) << "~";

		Game_window* gwin = Game_window::get_instance();
		Scroll_gump  scroll;
		scroll.add_text(text.str().c_str());
		scroll.paint();
		do {
			int x;
			int y;
			Get_click(x, y, Mouse::hand, nullptr, false, &scroll);
		} while (scroll.show_next_page());
		gwin->paint();
	}

	void display_armor_properties(Game_object* object) {
		const Armor_info* armor = object->get_info().get_armor_info();
		if (!armor) {
			return;
		}
		const Shape_info&  info = object->get_info();
		std::ostringstream text;
		text << "Name: " << object->get_name() << "~";
		text << "Prot: " << static_cast<int>(armor->get_prot()) << "~";
		text << "XP: " << armor->get_base_xp_value() << "~";
		text << "Weight: " << info.get_weight() << "~";
		text << "Volume: " << info.get_volume() << "~";
		text << "ReadyType: " << get_ready_type_name(info.get_ready_type()) << "~";

		Game_window* gwin = Game_window::get_instance();
		Scroll_gump  scroll;
		scroll.add_text(text.str().c_str());
		scroll.paint();
		do {
			int x;
			int y;
			Get_click(x, y, Mouse::hand, nullptr, false, &scroll);
		} while (scroll.show_next_page());
		gwin->paint();
	}

	void display_ammo_properties(Game_object* object) {
		const Ammo_info* ammo = object->get_info().get_ammo_info();
		if (!ammo) {
			return;
		}
		const Shape_info&  info = object->get_info();
		std::ostringstream text;
		text << "Name: " << object->get_name() << "~";
		text << "Damage: " << ammo->get_damage() << "~";
		text << "DamageType: " << get_damage_type_name(ammo->get_damage_type()) << "~";
		text << "Powers: " << get_power_names(ammo->get_powers()) << "~";
		text << "IsSpell: " << (info.is_spell() ? "true" : "false") << "~";
		text << "ReadyType: " << get_ready_type_name(info.get_ready_type()) << "~";

		Game_window* gwin = Game_window::get_instance();
		Scroll_gump  scroll;
		scroll.add_text(text.str().c_str());
		scroll.paint();
		do {
			int x;
			int y;
			Get_click(x, y, Mouse::hand, nullptr, false, &scroll);
		} while (scroll.show_next_page());
		gwin->paint();
	}

}    // namespace

void Itemmenu_gump::select_object(Game_object* obj) {
	objectSelected = obj;
	auto it        = objects.find(obj);
	assert(it != objects.cend());
	objectSelectedClickXY = it->second;
	close();
}

int clamp(int val, int low, int high) {
	assert(!(high < low));
	if (val < low) {
		return low;
	}
	if (high < val) {
		return high;
	}
	return val;
}

void Itemmenu_gump::fix_position(int num_elements) {
	const int w           = Game_window::get_instance()->get_width();
	const int h           = Game_window::get_instance()->get_height();
	const int menu_height = clamp(num_elements * button_spacing_y, 0, h);
	x                     = clamp(x, 0, w - 100);
	y                     = clamp(y, 0, h - menu_height);
}

Itemmenu_gump::Itemmenu_gump(Game_object_map_xy* mobjxy, int cx, int cy)
		: Modal_gump(nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP, SF_EXULT_FLX) {
	objectSelected        = nullptr;
	objectSelectedClickXY = {-1, -1};
	objectAction          = no_action;
	// set_object_area(TileRect(0, 0, 0, 0), -1, -1);//++++++ ???
	int       btop = 0;
	const int maxh = Game_window::get_instance()->get_height() - 2 * button_spacing_y;
	Game_object* ammoPropertiesObject   = nullptr;
	Game_object* armorPropertiesObject  = nullptr;
	Game_object* potionPropertiesObject = nullptr;
	Game_object* weaponPropertiesObject = nullptr;
	int          ammoPropertiesCount    = 0;
	int          armorPropertiesCount   = 0;
	int          potionPropertiesCount  = 0;
	int          weaponPropertiesCount  = 0;
	const bool   showProperties         = show_item_properties();
	for (auto it = mobjxy->begin(); it != mobjxy->end() && btop < maxh; it++) {
		Game_object* o    = it->first;
		std::string  name = o->get_name();
		if (showProperties && has_potion_properties(o)) {
			name = get_potion_description(o);
		}
		// Skip objects with no name.
		if (name.empty()) {
			continue;
		}
		objects[o] = it->second;
		buttons.push_back(
				std::make_unique<Itemmenu_object>(this, &Itemmenu_gump::select_object, ObjectParams{o}, name, 10, btop, 59, 20));
		btop += button_spacing_y;
		if (showProperties) {
			if (o->get_info().has_weapon_info()) {
				weaponPropertiesObject = o;
				++weaponPropertiesCount;
			}
			if (o->get_info().has_armor_info()) {
				armorPropertiesObject = o;
				++armorPropertiesCount;
			}
			if (o->get_info().has_ammo_info()) {
				ammoPropertiesObject = o;
				++ammoPropertiesCount;
			}
			if (has_potion_properties(o)) {
				potionPropertiesObject = o;
				++potionPropertiesCount;
			}
		}
	}
	if (objects.size() == 1
		&& (weaponPropertiesCount == 1 || armorPropertiesCount == 1 || ammoPropertiesCount == 1
			|| potionPropertiesCount == 1)) {
		Game_object* propertiesObject
				= weaponPropertiesCount == 1
						  ? weaponPropertiesObject
						  : (armorPropertiesCount == 1
									 ? armorPropertiesObject
									 : (ammoPropertiesCount == 1 ? ammoPropertiesObject : potionPropertiesObject));
		buttons.push_back(std::make_unique<Itemmenu_object>(
				this, &Itemmenu_gump::select_weapon_properties, ObjectParams{propertiesObject}, Strings::Properties(),
				10, btop, 59, 20));
		btop += button_spacing_y;
	}
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::cancel_menu, Strings::Cancel(), 10, btop, 59, 20));
	fix_position(buttons.size());
}

Itemmenu_gump::Itemmenu_gump(Game_object* obj, int ox, int oy, int cx, int cy)
		: Modal_gump(nullptr, cx, cy, EXULT_FLX_TRANSPARENTMENU_SHP, SF_EXULT_FLX) {
	// Ths gump cannot be dragged at this time
	no_dragging = true;

	objectSelected                                  = obj;
	objectAction                                    = item_menu;
	objectSelectedClickXY                           = {ox, oy};
	int                           btop              = 0;
	const Shape_info&             info              = objectSelected->get_info();
	const Shape_info::Shape_class cls               = info.get_shape_class();
	const bool                    is_npc_or_monster = cls == Shape_info::human || cls == Shape_info::monster;
	const bool                    in_party          = objectSelected->get_flag(Obj_flags::in_party);
	if (in_party || (is_npc_or_monster && cheat.in_pickpocket())) {
		buttons.push_back(
				std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_inventory, Strings::ShowInventory(), 10, btop, 59, 20));
		btop += button_spacing_y;
	}
	const bool is_avatar = objectSelected == gwin->get_main_actor();
	if (!is_avatar
		&& ((is_npc_or_monster && !cheat.in_pickpocket()) || (cls == Shape_info::container && !info.is_container_locked())
			|| objectSelected->usecode_exists())) {
		std::string useText;
		if (is_npc_or_monster) {
			useText = Strings::Talkto();
		} else if (cls == Shape_info::container) {
			useText = Strings::ShowContents();
		} else {
			useText = Strings::Interactwith();
		}
		buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_use, useText, 10, btop, 59, 20));
		btop += button_spacing_y;
	}
	if (cheat.in_hack_mover() || objectSelected->is_dragable()) {
		buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_pickup, Strings::Pickup(), 10, btop, 59, 20));
		btop += button_spacing_y;
		buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_move, Strings::Moveto(), 10, btop, 59, 20));
		btop += button_spacing_y;
	}
	if (show_item_properties()
		&& (info.has_weapon_info() || info.has_armor_info() || info.has_ammo_info()
			|| has_potion_properties(objectSelected))) {
		buttons.push_back(
				std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::set_weapon_properties, Strings::Properties(), 10, btop, 59, 20));
		btop += button_spacing_y;
	}
	buttons.push_back(std::make_unique<Itemmenu_button>(this, &Itemmenu_gump::cancel_menu, Strings::Donothing(), 10, btop, 59, 20));
	fix_position(buttons.size());
}

Itemmenu_gump::~Itemmenu_gump() {
	postCloseActions();
}

void Itemmenu_gump::paint() {
	for (auto& objPos : objects) {
		const auto& obj = objPos.first;
		obj->paint_outline(CHARMED_PIXEL);
	}
	if (objectSelected) {
		objectSelected->paint_outline(PROTECT_PIXEL);
	}
	Modal_gump::paint();
	for (auto& btn : buttons) {
		btn->paint();
	}
	gwin->set_painted();
}

bool Itemmenu_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button != MouseButton::Left && button != MouseButton::Right) {
		return false;
	}
	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}
	// First try checkmark
	pushed = Gump::on_button(mx, my);
	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}
	// On a button?
	if (pushed && !pushed->push(button)) {
		pushed = nullptr;
	}
	return button == MouseButton::Left || pushed != nullptr;
}

bool Itemmenu_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		close();
		return false;
	}
	if (pushed->get_pushed() != button) {
		return button == MouseButton::Left;
	}
	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res;
}

void Itemmenu_gump::postCloseActions() {
	if (!objectSelected) {
		return;
	}
	Game_window* gwin = Game_window::get_instance();
	switch (objectAction) {
	case show_inventory: {
		auto* act = objectSelected->as_actor();
		if (act != nullptr) {
			act->show_inventory();
		}
		break;
	}
	case use_item:
		objectSelected->activate();
		break;
	case show_weapon_properties:
		if (objectSelected->get_info().has_weapon_info()) {
			display_weapon_properties(objectSelected);
		} else if (objectSelected->get_info().has_armor_info()) {
			display_armor_properties(objectSelected);
		} else if (objectSelected->get_info().has_ammo_info()) {
			display_ammo_properties(objectSelected);
		} else if (has_potion_properties(objectSelected)) {
			display_potion_properties(objectSelected);
		}
		break;
	case pickup_item: {
		Main_actor*      ava    = gwin->get_main_actor();
		const Tile_coord avaLoc = ava->get_tile();
		const int        avaX   = (avaLoc.tx - gwin->get_scrolltx()) * c_tilesize;
		const int        avaY   = (avaLoc.ty - gwin->get_scrollty()) * c_tilesize;
		auto*            tmpObj = gwin->find_object(avaX, avaY);
		if (tmpObj != ava) {
			// Avatar isn't in a good spot...
			// Let's give up for now :(
			break;
		}
		if (gwin->start_dragging(objectSelectedClickXY.x, objectSelectedClickXY.y) && gwin->drag(avaX, avaY)) {
			gwin->drop_dragged(avaX, avaY, true);
		}
		break;
	}
	case move_item: {
		int tmpX;
		int tmpY;
		if (Get_click(tmpX, tmpY, Mouse::greenselect, nullptr, true)
			&& gwin->start_dragging(objectSelectedClickXY.x, objectSelectedClickXY.y) && gwin->drag(tmpX, tmpY)) {
			gwin->drop_dragged(tmpX, tmpY, true);
		}
		break;
	}
	case no_action: {
		// Make sure menu is visible on the screen
		// This will draw a selection menu for the object
		Itemmenu_gump itemgump(objectSelected, objectSelectedClickXY.x, objectSelectedClickXY.y, x, y);
		gwin->get_gump_man()->do_modal_gump(&itemgump, Mouse::hand);
		break;
	}
	case item_menu:
		break;
	}
}
