/*
 * usefuns.h - Collection of still hard-coded usecode functions.
 *
 * Copyright (C) 2013 - Marzo Sette Torres Junior
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USEFUNS_H
#define USEFUNS_H

const int SpeechUsecode             = 0x614;
const int SleepUsecode              = 0x622;
const int ArrestUsecode             = 0x625;
const int DestroyObjectsUsecode     = 0x626;
const int BargeUsecode              = 0x634;
const int FailCopyProtectionUsecode = 0x63d;
const int BaseSpellsUsecode         = 0x640;
const int SummonSpellUsecode        = BaseSpellsUsecode + 0x45;

const int SSMazePartyWait = 0x62F;

#endif
