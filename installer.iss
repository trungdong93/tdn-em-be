[Setup]
AppName=TDN Em Be
AppVersion=1.0.0
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=TDN-Em-Be-Setup

[Files]
Source: "stage\TDN Em Be.vst3\*"; DestDir: "{commoncf64}\VST3\TDN Em Be.vst3"; Flags: recursesubdirs ignoreversion
