; ─────────────────────────────────────────────────────────────────────────────
;  Steinbach Chanel Strip – Windows NSIS Installer
;
;  Aufruf (aus Projektroot):
;    makensis /DVERSION=1.0.1 installer\SteinbachChanelStrip.nsi
;
;  Voraussetzung: installer\stage\Steinbach Chanel Strip.vst3\ enthält den
;  fertigen VST3-Bundle (wird im CI durch den Workflow befüllt).
; ─────────────────────────────────────────────────────────────────────────────
!ifndef VERSION
  !define VERSION "1.0.1"
!endif

!define PRODUCT_NAME "Steinbach Chanel Strip"
!define PRODUCT_PUB  "Hauke Steinbach"
!define VST3_NAME    "Steinbach Chanel Strip.vst3"
!define VST3_TARGET  "$COMMONFILES64\VST3\${VST3_NAME}"
!define REG_KEY      "Software\Microsoft\Windows\CurrentVersion\Uninstall\SteinbachChanelStrip"

Name            "${PRODUCT_NAME} ${VERSION}"
OutFile         "SteinbachChanelStrip-${VERSION}-Setup.exe"
InstallDir      "$COMMONFILES64\VST3"
RequestExecutionLevel admin
SetCompressor   /SOLID lzma
Unicode         True

!include "MUI2.nsh"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install ──────────────────────────────────────────────────────────────────
Section "VST3 Plugin" SecVST3
  ; VST3-Bundle in den Standard-VST3-Ordner installieren
  SetOutPath "${VST3_TARGET}"
  File /r "installer\stage\${VST3_NAME}"

  ; Deinstallations-Eintrag anlegen
  WriteUninstaller "$INSTDIR\Uninstall_SteinbachChanelStrip.exe"

  WriteRegStr HKLM "${REG_KEY}" "DisplayName"     "${PRODUCT_NAME}"
  WriteRegStr HKLM "${REG_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr HKLM "${REG_KEY}" "Publisher"       "${PRODUCT_PUB}"
  WriteRegStr HKLM "${REG_KEY}" "UninstallString" '"$INSTDIR\Uninstall_SteinbachChanelStrip.exe"'
SectionEnd

; ── Uninstall ────────────────────────────────────────────────────────────────
Section "Uninstall"
  RMDir /r "${VST3_TARGET}"
  Delete    "$INSTDIR\Uninstall_SteinbachChanelStrip.exe"
  DeleteRegKey HKLM "${REG_KEY}"
SectionEnd
