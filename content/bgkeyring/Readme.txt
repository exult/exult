========================================================
Keyring Mod Readme
========================================================

0	Table of Contents
---------------------
	0	Table of Contents
	1	About this document
	2	Installing the mod
	3	Contributing to this mod
	4	Using it in your own mods
	5	What this mod does
	6	Spoilers
	7	Version history


1	About this document
-----------------------
Author: Marzo Sette Torres Junior
e-mail: marzojr@yahoo.com
version: 0.11.25

	This mod (the so called "Keyring Mod") contains a lot of small modifications
	to the Ultima VII: Black Gate game WITH the Forge of Virtue add-on. There are
	also a number of not-so-small changes. There is a lot of original usecode
	and art in the mod too.

	There are many acknowledgements I have to make. One is for Team Lazarus; I
	borrowed some ideas for the Shrines and Codex from them.
	
	The other acknowledgements are to those that gave me some useful suggestions
	in the old Phorum thread. They are Crysta the Elf, gruck, Gradilla Dragon,
	Dominus and Artaxerxes.

	I must also thank Crysta the Elf for some graphics she made/edited. The full
	list is in the version history, below.

	Also, I must acknowledge the fact that the structure of the mod is based on
	Alun Bestor's Quests and Iteractions mod, which is included almost entirely.

2	Installing the mod
----------------------
	Note: These instructions are for installing from the zip distribution found
	on my website. If you want to build it yourself, its source code is included
	with Exult's source in the "content/bgkeyring directory". In it, there is a
	README file with instructions for building it.
	
	First, I must be explicit that I have tested this mod *only* with Forge of
	Virtue add-on installed; it will *not* work without that add-on since it
	uses some of the add-on's data.

	Secondly (and just as importantly), this mod is will NOT work in Exult 1.2,
	and neither will work on any but *the* latest snapshots. There were several
	bugs and new features I had to fix to get everything working as is.

	With that out of the way: to install the mod, simply unzip the contents of
	the zip file into your Forge of Virtue's 'mods' folder. By default (that is,
	unless you changed it specifically in your 'Exult.cfg' file), this would be
	the 'mods' subdirectory of the path you set for Forge of Virtue in your
	'Exult.cfg' file. If the folder is not there, simply create an empty folder
	called 'mods' in your Forge of Virtue directory and proceed as above.
	
	There are two optional (but highly recommended) additional steps you might
	want to do:

	OPTIONAL: You can create a shortcut to start Exult directly in the Keyring
	mod. To do so, make sure that the command-line is similar to the following
	example:
		<path to Exult>\Exult --fov --mod Keyring
	You can use the supplied 'Keyring.ico' file for the shortcut (if you use
	Windows) or use the supplied 'Keyring.png' file to make your own icon for
	other OSes.

	OPTIONAL: This mod adds a custom keybinding *patch* file, which alters what
	two key combinations do:
		-- The 'K' key calls the Keyring, like SI;
		-- Again like SI, the 'Alt+K' combination tries all party keys.
	These changes are loaded by default. You can, if you like, edit these new
	key bindings to match your preferences; just edit the 'data/patchkeys.txt'
	file. This file will override your BG settings -- which is why it is such
	a small file -- but only for the Keyring mod.


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
	zip file. Starting with version 0.11.01, only the files required to play the
	mod are included. You can obtain the source of the mod in the Exult source
	code, available at the 'Downloads' page or through GitHub.
	It is in the 'content/bgkeyring' directory.

5	What this mod does
----------------------
	The mod started out by adding a keyring to Black Gate. It has two
	new NPCs, several new items and graphics and the following things:
	- You can now meditate at the Shrines of the Virtues.
	- You can now view the Codex; you must be in a sacred quest, though.
	- A brand new Shrine of the Codex, based on the image from the Mysterious
	  Sosaria website.
	- The three items of Principles (Book of Truth, Candle of Love and Bell of
	  Courage), as well as the three Flames of Principles.
	- Lock Lake Cleanup: Once Miranda's bill is signed, the Lake will be slowly
	  and gradually cleaned up of garbage.
	- A brand new Shrine of Spirituality and a new basement for Serpent's Hold,
	  where the Flame of Courage is located. Both are located in a new map, thus
	  using Exult's Multimap support.
	- An improved Orb of the Moons, allowing you to visit the shrines too.
	- Innkeepers will reclaim the room keys when you are leaving the inn. They
	  will also lock the doors and make the beds.
	- The rudiments of a brand-new spellcasting system which allows NPCs to cast
	  spells are now in place. Each NPC has his/her own spellcasting item which
	  can be used to cast spells, and they will use mana and reagents. There are
	  also some new NPC-only spells.
	- The 'Iteractions' mod by Alun Bestor; you can bake pastries, forge swords,
	  milk cows, shear sheep...
	- The 'Avatar Pack' mod by yours truly, with several shiny new portraits for
	  your avatar.
	- NPCs can now cast spells too! The spellcasting system is very rudimentary,
	  being dialog based, but it will get better in the future.
	- Lord British now has his Crown Jewels, based on their Ultima 5 versions.
	  They don't do everything they should yet, but they are unbalanced already
	  if you get your filthy hands on them.
	- A rollable flying carpet you can carry around, which replaces the original
	  flying carpet.

