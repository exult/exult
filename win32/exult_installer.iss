[Setup]
AppName=Exult
AppVerName=Exult 1.9.0git Snapshot
AppPublisher=The Exult Team
AppPublisherURL=https://exult.info/
AppSupportURL=https://exult.info/
AppUpdatesURL=https://exult.info/
; Setup exe version number:
VersionInfoVersion=1.9.0
DisableDirPage=no
DefaultDirName={code:GetExultInstDir|{autopf}\Exult}
DisableProgramGroupPage=no
DefaultGroupName={code:GetExultGroupDir|Exult}
OutputBaseFilename=Exult
Compression=lzma
SolidCompression=yes
InternalCompressLevel=max
OutputDir=.
DisableWelcomePage=no
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible

[Dirs]
Name: "{app}\data"

[Types]
Name: full; Description: Full installation
Name: compact; Description: Compact installation
Name: pathsonly; Description: Setup Paths Only
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: Exult; Description: Install Exult; Types: full compact
Name: Docs; Description: Install Exult Documentation; Types: full
Name: GPL; Description: Install GPL License and Link to Source; Types: full compact custom; Flags: fixed
Name: Paths; Description: Setup Game Paths; Types: full compact custom pathsonly
Name: Icons; Description: Create Start Menu Icons; Types: full compact
Name: "downloads"; Description: "Download and install"; Types: full custom
Name: "downloads\audio"; Description: "Digital music and sound effects"; Types: full custom; ExtraDiskSpaceRequired: 48439905
Name: "downloads\mods"; Description: "Mods"; Types: full custom
Name: "downloads\mods\keyring"; Description: "Keyring mod for The Black Gate"; Types: full custom; ExtraDiskSpaceRequired: 750027
Name: "downloads\mods\sfisland"; Description: "SourceForge Island mod for The Black Gate"; Types: full custom; ExtraDiskSpaceRequired: 284857
Name: "downloads\mods\sifixes"; Description: "SIFixes mod for Serpent Isle"; Types: full custom; ExtraDiskSpaceRequired: 334482
Name: "downloads\3rdpartymods"; Description: "Mods not created by the Exult team"; Types: full custom;
Name: "downloads\3rdpartymods\ultima6"; Description: "Ultima VI Remake for The Black Gate"; Types: full custom; ExtraDiskSpaceRequired: 65341451
Name: "downloads\3rdpartymods\glimmerscape"; Description: "Glimmerscape by Donfrow for Serpent Isle"; Types: full custom; ExtraDiskSpaceRequired: 13187091

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
; 32-bit files
Source: Exult-i686\Exult.exe; DestDir: {app}; Flags: ignoreversion; Components: Exult; Check: not Is64BitInstallMode
Source: Exult-i686\*.dll; DestDir: {app};  Flags: ignoreversion; Components: Exult; Check: not Is64BitInstallMode
; 64-bit files
Source: Exult-x86_64\Exult.exe; DestDir: {app}; Flags: ignoreversion; Components: Exult; Check: Is64BitInstallMode
Source: Exult-x86_64\*.dll; DestDir: {app};  Flags: ignoreversion; Components: Exult; Check: Is64BitInstallMode
; Architecture-neutral files
Source: Exult-i686\COPYING.txt; DestDir: {app}; Flags: ignoreversion; Components: GPL
Source: Exult-i686\Exult Source Code.url; DestDir: {app}; Flags: ignoreversion; Components: GPL
Source: Exult-i686\README-SDL.txt; DestDir: {app}; Flags: ignoreversion; Components: Exult
Source: Exult-i686\AUTHORS.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\bgdefaultkeys.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\ChangeLog.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\faq.html; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\FAQ.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\NEWS.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\ReadMe.html; DestDir: {app}; Flags: ignoreversion isreadme; Components: Docs
Source: Exult-i686\README.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\README.win32.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\sidefaultkeys.txt; DestDir: {app}; Flags: ignoreversion; Components: Docs
Source: Exult-i686\images\*.gif; DestDir: {app}\images; Flags: ignoreversion nocompression; Components: Docs
Source: Exult-i686\images\docs*.png; DestDir: {app}\images; Flags: ignoreversion nocompression; Components: Docs
Source: Exult-i686\Data\exult.flx; DestDir: {app}\data; Flags: ignoreversion; Components: Exult
Source: Exult-i686\Data\exult_bg.flx; DestDir: {app}\data; Flags: ignoreversion; Components: Exult
Source: Exult-i686\Data\exult_si.flx; DestDir: {app}\data; Flags: ignoreversion; Components: Exult
; Always the 32-bit version
Source: exconfig-i686.dll; Flags: dontcopy

