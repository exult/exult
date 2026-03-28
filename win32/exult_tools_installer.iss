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
#include "archs_include.iss"
ArchitecturesInstallIn64BitMode={#archs64}
ArchitecturesAllowed={#archs}


[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
#if WANT_i686
Source: Tools\i686\ucxt.exe; DestDir: {app}; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\cmanip.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\expack.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\ipack.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\mklink.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\mockup.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\rip.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\shp2pcx.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\smooth.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\splitshp.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\textpack.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\u7voice2syx.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\ucc.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\wuc.exe; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: Tools\i686\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: not Is64BitInstallMode
#endif
#if WANT_x64
Source: Tools\x86_64\ucxt.exe; DestDir: {app}; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\cmanip.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\expack.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\ipack.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\mklink.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\mockup.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\rip.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\shp2pcx.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\smooth.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\splitshp.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\textpack.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\u7voice2syx.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\ucc.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\wuc.exe; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
Source: Tools\x86_64\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: Is64BitInstallMode and {#x64Check}
#endif
#if WANT_ARM64
Source: Tools\aarch64\ucxt.exe; DestDir: {app}; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\cmanip.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\expack.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\ipack.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\mklink.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\mockup.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\rip.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\shp2pcx.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\smooth.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\splitshp.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\textpack.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\u7voice2syx.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\ucc.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\wuc.exe; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
Source: Tools\aarch64\*.dll; DestDir: {app}\; Flags: ignoreversion; Check: IsArm64
#endif
; Architecture-neutral files
Source: Tools\AUTHORS.txt; DestDir: {app}\; Flags: onlyifdoesntexist
Source: Tools\COPYING.txt; DestDir: {app}\; Flags: onlyifdoesntexist
Source: Tools\Exult Source Code.url; DestDir: {app}\; Flags: ignoreversion
Source: Tools\README-SDL.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\data\bginclude.uc; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\events.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\flags.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\u7bgintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\u7misc.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\u7opcodes.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\u7siintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\data\u7sibetaintrinsics.data; DestDir: {app}\data\; Flags: ignoreversion
Source: Tools\expack.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\intrins1.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\intrins2.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\ipack.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\shp2pcx.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\splitshp.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\textpack.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\u7bgflag.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\u7siflag.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\ucformat.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\ucc.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\mockup.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\mappings.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\mappings_alternative.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\map.png; DestDir: {app}\; Flags: ignoreversion
Source: Tools\smooth.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\smooth.conf; DestDir: {app}\; Flags: ignoreversion
Source: Tools\rough.bmp; DestDir: {app}\; Flags: ignoreversion
Source: Tools\smoothed.bmp; DestDir: {app}\; Flags: ignoreversion
Source: Tools\ucxt.txt; DestDir: {app}\; Flags: ignoreversion
Source: Tools\ucxtread.txt; DestDir: {app}\; Flags: ignoreversion

[Dirs]
Name: {app}\data
Name: {app}\tools
