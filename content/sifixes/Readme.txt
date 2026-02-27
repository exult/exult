========================================================
Miscellaneous fixes to Serpent Isle Usecode
========================================================

0	Table of Contents
---------------------
	0	Table of Contents
	1	About this document
	2	Installing the mod
	3	Contributing to this mod
	4	Using it in your own mods
	5	What this mod does
	6	Version history


1	About this document
-----------------------
Author: Marzo Sette Torres Junior
e-mail: marzojr@taskmail.com.br
version: 0.15.15

	This modification fixes a few of the Usecode bugs that plague Ultima VII:
	Serpent Isle. I have tested it *only* with Silver Seed installed; while it
	*might* work without the add-on, I woudn't count on it.

	A potion of the code (the healing of Cantra) is based on Usecode available
	with the Exult source code in the "content/si" directory. I did some minor
	tweaks and some reorganization to make it more compatible with the rest of
	the mod's usecode. I am including it in this mod because many people would
	never know about it otherwise... The healing of Cantra was written by Jeff
	Freedman (aka "DrCode"), and has been split into the files "npcs/cantra.uc"
	and "items/bucketcure.uc".
	
	Knight Captain has provided fixes to version 0.15.12 and later editions.

	Everything else has been my own work, although the structure of the mod is
	based on Alun Bestor's Quests and Iteractions mod. Many fixes were based on
	the document found at "docs/usecode_bugs.txt" on the Exult source.

2	Installing the mod
----------------------
	Note: These instructions are for installing from the zip distribution found
	on my website. If you want to build it yourself, its source code is included
	with Exult's source in the "content/sifixes directory". In it, there is a
	README file with instructions for building it.
	
	First, I must be explicit that I have tested this mod *only* with the Silver
	Seed add-on installed; I don't know if it will work or not without.
	
	Secondly (and just as importantly), this mod is only guaranteed to work on
	*the* latest Exult snapshots. It will *not* work in Exult 1.2.

	With that out of the way: to install the mod, simply unzip the contents of
	the zip file into your Silver Seed's 'mods' folder. By default (that is,
	unless you changed it specifically in your 'Exult.cfg' file), this would be
	the 'mods' subdirectory of the path you set for Silver Seed in your
	'Exult.cfg' file. If the folder is not there, simply create an empty folder
	called 'mods' in your Silver Seed directory and proceed as above.
	
	There is an optional (but highly recommended) additional step you may want
	to perform:
	
	OPTIONAL: You can create a shortcut to start Exult directly in the SI Fixes
	mod. To do so, make sure that the command-line is similar to the following
	example:
		<path to Exult>\Exult --si --mod sifixes
	You can use the supplied 'SIFixes.ico' file for the shortcut (if you use
	Windows) or use the supplied 'SIFixes.png' file to make your own icon for
	other OSes.

3	Contributing to this mod
----------------------------
	If you have any bugs that you would like to see fixed -- or that you *have*
	fixed -- please send them to me! You can either e-mail them to me or you can
	post them in the Exult Phorum.

4	Using it in your own mods
-----------------------------
	You are free to use anything I have written in your own mods; I ask only for
	proper credit -- and maybe tell me about the mod you are doing, as I might
	be interested enough to help and/or to include as a part of this mod.

	In previous releases of this mod, the mod's full source was included in the
	zip file. Starting with version 0.15.04, only the files required to play the
	mod are included. You can obtain the source of the mod in the Exult source
	code, available at the 'Downloads' page or through GitHub.
	It is in the 'content/sifixes' directory.

