/*
 *
 *  Copyright (C) 2006  Alun Bestor
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
 *
 *
 *	This header file defines constants for shapes and frames in Black Gate's
 *	SHAPES.VGA. Fill them in as you go along! It is best to use enums to group
 *	shapes thematically by type and frames by shape, rather than attempting to
 *	order them numerically.
 *
 *	Author: Alun Bestor (exult@washboardabs.net)
 *	Last Modified: 2006-03-19
*/

//Item-related functions
//----------------------

//If <object> belongs to someone (i.e., does not have the OKAY_TO_TAKE flag and the party is not in a dungeon), this executes stolen-item behaviour: guards showing up, party members leaving etc.
extern void stealItem 0x8FA(var object);

//Remove 1 from <object>'s quantity stack. If it's quantity is at 1, or it has no quantity, this will remove the object.
extern void subtractQuantity 0x925(var object);

//This will try to pathfind the avatar to <target>, which can be an object, NPC or X,Y,Z location(?).
//<rangex> and <rangey> are arrays that define a grid of points around the object, and the Avatar will pathfind to the first unoccupied point in that grid (in the order the coordinates were defined). <rangez> I think declares an acceptable z range to pathfind to, and is usually -3.
//if the player can get there, then they will walk there at regular speed. When they arrive in the region, <func> will be called with <context> as the item and <eventid> as the event.
//if the player cannot get to any point in the grid it flashes the X cursor and no movement occurs.
extern void gotoObject 0x828(var target, var rangex, var rangey, var rangez, var func, var context, var eventid);

//returns true if <obj> is carried by the avatar, false otherwise. Supports nested containers.
extern var containedByAvatar 0x944(var obj);

//returns an item reference to the outermost container of <obj>. Used for when an object is contained several levels deep.
extern var getOuterContainer 0x945(var obj);

//returns the cardinal direction <obj> lies to, relative to the Avatar.
extern var directionFromAvatar 0x92D(var obj);

//Places <obj> on or near <target>, at the specified x y and z offsets from <target>'s 0 point.
//Note: This is probably FV-only.
extern var placeOnTarget 0x837(var obj, var target, var offset_x, var offset_y, var offset_z);


// Conversation/bark-related functions
//------------------------------------

//<npc> barks <line> after <delay> ticks.
extern void delayedBark 0x933(var npc, var line, var delay);

//Get a random nearby party member to say <line> as a bark
extern void randomPartyBark 0x08FE(var line);
//Get a random nearby party member to say <line> in conversation form
extern void randomPartySay 0x08FF(var line);

//returns a random party member who is nearby, or the Avatar if none can be found.
extern var randomPartyMember 0x900();

//returns the name of the Avatar
extern var getAvatarName 0x0908();
//returns "milord" or "milady", depending on the gender of the Avatar
extern var getPoliteTitle 0x909();

//asks a Yes/No question in a conversation; returns true/false respectively
extern var askYesNo 0x90A();
//takes an array of possible response options, and returns the text of the chosen option
extern var askForResponse 0x90B(var options);

//Returns false if <npc> is asleep, paralysed, a moron (intelligence < 12) or unconscious (health < 0), or true otherwise.
//This can take either an NPC constant or an object reference.
extern var canTalk 0x937(var npc);

//Returns true if <npc> is nearby (according to UI_npc_nearby()) and is not invisible.
//This can take either an NPC constant or an object reference.
extern var isNearby 0x8F7(var npc);

//Returns true if <npc1> is within 20 units of <npc2>.
//This can take either NPC constants or object references.
extern var nearEachOther 0x8FC(var npc1, var npc2);

//Spouts the generic Fellowship spiel ("An organisation of spiritual seekers...") in the conversation.
extern void askAboutFellowship 0x919();

//responsible for common schedule-related barks: e.g. snoring, uttering Fellowship epithets, commenting on the weather, etc. Accepts either an NPC constant or an object reference.
extern void scheduleBarks 0x92E(var npc);


//Magic-related functions
//-----------------------

//returns true if there is a magic storm going on, false otherwise
//this is used by spells to prevent magic from being cast in these zones (AFAIK, only Ambrosia)
extern var inMagicStorm 0x906();


//Script-related functions (used in script{} blocks or with UI_execute_usecode_array())
//------------------------

//Puts <item> in the player's inventory, from world or container.
//if this is not possible, it will flash the X cursor and restore the item to its original position/container.
extern void giveToAvatar object#(0x692) ();


// Miscellaneous functions
//------------------------

//Gives the specified amount of experience points to every party member.
extern void giveExperience 0x911(var exp);

//Shows a blocked cursor and plays the "errn" sound. All this really does is supplement UI_flash_mouse() with the sound. (Some originals use this function, some just call UI_flash_mouse directly.)
//See constant.uc for the constants for cursor graphics.
extern void flashBlocked 0x8FD(var cursor);
