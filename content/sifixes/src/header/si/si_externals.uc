/*
 *
 *	Copyright (C) 2006  The Exult Team
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 *	Called at the first meeting with Thoxa, when you meet her at the dream world
 *	and when she appears before the Banes are released. The SI devs must have
 *	been running out of available functions... or maybe out of time to search for
 *	them.
 */
extern void gwaniChild shape#(0x363) ();

// Calls the ring (???) usecode, which promptly forwards to a function
// that will start the appropriate speech for the plot:
extern void startSerpentSpeechViaRing shape#(0x377) ();

// Makes all non-automaton party members face item.
// Better call this as "npc->makePartyFaceNPC();"
extern void makePartyFaceNPC object#(0x7D1) ();

// Makes NPC face Avatar:
extern void faceAvatar object#(0x7D2) ();

// Generic party automaton dialog:
extern void partyAutomaton 0x809 ();

// Resurrects Iolo, Shamino and Dupre and adds them back to the party
extern void resurrectCompanions 0x86D ();

// playmusic refers to the Batlin cutscene music:
extern void animateWallOfLights 0x8C0 (var playmusic);

extern void xenkaReturns 0x8F4 ();
extern void xenkanMonkDies 0x8F5 (var monk);

// Returns true is point is inside rectangle
extern var pointInsideRect 0x8F8 (struct<Position2D> point, struct<Position2D> upperleft, struct<Position2D> lowerright);

// List of dead bodies nearby:
extern var getNearbyBodies 0x8FB ();

extern var areThereBodiesNearby 0x8FC ();

// List the NPC names for joinable NPCs:
extern var getJoinableNPCNames 0x8FD ();

// 0 is unlocked, 1 is swung open, 2 is locked, 3 is magically locked:
extern var getDoorState 0x906 (var door);

// state 0 = unlocked, state 1 = swung open, state 2 is locked and
// state 3 is magically locked
extern void setDoorState 0x907 (var door, var state);

// Presses switch/pulls lever and alters state of nearby sliding doors.
// Does several additional things, such as checking for Skullcrusher password
// for the appropriate door, etc.
// If mode == 0, closes opened sliding doors.
// If mode == 1, opens closed sliding doors.
// If mode == SHAPE_ANY, toggles sliding doors.
extern void changeSlidingDoorState 0x924 (var lever, var mode);

extern void usecodeStartSpeechWrapper 0x922 (var track);
extern var exchangedItemList 0x92C (var index);

// Total guess:
extern void endMonitorTraining 0x92E (var npc);

// The dialog of the Monitorians after training ends:
extern void trainingEndDialog 0x936 (var npc1, var npc2);

extern var getNPCLevel 0x941 (var npc);
extern var npcNearbyAndVisible 0x942 (var npc);

// Causes NPC to speak if nearby
extern void npcSpeakIfNearby 0x94E (var npc, var text);

// Returns true if the party is NOT in a magic storm:
extern var notInMagicStorm 0x951();

extern var getAvatarName 0x953 ();
extern var getPoliteTitle 0x954 ();
extern var askYesNo 0x955 ();
extern var chooseFromMenu2 0x957 (var choices);

// Returns the value of propid for NPC:
extern var getPropValue 0x95C (var npc, var propid);

// Increases the value of propid for NPC by delta:
extern void setPropValue 0x95E (var npc, var propid, var delta);

extern var getArraySize 0x977 (var array);

// Wrapper for UI_direction_from(AVATAR, obj)
extern var directionFromAvatar 0x979 (var obj);

// Sees if the party has a minimum count of the specified item:
extern var hasItemCount 0x97D (var cont, var mincount, var shapenum, var quality, var framenum);

extern var absoluteValueOf 0x97E (var number);

// Makes the NPC bark after delay ticks:
extern void delayedBark 0x97F (var npc, var bark, var delay);

// Sees if the NPC can talk (i.e., not asleep/paralyzed/dead/unconscious):
extern var npcCanTalk 0x983 (var npc);

// Removes all instances of element from the array:
extern var removeFromArray 0x988 (var element, var array);

// List the NPC numbers for joinable NPCs:
extern var getJoinableNPCNumbers 0x98D ();

// Gets a list of all non-automaton party members:
extern var getNonAutomatonPartyMembers 0x98E ();

/*
 *	This one is a beast! Here it goes:
 *	if there is only the avatar in the party, the avatar will utter avatarutter.
 *	Otherwise:
 *	if npcnum == 1 or if npcnum is not a joinable NPC, a random (non-avatar)
 *	party member utters partyutter.
 *	Otherwise:
 *	if npcnum *is* a joinable NPC:
 *	if npcnum is in the party, it will utter partyutter.
 *	Otherwise:
 *	tries to find an in-party substitute for npcnum who will utter partyutter.
 *	If the function can't find an appropriate substitute for npcnum, it will give
 *	an error.
 *	If avatarutter == 0 and only the avatar is in the party, he will not utter
 *	anything.
 *	Likewise, partyutter == 0 results in nothing being uttered.
 *	Finally, isbark determines if the NPCs bark or interject the text. This is
 *	what I meant by "utter" above.
 */
extern var partyUtters 0x992 (var npcnum, var partyutter, var avatarutter, var isbark);

// Makes a random party member bark utter.
// Equivalent to partyUtters(1, utter, utter, true);
extern void randomPartyBark 0x949 (var utter);

// Gets the Avatar's location ID; see getLocationID in "misc/location_ids.uc"
// for details:
extern var getAvatarLocationID 0x994 ();

// Takes an item from a container and gives it to another:
extern var giveItem 0x996 (var givefrom, var giveto, var givecount, var shapenum, var quality, var framenum, var flag);

extern var giveItemsToPartyMember 0x99B (var cont, var count, var shapenum, var quality, var framenum, var flag1, var flag2);
extern var getOuterContainer 0x99E (var obj);

// In SI, path eggs are used for scripting purposes. This function turns a pair
// of numbers (num1 and num2) into a number 0-31 then returns the path egg with
// the corresponding frame. Returned path eggs come from the Usecode Container.
extern var getPathEgg 0x9A0 (var num1, var num2);

// Remove all items from a container, then removes the container.
extern void removeAllItemsAndContainer 0x9A3 (var obj);

// Returns an array containing the index of all instances of
// element in the array:
extern var getIndexForElement 0x9A8 (var element, var array);

// Clears DONTMOVE flag:
extern void unfreezeAvatar 0x9AA ();

/*
 *	 Sets the NPCs schedule to activity at [posx, posy, 0] if posx >= 0.
 *	 If posx < 0, it ignores posx and posy and uses the NPCs current
 *	 position instead if the z-coordinate of this position is zero.
 *	 If the z-coordinate of the NPC is *not* zero, it sets the NPCs
 *	 immediate schedule to WAIT (right then and there) and -- if they
 *	 are in the party -- to EAT_AT_INN in the kitchen at Monk isle.
 */
extern void setNewSchedules 0x9AC (var npc, var posx, var posy, var activity);

// Sets a single NPC to attack the party,
// and changes that NPC's alignment to Evil.
extern void attackParty 0x9AD (var npc);

// Makes the NPC ask where to wait when kicked from the party
// (or, in some cases, simply says where he will be later):
extern void npcAskWhereToWait 0x9B4 (var npcnum);
