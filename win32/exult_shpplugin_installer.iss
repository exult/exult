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
AppName=Ultima 7 Shape plug-in for GIMP
AppVerName=Shape plug-in Git
AppPublisherURL=https://exult.info/
AppSupportURL=https://exult.info/
AppUpdatesURL=https://exult.info/
; Setup exe version number:
VersionInfoVersion=1.9.0
DisableDirPage=no
DefaultDirName={code:GetGimpDir|{pf}\GIMP 2}
DisableProgramGroupPage=yes
DefaultGroupName=Exult GIMP Plugin
OutputBaseFilename=Gimp20Plugin
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
UninstallFilesDir={code:GetUninstallDir}

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; 32-bit files
Source: "GimpPlugin-i686\u7shp.exe"; DestDir: "{app}\lib\gimp\2.0\plug-ins"; Flags: ignoreversion; Check: not Is64BitGIMP
; 64-bit files
Source: "GimpPlugin-x86_64\u7shp.exe"; DestDir: "{app}\lib\gimp\2.0\plug-ins"; Flags: ignoreversion; Check: Is64BitGIMP

[Code]
function SHAutoComplete(hWnd: Integer; dwFlags: DWORD): Integer; external 'SHAutoComplete@shlwapi.dll stdcall delayload';

function WideCharToMultiByte(CodePage: Cardinal; dwFlags: DWORD; lpWideCharStr: String; cchWideCharStr: Integer;
							 lpMultiByteStr: PAnsiChar; cbMultiByte: Integer; lpDefaultChar: Integer;
							 lpUsedDefaultChar: Integer): Integer; external 'WideCharToMultiByte@Kernel32 stdcall';

function MultiByteToWideChar(CodePage: Cardinal; dwFlags: DWORD; lpMultiByteStr: PAnsiChar; cbMultiByte: Integer;
							 lpWideCharStr: String; cchWideChar: Integer): Integer;
							 external 'MultiByteToWideChar@Kernel32 stdcall';

function GetLastError(): DWORD; external 'GetLastError@Kernel32 stdcall';

const
	SHACF_FILESYSTEM = $1;
	CP_UTF8 = 65001;

	RegPath = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\WinGimp-2.0_is1';
	RegPathNew = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GIMP-2_is1';
	RegKey = 'Inno Setup: App Path';
	VerKey = 'DisplayName';

var
	GimpPath: String;
	GimpVer: String;

	InstallType: (itOld, itNew, itNew64);

function Count(What, Where: String): Integer;
begin
	Result := 0;
	if Length(What) = 0 then //nothing to count - should this throw an error?
		exit;

	while Pos(What,Where)>0 do
	begin
		Where := Copy(Where,Pos(What,Where)+Length(What),Length(Where));
		Result := Result + 1;
	end;
end;


//split text to array
procedure Explode(var ADest: TArrayOfString; aText, aSeparator: String);
var tmp: Integer;
begin
	if aSeparator='' then
		exit;

	SetArrayLength(ADest,Count(aSeparator,aText)+1)

	tmp := 0;
	repeat
		if Pos(aSeparator,aText)>0 then
		begin

			ADest[tmp] := Copy(aText,1,Pos(aSeparator,aText)-1);
			aText := Copy(aText,Pos(aSeparator,aText)+Length(aSeparator),Length(aText));
			tmp := tmp + 1;

		end else
		begin

			 ADest[tmp] := aText;
			 aText := '';

		end;
	until Length(aText)=0;
end;

//compares two version numbers, returns -1 if vA is newer, 0 if both are identical, 1 if vB is newer
function CompareVersion(vA,vB: String): Integer;
var tmp: TArrayOfString;
	verA,verB: Array of Integer;
	i,len: Integer;
begin

	StringChange(vA,'-','.');
	StringChange(vB,'-','.');

	Explode(tmp,vA,'.');
	SetArrayLength(verA,GetArrayLength(tmp));
	for i := 0 to GetArrayLength(tmp) - 1 do
		verA[i] := StrToIntDef(tmp[i],0);

	Explode(tmp,vB,'.');
	SetArrayLength(verB,GetArrayLength(tmp));
	for i := 0 to GetArrayLength(tmp) - 1 do
		verB[i] := StrToIntDef(tmp[i],0);

	len := GetArrayLength(verA);
	if GetArrayLength(verB) < len then
		len := GetArrayLength(verB);

	for i := 0 to len - 1 do
		if verA[i] < verB[i] then
		begin
			Result := 1;
			exit;
		end else
		if verA[i] > verB[i] then
		begin
			Result := -1;
			exit
		end;

	if GetArrayLength(verA) < GetArrayLength(verB) then
	begin
		Result := 1;
		exit;
	end else
	if GetArrayLength(verA) > GetArrayLength(verB) then
	begin
		Result := -1;
		exit;
	end;

	Result := 0;
