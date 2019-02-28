;--------------------------------
;Include Modern UI

!include "MUI2.nsh"
!include "x64.nsh"

;--------------------------------
;General

!define Version 1.1.0
Name "Piano From Above"
OutFile "PFA-${Version}-setup.exe"
BrandingText "PFA ${Version}"
Var TargetFile ;Set in onInit

RequestExecutionLevel admin
SetCompressor /SOLID /FINAL lzma

;Default installation folder
InstallDir "$LOCALAPPDATA\Piano From Above"

;Get installation folder from registry if available
InstallDirRegKey HKCU "Software\Piano From Above" ""

Function .onInit
    ${If} ${RunningX64}
        StrCpy $TargetFile "PFA-${Version}-x86_64.exe"
    ${Else}
        StrCpy $TargetFile "PFA-${Version}-x86.exe"
    ${EndIf}
    SetShellVarContext current
    Call uninstall
FunctionEnd

Function uninstall
    ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "UninstallString"
    StrCmp $R0 "" Done
 
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "Piano From Above needs to be unstalled first.$\n$\nClick OK to remove the previous version. Your settings will be saved." IDCANCEL Cancel
    ExecShell open "$R0"

    Cancel:
    Abort
    Done:
FunctionEnd

;--------------------------------
;Interface Configuration

!define MUI_ICON "PianoFromAbove\Images\PFA Icon.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "PianoFromAbove\Images\Install Header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "PianoFromAbove\Images\Mirror Wizard.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\$TargetFile"
!define MUI_ABORTWARNING

;--------------------------------
;Pages

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
  
;--------------------------------
;Languages
 
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Piano From Above (required)" SecProgramFile
    SectionIn RO

    SetOutPath "$INSTDIR"

    ${If} ${RunningX64}
        SetRegView 64
        File "Release\PFA-1.1.0-x86_64.exe"
    ${Else}
        File "Release\PFA-1.1.0-x86.exe"
    ${EndIf}
    File "Docs\Readme.txt"
    File "Docs\Credits.txt"

    ;Store installation folder
    WriteRegStr HKCU "Software\Piano From Above" "" $INSTDIR

    ;Add/Remove programns
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "DisplayName" "Piano From Above"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "DisplayVersion" "1.1.0"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "DisplayIcon" "$INSTDIR\$TargetFile"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "Publisher" "Brian Pantano"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "EstimatedSize" 1122
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above" "NoRepair" 1

    ;Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "DirectX 9 Update" SecDirectX
    SetOutPath "$INSTDIR"

    File "Redist\DXSETUP.exe"
    File "Redist\DSETUP.dll"
    File "Redist\dsetup32.dll"
    File "Redist\dxdllreg_x86.cab"
    File "Redist\dxupdate.cab"
    File "Redist\Jun2010_d3dx9_43_x86.cab"
    File "Redist\Jun2010_d3dx9_43_x64.cab"

    ExecWait  '"$INSTDIR\DXSETUP.exe" /silent'

    Delete "$INSTDIR\DXSETUP.exe"
    Delete "$INSTDIR\DSETUP.dll"
    Delete "$INSTDIR\dsetup32.dll"
    Delete "$INSTDIR\dxdllreg_x86.cab"
    Delete "$INSTDIR\dxupdate.cab"
    Delete "$INSTDIR\Jun2010_d3dx9_43_x86.cab"
    Delete "$INSTDIR\Jun2010_d3dx9_43_x64.cab"
SectionEnd

Section "Start Menu Shortcuts" SecStartMenu
    CreateDirectory "$SMPROGRAMS\Piano From Above"
    CreateShortCut "$SMPROGRAMS\Piano From Above\Piano From Above.lnk" "$INSTDIR\$TargetFile" "" "$INSTDIR\$TargetFile" 0
    CreateShortCut "$SMPROGRAMS\Piano From Above\View Readme.lnk" "$INSTDIR\Readme.txt"
    CreateShortCut "$SMPROGRAMS\Piano From Above\View Credits.lnk" "$INSTDIR\Credits.txt"
    CreateShortCut "$SMPROGRAMS\Piano From Above\Uninstall.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0
