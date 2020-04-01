/*************************************************************************************************
*
* File: ConfigProcs.cpp
*
* Description: Implements configuration GUI functions. Mostly window procs. Not C++ :/
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <TChar.h>
#include <shlobj.h>
#include <Uxtheme.h>
#include <Vsstyle.h>
#include <Dbt.h>

#include "ConfigProcs.h"
#include "MainProcs.h"
#include "Globals.h"
#include "resource.h"

#include "GameState.h"

VOID DoPreferences( HWND hWndOwner )
{
    int pDialogs[] = { IDD_PP1_VISUAL, IDD_PP2_AUDIO, IDD_PP4_CONTROLS };
    DLGPROC pProcs[] = { VisualProc, AudioProc, ControlsProc };
    LPCWSTR pTitles[] = { TEXT( "Visual" ), TEXT( "Audio" ), TEXT( "Controls" ) };
    PROPSHEETPAGE psp[3];
    PROPSHEETHEADER psh;

    for ( int i = 0; i < sizeof( psp ) / sizeof( PROPSHEETPAGE ); i++ )
    {
        psp[i].dwSize = sizeof( PROPSHEETPAGE );
        psp[i].dwFlags = PSP_USETITLE;
        psp[i].hInstance = g_hInstance;
        psp[i].pszTemplate = MAKEINTRESOURCE( pDialogs[i] );
        psp[i].pszIcon = NULL;
        psp[i].pfnDlgProc = pProcs[i];
        psp[i].pszTitle = pTitles[i];
        psp[i].lParam = 0;
        psp[i].pfnCallback = NULL;
    }
    psh.dwSize = sizeof( PROPSHEETHEADER );
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOCONTEXTHELP;
    psh.hwndParent = hWndOwner;
    psh.hInstance = g_hInstance;
    psh.pszIcon = NULL;
    psh.pszCaption = TEXT( "Preferences" );
    psh.nPages = sizeof( psp ) / sizeof( PROPSHEETPAGE );
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;
    psh.pfnCallback = NULL;

    INT_PTR result = PropertySheet(&psh);
}

VOID Changed( HWND hWnd )
{
    SendMessage( GetParent( hWnd ), PSM_CHANGED, ( WPARAM )hWnd, 0 );
}

INT_PTR WINAPI VisualProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            HWND hWndFirstKey = GetDlgItem( hWnd, IDC_FIRSTKEY );
            HWND hWndLastKey = GetDlgItem( hWnd, IDC_LASTKEY );

            // Enumerate the keys
            for ( int i = MIDI::A0; i <= MIDI::C8; i++ )
            {
                SendMessage( hWndFirstKey, CB_ADDSTRING, i, ( LPARAM )MIDI::NoteName(i).c_str() );
                SendMessage( hWndLastKey, CB_ADDSTRING, i, ( LPARAM )MIDI::NoteName(i).c_str() );
            }

            // Config to fill out the form
            Config &config = Config::GetConfig();
            SetVisualProc( hWnd, config.GetVisualSettings() );
            return TRUE;
        }
        // Draws the colored buttons
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            if ( ( pdis->CtlID < IDC_COLOR1 || pdis->CtlID > IDC_COLOR6 ) && pdis->CtlID != IDC_BKGCOLOR )
                return FALSE;

            SetDCBrushColor( pdis->hDC, (COLORREF)GetWindowLongPtr( pdis->hwndItem, GWLP_USERDATA ) );
            FillRect( pdis->hDC, &pdis->rcItem, ( HBRUSH )GetStockObject( DC_BRUSH ) );
            return TRUE;
        }
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            Changed( hWnd );
            switch ( iId )
            {
                case IDC_SHOWCUSTOMKEYS:
                    EnableWindow( GetDlgItem( hWnd, IDC_FIRSTKEY ), TRUE );
                    EnableWindow( GetDlgItem( hWnd, IDC_THROUGH ), TRUE );
                    EnableWindow( GetDlgItem( hWnd, IDC_LASTKEY ), TRUE );
                    return TRUE;
                case IDC_SHOWALLKEYS: case IDC_SHOWSONGKEYS:
                    EnableWindow( GetDlgItem( hWnd, IDC_FIRSTKEY ), FALSE );
                    EnableWindow( GetDlgItem( hWnd, IDC_THROUGH ), FALSE );
                    EnableWindow( GetDlgItem( hWnd, IDC_LASTKEY ), FALSE );
                    return TRUE;
                // Color buttons. Pop up color choose dialog and set color.
                case IDC_COLOR1: case IDC_COLOR2: case IDC_COLOR3:
                case IDC_COLOR4: case IDC_COLOR5: case IDC_COLOR6: 
                case IDC_BKGCOLOR:
                {
                    static COLORREF acrCustClr[16] = { 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
                                                       0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF }; 
                    HWND hWndBtn = ( HWND )lParam;

                    // Choose and save the color
                    CHOOSECOLOR cc = { 0 };
                    cc.lStructSize = sizeof( cc );
                    cc.hwndOwner = hWnd;
                    cc.lpCustColors = (LPDWORD) acrCustClr;
                    cc.rgbResult = (COLORREF)GetWindowLongPtr( hWndBtn, GWLP_USERDATA );
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if ( !ChooseColor( &cc ) ) return TRUE;

                    // Draw the button (indirect)
                    SetWindowLongPtr( hWndBtn, GWLP_USERDATA, cc.rgbResult );
                    InvalidateRect( hWndBtn, NULL, FALSE );
                    return TRUE;
                }
                case IDC_RESTOREDEFAULTS:
                {
                    VisualSettings cVisualSettings;
                    cVisualSettings.LoadDefaultValues();

                    SendMessage( hWnd, WM_SETREDRAW, FALSE, 0 );
                    SetVisualProc( hWnd, cVisualSettings  );
                    SendMessage( hWnd, WM_SETREDRAW, TRUE, 0 );
                    InvalidateRect( hWnd, NULL, FALSE );
                    return TRUE;
                }
            }
            break;
        }
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            switch (lpnmhdr->code)
            {
                // OK or Apply button pressed
                case PSN_APPLY:
                {
                    // Get a copy of the config to overwrite the settings
                    Config &config = Config::GetConfig();
                    VisualSettings cVisual = config.GetVisualSettings();
                    ViewSettings &cView = config.GetViewSettings();

                    // VisualSettings struct
                    bool bAlwaysShowControls = cVisual.bAlwaysShowControls;
                    cVisual.eKeysShown = ( IsDlgButtonChecked( hWnd, IDC_SHOWALLKEYS ) == BST_CHECKED ? cVisual.All : 
                                           IsDlgButtonChecked( hWnd, IDC_SHOWSONGKEYS ) == BST_CHECKED ? cVisual.Song :
                                           IsDlgButtonChecked( hWnd, IDC_SHOWCUSTOMKEYS ) == BST_CHECKED ? cVisual.Custom :
                                           cVisual.Song );
                    cVisual.bAlwaysShowControls = ( IsDlgButtonChecked( hWnd, IDC_SHOWCONTROLS ) == BST_CHECKED );
                    cVisual.bAssociateFiles = ( IsDlgButtonChecked( hWnd, IDC_ASSOCIATEFILES ) == BST_CHECKED );
                    cVisual.iFirstKey = (int)SendMessage( GetDlgItem( hWnd, IDC_FIRSTKEY ), CB_GETCURSEL, 0, 0 ) + MIDI::A0;
                    cVisual.iLastKey = (int)SendMessage( GetDlgItem( hWnd, IDC_LASTKEY ), CB_GETCURSEL, 0, 0 ) + MIDI::A0;
                    for ( int i = 0; i < IDC_COLOR6 - IDC_COLOR1 + 1; i++ )
                        cVisual.colors[i] = (int)GetWindowLongPtr( GetDlgItem( hWnd, IDC_COLOR1 + i ), GWLP_USERDATA );
                    cVisual.iBkgColor = (int)GetWindowLongPtr( GetDlgItem( hWnd, IDC_BKGCOLOR ), GWLP_USERDATA );

                    // Report success and return
                    config.SetVisualSettings( cVisual );
                    if ( cView.GetFullScreen() && bAlwaysShowControls != cVisual.bAlwaysShowControls )
                        cView.SetControls( cView.GetControls(), true );
                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_NOERROR );
                    return TRUE;
                }
            }
            break;
        }
    }

    return FALSE;
}

// Sets the values in the playback settings dialog. Used at init and restoring defaults
VOID SetVisualProc( HWND hWnd, const VisualSettings &cVisual )
{
    HWND hWndFirstKey = GetDlgItem( hWnd, IDC_FIRSTKEY );
    HWND hWndLastKey = GetDlgItem( hWnd, IDC_LASTKEY );

    // Set values
    CheckRadioButton( hWnd, IDC_SHOWALLKEYS, IDC_SHOWCUSTOMKEYS, IDC_SHOWALLKEYS + cVisual.eKeysShown );
    CheckDlgButton( hWnd, IDC_SHOWCONTROLS, cVisual.bAlwaysShowControls ? BST_CHECKED : BST_UNCHECKED );
    CheckDlgButton( hWnd, IDC_ASSOCIATEFILES, cVisual.bAssociateFiles ? BST_CHECKED : BST_UNCHECKED );
    SendMessage( hWnd, WM_COMMAND, IDC_SHOWALLKEYS + cVisual.eKeysShown, 0 );
    SendMessage( hWndFirstKey, CB_SETCURSEL, cVisual.iFirstKey - MIDI::A0, 0 );
    SendMessage( hWndLastKey, CB_SETCURSEL, cVisual.iLastKey - MIDI::A0, 0 );

    // Colors
    for ( int i = 0; i < IDC_COLOR6 - IDC_COLOR1 + 1; i++ )
        SetWindowLongPtr( GetDlgItem( hWnd, IDC_COLOR1 + i ), GWLP_USERDATA, cVisual.colors[i] );
    SetWindowLongPtr( GetDlgItem( hWnd, IDC_BKGCOLOR ), GWLP_USERDATA, cVisual.iBkgColor );
}

INT_PTR WINAPI AudioProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_INITDIALOG:
            SetAudioProc( hWnd, Config::GetConfig().GetAudioSettings() );
            return TRUE;
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            int iCode = HIWORD( wParam );
            if ( iCode == LBN_SELCHANGE ) 
                Changed( hWnd );
            break;
        }
        case WM_DEVICECHANGE:
            Sleep( 200 );
            SetAudioProc( hWnd, Config::GetConfig().GetAudioSettings() );
            break;
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            switch (lpnmhdr->code)
            {
                // OK or Apply button pressed
                case PSN_APPLY:
                {
                    // Get a copy of the config to overwrite the settings
                    Config &config = Config::GetConfig();
                    AudioSettings cAudio = config.GetAudioSettings();

                    // Get the values
                    cAudio.iOutDevice = (int)SendDlgItemMessage( hWnd, IDC_MIDIOUT, LB_GETCURSEL, 0, 0 );
                    if ( cAudio.iOutDevice >= 0 ) cAudio.sDesiredOut = cAudio.vMIDIOutDevices[cAudio.iOutDevice];

                    // Set the values
                    bool bChanged = ( cAudio.iOutDevice != config.GetAudioSettings().iOutDevice );
                    config.SetAudioSettings( cAudio );

                    if ( bChanged )
                        HandOffMsg( WM_DEVICECHANGE, 0, 0 );

                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_NOERROR );
                    return TRUE;
                }
            }
            break;
        }
    }
    return FALSE;
}

// Sets the values in the playback settings dialog. Used at init and restoring defaults
VOID SetAudioProc( HWND hWnd, const AudioSettings &cAudio )
{
    Config::GetConfig().LoadMIDIDevices();

    HWND hWndOutDevs = GetDlgItem( hWnd, IDC_MIDIOUT );
    while( SendMessage( hWndOutDevs, LB_DELETESTRING, 0, 0 ) > 0  );
    for ( vector< wstring >::const_iterator it = cAudio.vMIDIOutDevices.begin(); it != cAudio.vMIDIOutDevices.end(); ++it )
        SendMessage( hWndOutDevs, LB_ADDSTRING, 0, ( LPARAM )( it->c_str() ) );
    SendMessage( hWndOutDevs, LB_SETCURSEL, cAudio.iOutDevice, 0 );
}

INT_PTR WINAPI VideoProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            // Config to fill out the form
            Config &config = Config::GetConfig();
            const VideoSettings &cVideo = config.GetVideoSettings();

            CheckRadioButton( hWnd, IDC_DIRECT3D, IDC_GDI, IDC_DIRECT3D + cVideo.eRenderer );
            CheckDlgButton( hWnd, IDC_DISPLAYFPS, cVideo.bShowFPS ? BST_CHECKED : BST_UNCHECKED );
            CheckDlgButton( hWnd, IDC_LIMITFPS, cVideo.bLimitFPS ? BST_CHECKED : BST_UNCHECKED );

            return TRUE;
        }
        case WM_COMMAND:
            Changed( hWnd );
            break;
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            switch (lpnmhdr->code)
            {
                // OK or Apply button pressed
                case PSN_APPLY:
                {
                    // Get a copy of the config to overwrite the settings
                    Config &config = Config::GetConfig();
                    VideoSettings cVideo = config.GetVideoSettings();

                    cVideo.eRenderer = ( IsDlgButtonChecked( hWnd, IDC_DIRECT3D ) == BST_CHECKED ? cVideo.Direct3D : 
                                         IsDlgButtonChecked( hWnd, IDC_OPENGL ) == BST_CHECKED ? cVideo.OpenGL :
                                         IsDlgButtonChecked( hWnd, IDC_GDI ) == BST_CHECKED ? cVideo.GDI :
                                         cVideo.Direct3D );
                    cVideo.bShowFPS = ( IsDlgButtonChecked( hWnd, IDC_DISPLAYFPS ) == BST_CHECKED );
                    cVideo.bLimitFPS = ( IsDlgButtonChecked( hWnd, IDC_LIMITFPS ) == BST_CHECKED );

                    config.SetVideoSettings( cVideo );
                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_NOERROR );
                    return TRUE;
                }
            }
            break;
        }
    }
    return FALSE;
}

INT_PTR WINAPI ControlsProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            // Config to fill out the form
            Config &config = Config::GetConfig();
            const ControlsSettings &cControls = config.GetControlsSettings();

            HWND hWndFwdBack = GetDlgItem( hWnd, IDC_LRARROWS );
            HWND hWndSpeedPct = GetDlgItem( hWnd, IDC_UDARROWS );

            // Edit boxes
            TCHAR buf[32];
            int len = _stprintf_s( buf, TEXT( "%g" ), cControls.dFwdBackSecs );
            SetWindowText( hWndFwdBack, buf );
            _stprintf_s( buf, TEXT( "%g" ), cControls.dSpeedUpPct );
            SetWindowText( hWndSpeedPct, buf );

            return TRUE;
        }
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            switch (lpnmhdr->code)
            {
                // Spin boxes. Just increment or decrement
                case UDN_DELTAPOS:
                    switch ( lpnmhdr->idFrom )
                    {
                        case IDC_LRARROWSSPIN:
                        {
                            TCHAR buf[32];
                            LPNMUPDOWN lpnmud  = ( LPNMUPDOWN )lParam;
                            HWND hWndFwdBack = GetDlgItem( hWnd, IDC_LRARROWS );
                            double dOldVal = 0;
                            int len = GetWindowText( hWndFwdBack, buf, 32 );
                            if ( len > 0 && _stscanf_s( buf, TEXT( "%lf" ), &dOldVal ) == 1 )
                            {
                                double dNewVal = dOldVal - lpnmud->iDelta * .1;
                                _stprintf_s( buf, TEXT( "%g" ), dNewVal );
                                SetWindowText( hWndFwdBack, buf );
                            }
                            return TRUE;
                        }
                        case IDC_UDARROWSSPIN:
                        {
                            TCHAR buf[32];
                            LPNMUPDOWN lpnmud  = ( LPNMUPDOWN )lParam;
                            HWND hWndSpeedPct = GetDlgItem( hWnd, IDC_UDARROWS );
                            double dOldVal = 0;
                            int len = GetWindowText( hWndSpeedPct, buf, 32 );
                            if ( len > 0 && _stscanf_s( buf, TEXT( "%lf" ), &dOldVal ) == 1 )
                            {
                                double dNewVal = dOldVal - lpnmud->iDelta;
                                _stprintf_s( buf, TEXT( "%g" ), dNewVal );
                                SetWindowText( hWndSpeedPct, buf );
                            }
                            return TRUE;
                        }
                    }
                    break;
                // OK or Apply button pressed
                case PSN_APPLY:
                {
                    // Get a copy of the config to overwrite the settings
                    Config &config = Config::GetConfig();
                    ControlsSettings cControls = config.GetControlsSettings();

                    // Edit boxes
                    TCHAR buf[32];
                    double dEditVal = 0;
                    
                    HWND hWndFwdBack = GetDlgItem( hWnd, IDC_LRARROWS );
                    int len = GetWindowText( hWndFwdBack, buf, 32 );
                    if ( len > 0 && _stscanf_s( buf, TEXT( "%lf" ), &dEditVal ) == 1 )
                        cControls.dFwdBackSecs = dEditVal;
                    else
                    {
                        MessageBox( hWnd, TEXT( "Please specify a numeric value for the left and right arrows" ), TEXT( "Error" ), MB_OK | MB_ICONEXCLAMATION );
                        PostMessage( hWnd, WM_NEXTDLGCTL, ( WPARAM )hWndFwdBack, TRUE);
                        SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_INVALID );
                        return TRUE;
                    }
                    
                    HWND hWndSpeedPct = GetDlgItem( hWnd, IDC_UDARROWS );
                    len = GetWindowText( hWndSpeedPct, buf, 32 );
                    if ( len > 0 && _stscanf_s( buf, TEXT( "%lf" ), &dEditVal ) == 1 )
                        cControls.dSpeedUpPct = dEditVal;
                    else
                    {
                        MessageBox( hWnd, TEXT( "Please specify a numeric value for the up and down arrows" ), TEXT( "Error" ), MB_OK | MB_ICONEXCLAMATION );
                        PostMessage( hWnd, WM_NEXTDLGCTL, ( WPARAM )hWndSpeedPct, TRUE);
                        SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_INVALID );
                        return TRUE;
                    }
                    
                    // Report success and return
                    config.SetControlsSettings( cControls );
                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, PSNRET_NOERROR );
                    return TRUE;
                }
            }
            break;
        }
    }

    return FALSE;
}

BOOL ToggleYN( HWND hWndListview, int iItem )
{
    if ( iItem < 0 ) return FALSE;

    LVITEM lvi;
    TCHAR YN[2];
    lvi.iSubItem = 1;
    lvi.pszText = YN;
    lvi.cchTextMax = 2;
    if ( SendMessage( hWndListview, LVM_GETITEMTEXT, iItem, ( LPARAM )&lvi )  <= 0 ) return FALSE;

    if ( YN[0] == TEXT( 'Y' ) ) lvi.pszText = TEXT( "No" );
    else if ( YN[0] == TEXT( 'N' ) ) lvi.pszText = TEXT( "Yes" );
    else return TRUE;

    SendMessage( hWndListview, LVM_SETITEMTEXT, iItem, ( LPARAM )&lvi );
    return TRUE;
}

BOOL GetCustomSettings( MainScreen *pGameState )
{
    INT_PTR iDlgResult = DialogBoxParam( g_hInstance, MAKEINTRESOURCE( IDD_TRACKSETTINGS ),
                                         g_hWnd, TracksProc, ( LPARAM )pGameState );
    return iDlgResult == IDOK;
}

INT_PTR WINAPI TracksProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    static vector< bool > vMuted, vHidden; // Would rather be part of control, but no subitem lparam available
    static vector< unsigned > vColors;

	switch ( msg )
	{
	    case WM_INITDIALOG:
        {
            HWND hWndTracks = GetDlgItem( hWnd, IDC_TRACKS );
            TCHAR buf[MAX_PATH];
            SendMessage( hWndTracks, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_DOUBLEBUFFER );

            // Get data
            MainScreen *pGameState = ( MainScreen* )lParam;
            const MIDI &midi = pGameState->GetMIDI();
            const MIDI::MIDIInfo &mInfo = midi.GetInfo();
            const vector< MIDITrack* > &vTracks = midi.GetTracks();
            SetWindowLongPtr( hWnd, GWLP_USERDATA, ( LONG_PTR )pGameState );

            // Fill out the static text vals
            SetWindowText( GetDlgItem( hWnd, IDC_FILE ), mInfo.sFilename.c_str() + mInfo.sFilename.find_last_of( L'\\' ) + 1 );
            SetWindowText( GetDlgItem( hWnd, IDC_FOLDER ), mInfo.sFilename.substr( 0, mInfo.sFilename.find_last_of( L'\\' ) ).c_str() );
            _stprintf_s( buf, TEXT( "%d" ), mInfo.iNoteCount );
            SetWindowText( GetDlgItem( hWnd, IDC_NOTES ), buf );
            _stprintf_s( buf, TEXT( "%lld:%02.0lf" ), mInfo.llTotalMicroSecs / 60000000,
                                                    ( mInfo.llTotalMicroSecs % 60000000 ) / 1000000.0 );
            SetWindowText( GetDlgItem( hWnd, IDC_LENGTH ), buf );

            // Initialize the state vars
            vMuted.resize( mInfo.iNumChannels );
            vHidden.resize( mInfo.iNumChannels );
            vColors.resize( mInfo.iNumChannels );
            int iMax = sizeof( cVisual.colors ) / sizeof ( cVisual.colors[0] );
            for ( int i = 0; i < mInfo.iNumChannels; i++ )
            {
                vMuted[i] = vHidden[i] = false;
                if ( i < iMax ) vColors[i] = cVisual.colors[i];
                else vColors[i] = Util::RandColor();
            }
            
            // Set up the columns of the list view
            RECT rcTracks;
            GetClientRect( hWndTracks, &rcTracks );
            int aFmt[6] = { LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_CENTER, LVCFMT_CENTER, LVCFMT_CENTER };
            int aCx[6] = { 27, rcTracks.right - 27 - 55 - 50 - 50 - 50, 55, 50, 50, 50 };
            TCHAR *aText[6] = { TEXT( "Trk" ), TEXT( "Instrument" ), TEXT( "Notes" ), TEXT( "Muted" ), TEXT( "Hidden" ), TEXT( "Color" ) };

            LVCOLUMN lvc = { 0 };
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
            for ( int i = 0; i < sizeof( aFmt ) / sizeof( int ); i++ )
            {
                lvc.fmt = aFmt[i];
                lvc.cx = aCx[i];
                lvc.pszText = aText[i];
                SendMessage( hWndTracks, LVM_INSERTCOLUMN, i, ( LPARAM )&lvc );
            }

            // Set rows of the list view
            LVITEM lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.pszText = buf;

            int iPos = 0;
            for ( int i = 0; i < mInfo.iNumTracks; i++ )
            {
                const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
                for ( int j = 0; j < 16; j++ )
                    if ( mTrackInfo.aNoteCount[j] > 0 )
                    {
                        lvi.iSubItem = 0;
                        _stprintf_s( buf, TEXT( "%d" ), iPos + 1 );
                        lvi.iItem = (int)SendMessage( hWndTracks, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

                        lvi.iSubItem++;
                        _stprintf_s( buf, TEXT( "%s" ), j == 9 ? TEXT( "Drums" ) : MIDI::Instruments[mTrackInfo.aProgram[j]].c_str() );
                        SendMessage( hWndTracks, LVM_SETITEM, 0, ( LPARAM )&lvi );

                        lvi.iSubItem++;
                        _stprintf_s( buf, TEXT( "%d" ), mTrackInfo.aNoteCount[j] );
                        SendMessage( hWndTracks, LVM_SETITEM, 0, ( LPARAM )&lvi );

                        lvi.iItem++;
                        iPos++;
                    }
            }

            if ( GetWindowLongPtr( hWndTracks, GWL_STYLE ) & WS_VSCROLL )
                SendMessage( hWndTracks, LVM_SETCOLUMNWIDTH, 1, aCx[1] - 17 );

            RECT rcItem = { LVIR_BOUNDS };
            SendMessage( hWndTracks, LVM_GETITEMRECT, 0, ( LPARAM )&rcItem );
            int iItemHeight = rcItem.bottom - rcItem.top;
            int iBmpSize = iItemHeight - 2;

            return TRUE;
        }
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            if ( lpnmhdr->idFrom == IDC_TRACKS)
            {
                switch ( lpnmhdr->code )
                {
                    // Prevent's item selection
                    case LVN_ITEMCHANGING:
                    {
                        LPNMLISTVIEW pnmv = ( LPNMLISTVIEW )lParam;
                        if ( ( pnmv->uChanged & LVIF_STATE ) &&
                             ( pnmv->uNewState & LVIS_SELECTED ) != ( pnmv->uOldState & LVIS_SELECTED ) )
                        {
                            SetWindowLongPtr( hWnd, DWLP_MSGRESULT, TRUE );
                            return TRUE;
                        }
                        break;
                    }
                    // Turn all on or all off or reset colors
                    case LVN_COLUMNCLICK:
                    {
                        LPNMLISTVIEW lpnmlv = ( LPNMLISTVIEW )lParam;
                        bool bAllChecked = true;
                        size_t iMax = sizeof( cVisual.colors ) / sizeof ( cVisual.colors[0] );
                        switch ( lpnmlv->iSubItem )
                        {
                            case 3:
                                for ( size_t i = 0; i < vMuted.size(); i++ ) bAllChecked &= vMuted[i];
                                for ( size_t i = 0; i < vMuted.size(); i++ ) vMuted[i] = !bAllChecked;
                                InvalidateRect( lpnmlv->hdr.hwndFrom, NULL, FALSE );
                                return TRUE;
                            case 4:
                                for ( size_t i = 0; i < vHidden.size(); i++ ) bAllChecked &= vHidden[i];
                                for ( size_t i = 0; i < vHidden.size(); i++ ) vHidden[i] = !bAllChecked;
                                InvalidateRect( lpnmlv->hdr.hwndFrom, NULL, FALSE );
                                return TRUE;
                            case 5:
                                for ( size_t i = 0; i < vColors.size(); i++ )
                                    if ( i < iMax ) vColors[i] = cVisual.colors[i];
                                    else vColors[i] = Util::RandColor();
                                InvalidateRect( lpnmlv->hdr.hwndFrom, NULL, FALSE );
                                return TRUE;
                        }
                        return TRUE;
                    }
                    // Toggle checkboxes or select color
                    case NM_CLICK:
                    case NM_DBLCLK:
                    {
                        // Have to manually figure out the corresponding item. Silly.
                        LPNMITEMACTIVATE lpnmia = ( LPNMITEMACTIVATE )lpnmhdr;
                        LVHITTESTINFO lvhti = { lpnmia->ptAction };
                        SendMessage( lpnmia->hdr.hwndFrom, LVM_SUBITEMHITTEST, 0, ( LPARAM )&lvhti );
                        if ( lvhti.iItem < 0 || lvhti.iItem >= ( int )vMuted.size() ) return FALSE;

                        RECT rcItem = { LVIR_BOUNDS };
                        SendMessage( lpnmia->hdr.hwndFrom, LVM_GETITEMRECT, lvhti.iItem, ( LPARAM )&rcItem );
                        switch ( lvhti.iSubItem )
                        {
                            case 3:
                                vMuted[ lvhti.iItem ] = !vMuted[ lvhti.iItem ];
                                InvalidateRect( lpnmia->hdr.hwndFrom, &rcItem, FALSE );
                                return TRUE;
                            case 4:
                                vHidden[ lvhti.iItem ] = !vHidden[ lvhti.iItem ];
                                InvalidateRect( lpnmia->hdr.hwndFrom, &rcItem, FALSE );
                                return TRUE;
                            case 5:
                            {
                                static COLORREF acrCustClr[16] = { 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 
                                                                   0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF }; 
                                CHOOSECOLOR cc = { 0 };
                                cc.lStructSize = sizeof( cc );
                                cc.hwndOwner = hWnd;
                                cc.lpCustColors = (LPDWORD) acrCustClr;
                                cc.rgbResult = vColors[ lvhti.iItem ];
                                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                                if ( !ChooseColor( &cc ) ) return TRUE;
                                vColors[ lvhti.iItem ] = cc.rgbResult;
                                InvalidateRect( lpnmia->hdr.hwndFrom, &rcItem, FALSE );
                                return TRUE;
                            }
                        }
                        break;
                    }
                    // Draw the checkboxes and colors
                    case NM_CUSTOMDRAW:
                    {
                        LPNMCUSTOMDRAW lpnmcd = ( LPNMCUSTOMDRAW )lParam;
                        LPNMLVCUSTOMDRAW lpnmlvcd = ( LPNMLVCUSTOMDRAW )lParam;
                        switch ( lpnmcd->dwDrawStage )
                        {
                            case CDDS_PREPAINT: case CDDS_ITEMPREPAINT:
                                SetWindowLongPtr( hWnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW );
                                return TRUE;
                            case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
                                if ( lpnmlvcd->iSubItem >= 3 && lpnmlvcd->iSubItem <= 5 )
                                {
                                    // Figure out size. Too big a rect is fine: will be clipped
                                    RECT rcOut;
                                    int iBmpSize = lpnmcd->rc.bottom - lpnmcd->rc.top - 2;
                                    rcOut.left = lpnmcd->rc.left + ( lpnmcd->rc.right - lpnmcd->rc.left - iBmpSize ) / 2;
                                    rcOut.top = lpnmcd->rc.top + ( lpnmcd->rc.bottom - lpnmcd->rc.top - iBmpSize ) / 2;
                                    rcOut.right = rcOut.left + iBmpSize;
                                    rcOut.bottom = rcOut.top + iBmpSize;

                                    // Drawing. Checkbox, checkbox, color
                                    if ( lpnmlvcd->iSubItem == 3 )
                                        DrawFrameControl( lpnmcd->hdc, &rcOut, DFC_BUTTON,
                                            DFCS_BUTTONCHECK | ( vMuted[ lpnmcd->dwItemSpec ] ? DFCS_CHECKED : 0 ) );
                                    else if ( lpnmlvcd->iSubItem == 4 )
                                        DrawFrameControl( lpnmcd->hdc, &rcOut, DFC_BUTTON,
                                            DFCS_BUTTONCHECK | ( vHidden[ lpnmcd->dwItemSpec ] ? DFCS_CHECKED : 0 ) );
                                    else
                                    {
                                        SetDCBrushColor( lpnmcd->hdc, lpnmlvcd->clrFace );
                                        FillRect( lpnmcd->hdc, &lpnmcd->rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
                                        InflateRect( &lpnmcd->rc, -1, -1 );
                                        SetDCBrushColor( lpnmcd->hdc, vColors[ lpnmcd->dwItemSpec ] );
                                        FillRect( lpnmcd->hdc, &lpnmcd->rc, ( HBRUSH )GetStockObject( DC_BRUSH ) );
                                        DrawEdge( lpnmcd->hdc, &lpnmcd->rc, BDR_SUNKENINNER, BF_RECT );
                                    }
                                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, CDRF_SKIPDEFAULT );
                                }
                                else
                                    SetWindowLongPtr( hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT );
                                return TRUE;
                        }
                        break;
                    }
               }
            }
            break;
        }
	    case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch ( iId )
            {
                case IDOK:
                {
                    MainScreen *pGameState = ( MainScreen* )GetWindowLongPtr( hWnd, GWLP_USERDATA );
                    pGameState->SetChannelSettings( vMuted, vHidden, vColors );

                    wchar_t szCapacity[80];
                    if (GetDlgItemText(hWnd, IDC_VQCAPACITY, szCapacity, 80)) {
                        if (swscanf(szCapacity, L"%llu", &vq_capacity_proc_res) != 1) {
                            vq_capacity_proc_res = 0; // just in case
                        }
                    }

                    Config::GetConfig().m_bManualTimer = IsDlgButtonChecked(hWnd, IDC_CHECK1);
                    Config::GetConfig().m_bUltraTurboModeXtreme = IsDlgButtonChecked(hWnd, IDC_CHECK2);
                }
                case IDCANCEL:
			        EndDialog( hWnd, iId );
			        return TRUE;
            }
		    break;
        }
	}

	return FALSE;
}

// still using a global variable for this
size_t vq_capacity_proc_res = 0;