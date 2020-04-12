/*
 *  Ucscriptop.h - Usecode-script opcode definitions.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef USCRIPTOP_H
#define USCRIPTOP_H

/*
 *  Opcodes for Usecode_script's:
 */
namespace Ucscript {
enum Ucscript_ops {
    cont =      0x01,       // Continue without painting.
    reset =     0x0a,       // Resets script ip.
    repeat =    0x0b,       // Loop(offset, cnt).
    repeat2 =   0x0c,       // Loop(offset, cnt1, cnt2).
    nop =       0x21,       // Not sure about this.
    dont_halt = 0x23,       // Not right?
    wait_while_near = 0x24, // wait_while_near(dist). Halt on opcode for as long
    // as avatar is within dist tiles.
    delay_ticks =   0x27,       // Delay(ticks).
    delay_minutes = 0x28,       // Delay(minutes).
    delay_hours =   0x29,       // Delay nn game hours.
    wait_while_far = 0x2b,  // wait_while_far(dist). Halt on opcode for as long
    // as avatar is further than dist tiles.
    finish =    0x2c,       // Finish script if killed.
    remove =    0x2d,       // Remove item & halt.
    step_n =    0x30,       // Step in given direction.
    step_ne =   0x31,
    step_e =    0x32,
    step_se =   0x33,
    step_s =    0x34,
    step_sw =   0x35,
    step_w =    0x36,
    step_nw =   0x37,
    descend =   0x38,       // Decr. lift.
    rise =      0x39,       // Incr. lift.
    frame =     0x46,       // Set_frame(frnum).
    egg =       0x48,       // Activate egg.
    set_egg =   0x49,       // Set egg's criteria, distance.
    next_frame_max = 0x4d,      // Next frame, but stop at max.
    next_frame =    0x4e,       // Next frame, but wrap.
    prev_frame_min = 0x4f,      // Prev frame, but stop at 0.
    prev_frame =    0x50,       // Prev. frame, but wrap.
    say =       0x52,       // Say(string).
    step =      0x53,       // Step(dir).
    music =     0x54,       // Play(track#).
    usecode =   0x55,       // Call usecode(fun).
    speech =    0x56,       // Speech(track#).
    sfx =       0x58,       // Sound_effect(#).
    face_dir =  0x59,       // Face_dir(dir), dir=0-7, 0=north.
    weather =   0x5A,       // Set weather(type).
    npc_frame = 0x61,       // 61-70:  Set frame, but w/ cur. dir.
    hit =       0x78,       // Hit(hps, type).  Item attacked.
    attack =    0x7a,       // Attack using vals from
    //   set_to_attack intrinsic.
    /*
     *  These are (I think) not in the original:
     */
    usecode2 =  0x80,       // Call usecode(fun, eventid).
    resurrect = 0x81        // Parm. is body.
};
}
#endif