5	What this mod does
----------------------
	As the title implies, this mod fixes SI usecode bugs. The way that they are
	fixed does not depend on starting a new game -- although if the save game is
	at a late enough part of the game, you will not see some of the fixes.

	Here are the specific bugs that this mod addresses so far:

	- Cantra can be cured of madness.
	- If you prefer 24-hour time, you can have it. Just ask Shamino about 'time'
	  to switch formats.
	- After being cured of insanity, the Companions will thank you for it. They
	  will also join *before* Xenka is summoned.
	- Basement of Temple of Tolerance was mistakenly identified as being "Temple
	  of Logic" by the "Locate" spell.
	- Gwenno can now receive the White Diamond Necklace from Iolo;
	- Gwenno will no longer try to talk to the Avatar when she is resurrected;
	- Shamino will add his own misplaced items in the exchanged item list when
	  he joins;
	- On the same note, the moonsilk stockings and the filari will also appear
	  on that list;
	- Fixed a few bugs on the exchanged item list when you find out the origins
	  of the items. Specifically, the lab apparatus, the fur cap, the bear skull
	  and the plain shield had a few problems.
	- The Pillars in dining hall of Serpent's Fang Keep no longer teleport you
	  to the Test of Purity;
	- After the the Wall of Lights cutscene, you can no longer summon Thoxa to
	  "resurrect" your possessed party. Or rather, you can summon her, but she
	  won't bring Iolo, Dupre and Shamino back as they are the Banes now. Even
	  if you have their bodies right there -- or elsewhere.
	- Dupre can no longer be resurrected after he sacrifices himself.
	- Iolo, Shamino and Dupre will all refuse to leave the party while you are
	  on the Spinebreaker Mountains. They will also refuse any blue potions you
	  offer them while there...
	- The Wall of Lights has been revamped also. There is nothing you can do now
	  to keep the companions out of it -- nothing I can think of, at any rate.
	  Considering that I *have* read the Doug the Eagle's anti-walkthrough from
	  http://www.it-he.org, that is a lot. Also, the companions don't dump every
	  item directly to the ground when they die -- they will intelligently drop
	  it, so that items remain in their respective containers. Not really a bug,
	  but it was extremely annoying...
	- Fixes the Monitor Banquet so that the pikeman from the training area will
	  not disappear anymore.
	- Ghosts no longer talk as the Chaos Hierophant when double-clicked.
	- Fawn Tower is properly cleaned up. Goblins will no longer spawn after it
	  has been cleaned up (unless the banes have been released) and the broken
	  dishes all go away.
	- When you get Dupre's shield back from Luther, you now really give him his
	  shield back. The exchanged item list also registers that.
	- Iolo's lute is no longer duplicated by the Teleport Storm on Fawn; a new
	  ordinary lute will appear instead.
	- Inn keys are now reclaimed by the innkeepers when you are leaving the inn.
	  They will lock the doors and make the beds too. Innkeepers will also drop
	  their inn keys when the banes are released (or, in Simon's case, when he
	  is slain).
	- The Vibrate spell no longer makes you drop the Usecode container, nor its
	  contents, which could break the game.
	- The Firesnake spell finally *works* now! Tell me what you think of it.
	  Just make sure not to stand too close to your target...
	- Spell incantations. In the original SI, there were no less than *four* "Ex
	  Por" spells, and slightly more spells were out of synch with the manual.
	  Moreover, the runes in the spellbook and the runes in the magic scrolls
	  were often at odds with one another, with the spoken incantation, with the
	  manual or any combination of these. The details are:
		Spell                Spoken words        Spellbook runes     Magic scroll runes  Manual              This Mod
		======================================================================================================================
		Cure                 An Nox              An Nox              Ylem Nox            An Nox              An Nox
		Columna's Intuition  Wis Jux Ylem        Wis Jux Nox         Wis Jux Ylem        Wis Jux Ylem        Wis Jux Ylem
		Paralyze             An Por              Vas Por             An Por              An Por              An Por
		Create Soul Prism    An Mani Ex%         An Mani Ex%         Ylem Mani Ex        (not shown)         An Mani Ex%
		Unlock Magic         Ex Por              Ex Por              Ex Ort              Ex Por              Ex Por
		Conjure              Kal Xen             Kal Xen             Kal Wis             Kal Xen             Kal Xen
		Explosion            Vas Frio Hur        Vas Frio Por        Ylem Frio Hur       Vas Frio Hur        Vas Frio Hur
		Erstam's Surprise    Ex Por              Uus Vas Grav        Uus Ylem Grav       Uus Vas Grav        Ex Jux Hur*
		Cold Strike          Vas In Frio Grav    Vas In Frio Grav    Ylem In Frio Grav   Vas In Frio Grav    Vas In Frio Grav
		Serpent Bond         An Frio Xen Ex      Kal Frio Xen Ex     Kal Frio Xen Ex     Kal Frio Xen Ex     Kal Frio Xen Ex
		Create Ammo          In Hur Sanct        In Hur Sanct        In Hur Sanct        In Jux Ylem         In Jux Ylem
		Vibrate              Ex Por              An Uus Mani         Rel Uus Mani        An Grav Ex          Uus Des Por Grav*
		Create Ice           In Sanct Grav       In Frio             In Frio             In Frio             In Frio
		Fetch                An Frio Xen Ex      Por Nox             Por Ylem            Por Ylem            Por Ylem
		Firesnake            (nothing)           Kal Sanct           Kal Sanct           Kal Vas Frio Grav   Kal Vas Frio Grav
		Swordstrike          In Jux Por Ylem     In Lor Por Nox      In Jux Por Ylem     In Jux Por Ylem     In Jux Por Ylem
		Spiral Missile       Uus Vas Jux Ylem    Uus Vas Jux Nox     Uus Vas Jux Ylem    Uus Vas Jux Ylem    Uus Vas Jux Ylem
		Stop Storm           Rel Hur             An Hur              An Hur              An Hur              An Hur
		Imbalance            Ex Por              Kal Vas An Grav$    Kal Vas An Grav$    Kal Vas An Grav$    Kal Vas An Grav$

	    *	Thanks to Neutronium Dragon for the suggestions.
	    %	"Ylem Mani Ex" fits better, but I decided to keep "An Mani Ex".
	    $	Perhaps "Kal Vas In Grav" is a better fit to the spell...
	- Ankh-less sails, as there was no reason the residents would use one of the
	  symbols of Beast British's tyrannical commands.
	    
6	Version history
-------------------
version 0.15.17 - 2025-02-26
	- Upped the intelligence to 6 of shapes 299 (Cantra), 652 (Frigidazzi, Lydia),
	  660 (skinless Yelinda), 744 (Goblin woman) so they can act on npc->is_sentient.
