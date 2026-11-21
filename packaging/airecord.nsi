; AireRecord NSIS installer - Modern UI 2, admin (registers the SCM service).
; Architecture-aware: ships both the x86 build (GDI-only, no Skia plugin) and the
; x64 build + x64 Skia plugin, and installs whichever matches the running OS.
!include "MUI2.nsh"
!include "x64.nsh"

!define APP_NAME    "AireRecord"
; APP_VERSION is injected by packaging\build-installer.ps1, parsed from the
; single source of truth: project(AireRecord VERSION x.y.z) at the top of
; CMakeLists.txt (the same declaration that becomes AIRUI_VERSION in
; cmake/Version.h.in). The fallback below only serves a direct
; `makensis packaging\airecord.nsi` invocation without /DAPP_VERSION.
!ifndef APP_VERSION
  !define APP_VERSION "0.9.0"
!endif
!define APP_PUBLISHER "AireRecord"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AireRecord"

; All paths relative to the repo root. Prefer the absolute ROOT passed on the
; makensis command line (/DROOT=...); fall back to the .nsi's parent dir.
!ifndef ROOT
  !define ROOT "${__FILEDIR__}\.."
!endif
!define SRC_X86     "${ROOT}\build\Release"                     ; x86 app (GDI-only)
!define SRC_X64     "${ROOT}\build-x64\Release"                 ; x64 app
!define SRC_SKIA    "${ROOT}\build\ci-all\airui-skia-windows-x64" ; x64 Skia plugin bundle
!define ICON_PATH "${ROOT}\res\app.ico"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "${ROOT}\build\installer\AireRecord-${APP_VERSION}-setup.exe"
; Default target dir; overridden per-arch in .onInit based on RunningX64.
; We resolve $INSTDIR explicitly here and again in .onInit because NSIS installers
; are x86 by default and $PROGRAMFILES resolves to the 32-bit dir on x86 only.
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

; Choose the install dir up front so both the install and the uninstaller use
; the same per-arch Program Files location ($PROGRAMFILES64 on x64, $PROGRAMFILES32
; on x86). NSIS resolves the *_PROGRAMFILES constants to the real dirs regardless
; of the (x86) installer's own bitness.
Function .onInit
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\${APP_NAME}"
  ${Else}
    StrCpy $INSTDIR "$PROGRAMFILES32\${APP_NAME}"
  ${EndIf}
FunctionEnd

; Best-effort stop: stop the SCM service first, then force-kill any UI /
; service process. The RUNNING service holds airecordd.exe (and the watchdog
; re-launches airecord.exe), so copying over it either leaves stale files or
; errors. nsExec::ExecToLog never aborts the install if a process isn't
; running or the service is already stopped, so these are safe to run
; unconditionally. A longer Sleep lets Windows release the file handles.
!macro StopAireRecord
  nsExec::ExecToLog 'net stop AireRecord'
  nsExec::ExecToLog 'taskkill /f /im airecord.exe'
  nsExec::ExecToLog 'taskkill /f /im airecordd.exe'
  Sleep 500
!macroend

Section "Install" SecMain
  SetOutPath "$INSTDIR"

  ; Stop the service + kill the UI BEFORE copying files: airecord.exe and
  ; airecordd.exe are locked while running, and the service watchdog
  ; re-launches the UI, so without this the File copy leaves the OLD binaries
  ; in place (or fails) and a stale service can keep spawning duplicate UIs.
  !insertmacro StopAireRecord

  ${If} ${RunningX64}
    ; x64 build. The x64 Skia plugin is only shipped when the build script
    ; proved the bundle is a fresh ABI-v2 plugin (exports airui_skia_describe).
    ; A stale ABI-v1 bundle (exports airui_skia_abi, built before P0-3) would
    ; be loaded but rejected by the app -> silently falling back to GDI and
    ; misleadingly reporting the plugin as present. We omit it so the installed
    ; app is honest GDI until a fresh CI bundle is published. See
    ; packaging\build-installer.ps1 (it sets /DSHIP_SKIA=1 only for a fresh bundle).
    File "${SRC_X64}\airecord.exe"
    File "${SRC_X64}\airecordd.exe"
    !ifdef SHIP_SKIA
      SetOutPath "$INSTDIR"
      File /nonfatal /r "${SRC_SKIA}\*.dll"
      SetOutPath "$INSTDIR"
    !endif
  ${Else}
    ; x86 build. No x86 Skia plugin is published -> GDI rendering only.
    File "${SRC_X86}\airecord.exe"
    File "${SRC_X86}\airecordd.exe"
  ${EndIf}

  SetOutPath "$INSTDIR"
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
  ; Stop the service + kill any running UI / service process so the locked
  ; files can be removed (the service watchdog otherwise re-launches the UI
  ; while we are deleting its binaries). Best-effort like install.
  !insertmacro StopAireRecord
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
