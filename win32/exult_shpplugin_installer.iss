;.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,
;                                                                       ;
;Copyright (c) 2002-2010 Jernej SimonŸiŸ                                ;
;                                                                       ;
;This software is provided 'as-is', without any express or implied      ;
;warranty. In no event will the authors be held liable for any damages  ;
;arising from the use of this software.                                 ;
;                                                                       ;
;Permission is granted to anyone to use this software for any purpose,  ;
;including commercial applications, and to alter it and redistribute it ;
;freely, subject to the following restrictions:                         ;
;                                                                       ;
;   1. The origin of this software must not be misrepresented; you must ;
;      not claim that you wrote the original software. If you use this  ;
;      software in a product, an acknowledgment in the product          ;
;      documentation would be appreciated but is not required.          ;
;                                                                       ;
;   2. Altered source versions must be plainly marked as such, and must ;
;      not be misrepresented as being the original software.            ;
;                                                                       ;
;   3. This notice may not be removed or altered from any source        ;
;      distribution.                                                    ;
;.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.,.;

[Setup]
AppName=Ultima VII Shape plug-in for GIMP
AppVerName=Shape plug-in Git
AppPublisherURL=https://exult.info/
AppSupportURL=https://exult.info/
AppUpdatesURL=https://exult.info/
; Setup exe version number:
VersionInfoVersion=1.13.1
DisableDirPage=yes
DefaultDirName="{userappdata}\gimp\3.0\plug-ins\u7shp"
DisableProgramGroupPage=yes
DefaultGroupName=Exult GIMP Plugin
OutputBaseFilename=Gimp30Plugin
Compression=lzma
SolidCompression=yes
InternalCompressLevel=max
AppendDefaultDirName=false
AllowNoIcons=true
OutputDir=.
DirExistsWarning=no
WizardStyle=modern
CreateUninstallRegKey=no
UpdateUninstallLogAppName=no
AppID=GIMP-U7SHP-Plugin
PrivilegesRequired=none
#include "archs_include.iss"
ArchitecturesInstallIn64BitMode={#archs64}
ArchitecturesAllowed={#archs}

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; The official Gimp 3 64 bits is built with CLang64 and so is Exult
; => The dependent DLLs of the Gimp plugin are provided by Gimp

#if WANT_x64
Source: "GimpPlugin-x86_64\u7shp.exe"; DestDir: "{userappdata}\gimp\3.0\plug-ins\u7shp"; Flags: ignoreversion; Check: IsGIMPx64
#endif
#if WANT_ARM64
Source: "GimpPlugin-aarch64\u7shp.exe"; DestDir: "{userappdata}\gimp\3.0\plug-ins\u7shp"; Flags: ignoreversion; Check: IsGIMPARM64
#endif

[Code]
const
	RegPath = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\WinGimp-3.0_is1';
	RegPathNew = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GIMP-3_is1';
	PathValue = 'Inno Setup: App Path';
	VerValue = 'DisplayName';
	CompValue = 'Inno Setup: Selected Components';

var
	GimpPath: String;
	GimpVer: String;
	GimpArch: (GimpUnknown,gimpx64,gimpARM64);

procedure GetGimpInfo(const pRootKey: Integer; const pRegPath: String);
var p: Integer;
var comps: string;
begin
	If not RegQueryStringValue(pRootKey, pRegPath, PathValue, GimpPath) then //find Gimp install dir
	begin
		GimpPath := ExpandConstant('{pf}\GIMP 3');
	end;
	If not RegQueryStringValue(pRootKey, pRegPath, CompValue , comps) then //Get the installed arch
	begin
	comps := '';
	end;

	if Pos('gimpx64',comps) <> 0 then begin
		GimpArch := gimpx64;
	end else
	if Pos('gimpARM64',comps) <> 0 then begin
		GimpArch := gimpARM64;
    end;



	If not RegQueryStringValue(pRootKey, pRegPath, VerValue, GimpVer) then //find Gimp version
	begin
		GimpVer := '0';
	end;

	if GimpVer <> '' then //GimpVer at this point contains 'GIMP x.y.z' - strip the first part
	begin
		p := Pos('gimp ',LowerCase(GimpVer));
		if p > 0 then
			GimpVer := Copy(GimpVer,p+5,Length(GimpVer))
		else
			GimpVer := '0';
	end;
end;


function InitializeSetup(): Boolean;
begin
	Result := True;

	GimpArch:=GimpUnknown
	if RegValueExists(HKLM64, RegPathNew, PathValue) then 
	begin
		GetGimpInfo(HKLM64, RegPathNew);
	end else
	if RegValueExists(HKCU, RegPathNew, PathValue) then 
	begin
		GetGimpInfo(HKCU, RegPathNew);
	end else

	if	GimpArch=GimpUnknown
	then
	begin
	MsgBox('Error unable to dectect Architecture of Gimp. Aborting',mbError, MB_OK);
	result:= False;
	end;

end;

function IsGIMPx64(): boolean;
begin
	Result := GimpArch = gimpx64;
end;

function IsGIMPARM64(): boolean;
begin
	Result := GimpArch = gimpARM64;
end;
