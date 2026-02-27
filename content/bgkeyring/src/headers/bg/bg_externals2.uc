/*
 *
 *  Copyright (C) 2006  The Exult Team
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
 **
 *	This header file contains several more external functinos. It is mostly unsorted...
 *
 *	Author: Marzo Junior
 *	Last Modified: 2006-02-27
 */

extern var makeSellPriceString 0x91B (var article, var itemname, var isplural, var price, var quantity_text);

//Prompts player for quantity of objects to sell (in units of base_quant), charges appropriate gold
//and tries to distribute bought items to party.
//Returns zero if the player chose that number, 2 if the party can't carry what was bought, 3 if the
//party does not have enough gold or 1 if successful.
//If max_quant is zero, it skips the numeric input.
extern var sellAmountToParty 0x8F8(var shapenum, var framenum, var base_quant, var price, var max_quant, var min_quant, var unknownflag);

//Returns the index (1-n) of a choice the player made from the items in itemnames
extern var chooseFromMenu2 0x90C(var itemnames);

//Generates a poof of smoke and associated sound:
extern void spellFails object#(0x606) ();
//Returns the level of npc:
extern var getNPCLevel 0x8F6 (var npc);
extern var absoluteValueOf 0x932 (var number);
//Returns all non-party NPCs within dist tiles from Avatar:
extern var getNearbyNonPartyNPCs 0x934 (var dist);
extern void hurtNPC 0x936 (var npc, var damage);
extern void setNonpartySchedule 0x93F (var npc, var sched);

//Makes an NPC stop dancing (from Dance spell):
extern void stopDancing object#(0x688) ();
//Stops magic storm, clears magic storm flag:
extern void stopMagicStorm object#(0x68A) ();
//Used by magic storm; lightning bolt falls on enemy and causes damage:
extern void callLightning object#(0x60F) ();

//Sees if container has a minimum amount of a given item:
extern var contHasItemCount 0x931 (var container, var min_count, var shapenum, var quality, var framenum);

//Uses a key on a door:
extern void UseKeyOnDoor 0x815 (var Door);
//The usecode for the door shapes:
extern void doorHorizontal shape#(0x10E) ();
extern void doorVertical shape#(0x178) ();
extern void door2Horizontal shape#(0x1B0) ();
extern void door2Vertical shape#(0x1B1) ();

//Moongate-related externs:
//Plays animation where the avatar leaves a moongate:
extern void exitMoongate object#(0x636) ();
//Closes a moongate created by the orb of the moons:
extern void closeOrbMoongate 0x821 (var moongate);
//Returns the position where the player clicked as a respose of UI_click_on_item:
extern struct<Position> getClickPosition 0x822 (struct<ObjPos> itemref);
//The falling-down-kneeling-over-in-pain animation when the
//avatar gets hurt trying to cross a moongate:
extern struct<Position> badMoongateAnim 0x825 (struct<Position> avatarpos, struct<Position> mongatepos, var coord);
//Returns true if the player is inside a paralelepiped which
//contains all of Trinsic:
extern var inGreaterTrinsicArea 0x93E ();

//Blacksword-related externs:
//Fire power:
extern void createFire object#(0x6FC) ();
extern void teleportIsleOfFire object#(0x6F9) ();
//Death power:
extern void killTarget object#(0x70F) ();
//The animation (and sound) of the daemon mirror after you
//say "Bye" to Arcadion:
extern void arcadionMirrorHide 0x843 ();
extern var arcadionGemInList 0x844 (var gemlist);
//The Power power:
extern void replenishMana 0x845 (var isblacksword);
extern var swordBlankAndGemInHands 0x846 ();
//The blacksword doesn't kill corpses:
extern var isCorpseShape 0x847 (var target_shape);
//Forces the blacksword into the avatar's inventory:
extern void forceGiveBlackSword object#(0x70B) ();
extern var inIsleOfFire 0x8E7 ();

//Deletes container and all objects inside it:
extern void deleteObjectAndContents 0x8E6 (var container);
//Does... *something*... to bodies (used mainly for FoV NPCs)
extern void makeStuffToBodies object#(0x6F7) ();

//Reimplemented:
//Used by Death power to see if the blacksword deems the target to be
//a worthy foe:
extern var isWorthyToKill 0x848 (var target_shape);
//The blacksword is affraid of walking corpses, and won't kill them:
extern var isUndead 0x849 (var target_shape);
