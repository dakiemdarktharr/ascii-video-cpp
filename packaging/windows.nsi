Unicode True
!include "MUI2.nsh"
!include "x64.nsh"
Name "ASCII Video C++"
OutFile "${OUTPUT}\Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\ASCII Video C++"
InstallDirRegKey HKCU "Software\ASCII Video C++" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "ASCII Video C++"
VIAddVersionKey "FileDescription" "ASCII Video C++ installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "KoiSee and contributors"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\ascii-video-cpp.exe"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This installer requires 64-bit Windows."
    Abort
  ${EndIf}
  SetShellVarContext current
  SetRegView 64
FunctionEnd

Section "Application"
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "${PAYLOAD}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\ASCII Video C++"
  CreateShortcut "$SMPROGRAMS\ASCII Video C++\ASCII Video C++.lnk" "$INSTDIR\ascii-video-cpp.exe"
  CreateShortcut "$SMPROGRAMS\ASCII Video C++\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\ASCII Video C++" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "DisplayName" "ASCII Video C++"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "Publisher" "KoiSee"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "DisplayIcon" "$INSTDIR\ascii-video-cpp.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  SetRegView 64
  !include "${OUTPUT}\uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\ASCII Video C++\ASCII Video C++.lnk"
  Delete "$SMPROGRAMS\ASCII Video C++\Uninstall.lnk"
  RMDir "$SMPROGRAMS\ASCII Video C++"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ASCII Video C++"
  DeleteRegKey HKCU "Software\ASCII Video C++"
SectionEnd