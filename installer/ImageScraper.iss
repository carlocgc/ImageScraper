#define AppName "ImageScraper"
#ifndef AppVersion
#define AppVersion "0.0.0"
#endif
#ifndef SourceDir
#define SourceDir "..\bin\ImageScraper\x64\Release"
#endif
#ifndef OutputDir
#define OutputDir "..\dist"
#endif
#ifndef RootDir
#define RootDir ".."
#endif

[Setup]
AppId={{2F6A1DB7-BC78-4C80-8C91-3F5082C80D9D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=ImageScraper
AppPublisherURL=https://github.com/carlocgc/ImageScraper
AppSupportURL=https://github.com/carlocgc/ImageScraper/issues
AppUpdatesURL=https://github.com/carlocgc/ImageScraper/releases
DefaultDirName={localappdata}\Programs\ImageScraper
DefaultGroupName=ImageScraper
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=ImageScraper-v{#AppVersion}-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
UninstallDisplayIcon={app}\ImageScraper.exe
VersionInfoVersion={#AppVersion}
VersionInfoCompany=ImageScraper
VersionInfoDescription=ImageScraper installer
VersionInfoProductName=ImageScraper
VersionInfoProductVersion={#AppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\ImageScraper.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.crt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\auth.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\THIRD_PARTY_LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RootDir}\README.MD"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\ImageScraper"; Filename: "{app}\ImageScraper.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\ImageScraper"; Filename: "{app}\ImageScraper.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ImageScraper.exe"; Description: "{cm:LaunchProgram,ImageScraper}"; Flags: nowait postinstall skipifsilent
