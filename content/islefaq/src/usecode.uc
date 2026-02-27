#game "blackgate"

/*
 *	Global flags, assuming 1000- are free.
 */
enum Isleflags
	{
	TALKED_DRCODE = 1000,
	ASKED_ABOUT_PYRO,
	ASKED_WHERE_PYRO,
	ASKED_KNOW_PYRO,
	LOST_FAQ,			// Talked to Dom about lost FAQ.
	WILL_FIND_FAQ,
	RABBIT_TRACKS,
	CHURCH_CARROTS,
	RETURNED_FAQ
	};

/*
 *	Item flags.  ++++These should be in an 'include' file.
 */
const int IN_PARTY = 6;
const int IF_MET = 28;

/*
 *	Existing functions in BG.
 */
extern var Ask_yesno 0x90a();		// Returns true if 'Yes', false if 'No.
extern void Add_experience 0x911(var incr);	// Add exper. to each party member.

/*
 *	NPC #'s:
 */
const int AMY = -349;
// Amy doesn't have her own face so AMY.say would show none
const int AMYFACE = -221;
const int AVATAR = -356;

/*
 *	Egg on island created just E. of Trinsic.
 */

void new_island_egg0 object#(0x740) ()
	{
					// Get random NPC (or Avatar).
	var party = UI_get_party_list();
	var party_cnt = UI_get_array_size(party);
	var npc;
	if (party_cnt > 1)		// Companions?
		npc = party[1 + UI_get_random(party_cnt - 1)];
	else
		npc = AVATAR;
	if (npc)
		{
		npc.say("Do I detect...~... the smell of Usecode?");
		npc.hide();
		}
	}

/*
 *	Egg behind church by the carrots.
 */

void new_island_egg1 object#(0x741) ()
	{
	if (!gflags[CHURCH_CARROTS])
		{
		AVATAR.say("Mmmm... They do make a tasty snack.");
		return;
		}
	if (!UI_get_item_flag(AMY, IN_PARTY))
		{
		AVATAR.say("There's something hidden here, but I cannot find it. If only Amy were here.");
		return;
		}
					// Create book.
	var faq = UI_create_new_object(0x282);
	faq->set_item_quality(0x88);	// BOOK OF CIRCLES.  Good enough...
					// Place on top of egg.
	UI_update_last_created(get_object_position());
	remove_item();		// Done with this egg.
	AMYFACE.say("Look!  There appears to be a book here!");
	AVATAR.say("Can it be...");
	AMYFACE.say("Yes!  It is!  The lost FAQ!");
	AVATAR.hide();
	AMYFACE.hide();
	}

void Random_barks 0xC00 (var barks)
	{
	if (get_schedule_type() == 14)
				// Sleeping.
		item_say("Zzzz...");
	else
		{
		barks = [barks];
		var n = UI_get_random(UI_get_array_size(barks));
		item_say(barks[n]);
		}
	}

/*
 *	'DrCode' on island.
 */
void DrCode object#(0x564) ()
	{
	if (event == 0)
		{
		item_say("Someone say 'usecode'??");
		return;
		}
	else if (event != 1)
		return;
	var answers;
	if (gflags[TALKED_DRCODE])
		{
		item.say("I knew you would return!");
		answers = "How??";
		}
	else
		{
		item.say("Hello!  How may I help you?");
		gflags[TALKED_DRCODE] = true;
		answers = ["Name", "Job"];
		}
	converse ([answers, "Bye"])
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm DrCode");
		case "Job" (remove):
			say("I search...");
			add("Search for what?");
		case "Search for what?" (remove):
			say("... for Usecode!!");
			add("Usecode");
		case "How??" (remove):
			say("I felt a great disturbance in the Usecode...");
			if (gflags[WILL_FIND_FAQ] &&
			    AMY->get_npc_object() in UI_get_party_list())
				{
				AMYFACE.say("Say, Dr. Code...~" +
				        "You certainly have a lot of papers and books strewn about");
				item.say("Er, yes, I suppose I do.");
				AMYFACE.say("Are you sure the FAQ isn't somewhere amongst them?");
				AMYFACE.hide();
				item.say("Dr.Code looks away from Amy as he hides his shaking hands in his pockets.");
				add("FAQ");
				}
		case "Usecode" (remove):
			say("Usecode is a mythical force.~" +
			    "Some believe it controls the fate of all in our world. " +
			    "And to control it would give one ultimate power.");
			add("Power");
		case "Power":
			say("It MUST not fall into evil hands.~" +
			    "I can not let it...~" +
			    "No!  NO!~NOOooooooo!!");
						// Put in combat/flee mode.
			set_attack_mode(7);
			set_schedule_type(0);
			break;
		case "FAQ" (remove):
			say("I don't know what you are talking about!");
			AMYFACE.say("Are you sure?");
			AMYFACE.hide();
			say("YES!  I can stand the guilt no more!");
			say("But... I only meant to borrow it.  I, er, hoped that it " +
			    "would help me with my research into Usecode.");
			say("Dominik was asleep, and I planned to return it before he awoke");
			AVATAR.say("May we have it back then?");
			AVATAR.hide();
			say("I'm afraid to tell you...~...it's gone!");
		}
	item.hide();
	}