6	Spoilers
------------
	So, you just can't keep away... fine: here they are.
	Proceed at your own peril...

	Keyring Spoilers
	----------------
		- The Quest is started by Zauriel. He is located just outside of  Lord
		  British's castle. You can't miss him :-)
		- Most dialogs have a 'shortcut' out with no ill effects.
		- You must be very powerful to survive the quest.
		- Try hackmoving Laurianna to Zauriel without giving her some blackrock
		  first. Then talk to Zauriel.
		- It is Laurianna that will give you the keyring. She will only do so
		  *after* you give her Zauriel's Journal.
		- Use the keyring on the Avatar to add all party keys to it.

	Shrines of Virtues
	------------------
		- The Shrine of Sacrifice starts out defiled; you can restore it  with
		  the correct Word of Power.
		- Before cleasing the Shrine of Sacrifice, find the "Book of Forgotten
		  Mantras" and take it with you.
		- After you meditate for the right amount of cycles and with the right
		  mantra, each shrine will give you a quest to see the Codex of Ultimate
		  Wisdom. This allows you to go pst the guardians.
		- After going through seven shrines, and if you don't have access to the
		  Shrine of Spirituality, you will be able to attune the white Virtue
		  Stone to the Shrine with the Codex. Be sure to have a way out, or you
		  may be trapped!
		- Returning to the Shrine after seeing the Codex gives you a reward.

	Shrine of the Codex
	-------------------
		- You can only enter if you are in a sacred quest.
		- You must have both lenses to view the Codex. You can double-click the
		  lenses to rotate them.
		- Make sure to carry the lenses with you on your way out -- otherwise,
		  they may be trapped inside the shrine if you can no longer enter!
		- Returning the Items of Principles to their rightful places is always
		  the right thing to do.
		- Try teleporting inside the Shrine and viewing the Codex when you are
		  not on a quest to see it.

	Improved Orb of the Moons
	-------------------------
		- You can now teleport to the Shrines too; just throw the orb far from
		  you. Here are the new destinations of the Orb:
		Honesty                         Compassion                  Valor

		                Moonglow        Britain         Jhelom

		Humility        New Magincia    (Avatar)        Yew         Justice

		                Skara Brae      Trinsic         Minoc

		Spirituality                    Honor                       Sacrifice

	Lock Lake Cleanup
	-----------------
		- Eventually, Mack's key will be in possession of Lord Heather. You can
		  ask him for it.

7	Version history
-------------------
version: 0.11.26
	- Fixed Orb of the Moons only teleporting the avatar.
	- Laurianna and Jaana no longer use the spellsystem AI when asked to heal;
	  this will remain until the AI is more mature.
version: 0.11.25
	- Added the rollable flying carpet.
version: 0.11.24
	- Fixed dialog bug where Shamino would interject to insult Dupre even if
	  the later is not in the party.
version: 0.11.23
	- Fixed Skara Brae/New Magincia moongate destinations.
version: 0.11.22
	- LB's crown will grant infravision to the wearer.
version: 0.11.21
	- Meditation failed in shrine of spirituality. Fixed
version: 0.11.20
	- Fixed comment when adding all party keys to keyring.
version: 0.11.19
	- Compatibility update with Exult 1.4.05cvs: frame names for several shapes,
	  differential *.dat files, added back data lost due to bad behavior of older
	  versions of ES. Julia's hammer is now 'lucky'.
version: 0.11.18
	- New diagonal moongates that do not stick out like sore thumbs.
version: 0.11.17
	- Compatibility update with new Exult snapshot.
version: 0.11.16
	- I know I said this before, but this time I actually mean it: making
	  healing services actually work for LB. *sigh*
	- Making Death Bolt spell work again.
version: 0.11.15
	- Making healing services actually work for LB, Laurianna & Jaana.
