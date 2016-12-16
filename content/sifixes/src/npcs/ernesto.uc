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

/*

	else if (event == DEATH)
	{
		UI_clear_item_flag(item, SI_TOURNAMENT);
		UI_reduce_health(item, 50, 0);
		gflags[ERNESTO_IS_DEAD] = true;
	}

	/*
	else if (event == PROXIMITY) /// WTF WHY DOESN"T THIS WORK!?
	{	
		var meows = ["@Leave Yurel alone!@", "@Stay away from Yurel!@", "@Yurel wants to go home!@", "@Oh, woe!@", "@Yurel hungry!@", "@Help me!@"];
		delayedBark(ERNESTO, meows[UI_get_random(UI_get_array_size(meows))], 0);
	}
	*/
/*	
		
		
		void Func0103 shape#(0x103) () // Ernesto the Ranger is only a Shape not a real NPC.
{

	var0000 = Func0994();
	var0001 = UI_(item);
	var0002 = Func0954();
	var0003 = UI_();
	var0004 = Func0953();
	var0005 = false;
	if (!((event == DEATH) && (var0001 == 0x0001))) goto labelFunc0103_004C;
	UI_clear_item_flag(item, 0x001D);
	UI_reduce_health(item, 0x0032, 0x0000);
	gflags[0x0120] = true;

labelFunc0103_0074:
	if (!(event == PROXIMITY)) goto labelFunc0103_00FC;
	if (!(var0001 != 0x0001)) goto labelFunc0103_00FC;
	var0006 = UI_(0x0006);
	if (!(var0006 == 0x0001)) goto labelFunc0103_00A2;
	UI_(item, "@Try our wine!@");
labelFunc0103_00A2:
	if (!(var0006 == 0x0002)) goto labelFunc0103_00B4;
	UI_(item, "@Yes, ma'am! Back to work!@");
labelFunc0103_00B4:
	if (!(var0006 == 0x0003)) goto labelFunc0103_00C6;
	UI_(item, "@Work, work, work...@");
labelFunc0103_00C6:
	if (!(var0006 == 0x0004)) goto labelFunc0103_00D8;
	UI_(item, "@What art thou doing?@");
labelFunc0103_00D8:
	if (!(var0006 == 0x0005)) goto labelFunc0103_00EA;
	UI_(item, "@Come and see the wine press!@");
labelFunc0103_00EA:
	if (!(var0006 == 0x0006)) goto labelFunc0103_00FC;
	UI_(item, "@Magic wine!@");


		
		
		item->funcRanger();
	}
	
}

*/
/*
		
		
		
		
		say("@No truer friend can I have than thee, ", avatar_title,
				"! Not only thou didst save my wife, but thou hast saved me as well... Thank thee, my friend!@");
				
					message("\"My name is Ernesto the Ranger. How may I be of service, ");
	message(var0002);
	message("?\"");
		

		add(["name", "bye"]);
		if (hasItemCount(PARTY, 1, SHAPE_BUCKET, 9, 2))
			add("got blood");

		converse (0)
		{
			case "got blood" (remove):
				say("@Hurry! Take Ice Dragon blood to Yenani! No time to delay!@");
				abort;

			case "name" (remove):
	
		
		delayedBark(VASCULIO, "@Ahhh, a victim...@", 1);
		
		delayedBark(VASCULIO, "@Ahhh, a victim...@", 1);
		partyUtters(1, "@Spooky place.@", "@I'm scared alone.@", true);
		VASCULIO->set_new_schedules(DAWN, STANDTHERE, [0104, 0035]); // 0104, 0035 // [0x68, 0x23]
		VASCULIO->set_schedule_type(TALK); // Will approach the Avatar if not in Waiting.
	}
		
			if (!(event == DOUBLECLICK)) goto labelFunc0103_0074;
	UI_(0xFE9C, "@Ranger!@");
	item->Func07D1();
	Func097F(item, "@Yes?@", 0x0002);
	UI_(item, 0x0003);
		
		
		item->funcRanger();
		// The Ranger shape code does not set the MET flag, so it is added here.
		// Does this work because his function returns vs. aborts?
		ERNESTO->set_item_flag(MET);
		
		



	else if (event == DEATH)
	{
		// Replaces the original death code which sets a gflag to prevent respawning him.
		// This adds hit points back to him when he "dies" if the SI_TOURNAMENT flag is set.
		ERNESTO->set_npc_prop(HEALTH, 10);
	}



{
	if (event == DOUBLECLICK)
	{
		item->funcRanger();
	}
	else if (event == STARTED_TALKING)
	{
		item->funcRanger();
		ERNESTO->set_item_flag(MET);
	}
	else if (event == PROXIMITY)
	{
		item->funcRanger();
	}
	else if (event == DEATH)
	{
		item->funcRanger();
	}
}
*/



