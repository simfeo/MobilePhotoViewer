; Inno Setup script for MobilePhotoViewer.
; Driven by the GitHub Actions release workflow, which passes:
;   /DMyAppVersion=<version>   (from the git tag)
;   /DSourceDir=<deploy dir>   (folder holding MobilePhotoViewer.exe + all deployed DLLs/plugins)
; and expects the compiled installer in .\Output\.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\Release"
#endif

#define MyAppName "MobilePhotoViewer"
#define MyAppExeName "MobilePhotoViewer.exe"
#define MyAppPublisher "simfeo"
#define MyAppURL "https://github.com/simfeo/MobilePhotoViewer"

[Setup]
AppId={{7E2C4B1A-9D3F-4E5A-8C6B-1F2A3B4C5D6E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputBaseFilename=MobilePhotoViewer-{#MyAppVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
WizardStyle=modern
DisableProgramGroupPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Recursively pack the entire deployed folder (exe + Qt DLLs + plugins + heif/libde265).
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