/*
 *	'Dominik' on island.
 */
void Dominik object#(0x565) ()
	{
	if (event == 0)
		{
		Random_barks(["...xml",
		              "Damn mouse!",
		              "...sound font...",
		              "...data dir"]);
		return;
		}
	if (event != 1)
		return;
	item.say("I'm very busy, so please be brisk!");
	var answers;
	if (gflags[ASKED_ABOUT_PYRO])
		answers = ["Name", "Job", "Where is Pyro-X?"];
	else
		answers = ["Name", "Job"];
					// Find "FAQ" in party.
	var faq = UI_find_object(-357, 0x282, 0x88, -359);
	if (faq)
		answers = [answers, "Found the FAQ"];
	answers = [answers, "Bye"];
	var faqcnt = 0;
	converse (answers)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("Dominik");
		case "Job":
			faqcnt += 1;
			if (faqcnt == 1)
				{
				say("Please see the FAQ.");
				add("FAQ");
				}
			else if (faqcnt == 2)
				say("I said:  See the FAQ.");
			else if (faqcnt >= 3)
				say("I've told you " + (faqcnt - 1) +
				    " times already...~SEE THE FAQ!!!");
		case "FAQ" (remove):
			say("All of life's answers may be found there.~Though... there are some who think otherwise.");
			add("Otherwise?");
			if (!gflags[LOST_FAQ])
				add("May I see it?");
			else if (!gflags[WILL_FIND_FAQ])
				add("Find");
		case "Otherwise?" (remove):
			say("There is a crackpot who wanders this island. " +
			    "He calls himself 'DrCode', and mutters incessantly about something called 'Usecode'");
			add("Usecode?");
		case "Usecode?" (remove):
			say("Don't bother me with that silly prattle! " +
			    "I must work on the FAQ.~Begone!");
			break;
		case "May I see it?" (remove):
			gflags[LOST_FAQ] = true;
			say("Yes, of course.");
			say("You wait while he looks on his desk.");
			say("You wait a bit more while he hastily pulls out the drawers and peers in each.");
			say("Finally, he turns back, looking markedly upset.");
			say("It's gone!!  Where can it be?~He turns to you.");
			say("Avatar!  It's vital that the FAQ be recovered. Our users will be lost without it!");
			say("Would you help me recover it?");
			if (Ask_yesno())
				{
				gflags[WILL_FIND_FAQ] = true;
				say("Thank you, Avatar! All Britannia awaits the recovery of this vital document.");
				}
			else
				say("Woe to us!  Our efforts are doomed.");
		case "Find" (remove):
			gflags[WILL_FIND_FAQ] = true;
			say("I KNEW you would reconsider. Thank you, Avatar!");
		case "Found the FAQ" (remove):
			AVATAR.say("Thanks to Amy's sharp eyes, we have found the missing FAQ.");
			AVATAR.hide();
			faq->remove_item();
			item.say("At last, I can rest, assured that our users' questions shall be answered.");
			Add_experience(20);
			say("And, Avatar... I believe the presence of the FAQ may even have increased thy knowledge!");
			AVATAR->set_npc_prop(2, 3);	// Gain 3 intel. pts.
			gflags[RETURNED_FAQ] = true;
		case "Where is Pyro-X?" (remove):
			say("As I wrote in the FAQ, you have to ask Colourless!");
			add("Who is Colourless?");
			gflags[ASKED_WHERE_PYRO] = true;
		case "Who is Colourless?" (remove):
			say("Read the FAQ!!!");
		}
	item.hide();
	}
