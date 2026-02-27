[Setup]
AppName=Exult Studio
AppVerName=Exult Studio Git
AppPublisher=The Exult Team
AppPublisherURL=https://exult.info/
AppSupportURL=https://exult.info/
AppUpdatesURL=https://exult.info/
; Setup exe version number:
VersionInfoVersion=1.13.1
DisableDirPage=no
DefaultDirName={autopf}\Exult
DisableProgramGroupPage=no
DefaultGroupName=Exult Studio
OutputBaseFilename=ExultStudio
Compression=lzma
SolidCompression=yes
InternalCompressLevel=max
AppendDefaultDirName=false
AllowNoIcons=true
OutputDir=.
DisableWelcomePage=no
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; 32-bit files
Source: Studio-i686\exult_studio.exe; DestDir: {app}; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Studio-i686\*.dll; DestDir: {app}; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Studio-i686\lib\*; DestDir: {app}\lib\; Flags: ignoreversion recursesubdirs; Check: not Is64BitInstallMode
; 64-bit files
Source: Studio-x86_64\exult_studio.exe; DestDir: {app}; Flags: ignoreversion; Check: Is64BitInstallMode
Source: Studio-x86_64\*.dll; DestDir: {app}; Flags: ignoreversion; Check: Is64BitInstallMode
Source: Studio-x86_64\lib\*; DestDir: {app}\lib\; Flags: ignoreversion recursesubdirs; Check: Is64BitInstallMode
; Architecture-neutral files
Source: Studio-i686\share\*; DestDir: {app}\share\; Flags: ignoreversion recursesubdirs
Source: Studio-i686\COPYING.txt; DestDir: {app}; Flags: onlyifdoesntexist
Source: Studio-i686\Exult Source Code.url; DestDir: {app}; Flags: ignoreversion;
Source: Studio-i686\AUTHORS.txt; DestDir: {app}; Flags: onlyifdoesntexist
Source: Studio-i686\images\*.gif; DestDir: {app}\images\; Flags: ignoreversion
Source: Studio-i686\images\*.svg; DestDir: {app}\images\; Flags: ignoreversion
Source: Studio-i686\images\studio*.png; DestDir: {app}\images\; Flags: ignoreversion
Source: Studio-i686\exult_studio.html; DestDir: {app}; Flags: ignoreversion
Source: Studio-i686\exult_studio.txt; DestDir: {app}; Flags: ignoreversion
Source: Studio-i686\data\estudio\new\*.flx; DestDir: {app}\data\estudio\new; Flags: ignoreversion
Source: Studio-i686\data\estudio\new\*.vga; DestDir: {app}\data\estudio\new; Flags: ignoreversion
Source: Studio-i686\data\estudio\new\*.shp; DestDir: {app}\data\estudio\new; Flags: ignoreversion
Source: Studio-i686\data\estudio\new\blends.dat; DestDir: {app}\data\estudio\new; Flags: ignoreversion
Source: Studio-i686\data\exult_studio.glade; DestDir: {app}\data\; Flags: ignoreversion

[Icons]
Name: {group}\Exult Studio; Filename: {app}\exult_studio.exe; WorkingDir: {app}
Name: {group}\{cm:UninstallProgram,Exult Studio}; Filename: {uninstallexe}
Name: {group}\exult_studio.html; Filename: {app}\exult_studio.html; WorkingDir: {app}; Comment: exult_studio.html; Flags: createonlyiffileexists

[Run]
Filename: {app}\exult_studio.exe; Description: {cm:LaunchProgram,Exult Studio}; Flags: nowait postinstall skipifsilent

[Dirs]
Name: {app}\images
Name: {app}\data
; Name: {app}\lib
; Name: {app}\etc