version 0.15.16 - 2023-03-15
	- Added Smith's face for the nightmare horse in Dream Realm. Added some headers
	  and a utility folder to make importing from Knight Captain's mod a bit easier.
version 0.15.15 - 2017-09-03
	- Fixed paperdoll art for the moonsilk and fishnet stockings to cover toes.
	- Fixed sprite art for the blue "teleport here" to completed clipped circles.
version 0.15.14 - 2016-12-02
	- Fixed the existing teleporter egg out of the gridiron football field,
	  going to the intended location outside of Pothos' house/island: 2265,1919.
version 0.15.13 - 2016-08-08
	- Draxinar's second riddle now spells correct correctly.
	- Draxinar's last of four riddles now accepts the correct answer of 13.
	  The prior accepted-but-wrong answer now gives a new unique response.
	- Bwundiai and Mwaerno now use a fixed check for Gwani furs in the party,
	  and the party will respond differently if two Gwani are nearby or only one.
	- The automatons in Skullcrusher now confront the Avatar upon entry, when
	  reaching the exit door, and approaching both sets of brass doors inside.
version 0.15.12 - 2016-07-15
	- The equipment scroll has been respaced and reordered to reflect the
	  order the companions rejoin the party. Avatar, Shamino, Dupre, Iolo.
	- Delin can be asked about Batlin after you ask Jendon about daemon artifacts.
	- Edrin's response about his dreams and Siranush will change if you have
	  completed the Dream Realm.
	- Neyobi the Gwani child now has a schedule once cured and woken up by double
	  clicking on her. She will now accompany her mother and Baiyanda around.
	- Kylista can be asked about the breastplate even if you know she owns it.
	- Reading the evidence against Marsten will allow you to accuse him.
	- Reading the same scroll will also set a flag if Pomdirgun is dead, which
	  will change 6 conversations in Monitor.
	- Iolo will return to pacing his cell after you talk with him in Monitor's jail.
version: 0.15.11
	- Fixed spell incantation runes in spellbook and spell scrolls.
version: 0.15.10
	- Fixed some missing barks in Fawn trial.
version: 0.15.09
	- Ship sails no longer have an ankh.
version: 0.15.08
	- Fixed serpent bond bug.
version: 0.15.07
	- Fixed version number.
	- Fixed the dumping of items in Wall of Lights cutscene.
version: 0.15.06
	- Compatibility update.
	- You can now ask Shamino to set the time format of your watch to 24-hour
	  time if you prefer. He can switch it back too, if you change your mind.
version: 0.15.05
	- Added SI Fixes title screen.
version: 0.15.04a
	- Improved installation instructions.
version: 0.15.04
	- SI Fixes is now licensed under the GNU General Public License.
version: 0.15.03
	- Modified SI Fixes to use new intrinsic calling method (using '->').
	- Fixed bug on resurrection of Gwenno and companions (post-banes).
	- Fixed bug where companions would have the wrong dialog after you went
	  through the Silver Seed Maze.
version: 0.15.02
	- Baiyanda now really gives dried fish when you ask her.
	- You can no longer smuggle your party members into the Dream World
	  by use of the Serpent Bond spell.
version: 0.15.01
	- Fixed stupid bug I introduced while preventing Vibrate spell
	  from dropping Usecode container (basically, the spell would only
	  work correctly if used on the Avatar...)
	- Switched version number to #.##.## format for greater flexibility
	  in revisions.
version: 0.15.00
	- Reorganized file structure.
	- Readme file had one omission: Basement of Temple of Tolerance
	  was misidentified as being "Temple of Logic" by "Locate" spell.
	- Cantra's healing now actually works...
	- If you prefer 24-hour time, you can have it. Find the following
	  line in usecode.uc and remove the starting double-slash:
			//#include "items/time_tellers.uc";
	  You will have to recompile the usecode afterwards,
	  but it is not too hard.
	- After being cured of insanity, the Companions will thank
	  the Avatar for it. They will also join *before* Xenka is summoned.
	- Fixed a few bugs on the exchanged item list when you find
	  out the origins of the items. Specifically, the lab apparatus,
	  the fur cap, the bear skull and the plain shield had a few problems.
	- Fawn Tower is properly cleaned up. Goblins no longer spawn
	  after it is cleaned up (unless the banes have been released)
	  and the broken dishes all go away.
	- When you get Dupre's shield back from Luther, you now really
	  give him his shield back. The exchanged item list also
	  registers that.
	- Iolo's lute is no longer duplicated by the Teleport Storm on
	  Fawn -- a regular lute appears instead.
	- Inn keys are now reclaimed by the innkeepers when you are
	  leaving the inn. They will lock the doors and make the beds too.
	  Innkeepers will also drop inn keys when the banes are released
	  (or, in Simon's case, when he is slain).
	- The Vibrate spell no longer makes you drop the Usecode container,
	  nor its contents.
	- Slightly modified the Firesnake spell.
	- Spells. I took Neutronium Dragon's suggestions for
	  Erstam's Surprise and Vibrate.