SectionEnd

Section "Desktop Shortcut" SecDesktop
    CreateShortCut "$DESKTOP\Piano From Above.lnk" "$INSTDIR\$TargetFile" "" "$INSTDIR\$TargetFile" 0
SectionEnd

Section "Piano MIDI Files" SecMusicFiles
    CreateDirectory "$MUSIC\Piano From Above"
    SetOutPath "$MUSIC\Piano From Above"
    File "Docs\Credits.txt"

    CreateDirectory "$MUSIC\Piano From Above\1 Intermediate 1"
    SetOutPath "$MUSIC\Piano From Above\1 Intermediate 1"
    File "Music\1 Intermediate 1\Bach - Minuet in B-flat Major.mid"
    File "Music\1 Intermediate 1\Bach - Minuet in F Major.mid"
    File "Music\1 Intermediate 1\Bach - Minuet in G Major.mid"
    File "Music\1 Intermediate 1\Bach - Minuet in G Minor.mid"
    File "Music\1 Intermediate 1\Bach - Musette in D Major.mid"
    File "Music\1 Intermediate 1\Chopin - Prelude in C Minor.mid"
    File "Music\1 Intermediate 1\Clementie - Sonatina Op. 36, No. 1 1st Movement.mid"
    File "Music\1 Intermediate 1\Schumann - First Sorrow.mid"
    File "Music\1 Intermediate 1\Schumann - Humming Song.mid"
    File "Music\1 Intermediate 1\Schumann - Melody.mid"
    File "Music\1 Intermediate 1\Schumann - Sicilienne.mid"
    File "Music\1 Intermediate 1\Schumann - Soldiers' March.mid"
    File "Music\1 Intermediate 1\Schumann - The Happy Farmer.mid"

    CreateDirectory "$MUSIC\Piano From Above\2 Intermediate 2"
    SetOutPath "$MUSIC\Piano From Above\2 Intermediate 2"
    File "Music\2 Intermediate 2\Bach - Invention 01.mid"
    File "Music\2 Intermediate 2\Bach - Invention 04.mid"
    File "Music\2 Intermediate 2\Bach - Invention 09.mid"
    File "Music\2 Intermediate 2\Bach - Invention 15.mid"
    File "Music\2 Intermediate 2\Beethoven - Fur Elise.mid"
    File "Music\2 Intermediate 2\Beethoven - Moonlight Sonata 1st Movement.mid"
    File "Music\2 Intermediate 2\Beethoven - Pathetique 2nd Movement.mid"
    File "Music\2 Intermediate 2\Chopin - Prelude in A Major.mid"
    File "Music\2 Intermediate 2\Chopin - Prelude in E Minor.mid"
    File "Music\2 Intermediate 2\Mozart - Sonata Facile 1st Movement.mid"
    File "Music\2 Intermediate 2\Mozart - Turkish March.mid"
    File "Music\2 Intermediate 2\Schumann - Dreaming.mid"
    File "Music\2 Intermediate 2\Schumann - Farmhand Rupert.mid"
    File "Music\2 Intermediate 2\Schumann - Hunting Song.mid"
    File "Music\2 Intermediate 2\Schumann - The Wild Horseman.mid"

    CreateDirectory "$MUSIC\Piano From Above\3 Advanced"
    SetOutPath "$MUSIC\Piano From Above\3 Advanced"
    File "Music\3 Advanced\Bach - Prelude and Fugue in C Major.mid"
    File "Music\3 Advanced\Bach - Prelude and Fugue in C Minor.mid"
    File "Music\3 Advanced\Chopin - Black Key Etude.mid"
    File "Music\3 Advanced\Chopin - Etude Op. 10, No. 1.mid"
    File "Music\3 Advanced\Chopin - Fantaisie-Impromptu.mid"
    File "Music\3 Advanced\Chopin - Funeral March.mid"
    File "Music\3 Advanced\Chopin - Revolutionary Etude.mid"
    File "Music\3 Advanced\Debussy - Clair de Lune.mid"
    File "Music\3 Advanced\Godowsky - Study of Chopin Etude Op. 10, No. 1.mid"
    File "Music\3 Advanced\Grieg - March of the Trolls.mid"
    File "Music\3 Advanced\Joplin - Maple Leaf Rag.mid"
    File "Music\3 Advanced\Joplin - The Entertainer.mid"
    File "Music\3 Advanced\Liszt - Hungarian Rhapsody No. 2.mid"
    File "Music\3 Advanced\Liszt - Love Dreams No. 3.mid"
