/*************************************************************************************************
*
* File: GameState.cpp
*
* Description: Implements the game states and objects rendered into the graphics window
*              Contains the core game logic (IntroScreen, SplashScreen, MainScreen objects)
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <tchar.h>

#include "Globals.h"
#include "GameState.h"
#include "Config.h"
#include "resource.h"

const wstring GameState::Errors[] =
{
    L"Success.",
    L"Invalid pointer passed. It would be nice if you could submit feedback with a description of how this happened.",
    L"Out of memory. This is a problem",
    L"Error calling DirectX. It would be nice if you could submit feedback with a description of how this happened.",
    L"This mode requires a MIDI input device. Plug one in!",
    L"Failed to open the MIDI input device. Check that it's not open in another application."
};

GameState::GameError GameState::ChangeState( GameState *pNextState, GameState **pDestObj )
{
    // Null NextState is valid. Signifies no change in state.
    if ( !pNextState )
        return Success;
    if (!pDestObj )
        return BadPointer;

    // Get rid of the old one. Carry over new window/renderer if needed
    if ( *pDestObj )
    {
        if ( !pNextState->m_hWnd ) pNextState->m_hWnd = ( *pDestObj )->m_hWnd;
        if ( !pNextState->m_pRenderer ) pNextState->m_pRenderer = ( *pDestObj )->m_pRenderer;
        delete *pDestObj;
    }
    *pDestObj = pNextState;
    GameError iResult = pNextState->Init();
    if ( iResult )
    {
        *pDestObj = new IntroScreen( pNextState->m_hWnd, pNextState->m_pRenderer );
        delete pNextState;
        ( *pDestObj )->Init();
        return iResult;
    }

    return Success;
}

//-----------------------------------------------------------------------------
// IntroScreen GameState object
//-----------------------------------------------------------------------------

GameState::GameError IntroScreen::MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch (msg)
    {
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch( iId )
            {
                case ID_CHANGESTATE:
                    m_pNextState = reinterpret_cast< GameState* >( lParam );
                    return Success;
                case ID_VIEW_RESETDEVICE:
                    m_pRenderer->ResetDevice();
                    return Success;
            }
        }
    }

    return Success;
}

GameState::GameError IntroScreen::Init()
{
    return Success;
}

GameState::GameError IntroScreen::Logic()
{
    Sleep( 10 );
    return Success;
}

GameState::GameError IntroScreen::Render()
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    // Clear the backbuffer to a blue color
    m_pRenderer->Clear( D3DCOLOR_XRGB( 0, 0, 0 ) );

    m_pRenderer->BeginScene();
    m_pRenderer->DrawRect( 0.0f, 0.0f, static_cast< float >( m_pRenderer->GetBufferWidth() ),
                           static_cast< float >( m_pRenderer->GetBufferHeight() ), 0x00000000 );
    m_pRenderer->EndScene();

    // Present the backbuffer contents to the display
    m_pRenderer->Present();
    return Success;
}

//-----------------------------------------------------------------------------
// SplashScreen GameState object
//-----------------------------------------------------------------------------

SplashScreen::SplashScreen( HWND hWnd, Renderer *pRenderer ) : GameState( hWnd, pRenderer ) 
{
    HRSRC hResInfo = FindResource( NULL, MAKEINTRESOURCE( IDR_SPLASHMIDI ), TEXT( "MIDI" ) );
    HGLOBAL hRes = LoadResource( NULL, hResInfo );
    int iSize = SizeofResource( NULL, hResInfo );
    unsigned char *pData = ( unsigned char * )LockResource( hRes );

    // Parse MIDI
    m_MIDI.ParseMIDI( pData, iSize );
    vector< MIDIEvent* > vEvents;
    vEvents.reserve( m_MIDI.GetInfo().iEventCount );
    m_MIDI.ConnectNotes(); // Order's important here
    m_MIDI.PostProcess( &vEvents );

    // Allocate
    m_vTrackSettings.resize( m_MIDI.GetInfo().iNumTracks );
    m_vState.reserve( 128 );

    // Initialize
    InitNotes( vEvents );
    InitState();
}

void SplashScreen::InitNotes( const vector< MIDIEvent* > &vEvents )
{
    //Get only the channel events
    m_vEvents.reserve( vEvents.size() );
    for ( vector< MIDIEvent* >::const_iterator it = vEvents.begin(); it != vEvents.end(); ++it )
        if ( (*it)->GetEventType() == MIDIEvent::ChannelEvent )
            m_vEvents.push_back( reinterpret_cast< MIDIChannelEvent* >( *it ) );
}

void SplashScreen::InitState()
{
    static Config &config = Config::GetConfig();
    static const PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const VisualSettings &cVisual = config.GetVisualSettings();
    static const AudioSettings &cAudio = Config::GetConfig().GetAudioSettings();

    m_iStartPos = 0;
    m_iEndPos = -1;
    m_llStartTime = m_MIDI.GetInfo().llFirstNote - 3000000;
    m_bPaused = cPlayback.GetPaused();
    m_bMute = cPlayback.GetMute();

    SetChannelSettings( vector< bool >(), vector< bool >(), vector< bool >(),
        vector< unsigned >( cVisual.colors, cVisual.colors + sizeof( cVisual.colors ) / sizeof( cVisual.colors[0] ) ) );

    if ( cAudio.iOutDevice >= 0 )
        m_OutDevice.Open( cAudio.iOutDevice );
    m_OutDevice.SetVolume( 1.0 );
}

GameState::GameError SplashScreen::Init()
{
    return Success;
}

void SplashScreen::ColorChannel( int iTrack, int iChannel, unsigned int iColor, bool bRandom )
{
    if ( bRandom )
        m_vTrackSettings[iTrack].aChannels[iChannel].SetColor();
    else
        m_vTrackSettings[iTrack].aChannels[iChannel].SetColor( iColor );
}

void SplashScreen::SetChannelSettings( const vector< bool > &vScored, const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    bool bScored = vScored.size() > 0;
    bool bMuted = vMuted.size() > 0;
    bool bHidden = vHidden.size() > 0;
    bool bColor = vColor.size() > 0;

    size_t iPos = 0;
    for ( int i = 0; i < mInfo.iNumTracks; i++ )
    {
        const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
        for ( int j = 0; j < 16; j++ )
            if ( mTrackInfo.aNoteCount[j] > 0 )
            {
                if ( bColor && iPos < vColor.size() )
                    ColorChannel( i, j, vColor[iPos] );
                else
                    ColorChannel( i, j, 0, true );
                iPos++;
            }
    }
}

GameState::GameError SplashScreen::MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    static Config &config = Config::GetConfig();
    static PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const AudioSettings &cAudio = config.GetAudioSettings();

    switch (msg)
    {
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch( iId )
            {
                case ID_CHANGESTATE:
                    m_pNextState = reinterpret_cast< GameState* >( lParam );
                    return Success;
                case ID_VIEW_RESETDEVICE:
                    m_pRenderer->ResetDevice();
                    return Success;
            }
        }
        case WM_DEVICECHANGE:
            if ( cAudio.iOutDevice >= 0 && m_OutDevice.GetDevice() != cAudio.vMIDIOutDevices[cAudio.iOutDevice] )
                m_OutDevice.Open( cAudio.iOutDevice );
            break;
        case WM_KEYDOWN:
        {
            bool bCtrl = GetKeyState( VK_CONTROL ) < 0;
            bool bAlt = GetKeyState( VK_MENU ) < 0;

            switch( wParam )
            {
                case VK_SPACE:
                    cPlayback.TogglePaused( true );
                    return Success;
            }
        }
    }

    return Success;
}

GameState::GameError SplashScreen::Logic()
{
    static Config &config = Config::GetConfig();
    static PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();

    // Detect changes in state
    bool bPaused = cPlayback.GetPaused();
    bool bMute = cPlayback.GetMute();
    bool bMuteChanged = ( bMute != m_bMute );
    bool bPausedChanged = ( bPaused != m_bPaused );
    
    // Set the state
    m_bMute = bMute;
    m_bPaused = bPaused;
    m_dVolume = cPlayback.GetVolume();

    double dMaxCorrect = ( mInfo.iMaxVolume > 0 ? 127.0 / mInfo.iMaxVolume : 1.0 );
    double dVolumeCorrect = ( mInfo.iVolumeSum > 0 ? ( m_dVolume * 127.0 * mInfo.iNoteCount ) / mInfo.iVolumeSum : 1.0 );
    dVolumeCorrect = min( dVolumeCorrect, dMaxCorrect );

    // Time stuff
    long long llMaxTime = m_MIDI.GetInfo().llTotalMicroSecs + 500000;
    long long llElapsed = m_Timer.GetMicroSecs();
    m_Timer.Start();

    // If we just paused, kill the music. SetVolume is better than AllNotesOff
    if ( ( bPausedChanged || bMuteChanged ) && ( m_bPaused || m_bMute ) )
        m_OutDevice.AllNotesOff();

    // Figure out start and end times for display
    long long llOldStartTime = m_llStartTime;
    long long llNextStartTime = m_llStartTime + llElapsed;
    if ( !bPaused && m_llStartTime < llMaxTime )
        m_llStartTime = llNextStartTime;
    long long llEndTime = m_llStartTime + TimeSpan;

    // Needs start time to be set. For creating textparticles.
    RenderGlobals();

    // Advance end position
    int iEventCount = (int)m_vEvents.size();
    while ( m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsMicroSec() < llEndTime )
        m_iEndPos++;
        
    // Advance start position updating initial state as we pass stale events
    // Also PLAYS THE MUSIC
    while ( m_iStartPos < iEventCount && m_vEvents[m_iStartPos]->GetAbsMicroSec() <= m_llStartTime )
    {
        MIDIChannelEvent *pEvent = m_vEvents[m_iStartPos];
        if ( pEvent->GetChannelEventType() != MIDIChannelEvent::NoteOn )
            m_OutDevice.PlayEvent( pEvent->GetEventCode(), pEvent->GetParam1(), pEvent->GetParam2() );
        else if ( !m_bMute && !m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bMuted )
            m_OutDevice.PlayEvent( pEvent->GetEventCode(), pEvent->GetParam1(),
                                    static_cast< int >( pEvent->GetParam2() * dVolumeCorrect + 0.5 ) );
        UpdateState( m_iStartPos );
        m_iStartPos++;
    }

    return Success;
}

void SplashScreen::UpdateState( int iPos )
{
    // Event data
    MIDIChannelEvent *pEvent = m_vEvents[iPos];
    if ( !pEvent->GetSister() ) return;

    MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
    int iTrack = pEvent->GetTrack();
    int iChannel = pEvent->GetChannel();
    int iNote = pEvent->GetParam1();
    int iVelocity = pEvent->GetParam2();

    // Turn note on
    if ( eEventType == MIDIChannelEvent::NoteOn && iVelocity > 0 )
        m_vState.push_back( iPos );
    else
    {
        MIDIChannelEvent *pSearch = pEvent->GetSister();
        // linear search and erase. No biggie given N is number of simultaneous notes being played
        vector< int >::iterator it = m_vState.begin();
        while ( it != m_vState.end() )
        {
            if ( m_vEvents[*it] == pSearch )
                it = m_vState.erase( it );
            else
                ++it;
        }
    }
}

const float SplashScreen::SharpRatio = 0.65f;

GameState::GameError SplashScreen::Render()
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    // Clear the backbuffer to a blue color
    m_pRenderer->Clear( D3DCOLOR_XRGB( 0, 0, 0 ) );

    m_pRenderer->BeginScene();
    m_pRenderer->DrawRect( 0.0f, 0.0f, static_cast< float >( m_pRenderer->GetBufferWidth() ),
                           static_cast< float >( m_pRenderer->GetBufferHeight() ), 0x00000000 );
    RenderNotes();
    m_pRenderer->EndScene();

    // Present the backbuffer contents to the display
    m_pRenderer->Present();
    return Success;
}

void SplashScreen::RenderGlobals()
{
    // Midi info
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    m_iStartNote = mInfo.iMinNote;
    m_iEndNote = mInfo.iMaxNote;

    // Screen info
    m_fNotesX = 0.0f;
    m_fNotesCX = static_cast< float >( m_pRenderer->GetBufferWidth() );
    m_fNotesY = 0.0f;
    m_fNotesCY = static_cast< float >( m_pRenderer->GetBufferHeight() );

    // Keys info
    m_iAllWhiteKeys = MIDI::WhiteCount( m_iStartNote, m_iEndNote + 1 );
    float fBuffer = ( MIDI::IsSharp( m_iStartNote ) ? SharpRatio / 2.0f : 0.0f ) +
                    ( MIDI::IsSharp( m_iEndNote ) ? SharpRatio / 2.0f : 0.0f );
    m_fWhiteCX = m_fNotesCX / ( m_iAllWhiteKeys + fBuffer );

    // Round down start time. This is only used for rendering purposes
    long long llMicroSecsPP = static_cast< long long >( TimeSpan / m_fNotesCY + 0.5f );
    m_llRndStartTime = m_llStartTime - ( m_llStartTime < 0 ? llMicroSecsPP : 0 );
    m_llRndStartTime = ( m_llRndStartTime / llMicroSecsPP ) * llMicroSecsPP;
}

void SplashScreen::RenderNotes()
{
    // Do we have any notes to render?
    if ( m_iEndPos < 0 || m_iStartPos >= static_cast< int >( m_vEvents.size() ) )
        return;

    // Render notes. Regular notes then sharps to  make sure they're not hidden
    bool bHasSharp = false;
    for ( vector< int >::iterator it = m_vState.begin(); it != m_vState.end(); ++it )
        if ( !MIDI::IsSharp( m_vEvents[*it]->GetParam1() ) )
            RenderNote( *it );
        else
            bHasSharp = true;

    for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
    {
        MIDIChannelEvent *pEvent = m_vEvents[i];
        if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
             pEvent->GetParam2() > 0 && pEvent->GetSister() )
        {
            if ( !MIDI::IsSharp( pEvent->GetParam1() ) )
                RenderNote( i );
            else
                bHasSharp = true;
        }
    }

    // Do it all again, but only for the sharps
    if ( bHasSharp )
    {
        for ( vector< int >::iterator it = m_vState.begin(); it != m_vState.end(); ++it )
            if ( MIDI::IsSharp( m_vEvents[*it]->GetParam1() ) )
                RenderNote( *it );

        for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
        {
            MIDIChannelEvent *pEvent = m_vEvents[i];
            if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
                 pEvent->GetParam2() > 0 && pEvent->GetSister() &&
                 MIDI::IsSharp( pEvent->GetParam1() ) )
                RenderNote( i );                
        }
    }
}

void SplashScreen::RenderNote( int iPos )
{
    const MIDIChannelEvent *pNote = m_vEvents[iPos];
    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    long long llNoteStart = pNote->GetAbsMicroSec();
    long long llNoteEnd = pNote->GetSister()->GetAbsMicroSec();

    ChannelSettings &csTrack = m_vTrackSettings[iTrack].aChannels[iChannel];
    if ( m_vTrackSettings[iTrack].aChannels[iChannel].bHidden ) return;

    // Compute true positions
    float x = GetNoteX( iNote );
    float y = m_fNotesY + m_fNotesCY * ( 1.0f - static_cast< float >( llNoteStart - m_llRndStartTime ) / TimeSpan );
    float cx =  MIDI::IsSharp( iNote ) ? m_fWhiteCX * SharpRatio : m_fWhiteCX;
    float cy = m_fNotesCY * ( static_cast< float >( llNoteEnd - llNoteStart ) / TimeSpan );
    float fDeflate = m_fWhiteCX * 0.15f / 2.0f;

    // Rounding to make everything consistent
    cy = floor( cy + 0.5f ); // constant cy across rendering
    y = floor( y + 0.5f );
    fDeflate = floor( fDeflate + 0.5f );
    fDeflate = max( min( fDeflate, 3.0f ), 1.0f );

    // Clipping :/
    float fMinY = m_fNotesY - 5.0f;
    float fMaxY = m_fNotesY + m_fNotesCY + 5.0f;
    if ( y > fMaxY )
    {
        cy -= ( y - fMaxY );
        y = fMaxY;
    }
    if ( y - cy < fMinY )
    {
        cy -= ( fMinY - ( y - cy ) );
        y = fMinY + cy;
    }

    // Visualize!
    long long llDuration = m_llStartTime - ( m_MIDI.GetInfo().llFirstNote - 3000000 );
    int iAlpha = 0xFF - static_cast< int >( ( 0xFF * min( 1500000, llDuration ) ) / 1500000 );
    int iAlpha1 = static_cast< int >( ( 0xFF * ( m_fNotesCY - y ) / m_fNotesCY ) + 0.5f );
    int iAlpha2 = static_cast< int >( ( 0xFF * ( m_fNotesCY - ( y + cy ) ) / m_fNotesCY ) + 0.5f );
    iAlpha1 = max( iAlpha1, 0 );
    iAlpha2 = min( iAlpha1, 0xFF );
    iAlpha <<= 24;
    iAlpha1 <<= 24;
    iAlpha2 <<= 24;
    m_pRenderer->DrawRect( x, y - cy, cx, cy, csTrack.iVeryDarkRGB | iAlpha );
    m_pRenderer->DrawRect( x + fDeflate, y - cy + fDeflate,
                            cx - fDeflate * 2.0f, cy - fDeflate * 2.0f,
                            csTrack.iPrimaryRGB | iAlpha1, csTrack.iDarkRGB | iAlpha1, csTrack.iDarkRGB | iAlpha2, csTrack.iPrimaryRGB | iAlpha2 );
}

float SplashScreen::GetNoteX( int iNote )
{
    int iWhiteKeys = MIDI::WhiteCount( m_iStartNote, iNote );
    float fStartX = ( MIDI::IsSharp( m_iStartNote ) - MIDI::IsSharp( iNote ) ) * SharpRatio / 2.0f;
    if ( MIDI::IsSharp( iNote ) )
    {
        MIDI::Note eNote = MIDI::NoteVal( iNote );
        if ( eNote == MIDI::CS || eNote == MIDI::FS ) fStartX -= SharpRatio / 5.0f;
        else if ( eNote == MIDI::AS || eNote == MIDI::DS ) fStartX += SharpRatio / 5.0f;
    }
    return m_fNotesX + m_fWhiteCX * ( iWhiteKeys + fStartX );
}

//-----------------------------------------------------------------------------
// MainScreen GameState object
//-----------------------------------------------------------------------------

MainScreen::MainScreen( wstring sMIDIFile, State eGameMode, HWND hWnd, Renderer *pRenderer ) :
    GameState( hWnd, pRenderer ), m_MIDI( sMIDIFile ), m_eGameMode( eGameMode ), m_cbLastNotes( 500 )
{
    // Finish off midi processing
    if ( !m_MIDI.IsValid() ) return;
    vector< MIDIEvent* > vEvents;
    vEvents.reserve( m_MIDI.GetInfo().iEventCount );
    m_MIDI.ConnectNotes(); // Order's important here
    m_MIDI.PostProcess( &vEvents );

    // Allocate
    m_vTrackSettings.resize( m_MIDI.GetInfo().iNumTracks );
    m_vState.reserve( 128 );

    // Initialize
    InitNoteMap( vEvents ); // Longish
    InitColors();
    InitLabels();
    InitState();
    InitLearning();
}

void MainScreen::InitNoteMap( const vector< MIDIEvent* > &vEvents )
{
    //Get only the channel events
    m_vEvents.reserve( vEvents.size() );
    m_vNoteOns.reserve( vEvents.size() / 2 );
    for ( vector< MIDIEvent* >::const_iterator it = vEvents.begin(); it != vEvents.end(); ++it )
        if ( (*it)->GetEventType() == MIDIEvent::ChannelEvent )
        {
            MIDIChannelEvent *pEvent = reinterpret_cast< MIDIChannelEvent* >( *it );
            m_vEvents.push_back( pEvent );

            // Makes random access to the song faster, but unsure if it's worth it
            MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
            if ( eEventType == MIDIChannelEvent::NoteOn && pEvent->GetParam2() > 0 && pEvent->GetSister() )
                m_vNoteOns.push_back( pair< long long, int >( pEvent->GetAbsMicroSec(), m_vEvents.size() - 1 ) );
            else
            {
                m_vNonNotes.push_back( pair< long long, int >( pEvent->GetAbsMicroSec(), m_vEvents.size() - 1 ) );
                if ( eEventType == MIDIChannelEvent::ProgramChange || eEventType == MIDIChannelEvent::Controller )
                   m_vProgramChange.push_back( pair< long long, int >( pEvent->GetAbsMicroSec(), m_vEvents.size() - 1 ) );
            }
        }
        // Have to keep track of tempo and signature for the measure lines
        else if ( (*it)->GetEventType() == MIDIEvent::MetaEvent )
        {
            MIDIMetaEvent *pEvent = reinterpret_cast< MIDIMetaEvent* >( *it );
            m_vMetaEvents.push_back( pEvent );

            MIDIMetaEvent::MetaEventType eEventType = pEvent->GetMetaEventType();
            if ( eEventType == MIDIMetaEvent::SetTempo )
                m_vTempo.push_back( pair< long long, int >( pEvent->GetAbsMicroSec(), m_vMetaEvents.size() - 1 ) );
            else if ( eEventType == MIDIMetaEvent::TimeSignature )
                m_vSignature.push_back( pair< long long, int >( pEvent->GetAbsMicroSec(), m_vMetaEvents.size() - 1 ) );
        }
}

// Display colors
void MainScreen::InitColors()
{
    m_csBackground.SetColor( 0x00464646, 0.7f, 1.3f );
    m_csKBBackground.SetColor( 0x00999999, 0.4f, 0.0f );
    m_csKBRed.SetColor( 0x000D0A98, 0.5f );
    m_csKBWhite.SetColor( 0x00FFFFFF, 0.8f, 0.6f );
    m_csKBSharp.SetColor( 0x00404040, 0.5f, 0.0f );
    m_csKBBadNote.SetColor( 0x00808080 );
}

void MainScreen::InitLabels()
{
    // Scoring notifications
    vector< TextPath::TextPathVertex > vPath;
    TextPath::TextPathVertex v1 = { 0.0f, -23.0f, 0.0f, 0xFF }; vPath.push_back( v1 );
    TextPath::TextPathVertex v2 = { 0.0f, -26.0f, 0.05f, 0xFF }; vPath.push_back( v2 );
    TextPath::TextPathVertex v3 = { 0.0f, -23.0f, 0.1f, 0xFF }; vPath.push_back( v3 );
    TextPath::TextPathVertex v4 = { 0.0f, -23.0f, 0.4f, 0xFF }; vPath.push_back( v4 );
    TextPath::TextPathVertex v5 = { 0.0f, 0.0f, 0.5f, 0xFF }; vPath.push_back( v5 );
    for ( int i = 0; i < 128; i++ )
    {
        m_tpParticles[i].SetPath( vPath );
        m_tpParticles[i].SetFont( Renderer::SmallComic );
        m_tpParticles[i].Reset( 0.0f, 0.0f, 0, NULL );
        m_tpParticles[i].Kill();
    }

    // Generic message
    vPath.clear();
    TextPath::TextPathVertex v6 = { 0.0f, 0.0f, 0.0f, 0xFF }; vPath.push_back( v6 );
    TextPath::TextPathVertex v7 = { 0.0f, 0.0f, 1.0f, 0xFF }; vPath.push_back( v7 );
    TextPath::TextPathVertex v8 = { 0.0f, 0.0f, 1.1f, 0x00 }; vPath.push_back( v8 );
    m_tpMessage.SetPath( vPath );
    m_tpMessage.SetFont( Renderer::Large );
    m_tpMessage.Kill();

    vPath.clear();
    TextPath::TextPathVertex v9 = { 0.0f, 0.0f, 0.0f, 0xFF }; vPath.push_back( v9 );
    TextPath::TextPathVertex v10 = { 0.0f, 0.0f, TransitionTime * 1.5f / 1000000.0f, 0xFF }; vPath.push_back( v10 );
    TextPath::TextPathVertex v11 = { 0.0f, 0.0f, TransitionTime * 1.5f / 1000000.0f + 0.1f, 0x00 }; vPath.push_back( v11 );
    m_tpLongMessage.SetPath( vPath );
    m_tpLongMessage.SetFont( Renderer::Large );
    m_tpLongMessage.Kill();

    // Note labels
    static Config &config = Config::GetConfig();
    static SongLibrary &cLibrary = config.GetSongLibrary();

    m_pFileInfo = cLibrary.GetInfo( cLibrary.AddFile( m_MIDI.GetInfo().sFilename, &m_MIDI )->infopos() );
    if ( !m_pFileInfo ) return;

    for ( int i = 0; i < m_pFileInfo->label_size(); i++ )
        m_vEvents[ m_pFileInfo->label( i ).pos() ]->SetLabelPtr( m_pFileInfo->mutable_label( i )->mutable_label() );
}

// Init state vars. Only those which validate the date.
void MainScreen::InitState()
{
    static Config &config = Config::GetConfig();
    static const PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const ViewSettings &cView = config.GetViewSettings();
    static const VisualSettings &cVisual = config.GetVisualSettings();

    if ( m_eGameMode != Practice && m_eGameMode != Play && m_eGameMode != Learn )
        m_eGameMode = Practice;
    m_iStartPos = m_iStartInputPos = m_iLearnPos = 0;
    m_iEndPos = m_iEndInputPos = -1;
    m_llStartTime = GetMinTime();
    m_iLastMetronomeNote = HiWoodBlock;
    m_bTrackPos = m_bTrackZoom = false;
    m_fTempZoomX = 1.0f;
    m_fTempOffsetX = m_fTempOffsetY = 0.0f;
    m_dFPS = 0.0;
    m_iFPSCount = 0;
    m_llFPSTime = 0;
    m_dSpeed = -1.0; // Forces a speed reset upon first call to Logic
    m_iNextHotNote = m_iSelectedNote = -1;
    m_bHaveMouse = false;
    m_iShowTop10 = -1;
    m_bScored = false;
    m_bInstructions = ( m_eGameMode != Practice );
    m_iNotesAlpha = 0;
    m_llTransitionTime = GetMinTime() - 1;
    m_llEndLoop = m_llTransitionTime - 1;

    m_eLearnMode = cPlayback.GetLearnMode();
    m_iLearnTrack = 0;
    m_iLearnOrdinal = m_iLearnChannel = -1;

    m_fZoomX = cView.GetZoomX();
    m_fOffsetX = cView.GetOffsetX();
    m_fOffsetY = cView.GetOffsetY();
    m_bPaused = m_bInstructions;
    m_bMute = cPlayback.GetMute();
    double dNSpeed = cPlayback.GetNSpeed();
    m_llTimeSpan = static_cast< long long >( 3.0 * dNSpeed * 1000000 );

    memset( m_pNoteState, -1, sizeof( m_pNoteState ) );
    memset( m_pInputState, -1, sizeof( m_pInputState ) );
    
    AdvanceIterators( m_llStartTime, true );
}

void MainScreen::InitLearning( bool bResetMinTime )
{
    m_bInTransition = false;
    m_iNotesAlpha = m_iNotesTime = m_iWaitingAlpha = m_iWaitingTime = 0;
    m_iLearnPos = m_iStartPos;
    if ( m_eGameMode == Learn && m_eLearnMode == Adaptive )
        m_llTransitionTime = GetMinTime() - 1;
    if ( bResetMinTime )
    {
        m_llMinTime = -1;
        m_bForceWait = false;
    }
    m_iGoodCount = 0;
    m_cbLastNotes.clear();
}

// Called immediately before changing to this state
GameState::GameError MainScreen::Init()
{
    static const AudioSettings &cAudio = Config::GetConfig().GetAudioSettings();
    if ( cAudio.iOutDevice >= 0 )
        m_OutDevice.Open( cAudio.iOutDevice );
    if ( cAudio.iInDevice >= 0 )
    {
        if ( !m_InDevice.Open( cAudio.iInDevice ) && m_eGameMode != Practice )
            return BadInputDevice;
    }
    else if ( m_eGameMode != Practice )
        return NoInputDevice;

    m_OutDevice.SetVolume( 1.0 );
    NextTrack(); // Called here so settings don't get overwritten
    return Success;
}

void MainScreen::ColorChannel( int iTrack, int iChannel, unsigned int iColor, bool bRandom )
{
    if ( bRandom )
        m_vTrackSettings[iTrack].aChannels[iChannel].SetColor();
    else
        m_vTrackSettings[iTrack].aChannels[iChannel].SetColor( iColor );
}

// Sets to a random color
void ChannelSettings::SetColor()
{
    SetColor( Util::RandColor(), 0.6, 0.2 );
}

// Flips around windows format (ABGR) -> direct x format (ARGB)
void ChannelSettings::SetColor( unsigned int iColor, double dDark, double dVeryDark )
{
    int R = ( iColor >> 0 ) & 0xFF, dR, vdR;
    int G = ( iColor >> 8 ) & 0xFF, dG, vdG;
    int B = ( iColor >> 16 ) & 0xFF, dB, vdB;
    int A = ( iColor >> 24 ) & 0xFF;

    int H, S, V;
    Util::RGBtoHSV( R, G, B, H, S, V );
    Util::HSVtoRGB( H, S, min( 100, static_cast< int >( V * dDark ) ), dR, dG, dB );
    Util::HSVtoRGB( H, S, min( 100, static_cast< int >( V * dVeryDark ) ), vdR, vdG, vdB );

    this->iOrigBGR = iColor;
    this->iPrimaryRGB = ( A << 24 ) | ( R << 16 ) | ( G << 8 ) | ( B << 0 );
    this->iDarkRGB = ( A << 24 ) | ( dR << 16 ) | ( dG << 8 ) | ( dB << 0 );
    this->iVeryDarkRGB =  ( A << 24 ) | ( vdR << 16 ) | ( vdG << 8 ) | ( vdB << 0 );
}

ChannelSettings* MainScreen::GetChannelSettings( int iTrack )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    size_t iPos = 0;
    for ( int i = 0; i < mInfo.iNumTracks; i++ )
    {
        const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
        for ( int j = 0; j < 16; j++ )
            if ( mTrackInfo.aNoteCount[j] > 0 )
            {
                if ( iPos == iTrack ) return &m_vTrackSettings[i].aChannels[j];
                iPos++;
            }
    }
    return NULL;
}

void MainScreen::SetChannelSettings( const vector< bool > &vScored, const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    bool bScored = vScored.size() > 0;
    bool bMuted = vMuted.size() > 0;
    bool bHidden = vHidden.size() > 0;
    bool bColor = vColor.size() > 0;

    size_t iPos = 0;
    for ( int i = 0; i < mInfo.iNumTracks; i++ )
    {
        const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
        for ( int j = 0; j < 16; j++ )
            if ( mTrackInfo.aNoteCount[j] > 0 )
            {
                ScoreChannel( i, j, bScored ? vScored[min( iPos, vScored.size() - 1 )] : false );
                MuteChannel( i, j, bMuted ? vMuted[min( iPos, vMuted.size() - 1 )] : false );
                HideChannel( i, j, bHidden ? vHidden[min( iPos, vHidden.size() - 1 )] : false );
                if ( bColor && iPos < vColor.size() )
                    ColorChannel( i, j, vColor[iPos] );
                else
                    ColorChannel( i, j, 0, true );
                iPos++;
            }
    }
}

GameState::GameError MainScreen::MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    // Not thread safe, blah
    static Config &config = Config::GetConfig();
    static PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static ViewSettings &cView = config.GetViewSettings();
    static const ControlsSettings &cControls = config.GetControlsSettings();
    static const AudioSettings &cAudio = config.GetAudioSettings();

    switch (msg)
    {
        // Commands that were passed straight through because they're more involved than setting a state variable
        case WM_COMMAND:
        {
            int iId = LOWORD( wParam );
            switch ( iId )
            {
                case ID_CHANGESTATE:
                    m_pNextState = reinterpret_cast< GameState* >( lParam );
                    return Success;
                case ID_PLAY_STOP:
                    JumpTo( GetMinTime() );
                    m_Score.Reset();
                    m_iShowTop10 = -1;
                    cPlayback.SetStopped( true );
                    return Success;
                case ID_PLAY_SKIPFWD:
                    if ( m_eGameMode == Play ) return Success;
                    JumpTo( static_cast< long long >( m_llStartTime + cControls.dFwdBackSecs * 1000000 ) );
                    return Success;
                case ID_PLAY_SKIPBACK:
                    if ( m_eGameMode == Play ) return Success;
                    JumpTo( static_cast< long long >( m_llStartTime - cControls.dFwdBackSecs * 1000000 ) );
                    return Success;
                case ID_PLAY_LOOP:
                {
                    if ( m_eGameMode != Practice && ( m_eGameMode != Learn || m_eLearnMode != Waiting ) ) return Success;

                    cPlayback.SetLoop( false );
                    long long llMinTime = GetMinTime();
                    if ( m_llEndLoop >= llMinTime )
                    {
                        m_llTransitionTime = llMinTime - 1;
                        m_llEndLoop = m_llTransitionTime - 1;
                        wcscpy_s( m_sBuf, L"Loop Cleared" );
                        m_tpMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, 0xFFFFFFFF, m_sBuf );
                        InitLearning();
                    }
                    else if ( m_llTransitionTime < llMinTime || m_llStartTime < m_llTransitionTime )
                    {
                        m_llTransitionTime = m_llStartTime;
                        wcscpy_s( m_sBuf, L"Loop Start Set" );
                        m_tpMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, 0xFFFFFFFF, m_sBuf );
                        InitLearning();
                    }
                    else
                    {
                        m_llEndLoop = m_llStartTime;
                        wcscpy_s( m_sBuf, L"Loop End Set" );
                        m_tpMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, 0xFFFFFFFF, m_sBuf );
                    }
                    return Success;
                }
                case ID_LEARN_NEXTTRACK:
                    NextTrack();
                    return Success;
                case ID_VIEW_RESETDEVICE:
                    m_pRenderer->ResetDevice();
                    return Success;
                case ID_SETLABEL:
                {
                    MIDIChannelEvent *pEvent = m_vEvents[lParam];
                    if ( pEvent->GetLabel() )
                        pEvent->SetLabel( cView.GetCurLabel() );
                    else
                    {
                        PFAData::Label *pLabel = m_pFileInfo->add_label();
                        pLabel->set_pos( (int)lParam );
                        pLabel->set_label( cView.GetCurLabel() );
                        pEvent->SetLabelPtr( pLabel->mutable_label() );
                    }
                    return Success;
                }
                case ID_VIEW_MOVEANDZOOM:
                    if ( cView.GetZoomMove() )
                    {
                        cView.SetOffsetX( cView.GetOffsetX() + m_fTempOffsetX );
                        cView.SetOffsetY( cView.GetOffsetY() + m_fTempOffsetY );
                        cView.SetZoomX( cView.GetZoomX() * m_fTempZoomX );
                    }
                    else
                    {
                        cView.SetZoomMove( true, true );
                        return Success;
                    }
                case ID_VIEW_CANCELMOVEANDZOOM:
                    cView.SetZoomMove( false, true );
                    m_bTrackPos = m_bTrackZoom = false;
                    m_fTempOffsetX = 0.0f;
                    m_fTempOffsetY = 0.0f;
                    m_fTempZoomX = 1.0f;
                    return Success;
                case ID_VIEW_RESETMOVEANDZOOM:
                    cView.SetOffsetX( 0.0f );
                    cView.SetOffsetY( 0.0f );
                    cView.SetZoomX( 1.0f );
                    m_fTempOffsetX = 0.0f;
                    m_fTempOffsetY = 0.0f;
                    m_fTempZoomX = 1.0f;
                    return Success;
            }
            break;
        }
        // These are doubled from MainProcs.cpp. Allows to get rid of the Ctrl requirement for accellerators
        case WM_KEYDOWN:
        {
            bool bCtrl = GetKeyState( VK_CONTROL ) < 0;
            bool bAlt = GetKeyState( VK_MENU ) < 0;
            bool bShift = GetKeyState( VK_SHIFT ) < 0;

            switch( wParam )
            {
                case VK_SPACE:
                    cPlayback.TogglePaused( true );
                    return Success;
                case VK_OEM_PERIOD:
                    JumpTo( GetMinTime() );
                    m_Score.Reset();
                    m_iShowTop10 = -1;
                    cPlayback.SetStopped( true );
                    return Success;
                case VK_UP:
                    if ( m_eGameMode == Play ) return Success;
                    if ( bAlt && !bCtrl )
                        cPlayback.SetVolume( min( cPlayback.GetVolume() + 0.1, 1.0 ), true );
                    else if ( bShift && !bCtrl )
                        cPlayback.SetNSpeed( cPlayback.GetNSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    else if ( !bAlt && !bShift )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return Success;
                case VK_DOWN:
                    if ( m_eGameMode == Play ) return Success;
                    if ( bAlt && !bShift && !bCtrl )
                        cPlayback.SetVolume( max( cPlayback.GetVolume() - 0.1, 0.0 ), true );
                    else if ( bShift && !bAlt && !bCtrl )
                        cPlayback.SetNSpeed( cPlayback.GetNSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    else if ( !bAlt && !bShift )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return Success;
                case 'R':
                    if ( m_eGameMode == Play ) return Success;
                    cPlayback.SetSpeed( 1.0, true );
                    return Success;
                case VK_LEFT:
                    if ( m_eGameMode == Play ) return Success;
                    JumpTo( static_cast< long long >( m_llStartTime - cControls.dFwdBackSecs * 1000000 ) );
                    return Success;
                case VK_RIGHT:
                    if ( m_eGameMode == Play ) return Success;
                    JumpTo( static_cast< long long >( m_llStartTime + cControls.dFwdBackSecs * 1000000 ) );
                    return Success;
                case 'M':
                    cPlayback.ToggleMute( true );
                    return Success;
                case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
                    if ( m_eGameMode != Practice || bAlt ) return Success;
                    ChannelSettings *pSettings = GetChannelSettings( wParam == '0' ? 9 : (int)wParam - '1' );
                    if ( !pSettings ) return Success;

                    if ( bCtrl ) pSettings->bHidden = !pSettings->bHidden;
                    else pSettings->bMuted = !pSettings->bMuted;

                    swprintf_s( m_sBuf, L"%s Track %d",
                        bCtrl && pSettings->bHidden ? L"Hiding" : bCtrl ? L"Showing" : pSettings->bMuted ? L"Muting" : L"Unmuting",
                        wParam == '0' ? 9 + 1 : wParam - '1' + 1 );
                    m_tpMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, pSettings->iPrimaryRGB, m_sBuf );
                    
                    return Success;
            }
            break;
        }
        case WM_DEVICECHANGE:
            if ( cAudio.iOutDevice >= 0 && m_OutDevice.GetDevice() != cAudio.vMIDIOutDevices[cAudio.iOutDevice] )
                m_OutDevice.Open( cAudio.iOutDevice );
            if ( cAudio.iInDevice >= 0 && m_InDevice.GetDevice() != cAudio.vMIDIInDevices[cAudio.iInDevice] )
                m_InDevice.Open( cAudio.iInDevice );
            break;
        case TBM_SETPOS:
        {
            long long llFirstTime = GetMinTime();
            long long llLastTime = GetMaxTime();
            JumpTo( llFirstTime + ( ( llLastTime - llFirstTime ) * lParam ) / 1000, false );
            break;
        }
        case WM_LBUTTONDOWN:
        {
            if ( m_bZoomMove )
            {
                m_ptLastPos.x = ( SHORT )LOWORD( lParam );
                m_ptLastPos.y = ( SHORT )HIWORD( lParam );
                m_bTrackPos = true;
            }
            else
                m_iSelectedNote = m_iHotNote;

            return Success;
        }
        case WM_RBUTTONDOWN:
        {
            if ( !m_bZoomMove ) return Success;
            m_ptLastPos.x = ( SHORT )LOWORD( lParam );
            m_ptLastPos.y = ( SHORT )HIWORD( lParam );
            m_ptStartZoom.x = static_cast< int >( ( m_ptLastPos.x - m_fOffsetX - m_fTempOffsetX ) / ( m_fZoomX * m_fTempZoomX ) );
            m_ptStartZoom.y = static_cast< int >( m_ptLastPos.y - m_fOffsetY - m_fTempOffsetY );
            m_bTrackZoom = true;
            return Success;
        }
        case WM_CAPTURECHANGED:
            m_bTrackPos = m_bTrackZoom = false;
            return Success;
        case WM_LBUTTONUP:
            if ( m_iSelectedNote >= 0 && m_iHotNote == m_iSelectedNote )
            {
                if ( m_vEvents[m_iHotNote]->GetLabel() ) cView.SetCurLabel( *m_vEvents[m_iHotNote]->GetLabel() );
                else cView.SetCurLabel( "" );
                PostMessage( hWnd, WM_COMMAND, ID_SETLABEL, m_iHotNote );
            }
            m_iSelectedNote = -1;
            m_bTrackPos = false;
            return Success;
        case WM_RBUTTONUP:
            m_bTrackZoom = false;
            return Success;
        case WM_MOUSEMOVE:
        {
            if ( !m_bHaveMouse )
                m_iNextHotNote = -1;
            m_bHaveMouse = true;

            if ( !m_bTrackPos && !m_bTrackZoom && !m_bPaused ) return Success;
            short x = LOWORD( lParam );
            short y = HIWORD( lParam );
            short dx = static_cast< short >( x - m_ptLastPos.x );
            short dy = static_cast< short >( y - m_ptLastPos.y );

            if ( m_bTrackPos )
            {
                m_fTempOffsetX += dx;
                m_fTempOffsetY += dy;
            }
            if ( m_bTrackZoom )
            {
                float fOldX = m_fOffsetX + m_fTempOffsetX + m_ptStartZoom.x * m_fZoomX * m_fTempZoomX;
                m_fTempZoomX *= pow( 2.0f, dx / 200.0f );
                float fNewX = m_fOffsetX + m_fTempOffsetX + m_ptStartZoom.x * m_fZoomX * m_fTempZoomX;
                m_fTempOffsetX = m_fTempOffsetX - ( fNewX - fOldX );
            }

            m_ptLastPos.x = x;
            m_ptLastPos.y = y;
            return Success;
        }
        case WM_MOUSELEAVE:
            m_iSelectedNote = m_iNextHotNote = -1;
            m_bHaveMouse = false;
            return Success;
    }

    return Success;
}

GameState::GameError MainScreen::Logic( void )
{
    static Config &config = Config::GetConfig();
    static PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const ViewSettings &cView = config.GetViewSettings();
    static const VisualSettings &cVisual = config.GetVisualSettings();
    static const VideoSettings &cVideo = config.GetVideoSettings();
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();

    // Detect changes in state
    bool bPaused = cPlayback.GetPaused();
    double dSpeed = cPlayback.GetSpeed();
    double dNSpeed = cPlayback.GetNSpeed();
    bool bMute = cPlayback.GetMute();
    long long llTimeSpan = static_cast< long long >( 3.0 * dNSpeed * 1000000 );
    LearnMode eLearnMode = cPlayback.GetLearnMode();
    bool bPausedChanged = ( bPaused != m_bPaused );
    bool bSpeedChanged = ( dSpeed != m_dSpeed );
    bool bMuteChanged = ( bMute != m_bMute );
    bool bTimeSpanChanged = ( llTimeSpan != m_llTimeSpan );
    bool bLearnModeChanged = ( eLearnMode != m_eLearnMode );
    
    // Set the state
    m_bPaused = bPaused;
    m_dSpeed = dSpeed;
    m_bMute = bMute;
    m_eLearnMode = eLearnMode;
    m_llTimeSpan = llTimeSpan;
    m_dVolume = cPlayback.GetVolume();
    m_bShowKB = cView.GetKeyboard();
    m_bNoteLabels = cView.GetNoteLabels();
    m_bZoomMove = cView.GetZoomMove();
    m_fOffsetX = cView.GetOffsetX();
    m_fOffsetY = cView.GetOffsetY();
    m_fZoomX = cView.GetZoomX();
    if ( !m_bZoomMove ) m_bTrackPos = m_bTrackZoom = false;
    m_eKeysShown = cVisual.eKeysShown;
    m_iStartNote = min( cVisual.iFirstKey, cVisual.iLastKey );
    m_iEndNote = max( cVisual.iFirstKey, cVisual.iLastKey );
    m_bShowFPS = cVideo.bShowFPS;
    m_pRenderer->SetLimitFPS( cVideo.bLimitFPS );
    if ( cVisual.iBkgColor != m_csBackground.iOrigBGR ) m_csBackground.SetColor( cVisual.iBkgColor, 0.7f, 1.3f );
    m_bInstructions &= m_bPaused;

    double dMaxCorrect = ( mInfo.iMaxVolume > 0 ? 127.0 / mInfo.iMaxVolume : 1.0 );
    double dVolumeCorrect = ( mInfo.iVolumeSum > 0 ? ( m_dVolume * 127.0 * mInfo.iNoteCount ) / mInfo.iVolumeSum : 1.0 );
    dVolumeCorrect = min( dVolumeCorrect, dMaxCorrect );

    m_iHotNote = m_iNextHotNote;
    m_iNextHotNote = -1;
    if ( !m_bPaused ) m_iSelectedNote = -1;

    // Time stuff
    long long llMaxTime = GetMaxTime();
    long long llElapsed = m_Timer.GetMicroSecs();
    m_Timer.Start();

    // Compute FPS every half a second
    m_llFPSTime += llElapsed;
    m_iFPSCount++;
    if ( m_llFPSTime >= 500000 )
    {
        m_dFPS = m_iFPSCount / ( m_llFPSTime / 1000000.0 );
        m_llFPSTime = m_iFPSCount = 0;
    }

    // If we just paused, kill the music. SetVolume is better than AllNotesOff
    if ( ( bPausedChanged || bMuteChanged ) && ( m_bPaused || m_bMute ) )
        m_OutDevice.AllNotesOff();

    // If speed has been changed, rejigger inputpos
    if ( bSpeedChanged && !m_bInTransition )
        FindInputPos();

    if ( bLearnModeChanged )
        InitLearning();

    // Figure out start and end times for display
    long long llOldStartTime = m_llStartTime;
    long long llNextStartTime = m_llStartTime + static_cast< long long >( llElapsed * m_dSpeed + 0.5 );

    // Figure out if we need to wait
    bool bWait = ( !m_bPaused ? DoWaiting( llNextStartTime, llElapsed ) : false );

    if ( !bWait && !m_bPaused && m_llStartTime < llMaxTime )
        m_llStartTime = llNextStartTime;
    m_iStartTick = GetCurrentTick( m_llStartTime );
    long long llEndTime = m_llStartTime + m_llTimeSpan;

    // Figure out start and end times for input
    long long llInputSpan = static_cast< long long >( GameScore::OkTime * m_dSpeed );
    long long llStartInputTime = m_llStartTime - llInputSpan;
    long long llEndInputTime = m_llStartTime + llInputSpan;
    long long llLearnTime = m_llStartTime - TestTime;

    // Needs start time to be set. For creating textparticles.
    RenderGlobals();

    // Advance end position
    int iEventCount = (int)m_vEvents.size();
    while ( m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsMicroSec() < llEndTime )
    {
        MIDIChannelEvent *pEvent = m_vEvents[m_iEndPos + 1];
        if ( m_InDevice.IsOpen() && pEvent->GetAbsMicroSec() >= m_llMinTime && 
            ( m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bScored || 
              ( m_eGameMode == Learn && m_iLearnOrdinal < 0 ) ) )
            pEvent->SetInputQuality( MIDIChannelEvent::OnRadar );
        else
            pEvent->SetInputQuality( MIDIChannelEvent::Ignore );
        m_iEndPos++;
    }
        
    // Advance end input pos. EndInputPos probably doesn't need to exist :/
    while ( m_iEndInputPos + 1 < iEventCount && m_vEvents[m_iEndInputPos + 1]->GetAbsMicroSec() < llEndInputTime )
        m_iEndInputPos++;

    // Only want to advance start positions when unpaused becuase advancing startpos "consumes" the events
    if ( !m_bPaused )
    {
        // Advance start position updating initial state as we pass stale events
        // Also PLAYS THE MUSIC
        while ( m_iStartPos < iEventCount && m_vEvents[m_iStartPos]->GetAbsMicroSec() <= m_llStartTime )
        {
            MIDIChannelEvent *pEvent = m_vEvents[m_iStartPos];
            if ( pEvent->GetChannelEventType() != MIDIChannelEvent::NoteOn )
                m_OutDevice.PlayEvent( pEvent->GetEventCode(), pEvent->GetParam1(), pEvent->GetParam2() );
            else if ( !m_bMute && !m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bMuted &&
                      ( m_eGameMode != Learn || m_iLearnOrdinal >= 0 ) )
                m_OutDevice.PlayEvent( pEvent->GetEventCode(), pEvent->GetParam1(),
                                       static_cast< int >( pEvent->GetParam2() * dVolumeCorrect + 0.5 ) );
            UpdateState( m_iStartPos );
            m_iStartPos++;
        }

        // Advance start input pos. Add to score. StartInputPos primarily serves as missed note detection
        while ( m_iStartInputPos < iEventCount && m_vEvents[m_iStartInputPos]->GetAbsMicroSec() <= llStartInputTime )
        {
            MIDIChannelEvent *pEvent = m_vEvents[m_iStartInputPos];
            if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn && pEvent->GetParam2() > 0 )
            {
                int iNote = pEvent->GetParam1();
                MIDIChannelEvent::InputQuality eInputQuality = pEvent->GetInputQuality();
                if ( eInputQuality == MIDIChannelEvent::OnRadar )
                {
                    eInputQuality = MIDIChannelEvent::Missed;
                    pEvent->SetInputQuality( eInputQuality );
                    m_Score.Missed();

                    float x = GetNoteX( iNote );
                    float cx = m_fWhiteCX * ( MIDI::IsSharp( iNote ) ? SharpRatio : 1.0f );
                    m_tpParticles[iNote].Reset( x + cx / 2.0f, 0.0f, GameScore::MissedColor, GameScore::MissedText );
                }
                if ( m_eGameMode == Learn && m_eLearnMode == Adaptive && eInputQuality != MIDIChannelEvent::Ignore && pEvent->GetAbsMicroSec() >= m_llMinTime )
                {
                    if ( m_cbLastNotes.full() ) m_iGoodCount -= m_cbLastNotes.front().first;
                    m_cbLastNotes.push_back( pair< int, int >( eInputQuality == MIDIChannelEvent::Missed ? 1 : 0, m_iStartInputPos ) );
                    m_iGoodCount += m_cbLastNotes.back().first;
                }
            }
            m_iStartInputPos++;
        }

        //Advance the learning iterator
        while ( m_iLearnPos < iEventCount && m_vEvents[m_iLearnPos]->GetAbsMicroSec() <= llLearnTime )
        {
            MIDIChannelEvent *pEvent = m_vEvents[m_iLearnPos];
            if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn && pEvent->GetParam2() > 0 && m_eGameMode == Learn &&
                 m_eLearnMode == Adaptive && !m_bInTransition && pEvent->GetInputQuality() != MIDIChannelEvent::Ignore && pEvent->GetAbsMicroSec() >= m_llMinTime )
            {
                while ( m_cbLastNotes.size() > 0 && m_cbLastNotes.front().second <= m_iLearnPos )
                {
                    m_iGoodCount -= m_cbLastNotes.front().first;
                    m_cbLastNotes.pop_front();
                }
                m_llTransitionTime = pEvent->GetAbsMicroSec() - 1000000 / 30;
            }
            m_iLearnPos++;
        }
        
        PlayMetronome( dVolumeCorrect );
        DoTransition( llElapsed, llOldStartTime );
    }

    // Advance the text particles
    for ( int i = 0; i < 128; i++ )
        if ( m_tpParticles[i].IsAlive() )
            m_tpParticles[i].Logic( llElapsed );
    if ( m_tpMessage.IsAlive() ) m_tpMessage.Logic( llElapsed );
    if ( m_tpLongMessage.IsAlive() ) m_tpLongMessage.Logic( llElapsed );

    AdvanceIterators( m_llStartTime, false );
    ProcessInput();

    // Update the position slider
    long long llFirstTime = GetMinTime();
    long long llLastTime = GetMaxTime();
    long long llOldPos = ( ( llOldStartTime - llFirstTime ) * 1000 ) / ( llLastTime - llFirstTime );
    long long llNewPos = ( ( m_llStartTime - llFirstTime ) * 1000 ) / ( llLastTime - llFirstTime );
    if ( llOldPos != llNewPos ) cPlayback.SetPosition( static_cast< int >( llNewPos ) );

    // Song's over
    if ( !m_bPaused && m_llStartTime >= llMaxTime && !m_bInTransition )
    {
        if ( m_eGameMode == Learn && m_iLearnOrdinal >= 0 )
        {
            wcscpy_s( m_sBuf, L"You made it!\nMoving to the next track" );
            m_tpLongMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f, 0x00FFFFFF, m_sBuf );
            NextTrack();
            JumpTo( GetMinTime() );
        }
        else
        {
            cPlayback.SetPaused( true, true );
            if ( m_eGameMode == Play && m_iShowTop10 == -1 )
                m_iShowTop10 = m_Score.AddToTop10( m_pFileInfo );
        }
    }
    return Success;
}

void MainScreen::UpdateState( int iPos )
{
    // Event data
    MIDIChannelEvent *pEvent = m_vEvents[iPos];
    if ( !pEvent->GetSister() ) return;

    MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
    int iTrack = pEvent->GetTrack();
    int iChannel = pEvent->GetChannel();
    int iNote = pEvent->GetParam1();
    int iVelocity = pEvent->GetParam2();

    // Turn note on
    if ( eEventType == MIDIChannelEvent::NoteOn && iVelocity > 0 )
    {
        m_vState.push_back( iPos );
        m_pNoteState[iNote] = iPos;
    }
    else
    {
        m_pNoteState[iNote] = -1;
        MIDIChannelEvent *pSearch = pEvent->GetSister();
        // linear search and erase. No biggie given N is number of simultaneous notes being played
        vector< int >::iterator it = m_vState.begin();
        while ( it != m_vState.end() )
        {
            if ( m_vEvents[*it] == pSearch )
                it = m_vState.erase( it );
            else
            {
                if ( it != m_vState.end() && m_vEvents[*it]->GetParam1() == iNote )
                    m_pNoteState[iNote] = *it;
                ++it;
            }
        }
    }
}

void TextPath::Logic( long long llElapsed )
{
    float fElapsed = llElapsed / 1000000.0f;
    m_t += fElapsed;

    int iSize = (int)m_vPath.size();
    while ( m_iPos < iSize - 1 && m_vPath[m_iPos + 1].t < m_t )
        m_iPos++;
    if ( m_iPos >= iSize - 1 ) return;
    
    const TextPathVertex &curr = m_vPath[m_iPos];
    const TextPathVertex &next = m_vPath[m_iPos + 1];
    m_x = curr.x + ( next.x - curr.x ) * ( m_t - curr.t ) / ( next.t - curr.t );
    m_y = curr.y + ( next.y - curr.y ) * ( m_t - curr.t ) / ( next.t - curr.t );
    int a = curr.a + static_cast< int >( ( next.a - curr.a ) * ( m_t - curr.t ) / ( next.t - curr.t ) + 0.5f );
    m_iColor = ( m_iColor & 0x00FFFFFF ) | ( a << 24 );
}

// Play the metronome if necessary. Used to be in Logic
void MainScreen::PlayMetronome( double dVolumeCorrect )
{
    static Config &config = Config::GetConfig();
    static const PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const AudioSettings &cAudio = config.GetAudioSettings();
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();

    // Play the metronome
    if ( m_iNextBeatTick <= m_iStartTick )
    {
        int iBeat = GetBeat( m_iNextBeatTick, m_iBeatType, m_iLastSignatureTick );
        bool bIsMeasure = !( ( iBeat < 0 ? -iBeat : iBeat ) % m_iBeatsPerMeasure );

        if ( !m_bMute && ( cPlayback.GetMetronome() == PlaybackSettings::EveryBeat ||
                ( bIsMeasure && cPlayback.GetMetronome() == PlaybackSettings::EveryMeasure ) ) )
        {
            m_iLastMetronomeNote = ( m_iLastMetronomeNote == HiWoodBlock ? LowWoodBlock : HiWoodBlock );
            m_OutDevice.PlayEvent( 0x99, m_iLastMetronomeNote, static_cast< int >( mInfo.iVolumeSum * dVolumeCorrect / mInfo.iNoteCount + -.5 ) );
        }

        m_iNextBeatTick = GetBeatTick( m_iStartTick + 1, m_iBeatType, m_iLastSignatureTick );
    }
}

void MainScreen::ProcessInput()
{
    static const ControlsSettings &cControls = Config::GetConfig().GetControlsSettings();
    static PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();

    int iMilliSecs;
    unsigned char cStatus, cParam1, cParam2;
    while ( m_InDevice.GetMIDIMessage( cStatus, cParam1, cParam2, iMilliSecs ) )
    {
        MIDIChannelEvent::ChannelEventType eEventType = static_cast< MIDIChannelEvent::ChannelEventType >( cStatus >> 4 );
        if ( eEventType == MIDIChannelEvent::NoteOff || ( eEventType == MIDIChannelEvent::NoteOn && cParam2 == 0 ) )
            m_pInputState[cParam1] = -1;
        else if ( eEventType == MIDIChannelEvent::NoteOn )
        {
            if ( m_bInstructions )
            {
                 m_bInstructions = false;
                 cPlayback.SetPaused( false, true );
            }
            else if ( cControls.aKeyboardMap[cParam1] > 0 )
                PostMessage( g_hWnd, WM_COMMAND, cControls.aKeyboardMap[cParam1] + 33, 0 );
            else
            {
                int iSecondaryPos = -1;
                m_pInputState[cParam1] = -2;
                for ( int i = m_iStartInputPos; i <= m_iEndInputPos; i++ )
                {
                    MIDIChannelEvent *pEvent = m_vEvents[i];
                    if ( pEvent->GetChannelEventType() == eEventType && pEvent->GetParam2() > 0 &&
                         pEvent->GetParam1() == cParam1 )
                    {
                        if ( pEvent->GetInputQuality() == MIDIChannelEvent::OnRadar ||
                             pEvent->GetInputQuality() == MIDIChannelEvent::Waiting )
                        {
                            m_pInputState[cParam1] = i;
                            break;
                        }
                        else if ( pEvent->GetInputQuality() == MIDIChannelEvent::Ignore )
                            iSecondaryPos = i;
                    }
                }
                if ( m_pInputState[cParam1] >= 0 ) iSecondaryPos = -1;
                else if ( iSecondaryPos >= 0 ) m_pInputState[cParam1] = iSecondaryPos;

                if ( !m_bPaused )
                {
                    if ( m_pInputState[cParam1] >= 0 )
                    {
                        MIDIChannelEvent *pEvent = m_vEvents[m_pInputState[cParam1]];
                        MIDIChannelEvent::InputQuality eQuality = m_Score.HitQuality( m_llStartTime - pEvent->GetAbsMicroSec(), m_dSpeed );
                        pEvent->SetInputQuality( eQuality );
                        if ( iSecondaryPos < 0 ) m_Score.Hit( eQuality );

                        if ( m_eGameMode != Learn || m_eLearnMode != Waiting )
                        {
                            float x = GetNoteX( cParam1 );
                            float cx = m_fWhiteCX * ( MIDI::IsSharp( cParam1 ) ? SharpRatio : 1.0f );
                            unsigned iColor = ( eQuality == MIDIChannelEvent::Great ? GameScore::GreatColor : eQuality == MIDIChannelEvent::Good ? GameScore::GoodColor : GameScore::OkColor );
                            const wchar_t *sText = ( eQuality == MIDIChannelEvent::Great ? GameScore::GreatText : eQuality == MIDIChannelEvent::Good ? GameScore::GoodText : GameScore::OkText );
                            m_tpParticles[cParam1].Reset( x + cx / 2.0f, 0.0f, iColor, sText );
                        }
   
                        m_bForceWait = false;
                        break;
                    }
                    else
                    {
                        if ( iSecondaryPos < 0 ) m_Score.Incorrect();

                        float x = GetNoteX( cParam1 );
                        float cx = m_fWhiteCX * ( MIDI::IsSharp( cParam1 ) ? SharpRatio : 1.0f );
                        m_tpParticles[cParam1].Reset( x + cx / 2.0f, 0.0f, GameScore::IncorrectColor, GameScore::IncorrectText );

                        if ( m_eGameMode == Learn && m_eLearnMode == Adaptive && m_llStartTime >= m_llMinTime )
                        {
                            if ( m_cbLastNotes.full() ) m_iGoodCount -= m_cbLastNotes.front().first;
                            m_cbLastNotes.push_back( pair< int, int >( 1, m_iStartInputPos ) );
                            m_iGoodCount += m_cbLastNotes.back().first;
                        }
                    }
                }
            }

            // Resets the windows inactivity timer
            INPUT in = { 0 };
            in.type = INPUT_MOUSE;
            in.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput( 1, &in, sizeof( INPUT ) );
        }
    }
}

void MainScreen::JumpTo( long long llStartTime, bool bUpdateGUI, bool bInitLearning )
{
    // Kill the music!
    m_OutDevice.AllNotesOff();
    m_bInstructions = false;
    if ( bInitLearning ) InitLearning();

    // Start time. Piece of cake!
    long long llFirstTime = GetMinTime();
    long long llLastTime = GetMaxTime();
    m_llStartTime = min( max( llStartTime, llFirstTime ), llLastTime );
    long long llEndTime = m_llStartTime + m_llTimeSpan;

    // Start position and current state: hard!
    eventvec_t::iterator itBegin = m_vNoteOns.begin();
    eventvec_t::iterator itEnd = m_vNoteOns.end();
    // Want lower bound to minimize simultaneous complexity
    eventvec_t::iterator itMiddle = lower_bound( itBegin, itEnd, pair< long long, int >( llStartTime, 0 ) );

    // Start position
    m_iStartPos = m_iLearnPos = (int)m_vEvents.size();
    if ( itMiddle != itEnd && itMiddle->second < m_iStartPos )
        m_iStartPos = m_iLearnPos = itMiddle->second;
    eventvec_t::iterator itNonNote = lower_bound( m_vNonNotes.begin(), m_vNonNotes.end(), pair< long long, int >( llStartTime, 0 ) );
    if ( itNonNote != m_vNonNotes.end() && itNonNote->second < m_iStartPos )
        m_iStartPos = m_iLearnPos = itNonNote->second;

    // Find the notes that occur simultaneously with the previous note on
    m_vState.clear();
    memset( m_pNoteState, -1, sizeof( m_pNoteState ) );
    if ( itMiddle != itBegin )
    {
        eventvec_t::iterator itPrev = itMiddle - 1;
        int iFound = 0;
        int iSimultaneous = m_vEvents[itPrev->second]->GetSimultaneous() + 1;
        for ( eventvec_t::reverse_iterator it( itMiddle ); iFound < iSimultaneous && it != m_vNoteOns.rend(); ++it )
        {
            MIDIChannelEvent *pEvent = m_vEvents[ it->second ];
            MIDIChannelEvent *pSister = pEvent->GetSister();
            if ( pSister->GetAbsMicroSec() > itPrev->first ) // > because itMiddle is the max for its time
                iFound++;
            if ( pSister->GetAbsMicroSec() > llStartTime ) // > because we don't care about simultaneous ending notes
            {
                m_vState.push_back( it->second );
                pEvent->SetInputQuality( MIDIChannelEvent::Ignore );
                if ( m_pNoteState[pEvent->GetParam1()] < 0 )
                    m_pNoteState[pEvent->GetParam1()] = it->second;
            }
        }
        reverse( m_vState.begin(), m_vState.end() );
    }

    // End position: a little tricky. Same as logic code. Only needed for paused jumping.
    m_iEndPos = m_iStartPos - 1;
    int iEventCount = (int)m_vEvents.size();
    while ( m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsMicroSec() < llEndTime )
    {
        MIDIChannelEvent *pEvent = m_vEvents[m_iEndPos + 1];
        if ( m_InDevice.IsOpen() && pEvent->GetAbsMicroSec() >= m_llMinTime && 
            ( m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bScored || 
              ( m_eGameMode == Learn && m_iLearnOrdinal < 0 ) ) )
            pEvent->SetInputQuality( MIDIChannelEvent::OnRadar );
        else
            pEvent->SetInputQuality( MIDIChannelEvent::Ignore );
        m_iEndPos++;
    }

    // Input position, iterators, tick
    FindInputPos();
    eventvec_t::const_iterator itOldProgramChange = m_itNextProgramChange;
    AdvanceIterators( llStartTime, true );
    PlaySkippedEvents( itOldProgramChange );
    m_iStartTick = GetCurrentTick( m_llStartTime );

    if ( bUpdateGUI )
    {
        static PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();
        long long llNewPos = ( ( m_llStartTime - llFirstTime ) * 1000 ) / ( llLastTime - llFirstTime );
        cPlayback.SetPosition( static_cast< int >( llNewPos ) );
    }
}

// Reset input pos upon change of speed or jump in start time
// Linear search because we're already so close. Much faster than a logN lookup for almost all songs
void MainScreen::FindInputPos()
{
    long long llInputSpan = static_cast< long long >( GameScore::OkTime * m_dSpeed );

    long long llStartInputTime = m_llStartTime - llInputSpan;
    m_iStartInputPos = m_iStartPos;
    while ( m_iStartInputPos - 1 >= 0 && m_vEvents[m_iStartInputPos - 1]->GetAbsMicroSec() > llStartInputTime )
    {
        m_vEvents[m_iStartInputPos - 1]->SetInputQuality( MIDIChannelEvent::Ignore );
        m_iStartInputPos--;
    }

    long long llEndInputTime = m_llStartTime + llInputSpan;
    m_iEndInputPos = m_iStartPos - 1;
    int iEventCount = (int)m_vEvents.size();
    while ( m_iEndInputPos + 1 < iEventCount && m_vEvents[m_iEndInputPos + 1]->GetAbsMicroSec() < llEndInputTime )
        m_iEndInputPos++;
}

// Plays skipped program change and controller events. Only plays the one's needed.
// Linear search assumes a small number of events in the file. Better than 128 maps :/
void MainScreen::PlaySkippedEvents( eventvec_t::const_iterator itOldProgramChange )
{
    if ( itOldProgramChange == m_itNextProgramChange )
        return;

    // Lookup tables to see if we've got an event for a given control or program. faster than map or hash_map.
    bool aControl[16][128], aProgram[16];
    memset( aControl, 0, sizeof( aControl ) );
    memset( aProgram, 0, sizeof( aProgram ) );

    // Go from one before the next to the beginning backwards. iterators are so verbose :/
    vector< MIDIChannelEvent* > vControl;
    eventvec_t::const_reverse_iterator itBegin = eventvec_t::const_reverse_iterator( m_itNextProgramChange );
    eventvec_t::const_reverse_iterator itEnd = m_vProgramChange.rend();
    if ( itOldProgramChange < m_itNextProgramChange ) itEnd = eventvec_t::const_reverse_iterator( itOldProgramChange );

    for ( eventvec_t::const_reverse_iterator it = itBegin; it != itEnd; ++it )
    {
        MIDIChannelEvent *pEvent = m_vEvents[it->second];
        // Order matters because some events affect others, thus store for later use
        if ( pEvent->GetChannelEventType() == MIDIChannelEvent::Controller &&
             !aControl[pEvent->GetChannel()][pEvent->GetParam1()] )
        {
            aControl[pEvent->GetChannel()][pEvent->GetParam1()] = true;
            vControl.push_back( m_vEvents[it->second] );
        }
        // Order doesn't matter. Just play as you go by.
        else if ( pEvent->GetChannelEventType() == MIDIChannelEvent::ProgramChange &&
                  !aProgram[pEvent->GetChannel()] )
        {
            aProgram[pEvent->GetChannel()] = true;
            m_OutDevice.PlayEvent( pEvent->GetEventCode(), pEvent->GetParam1(), pEvent->GetParam2() );
        }
    }

    // Finally play the controller events. vControl is in reverse time order
    for ( vector< MIDIChannelEvent* >::reverse_iterator it = vControl.rbegin(); it != vControl.rend(); ++it )
        m_OutDevice.PlayEvent( ( *it )->GetEventCode(), ( *it )->GetParam1(), ( *it )->GetParam2() );
}

// Advance program change, tempo, and signature
void MainScreen::AdvanceIterators( long long llTime, bool bIsJump )
{
    if ( bIsJump )
    {
        m_itNextProgramChange = upper_bound( m_vProgramChange.begin(), m_vProgramChange.end(), pair< long long, int >( llTime, m_vEvents.size() ) );

        m_itNextTempo = upper_bound( m_vTempo.begin(), m_vTempo.end(), pair< long long, int >( llTime, m_vMetaEvents.size() ) );
        MIDIMetaEvent *pPrevious = GetPrevious( m_itNextTempo, m_vTempo, 3 );
        if ( pPrevious )
        {
            MIDI::Parse24Bit( pPrevious->GetData(), 3, &m_iMicroSecsPerBeat );
            m_iLastTempoTick = pPrevious->GetAbsT();
            m_llLastTempoTime = pPrevious->GetAbsMicroSec();
        }
        else
        {
            m_iMicroSecsPerBeat = 500000;
            m_llLastTempoTime = m_iLastTempoTick = 0;
        }

        m_itNextSignature = upper_bound( m_vSignature.begin(), m_vSignature.end(), pair< long long, int >( llTime, m_vMetaEvents.size() ) );
        pPrevious = GetPrevious( m_itNextSignature, m_vSignature, 4 );
        if ( pPrevious )
        {
            m_iBeatsPerMeasure = pPrevious->GetData()[0];
            m_iBeatType = 1 << pPrevious->GetData()[1];
            m_iClocksPerMet = pPrevious->GetData()[2];
            m_iLastSignatureTick = pPrevious->GetAbsT();
        }
        else
        {
            m_iBeatsPerMeasure = 4;
            m_iBeatType = 4;
            m_iClocksPerMet = 24;
            m_iLastSignatureTick = 0;
        }

        m_iNextBeatTick = GetBeatTick( GetCurrentTick( llTime ), m_iBeatType, m_iLastSignatureTick );
        m_iNextMetTick = GetMetTick( GetCurrentTick( llTime ), m_iClocksPerMet, m_iLastSignatureTick );
    }
    else
    {
        while ( m_itNextProgramChange != m_vProgramChange.end() && m_itNextProgramChange->first <= llTime )
            ++m_itNextProgramChange;
        for ( ; m_itNextTempo != m_vTempo.end() && m_itNextTempo->first <= llTime; ++m_itNextTempo )
        {
            MIDIMetaEvent *pEvent = m_vMetaEvents[m_itNextTempo->second];
            if ( pEvent->GetDataLen() == 3 )
            {
                MIDI::Parse24Bit( pEvent->GetData(), 3, &m_iMicroSecsPerBeat );
                m_iLastTempoTick = pEvent->GetAbsT();
                m_llLastTempoTime = pEvent->GetAbsMicroSec();
            }
        }
        for ( ; m_itNextSignature != m_vSignature.end() && m_itNextSignature->first <= llTime; ++m_itNextSignature )
        {
            MIDIMetaEvent *pEvent = m_vMetaEvents[m_itNextSignature->second];
            if ( pEvent->GetDataLen() == 4 )
            {
                m_iBeatsPerMeasure = pEvent->GetData()[0];
                m_iBeatType = 1 << pEvent->GetData()[1];
                m_iClocksPerMet = pEvent->GetData()[2];
                m_iLastSignatureTick = pEvent->GetAbsT();
            }
        }
    }
}

// Might change the value of itCurrent
MIDIMetaEvent* MainScreen::GetPrevious( eventvec_t::const_iterator &itCurrent,
                                        const eventvec_t &vEventMap, int iDataLen )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    eventvec_t::const_iterator it = itCurrent;
    if ( itCurrent != vEventMap.begin() )
    {
        while ( it != vEventMap.begin() )
            if ( m_vMetaEvents[( --it )->second]->GetDataLen() == iDataLen )
                return m_vMetaEvents[it->second];
    }
    else if ( vEventMap.size() > 0 && itCurrent->first <= mInfo.llFirstNote && m_vMetaEvents[itCurrent->second]->GetDataLen() == iDataLen )
    {
        MIDIMetaEvent *pPrevious = m_vMetaEvents[itCurrent->second];
        ++itCurrent;
        return pPrevious;
    }
    return NULL;
}

void MainScreen::NextTrack()
{
    if ( m_eGameMode != Learn ) return;
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    // Reset and undo old one
    InitLearning();
    if ( m_iLearnOrdinal >= 0 )
        m_vTrackSettings[m_iLearnTrack].aChannels[m_iLearnChannel].bScored = 
            m_vTrackSettings[m_iLearnTrack].aChannels[m_iLearnChannel].bMuted = false;

    // All tracks
    m_iLearnOrdinal++;
    m_iLearnChannel++;
    if ( m_iLearnOrdinal == mInfo.iNumChannels || mInfo.iNumChannels == 1 )
    {
        m_iLearnTrack = 0;
        m_iLearnOrdinal = m_iLearnChannel = -1;
    }
    // Find the next track
    else
    {
        bool bFound = false;
        for ( ; m_iLearnTrack < mInfo.iNumTracks; m_iLearnTrack++ )
        {
            const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[m_iLearnTrack]->GetInfo();
            for ( ; m_iLearnChannel < 16; m_iLearnChannel++ )
                if ( mTrackInfo.aNoteCount[m_iLearnChannel] > 0 )
                {
                    m_vTrackSettings[m_iLearnTrack].aChannels[m_iLearnChannel].bScored = 
                        m_vTrackSettings[m_iLearnTrack].aChannels[m_iLearnChannel].bMuted = bFound = true;
                    break;
                }
            if ( bFound ) break;
            m_iLearnChannel = 0;
        }
    }

    // Change the note status
    for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
    {
        MIDIChannelEvent *pEvent = m_vEvents[i];
        if ( m_InDevice.IsOpen() && pEvent->GetAbsMicroSec() >= m_llMinTime && 
            ( m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bScored || 
              ( m_eGameMode == Learn && m_iLearnOrdinal < 0 ) ) )
            pEvent->SetInputQuality( MIDIChannelEvent::OnRadar );
        else
            pEvent->SetInputQuality( MIDIChannelEvent::Ignore );
    }
}

bool MainScreen::DoTransition( long long llElapsed, long long llOldStartTime )
{
    if ( ( m_eGameMode != Learn || m_eLearnMode != Adaptive ) && m_llEndLoop < m_llTransitionTime ) return false;
    bool bGoodJob = false;

    // Update transition state and return
    if ( m_bInTransition )
    {
        bool bFadeOut = ( m_iNotesTime < TransitionTime );
        m_iNotesTime += static_cast< int >( llElapsed );
        if ( m_iNotesTime >= 2 * TransitionTime )
        {
            InitLearning( false );
            m_bForceWait = m_InDevice.IsOpen();
            return false;
        }
        if ( bFadeOut && m_iNotesTime >= TransitionTime )
        {
            m_llMinTime = m_llTransitionTime;
            JumpTo( m_llMinTime - static_cast< long long>( TransitionTime * m_dSpeed ), true, false );
        }
        m_iNotesAlpha = ( ( -abs( m_iNotesTime - TransitionTime ) + TransitionTime ) * 255 ) / TransitionTime;
        return true;
    }
    // We haven't hit the 7.5 seconds or whatever it is to make a determination yet
    else if ( m_llTransitionTime < GetMinTime() )
        return false;
    // Learning mode check
    else if ( m_eGameMode == Learn && m_eLearnMode == Adaptive )
    {
        if ( m_cbLastNotes.empty() )
            return false;
        else if ( m_eLearnMode == Adaptive && ( m_iGoodCount * 100 ) / m_cbLastNotes.size() <= TransitionPct )
        {
            if ( m_dSpeed < 0.9999 ) bGoodJob = true;
            else return false;
        }
    }
    // Did we pass the loop end?
    else if ( llOldStartTime > m_llEndLoop || m_llStartTime <= m_llEndLoop )
        return false;

    // Definitely going to transition. Kick of a message indicating as such.
    if ( m_eGameMode != Learn || m_eLearnMode != Adaptive )
    {
        wcscpy_s( m_sBuf, L"Looping..." );
        m_tpMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, 0x00FFFFFF, m_sBuf );
    }
    else
    {
        PlaybackSettings &cPlayback = Config::GetConfig().GetPlaybackSettings();
        if ( bGoodJob )
        {
            cPlayback.SetSpeed( min( m_dSpeed / 0.80, 1.0 ), true );
            wcscpy_s( m_sBuf, L"Good job!\nTry again faster!" );
            m_tpLongMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f, 0x00FFFFFF, m_sBuf );
        }
        else
        {
            cPlayback.SetSpeed( max( m_dSpeed * 0.80, 0.25 ), true );
            wcscpy_s( m_sBuf, L"Try again with fewer mistakes!" );
            m_tpLongMessage.Reset( m_pRenderer->GetBufferWidth() / 2.0f, m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) / 2.0f - 35.0f / 2.0f, 0x00FFFFFF, m_sBuf );
        }
    }

    m_bInTransition = true;
    return true;
}

bool MainScreen::DoWaiting( long long llNextStartTime, long long llElapsed )
{
    bool bWait = false;
    int iEventCount = (int)m_vEvents.size();
    int iNewStartPos = m_iStartPos;
    if ( ( m_eGameMode == Learn && m_eLearnMode == Waiting ) || m_bForceWait )
        while ( iNewStartPos < iEventCount && m_vEvents[iNewStartPos]->GetAbsMicroSec() <= llNextStartTime )
        {
            MIDIChannelEvent *pEvent = m_vEvents[iNewStartPos];
            if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn && pEvent->GetParam2() > 0 &&
                 ( pEvent->GetInputQuality() == MIDIChannelEvent::OnRadar || pEvent->GetInputQuality() == MIDIChannelEvent::Waiting ||
                   m_eGameMode != Learn || m_eLearnMode != Waiting ) )
            {
                if ( !bWait )
                {
                    llElapsed -= pEvent->GetAbsMicroSec() - 1 - m_llStartTime;
                    m_llStartTime = pEvent->GetAbsMicroSec() - 1;
                }
                pEvent->SetInputQuality( MIDIChannelEvent::Waiting );
                bWait = true;
            }
            iNewStartPos++;
        }

    if ( bWait )
    {
        m_iWaitingTime += static_cast< int >( llElapsed );
        if ( m_iWaitingTime > FlashTime * 2 ) m_iWaitingTime = 0;
        m_iWaitingAlpha = ( ( -abs( m_iWaitingTime - FlashTime ) + FlashTime ) * 255 ) / FlashTime;
    }
    else
        m_iWaitingAlpha = 0;
    return bWait;
}

// Gets the tick corresponding to llStartTime using current tempo
int  MainScreen::GetCurrentTick( long long llStartTime )
{
    return GetCurrentTick( llStartTime, m_iLastTempoTick, m_llLastTempoTime, m_iMicroSecsPerBeat );
}

int  MainScreen::GetCurrentTick( long long llStartTime, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat )
{
    int iDivision = m_MIDI.GetInfo().iDivision;
    if ( !( iDivision & 0x8000 ) )
    {
        if ( llStartTime >= llLastTempoTime )
            return iLastTempoTick + static_cast< int >( ( iDivision * ( llStartTime - llLastTempoTime ) ) / iMicroSecsPerBeat );
        else 
            return iLastTempoTick - static_cast< int >( ( iDivision * ( llLastTempoTime - llStartTime ) + 1 ) / iMicroSecsPerBeat ) - 1;
    }
    return -1;
}

// Gets the time corresponding to the tick
long long MainScreen::GetTickTime( int iTick )
{
    return GetTickTime( iTick, m_iLastTempoTick, m_llLastTempoTime, m_iMicroSecsPerBeat );
}

long long MainScreen::GetTickTime( int iTick, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat )
{
    int iDivision = m_MIDI.GetInfo().iDivision;
    if ( !( iDivision & 0x8000 ) )
        return llLastTempoTime + ( static_cast< long long >( iMicroSecsPerBeat ) * ( iTick - iLastTempoTick ) ) / iDivision;
    //else
    //    return llLastTempoTime + ( 1000000LL * ( iTick - iLastTempoTick ) ) / iTicksPerSecond;
    return -1;
}

// Rounds up to the nearest beat
int MainScreen::GetBeat( int iTick, int iBeatType, int iLastSignatureTick )
{
    int iDivision = m_MIDI.GetInfo().iDivision;
    int iTickOffset = iTick - iLastSignatureTick;
    if ( !( iDivision & 0x8000 ) )
    {
        if ( iTickOffset > 0 )
            return ( iTickOffset * iBeatType - 1 ) / ( iDivision  * 4 ) + 1;
        else
            return ( iTickOffset * iBeatType ) / ( iDivision  * 4 );
    }
    return -1;
}

// Rounds up to the nearest beat
int MainScreen::GetBeatTick( int iTick, int iBeatType, int iLastSignatureTick )
{
    int iDivision = m_MIDI.GetInfo().iDivision;
    if ( !( iDivision & 0x8000 ) )
        return iLastSignatureTick + ( GetBeat( iTick, iBeatType, iLastSignatureTick ) * iDivision * 4 ) / iBeatType;
    return -1;
}

// Rounds up to the nearest metronome tick
int MainScreen::GetMetTick( int iTick, int iClocksPerMet, int iLastSignatureTick )
{
    int iDivision = m_MIDI.GetInfo().iDivision;
    int iTickOffset = iTick - iLastSignatureTick;
    if ( !( iDivision & 0x8000 ) )
    {
        int iMet = 0;
        if ( iTickOffset > 0 )
            iMet = ( ( iTickOffset * 24 - 1 ) / ( iDivision  * iClocksPerMet ) + 1 );
        else
            iMet = ( iTickOffset * 24 ) / ( iDivision  * iClocksPerMet );
        return iLastSignatureTick + ( iMet * iClocksPerMet * iDivision ) / 24;
    }
    return -1;
}

const float MainScreen::SharpRatio = 0.65f;
const float MainScreen::KBPercent = 0.25f;
const float MainScreen::KeyRatio = 0.1775f;

GameState::GameError MainScreen::Render() 
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    m_pRenderer->Clear( 0x00000000 );

    m_pRenderer->BeginScene();
    RenderLines();
    RenderNotes();
    RenderLabels();
    if ( m_bShowKB )
        RenderKeys();
    RenderBorder();
    RenderText();
    m_pRenderer->EndScene();

    // Present the backbuffer contents to the display
    m_pRenderer->Present();
    return Success;
}

// These used to be created as local variables inside each Render* function, but too much copying of code :/
// Depends on m_llStartTime, m_llTimeSpan, m_eKeysShown, m_iStartNote, m_iEndNote
void MainScreen::RenderGlobals()
{
    // Midi info
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    if ( m_eKeysShown == VisualSettings::All )
    {
        m_iStartNote = min( m_iStartNote, MIDI::A0 );
        m_iEndNote = max( m_iEndNote, MIDI::C8 );
    }
    else if ( m_eKeysShown == VisualSettings::Song )
    {
        m_iStartNote = mInfo.iMinNote;
        m_iEndNote = mInfo.iMaxNote;
    }

    // Screen X info
    m_fNotesX = m_fOffsetX + m_fTempOffsetX;
    m_fNotesCX = m_pRenderer->GetBufferWidth() * m_fZoomX * m_fTempZoomX;

    // Keys info
    m_iAllWhiteKeys = MIDI::WhiteCount( m_iStartNote, m_iEndNote + 1 );
    float fBuffer = ( MIDI::IsSharp( m_iStartNote ) ? SharpRatio / 2.0f : 0.0f ) +
                    ( MIDI::IsSharp( m_iEndNote ) ? SharpRatio / 2.0f : 0.0f );
    m_fWhiteCX = m_fNotesCX / ( m_iAllWhiteKeys + fBuffer );

    // Screen Y info
    m_fNotesY = m_fOffsetY + m_fTempOffsetY;
    if ( !m_bShowKB )
        m_fNotesCY = static_cast< float >( m_pRenderer->GetBufferHeight() );
    else
    {
        float fMaxKeyCY = m_pRenderer->GetBufferHeight() * KBPercent;
        float fIdealKeyCY = m_fWhiteCX / KeyRatio;
        // .95 for the top vs near. 2.0 for the spacer. .93 for the transition and the red. ESTIMATE.
        fIdealKeyCY = ( fIdealKeyCY / 0.95f + 2.0f ) / 0.93f;
        m_fNotesCY = floor( m_pRenderer->GetBufferHeight() - min( fIdealKeyCY, fMaxKeyCY ) + 0.5f );
    }

    // Round down start time. This is only used for rendering purposes
    long long llMicroSecsPP = static_cast< long long >( m_llTimeSpan / m_fNotesCY + 0.5f );
    m_llRndStartTime = m_llStartTime - ( m_llStartTime < 0 ? llMicroSecsPP : 0 );
    m_llRndStartTime = ( m_llRndStartTime / llMicroSecsPP ) * llMicroSecsPP;
}

void MainScreen::RenderLines()
{
    m_pRenderer->DrawRect( m_fNotesX, m_fNotesY, m_fNotesCX, m_fNotesCY, m_csBackground.iPrimaryRGB );

    // Vertical lines
    for ( int i = m_iStartNote + 1; i <= m_iEndNote; i++ )
        if ( !MIDI::IsSharp( i - 1 ) && !MIDI::IsSharp( i ) )
        {
            int iWhiteKeys = MIDI::WhiteCount( m_iStartNote, i );
            float fStartX = MIDI::IsSharp( m_iStartNote ) * SharpRatio / 2.0f;
            float x = m_fNotesX + m_fWhiteCX * ( iWhiteKeys + fStartX );
            x = floor( x + 0.5f ); // Needs to be rounded because of the gradient
            m_pRenderer->DrawRect( x - 1.0f, m_fNotesY, 3.0f, m_fNotesCY,
                m_csBackground.iDarkRGB, m_csBackground.iVeryDarkRGB, m_csBackground.iVeryDarkRGB, m_csBackground.iDarkRGB );
        }

    // Horizontal (Hard!)
    int iDivision = m_MIDI.GetInfo().iDivision;
    if ( !( iDivision & 0x8000 ) )
    {
        // Copy time state vars
        int iCurrTick = m_iStartTick - 1;
        long long llEndTime = m_llStartTime + m_llTimeSpan;

        // Copy tempo state vars
        int iLastTempoTick = m_iLastTempoTick;
        int iMicroSecsPerBeat = m_iMicroSecsPerBeat;
        long long llLastTempoTime = m_llLastTempoTime;
        eventvec_t::const_iterator itNextTempo = m_itNextTempo;

        // Copy signature state vars
        int iLastSignatureTick = m_iLastSignatureTick;
        int iBeatsPerMeasure = m_iBeatsPerMeasure;
        int iBeatType = m_iBeatType;
        eventvec_t::const_iterator itNextSignature = m_itNextSignature;

        // Compute initial next beat tick and next beat time
        long long llNextBeatTime = 0;
        do
        {
            int iNextBeatTick = GetBeatTick( iCurrTick + 1, iBeatType, iLastSignatureTick );

            // Next beat crosses the next tempo event. handle the event and recalculate next beat time
            while ( itNextTempo != m_vTempo.end() && m_vMetaEvents[itNextTempo->second]->GetDataLen() == 3 &&
                    iNextBeatTick > m_vMetaEvents[itNextTempo->second]->GetAbsT() )
            {
                MIDIMetaEvent *pEvent = m_vMetaEvents[itNextTempo->second];
                MIDI::Parse24Bit( pEvent->GetData(), 3, &iMicroSecsPerBeat );
                iLastTempoTick = pEvent->GetAbsT();
                llLastTempoTime = pEvent->GetAbsMicroSec();
                ++itNextTempo;
            }
            while ( itNextSignature != m_vSignature.end() && m_vMetaEvents[itNextSignature->second]->GetDataLen() == 4 &&
                    iNextBeatTick > m_vMetaEvents[itNextSignature->second]->GetAbsT() )
            {
                MIDIMetaEvent *pEvent = m_vMetaEvents[itNextSignature->second];
                iBeatsPerMeasure = pEvent->GetData()[0];
                iBeatType = 1 << pEvent->GetData()[1];
                iLastSignatureTick = pEvent->GetAbsT();
                iNextBeatTick = GetBeatTick( iLastSignatureTick + 1, iBeatType, iLastSignatureTick );
                ++itNextSignature;
            }

            // Finally render the beat or measure
            int iNextBeat = GetBeat( iNextBeatTick, iBeatType, iLastSignatureTick );
            bool bIsMeasure = !( ( iNextBeat < 0 ? -iNextBeat : iNextBeat ) % iBeatsPerMeasure );
            llNextBeatTime = GetTickTime( iNextBeatTick, iLastTempoTick, llLastTempoTime, iMicroSecsPerBeat ); 
            float y = m_fNotesY + m_fNotesCY * ( 1.0f - static_cast< float >( llNextBeatTime - m_llRndStartTime ) / m_llTimeSpan );
            y = floor( y + 0.5f );
            if ( bIsMeasure && y + 1.0f > m_fNotesY )
                m_pRenderer->DrawRect( m_fNotesX, y - 1.0f, m_fNotesCX, 3.0f,
                    m_csBackground.iDarkRGB, m_csBackground.iDarkRGB, m_csBackground.iVeryDarkRGB, m_csBackground.iVeryDarkRGB );

            iCurrTick = iNextBeatTick;
        }
        while ( llNextBeatTime <= llEndTime );
    }
}

void MainScreen::RenderNotes()
{
    // Do we have any notes to render?
    if ( m_iEndPos < 0 || m_iStartPos >= static_cast< int >( m_vEvents.size() ) )
        return;

    // Render notes. Regular notes then sharps to  make sure they're not hidden
    bool bHasSharp = false;
    for ( vector< int >::iterator it = m_vState.begin(); it != m_vState.end(); ++it )
        if ( !MIDI::IsSharp( m_vEvents[*it]->GetParam1() ) )
            RenderNote( *it );
        else
            bHasSharp = true;

    for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
    {
        MIDIChannelEvent *pEvent = m_vEvents[i];
        if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
             pEvent->GetParam2() > 0 && pEvent->GetSister() )
        {
            if ( !MIDI::IsSharp( pEvent->GetParam1() ) )
                RenderNote( i );
            else
                bHasSharp = true;
        }
    }

    // Do it all again, but only for the sharps
    if ( bHasSharp )
    {
        for ( vector< int >::iterator it = m_vState.begin(); it != m_vState.end(); ++it )
            if ( MIDI::IsSharp( m_vEvents[*it]->GetParam1() ) )
                RenderNote( *it );

        for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
        {
            MIDIChannelEvent *pEvent = m_vEvents[i];
            if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
                 pEvent->GetParam2() > 0 && pEvent->GetSister() &&
                 MIDI::IsSharp( pEvent->GetParam1() ) )
                RenderNote( i );                
        }
    }
}

void MainScreen::RenderNote( int iPos )
{
    const MIDIChannelEvent *pNote = m_vEvents[iPos];
    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    MIDIChannelEvent::InputQuality eInputQuality = pNote->GetInputQuality();
    long long llNoteStart = pNote->GetAbsMicroSec();
    long long llNoteEnd = pNote->GetSister()->GetAbsMicroSec();

    bool bBadLearn = ( m_eGameMode == Learn && m_iLearnOrdinal >= 0 && ( iTrack != m_iLearnTrack || iChannel != m_iLearnChannel ) );
    ChannelSettings &csTrack = ( eInputQuality == MIDIChannelEvent::Missed || bBadLearn ? m_csKBBadNote :
                                 m_vTrackSettings[iTrack].aChannels[iChannel] );
    if ( m_vTrackSettings[iTrack].aChannels[iChannel].bHidden ) return;

    // Compute true positions
    float x = GetNoteX( iNote );
    float y = m_fNotesY + m_fNotesCY * ( 1.0f - static_cast< float >( llNoteStart - m_llRndStartTime ) / m_llTimeSpan );
    float cx =  MIDI::IsSharp( iNote ) ? m_fWhiteCX * SharpRatio : m_fWhiteCX;
    float cy = m_fNotesCY * ( static_cast< float >( llNoteEnd - llNoteStart ) / m_llTimeSpan );
    float fDeflate = m_fWhiteCX * 0.15f / 2.0f;

    // Rounding to make everything consistent
    cy = floor( cy + 0.5f ); // constant cy across rendering
    y = floor( y + 0.5f );
    fDeflate = floor( fDeflate + 0.5f );
    fDeflate = max( min( fDeflate, 3.0f ), 1.0f );

    // Clipping :/
    float fMinY = m_fNotesY - 5.0f;
    float fMaxY = m_fNotesY + m_fNotesCY + 5.0f;
    if ( y > fMaxY )
    {
        cy -= ( y - fMaxY );
        y = fMaxY;
    }
    if ( y - cy < fMinY )
    {
        cy -= ( fMinY - ( y - cy ) );
        y = fMinY + cy;
    }

    if ( m_ptLastPos.x >= x && m_ptLastPos.x <= x + cx &&
         m_ptLastPos.y <= y && m_ptLastPos.y >= y - cy )
        m_iNextHotNote = iPos;

    // Visualize!
    int iAlpha = ( eInputQuality == MIDIChannelEvent::Waiting ? m_iWaitingAlpha : m_iNotesAlpha ) << 24;
    if ( m_bPaused && m_bHaveMouse && iPos == m_iHotNote && !m_bZoomMove && ( m_iSelectedNote == -1 || m_iSelectedNote == iPos ) )
    {
        m_pRenderer->DrawRect( x, y - cy, cx, cy, csTrack.iPrimaryRGB | iAlpha );
        m_pRenderer->DrawRect( x + fDeflate, y - cy + fDeflate,
                                cx - fDeflate * 2.0f, cy - fDeflate * 2.0f,
                                csTrack.iVeryDarkRGB | iAlpha, csTrack.iDarkRGB | iAlpha, csTrack.iDarkRGB | iAlpha, csTrack.iVeryDarkRGB | iAlpha );
    }
    else if ( llNoteStart < m_llMinTime && !bBadLearn )
    {
        m_pRenderer->DrawRect( x, y - cy, fDeflate, cy, csTrack.iVeryDarkRGB | iAlpha );
        m_pRenderer->DrawRect( x, y - cy, cx, fDeflate, csTrack.iVeryDarkRGB | iAlpha );
        m_pRenderer->DrawRect( x + cx - fDeflate, y - cy, fDeflate, cy, csTrack.iVeryDarkRGB | iAlpha );
        m_pRenderer->DrawRect( x, y - fDeflate, cx, fDeflate, csTrack.iVeryDarkRGB | iAlpha );
    }
    else
    {
        m_pRenderer->DrawRect( x, y - cy, cx, cy, csTrack.iVeryDarkRGB | iAlpha );
        m_pRenderer->DrawRect( x + fDeflate, y - cy + fDeflate,
                                cx - fDeflate * 2.0f, cy - fDeflate * 2.0f,
                                csTrack.iPrimaryRGB | iAlpha, csTrack.iDarkRGB | iAlpha, csTrack.iDarkRGB | iAlpha, csTrack.iPrimaryRGB | iAlpha );
    }
}

// Similar to RenderNotes. It's not in that function because text is done separate.
void MainScreen::RenderLabels()
{
    // Do we have any notes to render?
    if ( m_iEndPos < 0 || m_iStartPos >= static_cast< int >( m_vEvents.size() ) )
        return;

    bool bSetState = true;
    for ( vector< int >::iterator it = m_vState.begin(); it != m_vState.end(); ++it )
        bSetState &= !RenderLabel( *it, bSetState );

    for ( int i = m_iStartPos; i <= m_iEndPos; i++ )
    {
        MIDIChannelEvent *pEvent = m_vEvents[i];
        if ( pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
             pEvent->GetParam2() > 0 && pEvent->GetSister() )
            bSetState &= !RenderLabel( i, bSetState );
    }

    for ( int i = 0; i < 128; i++ )
        if ( m_tpParticles[i].IsAlive() )
        {
            if ( bSetState ) m_pRenderer->BeginText();
            m_tpParticles[i].Render( m_pRenderer, 0.0f, m_fNotesY + m_fNotesCY );
            bSetState = false;
        }

    if ( !bSetState ) m_pRenderer->EndText();
}

bool MainScreen::RenderLabel( int iPos, bool bSetState )
{
    const MIDIChannelEvent *pNote = m_vEvents[iPos];

    const string *sLabel = pNote->GetLabel();
    int iLabels = ( m_bNoteLabels ? 1 : 0 ) + ( sLabel && sLabel->length() > 0 ? 1 : 0 );
    if ( !iLabels ) return false;

    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    MIDIChannelEvent::InputQuality eInputQuality = pNote->GetInputQuality();
    long long llNoteStart = pNote->GetAbsMicroSec();
    ChannelSettings &csTrack = ( eInputQuality == MIDIChannelEvent::Missed ? m_csKBBadNote :
                                 m_vTrackSettings[iTrack].aChannels[iChannel] );
    if ( m_vTrackSettings[iTrack].aChannels[iChannel].bHidden ) return false;

    // Compute true positions
    float x = GetNoteX( iNote );
    float y = m_fNotesY + m_fNotesCY * ( 1.0f - static_cast< float >( llNoteStart - m_llRndStartTime ) / m_llTimeSpan );
    float cx =  MIDI::IsSharp( iNote ) ? m_fWhiteCX * SharpRatio : m_fWhiteCX;

    float fMaxY = m_fNotesY + m_fNotesCY + 3.0f + 15.0f * iLabels;
    if ( y > fMaxY ) return false;

    y = floor( y + 0.5f );
    RECT rc = { static_cast< int >( x + cx / 2.0f + 0.5f ), static_cast< int >( y - ( 3.0f + 15.0f * iLabels ) ), 0, 0 };
    rc.right = rc.left;

    if ( bSetState ) m_pRenderer->BeginText();

    int iAlpha = ( 0xFF - ( eInputQuality == MIDIChannelEvent::Waiting ? m_iWaitingAlpha : m_iNotesAlpha ) ) << 24;
    if ( sLabel && sLabel->length() > 0 )
    {
        OffsetRect( &rc, -1, -1 );
        m_pRenderer->DrawTextA( sLabel->c_str(), Renderer::SmallBold, &rc, DT_CENTER | DT_NOCLIP, csTrack.iVeryDarkRGB | iAlpha );
        OffsetRect( &rc, 1, 1 );
        m_pRenderer->DrawTextA( sLabel->c_str(), Renderer::SmallBold, &rc, DT_CENTER | DT_NOCLIP, 0x00FFFFFF | iAlpha );
        OffsetRect( &rc, 0, 15 );
    }
    if ( m_bNoteLabels )
    {
        OffsetRect( &rc, -1, -1 );
        const wstring &sLabel = MIDI::NoteName( iNote );
        m_pRenderer->DrawTextW( sLabel.c_str(), Renderer::SmallBold, &rc, DT_CENTER | DT_NOCLIP, csTrack.iVeryDarkRGB | iAlpha, (int)sLabel.length() - 1 );
        OffsetRect( &rc, 1, 1 );
        m_pRenderer->DrawTextW( sLabel.c_str(), Renderer::SmallBold, &rc, DT_CENTER | DT_NOCLIP, 0x00FFFFFF | iAlpha, (int)sLabel.length() - 1 );
    }

    return true;
}

float MainScreen::GetNoteX( int iNote )
{
    int iWhiteKeys = MIDI::WhiteCount( m_iStartNote, iNote );
    float fStartX = ( MIDI::IsSharp( m_iStartNote ) - MIDI::IsSharp( iNote ) ) * SharpRatio / 2.0f;
    if ( MIDI::IsSharp( iNote ) )
    {
        MIDI::Note eNote = MIDI::NoteVal( iNote );
        if ( eNote == MIDI::CS || eNote == MIDI::FS ) fStartX -= SharpRatio / 5.0f;
        else if ( eNote == MIDI::AS || eNote == MIDI::DS ) fStartX += SharpRatio / 5.0f;
    }
    return m_fNotesX + m_fWhiteCX * ( iWhiteKeys + fStartX );
}

void MainScreen::RenderKeys()
{
    // Screen info
    float fKeysY = m_fNotesY + m_fNotesCY;
    float fKeysCY = m_pRenderer->GetBufferHeight() - m_fNotesCY;

    float fTransitionPct = .02f;
    float fTransitionCY = max( 3.0f, floor( fKeysCY * fTransitionPct + 0.5f ) );
    float fRedPct = .05f;
    float fRedCY = floor( fKeysCY * fRedPct + 0.5f );
    float fSpacerCY = 2.0f;
    float fTopCY = floor( ( fKeysCY - fSpacerCY - fRedCY - fTransitionCY ) * 0.95f + 0.5f );
    float fNearCY = fKeysCY - fSpacerCY - fRedCY - fTransitionCY - fTopCY;

    // Draw the background
    m_pRenderer->DrawRect( m_fNotesX, fKeysY, m_fNotesCX, fKeysCY, m_csKBBackground.iVeryDarkRGB );
    m_pRenderer->DrawRect( m_fNotesX, fKeysY, m_fNotesCX, fTransitionCY,
        m_csBackground.iPrimaryRGB, m_csBackground.iPrimaryRGB, m_csKBBackground.iVeryDarkRGB, m_csKBBackground.iVeryDarkRGB );
    m_pRenderer->DrawRect( m_fNotesX, fKeysY + fTransitionCY, m_fNotesCX, fRedCY,
        m_csKBRed.iDarkRGB, m_csKBRed.iDarkRGB, m_csKBRed.iPrimaryRGB, m_csKBRed.iPrimaryRGB );
    m_pRenderer->DrawRect( m_fNotesX, fKeysY + fTransitionCY + fRedCY, m_fNotesCX, fSpacerCY,
        m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB );

    // Keys info
    float fKeyGap = max( 1.0f, floor( m_fWhiteCX * 0.05f + 0.5f ) );
    float fKeyGap1 = fKeyGap - floor( fKeyGap / 2.0f + 0.5f );

    int iStartRender = ( MIDI::IsSharp( m_iStartNote ) ? m_iStartNote - 1 : m_iStartNote );
    int iEndRender = ( MIDI::IsSharp( m_iEndNote ) ? m_iEndNote + 1 : m_iEndNote );
    float fStartX = ( MIDI::IsSharp( m_iStartNote ) ? m_fWhiteCX * ( SharpRatio / 2.0f - 1.0f ) : 0.0f );
    float fSharpCY = fTopCY * 0.67f;

    // Draw the white keys
    float fCurX = m_fNotesX + fStartX;
    float fCurY = fKeysY + fTransitionCY + fRedCY + fSpacerCY;
    for ( int i = iStartRender; i <= iEndRender; i++ )
        if ( !MIDI::IsSharp( i ) )
        {
            if ( m_pNoteState[i] == -1 && m_pInputState[i] == -1 )
            {
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY, m_fWhiteCX - fKeyGap, fTopCY + fNearCY,
                    m_csKBWhite.iDarkRGB, m_csKBWhite.iDarkRGB, m_csKBWhite.iPrimaryRGB, m_csKBWhite.iPrimaryRGB );
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY + fTopCY, m_fWhiteCX - fKeyGap, fNearCY,
                    m_csKBWhite.iDarkRGB, m_csKBWhite.iDarkRGB, m_csKBWhite.iVeryDarkRGB, m_csKBWhite.iVeryDarkRGB );
                m_pRenderer->DrawRect( fCurX + fKeyGap1, fCurY + fTopCY, m_fWhiteCX - fKeyGap, 2.0f,
                    m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB, m_csKBWhite.iVeryDarkRGB, m_csKBWhite.iVeryDarkRGB );

                if ( i == MIDI::C4 )
                {
                    float fMXGap = floor( m_fWhiteCX * 0.25f + 0.5f );
                    float fMCX = m_fWhiteCX - fMXGap * 2.0f - fKeyGap;
                    float fMY = max( fCurY + fTopCY - fMCX - 5.0f, fCurY + fSharpCY + 5.0f );
                    m_pRenderer->DrawRect( fCurX + fKeyGap1 + fMXGap, fMY, fMCX, fCurY + fTopCY - 5.0f - fMY, m_csKBWhite.iDarkRGB );
                }
            }
            else
            {
                const MIDIChannelEvent *pEvent = ( m_pInputState[i] >= 0 ? m_vEvents[m_pInputState[i]] : m_pNoteState[i] >= 0 ? m_vEvents[m_pNoteState[i]] : NULL );
                const int iTrack = ( pEvent ? pEvent->GetTrack() : -1 );
                const int iChannel = ( pEvent ? pEvent->GetChannel() : -1 );

                int iAlpha = m_iNotesAlpha << 24;
                if ( iAlpha )
                {
                    m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY, m_fWhiteCX - fKeyGap, fTopCY + fNearCY - 2.0f,
                        m_csKBWhite.iDarkRGB, m_csKBWhite.iDarkRGB, m_csKBWhite.iPrimaryRGB, m_csKBWhite.iPrimaryRGB );
                    m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY + fTopCY + fNearCY - 2.0f, m_fWhiteCX - fKeyGap, 2.0f, m_csKBWhite.iDarkRGB );
                }

                bool bBadLearn = ( m_eGameMode == Learn && m_iLearnOrdinal >= 0 && ( iTrack != m_iLearnTrack || iChannel != m_iLearnChannel ) );
                ChannelSettings &csKBWhite = ( m_pInputState[i] == -2 || bBadLearn ||
                                               pEvent->GetInputQuality() == MIDIChannelEvent::Missed ? m_csKBBadNote :
                                               m_vTrackSettings[iTrack].aChannels[iChannel] );
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY, m_fWhiteCX - fKeyGap, fTopCY + fNearCY - 2.0f,
                    csKBWhite.iDarkRGB | iAlpha, csKBWhite.iDarkRGB | iAlpha, csKBWhite.iPrimaryRGB | iAlpha, csKBWhite.iPrimaryRGB | iAlpha );
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY + fTopCY + fNearCY - 2.0f, m_fWhiteCX - fKeyGap, 2.0f, csKBWhite.iDarkRGB | iAlpha );

                if ( i == MIDI::C4 )
                {
                    float fMXGap = floor( m_fWhiteCX * 0.25f + 0.5f );
                    float fMCX = m_fWhiteCX - fMXGap * 2.0f - fKeyGap;
                    float fMY = max( fCurY + fTopCY + fNearCY - fMCX - 7.0f, fCurY + fSharpCY + 5.0f );
                    if ( iAlpha )
                        m_pRenderer->DrawRect( fCurX + fKeyGap1 + fMXGap, fMY, fMCX, fCurY + fTopCY + fNearCY - 7.0f - fMY, m_csKBWhite.iDarkRGB );
                    m_pRenderer->DrawRect( fCurX + fKeyGap1 + fMXGap, fMY, fMCX, fCurY + fTopCY + fNearCY - 7.0f - fMY, csKBWhite.iDarkRGB | iAlpha );
                }
            }
            m_pRenderer->DrawRect( floor( fCurX + fKeyGap1 + m_fWhiteCX - fKeyGap + 0.5f ), fCurY, fKeyGap, fTopCY + fNearCY,
                m_csKBBackground.iVeryDarkRGB, m_csKBBackground.iPrimaryRGB, m_csKBBackground.iPrimaryRGB, m_csKBBackground.iVeryDarkRGB );

            fCurX += m_fWhiteCX;
        }

    // Draw the sharps
    iStartRender = ( m_iStartNote != MIDI::A0 && !MIDI::IsSharp( m_iStartNote ) && m_iStartNote > 0 && MIDI::IsSharp( m_iStartNote - 1 ) ? m_iStartNote - 1 : m_iStartNote );
    iEndRender = ( m_iEndNote != MIDI::C8 && !MIDI::IsSharp( m_iEndNote ) && m_iEndNote < 127 && MIDI::IsSharp( m_iEndNote + 1 ) ? m_iEndNote + 1 : m_iEndNote );
    fStartX = ( MIDI::IsSharp( m_iStartNote ) ? m_fWhiteCX * SharpRatio / 2.0f : 0.0f );

    float fSharpTop = SharpRatio * 0.7f;
    fCurX = m_fNotesX + fStartX;
    fCurY = fKeysY + fTransitionCY + fRedCY + fSpacerCY;
    for ( int i = iStartRender; i <= iEndRender; i++ )
        if ( !MIDI::IsSharp( i ) )
            fCurX += m_fWhiteCX;
        else
        {
            float fNudgeX = 0.0;
            MIDI::Note eNote = MIDI::NoteVal( i );
            if ( eNote == MIDI::CS || eNote == MIDI::FS ) fNudgeX = -SharpRatio / 5.0f;
            else if ( eNote == MIDI::AS || eNote == MIDI::DS ) fNudgeX = SharpRatio / 5.0f;

            const float cx = m_fWhiteCX * SharpRatio;
            const float x = fCurX - m_fWhiteCX * ( SharpRatio / 2.0f - fNudgeX );
            const float fSharpTopX1 = x + m_fWhiteCX * ( SharpRatio - fSharpTop ) / 2.0f;
            const float fSharpTopX2 = fSharpTopX1 + m_fWhiteCX * fSharpTop;

            if ( m_pNoteState[i] == -1 && m_pInputState[i] == -1 )
            {
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY + fSharpCY - fNearCY,
                                       fSharpTopX2, fCurY + fSharpCY - fNearCY,
                                       x + cx, fCurY + fSharpCY, x, fCurY + fSharpCY,
                                       m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNearCY,
                                       fSharpTopX1, fCurY + fSharpCY - fNearCY,
                                       x, fCurY + fSharpCY, x, fCurY,
                                       m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX2, fCurY + fSharpCY - fNearCY,
                                       fSharpTopX2, fCurY - fNearCY,
                                       x + cx, fCurY, x + cx, fCurY + fSharpCY,
                                       m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                m_pRenderer->DrawRect( fSharpTopX1, fCurY - fNearCY, fSharpTopX2 - fSharpTopX1, fSharpCY, m_csKBSharp.iVeryDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNearCY,
                                       fSharpTopX2, fCurY - fNearCY,
                                       fSharpTopX2, fCurY - fNearCY + fSharpCY * 0.45f,
                                       fSharpTopX1, fCurY - fNearCY + fSharpCY * 0.35f,
                                       m_csKBSharp.iDarkRGB, m_csKBSharp.iDarkRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNearCY + fSharpCY * 0.35f,
                                       fSharpTopX2, fCurY - fNearCY + fSharpCY * 0.45f,
                                       fSharpTopX2, fCurY - fNearCY + fSharpCY * 0.65f,
                                       fSharpTopX1, fCurY - fNearCY + fSharpCY * 0.55f,
                                       m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
            }
            else
            {
                const MIDIChannelEvent *pEvent = ( m_pInputState[i] >= 0 ? m_vEvents[m_pInputState[i]] : m_pNoteState[i] >= 0 ? m_vEvents[m_pNoteState[i]] : NULL );
                const int iTrack = ( pEvent ? pEvent->GetTrack() : -1 );
                const int iChannel = ( pEvent ? pEvent->GetChannel() : -1 );

                const float fNewNear = fNearCY * 0.25f;

                const int iAlpha = m_iNotesAlpha << 24;
                if ( iAlpha )
                {
                    m_pRenderer->DrawSkew( fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                           fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                           x + cx, fCurY + fSharpCY, x, fCurY + fSharpCY,
                                           m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                    m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                           fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                           x, fCurY + fSharpCY, x, fCurY,
                                           m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                    m_pRenderer->DrawSkew( fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                           fSharpTopX2, fCurY - fNewNear,
                                           x + cx, fCurY, x + cx, fCurY + fSharpCY,
                                           m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                    m_pRenderer->DrawRect( fSharpTopX1, fCurY - fNewNear, fSharpTopX2 - fSharpTopX1, fSharpCY, m_csKBSharp.iVeryDarkRGB );
                    m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                           fSharpTopX2, fCurY - fNewNear,
                                           fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                           fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                           m_csKBSharp.iDarkRGB, m_csKBSharp.iDarkRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB );
                    m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                           fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                           fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.75f,
                                           fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.65f,
                                           m_csKBSharp.iPrimaryRGB, m_csKBSharp.iPrimaryRGB, m_csKBSharp.iVeryDarkRGB, m_csKBSharp.iVeryDarkRGB );
                }

                const bool bBadLearn = ( m_eGameMode == Learn && m_iLearnOrdinal >= 0 && ( iTrack != m_iLearnTrack || iChannel != m_iLearnChannel ) );
                const ChannelSettings &csKBSharp = ( m_pInputState[i] == -2 || bBadLearn ||
                                                     pEvent->GetInputQuality() == MIDIChannelEvent::Missed ? m_csKBBadNote :
                                                     m_vTrackSettings[iTrack].aChannels[iChannel] );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                       fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                       x + cx, fCurY + fSharpCY, x, fCurY + fSharpCY,
                                       csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                       fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                       x, fCurY + fSharpCY, x, fCurY,
                                       csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha );
                m_pRenderer->DrawSkew( fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear,
                                       x + cx, fCurY, x + cx, fCurY + fSharpCY,
                                       csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha );
                m_pRenderer->DrawRect( fSharpTopX1, fCurY - fNewNear, fSharpTopX2 - fSharpTopX1, fSharpCY, csKBSharp.iDarkRGB | iAlpha );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                       fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                       csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.75f,
                                       fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.65f,
                                       csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iPrimaryRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha, csKBSharp.iDarkRGB | iAlpha );
            }
        }
}

void MainScreen::RenderBorder()
{
    // Top, bottom, left, right
    const unsigned iBlack = 0x00000000;
    float fBufferCY = static_cast< float >( m_pRenderer->GetBufferHeight() );
    m_pRenderer->DrawRect( m_fNotesX - 50.0f, m_fNotesY - 50.0f, m_fNotesCX + 100.0f, 50.0f, iBlack );
    m_pRenderer->DrawRect( m_fNotesX - 50.0f, m_fNotesY + fBufferCY, m_fNotesCX + 100.0f, 50.0f, iBlack );
    m_pRenderer->DrawRect( m_fNotesX - m_fWhiteCX, m_fNotesY - 50.0f, m_fWhiteCX, fBufferCY + 100.0f, iBlack );
    m_pRenderer->DrawRect( m_fNotesX + m_fNotesCX, m_fNotesY - 50.0f, m_fWhiteCX, fBufferCY + 100.0f, iBlack );

    const float fPad = 10.0f;
    const unsigned iBkg = m_csBackground.iPrimaryRGB;
    m_pRenderer->DrawSkew( m_fNotesX, m_fNotesY + fBufferCY, m_fNotesX + m_fNotesCX, m_fNotesY + fBufferCY,
                           m_fNotesX + m_fNotesCX + fPad, m_fNotesY + fBufferCY + fPad, m_fNotesX - fPad, m_fNotesY + fBufferCY + fPad,
                           iBkg, iBkg, iBlack, iBlack );
    m_pRenderer->DrawSkew( m_fNotesX - fPad, m_fNotesY - fPad, m_fNotesX + m_fNotesCX + fPad, m_fNotesY - fPad,
                           m_fNotesX + m_fNotesCX, m_fNotesY, m_fNotesX, m_fNotesY,
                           iBlack, iBlack, iBkg, iBkg );
    m_pRenderer->DrawSkew( m_fNotesX - fPad, m_fNotesY - fPad, m_fNotesX, m_fNotesY,
                           m_fNotesX, m_fNotesY + fBufferCY, m_fNotesX - fPad, m_fNotesY + fBufferCY + fPad,
                           iBlack, iBkg, iBkg, iBlack );
    m_pRenderer->DrawSkew( m_fNotesX + m_fNotesCX, m_fNotesY, m_fNotesX + m_fNotesCX + fPad, m_fNotesY - fPad,
                           m_fNotesX + m_fNotesCX + fPad, m_fNotesY + fBufferCY + fPad, m_fNotesX + m_fNotesCX, m_fNotesY + fBufferCY,
                           iBkg, iBlack, iBlack, iBkg );
}

void MainScreen::RenderText()
{
    int iLines = 2;
    if ( m_bShowFPS ) iLines++;
    if ( m_eGameMode == GameState::Learn ) iLines += 1;
    else if ( m_InDevice.IsOpen() && m_bScored ) iLines += 1;

    // Screen info
    RECT rcStatus = { m_pRenderer->GetBufferWidth() - 156, 0, m_pRenderer->GetBufferWidth(), 6 + 16 * iLines };

    int iMsgCY = 200;
    RECT rcMsg = { 0, static_cast< int >( m_pRenderer->GetBufferHeight() * ( 1.0f - KBPercent ) - iMsgCY ) / 2 };
    rcMsg.right = m_pRenderer->GetBufferWidth();
    rcMsg.bottom = rcMsg.top + iMsgCY;

    int iTop10CY = 250;
    RECT rcTop10 = { rcMsg.left, rcMsg.top - ( iTop10CY - iMsgCY ) / 2, rcMsg.right };
    rcTop10.bottom = rcTop10.top + iTop10CY;

    int pColWidths[8] = { 35, 80, 50, 55, 55, 55, 55, 80 };
    int iCols = sizeof( pColWidths ) / sizeof( int );
    int pColBorders[9] = { 0 };
    for ( int i = 0; i < iCols; i++ )
        pColBorders[i + 1] = pColBorders[i] + pColWidths[i];
    int xOffset = ( ( rcTop10.right - rcTop10.left ) - pColBorders[iCols] ) / 2;

    // Draw the backgrounds
    unsigned iBkgColor = 0x40000000;
    m_pRenderer->DrawRect( static_cast< float >( rcStatus.left ), static_cast< float >( rcStatus.top ), 
        static_cast< float >( rcStatus.right - rcStatus.left ), static_cast< float >( rcStatus.bottom - rcStatus.top ), 0x80000000 );
    if ( m_bZoomMove || m_bInstructions )
        m_pRenderer->DrawRect( static_cast< float >( rcMsg.left ), static_cast< float >( rcMsg.top ), 
            static_cast< float >( rcMsg.right - rcMsg.left ), static_cast< float >( rcMsg.bottom - rcMsg.top ), iBkgColor );
    else if ( m_iShowTop10 >= 0 )
    {
        m_pRenderer->DrawRect( static_cast< float >( rcTop10.left ), static_cast< float >( rcTop10.top ), 
            static_cast< float >( rcTop10.right - rcTop10.left ), static_cast< float >( rcTop10.bottom - rcTop10.top ), iBkgColor );
        m_pRenderer->DrawRect( static_cast< float >( xOffset ), static_cast< float >( rcTop10.top + 76 ), 
            static_cast< float >( pColBorders[iCols] ), 1.0f, 0x00FFFFFF );
        if ( m_iShowTop10 < 10 )
            m_pRenderer->DrawRect( static_cast< float >( xOffset ), static_cast< float >( rcTop10.top + 80 + m_iShowTop10 * 16 ), 
                static_cast< float >( pColBorders[iCols] ), 15.0f, 0x0066FF66 );
    }

    // Draw the text
    m_pRenderer->BeginText();

    RenderStatus( &rcStatus );    
    if ( m_bZoomMove )
        RenderMessage( &rcMsg, TEXT( "- Left-click and drag to move the screen\n- Right-click and drag to zoom horizontally\n- Press Escape to abort changes\n- Press Ctrl+V to save changes" ) );
    else if ( m_bInstructions && m_eGameMode == Play )
        RenderMessage( &rcMsg, TEXT( "You will be scored. Good luck.\n\nPlay any note when ready." ) );
    else if ( m_bInstructions && m_eGameMode == Learn )
        RenderMessage( &rcMsg, TEXT( "This mode will teach you a song, one track at a time.\nIn Adaptive mode, poorly played sections repeat at a slower rate.\nIn Waiting mode, notes will pause and wait to be played.\n\nPlay any note when ready." ) );
    else if ( m_iShowTop10 >= 0 )
        RenderTop10( &rcTop10, pColBorders );
    else if ( m_tpMessage.IsAlive() )
        m_tpMessage.Render( m_pRenderer, 0.0f, 0.0f );
    else if ( m_tpLongMessage.IsAlive() )
        m_tpLongMessage.Render( m_pRenderer, 0.0f, 0.0f );
    
    m_pRenderer->EndText();
}

void MainScreen::RenderStatus( LPRECT prcStatus )
{
    // Build the time text
    TCHAR sTime[128];
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    if ( m_llStartTime >= 0 )
        _stprintf_s( sTime, TEXT( "%lld:%04.1lf / %lld:%04.1lf" ),
            m_llStartTime / 60000000, ( m_llStartTime % 60000000 ) / 1000000.0,
            mInfo.llTotalMicroSecs / 60000000, ( mInfo.llTotalMicroSecs % 60000000 ) / 1000000.0 );
    else
        _stprintf_s( sTime, TEXT( "\t-%lld:%04.1lf / %lld:%04.1lf" ),
            -m_llStartTime / 60000000, ( -m_llStartTime % 60000000 ) / 1000000.0,
            mInfo.llTotalMicroSecs / 60000000, ( mInfo.llTotalMicroSecs % 60000000 ) / 1000000.0 );

    // Build the FPS text
    TCHAR sFPS[128];
    _stprintf_s( sFPS, TEXT( "%.1lf" ), m_dFPS );
    
    // Build the Scoring text
    TCHAR sScore[128] = TEXT( "N/A" ), sMult[128] = TEXT( "" );
    if ( m_InDevice.IsOpen() && m_bScored )
    {
        Util::CommaPrintf( sScore, m_Score.GetScore() );
        _stprintf_s( sMult, TEXT( "x%d.%d" ), m_Score.GetMult() / 10, m_Score.GetMult() % 10 );
    }

    // Build the learning text
    TCHAR sLearn[128] = TEXT( "All Tracks" ), *sMode = ( m_eLearnMode == GameState::Adaptive ? TEXT( "Adaptive" ) : TEXT( "Waiting" ) );
    if ( m_iLearnOrdinal >= 0 ) _stprintf_s( sLearn, TEXT( "Track %d" ), m_iLearnOrdinal + 1 );

    // Display the text
    InflateRect( prcStatus, -6, -3 );

    OffsetRect( prcStatus, 2, 1 );
    m_pRenderer->DrawText( TEXT( "Time:" ), Renderer::Small, prcStatus, 0, 0xFF404040 );
    m_pRenderer->DrawText( sTime, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
    OffsetRect( prcStatus, -2, -1 );
    m_pRenderer->DrawText( TEXT( "Time:" ), Renderer::Small, prcStatus, 0, 0xFFFFFFFF );
    m_pRenderer->DrawText( sTime, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );

    if ( m_bShowFPS )
    {
        OffsetRect( prcStatus, 2, 16 + 1 );
        m_pRenderer->DrawText( TEXT( "FPS:" ), Renderer::Small, prcStatus, 0, 0xFF404040 );
        m_pRenderer->DrawText( sFPS, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
        OffsetRect( prcStatus, -2, -1 );
        m_pRenderer->DrawText( TEXT( "FPS:" ), Renderer::Small, prcStatus, 0, 0xFFFFFFFF );
        m_pRenderer->DrawText( sFPS, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );
    }

    if ( m_eGameMode != GameState::Learn )
    {
        OffsetRect( prcStatus, 2, 16 + 1 );
        m_pRenderer->DrawText( TEXT( "Score:" ), Renderer::Small, prcStatus, 0, 0xFF404040 );
        m_pRenderer->DrawText( sScore, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
        OffsetRect( prcStatus, -2, -1 );
        m_pRenderer->DrawText( TEXT( "Score:" ), Renderer::Small, prcStatus, 0, 0xFFFFFFFF );
        m_pRenderer->DrawText( sScore, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );

        if ( m_InDevice.IsOpen() && m_bScored )
        {
            OffsetRect( prcStatus, 2, 16 + 1 );
            m_pRenderer->DrawText( sMult, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
            OffsetRect( prcStatus, -2, -1 );
            m_pRenderer->DrawText( sMult, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );
        }
    }
    else
    {
        OffsetRect( prcStatus, 2, 16 + 1 );
        m_pRenderer->DrawText( TEXT( "Learning:" ), Renderer::Small, prcStatus, 0, 0xFF404040 );
        m_pRenderer->DrawText( sLearn, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
        OffsetRect( prcStatus, -2, -1 );
        m_pRenderer->DrawText( TEXT( "Learning:" ), Renderer::Small, prcStatus, 0, 0xFFFFFFFF );
        m_pRenderer->DrawText( sLearn, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );
        OffsetRect( prcStatus, 2, 16 + 1 );
        m_pRenderer->DrawText( sMode, Renderer::Small, prcStatus, DT_RIGHT, 0xFF404040 );
        OffsetRect( prcStatus, -2, -1 );
        m_pRenderer->DrawText( sMode, Renderer::Small, prcStatus, DT_RIGHT, 0xFFFFFFFF );
    }
}

void MainScreen::RenderTop10( LPRECT prcTop10, int pColBorders[9] )
{
    if ( m_iShowTop10 < 0 ) return;

    OffsetRect( prcTop10, 0, 4 );

    OffsetRect( prcTop10, 2, 2 );
    m_pRenderer->DrawText( TEXT( "Top 10" ), Renderer::Large, prcTop10, DT_CENTER | DT_SINGLELINE, 0xFF404040 );
    OffsetRect( prcTop10, -2, -2 );
    m_pRenderer->DrawText( TEXT( "Top 10" ), Renderer::Large, prcTop10, DT_CENTER | DT_SINGLELINE, 0xFFFFFFFF );
    OffsetRect( prcTop10, 0, 37 );

    //Define the header
    TCHAR *pColumns[8] = { TEXT( "Rank" ), TEXT( "Score" ), TEXT( "Pct" ), TEXT( "Streak" ), TEXT( "Great" ), TEXT( "Good" ), TEXT( "OK" ), TEXT( "Date" ) };
    int pFormats[8] = { DT_CENTER, DT_CENTER, DT_CENTER, DT_CENTER, DT_CENTER, DT_CENTER, DT_CENTER, DT_CENTER};
    int iCols = sizeof( pColumns ) / sizeof( TCHAR* );
    int xOffset = ( ( prcTop10->right - prcTop10->left ) - pColBorders[iCols] ) / 2;

    //Draw the message
    TCHAR *sMsg = TEXT( "You didn't make it. Practice!" );
    if ( m_iShowTop10 < 1 &&  m_pFileInfo->top10_size() > 1 )
        sMsg = TEXT( "First place! Awesome!" );
    else if ( m_iShowTop10 < 3 &&  m_pFileInfo->top10_size() > 3 )
        sMsg = TEXT( "You made the top 3! Congratulations!" );
    else if ( m_iShowTop10 < 10 && ( m_pFileInfo->top10_size() > m_iShowTop10 + 1 || m_iShowTop10 == 9 || m_iShowTop10 == 0 ) )
        sMsg = TEXT( "You made the top 10!" );
    else if ( m_iShowTop10 < 10 )
        sMsg = TEXT( "Last place..." );
    OffsetRect( prcTop10, 1, 1 );
    m_pRenderer->DrawText( sMsg, Renderer::Small, prcTop10, DT_CENTER | DT_SINGLELINE, 0xFF000000 );
    OffsetRect( prcTop10, -1, -1 );
    m_pRenderer->DrawText( sMsg, Renderer::Small, prcTop10, DT_CENTER | DT_SINGLELINE, 0xFFFF73FF );
    OffsetRect( prcTop10, 0, 19 );

    //Draw the header
    for ( int i = 0; i < iCols; i++ )
    {
        RECT rcHdr = { pColBorders[i] + xOffset, prcTop10->top, pColBorders[i + 1] + xOffset };
        OffsetRect( &rcHdr, 1, 1 );
        m_pRenderer->DrawText( pColumns[i], Renderer::Small, &rcHdr, pFormats[i] | DT_NOCLIP, 0xFF404040 );
        OffsetRect( &rcHdr, -1, -1 );
        m_pRenderer->DrawText( pColumns[i], Renderer::Small, &rcHdr, pFormats[i] | DT_NOCLIP, 0xFFFFFFFF );
    }
    OffsetRect( prcTop10, 0, 20 );

    TCHAR buf[128];
    for ( int r = 0; r < m_pFileInfo->top10_size(); r++ )
    {
        int iTextColor = ( r == m_iShowTop10 ? 0xFF000000 : 0xFFFFFFFF );
        int iBkgColor = ( r == m_iShowTop10 ? 0xFFFFFFFF : 0xFF404040 );
        const PFAData::Score &dScore = m_pFileInfo->top10( r );
        int iNotes = dScore.great() + dScore.good() + dScore.ok() + dScore.incorrect() + dScore.missed();
        for ( int c = 0; c < iCols; c++ )
        {
            switch ( c )
            {
                case 0: _stprintf_s( buf, TEXT( "%d" ), r + 1 ); break;
                case 1: Util::CommaPrintf( buf, dScore.score() ); break;
                case 2: _stprintf_s( buf, TEXT( "%d%%" ), iNotes == 0 ? 0 :
                            ( ( dScore.great() + dScore.good() + dScore.ok() ) * 100 ) / iNotes ); break;
                case 3: Util::CommaPrintf( buf, dScore.goodstreak() ); break;
                case 4: Util::CommaPrintf( buf, dScore.great() ); break;
                case 5: Util::CommaPrintf( buf, dScore.good() ); break;
                case 6: Util::CommaPrintf( buf, dScore.ok() ); break;
                case 7: _stprintf_s( buf, TEXT( "%02d/%02d/%04d" ), ( dScore.date() / 100 ) % 100, dScore.date() % 100, dScore.date() / 10000 ); break;
            }

            RECT rcVal = { pColBorders[c] + xOffset, prcTop10->top, pColBorders[c + 1] + xOffset };
            OffsetRect( prcTop10, 1, 1 );
            m_pRenderer->DrawText( buf, Renderer::Small, &rcVal, pFormats[c] | DT_NOCLIP, iBkgColor );
            OffsetRect( prcTop10, -1, -1 );
            m_pRenderer->DrawText( buf, Renderer::Small, &rcVal, pFormats[c] | DT_NOCLIP, iTextColor );
        }
        OffsetRect( prcTop10, 0, 16 );
    }
}

void MainScreen::RenderMessage( LPRECT prcMsg, TCHAR *sMsg )
{
    RECT rcMsg = { 0 };
    Renderer::FontSize eFontSize = Renderer::Medium;
    m_pRenderer->DrawText( sMsg, eFontSize, &rcMsg, DT_CALCRECT, 0xFF000000 );
    if ( rcMsg.right > m_pRenderer->GetBufferWidth() )
    {
        eFontSize = Renderer::Small;
        m_pRenderer->DrawText( sMsg, eFontSize, &rcMsg, DT_CALCRECT, 0xFF000000 );
    }
    
    OffsetRect( &rcMsg, 2 + prcMsg->left + ( prcMsg->right - prcMsg->left - rcMsg.right ) / 2,
                2 + prcMsg->top + ( prcMsg->bottom - prcMsg->top - rcMsg.bottom ) / 2 );
    m_pRenderer->DrawText( sMsg, eFontSize, &rcMsg, 0, 0xFF404040 );
    OffsetRect( &rcMsg, -2, -2 );
    m_pRenderer->DrawText( sMsg, eFontSize, &rcMsg, 0, 0xFFFFFFFF );
}

void TextPath::Render( Renderer *pRenderer, float xOffset, float yOffset )
{
    if ( !IsAlive() ) return;

    RECT rcPos = { 0 };
    rcPos.left = static_cast< long >( m_x + m_xOffset + xOffset );
    rcPos.top = static_cast< long >( m_y + m_yOffset + yOffset );
    rcPos.right = rcPos.left;
    rcPos.bottom = rcPos.top;

    const int iShift = ( m_fFont == Renderer::Large || m_fFont == Renderer::Medium ? -2 : 1 );
    OffsetRect( &rcPos, -iShift, -iShift );
    pRenderer->DrawText( m_sText, m_fFont, &rcPos, DT_CENTER | DT_NOCLIP, 0xFF000000 & m_iColor );
    OffsetRect( &rcPos, iShift, iShift );
    pRenderer->DrawText( m_sText, m_fFont, &rcPos, DT_CENTER | DT_NOCLIP, m_iColor );
}

const wchar_t *GameScore::MissedText = L"Missed!";
const wchar_t *GameScore::IncorrectText = L"Wrong!";
const wchar_t *GameScore::OkText = L"OK!";
const wchar_t *GameScore::GoodText = L"Good!";
const wchar_t *GameScore::GreatText = L"Great!";

void GameScore::Missed()
{
    m_Score.set_score( m_Score.score() + MissedScore );
    m_Score.set_missed( m_Score.missed() + 1 );
    m_Score.clear_mult();
    if ( m_Score.curstreak() <= 0 )
        m_Score.set_curstreak( m_Score.curstreak() - 1 );
    else
        m_Score.set_curstreak( -1 );
    m_Score.set_badstreak( max( m_Score.badstreak(), -m_Score.curstreak() ) );
}

void GameScore::Incorrect()
{
    m_Score.set_score( m_Score.score() + IncorrectScore );
    m_Score.set_incorrect( m_Score.incorrect() + 1 );
    m_Score.clear_mult();
    if ( m_Score.curstreak() <= 0 )
        m_Score.set_curstreak( m_Score.curstreak() - 1 );
    else
        m_Score.set_curstreak( -1 );
    m_Score.set_badstreak( max( m_Score.badstreak(), -m_Score.curstreak() ) );
}

MIDIChannelEvent::InputQuality GameScore::HitQuality( long long llError, double dSpeed )
{
    llError = abs( llError );
    if ( llError <= static_cast< long long >( GreatTime * dSpeed ) ) return MIDIChannelEvent::Great;
    else if ( llError <= static_cast< long long >( GoodTime * dSpeed ) )  return MIDIChannelEvent::Good;
    else if ( llError <= static_cast< long long >( OkTime * dSpeed ) ) return MIDIChannelEvent::Ok;
    else return MIDIChannelEvent::Missed;
}

void GameScore::Hit( MIDIChannelEvent::InputQuality eHitQuality )
{
    switch ( eHitQuality )
    {
        case MIDIChannelEvent::Great:
            m_Score.set_score( m_Score.score() + ( GreatScore * m_Score.mult() ) / 10 );
            m_Score.set_great( m_Score.great() + 1 );
            break;
        case MIDIChannelEvent::Good:
            m_Score.set_score( m_Score.score() + ( GoodScore * m_Score.mult() ) / 10 );
            m_Score.set_good( m_Score.good() + 1 );
            break;
        case MIDIChannelEvent::Ok:
            m_Score.set_score( m_Score.score() + ( OkScore * m_Score.mult() ) / 10 );
            m_Score.set_ok( m_Score.ok() + 1 );
            break;
        default:
            Incorrect();
            return;
    }

    m_Score.set_mult( min( m_Score.mult() + 1, 80 ) );

    if ( m_Score.curstreak() >= 0 )
        m_Score.set_curstreak( m_Score.curstreak() + 1 );
    else
        m_Score.set_curstreak( 1 );
    m_Score.set_goodstreak( max( m_Score.goodstreak(), m_Score.curstreak() ) );

    return;
}

int GameScore::AddToTop10( PFAData::FileInfo *pFileInfo )
{
    if ( !m_Score.has_score() ) return -1;

    int iPos = pFileInfo->top10_size();
    PFAData::Score *pScore = pFileInfo->add_top10();
    pScore->CopyFrom( m_Score );

    SYSTEMTIME st;
    GetSystemTime( &st );
    pScore->set_date( st.wYear * 10000 + st.wMonth * 100 + st.wDay );

    while ( iPos > 0 && pFileInfo->top10( iPos - 1 ).score() < m_Score.score() )
    {
        pFileInfo->mutable_top10()->SwapElements( iPos, iPos - 1 );
        iPos--;
    }

    if ( pFileInfo->top10_size() > 10 )
        pFileInfo->mutable_top10()->RemoveLast();
    return iPos;
}