[Icons]
Name: {group}\Exult; Filename: {app}\Exult.exe; WorkingDir: {app}; Flags: createonlyiffileexists; Components: Icons
Name: {group}\reset video settings; Filename: {app}\Exult.exe; Parameters: --reset-video; WorkingDir: {app}; Flags: createonlyiffileexists; Components: Icons
Name: {group}\Uninstall Exult; Filename: {uninstallexe}; Components: Icons
Name: {group}\FAQ; Filename: {app}\FAQ.html; Flags: createonlyiffileexists; Components: Icons
Name: {group}\Readme; Filename: {app}\ReadMe.html; Flags: createonlyiffileexists; Components: Icons
Name: {group}\Exult Source Code; Filename : {app}\Exult Source Code.url; Components: Icons
; Name: {group}\COPYING; Filename: {app}\COPYING.txt; Flags: createonlyiffileexists; Components: Icons
; Name: {group}\ChangeLog; Filename: {app}\ChangeLog.txt; Flags: createonlyiffileexists; Components: Icons
; Name: {group}\Readme Win32; Filename: {app}\README.win32.txt; Flags: createonlyiffileexists; Components: Icons
; Name: {group}\NEWS; Filename: {app}\NEWS.txt; Flags: createonlyiffileexists; Components: Icons

[Run]
Filename: {app}\Exult.exe; Description: {cm:LaunchProgram,Exult}; WorkingDir: {app}; Flags: nowait postinstall skipifsilent skipifdoesntexist

[Code]
var
  DataDirPage: TWizardPage;
  bSetPaths: Boolean;
  BGBrowseButton: TButton;
  SIBrowseButton: TButton;
  BGText: TNewStaticText;
  SIText: TNewStaticText;
  BGEdit: TEdit;
  SIEdit: TEdit;
  BGPath: String;
  SIPath: String;
  DownloadPage: TDownloadWizardPage;
  PrevItemAChecked: Boolean;
  iBGVerified: Integer;
  iSIVerified: Integer;

// Get Paths from Exult.cfg
procedure GetExultGamePaths(sExultDir, sBGPath, sSIPath: AnsiString; iMaxPath: Integer);
external 'GetExultGamePaths@files:exconfig-i686.dll cdecl';

// Write Paths into Exult.cfg
procedure SetExultGamePaths(sExultDir, sBGPath, sSIPath: AnsiString);
external 'SetExultGamePaths@files:exconfig-i686.dll cdecl';

// Verify BG Dir
function VerifyBGDirectory(sPath: AnsiString) : Integer;
external 'VerifyBGDirectory@files:exconfig-i686.dll cdecl';

// Verify SI Dir
function VerifySIDirectory(sPath: AnsiString) : Integer;
external 'VerifySIDirectory@files:exconfig-i686.dll cdecl';

// Get the Previous Exult Installation Dir
// This is done in a manner that is compatible with the old InstallShield setup
function GetExultInstDir(sDefault: String): String;
var
  sPath: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'Software\Exult', 'Path', sPath) then begin
    result := sPath;
  end else
    result := sDefault;
end;

// Get the Previous Exult StartMenu Dir
// This is done in a manner that is compatible with the old InstallShield setup
function GetExultGroupDir(sDefault: String): String;
var
  sPath: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'Software\Exult', 'ShellObjectFolder', sPath) then begin
    result := sPath;
  end else
    result := sDefault;
end;

procedure OnBGBrowseButton(Sender: TObject);
var
  sDir : string;
begin
    sDir := BGEdit.Text;
    if BrowseForFolder('Select the folder where Ultima VII: The Black Gate is installed.', sDir, False ) then begin
      if VerifyBGDirectory ( sDir ) = 0 then begin
        if MsgBox ('Folder does not seem to contain a valid installation of Ultima VII: The Black Gate. Do you wish to continue?', mbConfirmation, MB_YESNO) = IDYES then begin
          BGEdit.Text := sDir;
          BGPath := sDir;
        end
      end else begin
        BGEdit.Text := sDir;
        BGPath := sDir;
      end
    end;