end;

function Utf82String(const pInput: AnsiString): String;
#ifndef UNICODE
	#error "Unicode Inno Setup required"
#endif
var Output: String;
	ret, outLen, nulPos: Integer;
begin
	outLen := MultiByteToWideChar(CP_UTF8, 0, pInput, -1, Output, 0);
	Output := StringOfChar(#0,outLen);
	ret := MultiByteToWideChar(CP_UTF8, 0, pInput, -1, Output, outLen);

	if ret = 0 then
		RaiseException('MultiByteToWideChar failed: ' + IntToStr(GetLastError));

	nulPos := Pos(#0,Output) - 1;
	if nulPos = -1 then
		nulPos := Length(Output);

	Result := Copy(Output,1,nulPos);
end;

function LoadStringFromUTF8File(const pFileName: String; var oS: String): Boolean;
var Utf8String: AnsiString;
begin
	Result := LoadStringFromFile(pFileName, Utf8String);
	oS := Utf82String(Utf8String);
end;

function String2Utf8(const pInput: String): AnsiString;
var Output: AnsiString;
	ret, outLen, nulPos: Integer;
begin
	outLen := WideCharToMultiByte(CP_UTF8, 0, pInput, -1, Output, 0, 0, 0);
	Output := StringOfChar(#0,outLen);
	ret := WideCharToMultiByte(CP_UTF8, 0, pInput, -1, Output, outLen, 0, 0);

	if ret = 0 then
		RaiseException('WideCharToMultiByte failed: ' + IntToStr(GetLastError));

	nulPos := Pos(#0,Output) - 1;
	if nulPos = -1 then
		nulPos := Length(Output);

	Result := Copy(Output,1,nulPos);
end;

function SaveStringToUTF8File(const pFileName, pS: String; const pAppend: Boolean): Boolean;
begin
	Result := SaveStringToFile(pFileName, String2Utf8(pS), pAppend);
end;

procedure SaveToUninstInf(const pText: AnsiString);
var sUnInf: String;
	sOldContent: String;
begin
	sUnInf := ExpandConstant('{app}\uninst\uninst.inf');

	if not FileExists(sUnInf) then //save small header
		SaveStringToUTF8File(sUnInf,#$feff+'#Additional uninstall tasks'#13#10+ //#$feff BOM is required for LoadStringsFromFile
									'#This file uses UTF-8 encoding'#13#10+
									'#'#13#10+
									'#Empty lines and lines beginning with # are ignored'#13#10+
									'#'#13#10+
									'#Add uninstallers for GIMP add-ons that should be removed together with GIMP like this:'#13#10+
									'#Run:<description>/<full path to uninstaller>/<parameters for automatic uninstall>'#13#10+
									'#'#13#10+
									'#The file is parsed in reverse order' + #13#10 +
									'' + #13#10 //needs '' in front, otherwise preprocessor complains
									,False)
	else
	begin
		if LoadStringFromUTF8File(sUnInf,sOldContent) then
			if Pos(#13#10+pText+#13#10,sOldContent) > 0 then //don't write duplicate lines
				exit;
	end;

	SaveStringToUTF8File(sUnInf,pText+#13#10,True);
end;

procedure GetGimpInfo(const pRootKey: Integer; const pRegPath: String);
var p: Integer;
begin
	If not RegQueryStringValue(pRootKey, pRegPath, RegKey, GimpPath) then //find Gimp install dir
	begin
		GimpPath := ExpandConstant('{pf}\GIMP 2');
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

procedure WriteUninstallInfo();
begin
	if InstallType <> itOld then //new installer expects components to be listed in the uninst.inf file
		SaveToUninstInf('Run:GIMP Help/'+ExpandConstant('{uninstallexe}')+'//SILENT /NORESTART');
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
		GimpPath := ExpandConstant('{pf}\GIMP 2'); //provide some defaults
		GimpVer := '0';
	end;
end;

function Is64BitGIMP(): boolean;
begin
	Result := InstallType = itNew64;
end;

procedure CurStepChanged(pCurStep: TSetupStep);
begin
	case pCurStep of
		ssPostInstall:
		begin
			WriteUninstallInfo();
		end;
	end;
end;

procedure InitializeWizard();
var r: Integer;
begin
	r := SHAutoComplete(WizardForm.DirEdit.Handle,SHACF_FILESYSTEM);
end;

function GetGimpDir(S: String): String;
begin
	Result := GimpPath;
end;

function GetUninstallDir(default: String): String;
begin
	if CompareVersion(GimpVer,'2.10.0') >= 0 then
		Result := ExpandConstant('{app}')
	else
	begin
		if InstallType = itOld then
			Result := ExpandConstant('{app}\setup')
		else
			Result := ExpandConstant('{app}\uninst');
	end;
end;
