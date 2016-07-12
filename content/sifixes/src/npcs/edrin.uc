/*  Copyright (C) 2016  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  There is a very minor flag, only checked by Edrin, that changes his
 *  response when asked about Siranush. It should have been set once he
 *  sees her for the last time, prior to the Dream Crystal being broken.
 *  This code checks if the Dream Realm has been completed, and if so it
 *  sets flag 531 (0x213). In testing you must ask about his dreams twice
 *  as he checks and sets flag 243 (0xF3) when he first speaks of her.
 *
 *  2016-07-12 Written by Knight Captain
 */

void Edrin object#(0x410) () // NPC 16
{
	if (event == STARTED_TALKING) // DoubleClick would also work here.
	{
		if (gflags[0x02DB]) // 731 Dream_Realm_Complete is not defined in si_flags.uc yet.
            {
            gflags[EDRIN_KNOWS_SIRANUSH_IS_REAL] = true; // 531, 0x213 is only checked by Edrin.
            }
    }
    Edrin.original();
}
