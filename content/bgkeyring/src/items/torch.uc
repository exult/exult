const int SHAPE_TORCH		= 595;
const int SHAPE_LIT_TORCH	= 701;
const int SHAPE_LIT_LIGHT_SOURCE = 0x152;
const int SHAPE_SPENT_LIGHT_SOURCE = 0x3E5;
const int SHAPE_LIT_SCONCE = 0x1B3;
const int SHAPE_SPENT_SCONCE = 0x217;

const int SOUND_SPEND_LIGHT = 0x6A;

extern void startLight 0x905 (var obj);
extern void useLight object#(0x600) ();
extern var directionFrom 0x827 (var from, var to_);

void Torch shape#(0x253) () {
	if (event != DOUBLECLICK && event != SCRIPTED) {
		return;
	}
	LightTorch(item, SHAPE_LIT_TORCH);
}

void LitTorch shape#(0x2BD)() {
	var var0000;
	var var0001;
	var var0002;

	if (event == DOUBLECLICK || event == SCRIPTED) {
		DouseTorch(item, SHAPE_TORCH, event);
	} else if (event == 7) {
		script AVATAR {
			face directionFrom(AVATAR, item);
			actor frame 9;
			actor frame 0;
		}
		DouseTorch(item, SHAPE_TORCH, event);
	} else if (event == 5) {
		startLight(item);
	}
}


void DouseTorch 0x839 (var eventid, var shp, var obj) {
	if(eventid == DOUBLECLICK || eventid == SCRIPTED) {
		obj->set_item_shape(shp);
		obj->halt_scheduled();
		UI_play_sound_effect(0x002E);
		set_light(true);
	} else if (eventid == 7) {
		obj->set_item_shape(shp);
		obj->halt_scheduled();
		UI_play_sound_effect(0x002E);
		set_light(true);
	} else if (eventid == 5) {
		set_light(true);
	} else if (eventid == 6) {
		set_light(false);
	}
}


void LightTorch 0x942 (var shp, var obj) {
	var framenum;
	var container;

	framenum = obj->get_item_frame();
	if (!obj->get_item_quality()) {
		obj->set_item_quality(UI_die_roll(30, 60));
	}

	if (obj->get_item_shape() == SHAPE_TORCH) {
		if (obj->get_item_quality() == 255) {
			obj->item_say("Spent");
			return;
		}
	}

	container = obj->get_container();
	if (container == 0 || container->is_npc()) {
		set_item_shape(shp);
		if (container in UI_get_party_list()) {
			startLight(obj);
		}
	} else {
		UI_flash_mouse(0);
	}

	obj->set_light(true);
	UI_set_time_palette();
	return;
}


void startLight 0x905 (var obj) {
	obj->halt_scheduled();
	set_light(true);
	UI_set_time_palette();
	script obj after 50 ticks {
		nohalt;
		call useLight;
	}
}


void useLight object#(0x600) () {
	var reduced_quality	= get_item_quality() - 1;
	var new_quality		= set_item_quality(reduced_quality);
	if (reduced_quality == 0) {
		halt_scheduled();
		var shp = get_item_shape();

		if (shp == SHAPE_LIT_LIGHT_SOURCE) {
			set_item_shape(SHAPE_SPENT_LIGHT_SOURCE);
		} else if (shp == SHAPE_LIT_TORCH) {
			set_item_shape(SHAPE_TORCH);
			new_quality = set_item_quality(255);
		} else if (shp == SHAPE_LIT_SCONCE) {
			set_item_shape(SHAPE_SPENT_SCONCE);
		}
		UI_play_sound_effect(SOUND_SPEND_LIGHT);
	} else {
		script item after 50 ticks call useLight;
	}
}
