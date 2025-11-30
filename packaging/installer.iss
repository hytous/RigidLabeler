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
UninstallDisplayIcon={app}\RigidLabeler.exe

; Compression
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes

; License and info files (optional)
; LicenseFile=..\LICENSE
; InfoBeforeFile=..\README.md

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main launcher executable
Source: "dist\RigidLabeler.exe"; DestDir: "{app}"; Flags: ignoreversion

; Frontend executable and Qt dependencies (all in app root)
Source: "dist\frontend.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\iconengines'))
Source: "dist\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\imageformats'))
Source: "dist\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\platforms'))
Source: "dist\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\styles'))
Source: "dist\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\translations'))

; Backend files
Source: "dist\rigidlabeler_backend\*"; DestDir: "{app}\rigidlabeler_backend"; Flags: ignoreversion recursesubdirs createallsubdirs

; Config files
Source: "dist\config\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: DirExists(ExpandConstant('{#SourcePath}\dist\config'))

[Dirs]
; Create data directory for user files
Name: "{app}\data"

[Icons]
; Start menu shortcut - points to launcher
Name: "{group}\{#MyAppName}"; Filename: "{app}\RigidLabeler.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

; Desktop shortcut
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\RigidLabeler.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Option to launch app after installation
Filename: "{app}\RigidLabeler.exe"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

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