/*
 *	'Willem' on island.
 */
void Willem object#(0x566) ()
	{
	if (event == 0)
		{
		Random_barks(["...Pentagram",
		              "...Pagan..",
		              "another world"]);
		return;
		}
	if (event != 1)
		return;
	item.say("Hello Avatar!");
	converse (["Name", "Job", "Bye"])
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm Willem");
		case "Job":
			say("I work on Pentagram!");
			add("What is Pentagram?");
		case "What is Pentagram?" (remove):
			say("Pentagram is a magical device that lets you travel to another world. To the world of Pagan!");
			add("Tell me about Pagan");
		case "Tell me about Pagan" (remove):
			say("You will find out soon enough. Believe me!");
		}
	item.hide();
	}
/*
 *	'Fingolfin' on island.
 */
void Fingolfin object#(0x567) ()
	{
	if (event == 0)
		{
		Random_barks(["...OSX",
		              "...Pentagram..",
		              "SDL"]);
		return;
		}
	if (event != 1)
		return;
	item.say("Hello Avatar!");
	converse (["Name", "Job", "Bye"])
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("My name is Fingolfin");
		case "Job":
			say("I work on Pentagram!");
			add("What is Pentagram?");
		case "What is Pentagram?" (remove):
			say("You should ask Willem about this");
		}
	item.hide();
	}

/*
 *	'Amy' on island.
 */
void Amy object#(0x568) ()
	{
	if (event == 0)
		{
		Random_barks(["...Studio",
		              "...Time warp..",
		              "Information"]);
		return;
		}
	if (event == 2)			// Looking in bushes by Nadir.
		{
		item.say("Avatar!");
		AVATAR.say("Yes?  What hast thou found?");
		item.say("There are some strange marks on the ground.");
		say("Perhaps they were made by a rabbit.");
		gflags[RABBIT_TRACKS] = true;
		AVATAR.hide();
		item.hide();
		return;
		}
	if (event != 1)
		return;
	if (!get_item_flag(IF_MET))
		{			// First time.
		item.say("You see a young lady with blonde hair.  She looks a bit annoyed.");
		say("And what are you after?");
		}
	else
		item.say("Hello again Avatar!");
	var answers = "Name";
	var party = UI_get_party_list();
	answers << "Job";
	if (item in party)
		answers << "Leave";
	answers << "Bye";
	converse (answers)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm Amy.");
		case "Job" (remove):
			say("I don't have a job. I'm just hanging out here and ",
			    "give information about the people on this island");
			if (item in party)
				add(["Island?", "Who lives here?"]);
			else
				add(["Island?", "Who lives here?", "Are you happy here?"]);
		case "Island?" (remove):
			say("It is called SourceForge Island. To remember the smithy who provided us with our tools");
		case "Are you happy here?" (remove):
			say("It's okay...~...but I'd rather be more useful");
			converse (["Join", "Good luck"])
				{
				case "Join":
					if (gflags[WILL_FIND_FAQ])
						{
						say("Ah, yes!  I hear you are searching for something precious that was lost.",
						    "  Perhaps I can prove to my fellow islanders that I'm not just a pretty face.");
						add_to_party();
						}
					else
						{
						say("But you have not work for me! All you do is wander aimlessly.");
						}
					break;
				case "Good luck":
					break;
				}
		case "Leave" (remove):
			remove_from_party();
			say("Goodbye, for now, Avatar");
			break;
		case "Who lives here?" (remove):
			say("Of whom do you want to speak?");
			converse (["Dr.Code",
			           "Willem",
			           "Colourless",
			           "Fingolfin",
			           "Darke",
			           "Dominus",
			           "EsBee-Ex",
			           "Nobody"])
				{
				case "Dr.Code" (remove):
					say("He's the master of this group. Unfortunately he got mad.");
				case "Willem" (remove):
					say("Some people refer to him as Arthuris Dragon or wjp. " +
					    "He's working on some strange magical device.");
				case "Colourless" (remove):
					say("She's also a member of that mystical group called The Dragons. " +
					    "Among other things she works with Willem on this magical device.");
				case "Fingolfin" (remove):
					say("If he's not working on other things he helps other members of this group.");
				case "Darke" (remove):
					say("Darke thinks he is a dangerous rabbit but he's just a cute bunny. " +
					    "Unfortunately he is also getting slowly as mad as Dr.Code.");
				case "Dominus" (remove):
					say("Don't bother talking to him. " +
					    "He's always referring to some book called FAQ which no one ever read.");
				case "EsBee-Ex" (remove):
					say("EsBee-Ex is a gargoyle. He believes his twin brother is evil.");
				case "Nobody":
					break;
				}
		}
	item.hide();
	}

