; 20-20-20 installer with a first-run configuration wizard.
; Built automatically by build.cmd when Inno Setup 6 is installed.

#define AppName "20-20-20"
#define AppExe "TwentyTwentyTwenty.exe"
#define AppVersion "1.0.0"
#define AppId "TwentyTwentyTwenty"

[Setup]
AppId={{7D5F3B2A-20E0-4C20-A020-202020202020}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Sadiq
DefaultDirName={localappdata}\{#AppId}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=TwentyTwentyTwenty-Setup
SetupIconFile=..\build\app.ico
WizardImageFile=..\build\wizard.bmp
WizardSmallImageFile=..\build\wizard-small.bmp
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
Compression=lzma2/ultra
SolidCompression=yes
WizardStyle=modern
CloseApplications=force
MinVersion=6.1

[Tasks]
Name: "startup"; Description: "Start {#AppName} automatically when I sign in to Windows"; GroupDescription: "Startup:"

[Files]
Source: "..\dist\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppId}"; ValueData: """{app}\{#AppExe}"" --minimized"; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#AppExe}"; Parameters: "--installed"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "taskkill.exe"; Parameters: "/f /im {#AppExe}"; Flags: runhidden; RunOnceId: "KillApp"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
var
  NamePage: TInputQueryWizardPage;
  SkinPage: TInputOptionWizardPage;
  IntervalPage: TInputOptionWizardPage;
  BreakPage: TInputOptionWizardPage;
  OptionsPage: TInputOptionWizardPage;

function SettingsPath(): String;
begin
  Result := ExpandConstant('{localappdata}\{#AppId}\settings.ini');
end;

function IsUpgrade(): Boolean;
begin
  Result := FileExists(SettingsPath());
end;

procedure InitializeWizard();
begin
  NamePage := CreateInputQueryPage(wpSelectTasks,
    'Meet your eye-care buddy', 'A pixel cat will sit on your desktop and remind you to rest your eyes.',
    'What would you like to call it?');
  NamePage.Add('Cat name:', False);
  NamePage.Values[0] := 'Mochi';

  SkinPage := CreateInputOptionPage(NamePage.ID,
    'Pick a look', 'Which coat should your cat wear?',
    'You can switch skins any time from the tray menu, or draw your own: one text character = one pixel.',
    True, False);
  SkinPage.Add('Tabby  -  classic orange');
  SkinPage.Add('Midnight  -  dark fur, glowing green eyes');
  SkinPage.Add('Snow  -  white fur, blue eyes');
  SkinPage.Add('Matcha  -  mint green');
  SkinPage.SelectedValueIndex := 0;

  IntervalPage := CreateInputOptionPage(SkinPage.ID,
    'Work interval', 'How often should the cat ask you to look away?',
    'The classic rule is every 20 minutes. Time spent locked, asleep or idle counts as a break automatically.',
    True, False);
  IntervalPage.Add('Every 15 minutes');
  IntervalPage.Add('Every 20 minutes  (recommended)');
  IntervalPage.Add('Every 25 minutes');
  IntervalPage.Add('Every 30 minutes');
  IntervalPage.SelectedValueIndex := 1;

  BreakPage := CreateInputOptionPage(IntervalPage.ID,
    'Break length', 'How long should each eye break last?',
    'Look at something about 20 feet (6 m) away for the whole countdown.',
    True, False);
  BreakPage.Add('20 seconds  (recommended)');
  BreakPage.Add('30 seconds');
  BreakPage.Add('60 seconds');
  BreakPage.SelectedValueIndex := 0;

  OptionsPage := CreateInputOptionPage(BreakPage.ID,
    'Options', 'Fine-tune how the reminder behaves.',
    'You can change all of these later from the tray icon menu.',
    False, False);
  OptionsPage.Add('Full-screen break on all monitors (otherwise a small window)');
  OptionsPage.Add('Play a sound when a break starts and ends');
  OptionsPage.Add('Show the cat on my desktop (otherwise tray icon only)');
  OptionsPage.Values[0] := True;
  OptionsPage.Values[1] := True;
  OptionsPage.Values[2] := True;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // On upgrade/reinstall keep the user's existing settings and skip the config pages.
  Result := IsUpgrade() and
    ((PageID = NamePage.ID) or (PageID = SkinPage.ID) or (PageID = IntervalPage.ID) or (PageID = BreakPage.ID) or (PageID = OptionsPage.ID));
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = NamePage.ID) and (Trim(NamePage.Values[0]) = '') then
  begin
    MsgBox('Please give your cat a name.', mbError, MB_OK);
    Result := False;
  end;
end;

function BoolStr(B: Boolean): String;
begin
  if B then Result := 'True' else Result := 'False';
end;

function IntervalMinutes(): Integer;
begin
  case IntervalPage.SelectedValueIndex of
    0: Result := 15;
    1: Result := 20;
    2: Result := 25;
  else
    Result := 30;
  end;
end;

function SkinName(): String;
begin
  case SkinPage.SelectedValueIndex of
    0: Result := 'Tabby';
    1: Result := 'Midnight';
    2: Result := 'Snow';
  else
    Result := 'Matcha';
  end;
end;

function BreakSeconds(): Integer;
begin
  case BreakPage.SelectedValueIndex of
    0: Result := 20;
    1: Result := 30;
  else
    Result := 60;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Ini: String;
begin
  if (CurStep = ssPostInstall) and not IsUpgrade() then
  begin
    ForceDirectories(ExtractFileDir(SettingsPath()));
    Ini :=
      'WorkMinutes=' + IntToStr(IntervalMinutes()) + #13#10 +
      'BreakSeconds=' + IntToStr(BreakSeconds()) + #13#10 +
      'FullScreen=' + BoolStr(OptionsPage.Values[0]) + #13#10 +
      'PlaySound=' + BoolStr(OptionsPage.Values[1]) + #13#10 +
      'Paused=False' + #13#10 +
      'ShowPet=' + BoolStr(OptionsPage.Values[2]) + #13#10 +
      'CatName=' + Trim(NamePage.Values[0]) + #13#10 +
      'SkinName=' + SkinName() + #13#10 +
      'PetScale=4' + #13#10 +
      'PetX=-1' + #13#10 +
      'PetY=-1' + #13#10;
    SaveStringToFile(SettingsPath(), Ini, False);
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  S: String;
begin
  S := '';
  if not IsUpgrade() then
  begin
    S := S + 'Your buddy:' + NewLine + Space + Trim(NamePage.Values[0]) + ' (' + SkinName() + ')' + NewLine + NewLine;
    S := S + 'Reminder:' + NewLine + Space + 'Every ' + IntToStr(IntervalMinutes()) + ' min, ' +
      IntToStr(BreakSeconds()) + ' s break' + NewLine + NewLine;
    S := S + 'Options:' + NewLine;
    if OptionsPage.Values[0] then S := S + Space + 'Full-screen on all monitors' + NewLine
    else S := S + Space + 'Small break window' + NewLine;
    if OptionsPage.Values[1] then S := S + Space + 'Sound on' + NewLine;
    if OptionsPage.Values[2] then S := S + Space + 'Desktop cat on' + NewLine;
    S := S + NewLine;
  end;
  if MemoTasksInfo <> '' then S := S + MemoTasksInfo + NewLine + NewLine;
  S := S + MemoDirInfo;
  Result := S;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    if MsgBox('Also remove your 20-20-20 settings?', mbConfirmation, MB_YESNO) = IDYES then
      DelTree(ExpandConstant('{localappdata}\{#AppId}'), True, True, True);
end;
