; Inno Setup script for the MC-2 Windows installer.
;
; Built by CI (see .github/workflows/build.yml) against the freshly built
; VST3 and Standalone binaries in build\MC2_artefacts\Release. Run manually
; with e.g.:
;   ISCC.exe /DMyAppVersion=1.0.0 installer\windows\MC2.iss
; (from the repository root) after a Release build; MyAppVersion falls back
; to 1.0.0 if not supplied.

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#define MyAppName "MC-2 Mastering Compressor"
#define MyAppPublisher "JTEKK Audio"
#define MyAppExeName "MC-2 Mastering Compressor.exe"
#define MyAppVst3Name "MC-2 Mastering Compressor.vst3"
#define BuildDir "..\..\build\MC2_artefacts\Release"

[Setup]
AppId={{A3F1D9E2-6B4E-4F1A-9C3D-7E2B8F5A1C40}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=MC2-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
LicenseFile=..\..\LICENSE
UninstallDisplayIcon={app}\{#MyAppExeName}
PrivilegesRequired=admin

[Types]
Name: "full"; Description: "Full installation (VST3 + Standalone)"
Name: "vst3only"; Description: "VST3 plug-in only"
Name: "standaloneonly"; Description: "Standalone application only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full vst3only custom
Name: "standalone"; Description: "Standalone application"; Types: full standaloneonly custom

[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildDir}\VST3\{#MyAppVst3Name}\*"; DestDir: "{commoncf64}\VST3\{#MyAppVst3Name}"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
Source: "{#BuildDir}\Standalone\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: standalone
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent; Components: standalone
