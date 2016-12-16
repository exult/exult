/*	Copyright (C) 2016  The Exult Team
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
 
/*	In the original game Ernesto is not a real NPC but embedded in the
 *	Ranger shape using the Ident number 1. He was being spawned by eggs
 *	that would duplicate him at times and disappear him at others. His
 *	original conversation code did not check for the items he was asked
 *	about, use the MET flag, nor notice the Avatar's tattoo. His checks
 *	for the Banes being released were also incorrect. To fix each issue
 *	he has been re-implemented.
 *
 *	He is excluded from the TEND_SHOP Ranger barks in the original.
 *
 *	2016-12-15 Written by Knight Captain.
 */

void Ernesto object#(0x4E9) ()
{
	var avatar_title = getPoliteTitle();
	var avatar_name = getAvatarName();
		
	if (event == DOUBLECLICK)
	{
		if (!ERNESTO->get_item_flag(MET))
		{
			delayedBark(AVATAR, "@Ranger!@", 0);
			delayedBark(ERNESTO, "@Yes?@", 0);
		}
		else
		{
			delayedBark(AVATAR, "@Ernesto?@", 0);
			delayedBark(ERNESTO, "@Yes?@", 0);
		}
		ERNESTO->set_schedule_type(TALK);
	}
	
	else if (event == STARTED_TALKING)
	{
		ERNESTO->run_schedule();
		ERNESTO->clear_item_say();
		UI_show_npc_face0(FACE_ERNESTO);

		if (!ERNESTO->get_item_flag(MET))
		{
			say("@My name is Ernesto, Lieutenant Commander of the Rangers. How may I be of service, ", avatar_title, "?@");
			ERNESTO->set_item_flag(MET);
		}
		else
			say("@It is good to see thee, ", avatar_name, ". How may I be of service?@");
		
		if ((gflags[KNOW_SLIPPERS_ARE_MUNDANE]) && (!gflags[KNOWS_SLIPPERS_OWNER]))
			add("slippers");

		if ((gflags[STORM_ICEWINE]) && (!gflags[KNOWS_ICEWINE_ORIGIN]))
			add("strange wine");

		// Rather than check if the human Simon is dead, check that he hasn't been confronted and swapped with Goblin Simon.
		if ((gflags[PICKED_UP_BROWN_BOTTLE]) && (!SIMON->get_schedule_type(WAIT)))
			add("brown bottle");

		if ((gflags[ASK_ERNESTO_ABOUT_SHIELD]) && (!gflags[KNOWS_MONITOR_SHIELD_ORIGIN]))
			add("mystery shield");

		// If the kidnapping has taken place, but Rotoluncia has not been slain yet.
		if ((gflags[COMPANION_KIDNAPPED]) && (!ROTOLUNCIA->get_item_flag(DEAD)))
			add("kidnap");
		
		add(["Ranger", "bye"]);

		converse (0)
		{
			case "kidnap" (remove):
				say("@This is such a terrible crime. And it involves magic!@");
				if (npcNearbyAndVisible(JULIA))
				{
					say("@Thou shouldst report such a crime as this to Julia. She can aid thee.@");
					JULIA->set_schedule_type(TALK);
					delayedBark(JULIA, "@Yes?@", 0);
				}
				else
					say("@Do not tell that I told thee this, but do not take such a matter to Julia.@");
					say("@She shall only extort a bribe, then send thee elsewhere.@");
					say("@Instead, speak to one of the Adepts. Perhaps Gustacio, if he'll give thee his ear, or Fedabiblio. They can help thee.@");

			case "Ranger" (remove):
				say("@A Ranger is in service to the MageLord and the Council. 'Tis our duty to guard the city and keep order...@");
				say("@We also make fine wine that is sold throughout all of Moonshade.@");
				add(["MageLord", "Council"]);
				
				case "MageLord" (remove):
				// The original code checks for the Bane Release and that Anti-Dupre's shape not in WAITING.
				// Checking the business activity of the Banes is problematic, since they are in WAITING until you enter their throne room.
				// Checking for the death of certain NPCs, like Filbercio, is also a problem because they are not DEAD until you are near their egg.
				// Only checking instead for the Banes being released.
				if (gflags[BANES_RELEASED])
				{
					say("@Shamino the Anarch is the current MageLord. He hath brought great change to our city...@");
					say("@No longer must a Mage or Mundane be bound by petty rules. It is quite exciting!@");
				}
				else
				{
					say("@The current MageLord is Filbercio, ", avatar_title, ".@");
					say("@He is a wise ruler, who ensures the safety of the people by keeping the Mages within the confines of the Strictures.@");
				}
					add("Strictures");
					
					case "Strictures" (remove):
						say("@I can only tell thee what is common knowledge, ", avatar_title, ". That the Strictures are meant to curtail the use of dangerous spells within the city.@");
						say("@That way, should anything go wrong during the casting, the Rangers do not have to worry about saving half the populace as well as containing the disaster.@");
					
				case "Council" (remove):
					say("@The Council is composed of Adepts that balance the MageLord's power. They represent the voice of the Mages within the city.@");
					say("@Of course, the Mundanes have little or no voice in the Council. I suppose that the Rangers are the closest thing to representatives for the Mundanes that Moonshade doth have.@");
					add(["Mages", "Mundanes"]);
					
					case "Mages" (remove):
						say("@Mages are those that make any use of magic, ", avatar_title, "... Adept or no. And since Moonshade is the city of Mages, it is the Mages that control the city.@");
						
					case "Mundanes" (remove):
						say("@Mundanes are those that cannot call upon magic without the use of magic items that contain a specific spell. They have no magical ability in and of themselves.@");
						say("@And here in Moonshade, I'm afraid, Mundanes are little better than chattel.@");
					
			case "slippers" (remove):
				if (hasItemCount(PARTY, 1, SHAPE_BOOTS, QUALITY_ANY, 5))
				{
					say("@Too small for me, I'm afraid. I don't think that these belong to any of the Rangers. Thou couldst ask Bucia.@");
				}
				else
				{	
					say("@I don't think what thou describe would belong to any of the Rangers. Thou couldst ask Bucia.@");
				}
				add("Bucia");
				
				case "Bucia" (remove):
					say("@She is Moonshade's provisioner, and she would know who bought such a pair, I'm sure... Bucia loves all sorts of information, if thou takest my meaning.@");
					say("@Thou canst usually find her at the Capessi Canton.@");

					case "Capessi Canton" (remove):
						say("@The Capessi Canton is Bucia's shop. Well, at least I know that she runs it, whether or not she owns it.@");
						say("@Thou canst find almost anything there. But remember, Bucia loves to talk. Don't say that I didn't warn thee.@");
					
			case "strange wine" (remove):
				if (hasItemCount(PARTY, 1, SHAPE_BOTTLE, QUALITY_ANY, 16))
				{
					say("@'Tis our wine, sure enough! Can't fool my nose after all these years working with the Mad Mage's wine press!@");
					say("@I wonder if this was the bottle that disappeared... Thou shouldst ask my commander about the wine.@");
					gflags[KNOWS_ICEWINE_ORIGIN] = true;
					add("Mad Mage");
				}
				else
				say("@As fond as I am of good wine, ", avatar_title, ", I cannot help thee unless I see the bottle. No offense, but thy description could fit several wines that I know of.@");
				
				case "Mad Mage" (remove):
					say("@Erstam is the Mad Mage, ", avatar_title, ".@");
					say("@Long ago, he was the teacher of the Adepts here in Moonshade. But then, like any man I suppose, he became obsessed with the thought of his own mortality.@");
					say("@His attempts to wrest life from death have been rather pathetic. And he hath gone completely mad.@");

			case "brown bottle" (remove):
				// Adding a new check since the original code does not have one.
				if (hasItemCount(PARTY, 1, SHAPE_BOTTLE, QUALITY_ANY, 9))
				{
					say("@Hmmm. I can't recall having seen a bottle like this one. But then I don't generally try any vintages but our own.@");
					say("@Thou shouldst ask Julia, though. If anyone would know, it would be Julia.@");
					add("Julia");
				}
				else
					say("@Without the bottle, ", avatar_title, ", there is little I canst tell thee.@");

				case "Julia" (remove):
				say("@Perhaps I should have referred to her as Commander, for that is what she is... Commander of the Rangers.@");
				// Don't badmouth the boss if she's around.
				if (!npcNearbyAndVisible(JULIA))
					say("@She's a bit sour, I'm afraid, ", avatar_title, ". Leading the Rangers is a thankless job, and she hath become quite disillusioned over the last few years.@");
				
			case "mystery shield" (remove):
				// Adding a new check since the original code does not have one.
				if (hasItemCount(PARTY, 1, SHAPE_MONITOR_SHIELD, QUALITY_ANY, FRAME_ANY))
					// New check for the tattoo.
					if (gflags[GOT_TATTOOED])
					{	
						say("@Surely thou art jesting, ", avatar_title, ". 'Tis another Pikeman's shield.@");
						say("@I wish that the Council would give us the funds for arms like this!@");
						say("@I went to Monitor once, as a young lad. That's what inspired me to become a Ranger.@");
					}
					else
					{
						say("@I thought that thou didst have a challenge for me! This one is easy... It is a Pikemen's shield from Monitor.@");
						say("@I wish that the Council would give us the funds for arms like this!@");
						add(["Pikemen", "Monitor"]);
					}
					
				else
					if (gflags[GOT_TATTOOED])
					{
						say("@If not a shield of Monitor, ", avatar_title, ", I knowest not what it could be.@");
						say("@If thou couldst show me thy shield, I may say for certain.@");
					}
					else
					{
						say("@Thy description of white and green makes me think of the Pikemen of Monitor, ", avatar_title, ".@");
						say("@If thou couldst show me thy shield, I may say for certain.@");
						add(["Pikemen", "Monitor"]);
					}
				
				case "Pikemen" (remove):
				say("@The Pikemen are to Monitor as the Rangers are to Moonshade, ", avatar_title, ". But more so...@");
				say("@For the Pikemen maintain guard towers all along the roads on the mainland. So they are much more widely respected than the Rangers.@");
				
				case "Monitor" (remove):
				say("@Monitor is on the southern edge of the mainland. Its citizens are noted for their bravery.@");
				say("@I went there once, as a young lad. That's what inspired me to become a Ranger.@");
				

			case "bye" (remove):
				UI_remove_npc_face0();
				UI_remove_npc_face1();
				delayedBark(AVATAR, "@Farewell!@", 0);
				delayedBark(ERNESTO, "@Peace be with thee.@", 2);
				break;
		}
	}
}