version: 0.11.14
	- Fixed misidentification bug in Mage & Goons cutscene where Sentri can
	  be wrongly used by the usecode as if it where Laudo's fighter goon. The
	  fix also prevents this from potentially happening with possible future
	  NPCs with the same shape as the other monsters in the cutscene.
	- Fixed "Unlock Magic" bug where it would not work.
	- Removed unneccessary function "avatarSpeak" (Exult does all the work it
	  supposedly did).
	- Fixed error in reagent lists of "Explosion" and "Great Heal" spells which
	  rendered them uncastable.
	- If an NPC is in 'archwizard' (i.e. cheat) mode, you can ask it for reagents
	  to get a bag full of them.
version: 0.11.13
	- Fixed bug which would cause a purple outline to be displayed around NPCs
	  during some animations in recent snapshots of Exult.
version: 0.11.12
	- Fixed a few bugs in Zauriel's gem of dispelling functions. Improved the
	  functions for making the blackrock potion.
version: 0.11.11
	- New sprite for male bucket-heade avatar, made by Skutarth. Also, removed
	  the silly fully-armored paperdoll for the bucket-headed avatars.
version: 0.11.10
	- New title screen for Keyring mod.
	- Modified avatar bodies to be shape 1098.
	- Merged updates from the Avatar Pack.
version: 0.11.09a
	- Improved installation instructions.
version: 0.11.09
	- Cool new avatar portraits and sprites. The avatar portraits are SI-style;
	  I *do* have BG-style versions of the portraits but they don't look nowhere
	  near as good.
	- New "angry" portrait for Laurianna glows.
version: 0.11.08
	- Laurianna no longer sells reagents while she is in the party. She also
	  won't sell reagents if you kick her out and tell her to wait there.
	- Mariah now sells spells while on the road.
	- Added independent usecode versioning for unknown and favorite spell lists
	  in the spell system to fix older save games.
version: 0.11.07
	- Fixed reagent bug for linear spells.
	- Removed Fire Snake spell from the list as it is not castable.
	- Improved Mass Resurrection spell to work only if there are resurrectable
	  bodies within range of the spell.
version: 0.11.06
	- Updated Joneleth and Laundo and his goons to use the new set_usecode_fun
	  intrinsic.
	- Fixed resurrection of Laurianna and Mariah.
	- Fixed WIHH data for NPC spellbooks.
version: 0.11.05
	- Compatibility update with new BG paperdoll data.
	- Rewrote the Keyring code to use a static class.
version: 0.11.04
	- Fixed several bugs in usecode, and added an updated tfa.dat file
	  to make fresh fish work.
	- Some new art by Crysta.
	- Reformatted the Orb of the Moons' destination table to use spaces
	  for aligmnent
version: 0.11.03
	- Fixed bugs in forging code/Menion's training.
version: 0.11.02
	- Modifications added due to new UCC capabilities.
version: 0.11.01
	- New sprite and Gump for Mariah
	- Lord British's sprite has been retouched by Crysta the Elf.
	- New data files for the bodies of Laurianna and Mariah.
	- Bug in Final Fight made former party members useless.
	- Fixed bug when Innkeepers came to reclaim the key.
version: 0.11.00
	- Merged in the 'Iteractions' part of Alun Bestor's 'Quests and
	  Iteractions' mod. It is released under the GNU General Public
	  License (with Alun Bestor's permission, of course).
	- Added in the Crown Jewels of Britannia, with graphics by Crysta
	  the Elf.
	- Added new graphics (for spellcasting) to Iolo, Shamino, Dupre and
	  Julia, by me, and to Lord British (from SI).
	- Added new 'services' code; first service is healing, and all NPC
	  healers now use it.
	- Added Julia's hammer to the host of spellcasting items.
version: 0.10.04
	- New NPC spellbook graphics by Crysta the Elf.
	- Added Dupre's Amulet and Shamino's Ankh (their spellcasting items)
	  by Crysta the Elf.
	- Started adding paperdoll files.
	- Added Mariah's BG Gump, by Sissy Knox.
	- The Keyring is now licensed under the GNU General Public License.
version: 0.10.03
	- Added basic NPC spellcasting files.
	- Reorganized mod based on new UCC capabilities.
version: 0.10.02
	- Some faces were retouched by Crysta the Elf: Zauriel, Laundo and
	  Joneleth.
	- The Codex book displayed in-game got its own graphic, instead of
	  reusing Zauriel's Journal, made by Crysta the Elf.
	- New graphic for Book of Truth by Crysta the Elf (slightly edited
	  by me).
	- 'Wild' wisps weren't displaying their portrait.
	- Shamino was identifying the 'Shrine of the Codex' as being the
	  'Shrine of Humility'. Some ranger :-)
	- Added some feedback if the player is leaving the Shrine of the
	  Codex *after* finishing all quests but is leaving the lenses,
	  the Items of Principle and/or the Vortex Cube.
version: 0.10.01
	- Fixed initgame.dat and install.bat.
version: 0.10.00
	- Document created.

