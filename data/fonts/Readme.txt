Except for the menu font (from MAINSHP.FLX, file #9) these are all the fonts we use and the shapes they replace in the original files (lead by shape or file number of the original flex file):

FONTS.VGA
0_original.shp
0_serif.shp
1_wood_runic.shp
2_small_black.shp
3_metal_runic.shp
4_tiny_black.shp
5_tiny_white.shp
6_gold_runic.shp
7_original.shp
7_serif.shp
8_tiny_ophidian.shp (SI FONTS.VGA only)
9_metal_ophidian.shp (SI FONTS.VGA only)
10_gold_ophidian.shp (SI FONTS.VGA only)


MAINSHP.FLX
3_mainshp_original.shp (BG MAINSHP.FLX only)
3_mainshp_serif.shp (BG MAINSHP.FLX only)
9_mainshp_bg.shp (BG menu font)
9_mainshp_si.shp (SI menu font)

ENDGAME.DAT (BG only)
4_endgame_original.shp
4_endgame_serif.shp
5_endgame_original.shp
5_endgame_serif.shp
6_endgame_original.shp
6_endgame_serif.shp


INTRO.DAT (SI only)
14_intro_original.shp
14_intro_serif.shp

Exult menu fonts:
font.shp
fonton.shp
navfont.shp
navfonton.shp


Notes:

INTRO.DAT:
Font shape #14 is used in both the intro and the endgame and uses only three colors (index #252, #253, #254). The font uses the palette of the flic files it is overlayed on and these three colors are deliberately set for the font.
When you edit the shape in The Gimp, make sure the darker grey is set to color #253 before saving it as The Gimp likes to auto assign a different color (on all frames of the shape).

6_endgame_*.shp:
This font is used in both BG and SI for the congratulations screen and therefore has the @ readded to its UTF-8 position. When the built-in fonts are disabled, Exult will use the yellow conversation font (0_original.shp) but will use the @ from the built-in font.


Adding new characters:
For those characters for which the position in our fonts files do not match their position in the UTF-8 encoding table we are using a map in files/msgfile.cc, remapping their positions to their positions in our fonts.
When you add new characters, orient yourself at 2_small_black.shp as that is the most complete font (others do not need all the special characters it provides).
Make sure that you do not add a character to frame #32 as that is reserved for space.
Adding new characters after the last current frame is save to do, remember to add the new position to the map in files/msgfile.cc.

Adding new fonts shapes:
When you add new fonts don't forget to edit data/fonts/original.in, data/fonts/serif.in, data/Makefile.am, Makefile.common, msvcstuff/vs2019/data/data.vcxproj.
