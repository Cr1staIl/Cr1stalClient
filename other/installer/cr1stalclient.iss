; Windows installer for Cr♱stal Client (Inno Setup 6)
; Built by .github/workflows/release.yml:
;   ISCC.exe /DAppVersion=1.0.0 /DSourceDir=<unpacked win64 package> other\installer\cr1stalclient.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must point to the unpacked Windows package
#endif

#define AppName "Cr♱stal Client"
#define AppExe "DDNet.exe"

[Setup]
AppId={{BE3336F3-2C8D-4B2E-90B6-1DDB7A32DAA8}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Cr1staIl
AppPublisherURL=https://github.com/Cr1staIl/Cr1stalClient
DefaultDirName={autopf}\Cr1stalClient
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; installs for the current user without an admin prompt, admins may choose "all users"
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\installer-out
OutputBaseFilename=Cr1stalClient-Setup
SetupIconFile=..\icons\DDNet.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