/*
void Func0103 shape#(0x103) () // Ernesto the Ranger is only a Shape not a real NPC.
{
	var var0000;
	var var0001;
	var var0002;
	var var0003;
	var var0004;
	var var0005;
	var var0006;

	var0000 = Func0994();
	var0001 = UI_(item);
	var0002 = Func0954();
	var0003 = UI_();
	var0004 = Func0953();
	var0005 = false;
	if (!((event == DEATH) && (var0001 == 0x0001))) goto labelFunc0103_004C;
	UI_(item, 0x001D);
	UI_(item, 0x0032, 0x0000);
	gflags[0x0120] = true;
labelFunc0103_004C:
	if (!(event == DOUBLECLICK)) goto labelFunc0103_0074;
	UI_(0xFE9C, "@Ranger!@");
	item->Func07D1();
	Func097F(item, "@Yes?@", 0x0002);
	UI_(item, 0x0003);
labelFunc0103_0074:
	if (!(event == PROXIMITY)) goto labelFunc0103_00FC;
	if (!(var0001 != 0x0001)) goto labelFunc0103_00FC;
	var0006 = UI_(0x0006);
	if (!(var0006 == 0x0001)) goto labelFunc0103_00A2;
	UI_(item, "@Try our wine!@");
labelFunc0103_00A2:
	if (!(var0006 == 0x0002)) goto labelFunc0103_00B4;
	UI_(item, "@Yes, ma'am! Back to work!@");
labelFunc0103_00B4:
	if (!(var0006 == 0x0003)) goto labelFunc0103_00C6;
	UI_(item, "@Work, work, work...@");
labelFunc0103_00C6:
	if (!(var0006 == 0x0004)) goto labelFunc0103_00D8;
	UI_(item, "@What art thou doing?@");
labelFunc0103_00D8:
	if (!(var0006 == 0x0005)) goto labelFunc0103_00EA;
	UI_(item, "@Come and see the wine press!@");
labelFunc0103_00EA:
	if (!(var0006 == 0x0006)) goto labelFunc0103_00FC;
	UI_(item, "@Magic wine!@");
labelFunc0103_00FC:
	if (!(event == STARTED_TALKING)) goto labelFunc0103_0732;
	if (!(var0001 == 0x0000)) goto labelFunc0103_0407;
	UI_(item, 0x0007);
	UI_(item);
	UI_(0xFEEE, 0x0000);
	message("\"How may I help thee? As a Ranger, I must help thee in any way that I can.\"");
	say();
	if (!(gflags[0x0285] && (!gflags[0x0297]))) goto labelFunc0103_013B;
	UI_("strange wine");
labelFunc0103_013B:
	if (!(gflags[0x0045] && (UI_(0xFFB4) != 0x000F))) goto labelFunc0103_0154;
	UI_("brown bottle");
labelFunc0103_0154:
	if (!(gflags[0x0118] && (!gflags[0x029C]))) goto labelFunc0103_0166;
	UI_("mystery shield");
labelFunc0103_0166:
	if (!(gflags[0x00EA] && (!UI_(0xFFE1, 0x0004)))) goto labelFunc0103_017F;
	UI_("kidnap");
labelFunc0103_017F:
	UI_(["Ranger", "bye"]);
labelFunc0103_018C:
	converse (0) atend labelFunc0103_0406;
	case "kidnap" atend labelFunc0103_01C2:
	UI_("kidnap");
	message("\"Thou shouldst report such a crime as this to Julia. She can aid thee.\"");
	say();
	if (!UI_(0xFFE9)) goto labelFunc0103_01C2;
	UI_(0xFFE9, 0x0003);
	Func097F(0xFFE9, "@Yes?@", 0x0000);
labelFunc0103_01C2:
	case "Ranger" atend labelFunc0103_01EC:
	UI_("Ranger");
	message("\"A Ranger is much like any other city guard, ");
	message(var0002);
	message(". It is our job to catch thieves, and stop fights. Nothing very exciting...\"");
	say();
	message("\"...Until one of the Mages manages to destroy a section of town with his experiments. Then things become lively.\"");
	say();
	UI_(["Mage", "lively"]);
labelFunc0103_01EC:
	case "Mage" atend labelFunc0103_021A:
	UI_("Mage");
	message("\"Moonshade is the city of Mages, ");
	message(var0002);
	message(".\"");
	say();
	message("\"Mages are very powerful, and many of them can do things that make my skin crawl! And oftimes their experiments escape and then the Rangers must step in and take control.\"");
	say();
	message("\"But, 'tis my job... cleaning after the Mages.\"");
	say();
	UI_(["things", "escape"]);
labelFunc0103_021A:
	case "things" atend labelFunc0103_0237:
	UI_("things");
	message("\"I pretend I do not see half of what the Mages can do, ");
	message(var0002);
	message(". I find 'tis easier to keep my sanity that way.\"");
	say();
	message("\"Some can summon fire or ice. Others can summon storms or daemons. And none of it I want to know about, to tell thee truth.\"");
	say();
labelFunc0103_0237:
	case "escape" atend labelFunc0103_0252:
	UI_("escape");
	message("\"Either the mage tries a spell that is too advanced for him, or he simply loses concentration. I do not know.\"");
	say();
	message("\"All I know is that suddenly we have people running through the streets trying to escape whatever the Mage was attempting to conjure...\"");
	say();
	message("\"And 'tis no mean feat for us poor Rangers to overcome something that overpowered a Mage, I can tell thee!\"");
	say();
labelFunc0103_0252:
	case "lively" atend labelFunc0103_0276:
	UI_("lively");
	message("\"Oh, yes! It might be as simple as putting out a fire and rebuilding a section or two of wall after.\"");
	say();
	message("\"But sometimes 'tis far more dangerous... Like a dragon, or one of Rotoluncia's daemons, or somesuch. Never know from one day to the next what thou wilt have to face.\"");
	say();
	UI_(["dragon", "daemon"]);
labelFunc0103_0276:
	case "dragon" atend labelFunc0103_0290:
	UI_("dragon");
	message("\"To be honest, I have never had to face one myself. But my commander remembers when she was a new Ranger, when the Strictures were newly in place. A Mage summoned a dragon that burned half of the city.\"");
	say();
	UI_("Strictures");
labelFunc0103_0290:
	case "Strictures" atend labelFunc0103_02A3:
	UI_("Strictures");
	message("\"I cannot tell thee much about them... Not being a Mage and all. Perhaps thou couldst ask a Mage.\"");
	say();
labelFunc0103_02A3:
	case "Rotoluncia" atend labelFunc0103_02BA:
	UI_("Rotoluncia");
	message("\"Rotoluncia's forte is fire spells. So I suppose it follows that she would seek to summon daemons to her service.\"");
	say();
	message("\"To be truthful, she hath not yet succeeded in binding a daemon. But it disturbs me to think that she might... and that I might have to face one some day.\"");
	say();
labelFunc0103_02BA:
	case "daemons" atend labelFunc0103_02D1:
	UI_("daemons");
	message("\"Everyone says that daemons come from the hot depths of the world. That they are foul creatures of flame and melted flesh.\"");
	say();
	message("\"I hope never to find if it is true...\"");
	say();
labelFunc0103_02D1:
	case "strange wine" atend labelFunc0103_0313:
	UI_("strange wine");
	if (!Func097D(0xFE9B, 0x0001, 0x0268, 0xFE99, 0x0010)) goto labelFunc0103_0302;
	message("\"It looks to be wine from our press, ");
	message(var0002);
	message(". But thou shouldst ask my commander, to be sure.\"");
	say();
	goto labelFunc0103_030C;
labelFunc0103_0302:
	message("\"'Tis true that we deal in wine, ");
	message(var0002);
	message(". But without seeing the bottle, I would not be able to help thee.\"");
	say();
labelFunc0103_030C:
	UI_("commander");
labelFunc0103_0313:
	case "commander" atend labelFunc0103_0334:
	UI_("commander");
	message("\"Julia is our commander, ");
	message(var0002);
	message(". She's a stern woman, but worthy of thy respect.\"");
	say();
	message("\"There are those that consider her a handsome woman, when her mouth is closed... If thou knowest what I mean.\"");
	say();
	var0005 = true;
labelFunc0103_0334:
	case "brown bottle" atend labelFunc0103_0360:
	UI_("brown bottle");
	message("\"What an odd bottle! Who would bottle their wares in such an ugly container? I hope that whatever was inside is not as coarse as the bottle suggests.\"");
	say();
	message("\"Our commander oversees the bottling of our wine, ");
	message(var0002);
	message(". Thou mightest ask if she knows who doth use such a bottle, but I would not stake much on it, if I were thee.\"");
	say();
	if (!(var0005 == false)) goto labelFunc0103_0360;
	UI_("commander");
labelFunc0103_0360:
	case "mystery shield" atend labelFunc0103_038E:
	UI_("mystery shield");
	message("\"It certainly is a fine looking piece of equipment, ");
	message(var0002);
	message(". I wish we had arms as nice.\"");
	say();
	message("\"Thou shouldst ask Ernesto if he recognizes it. He is the arms expert among the Rangers.\"");
	say();
	message("\"On a guess, I'd say it came from Monitor. Everyone knows that they practically sleep with weapons such as this.\"");
	say();
	UI_(["Ernesto", "Monitor"]);
labelFunc0103_038E:
	case "Ernesto" atend labelFunc0103_03AE:
	UI_("Ernesto");
	message("\"Ernesto is our second in command, ");
	message(var0002);
	message(". Thou wilt find him somewhere around the wine press, I'll warrant.\"");
	say();
	UI_("wine press");
labelFunc0103_03AE:
	case "wine press" atend labelFunc0103_03CB:
	UI_("wine press");
	message("\"One other thing that the Rangers do, ");
	message(var0002);
	message(", is produce the wine sold throughout Moonshade.\"");
	say();
	message("\"Our wine press is very special. I'm certain that either Ernesto or the commander would be happy to show it to thee. We are all very proud of it.\"");
	say();
labelFunc0103_03CB:
	case "Monitor" atend labelFunc0103_03DE:
	UI_("Monitor");
	message("\"Monitor is on the mainland. 'Tis the city of warriors. That is why I thought the shield might be from there.\"");
	say();
labelFunc0103_03DE:
	case "bye" atend labelFunc0103_0403:
	UI_();
	Func097F(0xFE9C, "@So long!@", 0x0000);
	Func097F(item, "@Fare well!@", 0x0002);
	goto labelFunc0103_0406;
labelFunc0103_0403:
	goto labelFunc0103_018C;
labelFunc0103_0406:
	break;
labelFunc0103_0407:
	if (!(var0001 == 0x0001)) goto labelFunc0103_0732;
	UI_(item, 0x0007);
	UI_(item);
	UI_(0xFEE7, 0x0000);
	message("\"My name is Ernesto the Ranger. How may I be of service, ");
	message(var0002);
	message("?\"");
	say();
	if (!(gflags[0x0115] && (!gflags[0x0293]))) goto labelFunc0103_0444;
	UI_("slippers");
labelFunc0103_0444:
	if (!(gflags[0x0285] && (!gflags[0x0297]))) goto labelFunc0103_0456;
	UI_("strange wine");
labelFunc0103_0456:
	if (!(gflags[0x0045] && (UI_(0xFFB4) != 0x000F))) goto labelFunc0103_046F;
	UI_("brown bottle");
labelFunc0103_046F:
	if (!(gflags[0x0118] && (!gflags[0x029C]))) goto labelFunc0103_0481;
	UI_("mystery shield");
labelFunc0103_0481:
	if (!(gflags[0x00EA] && (!UI_(0xFFE1, 0x0004)))) goto labelFunc0103_049A;
	UI_("kidnap");
labelFunc0103_049A:
	UI_(["Ranger", "bye"]);
labelFunc0103_04A7:
	converse (0) atend labelFunc0103_0731;
	case "kidnap" atend labelFunc0103_04F0:
	UI_("kidnap");
	message("\"This is such a terrible crime. And it involves magic!");
	say();
	if (!UI_(0xFFE9)) goto labelFunc0103_04E4;
	message("\"Thou shouldst report such a crime as this to Julia. She can aid thee.\"");
	say();
	UI_(0xFFE9, 0x0003);
	Func097F(0xFFE9, "@Yes?@", 0x0000);
	goto labelFunc0103_04F0;
labelFunc0103_04E4:
	message("\"Do not tell that I told thee this, but do not take such a matter to Julia -- \"");
	say();
	message("\"She shall only extort a bribe, then send thee elsewhere.\"");
	say();
	message("\"Instead, speak to one of the Adepts. Perhaps Gustacio, if he'll give thee his ear, or Fedabiblio. They can help thee.\"");
	say();
labelFunc0103_04F0:
	case "Ranger" atend labelFunc0103_0514:
	UI_("Ranger");
	message("\"A Ranger is in service to the MageLord and the Council. 'Tis our duty to guard the city and keep order...\"");
	say();
	message("\"We also make fine wine that is sold throughout all of Moonshade.\"");
	say();
	UI_(["MageLord", "Council"]);
labelFunc0103_0514:
	case "MageLord" atend labelFunc0103_0551:
	UI_("MageLord");
	if (!(gflags[0x0004] && (UI_(0x038A) != 0x000F))) goto labelFunc0103_0540;
	message("\"Shamino the Anarch is the current MageLord. He hath brought great change to our city...\"");
	say();
	message("\"No longer must a Mage or Mundane be bound by petty rules. It is quite exciting!\"");
	say();
	goto labelFunc0103_0551;
labelFunc0103_0540:
	message("\"The current MageLord is Filbercio, ");
	message(var0002);
	message(". He is a wise ruler, who ensures the safety of the people by keeping the Mages within the confines of the Strictures.\"");
	say();
	UI_("Strictures");
labelFunc0103_0551:
	case "Strictures" atend labelFunc0103_056E:
	UI_("Strictures");
	message("\"I can only tell thee what is common knowledge, ");
	message(var0002);
	message(". That the Strictures are meant to curtail the use of dangerous spells within the city.\"");
	say();
	message("\"That way, should anything go wrong during the casting, the Rangers do not have to worry about saving half the populace as well as containing the disaster.\"");
	say();
labelFunc0103_056E:
	case "Council" atend labelFunc0103_0592:
	UI_("Council");
	message("\"The Council is composed of Adepts that balance the MageLord's power. They represent the voice of the Mages within the city.\"");
	say();
	message("\"Of course, the Mundanes have little or no voice in the Council. I suppose that the Rangers are the closest thing to representatives for the Mundanes that Moonshade doth have.\"");
	say();
	UI_(["Mages", "Mundanes"]);
labelFunc0103_0592:
	case "Mages" atend labelFunc0103_05AB:
	UI_("Mages");
	message("\"Mages are those that make any use of magic, ");
	message(var0002);
	message("... Adept or no. And since Moonshade is the city of Mages, it is the Mages that control the city.\"");
	say();
labelFunc0103_05AB:
	case "Mundanes" atend labelFunc0103_05C2:
	UI_("Mundanes");
	message("\"Mundanes are those that cannot call upon magic without the use of magic items that contain a specific spell. They have no magical ability in and of themselves.\"");
	say();
	message("\"And here in Moonshade, I'm afraid, Mundanes are little better than chattel.\"");
	say();
labelFunc0103_05C2:
	case "slippers" atend labelFunc0103_05DC:
	UI_("slippers");
	message("\"Too small for me, I'm afraid. I don't think that these belong to any of the Rangers. Thou couldst ask Bucia.\"");
	say();
	UI_("Bucia");
labelFunc0103_05DC:
	case "Bucia" atend labelFunc0103_05FA:
	UI_("Bucia");
	message("\"She is Moonshade's provisioner, and she would know who bought such a pair, I'm sure... Bucia loves all sorts of information, if thou takest my meaning.\"");
	say();
	message("\"Thou canst usually find her at the Capessi Canton.\"");
	say();
	UI_("Capessi Canton");
labelFunc0103_05FA:
	case "Capessi Canton" atend labelFunc0103_0611:
	UI_("Capessi Canton");
	message("\"The Capessi Canton is Bucia's shop. Well, at least I know that she runs it, whether or not she owns it.\"");
	say();
	message("\"Thou canst find almost anything there. But remember, Bucia loves to talk. Don't say that I didn't warn thee.\"");
	say();
labelFunc0103_0611:
	case "strange wine" atend labelFunc0103_0655:
	UI_("strange wine");
	if (!Func097D(0xFE9B, 0x0001, 0x0268, 0xFE99, 0x0010)) goto labelFunc0103_064B;
	message("\"'Tis our wine, sure enough! Can't fool my nose after all these years working with the Mad Mage's wine press!\"");
	say();
	message("\"I wonder if this was the bottle that disappeared... Thou shouldst ask my commander about the wine.\"");
	say();
	gflags[0x0297] = true;
	UI_("Mad Mage");
	goto labelFunc0103_0655;
labelFunc0103_064B:
	message("\"As fond as I am of good wine, ");
	message(var0002);
	message(", I cannot help thee unless I see the bottle. No offense, but thy description could fit several wines that I know of.\"");
	say();
labelFunc0103_0655:
	case "Mad Mage" atend labelFunc0103_0676:
	UI_("Mad Mage");
	message("\"Erstam is the Mad Mage, ");
	message(var0002);
	message(".\"");
	say();
	message("\"Long ago, he was the teacher of the Adepts here in Moonshade. But then, like any man I suppose, he became obsessed with the thought of his own mortality.\"");
	say();
	message("\"His attempts to wrest life from death have been rather pathetic. And he hath gone completely mad.\"");
	say();
labelFunc0103_0676:
	case "brown bottle" atend labelFunc0103_0694:
	UI_("brown bottle");
	message("\"Hmmm. I can't recall having seen a bottle like this one. But then I don't generally try any vintages but our own.\"");
	say();
	message("\"Thou shouldst ask Julia, though. If anyone would know, it would be Julia.\"");
	say();
	UI_("Julia");
labelFunc0103_0694:
	case "Julia" atend labelFunc0103_06B1:
	UI_("Julia");
	message("\"Perhaps I should have referred to her as Commander, for that is what she is... Commander of the Rangers.\"");
	say();
	message("\"She's a bit sour, I'm afraid, ");
	message(var0002);
	message(". Leading the Rangers is a thankless job, and she hath become quite disillusioned over the last few years.\"");
	say();
labelFunc0103_06B1:
	case "mystery shield" atend labelFunc0103_06D5:
	UI_("mystery shield");
	message("\"I thought that thou didst have a challenge for me! This one is easy... It is a Pikemen's shield from Monitor.\"");
	say();
	message("\"I wish that the Council would give us the funds for arms like this!\"");
	say();
	UI_(["Pikemen", "Monitor"]);
labelFunc0103_06D5:
	case "Pikemen" atend labelFunc0103_06F2:
	UI_("Pikemen");
	message("\"The Pikemen are to Monitor as the Rangers are to Moonshade, ");
	message(var0002);
	message(". But more so...\"");
	say();
	message("\"For the Pikemen maintain guard towers all along the roads on the mainland. So they are much more widely respected than the Rangers.\"");
	say();
labelFunc0103_06F2:
	case "Monitor" atend labelFunc0103_0709:
	UI_("Monitor");
	message("\"Monitor is on the southern edge of the mainland. Its citizens are noted for their bravery.\"");
	say();
	message("\"I went there once, as a young lad. That's what inspired me to become a Ranger.\"");
	say();
labelFunc0103_0709:
	case "bye" atend labelFunc0103_072E:
	UI_();
	Func097F(0xFE9C, "@Farewell!@", 0x0000);
	Func097F(item, "@Peace be with thee.@", 0x0002);
	goto labelFunc0103_0731;
labelFunc0103_072E:
	goto labelFunc0103_04A7;
labelFunc0103_0731:
	break;
labelFunc0103_0732:
	if (!(var0001 == 0x0002)) goto labelFunc0103_0765;
	UI_(item);
	UI_(0xFEEE, 0x0000);
	message("\"Please, I cannot speak with thee at this time. My grief doth take the words from my mouth and leave me only with tears.\" ~\"Please away, I do not wish to be seen in this state.\"");
	say();
	Func097F(item, "@If only I had flowers...@", 0x0002);
	UI_(item, 0x000F);
	UI_();
labelFunc0103_0765:
	return;
}
*/