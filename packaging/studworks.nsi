!include "MUI2.nsh"
!include "FileFunc.nsh"

; The name of the installer
Name "StudWorks"

; The file to write
;OutFile "studworks-setup.exe"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

; Build Unicode installer
Unicode True

; The default installation directory
InstallDir $PROGRAMFILES64\StudWorks

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\StudWorks" "InstallFolder"
  
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
!define MUI_FINISHPAGE_RUN "$INSTDIR\StudWorks.exe"
!insertmacro MUI_PAGE_FINISH
  
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
  
!insertmacro MUI_LANGUAGE "English"


;Installer Sections

Section "Application Files" SecStudWorks

  SectionIn RO
  SetShellVarContext all

  ;Remove the previous version, its uninstaller keeps the user settings
  IfFileExists "$INSTDIR\Uninstall.exe" 0 PastPreviousCheck
    ExecWait '"$INSTDIR\Uninstall.exe" /S /KEEPSETTINGS _?=$INSTDIR'
    Delete "$INSTDIR\Uninstall.exe"
  PastPreviousCheck:

  SetOutPath "$INSTDIR"

  File /r /x library.bin "${DIRECTORY_PACKAGE}\*.*"

  ;Register file extension
  WriteRegStr HKCR ".lcd" "" "StudWorks.Project"
  WriteRegStr HKCR "StudWorks.Project" "" "StudWorks Project"
  WriteRegStr HKCR "StudWorks.Project\DefaultIcon" "" "$INSTDIR\StudWorks.exe,0"
  WriteRegStr HKCR "StudWorks.Project\shell" "" "open"
  WriteRegStr HKCR "StudWorks.Project\shell\open\command" "" '"$INSTDIR\StudWorks.exe" "%1"'
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'

  IfFileExists "$INSTDIR\vc_redist.x64.exe" VcRedist64Exists PastVcRedist64Check
  VcRedist64Exists:
    ExecWait '"$INSTDIR\vc_redist.x64.exe"  /quiet /norestart'
  PastVcRedist64Check:

  ;Store installation folder
  WriteRegStr HKLM "Software\StudWorks" "InstallFolder" $INSTDIR
  
  CreateShortCut "$SMPROGRAMS\StudWorks.lnk" "$INSTDIR\StudWorks.exe"

  ;Create uninstaller
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "DisplayName" "StudWorks"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "DisplayVersion" $%VERSION%
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "Publisher" "StudWorks.org"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "DisplayIcon" '"$INSTDIR\StudWorks.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "URLUpdateInfo" "https://github.com/nathaneltitane/studworks"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "URLInfoAbout" "https://github.com/nathaneltitane/studworks"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "NoRepair" 1

  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks" "EstimatedSize" "$0"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

Section "Parts Library" SecLibrary

  SetOutPath "$INSTDIR"

  File "${DIRECTORY_PACKAGE}\library.bin"

SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecStudWorks ${LANG_ENGLISH} "Application Files (required)"
  LangString DESC_SecLibrary ${LANG_ENGLISH} "Library of parts that represent those produced by the LEGO® company and created by the LDraw community"

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStudWorks} $(DESC_SecStudWorks)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecLibrary} $(DESC_SecLibrary)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  SetShellVarContext all

  Delete "$SMPROGRAMS\StudWorks.lnk"

  ;Every file and folder the installer placed, listed at build time
  !include "uninstall.nsh"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKCR ".lcd"
  DeleteRegKey HKCR "StudWorks.Project"
  System::Call 'shell32.dll::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)'

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\StudWorks"
  DeleteRegKey HKLM "Software\StudWorks"

  ;Settings, parts cache and temporary files, kept when a new version replaces this one
  ${un.GetParameters} $R0
  ClearErrors
  ${un.GetOptions} $R0 "/KEEPSETTINGS" $R1
  IfErrors 0 PastSettings
    SetShellVarContext current
    DeleteRegKey HKCU "Software\StudWorks"
    RMDir /r "$LOCALAPPDATA\StudWorks"
    RMDir /r "$APPDATA\StudWorks"
    Delete "$TEMP\studworks-*"
  PastSettings:

SectionEnd
