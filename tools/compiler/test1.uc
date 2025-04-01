#game "blackgate"
#autonumber 0xC00
#include "ucdefs.h"

extern var adder (a, b);	// Declaration.
const int const13 = 13;
const int DOUBLECLICK = 1;

/*
 *	Return sum.
 */

var adder1 (a, b)
	{
	var c;
	var d;
	string hello = "Hello, Avatar!";

	adder(a, b);	// :-)
	c = a + b;
	d = [a, 1, b];		// Should pop as a, 1, b.
// 	hello = a;		// Should cause an error.
	say("Hello, the time is ", 11, "o'clock");
	// Old-style, super-verbose conversations:
	converse
		{
		if (response == "Name")
			{
			say("My name is \"DrCode\".");
			UI_remove_answer("Name");
			// This also works:
			// UI_remove_answer(user_choice);
			}
		else if (response == "Bye")
			break;
		else if (response in ["bogus", "nonsense", "garbage"])
			say("This is utter nonsense!");
		}
	say("Hello, the time is ", 12, "o'clock");
	converse (0)
		{
		case "bye" (remove):
			break;
		}
	// Newer conversation style:
	converse (["Name", "Bye", "name"])
		{
		case "Name" (remove):
			{
			say("My name is \"DrCode\".");
			}
		case "Bye":
			break;
		case "bogus", "nonsense", "garbage":
			say("This is utter"*);
		default:
			say("All other answers get here. Your choice was ", user_choice);
		}
				// This is nonsense:
	c = a[7];
	a[const13] = 46;
	c = item;
	event += 7;
	c = UI_get_item_flag(item, 10);
	c = item->get_item_flag(10);
	c = get_item_flag(10);
	c = UI_get_party_list();
	c = item->resurrect();
	for(actor1 in c with i to max)
		{
		say("Hello, ", UI_get_npc_name(actor1));
		}
	var dd = a + 5, f, e = 2*d;

	script item {
		nohalt;
		next frame;
	}

	return adder(a, 3);
}

var adder (a, b)
{
	return a + b;
}

class Test
{
	var testvar;
	void now_what() { ; }
	void Fun2() { ; }
}

class Test2 : Test	// Inheritance: class Test2 has all data members and
		// functions of class Test, and can be converted to class Test if
		// needed by e.g., a function call.
{
	var testvar1;
	void now_what1() { testvar1 = 0; Test::Fun2(); now_what(); Fun2(); }
	class<Test> Fun5()
	{
		testvar1 = 1;
		//return;	// Fails to compile
		//return 1;	// Fails to compile
		//return 0;	// Compiles correctly
		return this;	// this is of type class<Test2>, which is
				// derived from class<Test> hence can be
				// converted to it
	}
	void Fun2() { ; }
	void Fun3()
	{
		//return 1;	// Fails to compile
		//return this;	// Fails to compile
		return;	// Fails to compile
	}
}

class Test3 : Test
{  }

class Test4 : Test2
{  }

class<Test> Fun2 (class<Test> a)
{
	class<Test> mytest;
	class<Test2> foo = new Test2(5, 6);	// Constructor example
	class<Test> bar = new Test2(7, 8);	// Constructor and casting example

	foo->Fun2();		// Calls Fun2 of class Test2 with 'foo' as 'this'
	foo->Test::Fun2();	// Calls Fun2 of class Test with 'foo' as 'this'
	foo->now_what1();

	bar = foo;
	//foo = bar;	// Fails to compile

	// Constructor and casting examples:
	mytest = new Test(9);
	mytest = new Test2(10, 11);
	mytest = new Test(12);
	//mytest = new Test4(a, a);		// Fails to compile
	mytest = new Test4(1, 2);
	//mytest = new Test3(a);		// Fails to compile
	mytest = new Test3(3);
	bar = new Test(13);

	// Conditions:
	if (bar)
		;

	// Deletion:
	delete bar;
}

/*
var adder (a, b)
	{
	return a + b;
	}
*/

enum party_members
{
	PARTY		= -357,	//Used by several intrinsics (e.g. UI_count_objects) that would otherwise take a single NPC
						//Not supported by several other intrinsics that you'd really like it to (e.g. UI_get_cont_items)
	AVATAR		= -356,
	IOLO		= -1,
	SPARK		= -2,
	SHAMINO		= -3,
	DUPRE		= -4,
	JAANA		= -5,
	SENTRI		= -7,
	JULIA		= -8,
	KATRINA		= -9,
	TSERAMED	= -10
};

