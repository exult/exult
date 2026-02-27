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
 *
 *
 *  This file has been created from usecode found in the Exult CVS snapshot.
 *  I include it here only for convenience; I have edited it to fit the
 *  conventions used in the rest of the mod.
 *
 *	The original code was written by Jeff Freedman (aka "DrCode").
 *
 *	I modified the code so that the companions will have time to join
 *	before Xenka returns, as well as having a better post-resurrection,
 *	post-insanity dialog.
 */

void DumpBucket 0x88A (var bucket) {
	struct<ObjPos> target = UI_click_on_item();
	if (!target) {
		return;
	}

	UI_printf(["The shape clicked on is %s", UI_get_item_shape(target)]);

	// It's item, position.
	var npcnum = target.obj->get_npc_number();
	var bucket_quality = get_item_quality();
	var handled = false;

	if (get_item_frame() == 1) {
		if ((npcnum == CANTRA) &&  (bucket_quality == QUALITY_LOGIC)) {
			CANTRA->set_schedule_type(WAIT);
			// Without UI_set_path_failure, Cantra's would be stuck in waiting
			// mode if the player moved the Avatar before reaching Cantra:
			if (UI_path_run_usecode([target.x, target.y, target.z], CureCantra, bucket, SI_PATH_SUCCESS)) {
				UI_set_path_failure(CureCantra, bucket, SI_PATH_FAILURE);
			}
			abort;
		} else if (((npcnum == DUPRE) && (bucket_quality == QUALITY_DISCIPLINE)) ||
		    ((npcnum == SHAMINO) && (bucket_quality == QUALITY_ETHICALITY)) ||
		    ((npcnum == IOLO) && (bucket_quality == QUALITY_LOGIC))) {
			npcnum->set_schedule_type(WAIT);

			if (UI_path_run_usecode([target.x, target.y, target.z], CureCompanion, bucket, SI_PATH_SUCCESS)) {
				UI_set_path_failure(CureCompanion, bucket, SI_PATH_FAILURE);
			}
			abort;
		}
	}

	// Let orig. function handle it.
	target->set_intercept_item();
	DumpBucket.original(bucket);
}
