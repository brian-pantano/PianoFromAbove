/*************************************************************************************************
*
* File: MainProcs.cpp
*
* Description: Implements the main GUI functions. Mostly just window procs.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <TChar.h>
#include <shlobj.h>
#include <Dbt.h>

#include <set>

#include "MainProcs.h"
#include "ConfigProcs.h"
#include "Globals.h"
#include "resource.h"

#include "GameState.h"
#include "Config.h"

static WNDPROC g_pPrevBarProc; // Have to override the toolbar proc to make controls transparent

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: The window's message handler
//-----------------------------------------------------------------------------
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();
    static ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const ControlsSettings &cControls = Config::GetConfig().GetControlsSettings();
    static SongLibrary &cLibrary = Config::GetConfig().GetSongLibrary();
    static bool bInSizeMove = false;

    switch( msg )
    {
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            int iCode = HIWORD( wParam );
            switch ( iId )
            {
                case IDOK:
                {
                    HWND hWndLib = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
                    if ( GetFocus() == hWndLib )
                        PlayLibrary( hWndLib, (int)SendMessage( hWndLib, LVM_GETNEXTITEM, -1, LVNI_SELECTED ), GameState::Practice );
                    return 0;
                }
                case ID_FILE_LEARNSONG: case ID_FILE_PRACTICESONG: case ID_FILE_PRACTICESONGCUSTOM: case ID_FILE_PLAYSONG:
                {
                    CheckActivity( TRUE );
                    GameState::State ePlayMode = ( iId == ID_FILE_PRACTICESONG || iId == ID_FILE_PRACTICESONGCUSTOM ? GameState::Practice :
                                                   iId == ID_FILE_PLAYSONG ? GameState::Play : GameState::Learn );
                    // Get the file(s) to add
                    OPENFILENAME ofn = { 0 };
                    TCHAR sFilename[1024] = { 0 };
                    ofn.lStructSize = sizeof( OPENFILENAME );
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = TEXT( "MIDI Files\0*.mid\0" );
                    ofn.lpstrFile = sFilename;
                    ofn.nMaxFile = sizeof( sFilename ) / sizeof( TCHAR );
                    ofn.lpstrTitle = TEXT( "Please select a song to play" );
                    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if ( GetOpenFileName( &ofn ) )
                        PlayFile( sFilename, ePlayMode, iId == ID_FILE_PRACTICESONGCUSTOM, true );
                    return 0;
                }
                case ID_FILE_CLOSEFILE:
                {
                    if ( !cPlayback.GetPlayMode() ) break;
                    HWND hWndLib = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
                    cPlayback.SetPlayMode( GameState::Intro, true );
                    cPlayback.SetPlayable( SendMessage( hWndLib, LVM_GETNEXTITEM, -1, LVNI_SELECTED ) >= 0, true );
                    cPlayback.SetPosition( 0 );
                    SetWindowText( g_hWnd, TEXT( APPNAME ) );
                    HandOffMsg( WM_COMMAND, ID_CHANGESTATE, ( LPARAM )new IntroScreen( NULL, NULL ) );
                    return 0;
                }
                case ID_FILE_ADDFILE:
                {
                    CheckActivity( TRUE );

                    // Get the file(s) to add
                    OPENFILENAME ofn = { 0 };
                    TCHAR sFilename[1024] = { 0 };
                    ofn.lStructSize = sizeof( OPENFILENAME );
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = TEXT( "MIDI Files\0*.mid\0" );
                    ofn.lpstrFile = sFilename;
                    ofn.nMaxFile = sizeof( sFilename ) / sizeof( TCHAR );
                    ofn.lpstrTitle = TEXT( "Please select a song to add to the library" );
                    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if ( !GetOpenFileName( &ofn ) ) return 0;

                    HWND hWndLib = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
                    static SongLibrary &cLibrary = Config::GetConfig().GetSongLibrary();
                    int iChanged = 0;

                    // Add multiple files
                    if ( ofn.nFileOffset > 0 && ofn.lpstrFile[ofn.nFileOffset - 1] == '\0' )
                    {
                        ofn.lpstrFile[ofn.nFileOffset - 1] = '\\';
                        TCHAR *pFilename = ofn.lpstrFile + ofn.nFileOffset;
                        TCHAR *pNextFile = pFilename + _tcslen( pFilename ) + 1;
                        while ( *pFilename )
                        {
                            iChanged += cLibrary.AddSource( sFilename, SongLibrary::File, true );
                            size_t len = _tcslen( pNextFile );
                            memmove( pFilename, pNextFile, ( len + 1 ) * sizeof( TCHAR ) ); // memmove because buffers overlap
                            pNextFile += len + 1;
                        }
                    }
                    // Add a single file
                    else
                        iChanged += cLibrary.AddSource( sFilename, SongLibrary::File, true );

                    if ( iChanged ) PopulateLibrary( hWndLib );
                    return 0;
                }
                case ID_FILE_ADDFOLDER:
                {
                    CheckActivity( TRUE );

                    // Set up the data structure for the shell common dialog
                    TCHAR sFolder[MAX_PATH];
                    LPITEMIDLIST pidl = NULL;
                    BROWSEINFO bi = { 0 };
                    bi.hwndOwner = hWnd;
                    bi.pszDisplayName = sFolder;
                    bi.pidlRoot = NULL;
                    bi.lpszTitle = TEXT( "Please select a folder to add to the library" );
                    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

                    // Get the folder. COM bleh
                    if ( ( pidl = SHBrowseForFolder( &bi ) ) == NULL ) return 0;
                    BOOL bResult = SHGetPathFromIDList( pidl, sFolder );
                    CoTaskMemFree(pidl);

                    // Add the folder to the library
                    HWND hWndLib = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
                    static SongLibrary &cLibrary = Config::GetConfig().GetSongLibrary();
                    int iChanged = 0;
                    if ( bResult ) iChanged = cLibrary.AddSource( sFolder, SongLibrary::Folder, true );
                    if ( iChanged ) PopulateLibrary( hWndLib );

                    return 0;
                }
                case ID_FILE_REFRESH:
                {
                    map< wstring, SongLibrary::Source > mSources = cLibrary.GetSources(); // Uggglllly
                    for ( map< wstring, SongLibrary::Source >::const_iterator it = mSources.begin(); it != mSources.end(); ++it )
                        cLibrary.RemoveSource( it->first );
                    for ( map< wstring, SongLibrary::Source >::const_iterator it = mSources.begin(); it != mSources.end(); ++it )
                        cLibrary.AddSource( it->first, it->second );
                    PopulateLibrary( GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES ) );
                    return 0;
                }
                case ID_LEARN_DEFAULT:
                case ID_PRACTICE_DEFAULT:
                case ID_PRACTICE_CUSTOM:
                case ID_PLAY_DEFAULT:
                case ID_PLAY_PLAY:
                    if ( cPlayback.GetPlayMode() && iId == ID_PLAY_PLAY )
                        cPlayback.SetPaused( false, true );
                    else
                    {
                        HWND hWndLib = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
                        GameState::State ePlayMode = ( iId == ID_PLAY_DEFAULT ? GameState::Play :
                                                       iId == ID_LEARN_DEFAULT ? GameState::Learn :
                                                       GameState::Practice );
                        PlayLibrary( hWndLib, (int)SendMessage( hWndLib, LVM_GETNEXTITEM, -1, LVNI_SELECTED ),
                            ePlayMode, iId == ID_PRACTICE_CUSTOM );
                    }
                    return 0;
                case ID_PLAY_PAUSE:
                    cPlayback.SetPaused( true, true );
                    return 0;
                case ID_PLAY_PLAYPAUSE:
                    if ( cPlayback.GetPlayMode() ) cPlayback.TogglePaused( true );
                    return 0;
                case ID_PLAY_STOP:
                    if ( cPlayback.GetPlayMode() ) HandOffMsg( msg, wParam, lParam );
                    return 0;
                case ID_PLAY_SKIPFWD: case ID_PLAY_SKIPBACK:
                    if ( cPlayback.GetPlayMode() && cPlayback.GetPlayMode() != GameState::Play )
                        HandOffMsg( msg, wParam, lParam );
                    return 0;
                case ID_PLAY_LOOP:
                    HandOffMsg( msg, ID_PLAY_LOOP, lParam );
                    return 0;
                case ID_LEARN_ADAPTIVE:
                    cPlayback.SetLoop( true );
                    cPlayback.SetLearnMode( GameState::Adaptive, true );
                    return 0;
                case ID_LEARN_WAITING:
                    cPlayback.SetLearnMode( GameState::Waiting, true );
                    return 0;
                case ID_LEARN_NEXTTRACK:
                    HandOffMsg( msg, wParam, lParam );
                    return 0;
                case ID_PLAY_INCREASERATE:
                    if ( cPlayback.GetPlayMode() != GameState::Play )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return 0;
                case ID_PLAY_DECREASERATE:
                    if ( cPlayback.GetPlayMode() != GameState::Play )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return 0;
                case ID_PLAY_RESETRATE:
                    if ( cPlayback.GetPlayMode() != GameState::Play )
                        cPlayback.SetSpeed( 1.0, true );
                    return 0;
                case ID_PLAY_NFASTER:
                    cPlayback.SetNSpeed( cPlayback.GetNSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return 0;
                case ID_PLAY_NSLOWER:
                    cPlayback.SetNSpeed( cPlayback.GetNSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return 0;
                case ID_PLAY_NRESET:
                    cPlayback.SetNSpeed( 1.0, true );
                    return 0;
                case ID_PLAY_VOLUMEUP:
                    cPlayback.SetVolume( min( cPlayback.GetVolume() + 0.1, 1.0 ), true );
                    return 0;
                case ID_PLAY_VOLUMEDOWN:
                    cPlayback.SetVolume( max( cPlayback.GetVolume() - 0.1, 0.0 ), true );
                    return 0;
                case ID_PLAY_MUTE:
                    cPlayback.ToggleMute( true );
                    return 0;
                case ID_VIEW_CONTROLS:
                    cView.ToggleControls( true );
                    return 0;
                case ID_VIEW_LIBRARY:
                    cView.ToggleLibrary( true );
                    return 0;
                case ID_VIEW_KEYBOARD:
                    cView.ToggleKeyboard( true );
                    return 0;
                case ID_VIEW_NOTELABELS:
                    cView.ToggleNoteLabels( true );
                    return 0;
                case ID_VIEW_ALWAYSONTOP:
                    cView.ToggleOnTop( true );
                    return 0;
                case ID_VIEW_FULLSCREEN:
                    cView.ToggleFullScreen( true );
                    return 0;
                case ID_VIEW_MOVEANDZOOM:
                    HandOffMsg( msg, wParam, lParam );
                    return 0;
                case ID_VIEW_RESETMOVEANDZOOM:
                    HandOffMsg( msg, wParam, lParam );
                    return 0;
                case ID_VIEW_NOFULLSCREEN:
                    if ( cView.GetZoomMove() ) HandOffMsg( msg, ID_VIEW_CANCELMOVEANDZOOM, lParam );
                    else if ( cView.GetFullScreen() ) cView.SetFullScreen( false, true );
                    return 0;
                case ID_OPTIONS_PREFERENCES:
                    CheckActivity( TRUE );
                    DoPreferences( hWnd );
                    return 0;
                case ID_HELP_ONLINEFAQ:
                    if ( cView.GetOnTop() ) cView.SetOnTop( false, true );
                    ShellExecute( hWnd, TEXT( "open" ), TEXT( "http://www.PianoFromAbove.com/faq.html" ), NULL, NULL, SW_SHOWNORMAL );
                    return 0;
                case ID_HELP_ABOUT:
                    DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_ABOUT ), g_hWnd, AboutProc );
                    return 0;
                case IDC_METRONOME:
                    if ( iCode == CBN_SELCHANGE )
                        cPlayback.SetMetronome( ( PlaybackSettings::Metronome )SendMessage( ( HWND )lParam, CB_GETCURSEL, 0, 0 ) );
                    return 0;
                case ID_GAMEERROR:
                    MessageBoxW( hWnd, GameState::Errors[lParam].c_str(), L"Error", MB_OK | MB_ICONEXCLAMATION );
                    return 0;
            }
            break;
        }
        case WM_ACTIVATE:
            if ( LOWORD( wParam ) != WA_INACTIVE )
                SetFocus( g_hWndGfx );
            return  0;
        case WM_SYSCOMMAND:
            if ( wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER )
            {
                if ( cPlayback.GetPlayMode() && !cPlayback.GetPaused() )
                    return 0;
            }
            break;
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpmmi = ( LPMINMAXINFO )lParam;
            lpmmi->ptMinTrackSize.x = MINWIDTH;
            lpmmi->ptMinTrackSize.y = MINHEIGHT;
            return 0;
        }
        case WM_SIZE:
            if ( wParam == SIZE_MINIMIZED ) return 0;
            SizeWindows( LOWORD( lParam ), HIWORD( lParam ) );

            if ( wParam != SIZE_MAXIMIZED && !cView.GetFullScreen() )
            {
                RECT rcMain;
                GetWindowRect( hWnd, &rcMain );
                cView.SetMainSize( rcMain.right - rcMain.left, rcMain.bottom - rcMain.top );
            }
            if ( !bInSizeMove )
                HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
            return 0;
        case WM_MOVE:
        {
            RECT rcMain;
            GetWindowRect( hWnd, &rcMain );
            cView.SetMainPos( rcMain.left, rcMain.top );
            return 0;
        }
        case WM_ENTERSIZEMOVE:
            bInSizeMove = true;
            return 0;
        case WM_EXITSIZEMOVE:
            HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
            bInSizeMove = false;
            return 0;
        case WM_DEVICECHANGE:
            Sleep( 200 );
            Config::GetConfig().LoadMIDIDevices();
            HandOffMsg( WM_DEVICECHANGE, 0, 0 );
            break;
        case WM_DESTROY:
            PostQuitMessage( 0 );
            return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

// Used in place of GetMenu because the menu gets detached when in full screen
HMENU GetMainMenu()
{
    static HMENU hMenu = NULL;
    if ( !hMenu ) hMenu = GetMenu( g_hWnd );
    return hMenu;
}

VOID SizeWindows( int iMainWidth, int iMainHeight )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    int iBarHeight = 0, iLibWidth = 0;
    UINT swpFlags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER;

    if ( !iMainWidth || !iMainHeight )
    {
        RECT rcMain;
        GetClientRect( g_hWnd, &rcMain );
        iMainWidth = rcMain.right;
        iMainHeight = rcMain.bottom;
    }

    HDWP hdwp = BeginDeferWindowPos( cView.GetControls() + cView.GetLibrary() + 1 );
    if ( cView.GetControls() )
    {
        RECT rcBarDlg;
        GetWindowRect( g_hWndBar, &rcBarDlg );
        iBarHeight = rcBarDlg.bottom - rcBarDlg.top;
        if ( hdwp ) hdwp = DeferWindowPos( hdwp, g_hWndBar, NULL, 0, 0, iMainWidth, iBarHeight, swpFlags );
    }
    if ( cView.GetFullScreen() && !cVisual.bAlwaysShowControls ) iBarHeight = 0;
    if ( cView.GetLibrary() )
    {
        RECT rcDlg;
        GetWindowRect( g_hWndLibDlg, &rcDlg );
        iLibWidth = cView.GetLibWidth();
        if ( hdwp ) hdwp = DeferWindowPos( hdwp, g_hWndLibDlg, NULL, 0, iBarHeight, iLibWidth, iMainHeight - iBarHeight, swpFlags );
    }
    if ( cView.GetFullScreen() ) iLibWidth = 0;
    if ( hdwp ) hdwp = DeferWindowPos( hdwp, g_hWndGfx, NULL, iLibWidth, iBarHeight,  iMainWidth - iLibWidth, iMainHeight - iBarHeight, swpFlags );
    if ( hdwp ) EndDeferWindowPos( hdwp );
}

LRESULT WINAPI GfxProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    // Static vars for poping up the lib and bar in full screen mode
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    static bool bShowLib, bShowBar;
    static int iBarHeight;

    static bool bTrack = false, bTrackL = false, bTrackR = false;
    static HMENU hMenu;

    if ( ( msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST ) || ( msg >= WM_KEYFIRST && msg <= WM_KEYLAST ) ||
         msg == WM_CAPTURECHANGED || msg == WM_MOUSELEAVE )
        HandOffMsg( msg, wParam, lParam );

    switch (msg)
    {
        case WM_CREATE:
            hMenu = LoadMenu( g_hInstance, MAKEINTRESOURCE( IDR_CONTEXTMENU ) );
            ShowKeyboard( cView.GetKeyboard() );
            ShowNoteLabels( cView.GetNoteLabels() );
            SetTimer( hWnd, IDC_INACTIVITYTIMER, 2500, NULL );
            return 0;
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            if ( iId == ID_SETLABEL )
            {
                TRACKMOUSEEVENT tme = { sizeof( TRACKMOUSEEVENT ), TME_LEAVE | TME_CANCEL, hWnd, HOVER_DEFAULT };
                TrackMouseEvent( &tme );
                bTrack = false;
                if ( DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_NOTELABEL ), hWnd, NoteLabelProc ) == IDOK )
                    HandOffMsg( WM_COMMAND, ID_SETLABEL, lParam );
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
            SetFocus( hWnd );
            if ( !bTrackR && !bTrackL ) SetCapture( hWnd );
            if ( msg == WM_LBUTTONDOWN ) bTrackL = true;
            else bTrackR = true;
            return 0;
        case WM_LBUTTONUP: case WM_RBUTTONUP:
            if ( msg == WM_LBUTTONUP ) bTrackL = false;
            else bTrackR = false;
            if ( !bTrackR && !bTrackL ) ReleaseCapture();
            if ( cView.GetZoomMove()  ) return 0;
            break;
        case WM_CAPTURECHANGED:
            bTrackR = bTrackL = false;
            return 0;
        case WM_CONTEXTMENU:
        {
            POINT ptContext = { ( short )LOWORD( lParam ), ( short )HIWORD( lParam ) };
            HWND hWndContext = ( HWND )wParam;
            if ( hWndContext != g_hWndGfx || cView.GetZoomMove() ) break;

            // VK_APPS or Shift F10 were pressed. Figure out where to display the menu
            if ( ptContext.x < 0 && ptContext.y < 0 )
            {
                RECT rcGfx;
                POINT ptMouse;
                GetCursorPos( &ptMouse );
                GetWindowRect( g_hWndGfx, &rcGfx );

                if ( PtInRect( &rcGfx, ptMouse ) )
                    ptContext = ptMouse;
                else
                {
                    ptContext.x = rcGfx.left;
                    ptContext.y = rcGfx.top;
                }
            }
            
            HMENU hMenuPopup = GetSubMenu( hMenu, 1 );
            HMENU hMenuMain = GetMainMenu();
            CopyMenuState( GetSubMenu( hMenuMain, 0 ), hMenuPopup );
            CopyMenuState( GetSubMenu( hMenuMain, 1 ), hMenuPopup );
            CopyMenuState( GetSubMenu( hMenuMain, 2 ), hMenuPopup );
            CopyMenuState( GetSubMenu( GetSubMenu( hMenuMain, 1 ), 6 ), hMenuPopup );
            CopyMenuItem( GetSubMenu( hMenuMain, 1 ), 6, hMenuPopup, 10, TRUE );

            // Finally diaply the menu
            CheckActivity( TRUE, NULL, TRUE );
            TrackPopupMenuEx( hMenuPopup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, ptContext.x, ptContext.y, g_hWnd, NULL );
            CheckActivity( TRUE, NULL, TRUE );
            return 0;
        }
        // Send me everything except the tab character
        case WM_GETDLGCODE:
        {
            LRESULT lr = DLGC_WANTARROWS | DLGC_WANTCHARS;
            if ( lParam && wParam != VK_TAB ) lr |= DLGC_WANTMESSAGE;
            return lr;
        }
        // Pop up the library/controls if mouse is close enough
        case WM_MOUSEMOVE:
        {
            if ( !bTrack )
            {
                TRACKMOUSEEVENT tme = { sizeof( TRACKMOUSEEVENT ), TME_LEAVE, hWnd, HOVER_DEFAULT };
                TrackMouseEvent( &tme );
                bTrack = true;
            }

            short iXPos = LOWORD( lParam );
            short iYPos = HIWORD( lParam );
            POINT pt = { iXPos, iYPos };
            ClientToScreen( hWnd, &pt );
            CheckActivity( TRUE, &pt );

            if ( cView.GetFullScreen() )
            {
                if ( !iBarHeight )
                {
                    RECT rcBar;
                    GetWindowRect( g_hWndBar, &rcBar );
                    iBarHeight = rcBar.bottom - rcBar.top;
                }

                if ( iXPos < cView.GetLibWidth() && cView.GetLibrary() )
                {
                    ShowWindow( g_hWndLibDlg, SW_SHOWNA );
                    bShowLib = true;
                }
                else if ( bShowLib )
                {
                    ShowWindow( g_hWndLibDlg, SW_HIDE );
                    SetFocus( g_hWndGfx );
                    bShowLib = false;
                }

                if ( !cVisual.bAlwaysShowControls )
                {
                    if ( iYPos < iBarHeight && cView.GetControls() )
                    {
                        ShowWindow( g_hWndBar, SW_SHOWNA );
                        bShowBar = true;
                    }
                    else if ( bShowBar )
                    {
                        ShowWindow( g_hWndBar, SW_HIDE );
                        SetFocus( g_hWndGfx );
                        bShowBar = false;
                    }
                }
            }
            break;
        }
        case WM_MOUSELEAVE:
            bTrack = false;
            break;
        case WM_TIMER:
            if ( wParam == IDC_INACTIVITYTIMER )
                CheckActivity( FALSE );
            return 0;
        case WM_DESTROY:
            DestroyMenu( hMenu );
            KillTimer( hWnd, IDC_INACTIVITYTIMER );
            return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

INT_PTR WINAPI NoteLabelProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static ViewSettings &cView = Config::GetConfig().GetViewSettings();

    switch ( msg )
	{
	    case WM_INITDIALOG:
        {
            SetWindowText( GetDlgItem( hWnd, IDC_NOTELABEL ), Util::StringToWstring( cView.GetCurLabel() ) );

            // Center align to the graphic windows
            RECT rcPos, rcGfx;
            GetClientRect( hWnd, &rcPos );
            GetWindowRect( g_hWndGfx, &rcGfx );
            SetWindowPos( hWnd, NULL, rcGfx.left + ( rcGfx.right - rcGfx.left - rcPos.right ) / 2,
                rcGfx.top + ( static_cast< int >( ( rcGfx.bottom - rcGfx.top ) * ( 1 - MainScreen::KBPercent ) ) - rcPos.bottom ) / 2,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE );

            return TRUE;
        }
	    case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch ( iId )
            {
                case IDOK:
                {
                    TCHAR buf[1024];
                    GetWindowText( GetDlgItem( hWnd, IDC_NOTELABEL ), buf, sizeof( buf ) / sizeof( TCHAR ) );
                    cView.SetCurLabel( Util::WstringToString( buf ) );
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

VOID CopyMenuState( HMENU hMenuSrc, HMENU hMenuDest )
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof( MENUITEMINFO );
    mii.fMask = MIIM_STATE | MIIM_CHECKMARKS | MIIM_ID;
    int iCount = GetMenuItemCount( hMenuSrc );
    for ( int i = 0; i < iCount; i++ )
        if ( GetMenuItemInfo( hMenuSrc, i, TRUE, &mii ) )
            SetMenuItemInfo( hMenuDest, mii.wID, FALSE, &mii );
}

VOID CopyMenuItem( HMENU hMenuSrc, INT iItemSrc, HMENU hMenuDest, INT iItemDest, BOOL bByPosition )
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof( MENUITEMINFO );
    mii.fMask = MIIM_STATE | MIIM_CHECKMARKS;
    if ( GetMenuItemInfo( hMenuSrc, iItemSrc, bByPosition, &mii ) )
        SetMenuItemInfo( hMenuDest, iItemDest, bByPosition, &mii );
}

// Override of the toolbar proc to draw transparent controls
LRESULT WINAPI BarProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();

    switch( msg )
    {
        // WM_HSCROLL doesn't percolate up, so got to handle it here
        case WM_HSCROLL:
        {
            int iCode = LOWORD( wParam );
            if ( iCode == TB_LINEUP || iCode == TB_LINEDOWN || iCode == TB_PAGEUP ||
                 iCode == TB_PAGEDOWN  || iCode == TB_THUMBTRACK || iCode == TB_THUMBPOSITION )
            {
                HWND hWndTrackbar = ( HWND )lParam;
                int iPos = ( iCode == TB_THUMBTRACK || iCode == TB_THUMBPOSITION ? HIWORD( wParam ) : (int)SendMessage( hWndTrackbar, TBM_GETPOS, 0, 0 ) );
                int iId = GetDlgCtrlID( hWndTrackbar );
                switch( iId )
                {
                    case IDC_VOLUME:
                        cPlayback.SetVolume( iPos / 100.0, false );
                        return 0;
                    case IDC_SPEED:
                        if (iPos < 108 && iPos > 92 && iPos != 100) cPlayback.SetSpeed( 1.0, true );
                        else cPlayback.SetSpeed( iPos / 100.0, false );
                        return 0;
                    case IDC_NSPEED:
                        if (iPos < 108 && iPos > 92 && iPos != 100) cPlayback.SetNSpeed( 1.0, true );
                        else cPlayback.SetNSpeed( (200 - iPos) / 100.0, false );
                        return 0;
                }
            }
            break;
        }
        // This makes our static text have a transparent background
        case WM_CTLCOLORSTATIC:
        {
            HDC hDC = ( HDC )wParam;
            SetBkMode( hDC, TRANSPARENT );
            return ( INT_PTR )GetStockObject( NULL_BRUSH );
        }
        // This is for slider controls. Draw our own channel
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            if ( lpnmhdr->code == NM_CUSTOMDRAW &&
                 ( lpnmhdr->idFrom == IDC_VOLUME || lpnmhdr->idFrom == IDC_SPEED || lpnmhdr->idFrom == IDC_NSPEED ) )
            {
                LPNMCUSTOMDRAW lpnmcd = ( LPNMCUSTOMDRAW )lParam;
                switch ( lpnmcd->dwDrawStage )
                {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;
                    case CDDS_ITEMPREPAINT:
                        if ( lpnmcd->dwItemSpec != TBCD_CHANNEL )
                            return CDRF_DODEFAULT;
                        DrawSliderChannel( lpnmcd, hWnd );
                        return CDRF_DODEFAULT; // Still want it to draw the channel as per normal
                }
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            short iXPos = LOWORD( lParam );
            short iYPos = HIWORD( lParam );
            POINT pt = { iXPos, iYPos };
            ClientToScreen( hWnd, &pt );
            CheckActivity( TRUE, &pt );
            break;
        }
    }

    return CallWindowProc( g_pPrevBarProc, hWnd, msg, wParam, lParam);
}

HWND CreateRebar( HWND hWndOwner )
{
    // Create the Rebar. Just houses the toolbar.
    HWND hWndRebar = CreateWindowEx( WS_EX_CONTROLPARENT, REBARCLASSNAME, NULL, 
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CCS_NODIVIDER | RBS_VARHEIGHT,
        0, 0, 0, 0, hWndOwner, ( HMENU )IDC_TOPREBAR, g_hInstance, NULL );
    if( !hWndRebar ) return NULL;

    // Create the system font
    const INT ITEM_POINT_SIZE = 10;
    HDC hDC = GetDC( hWndOwner );
    INT nFontHeight = MulDiv( ITEM_POINT_SIZE, GetDeviceCaps( hDC, LOGPIXELSY ), 72 );
    HFONT hFont = CreateFont( nFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT( "MS Shell Dlg 2" ) );
    ReleaseDC(hWndOwner, hDC);

    // Create and load the button icons
    HIMAGELIST hIml = ImageList_LoadImage( g_hInstance, MAKEINTRESOURCE( IDB_MEDIAICONSSMALL ),
                                           16, 20, RGB( 255, 255, 0), IMAGE_BITMAP, LR_CREATEDIBSECTION );

    // Create the toolbar. Houses custom controls too. Don't want multiple rebar brands because you lose too much control
    HWND hWndToolbar = CreateWindowEx( WS_EX_CONTROLPARENT, TOOLBARCLASSNAME, NULL, 
                                       WS_CHILD | WS_TABSTOP | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
                                       0, 0, 0, 0, hWndRebar, ( HMENU )IDC_TOPTOOLBAR, g_hInstance, NULL);
    if (hWndToolbar == NULL)
        return NULL;

    TBBUTTON tbButtons[8] = 
    {
        { MAKELONG(0, 0), ID_PLAY_PLAY, 0, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Play" ) },
        { MAKELONG(1, 0), ID_PLAY_PAUSE, 0, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Pause" ) },
        { MAKELONG(2, 0), ID_PLAY_STOP, 0, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Stop" ) },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, NULL },
        { MAKELONG(3, 0), ID_PLAY_SKIPBACK, 0, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Skip Back" ) },
        { MAKELONG(4, 0), ID_PLAY_SKIPFWD, 0, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Skip Fwd" ) },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, NULL },
        { MAKELONG(5, 0), ID_PLAY_MUTE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, ( INT_PTR )TEXT( "Mute" ) }
    };

    // First add the toolbar buttons
    g_pPrevBarProc = ( WNDPROC )SetWindowLongPtr( hWndToolbar, GWLP_WNDPROC, ( LONG_PTR )BarProc );
    SendMessage( hWndToolbar, TB_SETIMAGELIST, 0, ( LPARAM )hIml );
    SendMessage( hWndToolbar, TB_SETMAXTEXTROWS, 0, 0 );
    SendMessage( hWndToolbar, TB_BUTTONSTRUCTSIZE, sizeof( TBBUTTON ), 0 );
    SendMessage( hWndToolbar, TB_ADDBUTTONS, sizeof( tbButtons ) / sizeof( TBBUTTON ), ( LPARAM )&tbButtons );
    SendMessage( hWndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG( 32, 29 ) );

    // Now add the other controls
    HWND hWndVolume = CreateWindowEx( 0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_BOTH | TBS_NOTICKS | TBS_TOOLTIPS,
                                      209, 2, 75, 26, hWndToolbar, ( HMENU )IDC_VOLUME, g_hInstance, NULL );
    SendMessage( hWndVolume, TBM_SETRANGE, FALSE, MAKELONG( 0, 100 ) );
    SendMessage( hWndVolume, TBM_SETLINESIZE, 0, 5 ); 

    HWND hWndStatic1 = CreateWindowEx( 0, WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
                                       288, 2, 1, 25, hWndToolbar, NULL, g_hInstance, NULL );
    HWND hWndStatic2 = CreateWindowEx( 0, WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | SS_WHITEFRAME,
                                       289, 2, 1, 25, hWndToolbar, NULL, g_hInstance, NULL );

    HWND hWndStatic3 = CreateWindowEx( 0, WC_STATIC, TEXT( "Metronome:" ), WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       297, 8, 60, 13, hWndToolbar, NULL, g_hInstance, NULL );
    HWND hWndMetronome = CreateWindowEx( 0, WC_COMBOBOX, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       361, 4, 99, 100, hWndToolbar, ( HMENU )IDC_METRONOME, g_hInstance, NULL );
    SendMessage( hWndMetronome, CB_ADDSTRING, 0, ( LPARAM )TEXT( "(Off)" ) );
    SendMessage( hWndMetronome, CB_ADDSTRING, 0, ( LPARAM )TEXT( "Every Beat" ) );
    SendMessage( hWndMetronome, CB_ADDSTRING, 0, ( LPARAM )TEXT( "Every Measure" ) );

    HWND hWndStatic4 = CreateWindowEx( 0, WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
                                       468, 2, 1, 25, hWndToolbar, NULL, g_hInstance, NULL );
    HWND hWndStatic5 = CreateWindowEx( 0, WC_STATIC, NULL, WS_CHILD | WS_VISIBLE | SS_WHITEFRAME,
                                       469, 2, 1, 25, hWndToolbar, NULL, g_hInstance, NULL );

    HWND hWndStatic6 = CreateWindowEx( 0, WC_STATIC, TEXT( "Playback:" ), WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       477, 8, 44, 13, hWndToolbar, NULL, g_hInstance, NULL );
    HWND hWndSpeed = CreateWindowEx( 0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_BOTH | TBS_NOTICKS,
                                     522, 2, 100, 26, hWndToolbar, ( HMENU )IDC_SPEED, g_hInstance, NULL );
    SendMessage( hWndSpeed, TBM_SETRANGE, FALSE, MAKELONG( 5, 195 ) );
    SendMessage( hWndSpeed, TBM_SETLINESIZE, 0, 10 ); 

    HWND hWndStatic7 = CreateWindowEx( 0, WC_STATIC, TEXT( "Notes:" ), WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       629, 8, 35, 13, hWndToolbar, NULL, g_hInstance, NULL );
    HWND hWndNSpeed = CreateWindowEx( 0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_BOTH | TBS_NOTICKS,
                                      665, 2, 100, 26, hWndToolbar, ( HMENU )IDC_NSPEED, g_hInstance, NULL );
    SendMessage( hWndNSpeed, TBM_SETRANGE, FALSE, MAKELONG( 5, 195 ) );
    SendMessage( hWndNSpeed, TBM_SETLINESIZE, 0, 10 ); 

    HWND hWndPosn = CreateWindowEx( 0, POSNCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED,
                                    0, 0, 0, 0, hWndRebar, ( HMENU )IDC_POSNCTRL, g_hInstance, NULL );

    // Set the font to the dialog font
    SendMessage( hWndVolume, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic1, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic2, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic3, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndMetronome, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic4, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic5, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic6, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndStatic7, WM_SETFONT, ( WPARAM )hFont, FALSE );
    SendMessage( hWndSpeed, WM_SETFONT, ( WPARAM )hFont, FALSE );

    REBARBANDINFO rbbi;
    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_TEXT;

    rbbi.fStyle = RBBS_NOGRIPPER | RBBS_VARIABLEHEIGHT;
    rbbi.lpText = TEXT( "" );
    rbbi.hwndChild = hWndToolbar;
    rbbi.cxMinChild = rbbi.cyIntegral = 0;
    rbbi.cyMinChild = rbbi.cyChild = rbbi.cyMaxChild = 31;
    SendMessage( hWndRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi );

    rbbi.fStyle = RBBS_NOGRIPPER | RBBS_VARIABLEHEIGHT | RBBS_BREAK;
    rbbi.lpText = TEXT( "" );
    rbbi.hwndChild = hWndPosn;
    rbbi.cyMinChild = rbbi.cyChild = rbbi.cyMaxChild = 20;
    SendMessage( hWndRebar, RB_INSERTBAND, -1, (LPARAM)&rbbi );

    // Now that the controls are created and set up, fill out the default values
    Config &config = Config::GetConfig();
    const PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    g_hWndBar = hWndRebar; // SetMute needs it :/
    SetLearnMode( cPlayback.GetLearnMode() );
    SetMute( cPlayback.GetMute() );
    SendMessage( hWndSpeed, TBM_SETPOS, TRUE, ( LONG )( 100 * cPlayback.GetSpeed() + .5 ) );
    SendMessage( hWndNSpeed, TBM_SETPOS, TRUE, ( LONG )( 100 * (2.0 - cPlayback.GetNSpeed()) + .5 ) );
    SendMessage( hWndVolume, TBM_SETPOS, TRUE, ( LONG )( 100 * cPlayback.GetVolume() + .5 ) );
    SendMessage( hWndMetronome, CB_SETCURSEL, cPlayback.GetMetronome(), 0 );

    return hWndRebar;
}

// Transparent slider. Draw the parent's background into a mem DC, blit to screen.
VOID DrawSliderChannel( LPNMCUSTOMDRAW lpnmcd, HWND hWndOwner )
{
    static HDC hdcMem = NULL;
    static HBITMAP hBitmap = NULL;

    RECT rcCtrl, rcOwner;
    GetWindowRect( lpnmcd->hdr.hwndFrom, &rcCtrl );
    GetWindowRect( hWndOwner, &rcOwner );
    RECT rcCtrlClient = rcCtrl, rcOwnerClient = rcOwner;
    OffsetRect( &rcCtrlClient, -rcCtrl.left, -rcCtrl.top );
    OffsetRect( &rcOwnerClient, -rcOwner.left, -rcOwner.top );

    // Only make the copy of the parent once
    // ASSUMES THE SAME PARENT! Function will need to change if it ever gets called by more than one Proc
    if ( !hdcMem && !hBitmap )
    {
        hdcMem = CreateCompatibleDC( lpnmcd->hdc );
        hBitmap = CreateCompatibleBitmap( lpnmcd->hdc, rcOwner.right - rcOwner.left, rcOwner.bottom - rcOwner.top );
        SelectObject( hdcMem, hBitmap );
        FillRect( hdcMem, &rcOwnerClient, ( HBRUSH )( COLOR_BTNFACE + 1 ) ); // In case we're on V5
        SendMessage( hWndOwner, WM_PRINTCLIENT, ( WPARAM )hdcMem, PRF_CLIENT | PRF_NONCLIENT ); // V6 Controls only
    }

    BitBlt( lpnmcd->hdc, 0, 0, rcCtrl.right - rcCtrl.left, rcCtrl.bottom - rcCtrl.top,
            hdcMem, rcCtrl.left - rcOwner.left, rcCtrl.top - rcOwner.top, SRCCOPY );
    if ( GetFocus() == lpnmcd->hdr.hwndFrom )
        DrawFocusRect( lpnmcd->hdc, &rcCtrlClient );
}

LRESULT WINAPI PosnProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    // Bad design. Will need to be moved into a struct if we need more than 1 in a program
    static BOOL bEnabled = TRUE, bTracking = FALSE;
    static int iPosition = 0;
    static HIMAGELIST hIml = NULL;
    static HBITMAP hBackbuffer = NULL;
    static HBITMAP hBackground = NULL;
    static int iLoopStart = -1, iLoopEnd = -1;

    switch( msg )
    {
        case WM_CREATE:
            hIml = ImageList_LoadImage( g_hInstance, MAKEINTRESOURCE( IDB_MEDIAICONSSMALL ),
                                        16, 20, CLR_DEFAULT, IMAGE_BITMAP, LR_CREATEDIBSECTION );
            bEnabled = ( ( GetWindowLongPtr( hWnd, GWL_STYLE ) & WS_DISABLED ) == 0 );
            return 0;
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            if ( iId == ID_PLAY_LOOP )
            {
                if ( iLoopEnd >= 0 )
                {
                    iLoopStart = iLoopEnd = -1;
                    
                    RECT rc;
                    SendMessage( g_hWndBar, RB_GETRECT, 1, ( LPARAM )&rc );
                    RedrawWindow( g_hWndBar, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
                }
                else if ( iLoopStart < 0 || iPosition < iLoopStart )
                    iLoopStart = iPosition;
                else
                    iLoopEnd = iPosition;
            }
            else if ( iId == ID_PLAY_CLEARLOOP )
            {
                iLoopStart = iLoopEnd = -1;
                RECT rc;
                SendMessage( g_hWndBar, RB_GETRECT, 1, ( LPARAM )&rc );
                RedrawWindow( g_hWndBar, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
            }
            return 0;
        }
        case WM_ENABLE:
        {
            bEnabled = (BOOL)wParam;
            if ( !bEnabled )
            {
                iPosition = 0;
                if ( bTracking )
                {
                    bTracking = FALSE;
                    ReleaseCapture();
                }
            }

            RECT rc;
            SendMessage( g_hWndBar, RB_GETRECT, 1, ( LPARAM )&rc );
            RedrawWindow( g_hWndBar, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN );
            return 0;
        }
        // 3 Draws, but complicated in order to prevent flicker
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hDC = BeginPaint( hWnd, &ps );
            HDC hDCMem = CreateCompatibleDC( hDC );
            HDC hDCBkg = CreateCompatibleDC( hDCMem );
            
            RECT rcCtrl, rcOwner;
            GetWindowRect( hWnd, &rcCtrl );
            GetWindowRect( g_hWndBar, &rcOwner );
            RECT rcCtrlClient = rcCtrl, rcOwnerClient = rcOwner;
            OffsetRect( &rcCtrlClient, -rcCtrl.left, -rcCtrl.top );
            OffsetRect( &rcOwnerClient, -rcOwner.left, -rcOwner.top );

            // Create bitmaps if needed. Get background.
            HGDIOBJ hbmpOld2 = NULL;
            if ( !hBackbuffer )
            {
                hBackbuffer = CreateCompatibleBitmap( hDC, rcCtrlClient.right, rcCtrlClient.bottom );

                hBackground = CreateCompatibleBitmap( hDC, rcOwner.right - rcOwner.left, rcOwner.bottom - rcOwner.top );
                hbmpOld2 = SelectObject( hDCBkg, hBackground );
                FillRect( hDCBkg, &rcOwnerClient, ( HBRUSH )( COLOR_BTNFACE + 1 ) ); // In case we're on V5
                SendMessage( g_hWndBar, WM_PRINT, ( WPARAM )hDCBkg, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND ); // V6 Controls only
            }
            else
                hbmpOld2 = SelectObject( hDCBkg, hBackground );
            HGDIOBJ hbmpOld1 = SelectObject( hDCMem, hBackbuffer );

            // Channel and thumb rects
            RECT rcChannel, rcThumb;
            GetChannelRect( hWnd, &rcChannel );
            GetThumbRect( hWnd, iPosition, &rcChannel, &rcThumb );

            // Copy background and draw
            BitBlt( hDCMem, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                hDCBkg, rcCtrl.left - rcOwner.left + ps.rcPaint.left, rcCtrl.top - rcOwner.top + ps.rcPaint.top, SRCCOPY );
            if ( bEnabled )
            {
                SetDCBrushColor( hDCMem, RGB( 255, 255, 255 ) );
                HBRUSH hBrush = ( HBRUSH )GetStockObject( DC_BRUSH );
                FillRect( hDCMem, &rcChannel, hBrush );
                if ( iLoopStart >= 0 && ( iLoopEnd >= 0 || iPosition >= iLoopStart ) )
                {
                    SetDCBrushColor( hDCMem, RGB( 63, 72, 204 ) );
                    HBRUSH hBrush = ( HBRUSH )GetStockObject( DC_BRUSH );
                    int iStartPos = ( 2 * iLoopStart * ( rcChannel.right - rcChannel.left - 1 ) + 1000 ) / ( 2 * 1000 );
                    int iEndPos = ( 2 * ( iLoopEnd >= 0 ? iLoopEnd : iPosition ) * ( rcChannel.right - rcChannel.left - 1 ) + 1000 ) / ( 2 * 1000 ) + 1;
                    RECT rcLoop = { rcChannel.left + iStartPos, rcChannel.top, rcChannel.left + iEndPos, rcChannel.bottom };
                    FillRect( hDCMem, &rcLoop, hBrush );
                }
            }
            DrawEdge( hDCMem, &rcChannel, BDR_SUNKENOUTER, BF_RECT );
            ImageList_DrawEx( hIml, 7 + bEnabled, hDCMem, rcThumb.left, rcThumb.top, rcThumb.right - rcThumb.left, rcThumb.bottom - rcThumb.top,
                              CLR_DEFAULT, CLR_DEFAULT, ILD_NORMAL );
            BitBlt( hDC, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                hDCMem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY );

            SelectObject( hDCBkg, hbmpOld2 );
            DeleteDC( hDCBkg );
            SelectObject( hDCMem, hbmpOld1 );
            DeleteDC( hDCMem );
            EndPaint( hWnd, &ps );
            return 0;
        }
        case WM_SIZE:
            if ( hBackbuffer != NULL ) DeleteObject( ( HGDIOBJ )hBackbuffer );
            if ( hBackground != NULL ) DeleteObject( ( HGDIOBJ )hBackground );
            hBackbuffer = NULL;
            hBackground = NULL;
            return 0;
        case WM_LBUTTONDOWN:
        {
            if ( !bEnabled ) return 0;
            POINT pt = { ( SHORT )LOWORD( lParam ), ( SHORT )HIWORD( lParam ) };

            RECT rcChannel, rcThumb;
            GetChannelRect( hWnd, &rcChannel );
            GetThumbRect( hWnd, iPosition, &rcChannel, &rcThumb );
            InflateRect( &rcChannel, 0, 4 );

            if ( PtInRect( &rcChannel, pt ) || PtInRect( &rcThumb, pt ) )
            {
                int iPositionNew = GetThumbPosition( ( SHORT )pt.x, &rcChannel );
                MoveThumbPosition( iPositionNew, iPosition, hWnd, &rcChannel, &rcThumb );
                bTracking = TRUE;
                SetCapture( hWnd );
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            bTracking = false;
            return 0;
        case WM_LBUTTONUP:
            if ( bTracking ) ReleaseCapture();
            bTracking = FALSE;
            return 0;
        case WM_MOUSEMOVE:
        {
            if ( !bTracking ) return 0;

            short iXPos = LOWORD( lParam );
            RECT rcChannel, rcThumbOld;
            GetChannelRect( hWnd, &rcChannel );
            GetThumbRect( hWnd, iPosition, &rcChannel, &rcThumbOld );

            int iPositionNew = GetThumbPosition( iXPos, &rcChannel );
            MoveThumbPosition( iPositionNew, iPosition, hWnd, &rcChannel, &rcThumbOld );
            return 0;
        }
        case TBM_SETPOS:
        {
            RECT rcChannel, rcThumbOld;
            GetChannelRect( hWnd, &rcChannel );
            GetThumbRect( hWnd, iPosition, &rcChannel, &rcThumbOld );
            int iPositionNew = max( min( (int)lParam, 1000 ), 0 );
            MoveThumbPosition( iPositionNew, iPosition, hWnd, &rcChannel, &rcThumbOld, FALSE );
            return 0;
        }
        case WM_DESTROY:
            if ( hBackbuffer != NULL ) DeleteObject( ( HGDIOBJ )hBackbuffer );
            if ( hBackground != NULL ) DeleteObject( ( HGDIOBJ )hBackground );
            ImageList_Destroy( hIml );
            return 0;
    }

    return DefWindowProc( hWnd, msg, wParam, lParam );
}

VOID GetChannelRect( HWND hWnd, RECT *rcChannel )
{
    static const int iSize = 7;

    GetClientRect( hWnd, rcChannel );
    InflateRect( rcChannel, -iSize, -( rcChannel->bottom - rcChannel->top - iSize ) / 2 );
    rcChannel->right--;
    if ( rcChannel->bottom - rcChannel->top == iSize + 1 )
        rcChannel->bottom--;
}

VOID GetThumbRect( HWND hWnd, int iPosition, const RECT *rcChannel, RECT *rcThumb )
{
    int iSize = rcChannel->bottom - rcChannel->top;
    int iPixel = ( 2 * iPosition * ( rcChannel->right - rcChannel->left - 1 ) + 1000 ) / ( 2 * 1000 );
    rcThumb->left = rcChannel->left + iPixel - iSize / 2 - 3;
    rcThumb->top = rcChannel->top - 4;
    rcThumb->right = rcThumb->left + iSize + 6;
    rcThumb->bottom = rcThumb->top + iSize + 8;
}

INT GetThumbPosition( short iXPos, RECT *rcChannel )
{
    int iPositionNew = ( 2 * 1000 * ( iXPos - rcChannel->left ) + rcChannel->right - rcChannel->left ) / ( 2 * ( rcChannel->right - rcChannel->left ) );
    iPositionNew = max( min( iPositionNew, 1000 ), 0 );
    return iPositionNew;
}

VOID MoveThumbPosition( int iPositionNew, int &iPosition, HWND hWnd, RECT *rcChannel, RECT *rcThumbOld, BOOL bUpdateGame )
{
    RECT rcThumbNew, rcInvalid;
    GetThumbRect( hWnd, iPositionNew, rcChannel, &rcThumbNew );
    UnionRect( &rcInvalid, rcThumbOld, &rcThumbNew );
    InflateRect( &rcInvalid, 10, 0 );

    if ( iPositionNew != iPosition )
    {
        MSG msg;
        iPosition = iPositionNew;
        RedrawWindow( hWnd, &rcInvalid, NULL, RDW_INVALIDATE | RDW_UPDATENOW );
        if ( bUpdateGame )
        {
            HandOffMsg( TBM_SETPOS, 0, iPosition );
            while ( PeekMessage( &msg, hWnd, TBM_SETPOS, TBM_SETPOS, PM_REMOVE ) );
        }
    }
}

INT_PTR WINAPI LibDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static HMENU hMenu;
    static SongLibrary &cLibrary = Config::GetConfig().GetSongLibrary();
    static PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();
    static ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();

    // Lots of ugly static vars to handle the splitter functionality :/
    static bool bInPanelResize = false;
    static int iCurWidth, iCurHeight, iBarHeight, iParentWidth, iSplitOffset, iMinWidth, iLibXOffset, iLibYOffset; 
    const static int iMaxOffset = 6;
    const static HCURSOR hCursorWE = LoadCursor( NULL, IDC_SIZEWE );

    // Set the cursor for the splitter. This automatically gets undone because we specified a cursor in RegisterClass
    // Gotta do it for all mouse events because Windows constantly resets it back
    if ( msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST )
    {
        short iXPos = LOWORD( lParam );
        short iYPos = HIWORD( lParam );
        int iTempSplitOffset = iCurWidth - iXPos - 1;
        if ( iTempSplitOffset < iMaxOffset && iTempSplitOffset >= 0 )
            SetCursor( hCursorWE );
    }

    switch(msg)
    {
        case WM_INITDIALOG:
        {
            hMenu = LoadMenu( g_hInstance, MAKEINTRESOURCE( IDR_CONTEXTMENU ) );

            // Set up min width for the splitter functionality
            RECT rcDlg;
            GetWindowRect( hWnd, &rcDlg );
            iCurWidth = iMinWidth = rcDlg.right - rcDlg.left;
            iCurHeight = rcDlg.bottom - rcDlg.top;
            if ( cView.GetLibWidth() < iCurWidth ) cView.SetLibWidth( iCurWidth );

            // Set up the list view
            RECT rcLibrary;
            HWND hWndLibrary = GetDlgItem( hWnd, IDC_LIBRARYFILES );
            SendMessage( hWndLibrary, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT );
            GetWindowRect( hWndLibrary, &rcLibrary );
            iLibXOffset = rcLibrary.left - rcDlg.left;
            iLibYOffset = rcLibrary.top - rcDlg.top;

            // Set up the columns of the list view
            int aFmt[5] = { LVCFMT_LEFT, LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_RIGHT, LVCFMT_RIGHT };
            int aCx[5] = { 166, 150, 40, 32, 49 };
            TCHAR *aText[5] = { TEXT( "File" ), TEXT( "Directory" ), TEXT( "Time" ), TEXT( "Trks" ), TEXT( "Notes/s" ) };

            LVCOLUMN lvc = { 0 };
            lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
            for ( int i = 0; i < sizeof( aFmt ) / sizeof( int ); i++ )
            {
                lvc.fmt = aFmt[i];
                lvc.cx = aCx[i];
                lvc.pszText = aText[i];
                SendMessage( hWndLibrary, LVM_INSERTCOLUMN, i, ( LPARAM )&lvc );
            }

            PopulateLibrary( hWndLibrary );
            return TRUE;
        }
        // Library functionality stuff
        case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = ( LPNMHDR )lParam;
            if ( lpnmhdr->idFrom == IDC_LIBRARYFILES )
            {
                switch ( lpnmhdr->code )
                {
                    case LVN_ITEMCHANGED:
                    {
                        LPNMLISTVIEW pnmv = ( LPNMLISTVIEW )lParam;
                        if ( !cPlayback.GetPlayMode() && ( pnmv->uChanged & LVIF_STATE ) &&
                             ( pnmv->uNewState & LVIS_SELECTED ) != ( pnmv->uOldState & LVIS_SELECTED ) )
                        {
                            if ( pnmv->uNewState & LVIS_SELECTED && !cPlayback.GetPlayable() )
                                cPlayback.SetPlayable( true, true );
                            else if ( !( pnmv->uNewState & LVIS_SELECTED ) && cPlayback.GetPlayable() )
                                cPlayback.SetPlayable( false, true );
                        }
                        return 0;
                    }
                    case LVN_ITEMACTIVATE:
                    {
                        LPNMLISTVIEW pnmv = ( LPNMLISTVIEW )lParam;
                        PlayLibrary( lpnmhdr->hwndFrom, pnmv->iItem, GameState::Practice );
                        return 0;
                    }
                    case LVN_COLUMNCLICK:
                    {
                        LPNMLISTVIEW lpnmlv = ( LPNMLISTVIEW )lParam;
                        int iSortCol = lpnmlv->iSubItem + 1;
                        if ( cLibrary.GetSortCol() == iSortCol ) iSortCol = -iSortCol;
                        SortLibrary( lpnmhdr->hwndFrom, iSortCol );
                    }
                }
            }
            break;
        }
        case WM_CONTEXTMENU:
        {
            HWND hWndContext = ( HWND )wParam;
            HWND hWndLibrary = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
            if ( hWndContext != hWndLibrary ) break;

            HMENU hMenuPopup = GetSubMenu( hMenu, 0 );
            POINT ptContext = { ( short )LOWORD( lParam ), ( short )HIWORD( lParam ) };
            int iItem = (int)SendMessage( hWndLibrary, LVM_GETNEXTITEM, -1, LVNI_SELECTED );

            // VK_APPS or Shift F10 were pressed. Figure out where to display the menu
            if ( ptContext.x < 0 && ptContext.y < 0 )
            {
                RECT rcLib, rcHdr;
                GetClientRect( hWndLibrary, &rcLib );
                GetClientRect( ( HWND )SendMessage( hWndLibrary, LVM_GETHEADER, 0, 0 ), &rcHdr );

                // Item selected? go to center of selected item
                if ( iItem >= 0 )
                {
                    RECT rcItem = { LVIR_BOUNDS };
                    SendMessage( hWndLibrary, LVM_GETITEMRECT, iItem, ( LPARAM )&rcItem );

                    ptContext.x = rcLib.right / 2;
                    ptContext.y = rcItem.top + ( rcItem.bottom - rcItem.top ) / 2;
                    if ( ptContext.y < rcHdr.bottom ) ptContext.y = rcHdr.bottom;
                    if ( ptContext.y >= rcLib.bottom ) ptContext.y = rcLib.bottom - 1;
                }
                // No item, go to mouse pos if in listview or top left of listview if not
                else
                {
                    POINT ptMouse;
                    GetCursorPos( &ptMouse );
                    ScreenToClient( hWndLibrary, &ptMouse );
                    if ( PtInRect( &rcLib, ptMouse ) )
                        ptContext = ptMouse;
                    else
                    {
                        ptContext.x = 0;
                        ptContext.y = rcHdr.bottom;
                    }
                }

                ClientToScreen( hWndLibrary, &ptContext );
            }
            
            // Only enable playing if an item is selected
            CopyMenuItem( GetMainMenu(), ID_FILE_LEARNSONG, hMenuPopup, ID_LEARN_DEFAULT, FALSE );
            UINT uEnable = ( iItem >= 0 ? MF_ENABLED : MF_GRAYED );
            EnableMenuItem( hMenuPopup, ID_PRACTICE_DEFAULT, MF_BYCOMMAND | uEnable );
            EnableMenuItem( hMenuPopup, ID_PRACTICE_CUSTOM, MF_BYCOMMAND | uEnable );
            EnableMenuItem( hMenuPopup, ID_PLAY_DEFAULT, MF_BYCOMMAND | uEnable );
            EnableMenuItem( hMenuPopup, ID_LEARN_DEFAULT, MF_BYCOMMAND | uEnable );

            // Finally diaply the menu
            SetMenuDefaultItem( hMenuPopup, ID_PRACTICE_DEFAULT, FALSE );
            TrackPopupMenuEx( hMenuPopup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, ptContext.x, ptContext.y, g_hWnd, NULL );
            return 0;
        }
        // Splitter functionality stuff
        case WM_SIZE:
        {
            iCurWidth = LOWORD( lParam );
            iCurHeight = HIWORD( lParam );
            cView.SetLibWidth( iCurWidth );

            HWND hWndLibrary = GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES );
            SetWindowPos(hWndLibrary, HWND_TOP, iLibXOffset, iLibYOffset,
                         iCurWidth - 2 * iLibXOffset, iCurHeight - iLibYOffset - iLibXOffset,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER );
            return 0;
        }
        case WM_LBUTTONDBLCLK:
        {
            short iXPos = LOWORD( lParam );
            short iYPos = HIWORD( lParam );
            iSplitOffset = iCurWidth - iXPos - 1;
            if ( iSplitOffset < iMaxOffset && iSplitOffset >= 0 )
            {
                int iNewWidth = ( iCurWidth == MINWIDTH * 3 / 4 ? iMinWidth : MINWIDTH * 3 / 4 );
                int iTop = ( !cView.GetFullScreen() || cVisual.bAlwaysShowControls ? iBarHeight : 0 );
                SetWindowPos( hWnd, NULL, 0, iTop, iNewWidth, iCurHeight, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER );
                if ( !cView.GetFullScreen() )
                    SetWindowPos( g_hWndGfx, NULL, iNewWidth, iBarHeight, iParentWidth - iNewWidth, iCurHeight, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER );
                HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
            }
        }
        case WM_LBUTTONDOWN:
        {
            // Must be short because can take negative values
            short iXPos = LOWORD( lParam );
            short iYPos = HIWORD( lParam );
            iSplitOffset = iCurWidth - iXPos - 1;
            if ( iSplitOffset < iMaxOffset && iSplitOffset >= 0 )
            {
                RECT rcParent;
                GetClientRect( g_hWnd, &rcParent );
                iParentWidth = rcParent.right;

                if ( cView.GetControls() )
                {
                    RECT rcBarDlg;
                    GetWindowRect( g_hWndBar, &rcBarDlg );
                    iBarHeight = rcBarDlg.bottom - rcBarDlg.top;
                }
                else
                    iBarHeight = 0;

                SetCapture( hWnd );
                bInPanelResize = true;
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            bInPanelResize = false;
            return 0;
        case WM_LBUTTONUP:
            if ( bInPanelResize )
            {
                ReleaseCapture();
                HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
                bInPanelResize = false;
            }
            return 0;
        case WM_MOUSEMOVE:
        {
            // Must be short because can take negative values
            short iXPos = LOWORD( lParam );
            short iYPos = HIWORD( lParam );

            // Resize if we're resizing and mouse moved east or west subject to a minimum width
            int iNewWidth = min( max( iMinWidth, iXPos + iSplitOffset + 1 ), MINWIDTH * 3 / 4 );
            if ( bInPanelResize && iNewWidth != iCurWidth )
            {
                int iTop = ( !cView.GetFullScreen() || cVisual.bAlwaysShowControls ? iBarHeight : 0 );
                SetWindowPos( hWnd, NULL, 0, iTop, iNewWidth, iCurHeight, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER );
                if ( !cView.GetFullScreen() )
                    SetWindowPos( g_hWndGfx, NULL, iNewWidth, iBarHeight, iParentWidth - iNewWidth, iCurHeight, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER );
            }
            return 0;
        }
        case WM_DESTROY:
            DestroyMenu( hMenu );
            return 0;
    }

    return 0;
}

VOID PopulateLibrary( HWND hWndLibrary )
{
    // Get a copy of the config to overwrite the settings
    Config &config = Config::GetConfig();
    SongLibrary &cLibrary = config.GetSongLibrary();
    const map< wstring, vector< PFAData::File* >* > &mFiles = cLibrary.GetFiles();
    set< wstring > sFiles;

    SendMessage( hWndLibrary, WM_SETREDRAW, FALSE, 0 );
    SendMessage( hWndLibrary, LVM_DELETEALLITEMS, 0, 0 );

    TCHAR buf[1024];
    LVITEM lvi = { 0 };
    lvi.pszText = buf;
    lvi.iItem = 0;
    for ( map< wstring, vector< PFAData::File* >* >::const_iterator itSource = mFiles.begin(); itSource != mFiles.end(); ++itSource )
    {
        const vector< PFAData::File* > *pvFiles = itSource->second;
        for ( vector< PFAData::File* >::const_iterator itFile = pvFiles->begin(); itFile != pvFiles->end(); ++itFile )
        {
            const wstring sFilename = Util::StringToWstring( ( *itFile )->filename() );
            if ( sFiles.find( sFilename ) == sFiles.end() )
            {
                const PFAData::SongInfo &dSongInfo = cLibrary.GetInfo( ( *itFile )->infopos() )->info();
                int iFileStart = (int)sFilename.find_last_of( L'\\' );
                int iFolderStart = (int)sFilename.find_last_of( L'\\', iFileStart - 1 );

                lvi.iSubItem = 0;
                lvi.mask = LVIF_TEXT | LVIF_PARAM;
                lvi.lParam = ( LPARAM )*itFile;
                _tcscpy_s( buf, sFilename.c_str() + iFileStart + 1 );
                lvi.iItem = (int)SendMessage( hWndLibrary, LVM_INSERTITEM, 0, ( LPARAM )&lvi );

                lvi.iSubItem++;
                lvi.mask = LVIF_TEXT;
                if ( iFileStart - 1 > iFolderStart )
                {
                    _tcsncpy_s( buf, sFilename.c_str() + iFolderStart + 1, iFileStart - iFolderStart - 1 );
                    SendMessage( hWndLibrary, LVM_SETITEM, 0, ( LPARAM )&lvi );
                }

                lvi.iSubItem++;
                _stprintf_s( buf, TEXT( "%d:%02d" ), dSongInfo.seconds() / 60, dSongInfo.seconds() % 60 );
                SendMessage( hWndLibrary, LVM_SETITEM, 0, ( LPARAM )&lvi );

                lvi.iSubItem++;
                _stprintf_s( buf, TEXT( "%d" ), dSongInfo.tracks() );
                SendMessage( hWndLibrary, LVM_SETITEM, 0, ( LPARAM )&lvi );

                lvi.iSubItem++;
                _stprintf_s( buf, TEXT( "%.1f" ), static_cast< float >( dSongInfo.notes() ) / dSongInfo.seconds() );
                SendMessage( hWndLibrary, LVM_SETITEM, 0, ( LPARAM )&lvi );

                lvi.iItem++;
                sFiles.insert( sFilename );
            }
        }
    }

    SortLibrary( hWndLibrary, cLibrary.GetSortCol() );
    SendMessage( hWndLibrary, WM_SETREDRAW, TRUE, 0 );
    InvalidateRect( hWndLibrary, NULL, FALSE );
}

VOID SortLibrary( HWND hWndLibrary, INT iSortCol )
{
    static SongLibrary &cLibrary = Config::GetConfig().GetSongLibrary();

    HWND hWndHeader = ( HWND )SendMessage( hWndLibrary, LVM_GETHEADER, 0, 0 );
    HDITEM hdi = { HDI_FORMAT };

    SendMessage( hWndHeader, HDM_GETITEM, abs( cLibrary.GetSortCol() ) - 1, ( LPARAM )&hdi );
    hdi.fmt &= ~( HDF_SORTDOWN | HDF_SORTUP );
    SendMessage( hWndHeader, HDM_SETITEM, abs( cLibrary.GetSortCol() ) - 1, ( LPARAM )&hdi );

    SendMessage( hWndHeader, HDM_GETITEM, abs( iSortCol ) - 1, ( LPARAM )&hdi );
    hdi.fmt &= ~( HDF_SORTDOWN | HDF_SORTUP );
    hdi.fmt |= ( iSortCol < 0 ? HDF_SORTDOWN : HDF_SORTUP );
    SendMessage( hWndHeader, HDM_SETITEM, abs( iSortCol ) - 1, ( LPARAM )&hdi );

    SendMessage( hWndLibrary, LVM_SORTITEMS, iSortCol, ( LPARAM )CompareLibrary );
    int iItem = (int)SendMessage( hWndLibrary, LVM_GETNEXTITEM, -1, LVNI_SELECTED );
    if ( iItem >= 0 ) SendMessage( hWndLibrary, LVM_ENSUREVISIBLE, iItem, 0 );

    cLibrary.SetSortCol( iSortCol );
}

INT CALLBACK CompareLibrary( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
    static Config &config = Config::GetConfig();
    static SongLibrary &cLibrary = config.GetSongLibrary();

    const PFAData::File *dFile1 = reinterpret_cast< PFAData::File * >( lParam1 );
    const PFAData::File *dFile2 = reinterpret_cast< PFAData::File * >( lParam2 );
    const PFAData::SongInfo &dSongInfo1 = cLibrary.GetInfo( dFile1->infopos() )->info();
    const PFAData::SongInfo &dSongInfo2 = cLibrary.GetInfo( dFile2->infopos() )->info();
    const string &sFile1 = dFile1->filename();
    const string &sFile2 = dFile2->filename();
    int iFileStart1 = (int)sFile1.find_last_of( L'\\' );
    int iFileStart2 = (int)sFile2.find_last_of( L'\\' );
    int iFolderStart1 = (int)sFile1.find_last_of( L'\\', iFileStart1 - 1 );
    int iFolderStart2 = (int)sFile2.find_last_of( L'\\', iFileStart2 - 1 );

    int iMult = ( lParamSort < 0 ? -1 : 1 );
    int iCompare = 0;
    switch ( abs( lParamSort ) )
    {
        case 1:
            iCompare = _stricmp( sFile1.c_str() + iFileStart1 + 1, sFile2.c_str() + iFileStart2 + 1 );
            break;
        case 2:
            break;
        case 3:
            iCompare = ( dSongInfo1.seconds() < dSongInfo2.seconds() ? -1 :
                         dSongInfo1.seconds() > dSongInfo2.seconds() ? 1 : 0 );
            break;
        case 4:
            iCompare = ( dSongInfo1.tracks() < dSongInfo2.tracks() ? -1 :
                         dSongInfo1.tracks() > dSongInfo2.tracks() ? 1 : 0 );
            break;
        case 5:
            if ( dSongInfo1.seconds() == 0 ) iCompare = 1;
            else if ( dSongInfo2.seconds() == 0 ) iCompare = -1;
            else iCompare = ( static_cast< float >( dSongInfo1.notes() ) / dSongInfo1.seconds() < static_cast< float >( dSongInfo2.notes() ) / dSongInfo2.seconds() ? -1 :
                              static_cast< float >( dSongInfo1.notes() ) / dSongInfo1.seconds() > static_cast< float >( dSongInfo2.notes() ) / dSongInfo2.seconds() ? 1 : 0 );
            break;
    }

    if ( !iCompare )
    {
        if ( iFileStart1 - iFolderStart1 <= 1 ) iCompare = 1;
        else if ( iFileStart2 - iFolderStart2 <= 1 ) iCompare = -1;
        else iCompare = _strnicmp( sFile1.c_str() + iFolderStart1 + 1, sFile2.c_str() + iFolderStart2 + 1,
                                   min( iFileStart1 - iFolderStart1 - 1, iFileStart2 - iFolderStart2 - 1 ) );
        if ( !iCompare ) iCompare = ( iFileStart1 - iFolderStart1 == iFileStart2 - iFolderStart2 ? 0 :
                                      iFileStart1 - iFolderStart1 < iFileStart2 - iFolderStart2 ? -1 : 1 );
    }

    if ( !iCompare && abs( lParamSort ) != 1 )
        iCompare = _stricmp( sFile1.c_str() + iFileStart1 + 1, sFile2.c_str() + iFileStart2 + 1 );

    return ( lParamSort < 0 ? -iCompare : iCompare );
}

VOID AddSingleLibraryFile( HWND hWndLibrary, const wstring &sFile )
{
    // Get a copy of the config to overwrite the settings
    Config &config = Config::GetConfig();
    const SongLibrary &cLibrary = config.GetSongLibrary();
    const map< wstring, vector< PFAData::File* >* > &mFiles = cLibrary.GetFiles();
    if ( mFiles.find( sFile ) == mFiles.end() ) return;

    // Set up insertion item
    const PFAData::File *pInfo = mFiles.at( sFile )->at( 0 );
    LVITEM lvi = { 0 };
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.lParam = ( LPARAM )pInfo;
    lvi.pszText = ( LPTSTR )( sFile.c_str() + sFile.find_last_of( L'\\' ) + 1 );

    // Make sure it's not already there
    int iStartPos = -1;
    LVFINDINFO lvfi = { LVFI_STRING, lvi.pszText };
    LVITEM lvif = { 0 };
    while ( ( iStartPos = (int)SendMessage( hWndLibrary, LVM_FINDITEM, iStartPos, ( LPARAM )&lvfi ) ) >= 0 )
    {
        lvif.mask = LVIF_PARAM;
        lvif.iItem = iStartPos;
        SendMessage( hWndLibrary, LVM_GETITEM, 0, ( LPARAM )&lvif );
        if ( pInfo->filename() == ( ( PFAData::File* )lvif.lParam )->filename() ) return;
    }

    // Insert!
    SendMessage( hWndLibrary, LVM_INSERTITEM, 0, ( LPARAM )&lvi );
}

BOOL PlayLibrary( HWND hWndLibrary, int iItem, INT ePlayMode, bool bCustomSettings )
{
    if ( iItem < 0 ) return FALSE;

    LVITEM lvi = { 0 };
    lvi.mask = LVIF_PARAM;
    lvi.iItem = iItem;
    SendMessage( hWndLibrary, LVM_GETITEM, 0, ( LPARAM )&lvi );

    PFAData::File* pmInfo = ( PFAData::File* )lvi.lParam;
    BOOL bSuccess = PlayFile( Util::StringToWstring( pmInfo->filename() ), ePlayMode, bCustomSettings );

    if ( bSuccess ) SetFocus( g_hWndGfx );
    return bSuccess;
}

INT_PTR WINAPI AboutProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static HANDLE hSplash = NULL;
    static ViewSettings &cView = Config::GetConfig().GetViewSettings();

    switch( msg )
    {
	    case WM_INITDIALOG:
        {
            if ( !hSplash ) hSplash = LoadImage( g_hInstance, MAKEINTRESOURCE( IDB_SPLASH ), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );
            SendMessage( GetDlgItem( hWnd, IDC_PICTURE ), STM_SETIMAGE, IMAGE_BITMAP, ( LPARAM )hSplash );

            RECT rcPos, rcParent;
            HWND hWndParent = GetParent( hWnd );
            GetClientRect( hWnd, &rcPos );
            GetWindowRect( hWndParent, &rcParent );
            SetWindowPos( hWnd, NULL, rcParent.left + ( rcParent.right - rcParent.left - rcPos.right ) / 2,
                rcParent.top + ( rcParent.bottom - rcParent.top - rcPos.bottom ) / 2,
                0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE );

            return TRUE;
        }
        case WM_CTLCOLORSTATIC:
        {
            HDC hDC = ( HDC )wParam;
            HWND hWndCtrl = ( HWND )lParam;
            SetBkMode( hDC, TRANSPARENT );
            return ( INT_PTR )GetStockObject( WHITE_BRUSH );
        }
	    case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch ( iId )
            {
                case IDOK: case IDCANCEL:
    			    EndDialog( hWnd, IDOK );
                    return TRUE;
            }
		    break;
        }
	}

	return FALSE;
}

// Helpers involved with user interaction and the GUI

VOID HandOffMsg( UINT msg, WPARAM wParam, LPARAM lParam )
{
    MSG msgGameThread = { g_hWndGfx, msg, wParam, lParam };
    g_MsgQueue.ForcePush( msgGameThread );
}

VOID ShowLibrary( BOOL bShow )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    SizeWindows( 0, 0 );
    if ( !cView.GetFullScreen() )
        ShowWindow( g_hWndLibDlg, bShow ? SW_SHOWNA : SW_HIDE );
    else
        ShowWindow( g_hWndLibDlg, SW_HIDE );
    HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );

    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_LIBRARY, MF_BYCOMMAND | ( bShow ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID ShowControls( BOOL bShow )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    SizeWindows( 0, 0 );
    if ( !cView.GetFullScreen() || cVisual.bAlwaysShowControls )
        ShowWindow( g_hWndBar, bShow ? SW_SHOWNA : SW_HIDE );
    else if ( cView.GetFullScreen() )
        ShowWindow( g_hWndBar, SW_HIDE );
    HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );

    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_CONTROLS, MF_BYCOMMAND | ( bShow ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID ShowKeyboard( BOOL bShow )
{
    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_KEYBOARD, MF_BYCOMMAND | ( bShow ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID ShowNoteLabels( BOOL bShow )
{
    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_NOTELABELS, MF_BYCOMMAND | ( bShow ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID SetOnTop( BOOL bOnTop )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    if ( !cView.GetFullScreen() )
        SetWindowPos( g_hWnd, bOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );

    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_ALWAYSONTOP, MF_BYCOMMAND | ( bOnTop ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID SetFullScreen( BOOL bFullScreen )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    static RECT rcOld = { 0 };
    HMENU hMenu = GetMainMenu();

    if ( bFullScreen )
    {
        RECT rcDesktop;
        GetWindowRect( g_hWnd, &rcOld );
        GetWindowRect( GetDesktopWindow(), &rcDesktop );

        SetMenu( g_hWnd, NULL );
        if ( !cVisual.bAlwaysShowControls ) ShowWindow( g_hWndBar, SW_HIDE );
        ShowWindow( g_hWndLibDlg, SW_HIDE );
        SetWindowLongPtr( g_hWnd, GWL_STYLE, GetWindowLongPtr( g_hWnd, GWL_STYLE ) & ~WS_CAPTION & ~WS_THICKFRAME );
        SetWindowPos( g_hWnd, HWND_TOPMOST, rcDesktop.left, rcDesktop.top,
                      rcDesktop.right - rcDesktop.left, rcDesktop.bottom - rcDesktop.top,
                      SWP_NOACTIVATE | SWP_FRAMECHANGED );
        HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
        CheckMenuItem( hMenu, ID_VIEW_FULLSCREEN, MF_BYCOMMAND | MF_CHECKED );
    }
    else
    {
        SetMenu( g_hWnd, hMenu );
        if ( cView.GetControls() ) ShowWindow( g_hWndBar, SW_SHOWNA );
        if ( cView.GetLibrary() ) ShowWindow( g_hWndLibDlg, SW_SHOWNA );
        SetWindowLongPtr( g_hWnd, GWL_STYLE, GetWindowLongPtr( g_hWnd, GWL_STYLE ) | WS_CAPTION | WS_THICKFRAME );
        SetWindowPos( g_hWnd, cView.GetOnTop() ? HWND_TOPMOST : HWND_NOTOPMOST, rcOld.left, rcOld.top,
                      rcOld.right - rcOld.left, rcOld.bottom - rcOld.top,
                      SWP_NOACTIVATE | SWP_FRAMECHANGED );
        HandOffMsg( WM_COMMAND, ID_VIEW_RESETDEVICE, 0 );
        CheckMenuItem( hMenu, ID_VIEW_FULLSCREEN, MF_BYCOMMAND | MF_UNCHECKED );
    }
}

VOID SetZoomMove( BOOL bZoomMove )
{
    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_VIEW_MOVEANDZOOM, MF_BYCOMMAND | ( bZoomMove ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID SetMute( BOOL bMute )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    SendMessage( hWndToolbar, TB_CHANGEBITMAP, ID_PLAY_MUTE, bMute ? 6 : 5 );
    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_PLAY_MUTE, MF_BYCOMMAND | ( bMute ? MF_CHECKED : MF_UNCHECKED ) );
}

VOID SetSpeed( DOUBLE dSpeed )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    HWND hWndSpeed = GetDlgItem( hWndToolbar, IDC_SPEED );
    SendMessage( hWndSpeed, TBM_SETPOS, TRUE, ( LONG )( 100 * dSpeed + .5 ) );
}

VOID SetNSpeed( DOUBLE dNSpeed )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    HWND hWndNSpeed = GetDlgItem( hWndToolbar, IDC_NSPEED );
    SendMessage( hWndNSpeed, TBM_SETPOS, TRUE, ( LONG )( 100 * (2.0 - dNSpeed) + .5 ) );
}

VOID SetVolume( DOUBLE dVolume )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    HWND hWndVolume = GetDlgItem( hWndToolbar, IDC_VOLUME );
    SendMessage( hWndVolume, TBM_SETPOS, TRUE, ( LONG )( 100 * dVolume + .5 ) );
}

VOID SetPosition( INT iPosition )
{
    HWND hWndPosn = GetDlgItem( g_hWndBar, IDC_POSNCTRL );
    PostMessage( hWndPosn, TBM_SETPOS, 0, iPosition );
}

VOID SetLoop( BOOL bClear )
{
    HWND hWndPosn = GetDlgItem( g_hWndBar, IDC_POSNCTRL );
    PostMessage( hWndPosn, WM_COMMAND, bClear ? ID_PLAY_CLEARLOOP : ID_PLAY_LOOP, 0 );
}

VOID SetMetronome( INT iMetronome )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    HWND hWndMetronome = GetDlgItem( hWndToolbar, IDC_METRONOME );
    SendMessage( hWndMetronome, CB_SETCURSEL, iMetronome, 0 );
}

VOID SetPlayable( BOOL bPlayable )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    SendMessage( hWndToolbar, TB_ENABLEBUTTON, ID_PLAY_PLAY, bPlayable );
}

VOID SetPlayMode( INT ePlayMode )
{
    static const PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();

    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    HMENU hMenu = GetMainMenu();
    BOOL bSplash = ( ePlayMode == GameState::Splash );
    BOOL bPlay = ( ePlayMode == GameState::Play );
    BOOL bPractice = ( ePlayMode == GameState::Practice );
    BOOL bLearn = ( ePlayMode == GameState::Learn );
    BOOL bWaiting = ( cPlayback.GetLearnMode() == GameState::Waiting );

    int iPlayButtons[] = { ID_PLAY_PLAY, ID_PLAY_PAUSE, ID_PLAY_STOP };
    for ( int i = 0; i < sizeof( iPlayButtons ) / sizeof( int ); i++ )
        SendMessage( hWndToolbar, TB_ENABLEBUTTON, iPlayButtons[i], bPractice || bPlay || bLearn );
    int iPracticeButtons[] = { ID_PLAY_SKIPFWD, ID_PLAY_SKIPBACK };
    for ( int i = 0; i < sizeof( iPracticeButtons ) / sizeof( int ); i++ )
        SendMessage( hWndToolbar, TB_ENABLEBUTTON, iPracticeButtons[i], bPractice || bLearn );

    SendMessage( hWndToolbar, TB_PRESSBUTTON, ID_PLAY_PLAY, TRUE );
    SetZoomMove( FALSE );

    int iMenuItems[][6] = { { 1, ePlayMode, ID_FILE_CLOSEFILE },
                            { 3, bPlay || bPractice || bLearn, ID_PLAY_PLAYPAUSE, ID_PLAY_STOP, ID_VIEW_MOVEANDZOOM },
                            { 2, bPractice || bLearn, ID_PLAY_SKIPFWD, ID_PLAY_SKIPBACK },
                            { 1, bPractice || ( bLearn && bWaiting ), ID_PLAY_LOOP },
                            { 3, !bPlay, ID_PLAY_INCREASERATE, ID_PLAY_DECREASERATE, ID_PLAY_RESETRATE },
                            { 3, bLearn, ID_LEARN_ADAPTIVE, ID_LEARN_WAITING, ID_LEARN_NEXTTRACK } };
    for ( int i = 0; i < sizeof( iMenuItems ) / sizeof( iMenuItems[0] ); i++ )
    {
        UINT uEnable = ( iMenuItems[i][1] ? MF_ENABLED : MF_GRAYED );
        for ( int j = 0; j < iMenuItems[i][0]; j++ )
            EnableMenuItem( hMenu, iMenuItems[i][j+2], MF_BYCOMMAND | uEnable );
    }

    HWND hWndPosn = GetDlgItem( g_hWndBar, IDC_POSNCTRL );
    EnableWindow( GetDlgItem( hWndToolbar, IDC_SPEED ), !bPlay );
    EnableWindow( hWndPosn, bPractice || bLearn );
}

VOID SetLearnMode( INT eLearnMode )
{
    HMENU hMenu = GetMainMenu();
    CheckMenuItem( hMenu, ID_LEARN_ADAPTIVE, MF_BYCOMMAND | ( eLearnMode == GameState::Adaptive ? MF_CHECKED : MF_UNCHECKED ) );
    CheckMenuItem( hMenu, ID_LEARN_WAITING, MF_BYCOMMAND | ( eLearnMode == GameState::Waiting ? MF_CHECKED : MF_UNCHECKED ) );
    EnableMenuItem( hMenu, ID_PLAY_LOOP, MF_BYCOMMAND | ( eLearnMode == GameState::Waiting ? MF_ENABLED : MF_GRAYED ) );
}

VOID SetPlayPauseStop( BOOL bPlay, BOOL bPause, BOOL bStop )
{
    HWND hWndToolbar = GetDlgItem( g_hWndBar, IDC_TOPTOOLBAR );
    SendMessage( hWndToolbar, TB_PRESSBUTTON, ID_PLAY_PLAY, bPlay );
    SendMessage( hWndToolbar, TB_PRESSBUTTON, ID_PLAY_PAUSE, bPause );
    SendMessage( hWndToolbar, TB_PRESSBUTTON, ID_PLAY_STOP, bStop );
}

BOOL PlayFile( const wstring &sFile, int ePlayMode, bool bCustomSettings, bool bLibraryEligible )
{
    Config &config = Config::GetConfig();
    const VisualSettings &cVisual = config.GetVisualSettings();
    const AudioSettings &cAudio = config.GetAudioSettings();
    PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    ViewSettings &cView = config.GetViewSettings();
    SongLibrary &cLibrary = config.GetSongLibrary();

    if ( ePlayMode != GameState::Practice && cAudio.iInDevice < 0 )
    {
        MessageBox( g_hWnd, TEXT( "This mode requires a MIDI input device. Plug one in!" ), TEXT( "Error" ), MB_OK | MB_ICONEXCLAMATION );
        return FALSE;
    }

    // Try loading the file
    MainScreen *pGameState = NULL;
    pGameState = new MainScreen( sFile, static_cast< GameState::State >( ePlayMode ), NULL, NULL );
    if ( !pGameState->IsValid() )
    {
        MessageBox( g_hWnd, ( L"Was not able to load " + sFile ).c_str(), TEXT( "Error" ), MB_OK | MB_ICONEXCLAMATION );
        return FALSE;
    }

    // Set up track settings
    if ( bCustomSettings )
    {
        if ( !GetCustomSettings( pGameState ) )
            return FALSE;
    }
    else
    {
        int iNumChannels = pGameState->GetMIDI().GetInfo().iNumChannels;
        pGameState->SetChannelSettings( 
            ePlayMode == GameState::Play ? vector< bool >( iNumChannels, true ) : vector< bool >(),
            ePlayMode == GameState::Play ? vector< bool >( iNumChannels, true ) : vector< bool >(),
            vector< bool >(),
            vector< unsigned >( cVisual.colors, cVisual.colors + sizeof( cVisual.colors ) / sizeof( cVisual.colors[0] ) ) );
    }

    // Success! Set up the GUI for playback
    if ( !cPlayback.GetPlayable() ) cPlayback.SetPlayable( true, true );
    if ( cPlayback.GetPlayMode() != ePlayMode ) cPlayback.SetPlayMode( static_cast< GameState::State >( ePlayMode ), true );
    cPlayback.SetPaused( ePlayMode != GameState::Practice, true );
    cPlayback.SetPosition( 0 );
    cPlayback.SetLoop( true );
    cView.SetZoomMove( false, true );
    if ( ePlayMode == GameState::Play ) cPlayback.SetSpeed( 1.0, true );
    SetWindowText( g_hWnd, sFile.c_str() + ( sFile.find_last_of( L'\\' ) + 1 ) );

    // Add to the library
    if ( bLibraryEligible && cLibrary.GetAlwaysAdd() )
        if ( cLibrary.AddSource( sFile, SongLibrary::File ) > 0 )
            AddSingleLibraryFile( GetDlgItem( g_hWndLibDlg, IDC_LIBRARYFILES ), sFile );

    // Switch game state
    HandOffMsg( WM_COMMAND, ID_CHANGESTATE, ( LPARAM )pGameState );
    return TRUE;
}

VOID CheckActivity( BOOL bIsActive, POINT *ptNew, BOOL bToggleEnable )
{
    static const ViewSettings &cView = Config::GetConfig().GetViewSettings();
    static const VisualSettings &cVisual = Config::GetConfig().GetVisualSettings();
    static bool bEnabled = true;
    static bool bWasActive = true;
    static bool bMouseHidden = false;
    static POINT ptOld;

    if ( !bEnabled && !bToggleEnable ) return;
    if ( bToggleEnable ) bEnabled = !bEnabled;

    // Has the cursor position changed?
    bool bSamePt;
    if ( ptNew )
    {
        bSamePt = ( ptNew->x == ptOld.x && ptNew->y == ptOld.y );
        ptOld = *ptNew;
    }
    else
    {
        POINT pt;
        GetCursorPos( &pt );
        bSamePt = ( pt.x == ptOld.x && pt.y == ptOld.y );
        ptOld = pt;
    }

    if ( ( bIsActive && !ptNew ) || !bSamePt || !cView.GetFullScreen() )
    {
        bWasActive = true;
        if ( bMouseHidden ) bMouseHidden = ( ShowCursor( TRUE ) < 0 );
    }
    else if ( !bIsActive && GetFocus() == g_hWndGfx && !IsWindowVisible( g_hWndLibDlg ) && ( !IsWindowVisible( g_hWndBar ) || cVisual.bAlwaysShowControls ))
    {
        HWND hTest = GetFocus();
        if ( bWasActive )
            bWasActive = false;
        else if ( !bMouseHidden )
            bMouseHidden = ( ShowCursor( FALSE ) < 0 );
    }
}