var throw_abrt_test(var flag) {
	if (flag == 0) {
		abort;
	} else if (flag == 1) {
		try {
			throw_abrt_test(0);
		} catch (errmsg) {
			AVATAR.say(errmsg);
			throw flag;
		}
	} else {
		throw flag;
	}
	return 1;
}

void try_catch_test()
{
	if (event == DOUBLECLICK)
	{
		try {
			throw_abrt_test(0);
		} catch () {
			DUPRE.say("Aha!");
		}
		try {
			throw_abrt_test(0);
		} catch (errmsg) {
			IOLO.say(errmsg);
		}
		try {
			throw_abrt_test(1);
		} catch (errmsg) {
			SHAMINO.say(errmsg);
		}
		try {
			throw_abrt_test("Test");
		} catch (errmsg) {
			SPARK.say(errmsg);
		}
		throw_abrt_test("Game over");
	}
}

extern var Predicate (var npc);

void for_nobreak_test(var list) {
	var npc;
	for (npc in list)
	{
		if (Predicate(npc))
		{
			break;
		}
	}
	nobreak
	{
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void while_nobreak_test(var list) {
	var count = UI_get_array_size(list);
	var index = 0;
	var npc;
	while (index < count)
	{
		npc = list[index];
		if (Predicate(npc))
		{
			break;
		}
		index += 1;
	}
	nobreak
	{
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void dowhile_nobreak_test(var list) {
	var count = UI_get_array_size(list);
	var index = 0;
	var npc;
	do
	{
		npc = list[index];
		if (Predicate(npc))
		{
			break;
		}
		index += 1;
	} while (index < count)
	nobreak
	{
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void breakable_nobreak_test(var list) {
	var count = UI_get_array_size(list);
	var index = 0;
	var npc;
	do
	{
		npc = list[index];
		if (Predicate(npc))
		{
			break;
		}
	} while (false)
	nobreak
	{
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void converse_nobreak_test(var list) {
	var npc;
	converse (["Iolo", "Shamino", "Dupre"])
	{
	case "Iolo" (remove):
		npc = -1;
		if (Predicate(npc))
		{
			break;
		}
	case "Shamino" (remove):
		npc = -2;
		if (Predicate(npc))
		{
			break;
		}
	case "Dupre" (remove):
		npc = -3;
		if (Predicate(npc))
		{
			break;
		}
	}
	nobreak
	{
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void infinite_warning_nobreak_test1(var list) {
	var count = UI_get_array_size(list);
	var index = 0;
	var npc;
	do
	{
		npc = list[index];
		if (Predicate(npc))
		{
			break;
		}
		else
		{
			goto test_nobreak;
		}
	} while (true)
	nobreak
	{
test_nobreak:
		npc = -356;
	}
	UI_set_opponent(-71, npc);
}

void infinite_warning_nobreak_test2(var list) {
	var count = UI_get_array_size(list);
	var index = 0;
	var npc;
	do
	{
		npc = list[index];
		if (Predicate(npc))
		{
			break;
		}
		else
		{
			goto test_nobreak;
		}
	} while (true)
	nobreak
	{
		npc = -356;
test_nobreak:
	}
	UI_set_opponent(-71, npc);
}

extern void doTraining(var price);
extern var askYesNo();
void preamble_test() {
	var avatarName = UI_get_npc_name(UI_get_avatar_ref());
	converse : nested (["name", "job", "bye"])
	{
		var party = UI_get_party_list();
		case "name" (remove):
			say("I am Mi. What is your name?");
			add(avatarName, "Avatar");
		case "Avatar" (remove):
			say("Your name is 'Avatar'? I find that unlikely...");
		case avatarName (remove):
			say("Pleased to meet you, ", avatarName, "!");
			UI_remove_answer("Avatar");
		case "job" (remove):
			say("I teach martial arts.");
			if (!(item in party))
			{
				add("join");
			}
			add("train");
		case "train":
			if (item in party)
			{
				say("Since we are traveling together, I will waive my usual fee.");
				doTraining(0);
			}
			else
			{
				say("It will cost 50 gold per person. Is this acceptable?");
				if (askYesNo())
				{
					doTraining(50);
				}
				else
				{
					say("Suit yourself.");
				}
			}
		case "bye":
			break;
	}
	say("Farewell!");
}

void array_tests()
{
	const int AVATAR = -356;
	const int SHAPE_ANY = -359;
	var var1 = UI_get_object_position(AVATAR) & (SHAPE_ANY & 0x0003);
	var var2 = (UI_get_object_position(AVATAR) & SHAPE_ANY) & 0x0003;
	var var3 = UI_get_object_position(AVATAR) & [SHAPE_ANY, 0x0003];
}

void fallthrough_always_test() {
	var avatarName = UI_get_npc_name(UI_get_avatar_ref());
	converse : nested (["name", "job", "bye"])
	{
		var party = UI_get_party_list();
		var remove_name = false;
		case "name" (remove):
			say("I am Mi. What is your name?");
			add(avatarName, "Avatar");
			fallthrough;
		case "Avatar":
			say("Your name is 'Avatar'? I find that unlikely...");
			remove_name = true;
			fallthrough;
		case avatarName:
			say("Pleased to meet you, ", avatarName, "!");
			remove_name = true;
			fallthrough;
		always:
			if (remove_name)
			{
				UI_remove_answer(["Avatar", avatarName]);
			}
			fallthrough;
		case "job" (remove):
			say("I teach martial arts.");
			if (!(item in party))
			{
				add("join");
			}
			add("train");
			fallthrough;
		case "train":
			if (item in party)
			{
				say("Since we are traveling together, I will waive my usual fee.");
				doTraining(0);
			}
			else
			{
				say("It will cost 50 gold per person. Is this acceptable?");
				if (askYesNo())
				{
					doTraining(50);
				}
				else
				{
					say("Suit yourself.");
				}
			}
			fallthrough;
		case "bye":
			break;
	}
	say("Farewell!");
}

void test_always_default()
{
	converse (["-I- am the Avatar!", "I -am- the Avatar!", "I am -the- Avatar!", "I am the -Avatar-!"])
	{
		default:
			message("\"No, no, no! That is all wrong! Thou art the 'Avatar'! Thou must feel like the Avatar! Thou must sound like the Avatar! Thou must -be- the Avatar! Try it again.\"");
			say();
			fallthrough;
		always:
			UI_clear_answers();
			UI_add_answer(["-I- am the Avatar!", "I -am- the Avatar!", "I am -the- Avatar!", "I am the -Avatar-!"]);
		default:
			message("\"Better... better... but I think perhaps thou dost need a prop.\"");
			say();
			break;
	}
}

void test_intrinsic()
{
	(@0x83)();
}

void test_script_assign()
{
	var temp1;
	temp1 = script item {
		nohalt;
		next frame;
	};
	var temp2 = script item {
		nohalt;
		next frame;
	};
}

void test_breakable_continue(var flag)
{
	do
	{
		if (flag == 1)
		{
			say("Continue");
			continue;
		}
		if (flag == 2)
		{
			say("Break");
			break;
		}
		if (flag == 3)
		{
			say("Fallthrough");
		}
	} while (false)
	nobreak
	{
		say("No-break");
	}
	say("Past the end");
	do
	{
		if (flag == 1)
		{
			say("Continue");
			continue;
		}
		if (flag == 2)
		{
			say("Break");
			break;
		}
		if (flag == 3)
		{
			say("Fallthrough");
		}
	} while (false);
	say("Past the end");
}

void test_for_attend(var array)
{
	var npc;
	enum();
loop:
	for (npc in array) attend exit;
	if (!npc->get_item_flag(0xA)) goto loop;
	npc->clear_item_flag(0xA);
	npc->set_item_flag(0xB);
	goto loop;
exit:
}

void test_converse_attend(var array)
{
loop:
	converse attend exit;
	case "name" attend next_case:
	message("a");
	say();
next_case:
	case array attend default_case:
	message("b");
	say();
default_case:
	default attend loop:
	message("c");
	say();
	endconv;
exit:
}

#strictbraces "true"

void test_strictbraces_edge_cases(var input) {
	if (input == 1) {
	} else if (input == 2) {
	} else {
		if (input == 1) {
		} else {
			breakable {
				;
			}
			nobreak {
				if (input == 1) {
					;
				}
			}
		}
	}
}
