/*  Copyright (C) 2016  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  The flags for the white breast plate are not well-handled. If you
 *  learn that the ceremonial armor was created by Standarr for Kylista,
 *  or if you ask Jendon about the item, they will set flag 660 (0x294).
 *  If set, you cannot ask Kylista about the breastplate and so cannot
 *  return it to her. She also will set the flag if you ask her about it
 *  without having it carried by the party, or if you refuse to return it.
 *
 *  Depending on the outcome of the Fawn Trial, Kylista may be arrested.
 *  If she is, you cannot ask about the breastplate anyway as the
 *  post-arrest conversation does not check for the breastplate flags.
 *
 *  2016-07-14 Written by Knight Captain
 */
 
void Kylista object#(0x436) () // NPC 54
{
	if (event == STARTED_TALKING) // DoubleClick would also work here.
	{
		// If Kylista does not have the breastplate.
		if (!KYLISTA->get_cont_items(SHAPE_BREAST_PLATE, QUALITY_ANY, FRAME_ANY)) 
		{   
			// And no one is currently on trial OR Kylista has not been arrested after the trial.
            if ((!(gflags[DUPRE_ACCUSED] || gflags[IOLO_ACCUSED] || gflags[SHAMINO_ACCUSED])) || ((gflags[AUDIENCE_WITH_YELINDA] && gflags[FAWN_TRIAL_DONE] && gflags[ORACLE_SET_TO_INNOCENT]))) 
                {
                if (gflags[KNOWS_BREAST_PLATE_OWNER]) // If we know she's the rightful owner, flag 660.
                    {
                    gflags[KNOWS_BREAST_PLATE_OWNER] = false; // We clear the flag so we can ask her about it.
                    }
                }
            }
    }
    Kylista.original();
}
