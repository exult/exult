Aseprite shapes plug-In
=======================

Aseprite is a popular pixel art editor https://aseprite.org.

This plug-in converts a shp to png and displays all the frames on hitting File -> "Import SHP".
It will also temporarily store the offsets of the shp file as cel "User Data", until you close
the imported file. However you can also save as the asprite file format which will store the
"User Data".

"File-> Export to U7 SHP" will make use of the "User Data" and save the offsets correctly in the exported SHP.

"Sprite-> Convert to U7 Palette" will convert any graphics to the default Ultima VII palette.
This will do the following steps:
1. Sets color mode to RGB.
2. Loads the default Ultima VII palette*.
3. Resets color to indexed (which will align colors to the best matching one)
4. Since RGB->indexed sets transparency to index #0, the alignment used #87 for black (which isn't completely black)
   and what should be transparent is index #0 (black). So we exchange everything colored #0 with #255, 
   everything colored #87 with #0 and finally make sure #255 is set to transparency.
* The palette has index #224 - #255 (color cycling and opaque colors) set to fully transparant to prevent matching 
  these colors in step 3.

Issues of the plug-in
=====================
- Currently you can only import a shp file, not open it directly.
- Currently you can only export a shp file, not save/save as directly.


How to Build and Install
========================

On Linux and macOS use "./configure --enable-aseprite-plugin". The only thing this checks for is the presence
of libpng.
On Windows run "make -f makefile.mingw aseprite-plugin"

Then zip the plug-in (exult_shape(.exe), main.lua, package.json, u7.pal and this Readme.txt) as "exult_shp.aseprite-extension".
Double clicking it will open Aseprite and install the plug-in.

To be safe, restart Aseprite, and you should see the "Import tu U7 SHP" and "Export to U7 SHP" commands in the File menu
and "Convert to U7 Palette" in the Sprite menu.