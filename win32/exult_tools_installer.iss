[Setup]
AppName=Exult Tools
AppVerName=Exult Tools Git
AppPublisher=The Exult Team
AppPublisherURL=https://exult.info/
AppSupportURL=https://exult.info/
AppUpdatesURL=https://exult.info/
; Setup exe version number:
VersionInfoVersion=1.13.1
DisableDirPage=no
DefaultDirName={autopf}\Exult\Tools
DisableProgramGroupPage=yes
DefaultGroupName=Exult Tools
OutputBaseFilename=ExultTools
Compression=lzma
SolidCompression=yes
InternalCompressLevel=max
AppendDefaultDirName=false
AllowNoIcons=true
OutputDir=.
DirExistsWarning=no
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; x86_64 files
Source: Tools-x86_64\ucxt.exe; DestDir: {app}; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\*.dll; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\cmanip.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\expack.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\ipack.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\mklink.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\mockup.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\rip.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\shp2pcx.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\smooth.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\splitshp.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\textpack.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\u7voice2syx.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\ucc.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\wuc.exe; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
Source: Tools-x86_64\tools\*.dll; DestDir: {app}\tools\; Flags: ignoreversion; Check: IsX64Compatible
; Architecture-neutral files
Source: Tools-x86_64\AUTHORS.txt; DestDir: {app}\tools\; Flags: onlyifdoesntexist
Source: Tools-x86_64\COPYING.txt; DestDir: {app}\tools\; Flags: onlyifdoesntexist
Source: Tools-x86_64\Exult Source Code.url; DestDir: {app}\tools\; Flags: ignoreversion;
Source: Tools-x86_64\tools\README-SDL.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\data\bginclude.uc; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\events.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\flags.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\u7bgintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\u7misc.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\u7opcodes.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\u7siintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\data\u7sibetaintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools-x86_64\tools\expack.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\intrins1.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\intrins2.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\ipack.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\shp2pcx.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\splitshp.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\textpack.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\u7bgflag.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\u7siflag.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\ucformat.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\ucc.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\mockup.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\mappings.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\mappings_alternative.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\map.png; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\smooth.txt; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\smooth.conf; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\rough.bmp; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\tools\smoothed.bmp; DestDir: {app}\tools\; Flags: ignoreversion
Source: Tools-x86_64\ucxt.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools-x86_64\ucxtread.txt; DestDir: {app}\; Flags: ignoreversion

[Dirs]
Name: {app}\data
Name: {app}\tools
