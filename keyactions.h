/*
 *  Copyright (C) 2001-2022 The Exult Team
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
 */

#ifndef KEYACTIONS_H
#define KEYACTIONS_H

void ActionQuit(const int* params);
void ActionFileGump(const int* params);
void ActionMixerGump(const int* params);
void ActionQuicksave(const int* params);
void ActionQuickrestore(const int* params);
void ActionAbout(const int* params);
void ActionHelp(const int* params);
void ActionCloseGumps(const int* params);
void ActionCloseOrMenu(const int* params);
void ActionMenuGump(const int* params);
void ActionOldFileGump(const int* params);

void ActionScreenshot(const int* params);
void ActionRepaint(const int* params);
void ActionScalevalIncrease(const int* params);
void ActionScalevalDecrease(const int* params);
void ActionBrighter(const int* params);
void ActionDarker(const int* params);
void ActionFullscreen(const int* params);

void ActionUseItem(const int* params);
void ActionUseFood(const int* params);
void ActionCallUsecode(const int* params);
void ActionCombat(const int* params);
void ActionCombatPause(const int* params);
void ActionTarget(const int* params);
void ActionInventory(const int* params);
void ActionTryKeys(const int* params);
void ActionStats(const int* params);
void ActionCombatStats(const int* params);
void ActionFaceStats(const int* params);
void ActionUseHealingItems(const int* params);

void ActionSIIntro(const int* params);
void ActionEndgame(const int* params);
void ActionScrollLeft(const int* params);
void ActionScrollRight(const int* params);
void ActionScrollUp(const int* params);
void ActionScrollDown(const int* params);
int  get_walking_speed(const int* params);
void ActionWalkWest(const int* params);
void ActionWalkEast(const int* params);
void ActionWalkNorth(const int* params);
void ActionWalkSouth(const int* params);
void ActionWalkNorthEast(const int* params);
void ActionWalkSouthEast(const int* params);
void ActionWalkNorthWest(const int* params);
void ActionWalkSouthWest(const int* params);
void ActionStopWalking(const int* params);
void ActionCenter(const int* params);
void ActionShapeBrowser(const int* params);
void ActionShapeBrowserHelp(const int* params);
void ActionCreateShape(const int* params);
void ActionDeleteObject(const int* params);
void ActionDeleteSelected(const int* params);
void ActionMoveSelected(const int* params);
void ActionCycleFrames(const int* params);
void ActionRotateFrames(const int* params);
void ActionToggleEggs(const int* params);
void ActionGodMode(const int* params);
void ActionGender(const int* params);
void ActionCheatHelp(const int* params);
void ActionMapeditHelp(const int* params);
void ActionInfravision(const int* params);
void ActionSkipLift(const int* params);
void ActionLevelup(const int* params);
void ActionMapEditor(const int* params);
void ActionHackMover(const int* params);
void ActionMapTeleport(const int* params);
void ActionWriteMiniMap(const int* params);
void ActionTeleport(const int* params);
void ActionTeleportTargetMode(const int* params);
void ActionNextMapTeleport(const int* params);
void ActionTime(const int* params);
void ActionWizard(const int* params);
void ActionHeal(const int* params);
void ActionCheatScreen(const int* params);
void ActionPickPocket(const int* params);
void ActionNPCNumbers(const int* params);
void ActionGrabActor(const int* params);

void ActionCut(const int* params);
void ActionCopy(const int* params);
void ActionPaste(const int* params);

void ActionPlayMusic(const int* params);
void ActionNaked(const int* params);
void ActionPetra(const int* params);
void ActionSkinColour(const int* params);
void ActionNotebook(const int* params);
void ActionSoundTester(const int* params);
void ActionTest(const int* params);

#endif
