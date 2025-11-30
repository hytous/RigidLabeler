; RigidLabeler Inno Setup Script
; This script creates a professional Windows installer

#define MyAppName "RigidLabeler"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "RigidLabeler Team"
#define MyAppURL "https://github.com/hytous/RigidLabeler"
#define MyAppExeName "RigidLabeler.exe"

[Setup]
; Basic installer info
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Installation settings
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
DisableDirPage=no

; Output settings
OutputDir=Output
OutputBaseFilename=RigidLabeler_Setup_{#MyAppVersion}
SetupIconFile=ico.ico
UninstallDisplayIcon={app}\frontend\frontend.exe

; Compression
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes

; Privileges (admin not required for user install)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; Visual settings
WizardStyle=modern
WizardSizePercent=120

; License and info files (optional)
; LicenseFile=..\LICENSE
; InfoBeforeFile=..\README.md

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode

[Files]
; Frontend files
Source: "dist\frontend\*"; DestDir: "{app}\frontend"; Flags: ignoreversion recursesubdirs createallsubdirs

; Backend files
Source: "dist\backend\rigidlabeler_backend\*"; DestDir: "{app}\backend\rigidlabeler_backend"; Flags: ignoreversion recursesubdirs createallsubdirs

; Config files
Source: "dist\config\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\config'))

; Launcher
Source: "dist\RigidLabeler.bat"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; Create data directory for user files
Name: "{app}\data"

[Icons]
; Start menu shortcut
Name: "{group}\{#MyAppName}"; Filename: "{app}\RigidLabeler.bat"; IconFilename: "{app}\frontend\frontend.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop shortcut
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\RigidLabeler.bat"; IconFilename: "{app}\frontend\frontend.exe"; WorkingDir: "{app}"; Tasks: desktopicon

; Quick launch shortcut
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\RigidLabeler.bat"; IconFilename: "{app}\frontend\frontend.exe"; Tasks: quicklaunchicon

[Run]
; Option to launch app after installation
Filename: "{app}\RigidLabeler.bat"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent shellexec

[UninstallDelete]
; Clean up generated files
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\config"

[Code]
// Custom code for additional functionality

// Check if VC++ Runtime is installed (optional, for future use)
function IsVCRedistInstalled(): Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(HKLM, 
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 
    'Version', Version);
end;

// Show a welcome message
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption := 
    'This will install {#MyAppName} {#MyAppVersion} on your computer.' + #13#10 + #13#10 +
    'RigidLabeler is a tool for labeling rigid image registration transformations.' + #13#10 + #13#10 +
    'It is recommended that you close all other applications before continuing.';
end;
