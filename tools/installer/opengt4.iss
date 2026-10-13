; OpenGT4 Windows installer (Inno Setup 6).
; Build the release folder first with tools/package_release.py, then:
;   ISCC /DVersion=0.2.0-alpha tools\installer\opengt4.iss
; The installer holds only what that folder holds: no game data.

#ifndef Version
  #error Pass /DVersion=<version>
#endif
#define Source "..\..\dist\OpenGT4-" + Version + "-win64"

[Setup]
AppId={{6B0E5C2A-4F1D-4B8E-9A47-0D3C5E2F7A91}
AppName=OpenGT4
AppVersion={#Version}
AppVerName=OpenGT4 {#Version}
AppPublisher=OpenGT4 contributors
AppComments=Unofficial native PC port of Gran Turismo 4. Requires your own game disc image.
DefaultDirName={autopf}\OpenGT4
DefaultGroupName=OpenGT4
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile={#Source}\LICENSE.txt
InfoBeforeFile={#Source}\NOTICE.txt
OutputDir=..\..\dist
OutputBaseFilename=OpenGT4-{#Version}-setup
UninstallDisplayIcon={app}\opengt4.exe
SetupIconFile=..\..\res\opengt4.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Source}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\OpenGT4"; Filename: "{app}\opengt4.exe"
Name: "{autoprograms}\OpenGT4 README"; Filename: "{app}\README.txt"
Name: "{autodesktop}\OpenGT4"; Filename: "{app}\opengt4.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\opengt4.exe"; Description: "{cm:LaunchProgram,OpenGT4}"; Flags: nowait postinstall skipifsilent
