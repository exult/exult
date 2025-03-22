Aseprite shapes plug-In
=======================

Aseprite is a popular pixel art editor https://aseprite.org.

This plug-in converts a shp to png and displays all the frames on hitting File -> "Import SHP".
It will also temporarily store the offsets of the shp file as cel "User Data", until you close
the imported file. However you can also save as the asprite file format which will store the
"User Data".

"Export SHP" will make use of the "User Data" and save the offsets correctly in the exported SHP.


Issues of the plug-in
=====================
- Currently you can only import a shp file, not open it directly.
- Currently you can only export a shp file, not save/save as directly.


How to Build and Install
========================

On Linux and macOS use "./configure --enable-aseprite-plugin". The only thing this checks for is the presence
of libpng.
On Windows run "make -f makefile.mingw aseprite-plugin"

Then zip the plug-in (exult_shape(.exe), main.lua, package.json and this Readme.txt) as "exult_shp.aseprite-extension".
Double clicking it will open Aseprite and install the plug-in.

To be safe, restart Aseprite, and you should see the "Import SHP" and "Export SHP" commands in the File menu.