SectionEnd

;--------------------------------
;Descriptions

;Language strings
LangString DESC_SecProgramFile ${LANG_ENGLISH} "Installs essential files needed to run Piano From Above."
LangString DESC_SecDirectX ${LANG_ENGLISH} "Updates DirectX to the June 2010 release. If you have a newer version, you will not be downgraded."
LangString DESC_SecStartMenu ${LANG_ENGLISH} "Adds links to your start menu for easy access."
LangString DESC_SecDesktop ${LANG_ENGLISH} "Adds a link to your desktop for easy access."
LangString DESC_SecMusicFiles ${LANG_ENGLISH} "Adds a selection of piano midi files to your Music folder."

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecProgramFile} $(DESC_SecProgramFile)
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDirectX} $(DESC_SecDirectX)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} $(DESC_SecStartMenu)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_SecDesktop)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMusicFiles} $(DESC_SecMusicFiles)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
    ${If} ${RunningX64}
        SetRegView 64
    ${EndIf}
    SetShellVarContext current

    ; Remove registry keys
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Piano From Above"
    DeleteRegKey HKCU "Software\Piano From Above"

    ;Remove files
    Delete "$INSTDIR\*.*"
    Delete "$SMPROGRAMS\Piano From Above\*.*"
    Delete "$DESKTOP\Piano From Above.lnk"

    Delete "$MUSIC\Piano From Above\Credits.*"
    
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Bach - Minuet in B-flat Major.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Bach - Minuet in F Major.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Bach - Minuet in G Major.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Bach - Minuet in G Minor.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Bach - Musette in D Major.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Chopin - Prelude in C Minor.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Clementie - Sonatina Op. 36, No. 1 1st Movement.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - First Sorrow.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - Humming Song.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - Melody.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - Sicilienne.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - Soldiers' March.mid"
    Delete "$MUSIC\Piano From Above\1 Intermediate 1\Schumann - The Happy Farmer.mid"

    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Bach - Invention 01.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Bach - Invention 04.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Bach - Invention 09.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Bach - Invention 15.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Beethoven - Fur Elise.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Beethoven - Moonlight Sonata 1st Movement.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Beethoven - Pathetique 2nd Movement.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Chopin - Prelude in A Major.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Chopin - Prelude in E Minor.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Mozart - Sonata Facile 1st Movement.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Mozart - Turkish March.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Schumann - Dreaming.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Schumann - Farmhand Rupert.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Schumann - Hunting Song.mid"
    Delete "$MUSIC\Piano From Above\2 Intermediate 2\Schumann - The Wild Horseman.mid"

    Delete "$MUSIC\Piano From Above\3 Advanced\Bach - Prelude and Fugue in C Major.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Bach - Prelude and Fugue in C Minor.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Chopin - Black Key Etude.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Chopin - Etude Op. 10, No. 1.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Chopin - Fantaisie-Impromptu.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Chopin - Funeral March.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Chopin - Revolutionary Etude.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Debussy - Clair de Lune.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Godowsky - Study of Chopin Etude Op. 10, No. 1.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Grieg - March of the Trolls.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Joplin - Maple Leaf Rag.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Joplin - The Entertainer.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Liszt - Hungarian Rhapsody No. 2.mid"
    Delete "$MUSIC\Piano From Above\3 Advanced\Liszt - Love Dreams No. 3.mid"

    ; Remove directories
    RMDir "$INSTDIR"
    RMDir "$SMPROGRAMS\Piano From Above"
    RMDir "$MUSIC\Piano From Above\1 Intermediate 1"
    RMDir "$MUSIC\Piano From Above\2 Intermediate 2"
    RMDir "$MUSIC\Piano From Above\3 Advanced"
    RMDir "$MUSIC\Piano From Above"
SectionEnd