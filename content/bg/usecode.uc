/*
 *	Patch for Black Gate.
 *	Written: 5/6/04.
 *
 *	To use, you need a 'patch' directory alongside 'static' and 'gamedat'
 *	for BlackGate.  Then, inside 'patch', run:
 *		ucc -o usecode <this file>
 */

#game "blackgate"

const int avatar = -356;
const int lordbrit_id = -23;		// LB's NPC #.
extern var Ask_yes_no 0x90a();

enum item_flags {			// Bit #'s of flags:
		invisible = 0,
		asleep = 1,
		charmed = 2,
		cursed = 3,
		dead = 4,
		in_party = 6,		// Guess, appears to be correct
		paralyzed = 7,
		poisoned = 8,
		protection = 9,
		on_moving_barge = 10,	// ??Guessing.
		okay_to_take = 11,	// Okay to take??
		might = 12,		// Double strength, dext, intel.
		no_spell_casting = 13,
		cant_die = 14,		// Test flag in Monster_info.
		in_action = 15,		// If the NPC is doing something. Set by "Dance" spell in BG.
		dont_move = 16,		// User can't move.  In BG,
					//   completely invisible.
		si_on_moving_barge = 17,// SI's version of 10?
		is_temporary = 18,	// Is temporary
		okay_to_land = 21,	// Used for flying-carpet.
		in_dungeon = 23,	// Pretty sure.  If set, you won't
					//   be accused of stealing food.
		confused = 25,		// ??Guessing.
		in_motion = 26,		// ??Guessing (cart, boat)??
		met = 28,			// Has the npc been met
		si_tournament = 29,	// SI-Call usecode (eventid=7)
		si_zombie = 30,		// Used for sick Neyobi.
		// Flags > 31
		polymorph = 32,		// SI.  Pretty sure about this.
		tattooed = 33,			// Guess (SI).
		read = 34,			// Guess (SI).
		petra = 35,			// Guess
		freeze = 37		// SI.  Pretty sure.
	};

/*
 *	Example to enable LB to join your party.
 */
void LB_fun object#(0x417) ()
	{
	var inparty = get_item_flag(in_party);
	static var count;

	LB_fun.original();
	count += 1;
	if (!inparty)
		{
		if (count%3 != 1)
			return;		// Just do it every 3rd time.
		if (UI_get_array_size(UI_get_party_list()) > 7)
			return;		// No room.
		item.say("One moment, my long-time friend...");
		say("My years here as sovereign have been pleasant, ",
		    "but I feel my bones growing soft.  ",
		    "Some days I yearn for our old times of adventure.");
		say("Though my hair hast a bit of grey, my hand is ",
		    "steady, and my eyes sharp.");
		say("Couldst thou find a place for me on your Quest?");
		if (Ask_yes_no())
			{
			add_to_party();
			count = 0;
			}
		}
	else
		{
		if (count%3 != 1)
			return;
		item.say("Mayest I continue to aid thee in thy quest, Avatar?");
		if (!Ask_yes_no())
			{
			remove_from_party();
			set_schedule_type(11);	// Loiter.
			count = 0;
			}
		}
	}
