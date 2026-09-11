; Pencil Dream Windows 安装包（Inno Setup 6）
; CI 调用：ISCC.exe pencil-dream.iss /DVersion=x.y.z.w /DPkgSuffix=xxx
; 相对路径基于本脚本所在目录 .github/packaging/

#ifndef Version
#define Version "0.0.0.0"
#endif
#ifndef PkgSuffix
#define PkgSuffix "dev"
#endif

#define MyAppName "Pencil Dream"
#define MyAppExeName "pencil2d.exe"
#define MyAppURL "https://github.com/1220874621zp-debug/pencil-dream"

[Setup]
AppId={{A3B0D5F2-6E84-4C71-9B2A-5D18F0E7C9A3}
AppName={#MyAppName}
AppVersion={#Version}
AppPublisher=Pencil Dream Project
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\Pencil Dream
DefaultGroupName=Pencil Dream
DisableProgramGroupPage=yes
UninstallDisplayName={#MyAppName}
OutputDir=..\..\dist
OutputBaseFilename=pencil-dream-setup-win64-{#PkgSuffix}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
SetupIconFile=..\..\app\data\pencil2d.ico
LicenseFile=..\..\LICENSE.TXT
MinVersion=10.0

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\dist\PencilDream\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
