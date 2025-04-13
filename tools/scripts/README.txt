Update copyright year
---------------------

The shell script update_copyright_year.sh updates the following files 
with the current year:

  - version dialog in ActionAbout (keyactions.cc)
  - Info.plist.in
  - win32/exultico.rc, win32/exultstudioico.rc
  - macosx/exult_studio_info.plist.in

From the top of our source run
./tools/scripts/update_copyright_year.sh 


Update version number
---------------------

The shell script update_version_number.sh updates the following files
with the supplied version number and properly identifies as git/snapshot:

Version number:
  - configure.ac
  - Makefile.common
  - win32/exconfig.rc, win32/exultico.rc, win32/exultstudioico.rc
        (FILEVERSION, PRODUCTVERSION, "FileVersion" and "ProductVersion")
  - win32/exult.exe.manifest, win32/exult_studio.exe.manifest
        (version)
  - win32/exult_installer.iss, win32/exult_studio_installer.iss, win32/exult_tools_installer.iss, win32/exult_shpplugin_installer.iss
        (AppVerName, VersionInfoVersion)
  - msvcstuff/vs2019/msvc_include.h
        (VERSION)
  - ios/include/config.h
        (PACKAGE_STRING, PACKAGE_VERSION, VERSION)
  - ios/info.plist
        (CFBundleShortVersionString)
  - tools/aseprite_plugin/package.json
        (version)

Git/Snapshot:
  - win32/exult_installer.iss, win32/exult_studio_installer.iss, win32/exult_tools_installer.iss
        (AppVerName)
  - macosx/macosx.am (--volname "...", dmg filename)
  
From the top of our source run
./tools/scripts/update_version_number.sh version_number
(e.g. ./tools/scripts/update_version_number.sh 1.11.0git)