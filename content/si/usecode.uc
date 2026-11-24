/*
 *	Patch for curing Cantra.
 *	Written: 9/20/03.
 *
 *	To use, you need a 'patch' directory alongside 'static' and 'gamedat'
 *	for SerpentIsle.  Then, inside 'patch', run:
 *		ucc -o usecode <this file>
 *	Start up game, and use water of logic on Cantra.
 */
#game "serpentisle"
const int avatar = -356;
const int cantra_id = -64;  // Cantra's NPC #.
const int CURED_CANTRA = 0x47;  // Flag when Cantra is cured.
const int talk_sched = 3;  // Talk schedule.
const int wait_sched = 15;

/*
 *	Cantra.  (I want this to override the original code for her.)
 */
void Cantra object#(0x440) () {
	var cantra = item;
	if (event != 1 || !get_item_flag(0x1e)) {
		Cantra.original();
		return;
	}
	var barks = ["I want thy flesh!", "I want thy blood!", "Blood! Blood everywhere!",
		"How hungry I am!"];
	var i = UI_get_random(UI_get_array_size(barks));
	cantra.say(barks[i]);
	converse (["Hello, Cantra.", "Bye"]) {
	case "Bye":
		break;
	case "Hello, Cantra." (remove):
		cantra.say("Cantra?");
		cantra.say("Was that my name?");
		add(["Yes, you are Cantra!", "Knight..."]);
	case "Yes, you are Cantra!" (remove):
		cantra.say("No. NO!  LIAR!");
		break;
	case "Knight..." (remove):
		cantra.say("Knight?");
		cantra.say("Perhaps... or was that someone else...");
	}
}

extern void Cure_cantra object#(0xa00) ();

/*
 *	This is called when the 'bucket' is used.
 */
void Dump_bucket 0x88a (bucket) {
	var target = UI_click_on_item();
	if (!target) return;
	UI_printf(["The shape clicked on is %s", target->get_item_shape()]);
	var cantra = target[1];  // It's item, position.
	if (cantra != cantra_id->get_npc_object() || bucket->get_item_quality() != 13 ||  // "Logic".
	bucket->get_item_frame() >= 6) {
		// Let orig. function handle it.
		target->set_intercept_item();
		Dump_bucket.original(bucket);
		return;
	}
	cantra->set_schedule_type(wait_sched);
	UI_path_run_usecode([target[2], target[3], target[4]], Cure_cantra, bucket, 10);
}

/*
 *	Called when correct bucket is dumped on Cantra.
 *	Input:	Item = bucket.
 */
void Cure_cantra object#(0xa00) () {
	if (event != 10) return;  // Shouldn't happen.
	UI_close_gumps();
	set_item_frame(0);  // Now empty.
	cantra_id->obj_sprite_effect(7, 0, 0, 0, 0, 0, 0);
	cantra_id->clear_item_flag(0x1e);  // No longer crazy.
	cantra_id->set_schedule_type(talk_sched);
	gflags[CURED_CANTRA] = 1;  // We've done it.
}