/*
 *	'Colourless' on island.
 */
void Colourless object#(0x569) ()
	{
	if (event == 0)
		{
		Random_barks(["...glide",
		              "Winsockets?",
		              "Colorless?",
		              "...hack"]);
		return;
		}
	if (event != 1)
		return;
	item.say("You shouldn't be able to see me");
	add(["Name", "Job"]);
	if (gflags[ASKED_WHERE_PYRO])
		add(["Do you know Pyro-X?"]);
	add("Bye");
	converse (0)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm Colourless");
		case "Job":
			say("I work with Darke, Willem and Fingolfin on Pentagram.");
			add("Pentagram?");
		case "Pentagram?" (remove):
			say("You better ask Willem about this.");
			add("Willem?");
		case "Willem?" (remove):
			say("He's somewhere around here.");
		case "Do you know Pyro-X?" (remove):
			say("Yes, I know him. He's responsible for many explosions in Britannia. " +
			    "To hide this he calls himself EsBee-Ex and says all those crimes were " +
			    "committed by his so called Evil Twin,");
			gflags[ASKED_KNOW_PYRO] = true;
		}
	item.hide();
	}

/*
 *	'Darke' on island.
 */
void Darke object#(0x56A) ()
	{
	if (event == 0)
		{
		Random_barks(["...usecode",
		              "...xml..",
		              "bowfluff",
		              "...fluff"]);
		return;
		}
	if (event != 1)
		return;
	item.say("Beware of my sharp teeth!");
	add(["Sharp teeth?", "Name", "Job"]);
	if (gflags[WILL_FIND_FAQ])
		add("FAQ");
	add("Bye");
	converse (0)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm Darke");
		case "Job":
			say("Annoying dragons, of course. And sometimes I make USECODE");
			add(["Dragons?", "Usecode?"]);
		case "Sharp teeth?" (remove):
			say("I'm a cute bunny with sharp teeth. Don't anger me!");
		case "FAQ":
			say("I'm frequently asked that question!");
			if (gflags[CHURCH_CARROTS])
				say("I've already told you about the carrots.");
			else if (gflags[RABBIT_TRACKS] &&
					AMY->get_item_flag(IN_PARTY))
				{
				AMYFACE.say("Were those your tracks over by the shrine?");
				AMYFACE.hide();
				item.say("Perhaps....");
				say("Am I in trouble?");
				AVATAR.say("Not if you help us.");
				item.say("I was looking for carrots, but all I found was a discarded book among the bushes.");
				say("It had lots of information, but nothing about where to find carrots." +
				    " So I didn't care much for it.");
				AVATAR.say("Well, where is it now?");
				item.say("Uh, er, I, er, uh...");
				AVATAR.hide();
				item.say("...I don't know! But I did find some nice carrots behind that church.");
				gflags[CHURCH_CARROTS] = true;
				}
		case "Usecode?" (remove):
			say("Ask that deranged Dr.Code");
			add("Dr.Code?");
		case "Dr.Code?" (remove):
			say("He got mad because he looked at the evil Usecode!" +
			    "  Don't believe what he says!");
		case "Dragons?" (remove):
			say("You know, those big, scaly things with wings. One dragon I tend to annoy in particular.");
			add(["Big?", "Annoy?"]);
		case "Big?" (remove):
			say("Large? Huge? Think that size. Although a number are quite a bit smaller for some reason." +
			    " Maybe a genetic defect.");
		case "Annoy?" (remove):
			say("Annoy? Well I generally harass with buckets of paint more then annoy, per se." +
			    " The silly thing thinks it's 'Colourless', but everyone knows it's just transparent.");
			add("Colourless?");
		case "Colourless?" (remove):
			say("One of those dragon things. Rather puny in size in comparison to her racial cousins," +
			    " but generally non-threatening, although she does tend to threaten and bluster a lot." +
			    " Especially when I paint her in fluro colours.");
			add("Threaten?");
		case "Threaten?" (remove):
			say("More of a bluster and intimidation really. Just general gripes like," +
			    " 'don't do that' and _continual_ complaints about my colour pallete choices" +
			    " when I'm decorating her. She's such an incredibly picky dragon.");
			add("Decorating?");
		case "Decorating?" (remove):
			say("Nothing fancy, just adding a few dashes of colour to an otherwise drab" +
			    " and dreary 'transparent'coloured dragon");
			add("Drab?");
		case "Drab?" (remove):
			say("Well she's just a little dull and monotonous, no real differences in his" +
			    " colour over his entire body." +
			    " I think giving her say, yellow wings, green paws, and a red muzzle would look rather cute.");
			add("Cute?");
		case "Cute?" (remove):
			say("Well... really it's more close to 'handsome' then 'cute'." +
			    " But since she lacks a nice coat of fur, she can't really be handsome," +
			    " so cute in a pet-iguana-that's-rather-larger-" +
				" than-an-iguana-and-doesn't-eat-insects" +
			    " kind of way, she's rather stuck at.");
			add("Fur?");
		case "Fur?" (remove):
			say("Unfortunately I must heed the call of sleep now. Goodnight.");
			break;
		}
	item.hide();
	}
