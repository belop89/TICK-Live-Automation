; installer.nsi
; NSIS script to install TICK (VST3 and optional Standalone)
Name "TICK"
OutFile "TICK-Installer.exe"
InstallDir "$PROGRAMFILES\TICK"
RequestExecutionLevel admin

!define PRODUCT_NAME "TICK"

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Install TICK" SecInstall
    ; Install VST3 bundle (expects dist\TICK.vst3\* staged)
    SetOutPath "$PROGRAMFILES\Common Files\VST3\TICK.vst3"
    ; create folder even if VST3 is a bundle
    CreateDirectory "$PROGRAMFILES\Common Files\VST3\TICK.vst3"
    File /r "dist\TICK.vst3\*"

    ; Install Standalone if present (dist\Standalone\*)
    IfFileExists "dist\Standalone\TICK.exe" 0 +2
    CreateDirectory "$PROGRAMFILES\TICK"
    SetOutPath "$PROGRAMFILES\TICK"
    File /r "dist\Standalone\*"

    ; Start Menu shortcut
    CreateDirectory "$SMPROGRAMS\TICK"
    ; If standalone exe exists, create shortcut
    IfFileExists "$PROGRAMFILES\TICK\TICK.exe" 0 +3
    CreateShortCut "$SMPROGRAMS\TICK\TICK.lnk" "$PROGRAMFILES\TICK\TICK.exe" "" "$PROGRAMFILES\TICK\TICK.exe" 0

    ; Write uninstall helper and registry entry for Add/Remove Programs
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "Publisher" "YourNameOrCompany"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayVersion" "1.0"
SectionEnd

Section "Uninstall"
    ; remove installed files
    ; remove Start Menu shortcuts
    Delete "$SMPROGRAMS\TICK\TICK.lnk"
    RMDir "$SMPROGRAMS\TICK"

    ; remove Standalone
    RMDir /r "$PROGRAMFILES\TICK"

    ; remove VST3 bundle
    RMDir /r "$PROGRAMFILES\Common Files\VST3\TICK.vst3"

    ; remove uninstall exe and registry entry
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
SectionEnd