end;

procedure OnSIBrowseButton(Sender: TObject);
var
  sDir : string;
begin
    sDir := SIEdit.Text;
    if BrowseForFolder('Select the folder where Ultima VII Part 2: Serpent Isle is installed.', sDir, False ) then begin
      if VerifySIDirectory ( sDir ) = 0 then begin
        if MsgBox ('Folder does not seem to contain a valid installation of Ultima VII Part 2: Serpent Isle. Do you wish to continue?', mbConfirmation, MB_YESNO) = IDYES then begin
          SIEdit.Text := sDir;
          SIPath := sDir;
        end
      end else begin
        SIEdit.Text := sDir;
        SIPath := sDir;
      end
    end;
end;

function OnDownloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  if Progress = ProgressMax then
    Log(Format('Successfully downloaded file to {tmp}: %s', [FileName]));
  Result := True;
end;

//
// unzip function for our downloads
//
const
  SHCONTCH_NOPROGRESSBOX = 4;
  SHCONTCH_RESPONDYESTOALL = 16;

procedure UnZip(ZipPath, TargetPath: string);
var
  Shell: Variant;
  ZipFile: Variant;
  TargetFolder: Variant;
begin
  Shell := CreateOleObject('Shell.Application');

  ZipFile := Shell.NameSpace(ZipPath);
  if VarIsClear(ZipFile) then
    RaiseException(
      Format('ZIP file "%s" does not exist or cannot be opened', [ZipPath]));

  TargetFolder := Shell.NameSpace(TargetPath);
  if VarIsClear(TargetFolder) then
    RaiseException(Format('Target path "%s" does not exist', [TargetPath]));

  TargetFolder.CopyHere(
    ZipFile.Items, SHCONTCH_NOPROGRESSBOX or SHCONTCH_RESPONDYESTOALL);
end;

procedure ExtractMe(src, target : AnsiString);
begin
  Log(Format('Zipfile "%s" ', [ExpandConstant(src)]));
  Log(Format('TargetPath "%s" ', [ExpandConstant(target)]));
  UnZip(ExpandConstant(src), ExpandConstant(target));
end;

//
// Create the Directory browsing page
//
procedure InitializeWizard;
begin
  DataDirPage := CreateCustomPage(wpSelectTasks,
    'Select Game Folders', 'Select the folders where Ultima VII and Ultima VII Part 2 are installed.');

  BGText := TNewStaticText.Create(DataDirPage);
  BGText.Caption := 'Please enter the path where Ultima VII: The Black Gate is installed.';
  BGText.AutoSize := True;
  BGText.Parent := DataDirPage.Surface;

  BGBrowseButton := TButton.Create(DataDirPage);
  BGBrowseButton.Top := BGText.Top + BGText.Height + ScaleY(8);
  BGBrowseButton.Left := DataDirPage.SurfaceWidth - ScaleX(75);
  BGBrowseButton.Width := ScaleX(75);
  BGBrowseButton.Height := ScaleY(23);
  BGBrowseButton.Caption := '&Browse';
  BGBrowseButton.OnClick := @OnBGBrowseButton;
  BGBrowseButton.Parent := DataDirPage.Surface;

  BGEdit := TEdit.Create(DataDirPage);
  BGEdit.Top := BGText.Top + BGText.Height + ScaleY(8);
  BGEdit.Width := DataDirPage.SurfaceWidth - (BGBrowseButton.Width + ScaleX(8));
  BGEdit.Text := '';
  BGPath := '';
  BGEdit.Parent := DataDirPage.Surface;


  SIText := TNewStaticText.Create(DataDirPage);
  SIText.Caption := 'Please enter the path where Ultima VII Part 2: Serpent Isle is installed.';
  SIText.AutoSize := True;
  SIText.Top := BGEdit.Top + BGEdit.Height + ScaleY(23);
  SIText.Parent := DataDirPage.Surface;

  SIBrowseButton := TButton.Create(DataDirPage);
  SIBrowseButton.Top := SIText.Top + SIText.Height + ScaleY(8);
  SIBrowseButton.Left := DataDirPage.SurfaceWidth - ScaleX(75);
  SIBrowseButton.Width := ScaleX(75);
  SIBrowseButton.Height := ScaleY(23);
  SIBrowseButton.Caption := 'Brow&se';
  SIBrowseButton.OnClick := @OnSIBrowseButton;
  SIBrowseButton.Parent := DataDirPage.Surface;

  SIEdit := TEdit.Create(DataDirPage);
  SIEdit.Top := SIText.Top + SIText.Height + ScaleY(8);
  SIEdit.Width := DataDirPage.SurfaceWidth - (SIBrowseButton.Width + ScaleX(8));
  SIEdit.Text := '';
  SIPath := '';
  SIEdit.Parent := DataDirPage.Surface;

  bSetPaths := False;

  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), SetupMessage(msgPreparingDesc), @OnDownloadProgress);