/*
 *	'EsBee_Ex' on island.
 */
void EsBee_Ex object#(0x56B) ()
	{
	if (event == 0)
		{
		Random_barks(["...burn",
		              "...Rome..",
		              "boom",
		              "...Pyro-X"]);
		return;
		}
	if (event != 1)
		return;
	if (gflags[ASKED_KNOW_PYRO])
		{
		item.say("Ah, did you find my twin?");
		add("You are Pyro-X!");
		}
	else
		{
 		item.say("Hello, did you see my evil twin?!");
		add(["Evil twin?", "Name", "Job", "Bye"]);
		}
	converse (0)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("I'm eesbee-eex");
		case "Job":
			say("I'm hiding from my evil twin");
		case "Evil twin?" (remove):
			say("Yes, my evil twin called Pyro-X.");
			add("Pyro-X");
		case "Pyro-X" (remove):
			say("I've never seen him before, but people blame him for all sorts of explosions " +
			    "and burnt houses. People say it's me but I say it is my evil twin.");
			gflags[ASKED_ABOUT_PYRO] = true;
		case "You are Pyro-X!" (remove):
			say("I'm NOT him! Go away! Go away! Run! He's coming!");
			attack_object(find_nearest(704, 20), 704);
			gflags[ASKED_ABOUT_PYRO] = false;
			gflags[ASKED_WHERE_PYRO] = false;
			gflags[ASKED_KNOW_PYRO] = false;
			break;
		}
	item.hide();
	}

/*
 *	Nadir.
 */
