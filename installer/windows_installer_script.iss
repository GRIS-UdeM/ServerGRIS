#define AppName "SpatGris"
#define AppPublisher "Gris - UdeM"
#define AppURL "http://gris.musique.umontreal.ca/"
#define AppExeName "SpatGris.exe"
#define ControlGrisDir "ControlGris"
#define ManualName "SpatGRIS2_2.0_Manual.pdf"
#define RootDir ".."

#define BuildDir RootDir + "\Builds\VisualStudio2019\x64\Release\App"
#define ControlGrisVersionLong GetVersionNumbersString(ControlGrisDir + "\ControlGris.vst3")
#define ResourcesDir RootDir + "\Resources"

#define AppExePath BuildDir + "\" + AppExeName
#define ControlGrisVersion Copy(ControlGrisVersionLong, 0, Len(ControlGrisVersionLong) - 2)

#define AppVersionLong GetVersionNumbersString(AppExePath)

#define AppVersion Copy(AppVersionLong, 0, Len(AppVersionLong) - 2)

[Setup]
AppId={{5B626B1D-A428-406C-95A7-EFAE69ECD751}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
ArchitecturesInstallIn64BitMode=x64
Compression=lzma
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
LicenseFile="..\license.txt"
OutputDir=.
OutputBaseFilename="{#AppName}_{#AppVersion}_Windows_x64"  
PrivilegesRequired=admin 
SetupIconFile="{#ResourcesDir}\ServerGRIS_icon_doc.ico"
SolidCompression=yes
WizardStyle=modern
; Add the following line to run user mode (install for all users.)
;PrivilegesRequired=lowest
;PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; NOTE: Don't use "Flags: ignoreversion" on any shared system files
Source: "{#AppExePath}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\freeglut.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ResourcesDir}\hrtf_compact\*"; DestDir: "{app}\Resources\hrtf_compact"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ResourcesDir}\default_preset\*"; DestDir: "{app}\Resources\default_preset"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ResourcesDir}\templates\*"; DestDir: "{app}\Resources\templates"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ResourcesDir}\{#ManualName}"; DestDir: "{app}\Resources"; Flags: ignoreversion
Source: "{#ResourcesDir}\ServerGRIS_icon_splash_small.png"; DestDir: "{app}\Resources"; Flags: ignoreversion
Source: "{#ResourcesDir}\splash_screen.png"; DestDir: "{app}\Resources"; Flags: ignoreversion

; ControlGris
Source: "{#ControlGrisDir}\ControlGris.dll"; DestDir: {code:GetVST2Dir|0}; Components: VST64;
Source: "{#ControlGrisDir}\ControlGris.vst3"; DestDir: "{cf64}\VST3\"; Components: VST364;

[Components]
Name: "VST364"; Description: "ControlGris {#ControlGrisVersion} VST3"; Types: full custom;
Name: "VST64"; Description: "ControlGris {#ControlGrisVersion} VST"; Types: custom;

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon
Name: "{group}\Uninstall {#AppName} {#AppVersion}"; Filename: {uninstallexe}

[Run]
Filename: "{app}\Resources\{#ManualName}"; WorkingDir: "{app}\Resources"; Description: "Open PDF Users Manual"; Flags: shellexec postinstall skipifsilent nowait
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; code shared by discoDSP https://www.kvraudio.com/forum/viewtopic.php?t=501615
[Code]
var
  VST2DirPage: TInputDirWizardPage;
  TypesComboOnChangePrev: TNotifyEvent;

procedure ComponentsListCheckChanges;
begin
  WizardForm.NextButton.Enabled := (WizardSelectedComponents(False) <> '');
end;

procedure ComponentsListClickCheck(Sender: TObject);
begin
  ComponentsListCheckChanges;
end;

procedure TypesComboOnChange(Sender: TObject);
begin
  TypesComboOnChangePrev(Sender);
  ComponentsListCheckChanges;
end;

procedure InitializeWizard;
begin

  WizardForm.ComponentsList.OnClickCheck := @ComponentsListClickCheck;
  TypesComboOnChangePrev := WizardForm.TypesCombo.OnChange;
  WizardForm.TypesCombo.OnChange := @TypesComboOnChange;

  VST2DirPage := CreateInputDirPage(wpSelectComponents,
  'Confirm VST2 Plugin Directory', '',
  'Select the folder in which setup should install the VST2 Plugin, then click Next.',
  False, '');

  VST2DirPage.Add('64-bit folder');
  VST2DirPage.Values[0] := GetPreviousData('VST64', ExpandConstant('{reg:HKLM\SOFTWARE\VST,VSTPluginsPath|{pf}\Steinberg\VSTPlugins}\gris'));

  If not Is64BitInstallMode then
  begin
    VST2DirPage.Buttons[0].Enabled := False;
    VST2DirPage.PromptLabels[0].Enabled := VST2DirPage.Buttons[0].Enabled;
    VST2DirPage.Edits[0].Enabled := VST2DirPage.Buttons[0].Enabled;
  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = VST2DirPage.ID then
  begin
    VST2DirPage.Buttons[0].Enabled := IsComponentSelected('VST64');
    VST2DirPage.PromptLabels[0].Enabled := VST2DirPage.Buttons[0].Enabled;
    VST2DirPage.Edits[0].Enabled := VST2DirPage.Buttons[0].Enabled;
  end;

  if CurPageID = wpSelectComponents then
  begin
    ComponentsListCheckChanges;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  if PageID = VST2DirPage.ID then
  begin
    If (not IsComponentSelected('VST')) and (not IsComponentSelected('VST64'))then
      begin
        Result := True
      end;
  end;
end;

function GetVST2Dir(Param: string): string;
begin
    Result := VST2DirPage.Values[StrToInt(Param)];
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'VST64', VST2DirPage.Values[0]);
end;