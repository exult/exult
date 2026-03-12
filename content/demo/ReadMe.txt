Exult Demo Game
---------------

This is a continuation of Malignant Manor's "Dev Game", which gives a good
starting point for people wanting to create their own game with the Exult
engine.

So far everything is quite crude but you can see how tiny changes could make
it look better, step by step.


INSTALL
-------

For systems that autodetect the games, drop the demo folder in the same folder
that contains the Utlima VII games (see https://exult.info/docs.html#installation).

On Windows you need to add the following inside the <game> </game> tags.
   <demo>
    <path>
    c:\PATH_TO_THE_DEMO_FOLDER
    </path>
   </demo>


Version history
---------------
version: 0.13
2026-03-08	Dominik Reichardt

	Brought everything into order to drop in Exult's source
	Merged Static and Patch files into one
	Renamed to Demo game (changed title screen of Mainshp.flx)
	Added an intro thus renamed the menu entry from "NO INTRO" to "INTRO"
	Front facing usecode text (books, scrolls, signs,...) have a shorter text,
		no longer describing the horrible fonts
	Not all gumps still state their function in their background
	Body gump is now a splat


version: 0.12
2011-02-09	Malignant Manor

	Added SI style paper dolls
	Fixed missing frame in main character
	Made the pickaxe visible when equipped
	Added needed status effect highlight and menu colors
	Cstat gumps work in all games with cstat.patch
	Added main character shapes for different skin/gender
	Made mainshp.flx use palette file and fix shape color indexes
	Door path finding for the 4 direction door should work properly with
		more_door_directions.patch
	Cloak clasps can have different frames depending on the arm frame
		(non-BG) with cloak_clasp.patch (just aesthetic)
	Added extracted shapes from all flexes (except fonts.vga) and made a
		flex index file for them
	Added hardcoded data to the right shape number. (as much as I could
		find offhand)
	Setup the game to have dummies or manually edited files in the static
		directory for easier game building.


version: 0.11
2009-07-12	Malignant Manor

Partial list of things I've added:
	Mining via the pickaxe. Double click the pickaxe and target or attack
		them with the pickaxe. (frame 3 ore is worthless)
	Doors that open in 4 directions.
	Doors and chests have can be locked and you can magic lock them.
	An altered version of Marzo's keyring.
	Spellbook and spells that can magic unlock and magic lock. (spells 00
		and 01 respectively)
	Containers shapes so that the gumps can be opened.
	Text gump items so you can test it out.
	FreeSans.ttf used to make font shapes. (They are not offset or
		modified for problems. It's mainly there right now to be able to
		see text and not crash.)
	I've modified Exult for some things to work properly.


version: 0.1
2009-07-06	Malignant Manor

I've got templates done for all the gumps.vga shapes except the serpent
	jawbone and teeth. I probably won't add them due to their nature. All
	frames for pointer.shp are done. I've done a bit more documenting, but
	it still needs a whole lot of work.
I haven't done any SI style paper doll's yet. I don't believe it is
	currently possible to use them in dev games at the moment though.
	(This is likely fixed with Marzo's upcoming patch).
There's currently only one font. Some fonts need to be created and also
	with an approximate size for ones that really matter like the status
	and paper doll font.
The avatar has all frames done. Some example usecode and items should
	also be done.