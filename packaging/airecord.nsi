; AireRecord NSIS installer - Modern UI 2, admin (registers the SCM service).
!include "MUI2.nsh"
!include "x64.nsh"

!define APP_NAME    "AireRecord"
!define APP_VERSION "0.9.0"
!define APP_PUBLISHER "AireRecord"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AireRecord"

; All paths relative to the repo root. Prefer the absolute ROOT passed on the
; makensis command line (/DROOT=...); fall back to the .nsi's parent dir.
!ifndef ROOT
  !define ROOT "${__FILEDIR__}\.."
!endif
!define SRC_REL   "${ROOT}\build\Release"
!define SRC_SKIA  "${ROOT}\build\ci-all\airui-skia-windows-x64"
!define ICON_PATH "${ROOT}\res\app.ico"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${ROOT}\build\installer\AireRecord-${APP_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\AireRecord"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; Installer window icon (best-effort; skip if the file is absent).
!if /FileExists "${ICON_PATH}"
  !define MUI_ICON   "${ICON_PATH}"
  !define MUI_UNICON "${ICON_PATH}"
!endif

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Section "Install" SecMain
  SetOutPath "$INSTDIR"

  ; Kill any running AireRecord processes first: airecord.exe / airecordd.exe
  ; are locked while running, so the File copy below would leave the OLD UI /
  ; service in place and Windows keeps using them.
  nsExec::ExecToLog 'taskkill /f /im airecord.exe'
  nsExec::ExecToLog 'taskkill /f /im airecordd.exe'
  Sleep 300

  File "${SRC_REL}\airecord.exe"
  File "${SRC_REL}\airecordd.exe"
  ; Skia plugin bundle (if present; else app falls back to GDI at runtime).
  !if /FileExists "${SRC_SKIA}\airui-skia.dll"
    SetOutPath "$INSTDIR"
    File /nonfatal /r "${SRC_SKIA}\*.dll"
  !endif
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\airecord.exe" "" "$INSTDIR\airecord.exe" 0
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\airecord.exe" "" "$INSTDIR\airecord.exe" 0
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME} Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  nsExec::ExecToLog '"$INSTDIR\airecordd.exe" -install'
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${APP_NAME} ${APP_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; Kill any running UI / service process so the locked files can be removed.
  nsExec::ExecToLog 'taskkill /f /im airecord.exe'
  nsExec::ExecToLog 'taskkill /f /im airecordd.exe'
  Sleep 300
  nsExec::ExecToLog '"$INSTDIR\airecordd.exe" -uninstall'
  Delete "$INSTDIR\airecord.exe"
  Delete "$INSTDIR\airecordd.exe"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\*.dll"
  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME} Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
