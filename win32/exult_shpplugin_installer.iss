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

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; The official Gimp 3 32 bits is built with MingW32 and so is Exult, the official Gimp 3 64 bits is built with CLang64 and so is Exult
; => The dependent DLLs of the Gimp plugin are provided by Gimp
; 32-bit files
Source: "GimpPlugin-i686\u7shp.exe"; DestDir: "{userappdata}\gimp\3.0\plug-ins\u7shp"; Flags: ignoreversion; Check: not Is64BitGIMP
; 64-bit files
Source: "GimpPlugin-x86_64\u7shp.exe"; DestDir: "{userappdata}\gimp\3.0\plug-ins\u7shp"; Flags: ignoreversion; Check: Is64BitGIMP

[Code]
const
	RegPath = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\WinGimp-3.0_is1';
	RegPathNew = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GIMP-3_is1';
	RegKey = 'Inno Setup: App Path';
	VerKey = 'DisplayName';

var
	GimpPath: String;
	GimpVer: String;

	InstallType: (itOld, itNew, itNew64);

procedure GetGimpInfo(const pRootKey: Integer; const pRegPath: String);
var p: Integer;
begin
	If not RegQueryStringValue(pRootKey, pRegPath, RegKey, GimpPath) then //find Gimp install dir
	begin
		GimpPath := ExpandConstant('{pf}\GIMP 3');
	end;

	If not RegQueryStringValue(pRootKey, pRegPath, VerKey, GimpVer) then //find Gimp version
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

	if IsWin64 and RegValueExists(HKLM64, RegPathNew, RegKey) then //check for 64bit GIMP with new installer first
	begin
		GetGimpInfo(HKLM64, RegPathNew);
		InstallType := itNew64;
	end else
	if RegValueExists(HKLM, RegPathNew, RegKey) then //then for 32bit GIMP with new installer
	begin
		GetGimpInfo(HKLM, RegPathNew);
		InstallType := itNew;
	end else
	if RegValueExists(HKLM, RegPath, RegKey) then //and finally for old installer
	begin
		GetGimpInfo(HKLM, RegPath);
		InstallType := itOld;
	end else
	begin
		GimpPath := ExpandConstant('{pf}\GIMP 3'); //provide some defaults
		GimpVer := '0';
	end;
end;

function Is64BitGIMP(): boolean;
begin
	Result := InstallType = itNew64;
end;