void Nadir object#(0x56C) ()
	{
	if (event == 0)
		{
		Random_barks(["...free software",
		              "...free beer",
		              "...autoconf",
		              "Follow the GPL!"]);
		return;
		}
	if (event != 1)
		return;
	item.say("A guard paces beside a shrine.  He stands at attention as you approach.");
	say("Hail, traveller!");
	say("Hast thou come to study and meditate?");
	var answers = ["Name", "Job", "Study?"];
	if (gflags[WILL_FIND_FAQ])
		answers = [answers, "FAQ"];
	answers = [answers, "Bye"];
	converse(answers)
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("My name is Nadir. Are you a follower of the source?");
			add("Source?");
		case "Job" (remove):
			say("I guard the shrine of Free Software. All who come, rich or poor, may gaze upon the source.");
			add("Source?");
		case "Study?" (remove):
			say("Gaze upon the mirror... The secrets of the universe dwell within!");
			AVATAR.say("Thanks, but maybe later.");
			item.say("Suit thyself.");
			AVATAR.hide();
		case "Source?" (remove):
			say("The source controls our fates.");
			if (gflags[TALKED_DRCODE])
				{
				AVATAR.say("But Dr.Code says Usecode does that.");
				item.say("That is so.");
				say("...and yet it is not.");
				say("...actually, it is but partly so.");
				say("But Dr.Code isn't quite right in the head.");
				say("He wanders around muttering, occasionally dropping documents amongst the bushes.");
				AVATAR.hide();
				if (UI_get_npc_object(AMY) in UI_get_party_list())
					script AMY
						{
						wait 1;		step 6;
						wait 1;		step 6;
						wait 1;		frame 11;
						wait 1;		frame 12;
						wait 2;		frame 11;
						wait 1;		frame 0;
						wait 1;		call Amy;
						}
				}
		case "FAQ" (remove):
			AVATAR.say("Hast thou happened to have seen the FAQ?");
			item.say("Others may deal with documentation, but I concern myself only with the source.");
			AVATAR.hide();
		}
	item.hide();
	}
/*
 *	'Kirben' on island.
 */
void Kirben object#(0x56D) ()
	{
	if (event == 0)
		{
		Random_barks(["...speed",
		              "...thrift..",
		              "design"]);
		return;
		}
	if (event != 1)
		return;
	if (!get_item_flag(IF_MET))
		{			// First time.
		item.say("You see a scribe with many different scrolls around him.");
		say("Welcome to the Open Church of SourceForge Island!");
		}
	else
		item.say("Hello Avatar!");
	converse (["Name", "Job", "Bye"])
	{
	case "Bye":
			break;
		case "Name" (remove):
			say("My name is Kirben.");
		case "Job" (remove):
			say("I am the official scribe of the Open Church of SourceForge Island!");
			add(["SourceForge Island?", "Open Church?", "Scribe?"]);
		case "SourceForge Island?" (remove):
			say("Amy knows all about our island and its inhabitants. Ask her for more information.");
		case "Open Church?" (remove):
			say("In the Open Church of SourceForge Island everyone " +
			    "can preach his beliefs as long as he makes them freely available to everyone. ");
			say("Do you want to know more?");
			if (Ask_yesno())
				{
				say("Our church is based on three principles.");
				say("At the moment I have no idea what these are." +
				    " I'm still waiting for someone to put some words in my mouth");
				}
			else
				say("Okay, come back later if you want to know more.");
		case "Scribe?" (remove):
			say("People rely on me to write down the changes in the beliefs every other day.");
			say("You can find the written beliefs of our people in the scrolls in this church.");
		}
	item.hide();
	}
/*
 *	'BillyG' on island.
 */
void BillyG object#(0x56E) ()
	{
	if (event == 0)
		{
		Random_barks(["...money",
		              "...security hole...",
		              "windows"]);
		return;
		}
	if (event != 1)
		return;
	if (!get_item_flag(IF_MET))
		{			// First time.
		item.say("You see a pathetic man with huge round glasses in his face.");
		say("Hello Avatar!");
		}
	else
		item.say("Hello again!");
	converse (["Name", "Job", "Bye"])
		{
		case "Bye":
			break;
		case "Name" (remove):
			say("My name is BillyG.");
		case "Job" (remove):
			say("At the moment I'm washing the windows of this island.");
			say("But soon I will own all these windows and no one may stop me.");
			say("Especially not that pathetic Open Church of fools.");
			add(["Windows?", "Open Church"]);
		case "Windows?" (remove):
			say("Windows rule the world. Without them we would live in darkness.");
			say("And soon I will own all the windows there are.");
			add("How?");
		case "Open Church" (remove):
			say("They stand in my way and try to resist me. " +
			    "They think they can order me around to wash their windows but" +
			    " soon they will be mine.");
		case "How?" (remove):
			say("Pretty simple. I'm going to save my money and " +
			    " soon I'll have enough to buy every window on this island.");
		}
	item.hide();
	}