end;

//
// Read in the config file if needed
//
procedure CurPageChanged(CurPageID: Integer);
var
  sBGPath: AnsiString;
  sSIPath: AnsiString;
begin
  if CurPageID = DataDirPage.ID then begin
    if bSetPaths = False then begin
      setlength(sBGPath, 1024);
      setlength(sSIPath, 1024);
      GetExultGamePaths(ExpandConstant('{app}'), sBGPath, sSIPath, 1023 );
      BGEdit.Text := sBGPath;
      SIEdit.Text := sSIPath;
    end
  end;
end;

//
// Make sure the SI and BG Paths are correct when we hit next button
//
function NextButtonClick(CurPageID: Integer): Boolean;
var 
  sDir : string;
begin
  if CurPageID = DataDirPage.ID then begin
      sDir := BGEdit.Text;
      if (Length(sDir) > 0) then begin
        iBGVerified := VerifyBGDirectory ( sDir );
      end else
        iBGVerified := 0;

      if (iBGVerified = 1) then
        BGPath := sDir;

     sDir := SIEdit.Text;
      if (Length(sDir) > 0) then begin
        iSIVerified := VerifySIDirectory ( sDir );
      end else 
        iSIVerified := 0;

      if (iSIVerified = 1) then
        SIPath := sDir;

      if (iBGVerified = 0) AND (iSIVerified = 0) then begin
        if MsgBox ('Warning: No valid game installations found. Do you wish to continue?', mbError, MB_YESNO or MB_DEFBUTTON2) = IDYES then begin
          Result := True;
        end else
          Result := False;
      end else if (iBGVerified = 0)  AND (iSIVerified = 1) then begin
        if MsgBox ('Warning: No valid installation of Ultima VII: The Black Gate. Do you wish to continue?', mbError, MB_YESNO or MB_DEFBUTTON2) = IDYES then begin
          Result := True;
        end else
          Result := False;
      end else if (iSIVerified = 0)  AND (iBGVerified = 1) then begin
        if MsgBox ('Warning: No valid installation of Ultima VII Part 2: Serpent Isle. Do you wish to continue?', mbError, MB_YESNO or MB_DEFBUTTON2) = IDYES then begin
          Result := True;
        end else
          Result := False;
      end else
        Result := True;

// Download page: Only download if the component was selected AND game paths are verified
  end else if CurPageID = wpReady then begin
      DownloadPage.Clear;
      if PrevItemAChecked <> WizardIsComponentSelected('downloads\audio') then
        DownloadPage.Add('https://downloads.sourceforge.net/project/exult/exult-data/exult_audio.zip', 'exult_audio.zip','72e10efa8664a645470ceb99f6b749ce99c3d5fd1c8387c63640499cfcdbbc68');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\keyring')) AND (iBGVerified = 1) then
        DownloadPage.Add('https://github.com/exult/exult/releases/latest/download/Keyring.zip', 'Keyring.zip', '');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\sfisland')) AND (iBGVerified = 1) then
        DownloadPage.Add('https://github.com/exult/exult/releases/latest/download/SFisland.zip', 'Sfisland.zip', '');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\sifixes')) AND (iSIVerified = 1) then
        DownloadPage.Add('https://github.com/exult/exult/releases/latest/download/Sifixes.zip', 'Sifixes.zip', '');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\3rdpartymods\ultima6')) AND (iBGVerified = 1) then
        DownloadPage.Add('https://exult.info/snapshots/Ultima6.zip', 'Ultima6.zip', '');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\3rdpartymods\glimmerscape')) AND (iSIVerified = 1) then
        DownloadPage.Add('https://exult.info/snapshots/Glimmerscape_SI_mod_by_Donfrow.zip', 'Glimmerscape_SI_mod_by_Donfrow.zip', '');
      DownloadPage.Show;
      try
        try
          DownloadPage.Download; // This downloads the files to {tmp}
          Result := True;
        except
          if DownloadPage.AbortedByUser then
            Log('Aborted by user.')
          else
            SuppressibleMsgBox(AddPeriod(GetExceptionMessage), mbCriticalError, MB_OK, IDOK);
          Result := False;
        end;
      finally
        DownloadPage.Hide;
      end;
  end else
    Result := True;
