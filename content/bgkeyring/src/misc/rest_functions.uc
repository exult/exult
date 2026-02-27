EnsureExtinguishHeldTorch () {
	//Prevent torch from further burning out:
	halt_scheduled();
	//Set it to spent:
	set_item_quality(QUALITY_TORCH_SPENT);
}

//Reimplemented to account for NPC "mana":
advanceTimeDueToRest 0x93A (var sleep_time, var itemref) {
	var party;
	var counter;
	var party_count;
	var member;
	var light_sources = [];
	var source_count;
	var light_obj;
	var obj_quality;
	var obj_shape;
	var obj_count;

	party = UI_get_party_list2();

	for (member in party with counter to party_count) {
		//The party member gets nothing from rest if he is starving:
		if (member->get_npc_prop(FOODLEVEL) >= 10) {
			//If the member is the Avatar, clear sleeping flag:
			if (!(member == AVATAR->get_npc_object())) {
				member->clear_item_flag(ASLEEP);
			}

			//Clear flags that go away with time:
			member->clear_item_flag(POISONED);
			member->clear_item_flag(PARALYZED);
			member->clear_item_flag(CURSED);
			member->clear_item_flag(CHARMED);
			member->clear_item_flag(INVISIBLE);
			member->clear_item_flag(PROTECTION);

			//Recover Health and mana:
			recoverPropByRest(member, HEALTH, STRENGTH, sleep_time);
			recoverPropByRest(member, MANA, MAX_MANA, sleep_time);

			//Make the NPC hungrier; why food level isn't reduced if
			//the NPC is already starving???
			member->set_npc_prop(FOODLEVEL, (sleep_time * -1));
		}

		//Gather readied light sources and torches:
		light_sources &= member->get_cont_items(SHAPE_TORCH_LIT, QUALITY_ANY, FRAME_ANY);
		light_sources &= member->get_cont_items(SHAPE_LIGHTSOURCE_LIT, QUALITY_ANY, FRAME_ANY);
	}

	//Gather all nearby torches and light sources:
	light_sources &= itemref->find_nearby(SHAPE_TORCH_LIT, 30, 0);
	light_sources &= itemref->find_nearby(SHAPE_LIGHTSOURCE_LIT, 30, 0);
	//Unfortunately, the original left out all readied light sources and torches, which means
	//a torch wouldn't go out if you were holding it...

	//Advance time for each one:
	for (light_obj in light_sources with counter to obj_count) {
		obj_quality = light_obj->get_item_quality();

		//If the quality is below a certain threshold, the light source
		//would be spent due to the passing time:
		if (obj_quality <= (sleep_time * 30)) {
			//Cancel all scripts without nohalt for the light source:
			light_obj->halt_scheduled();
			obj_shape = light_obj->get_item_shape();

			//Actually extinguish the light source, and set if to spent:
			if (obj_shape == SHAPE_LIGHTSOURCE_LIT) {
				light_obj->set_item_shape(SHAPE_LIGHTSOURCE_SPENT);
			} else if (obj_shape == SHAPE_TORCH_LIT) {
				light_obj->set_item_shape(SHAPE_TORCH);

				//This (combined with UI_halt_scheduled, above) should
				//be enough to extinguish all torches... but it is not.
				//The reason is that, when you ignite a torch and ready
				//it, the first script to run has nohalt in it... this
				//causes the quality to be decremented once or twice
				//during the sleep period from QUALITY_TORCH_SPENT...
				light_obj->set_item_quality(QUALITY_TORCH_SPENT);

				//Therefore, this 'hack' ensures that their quality is
				//indeed what it should be:
				script light_obj after 35 ticks call EnsureExtinguishHeldTorch;
				//Another possibility would be to edit the beginLightSourceEvolution
				//function (0x905) to remove nohalt from the script.

				//It would have been much better if torches also had a
				//"spent" shape like light sources do...
			}
		} else {
			//Otherwise, just reduce the quality by an appropriate amount:
			light_obj->set_item_quality((obj_quality - (sleep_time * 30)));
		}
	}
}
