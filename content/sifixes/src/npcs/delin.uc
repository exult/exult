/*  Copyright (C) 2016  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  There is a very minor flag, only checked by Delin, that allows you to
 *  ask him about Batlin. It looks like it should have been set by Jendon
 *  per the flag order but that bit was removed sometime during SI's dev.
 *  Since setting flag 344 happens after asking Jendon about Batlin it is
 *  a way to check the Avatar has already knows to ask Delin about Batlin.
 *
 *  2016-07-11 Written by Knight Captain
 */

void Delin object#(0x42F) () // NPC 47
{
	if (event == STARTED_TALKING) // DoubleClick would also work here.
	{
		if (gflags[0x0158]) // 344 Ask_Delin_About_Batlin is not defined in si_flags.uc yet.
            {
            gflags[0x0156] = true; // 342 AskedAboutDaemonArtifacts is not defined either.
            }
    }
    Delin.original();
}