end;

//
// Skip page because of components?
//
function ShouldSkipPage(PageID: Integer): Boolean;
var
  sBGPath: AnsiString;
  sSIPath: AnsiString;
begin
  if PageID = DataDirPage.ID then begin
    Result := (WizardIsComponentSelected('Paths') = False);

    if Result = True then begin
      if bSetPaths = False then begin
        setlength(sBGPath, 1024);
        setlength(sSIPath, 1024);
        GetExultGamePaths(ExpandConstant('{app}'), sBGPath, sSIPath, 1023 );
        BGEdit.Text := sBGPath;
        SIEdit.Text := sSIPath;
        BGPath := sBGPath;
        SIPath := sSIPath;
      end;

      if ( CompareStr(BGEdit.Text,'') = 0) and (CompareStr(SIEdit.Text,'') = 0) then
        Result := False;
    end

  end else if PageID = wpSelectProgramGroup then begin
    Result := (WizardIsComponentSelected('Icons') = False);
  end else
    Result := False;
end;

//
// Write out the Config file, Registry Entries and unzip the downloads
//
procedure CurStepChanged(CurStep: TSetupStep);
var
  sBGmods: String;
  sSImods: String;
begin
  if CurStep = ssPostInstall then begin
    sBGmods := BGPath + '\mods';
    sSImods := SIPath + '\mods';
    SetExultGamePaths(ExpandConstant('{app}'), BGPath, SIPath );
    RegWriteStringValue(HKEY_LOCAL_MACHINE, 'Software\Exult', 'Path', ExpandConstant('{app}'));
    if WizardIsComponentSelected('Icons') then
      RegWriteStringValue(HKEY_LOCAL_MACHINE, 'Software\Exult', 'ShellObjectFolder', ExpandConstant('{groupname}'));

    // Create mods subfolders
    if (iBGVerified = 1) then
      ForceDirectories(sBGmods);
    if (iSIVerified = 1) then
      ForceDirectories(sSImods);

    // Installing the downloads
    if not RegKeyExists(HKEY_LOCAL_MACHINE, 'Software\Wine') then begin
      // again check if component was selected and game paths are verified
      if PrevItemAChecked <> WizardIsComponentSelected('downloads\audio') then
        ExtractMe('{tmp}\exult_audio.zip','{app}\data\');
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\keyring')) AND (iBGVerified = 1) then
        ExtractMe('{tmp}\Keyring.zip',sBGmods);
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\sfisland')) AND (iBGVerified = 1) then
        ExtractMe('{tmp}\SFisland.zip',sBGmods);
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\mods\sifixes')) AND (iSIVerified = 1) then
        ExtractMe('{tmp}\Sifixes.zip',sSImods);
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\3rdpartymods\ultima6')) AND (iBGVerified = 1) then
        ExtractMe('{tmp}\Ultima6.zip',sBGmods);
      if (PrevItemAChecked <> WizardIsComponentSelected('downloads\3rdpartymods\glimmerscape')) AND (iSIVerified = 1) then
        ExtractMe('{tmp}\Glimmerscape_SI_mod_by_Donfrow.zip',sSImods);

    // Wine doesn't support the Windows built-in unzip method hence will crash at installing the downloads
    end else if RegKeyExists(HKEY_LOCAL_MACHINE, 'Software\Wine') then begin
      MsgBox('Warning: You seem to be using Wine. Unfortunately the installer cannnot install the extra downloads on Wine!', mbInformation, MB_OK);
    end;
  end;
end;
