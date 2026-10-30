!include "MUI2.nsh"
!include "FileFunc.nsh"

; The name of the installer
Name "StudWorx"

; The file to write
;OutFile "studworx-setup.exe"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir $PROGRAMFILES64\StudWorx

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\StudWorx" "InstallFolder"
  
;  Icon "setup.ico"

;  !define MUI_ICON "setup.ico"
;  !define MUI_UNICON "setup.ico"

SetCompressor /SOLID lzma


;Interface Settings

!define MUI_HEADERIMAGE
!define MUI_ABORTWARNING


;Pages

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\StudWorx.exe"
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
  
!insertmacro MUI_LANGUAGE "English"


;Installer Sections

Section "Application Files" SecStudWorx

  SectionIn RO
  SetShellVarContext all

  ;Remove the previous version, its uninstaller keeps the user settings
  IfFileExists "$INSTDIR\Uninstall.exe" 0 PastPreviousCheck
    ExecWait '"$INSTDIR\Uninstall.exe" /S /KEEPSETTINGS _?=$INSTDIR'
    Delete "$INSTDIR\Uninstall.exe"
  PastPreviousCheck:

  SetOutPath "$INSTDIR"

  File /r "${DIRECTORY_PACKAGE}\*.*"

  ;Register file extension
  WriteRegStr HKCR ".lcd" "" "StudWorx.Project"
  WriteRegStr HKCR "StudWorx.Project" "" "StudWorx Project"
  WriteRegStr HKCR "StudWorx.Project\DefaultIcon" "" "$INSTDIR\StudWorx.exe,0"
  WriteRegStr HKCR "StudWorx.Project\shell" "" "open"
  WriteRegStr HKCR "StudWorx.Project\shell\open\command" "" '"$INSTDIR\StudWorx.exe" "%1"'
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'

  IfFileExists "$INSTDIR\vc_redist.x64.exe" VcRedist64Exists PastVcRedist64Check
  VcRedist64Exists:
    ExecWait '"$INSTDIR\vc_redist.x64.exe"  /quiet /norestart'
  PastVcRedist64Check:

  ;Store installation folder
  WriteRegStr HKLM "Software\StudWorx" "InstallFolder" $INSTDIR
  
  CreateShortCut "$SMPROGRAMS\StudWorx.lnk" "$INSTDIR\StudWorx.exe"

  ;Create uninstaller
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "DisplayName" "StudWorx"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "DisplayVersion" $%VERSION%
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "Publisher" "StudWorx.org"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "DisplayIcon" '"$INSTDIR\StudWorx.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "URLUpdateInfo" "https://github.com/nathaneltitane/studworx"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "URLInfoAbout" "https://github.com/nathaneltitane/studworx"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx" "EstimatedSize" "$0"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecStudWorx ${LANG_ENGLISH} "Application Files (required)"

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStudWorx} $(DESC_SecStudWorx)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  SetShellVarContext all

  Delete "$SMPROGRAMS\StudWorx.lnk"

  ;Every file and folder the installer placed, listed at build time
  !include "uninstall.nsh"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKCR ".lcd"
  DeleteRegKey HKCR "StudWorx.Project"
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorx"
  DeleteRegKey HKLM "Software\StudWorx"

  ;Settings, parts cache and temporary files, kept when a new version replaces this one
  ${un.GetParameters} $R0
  ClearErrors
  ${un.GetOptions} $R0 "/KEEPSETTINGS" $R1
  IfErrors 0 PastSettings
    SetShellVarContext current
    DeleteRegKey HKCU "Software\StudWorx"
    RMDir /r "$LOCALAPPDATA\StudWorx"
    RMDir /r "$APPDATA\StudWorx"
    Delete "$TEMP\studworx-*"
  PastSettings:

SectionEnd
