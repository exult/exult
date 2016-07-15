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
 *  2016-07-07 List updated to display items in the order the party finds them,
 *  instead of the original haphazard list. Order is Avatar, Shamino, Dupre, Iolo.
 *  Also fixes the spacing so adding companion-authors does not push text down.
 *
 *  2016-07-15 Added Quality 122 section to allow accusing Marsten of betraying
 *  his city, and if Pomdirgun is dead, sets the flag Monitor NPCs check.
 */

void Scroll shape#(0x31D) () // 797
{
	var quality = get_item_quality();
	UI_play_sound_effect2(0x5E, item);
	book_mode();
    if (quality == 0x7A) // Quality 122, found in the Goblin King's treasure room, signed by Marsten, Lord of Monitor.
	{
        gflags[0x36] = true; // Marsten can be accused.
        if (UI_get_item_flag(GOBLIN_KING, DEAD)) // If the Goblin King is dead.
        {
            gflags[0xCC] = true; // Conversations in Monitor will reflect his death.
            // Needs to be added to si_gflags.uc as: POMDIRGUN_IS_DEAD
            // If set, changes or adds conversation with Harnna, Shazzana, Standarr, Templar, Brendann, Caladin,
            // and also looks to add a bark about it to Brendann. No one outside of Monitor checks this,
            // so the dialog in Fawn won’t change. Similar to the original Origin bug with Harnna and the Strange Coins,
            // if Flag 204 is set you can repeatedly ask Standarr about Pomdirgun, the option does not get removed.
        }
    }
    
	if (quality == 0xBD) // Quality 189, the Equipment Scroll.
	{

		say("The list of items which we found ourselves with after the storm:~"); // removed one ~
		say("Prepared by Shamino.");
		if (gflags[DUPRE_MADE_EQUIPMENT_LIST])
			say("With additional notes by Dupre.");
        if (!gflags[DUPRE_MADE_EQUIPMENT_LIST])
            say (""); // added a blank line
		if (gflags[IOLO_MADE_EQUIPMENT_LIST])
			say("And further comments by Iolo, since being freed from that vile Monitorian prison cell!");
        if (!gflags[IOLO_MADE_EQUIPMENT_LIST])
            say ("~~"); // Iolo's comment gets to three lines.
		say(""); // removed two ~

		if (gflags[STORM_PINECONE]) // Avatar items
		{
			if (gflags[KNOWS_PINECONE_OWNER])
				say("A pinecone from the northern woods.");
			else
				say("A pinecone (or, at least, it appears to be one).");
		}

		if (gflags[STORM_STOCKINGS]) // Avatar 
		{
			if (gflags[KNOWS_MOONSILK_OWNER])
				say("A pair of moonsilk stockings, such as the enchantress Columna doth wear.");
			else
				say("A fine pair of sheer stockings, probably women's attire.");
		}

		if (gflags[STORM_LAB_APPARATUS]) // Avatar 
		{
			if (gflags[KNOWS_LAB_APPARATUS_OWNER])
				say("The missing apparatus from the laboratory of Erstam, the so-called Mad Mage.");
			else if (gflags[HAS_CLUE_LAB_APPARATUS])
				say("A specimen of laboratory apparatus from a mage's laboratory.");
			else
				say("A strange apparatus of glass and copper.");
		}

		if (gflags[STORM_PUMICE]) // Avatar 
		{
			if (gflags[KNOWS_PUMICE_ORIGIN])
				say("A pumice rock from the fiery depths of some dungeon.");
			else
				say("A rock.");
		}

		if (gflags[STORM_WEDDING_RING]) // Avatar 
		{
			if (gflags[KNOWS_RING_OWNER])
				say("The engagement ring belonging to Alyssand of Fawn.");
			else if (gflags[HAS_CLUE_RING])
				say("A finely crafted silver ring, probably the lost engagement ring of a lass named Alyssand.");
			else
				say("A finely crafted ring, of silver, of a size to fit a small woman or a child.");
		}

		if (gflags[STORM_FUR_CAP]) // Avatar 
		{
			if (gflags[KNOWS_FURCAP_OWNER] || gflags[GAVE_FURCAP_BACK])
				say("The elegant fur cap which Filbercio the MageLord purchased for his favorite, the sorceress Frigidazzi.");
			else if (gflags[HAS_CLUE_FURCAP])
				say("An expensive fur cap, which the MageLord of Moonshade obtained for one of his many lovers.");
			else
				say("A ridiculous fur cap.");
		}

		if (gflags[STORM_BREAST_PLATE]) // Avatar 
		{
			if (gflags[KNOWS_BREAST_PLATE_OWNER])
				say("The ceremonial breastplate of the Priestess of Beauty, who is Kylista of Fawn. A very attractive lady, I should add.");
			else
				say("An enameled breastplate, suitable for ceremonial occasions.");
		}
        
        if (gflags[STORM_ICEWINE]) // Avatar 
		{
			if (gflags[KNOWS_ICEWINE_ORIGIN])
				say("A bottle of that excellent vintage of wine sold by the Rangers of Moonshade. Why, I should sample some now...");
			else
				say("A bottle of ice wine -- whatever that is!");
		}

        if (gflags[STORM_FILARI]) // Avatar 
		{
			if (gflags[KNOWS_FILARI_OWNER])
				say("Jeweled coins from the City of Beauty, Fawn.");
			else
				say("Strange baubles -- silver disks with jewels in the center.");
		}
        
        if (gflags[STORM_RUDDY_ROCK]) // Avatar 
		{
			if (gflags[KNOWS_STONEHEART_ORIGIN])
				say("The dangerous mineral known as Stoneheart, which is used to produce the illegal reagent Bloodspawn.");
			else
				say("A red hunk of stone.");
		}
        
       	if (gflags[STORM_SLIPPERS]) // Shamino items
		{
			if (gflags[KNOWS_SLIPPERS_OWNER])
				say("The well-worn slippers belonging to Devra, the mistress of the Inn of the Sleeping Bull.");
			else
				say("Some very old and worn slippers, such as might be worn in the privacy of one's home.");
		}
        
        if (gflags[STORM_GOBLIN_BRUSH]) // Shamino
		{
			if (gflags[KNOWS_GOBLIN_BRUSH_ORIGIN])
				say("A grisly brush made from the bones of some poor victim of the Goblins. How foul!");
			else
				say("A crude brush.");
		}

		if (gflags[STORM_BEAR_SKULL]) // Shamino
		{
			if (gflags[KNOWS_BEAR_SKULL_ORIGIN])
				say("The skull of a great mountain bear.");
			else
				say("A large skull, no doubt belonging to some large and dead animal.");
		}
        
		if (gflags[STORM_SEVERED_HAND]) // Shamino
		{
			if (gflags[KNOWS_SEVERED_HAND_OWNER])
				say("The severed hand from one of the Mad Mage's experiments. It is not dead, yet not living -- it doth not decay.");
			else
				say("A bloody hand, severed from its corpse. It shows no sign of decay, yet...");
		}

   		if (gflags[STORM_BLUE_EGG]) // Dupre items
		{
			if (gflags[KNOWS_BLUE_EGG_OWNER])
				say("A penguin egg, such as may be found in the ice fields of the distant north.");
			else
				say("A strange blue egg.");
		}

		if (gflags[STORM_MONITOR_SHIELD]) // Dupre 
		{
			if (gflags[KNOWS_MONITOR_SHIELD_ORIGIN])
				// I changed this one from the original:
 				// say("One of the common shields used by the Pikemen of Monitor.");
				say("A sturdy shield belonging to Luther of Monitor.");
 				// Added a brand new one:
			else if (gflags[HAS_CLUE_MONITOR_SHIELD])
				say("A shield from Monitor which possibly belongs to Luther.");
			else
				say("An inexpensive shield, sturdy and suitable for battle.");
		}

        if (gflags[STORM_URN]) //Iolo rejoins the party last
		{
			if (gflags[KNOWS_URN_ORIGIN])
				say("A funerary urn containing the Ashes of the Dead, taken from the Caves of Monitor.");
			else
				say("Some sort of vase, with soot inside.");
		}
	}
	else
		Scroll.original();
}
