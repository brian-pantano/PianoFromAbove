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
#include <algorithm>
#include <tchar.h>
#include <ppl.h>
#include <dwmapi.h>

#include "Globals.h"
#include "GameState.h"
#include "Config.h"
#include "resource.h"
#include "ConfigProcs.h"
#include <d3d9types.h>

const wstring GameState::Errors[] =
{
    L"Success.",
    L"Invalid pointer passed. It would be nice if you could submit feedback with a description of how this happened.",
    L"Out of memory. This is a problem",
    L"Error calling DirectX. It would be nice if you could submit feedback with a description of how this happened.",
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
    // Start new ImGui frame
    m_pRenderer->ImguiStartFrame();

    Sleep( 10 );
    return Success;
}

GameState::GameError IntroScreen::Render()
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    // Clear the backbuffer to a blue color
    m_pRenderer->ClearAndBeginScene( D3DCOLOR_XRGB( 0, 0, 0 ) );
    m_pRenderer->DrawRect( 0.0f, 0.0f, static_cast< float >( m_pRenderer->GetBufferWidth() ),
                           static_cast< float >( m_pRenderer->GetBufferHeight() ), 0x00000000 );

    // Present the backbuffer contents to the display
    m_pRenderer->EndScene();
    m_pRenderer->Present();
    return Success;
}

//-----------------------------------------------------------------------------
// SplashScreen GameState object
//-----------------------------------------------------------------------------

SplashScreen::SplashScreen( HWND hWnd, D3D12Renderer *pRenderer ) : GameState( hWnd, pRenderer )
{
    HRSRC hResInfo = FindResource( NULL, MAKEINTRESOURCE( IDR_SPLASHMIDI ), TEXT( "MIDI" ) );
    HGLOBAL hRes = LoadResource( NULL, hResInfo );
    int iSize = SizeofResource( NULL, hResInfo );
    unsigned char *pData = ( unsigned char * )LockResource( hRes );

    Config& config = Config::GetConfig();
    VizSettings viz = config.GetVizSettings();

    // Parse MIDI
    if (!viz.sSplashMIDI.empty()) {
        // this is REALLY BAD, but i can't figure out how to make it move ownership of the memory pool vector instead of copying
        m_MIDI.~MIDI();
        new (&m_MIDI) MIDI(viz.sSplashMIDI);
        if (!m_MIDI.IsValid()) {
            MessageBox(hWnd, L"The custom splash MIDI failed to load. Please choose a different MIDI.", L"", MB_ICONWARNING);
            m_MIDI = MIDI();
            m_MIDI.ParseMIDI(pData, iSize);
        }
    } else {
        m_MIDI.ParseMIDI(pData, iSize);
    }
    vector< MIDIEvent* > vEvents;
    vEvents.reserve( m_MIDI.GetInfo().iEventCount );
    m_MIDI.ConnectNotes(); // Order's important here
    m_MIDI.PostProcess(m_vEvents);

    // Allocate
    m_vTrackSettings.resize( m_MIDI.GetInfo().iNumTracks );
    for (int i = 0; i < 128; i++)
        m_vState[i].reserve(128);

    // Initialize
    //InitNotes( vEvents );
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

    SetChannelSettings( vector< bool >(), vector< bool >(),
        vector< unsigned >( cVisual.colors, cVisual.colors + sizeof( cVisual.colors ) / sizeof( cVisual.colors[0] ) ) );

    if ( cAudio.iOutDevice >= 0 )
        m_OutDevice.Open( cAudio.iOutDevice );
    m_OutDevice.SetVolume( 1.0 );

    m_Timer.Init(false);
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

void SplashScreen::SetChannelSettings( const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    static Config& config = Config::GetConfig();
    static const VizSettings& cViz = config.GetVizSettings();

    size_t iPos = 0;
    for ( int i = 0; i < mInfo.iNumTracks; i++ )
    {
        const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
        for ( int j = 0; j < 16; j++ )
            if ( mTrackInfo.aNoteCount[j] > 0 )
            {
                if (cViz.bColorLoop) {
                    ColorChannel(i, j, vColor[iPos % vColor.size()]);
                } else {
                    if (iPos < vColor.size())
                        ColorChannel(i, j, vColor[iPos]);
                    else
                        ColorChannel(i, j, 0, true);
                }
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
    // Start new ImGui frame
    m_pRenderer->ImguiStartFrame();

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

    // Update root constants
    auto& root_consts = m_pRenderer->GetRootConstants();
    root_consts.deflate = clamp(round(m_fWhiteCX * 0.15f / 2.0f), 1.0f, 3.0f);
    root_consts.notes_y = m_fNotesY;
    root_consts.notes_cy = m_fNotesCY;
    root_consts.white_cx = m_fWhiteCX;
    root_consts.timespan = TimeSpan;

    // Update fixed size constants
    auto& fixed_consts = m_pRenderer->GetFixedSizeConstants();
    memcpy(&fixed_consts.note_x, &notex_table, sizeof(float) * 128);
    memset(&fixed_consts.bends, 0, sizeof(float) * 16);

    // Update track colors
    // TODO: Only update track colors lazily
    auto* track_colors = m_pRenderer->GetTrackColors();
    for (int i = 0; i < min(m_vTrackSettings.size(), MaxTrackColors); i++) {
        for (int j = 0; j < 16; j++) {
            auto& src = m_vTrackSettings[i].aChannels[j];
            auto& dst = track_colors[i * 16 + j];
            dst.primary = src.iPrimaryRGB;
            dst.dark = src.iDarkRGB;
            dst.darker = src.iVeryDarkRGB;
        }
    }

    return Success;
}

// https://github.com/WojciechMula/simd-search/blob/master/sse-binsearch-block.cpp
int sse_bin_search(const std::vector<int>& data, int key) {

    const __m128i keys = _mm_set1_epi32(key);
    __m128i v;

    int limit = data.size() - 1;
    int a = 0;
    int b = limit;

    while (a <= b) {
        const int c = (a + b) / 2;

        if (data[c] == key) {
            return c;
        }

        if (key < data[c]) {
            b = c - 1;

            if (b >= 4) {
                v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&data[b - 4]));
                v = _mm_cmpeq_epi32(v, keys);
                const uint16_t mask = _mm_movemask_epi8(v);
                if (mask) {
                    return b - 4 + __builtin_ctz(mask) / 4;
                }
            }
        }
        else {
            a = c + 1;

            if (a + 4 < limit) {
                v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&data[a]));
                v = _mm_cmpeq_epi32(v, keys);
                const uint16_t mask = _mm_movemask_epi8(v);
                if (mask) {
                    return a + __builtin_ctz(mask) / 4;
                }
            }
        }
    }

    return -1;
}

void SplashScreen::UpdateState(int iPos)
{
    // Event data
    MIDIChannelEvent* pEvent = m_vEvents[iPos];
    if (!pEvent->GetSister()) return;

    MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
    int iNote = pEvent->GetParam1();
    int iVelocity = pEvent->GetParam2();

    int iSisterIdx = pEvent->GetSisterIdx();
    auto& note_state = m_vState[iNote];

    // Turn note on
    if (eEventType == MIDIChannelEvent::NoteOn && iVelocity > 0)
        note_state.push_back(iPos);
    else
    {
        if (iSisterIdx != -1) {
            // binary search
            auto pos = sse_bin_search(note_state, iSisterIdx);
            if (pos != -1)
                note_state.erase(note_state.begin() + pos);
        }
        else {
            // slow path, should rarely happen
            vector< int >::iterator it = note_state.begin();
            MIDIChannelEvent* pSearch = pEvent->GetSister();
            while (it != note_state.end())
            {
                if (m_vEvents[*it] == pSearch) {
                    it = note_state.erase(it);
                    break;
                }
                else {
                    ++it;
                }
            }
        }
    }
}

const float SplashScreen::SharpRatio = 0.65f;

GameState::GameError SplashScreen::Render()
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    // Clear the backbuffer to a blue color
    m_pRenderer->ClearAndBeginScene( D3DCOLOR_XRGB( 0, 0, 0 ) );
    m_pRenderer->DrawRect( 0.0f, 0.0f, static_cast< float >( m_pRenderer->GetBufferWidth() ),
                           static_cast< float >( m_pRenderer->GetBufferHeight() ), 0x00000000 );
    RenderNotes();

    // Present the backbuffer contents to the display
    m_pRenderer->EndScene();
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

    GenNoteXTable();
}

void SplashScreen::RenderNotes()
{
    // Do we have any notes to render?
    if ( m_iEndPos < 0 || m_iStartPos >= static_cast< int >( m_vEvents.size() ) )
        return;

    for (int i = m_iEndPos; i >= m_iStartPos; i--) {
        MIDIChannelEvent* pEvent = m_vEvents[i];
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
            pEvent->GetParam2() > 0 && pEvent->GetSister() &&
            MIDI::IsSharp(pEvent->GetParam1())) {
            RenderNote(pEvent);
        }
    }
    for (int i = 0; i < 128; i++) {
        if (MIDI::IsSharp(i)) {
            for (vector< int >::reverse_iterator it = (m_vState[i]).rbegin(); it != (m_vState[i]).rend(); it++) {
                RenderNote(m_vEvents[*it]);
            }
        }
    }

    for (int i = m_iEndPos; i >= m_iStartPos; i--) {
        MIDIChannelEvent* pEvent = m_vEvents[i];
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
            pEvent->GetParam2() > 0 && pEvent->GetSister())
        {
            if (!MIDI::IsSharp(pEvent->GetParam1())) {
                RenderNote(pEvent);
            }
        }
    }
    for (int i = 0; i < 128; i++) {
        if (!MIDI::IsSharp(i)) {
            for (vector< int >::reverse_iterator it = (m_vState[i]).rbegin(); it != (m_vState[i]).rend(); it++) {
                RenderNote(m_vEvents[*it]);
            }
        }
    }

    m_pRenderer->RenderBatch();
}

void SplashScreen::RenderNote(MIDIChannelEvent* pNote)
{
    /*
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
    m_pRenderer->DrawRect(x + fDeflate, y - cy + fDeflate,
        cx - fDeflate * 2.0f, cy - fDeflate * 2.0f,
        csTrack.iPrimaryRGB | iAlpha1, csTrack.iDarkRGB | iAlpha1, csTrack.iDarkRGB | iAlpha2, csTrack.iPrimaryRGB | iAlpha2);
    m_pRenderer->DrawRect(x, y - cy, cx, cy, csTrack.iVeryDarkRGB | iAlpha);
    */

    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    long long llNoteStart = pNote->GetAbsMicroSec();
    long long llNoteEnd = pNote->GetSister()->GetAbsMicroSec();
    m_pRenderer->PushNoteData(
        NoteData {
            .key = (uint8_t)iNote,
            .channel = (uint8_t)iChannel,
            .track = (uint16_t)iTrack,
            .pos = static_cast<float>(llNoteStart - m_llRndStartTime),
            .length = static_cast<float>(llNoteEnd - llNoteStart),
        }
    );
}

void SplashScreen::GenNoteXTable() {
    int min_key = min(max(0, m_iStartNote), 127);
    int max_key = min(max(0, m_iEndNote), 127);
    for (int i = min_key; i <= max_key; i++) {
        int iWhiteKeys = MIDI::WhiteCount(m_iStartNote, i);
        float fStartX = (MIDI::IsSharp(m_iStartNote) - MIDI::IsSharp(i)) * SharpRatio / 2.0f;
        if (MIDI::IsSharp(i))
        {
            MIDI::Note eNote = MIDI::NoteVal(i);
            if (eNote == MIDI::CS || eNote == MIDI::FS) fStartX -= SharpRatio / 5.0f;
            else if (eNote == MIDI::AS || eNote == MIDI::DS) fStartX += SharpRatio / 5.0f;
        }
        notex_table[i] = m_fNotesX + m_fWhiteCX * (iWhiteKeys + fStartX);
    }
}

float SplashScreen::GetNoteX(int iNote) {
    return notex_table[iNote];
}

//-----------------------------------------------------------------------------
// MainScreen GameState object
//-----------------------------------------------------------------------------

MainScreen::MainScreen( wstring sMIDIFile, State eGameMode, HWND hWnd, D3D12Renderer *pRenderer ) :
    GameState( hWnd, pRenderer ), m_MIDI( sMIDIFile ), m_eGameMode( eGameMode )
{
    // Finish off midi processing
    if ( !m_MIDI.IsValid() ) return;
    m_MIDI.ConnectNotes(); // Order's important here
    m_MIDI.PostProcess(m_vEvents, &m_vProgramChange, &m_vMetaEvents, &m_vTempo, &m_vSignature, &m_vMarkers);

    // Allocate
    m_vTrackSettings.resize( m_MIDI.GetInfo().iNumTracks );
    for (auto note_state : m_vState)
        note_state.reserve(m_MIDI.GetInfo().iNumTracks * 16);

    // Initialize
    //InitNoteMap( vEvents ); // Longish
    InitColors();
    InitState();

    g_LoadingProgress.stage = MIDILoadingProgress::Stage::Done;
}

void MainScreen::InitNoteMap( const vector< MIDIEvent* > &vEvents )
{
    g_LoadingProgress.stage = MIDILoadingProgress::Stage::Finalize;
    g_LoadingProgress.progress = 0;
    g_LoadingProgress.max = vEvents.size(); // probably stays the same

    bool bPianoOverride = Config::GetConfig().m_bPianoOverride;

    //Get only the channel events
    m_vEvents.reserve( vEvents.size() );
    m_vMarkers.push_back(pair<long long, int>(0, -1)); // dummy value
    for (vector< MIDIEvent* >::const_iterator it = vEvents.begin(); it != vEvents.end(); ++it) {
        if ((*it)->GetEventType() == MIDIEvent::ChannelEvent)
        {
            MIDIChannelEvent* pEvent = reinterpret_cast<MIDIChannelEvent*>(*it);
            m_vEvents.push_back(pEvent);

            // Makes random access to the song faster, but unsure if it's worth it
            MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
            if (eEventType == MIDIChannelEvent::ProgramChange || eEventType == MIDIChannelEvent::Controller) {
                m_vProgramChange.push_back(pair< long long, int >(pEvent->GetAbsMicroSec(), m_vEvents.size() - 1));
            }
            if (pEvent->GetSister())
                pEvent->GetSister()->SetSisterIdx(m_vEvents.size() - 1);
        }
        // Have to keep track of tempo and signature for the measure lines
        // markers too
        else if ((*it)->GetEventType() == MIDIEvent::MetaEvent)
        {
            MIDIMetaEvent* pEvent = reinterpret_cast<MIDIMetaEvent*>(*it);
            m_vMetaEvents.push_back(pEvent);

            MIDIMetaEvent::MetaEventType eEventType = pEvent->GetMetaEventType();
            switch (eEventType) {
            case MIDIMetaEvent::SetTempo:
                m_vTempo.push_back(pair< long long, int >(pEvent->GetAbsMicroSec(), m_vMetaEvents.size() - 1));
                break;
            case MIDIMetaEvent::TimeSignature:
                m_vSignature.push_back(pair< long long, int >(pEvent->GetAbsMicroSec(), m_vMetaEvents.size() - 1));
                break;
            case MIDIMetaEvent::Marker:
                m_vMarkers.push_back(pair< long long, int >(pEvent->GetAbsMicroSec(), m_vMetaEvents.size() - 1));
                break;
            }
        }
        g_LoadingProgress.progress++;
    }
}

// Display colors
void MainScreen::InitColors()
{
    static Config& config = Config::GetConfig();
    static const VizSettings& cViz = config.GetVizSettings();

    m_csBackground.SetColor( 0x00464646, 0.7f, 1.3f );
    m_csKBBackground.SetColor( 0x00999999, 0.4f, 0.0f );
    m_csKBRed.SetColor(cViz.iBarColor, 0.5f);
    m_csKBWhite.SetColor( 0x00FFFFFF, 0.8f, 0.6f );
    m_csKBSharp.SetColor( 0x00404040, 0.5f, 0.0f );
}

// Init state vars. Only those which validate the date.
void MainScreen::InitState()
{
    static Config &config = Config::GetConfig();
    static const PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const ViewSettings &cView = config.GetViewSettings();
    static const VisualSettings &cVisual = config.GetVisualSettings();
    static const VizSettings& cViz = config.GetVizSettings();

    m_eGameMode = Practice;
    m_iStartPos = 0;
    m_iEndPos = -1;
    m_llStartTime = GetMinTime();
    m_bTrackPos = m_bTrackZoom = false;
    m_fTempZoomX = 1.0f;
    m_fTempOffsetX = m_fTempOffsetY = 0.0f;
    m_dFPS = 0.0;
    m_iFPSCount = 0;
    m_llFPSTime = 0;
    m_dSpeed = -1.0; // Forces a speed reset upon first call to Logic

    m_fZoomX = cView.GetZoomX();
    m_fOffsetX = cView.GetOffsetX();
    m_fOffsetY = cView.GetOffsetY();
    m_bPaused = true;
    m_bMute = cPlayback.GetMute();
    double dNSpeed = cPlayback.GetNSpeed();
    m_llTimeSpan = static_cast< long long >( 3.0 * dNSpeed * 1000000 );

    // m_Timer will be initialized *later*
    m_RealTimer.Init(false);

    m_bDumpFrames = cViz.bDumpFrames;
    if (m_bDumpFrames) {
        RECT rect = {};
        GetWindowRect(g_hWndGfx, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        char buf[1024] = {};
        snprintf(buf, sizeof(buf), "Waiting for connection... (%d x %d)", width, height);
        SetWindowTextA(g_hWnd, buf);
        m_hVideoPipe = CreateNamedPipe(TEXT("\\\\.\\pipe\\pfadump"),
            PIPE_ACCESS_OUTBOUND,
            PIPE_TYPE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            static_cast<DWORD>(width * height * 4 * 120),
            0,
            0,
            nullptr);
        ConnectNamedPipe(m_hVideoPipe, NULL);
        SetWindowTextA(g_hWnd, "Connected!");
    }

    memset( m_pNoteState, -1, sizeof( m_pNoteState ) );
    
    AdvanceIterators( m_llStartTime, true );
}

// Called immediately before changing to this state
GameState::GameError MainScreen::Init()
{
    static Config& config = Config::GetConfig();
    static const AudioSettings &cAudio = config.GetAudioSettings();
    if ( cAudio.iOutDevice >= 0 )
        m_OutDevice.Open( cAudio.iOutDevice );

    m_OutDevice.Reset();
    m_OutDevice.SetVolume( 1.0 );
    m_Timer.Init(config.m_bManualTimer || m_bDumpFrames);
    if (m_bDumpFrames) {
        m_Timer.SetFrameRate(60);
    } else if (m_Timer.m_bManualTimer) {
        // get the screen's refresh rate
        DWM_TIMING_INFO timing_info;
        memset(&timing_info, 0, sizeof(timing_info));
        timing_info.cbSize = sizeof(timing_info);
        if (FAILED(DwmGetCompositionTimingInfo(NULL, &timing_info))) {
            MessageBox(NULL, L"Failed to get the screen refresh rate! Defaulting to 60hz...", L"", MB_ICONERROR);
            m_Timer.SetFrameRate(60);
        } else {
            m_Timer.SetFrameRate(ceil(static_cast<float>(timing_info.rateRefresh.uiNumerator) / static_cast<float>(timing_info.rateRefresh.uiDenominator)));
        }

    }

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

void MainScreen::SetChannelSettings( const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor )
{
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();
    const vector< MIDITrack* > &vTracks = m_MIDI.GetTracks();

    bool bMuted = vMuted.size() > 0;
    bool bHidden = vHidden.size() > 0;
    bool bColor = vColor.size() > 0;

    static Config& config = Config::GetConfig();
    static const VizSettings& cViz = config.GetVizSettings();

    size_t iPos = 0;
    for ( int i = 0; i < vTracks.size(); i++ )
    {
        const MIDITrack::MIDITrackInfo &mTrackInfo = vTracks[i]->GetInfo();
        for ( int j = 0; j < 16; j++ )
            if ( mTrackInfo.aNoteCount[j] > 0 )
            {
                MuteChannel( i, j, bMuted ? vMuted[min( iPos, vMuted.size() - 1 )] : false );
                HideChannel( i, j, bHidden ? vHidden[min( iPos, vHidden.size() - 1 )] : false );
                if (cViz.bColorLoop && bColor) {
                    ColorChannel(i, j, vColor[iPos % vColor.size()]);
                } else {
                    if (bColor && iPos < vColor.size())
                        ColorChannel(i, j, vColor[iPos]);
                    else
                        ColorChannel(i, j, 0, true);
                }
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
                    JumpTo(GetMinTime());
                    cPlayback.SetStopped(true);
                    return Success;
                case ID_PLAY_SKIPFWD:
                    JumpTo(static_cast<long long>(m_llStartTime + cControls.dFwdBackSecs * 1000000));
                    return Success;
                case ID_PLAY_SKIPBACK:
                    JumpTo(static_cast<long long>(m_llStartTime - cControls.dFwdBackSecs * 1000000));
                    return Success;
                case ID_VIEW_RESETDEVICE:
                    m_pRenderer->ResetDevice();
                    return Success;
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
                    JumpTo(GetMinTime());
                    cPlayback.SetStopped(true);
                    return Success;
                case VK_UP:
                    if ( bAlt && !bCtrl )
                        cPlayback.SetVolume( min( cPlayback.GetVolume() + 0.1, 1.0 ), true );
                    else if ( bShift && !bCtrl )
                        cPlayback.SetNSpeed( cPlayback.GetNSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    else if ( !bAlt && !bShift )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return Success;
                case VK_DOWN:
                    if ( bAlt && !bShift && !bCtrl )
                        cPlayback.SetVolume( max( cPlayback.GetVolume() - 0.1, 0.0 ), true );
                    else if ( bShift && !bAlt && !bCtrl )
                        cPlayback.SetNSpeed( cPlayback.GetNSpeed() / ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    else if ( !bAlt && !bShift )
                        cPlayback.SetSpeed( cPlayback.GetSpeed() * ( 1.0 + cControls.dSpeedUpPct / 100.0 ), true );
                    return Success;
                case 'R':
                    cPlayback.SetSpeed( 1.0, true );
                    return Success;
                case VK_LEFT:
                    JumpTo(static_cast<long long>(m_llStartTime - cControls.dFwdBackSecs * 1000000));
                    return Success;
                case VK_RIGHT:
                    JumpTo(static_cast<long long>(m_llStartTime + cControls.dFwdBackSecs * 1000000));
                    return Success;
                case 'M':
                    cPlayback.ToggleMute( true );
                    return Success;
            }
            break;
        }
        case WM_DEVICECHANGE:
            if ( cAudio.iOutDevice >= 0 && m_OutDevice.GetDevice() != cAudio.vMIDIOutDevices[cAudio.iOutDevice] )
                m_OutDevice.Open( cAudio.iOutDevice );
            break;
        case TBM_SETPOS:
        {
            long long llFirstTime = GetMinTime();
            long long llLastTime = GetMaxTime();
            JumpTo(llFirstTime + ((llLastTime - llFirstTime) * lParam) / 1000, false);
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
            m_bTrackPos = false;
            return Success;
        case WM_RBUTTONUP:
            m_bTrackZoom = false;
            return Success;
        case WM_MOUSEMOVE:
        {
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
    }

    return Success;
}

GameState::GameError MainScreen::Logic( void )
{
    // Start new ImGui frame
    m_pRenderer->ImguiStartFrame();

    static Config &config = Config::GetConfig();
    static PlaybackSettings &cPlayback = config.GetPlaybackSettings();
    static const ViewSettings &cView = config.GetViewSettings();
    static const VisualSettings &cVisual = config.GetVisualSettings();
    static const VideoSettings &cVideo = config.GetVideoSettings();
    static const VizSettings &cViz = config.GetVizSettings();
    const MIDI::MIDIInfo &mInfo = m_MIDI.GetInfo();

    // people are probably going to yell at me if you can't change the bar color during playback
    m_csKBRed.SetColor(cViz.iBarColor, 0.5f);

    // Detect changes in state
    bool bPaused = cPlayback.GetPaused();
    double dSpeed = cPlayback.GetSpeed();
    double dNSpeed = cPlayback.GetNSpeed();
    bool bMute = cPlayback.GetMute();
    long long llTimeSpan = static_cast< long long >( 3.0 * dNSpeed * 1000000 );
    bool bPausedChanged = ( bPaused != m_bPaused );
    bool bSpeedChanged = ( dSpeed != m_dSpeed );
    bool bMuteChanged = ( bMute != m_bMute );
    bool bTimeSpanChanged = ( llTimeSpan != m_llTimeSpan );
    
    // Set the state
    m_bTickMode = cViz.bTickBased;
    m_bPaused = bPaused;
    m_dSpeed = dSpeed;
    m_bMute = bMute;
    m_llTimeSpan = m_bTickMode ? dNSpeed * 3000 : llTimeSpan;
    m_dVolume = cPlayback.GetVolume();
    m_bShowKB = cView.GetKeyboard();
    m_bZoomMove = cView.GetZoomMove();
    m_fOffsetX = cView.GetOffsetX();
    m_fOffsetY = cView.GetOffsetY();
    m_fZoomX = cView.GetZoomX();
    if ( !m_bZoomMove ) m_bTrackPos = m_bTrackZoom = false;
    m_eKeysShown = cVisual.eKeysShown;
    m_iStartNote = min( cVisual.iFirstKey, cVisual.iLastKey );
    m_iEndNote = max( cVisual.iFirstKey, cVisual.iLastKey );
    m_bShowFPS = cVideo.bShowFPS;
    if (m_bDumpFrames)
        m_pRenderer->SetLimitFPS(false);
    else if (m_Timer.m_bManualTimer)
        m_pRenderer->SetLimitFPS(true);
    else
        m_pRenderer->SetLimitFPS( cVideo.bLimitFPS );
    if ( cVisual.iBkgColor != m_csBackground.iOrigBGR ) m_csBackground.SetColor( cVisual.iBkgColor, 0.7f, 1.3f );

    double dMaxCorrect = ( mInfo.iMaxVolume > 0 ? 127.0 / mInfo.iMaxVolume : 1.0 );
    double dVolumeCorrect = ( mInfo.iVolumeSum > 0 ? ( m_dVolume * 127.0 * mInfo.iNoteCount ) / mInfo.iVolumeSum : 1.0 );
    dVolumeCorrect = min( dVolumeCorrect, dMaxCorrect );

    if (cViz.eMarkerEncoding != m_iCurEncoding) {
        m_iCurEncoding = cViz.eMarkerEncoding;
        ApplyMarker(m_pMarkerData, m_iMarkerSize);
    }

    // Time stuff
    long long llMaxTime = GetMaxTime();
    long long llElapsed = m_Timer.GetMicroSecs();
    long long llRealElapsed = m_RealTimer.GetMicroSecs();
    m_Timer.Start();
    m_RealTimer.Start();

    // Compute FPS every half a second
    m_llFPSTime += llRealElapsed;
    m_iFPSCount++;
    if ( m_llFPSTime >= 500000 )
    {
        m_dFPS = m_iFPSCount / ( m_llFPSTime / 1000000.0 );
        m_llFPSTime = m_iFPSCount = 0;
    }

    // If we just paused, kill the music. SetVolume is better than AllNotesOff
    if ( ( bPausedChanged || bMuteChanged ) && ( m_bPaused || m_bMute ) )
        m_OutDevice.AllNotesOff();

    // Figure out start and end times for display
    long long llOldStartTime = m_llStartTime;
    long long llNextStartTime = m_llStartTime + static_cast< long long >( llElapsed * m_dSpeed + 0.5 );

    if ( !m_bPaused && m_llStartTime < llMaxTime )
        m_llStartTime = llNextStartTime;
    m_iStartTick = GetCurrentTick( m_llStartTime );
    long long llEndTime = 0;
    if (m_bTickMode)
        llEndTime = m_iStartTick + m_llTimeSpan;
    else
        llEndTime = m_llStartTime + m_llTimeSpan;

    RenderGlobals();

    // Advance end position
    int iEventCount = (int)m_vEvents.size();
    if (m_bTickMode) {
        while (m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsT() < llEndTime)
            m_iEndPos++;
    } else {
        while (m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsMicroSec() < llEndTime)
            m_iEndPos++;
    }

    // Only want to advance start positions when unpaused becuase advancing startpos "consumes" the events
    if ( !m_bPaused )
    {
        // Advance start position updating initial state as we pass stale events
        // Also PLAYS THE MUSIC
        long long notes_played = 0;
        while ( m_iStartPos < iEventCount && m_vEvents[m_iStartPos]->GetAbsMicroSec() <= m_llStartTime )
        {
            MIDIChannelEvent *pEvent = m_vEvents[m_iStartPos];
            if (pEvent->GetChannelEventType() != MIDIChannelEvent::NoteOn) {
                if (config.m_bPianoOverride && pEvent->GetChannelEventType() == MIDIChannelEvent::ProgramChange && pEvent->GetChannel() != MIDI::Drums)
                    pEvent->SetParam1(0);
                if (pEvent->GetChannelEventType() == MIDIChannelEvent::PitchBend) {
                    //m_pBends[pEvent->GetChannel()] = (short)(((pEvent->GetParam2() << 7) | pEvent->GetParam1()) - 8192);
                    m_pBends[pEvent->GetChannel()] = (notex_table[1] - notex_table[0]) * (((short)(((pEvent->GetParam2() << 7) | pEvent->GetParam1()) - 8192)) / (8192.0f / 12.0f));
                }
                m_OutDevice.PlayEvent(pEvent->GetEventCode(), pEvent->GetParam1(), pEvent->GetParam2());
            }
            else if (!m_bMute && !m_vTrackSettings[pEvent->GetTrack()].aChannels[pEvent->GetChannel()].bMuted) {
                m_OutDevice.PlayEvent(pEvent->GetEventCode(), pEvent->GetParam1(),
                    static_cast<int>(pEvent->GetParam2() * dVolumeCorrect + 0.5));
                notes_played++;
            }
            UpdateState( m_iStartPos );
            m_iStartPos++;
        }
        
        // Update NPS
        if (cViz.bNerdStats) {
            for (; !m_dNPSNotes.empty(); m_dNPSNotes.pop_front()) {
                if (std::get<0>(m_dNPSNotes.front()) >= m_llStartTime - 1000000)
                    break;
            }
            if (notes_played != 0)
                m_dNPSNotes.push_back(std::make_tuple(m_llStartTime, notes_played));
        }
    }

    AdvanceIterators( m_llStartTime, false );

    // Update the position slider
    long long llFirstTime = GetMinTime();
    long long llLastTime = GetMaxTime();
    long long llOldPos = ( ( llOldStartTime - llFirstTime ) * 1000 ) / ( llLastTime - llFirstTime );
    long long llNewPos = ( ( m_llStartTime - llFirstTime ) * 1000 ) / ( llLastTime - llFirstTime );
    if ( llOldPos != llNewPos ) cPlayback.SetPosition( static_cast< int >( llNewPos ) );

    // Song's over
    if (!m_bPaused && m_llStartTime >= llMaxTime) {
        if (m_bDumpFrames)
            CloseHandle(m_hVideoPipe);
        cPlayback.SetPaused(true, true);
    }

    if (m_Timer.m_bManualTimer)
        m_Timer.IncrementFrame();

    // Update root constants
    float fTransitionPct = .02f;
    float fTransitionCY = max(3.0f, floor((m_pRenderer->GetBufferHeight() - m_fNotesCY) * fTransitionPct + 0.5f));
    auto& root_consts = m_pRenderer->GetRootConstants();
    root_consts.deflate = clamp(round(m_fWhiteCX * 0.15f / 2.0f), 1.0f, 3.0f);
    root_consts.notes_y = m_fNotesY;
    root_consts.notes_cy = m_fNotesCY + fTransitionCY;
    root_consts.white_cx = m_fWhiteCX;
    root_consts.timespan = (float)m_llTimeSpan;

    // Update fixed size constants
    auto& fixed_consts = m_pRenderer->GetFixedSizeConstants();
    memcpy(&fixed_consts.note_x, &notex_table, sizeof(float) * 128);
    if (cViz.bVisualizePitchBends)
        memcpy(&fixed_consts.bends, &m_pBends, sizeof(float) * 16);
    else
        memset(&fixed_consts.bends, 0, sizeof(float) * 16);

    // Update track colors
    // TODO: Only update track colors lazily
    auto* track_colors = m_pRenderer->GetTrackColors();
    for (int i = 0; i < min(m_vTrackSettings.size(), MaxTrackColors); i++) {
        for (int j = 0; j < 16; j++) {
            auto& src = m_vTrackSettings[i].aChannels[j];
            auto& dst = track_colors[i * 16 + j];
            dst.primary = src.iPrimaryRGB;
            dst.dark = src.iDarkRGB;
            dst.darker = src.iVeryDarkRGB;
        }
    }

    return Success;
}

void MainScreen::UpdateState( int iPos )
{
    // Event data
    MIDIChannelEvent *pEvent = m_vEvents[iPos];
    if ( !pEvent->GetSister() ) return;
    if (pEvent->GetParam1() > 127)
        return;

    MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
    int iTrack = pEvent->GetTrack();
    int iChannel = pEvent->GetChannel();
    int iNote = pEvent->GetParam1();
    int iVelocity = pEvent->GetParam2();

    int iSisterIdx = pEvent->GetSisterIdx();
    auto& note_state = m_vState[iNote];

    // Turn note on
    if ( eEventType == MIDIChannelEvent::NoteOn && iVelocity > 0 )
    {
        note_state.push_back( iPos );
        m_pNoteState[iNote] = iPos;
    }
    else
    {
        if (iSisterIdx != -1) {
            // binary search
            auto pos = sse_bin_search(note_state, iSisterIdx);
            if (pos != -1)
                note_state.erase(note_state.begin() + pos);
        } else {
            // slow path, should rarely happen
            vector< int >::iterator it = note_state.begin();
            MIDIChannelEvent* pSearch = pEvent->GetSister();
            while (it != note_state.end())
            {
                if (m_vEvents[*it] == pSearch) {
                    it = note_state.erase(it);
                    break;
                } else {
                    ++it;
                }
            }
        }

        if (note_state.size() == 0)
            m_pNoteState[iNote] = -1;
        else
            m_pNoteState[iNote] = note_state.back();
    }
}

void MainScreen::JumpTo(long long llStartTime, bool bUpdateGUI)
{
    // Reset NPS
    m_dNPSNotes.clear();

    // Kill the music!
    m_OutDevice.AllNotesOff();

    // Start time. Piece of cake!
    long long llFirstTime = GetMinTime();
    long long llLastTime = GetMaxTime();
    m_llStartTime = min(max(llStartTime, llFirstTime), llLastTime);
    long long llEndTime = m_llStartTime + m_llTimeSpan;

    // Start position and current state: hard!
    auto itBegin = m_vEvents.begin();
    auto itEnd = m_vEvents.end();
    // Want lower bound to minimize simultaneous complexity
    auto itMiddle = lower_bound(itBegin, itEnd, llStartTime, [&](MIDIChannelEvent* lhs, const long long rhs) {
        return lhs->GetAbsMicroSec() < rhs;
    });

    // Start position
    m_iStartPos = (int)m_vEvents.size();
    if (itMiddle != itEnd && itMiddle - m_vEvents.begin() < m_iStartPos)
        m_iStartPos = itMiddle - m_vEvents.begin();

    // Find the notes that occur simultaneously with the previous note on
    for (auto& note_state : m_vState)
        note_state.clear();
    memset(m_pNoteState, -1, sizeof(m_pNoteState));
    if (itMiddle != itBegin)
    {
        auto itPrev = itMiddle - 1;
        int iFound = 0;
        int iSimultaneous = m_vEvents[itPrev - m_vEvents.begin()]->GetSimultaneous() + 1;
        for (std::vector<MIDIChannelEvent*>::reverse_iterator it(itMiddle); iFound < iSimultaneous && it != m_vEvents.rend(); ++it)
        {
            auto idx = m_vEvents.size() - 1 - (it - m_vEvents.rbegin());
            MIDIChannelEvent* pEvent = m_vEvents[idx];
            if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn && pEvent->GetParam2() > 0 && pEvent->GetSister()) {
                MIDIChannelEvent* pSister = pEvent->GetSister();
                if (pSister->GetAbsMicroSec() > pEvent->GetAbsMicroSec()) // > because itMiddle is the max for its time
                    iFound++;
                if (pSister->GetAbsMicroSec() > llStartTime) // > because we don't care about simultaneous ending notes
                {
                    (m_vState[pEvent->GetParam1()]).push_back(idx);
                    if (m_pNoteState[pEvent->GetParam1()] < 0)
                        m_pNoteState[pEvent->GetParam1()] = idx;
                }
            }
        }
        for (auto& note_state : m_vState)
            reverse(note_state.begin(), note_state.end());
    }

    // End position: a little tricky. Same as logic code. Only needed for paused jumping.
    m_iEndPos = m_iStartPos - 1;
    int iEventCount = (int)m_vEvents.size();
    while (m_iEndPos + 1 < iEventCount && m_vEvents[m_iEndPos + 1]->GetAbsMicroSec() < llEndTime)
        m_iEndPos++;

    // Input position, iterators, tick
    eventvec_t::const_iterator itOldProgramChange = m_itNextProgramChange;
    AdvanceIterators(llStartTime, true);
    PlaySkippedEvents(itOldProgramChange);
    m_iStartTick = GetCurrentTick(m_llStartTime);

    if (bUpdateGUI)
    {
        static PlaybackSettings& cPlayback = Config::GetConfig().GetPlaybackSettings();
        long long llNewPos = ((m_llStartTime - llFirstTime) * 1000) / (llLastTime - llFirstTime);
        cPlayback.SetPosition(static_cast<int>(llNewPos));
    }
}

// Plays skipped program change and controller events. Only plays the one's needed.
// Linear search assumes a small number of events in the file. Better than 128 maps :/
void MainScreen::PlaySkippedEvents(eventvec_t::const_iterator itOldProgramChange)
{
    if (itOldProgramChange == m_itNextProgramChange)
        return;

    // Lookup tables to see if we've got an event for a given control or program. faster than map or hash_map.
    bool aControl[16][128], aProgram[16];
    memset(aControl, 0, sizeof(aControl));
    memset(aProgram, 0, sizeof(aProgram));

    // Go from one before the next to the beginning backwards. iterators are so verbose :/
    vector< MIDIChannelEvent* > vControl;
    eventvec_t::const_reverse_iterator itBegin = eventvec_t::const_reverse_iterator(m_itNextProgramChange);
    eventvec_t::const_reverse_iterator itEnd = m_vProgramChange.rend();
    if (itOldProgramChange < m_itNextProgramChange) itEnd = eventvec_t::const_reverse_iterator(itOldProgramChange);

    for (eventvec_t::const_reverse_iterator it = itBegin; it != itEnd; ++it)
    {
        MIDIChannelEvent* pEvent = m_vEvents[it->second];
        // Order matters because some events affect others, thus store for later use
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::Controller &&
            !aControl[pEvent->GetChannel()][pEvent->GetParam1()])
        {
            aControl[pEvent->GetChannel()][pEvent->GetParam1()] = true;
            vControl.push_back(m_vEvents[it->second]);
        }
        // Order doesn't matter. Just play as you go by.
        else if (pEvent->GetChannelEventType() == MIDIChannelEvent::ProgramChange &&
            !aProgram[pEvent->GetChannel()])
        {
            aProgram[pEvent->GetChannel()] = true;
            m_OutDevice.PlayEvent(pEvent->GetEventCode(), pEvent->GetParam1(), pEvent->GetParam2());
        }
    }

    // Finally play the controller events. vControl is in reverse time order
    for (vector< MIDIChannelEvent* >::reverse_iterator it = vControl.rbegin(); it != vControl.rend(); ++it)
        m_OutDevice.PlayEvent((*it)->GetEventCode(), (*it)->GetParam1(), (*it)->GetParam2());
}

void MainScreen::ApplyMarker(unsigned char* data, size_t size) {
    m_pMarkerData = data;
    m_iMarkerSize = size;
    if (data) {
        Config& config = Config::GetConfig();
        VizSettings viz = config.GetVizSettings();

        constexpr int codepages[] = {1252, 932, CP_UTF8};

        auto temp_str = new char[size + 1];
        memcpy(temp_str, data, size);
        temp_str[size] = '\0';
        
        if (codepages[viz.eMarkerEncoding] != CP_UTF8) {
            // Yes, I have to convert to wide and then back to UTF-8...
            auto wide_len = MultiByteToWideChar(codepages[viz.eMarkerEncoding], 0, temp_str, size + 1, NULL, 0);
            auto wide_temp_str = new WCHAR[wide_len];
            MultiByteToWideChar(codepages[viz.eMarkerEncoding], 0, temp_str, size + 1, wide_temp_str, wide_len);

            auto utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_temp_str, -1, 0, 0, 0, 0);
            auto utf8_temp_str = new char[utf8_len];
            WideCharToMultiByte(CP_UTF8, 0, wide_temp_str, -1, utf8_temp_str, utf8_len, 0, 0);

            m_sMarker = std::string(utf8_temp_str);
            delete[] wide_temp_str;
            delete[] utf8_temp_str;
        } else {
            m_sMarker = temp_str;
        }

        // blacklist common "unset marker" stuff
        if (m_sMarker == "Setup" || m_sMarker == "Start")
            m_sMarker = std::string();

        delete[] temp_str;
    } else {
        m_sMarker = std::string();
    }
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

        auto itCurMarker = m_itNextMarker;
        m_itNextMarker = upper_bound(m_vMarkers.begin(), m_vMarkers.end(), pair< long long, int >(llTime, m_vMetaEvents.size()));
        if (!m_bNextMarkerInited || itCurMarker != m_itNextMarker) {
            m_bNextMarkerInited = true;
            if (m_itNextMarker != m_vMarkers.begin() && (m_itNextMarker - 1)->second != -1) {
                const auto eEvent = m_vMetaEvents[(m_itNextMarker - 1)->second];
                ApplyMarker(eEvent->GetData(), eEvent->GetDataLen());
            }
            else {
                ApplyMarker(nullptr, 0);
            }
        }
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
        auto itCurMarker = m_itNextMarker;
        while (m_itNextMarker != m_vMarkers.end() && m_itNextMarker->first <= llTime)
            ++m_itNextMarker;
        if (itCurMarker != m_itNextMarker) {
            if (m_itNextMarker != m_vMarkers.begin() && (m_itNextMarker - 1)->second != -1) {
                const auto eEvent = m_vMetaEvents[(m_itNextMarker - 1)->second];
                ApplyMarker(eEvent->GetData(), eEvent->GetDataLen());
            } else {
                ApplyMarker(nullptr, 0);
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

const float MainScreen::SharpRatio = 0.65f;
const float MainScreen::KBPercent = 0.25f;
const float MainScreen::KeyRatio = 0.1775f;

GameState::GameError MainScreen::Render() 
{
    if ( FAILED( m_pRenderer->ResetDeviceIfNeeded() ) ) return DirectXError;

    // Update background if it changed
    static Config& config = Config::GetConfig();
    static const VizSettings& cViz = config.GetVizSettings();
    if (cViz.sBackground != m_sCurBackground || cViz.sBackground.empty()) {
        m_bBackgroundLoaded = cViz.sBackground.empty() ? false : m_pRenderer->LoadBackgroundBitmap(cViz.sBackground);
        m_sCurBackground = cViz.sBackground;
    }

    m_pRenderer->ClearAndBeginScene( 0x00000000 );
    RenderLines();
    RenderNotes();
    if ( m_bShowKB )
        RenderKeys();
    RenderBorder();
    RenderText();

    // Present the backbuffer contents to the display
    m_pRenderer->EndScene(m_bBackgroundLoaded);
    m_pRenderer->Present();

    // Dump frame!!!!
    if (m_bDumpFrames) {
        // Get the current frame
        auto* frame = m_pRenderer->Screenshot();

        // Write to pipe
        WriteFile(m_hVideoPipe, frame, static_cast<DWORD>(m_pRenderer->GetBufferWidth() * m_pRenderer->GetBufferHeight() * 4), nullptr, nullptr);

        // Show dump speed on the title bar
        const std::wstring& name = m_MIDI.GetInfo().sFilename;
        TCHAR sTitle[1024];
        _stprintf_s(sTitle, TEXT("%ws (%.1lf%%)"), name.c_str() + (name.find_last_of(L'\\') + 1), (m_dFPS / m_Timer.m_dFramerate) * 100.0);
        SetWindowText(g_hWnd, sTitle);
    }
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
        m_iStartNote = 0;
        m_iEndNote = 127;
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
    if (m_bTickMode) {
        m_llRndStartTime = m_iStartTick;
    } else {
        long long llMicroSecsPP = static_cast< long long >( m_llTimeSpan / m_fNotesCY + 0.5f );
        m_llRndStartTime = m_llStartTime - ( m_llStartTime < 0 ? llMicroSecsPP : 0 );
        m_llRndStartTime = (m_llRndStartTime / llMicroSecsPP ) * llMicroSecsPP;
    }

    GenNoteXTable();
}

void MainScreen::RenderLines()
{
    if (m_bBackgroundLoaded)
        return;

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
    // fuck this lmao
    if ( !( iDivision & 0x8000 ) )
    {
        // Copy time state vars
        int iCurrTick = m_iStartTick - 1;
        long long llEndTime = (m_bTickMode ? m_iStartTick : m_llStartTime) + m_llTimeSpan;

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
        int iNextBeatTick = 0;
        do
        {
            iNextBeatTick = GetBeatTick( iCurrTick + 1, iBeatType, iLastSignatureTick );

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
            float y = m_fNotesY + m_fNotesCY * ( 1.0f - ( (float)(m_bTickMode ? iNextBeatTick : llNextBeatTime) - m_llRndStartTime) / m_llTimeSpan );
            y = floor( y + 0.5f );
            if ( bIsMeasure && y + 1.0f > m_fNotesY )
                m_pRenderer->DrawRect( m_fNotesX, y - 1.0f, m_fNotesCX, 3.0f,
                    m_csBackground.iDarkRGB, m_csBackground.iDarkRGB, m_csBackground.iVeryDarkRGB, m_csBackground.iVeryDarkRGB );

            iCurrTick = iNextBeatTick;
        }
        while ((m_bTickMode ? iNextBeatTick : llNextBeatTime) <= llEndTime );
        // hopefully no race condition?
    }
}

void MainScreen::RenderNotes()
{
    // Do we have any notes to render?
    if ( m_iEndPos < 0 || m_iStartPos >= static_cast< int >( m_vEvents.size() ) )
        return;

    // Ensure that any rects rendered after this point render over the notes
    m_pRenderer->SplitRect();

    bool visualize_bends = Config::GetConfig().GetVizSettings().bVisualizePitchBends;

    /*
    for (int i = m_iEndPos; i >= m_iStartPos; i--) {
        MIDIChannelEvent* pEvent = m_vEvents[i];
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
            pEvent->GetParam2() > 0 && pEvent->GetSister() &&
            MIDI::IsSharp(pEvent->GetParam1())) {
            RenderNote(pEvent, visualize_bends);
        }
    }
    for (int i = 0; i < 128; i++) {
        if (MIDI::IsSharp(i)) {
            for (vector< int >::reverse_iterator it = (m_vState[i]).rbegin(); it != (m_vState[i]).rend(); it++) {
                RenderNote(m_vEvents[*it], visualize_bends);
            }
        }
    }

    for (int i = m_iEndPos; i >= m_iStartPos; i--) {
        MIDIChannelEvent* pEvent = m_vEvents[i];
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
            pEvent->GetParam2() > 0 && pEvent->GetSister())
        {
            if (!MIDI::IsSharp(pEvent->GetParam1())) {
                RenderNote(pEvent, visualize_bends);
            }
        }
    }
    for (int i = 0; i < 128; i++) {
        if (!MIDI::IsSharp(i)) {
            for (vector< int >::reverse_iterator it = (m_vState[i]).rbegin(); it != (m_vState[i]).rend(); it++) {
                RenderNote(m_vEvents[*it], visualize_bends);
            }
        }
    }
    */

    for (int i = m_iEndPos; i >= m_iStartPos; i--) {
        MIDIChannelEvent* pEvent = m_vEvents[i];
        if (pEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
            pEvent->GetParam2() > 0 && pEvent->GetSister()) {
            RenderNote(pEvent, visualize_bends);
        }
    }

    for (int i = 0; i < 128; i++) {
        for (vector< int >::reverse_iterator it = (m_vState[i]).rbegin(); it != (m_vState[i]).rend(); it++) {
            RenderNote(m_vEvents[*it], visualize_bends);
        }
    }

    m_pRenderer->RenderBatch(true);
    m_vThreadWork.clear();
}

void MainScreen::RenderNote(const MIDIChannelEvent* pNote, bool bVisualizeBends)
{
    /*
    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    float fNoteStart = 0;
    float fNoteEnd = 0;
    if (m_bTickMode) {
        fNoteStart = pNote->GetAbsT();
        fNoteEnd = pNote->GetSister()->GetAbsT();
    } else {
        fNoteStart = pNote->GetAbsMicroSec();
        fNoteEnd = pNote->GetSister()->GetAbsMicroSec();
    }

    // TODO: this load is really expensive
    ChannelSettings &csTrack = m_vTrackSettings[iTrack].aChannels[iChannel];
    if ( m_vTrackSettings[iTrack].aChannels[iChannel].bHidden ) return;

    // Compute true positions
    float x = GetNoteX( iNote );
    if (bVisualizeBends)
        x += (notex_table[1] - notex_table[0]) * (m_pBends[iChannel] / (8192.0f / 12.0f));
    float y = m_fNotesY + m_fNotesCY * ( 1.0f - ( fNoteStart - m_fRndStartTime) / m_llTimeSpan );
    float cx =  MIDI::IsSharp( iNote ) ? m_fWhiteCX * SharpRatio : m_fWhiteCX;
    float cy = m_fNotesCY * ( ( fNoteEnd - fNoteStart ) / m_llTimeSpan);
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

    m_pRenderer->DrawRect(x + fDeflate, y - cy + fDeflate,
        cx - fDeflate * 2.0f, cy - fDeflate * 2.0f,
        csTrack.iPrimaryRGB, csTrack.iDarkRGB, csTrack.iDarkRGB, csTrack.iPrimaryRGB);
    m_pRenderer->DrawRect(x, y - cy, cx, cy, csTrack.iVeryDarkRGB);
    */

    int iNote = pNote->GetParam1();
    int iTrack = pNote->GetTrack();
    int iChannel = pNote->GetChannel();
    long long llNoteStart = pNote->GetAbsMicroSec();
    long long llNoteEnd = pNote->GetSister()->GetAbsMicroSec();
    if (m_bTickMode) {
        llNoteStart = pNote->GetAbsT();
        llNoteEnd = pNote->GetSister()->GetAbsT();
    }
    m_pRenderer->PushNoteData(
        NoteData{
            .key = (uint8_t)iNote,
            .channel = (uint8_t)iChannel,
            .track = (uint16_t)iTrack,
            .pos = static_cast<float>(llNoteStart - m_llRndStartTime),
            .length = static_cast<float>(llNoteEnd - llNoteStart),
        }
    );
}

void MainScreen::GenNoteXTable() {
    int min_key = min(max(0, m_iStartNote), 127);
    int max_key = min(max(0, m_iEndNote), 127);
    for (int i = min_key; i <= max_key; i++) {
        int iWhiteKeys = MIDI::WhiteCount(m_iStartNote, i);
        float fStartX = (MIDI::IsSharp(m_iStartNote) - MIDI::IsSharp(i)) * SharpRatio / 2.0f;
        if (MIDI::IsSharp(i))
        {
            MIDI::Note eNote = MIDI::NoteVal(i);
            if (eNote == MIDI::CS || eNote == MIDI::FS) fStartX -= SharpRatio / 5.0f;
            else if (eNote == MIDI::AS || eNote == MIDI::DS) fStartX += SharpRatio / 5.0f;
        }
        notex_table[i] = m_fNotesX + m_fWhiteCX * (iWhiteKeys + fStartX);
    }
}

float MainScreen::GetNoteX(int iNote) {
    return notex_table[iNote];
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
    if (m_bBackgroundLoaded) {
        auto dark = 0x80000000;
        auto very_dark = 0x00000000;
        m_pRenderer->DrawRect(m_fNotesX, fKeysY + fTransitionCY, m_fNotesCX, fKeysCY, very_dark);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY, m_fNotesCX, fTransitionCY,
            0xFF000000, 0xFF000000, very_dark, very_dark);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY + fTransitionCY, m_fNotesCX, fRedCY,
            m_csKBRed.iDarkRGB, m_csKBRed.iDarkRGB, m_csKBRed.iPrimaryRGB, m_csKBRed.iPrimaryRGB);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY + fTransitionCY + fRedCY, m_fNotesCX, fSpacerCY, dark);
    } else {
        m_pRenderer->DrawRect(m_fNotesX, fKeysY, m_fNotesCX, fKeysCY, m_csKBBackground.iVeryDarkRGB);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY, m_fNotesCX, fTransitionCY,
            m_csBackground.iPrimaryRGB, m_csBackground.iPrimaryRGB, m_csKBBackground.iVeryDarkRGB, m_csKBBackground.iVeryDarkRGB);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY + fTransitionCY, m_fNotesCX, fRedCY,
            m_csKBRed.iDarkRGB, m_csKBRed.iDarkRGB, m_csKBRed.iPrimaryRGB, m_csKBRed.iPrimaryRGB);
        m_pRenderer->DrawRect(m_fNotesX, fKeysY + fTransitionCY + fRedCY, m_fNotesCX, fSpacerCY,
            m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB, m_csKBBackground.iDarkRGB);
    }

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
            if ( m_pNoteState[i] == -1 )
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
                const MIDIChannelEvent *pEvent = ( m_pNoteState[i] >= 0 ? m_vEvents[m_pNoteState[i]] : NULL );
                const int iTrack = ( pEvent ? pEvent->GetTrack() : -1 );
                const int iChannel = ( pEvent ? pEvent->GetChannel() : -1 );

                ChannelSettings &csKBWhite = m_vTrackSettings[iTrack].aChannels[iChannel];
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY, m_fWhiteCX - fKeyGap, fTopCY + fNearCY - 2.0f,
                    csKBWhite.iDarkRGB, csKBWhite.iDarkRGB, csKBWhite.iPrimaryRGB, csKBWhite.iPrimaryRGB );
                m_pRenderer->DrawRect( fCurX + fKeyGap1 , fCurY + fTopCY + fNearCY - 2.0f, m_fWhiteCX - fKeyGap, 2.0f, csKBWhite.iDarkRGB );

                if ( i == MIDI::C4 )
                {
                    float fMXGap = floor( m_fWhiteCX * 0.25f + 0.5f );
                    float fMCX = m_fWhiteCX - fMXGap * 2.0f - fKeyGap;
                    float fMY = max( fCurY + fTopCY + fNearCY - fMCX - 7.0f, fCurY + fSharpCY + 5.0f );
                    m_pRenderer->DrawRect( fCurX + fKeyGap1 + fMXGap, fMY, fMCX, fCurY + fTopCY + fNearCY - 7.0f - fMY, csKBWhite.iDarkRGB );
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

            if ( m_pNoteState[i] == -1 )
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
                const MIDIChannelEvent *pEvent = ( m_pNoteState[i] >= 0 ? m_vEvents[m_pNoteState[i]] : NULL );
                const int iTrack = ( pEvent ? pEvent->GetTrack() : -1 );
                const int iChannel = ( pEvent ? pEvent->GetChannel() : -1 );

                const float fNewNear = fNearCY * 0.25f;

                const ChannelSettings &csKBSharp = m_vTrackSettings[iTrack].aChannels[iChannel];
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                       fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                       x + cx, fCurY + fSharpCY, x, fCurY + fSharpCY,
                                       csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iDarkRGB, csKBSharp.iDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                       fSharpTopX1, fCurY + fSharpCY - fNewNear,
                                       x, fCurY + fSharpCY, x, fCurY,
                                       csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iDarkRGB, csKBSharp.iDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX2, fCurY + fSharpCY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear,
                                       x + cx, fCurY, x + cx, fCurY + fSharpCY,
                                       csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iDarkRGB, csKBSharp.iDarkRGB );
                m_pRenderer->DrawRect( fSharpTopX1, fCurY - fNewNear, fSharpTopX2 - fSharpTopX1, fSharpCY, csKBSharp.iDarkRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                       fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                       csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB );
                m_pRenderer->DrawSkew( fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.25f,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.35f,
                                       fSharpTopX2, fCurY - fNewNear + fSharpCY * 0.75f,
                                       fSharpTopX1, fCurY - fNewNear + fSharpCY * 0.65f,
                                       csKBSharp.iPrimaryRGB, csKBSharp.iPrimaryRGB, csKBSharp.iDarkRGB, csKBSharp.iDarkRGB );
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
    Config& config = Config::GetConfig();
    VizSettings viz = config.GetVizSettings();

    int iLines = 2;
    if (m_bShowFPS && !m_bDumpFrames)
        iLines++;
    if (viz.bNerdStats)
        iLines += 2;
    if (m_Timer.m_bManualTimer && !m_bDumpFrames)
        iLines++;

    // Screen info
    RECT rcStatus = { m_pRenderer->GetBufferWidth() - 156, 0, m_pRenderer->GetBufferWidth(), 6 + 16 * iLines };

    // Current marker (if there is one)
    RECT rcMarker;
    //m_pRenderer->DrawText(m_wsMarker.c_str(), D3D12Renderer::Small, &rcMarker, DT_CALCRECT, 0);
    rcMarker = { 0, rcMarker.top, rcMarker.right - rcMarker.left + 12, rcMarker.bottom + 6 };

    int iMsgCY = 200;
    RECT rcMsg = { 0, static_cast<int>(m_pRenderer->GetBufferHeight() * (1.0f - KBPercent) - iMsgCY) / 2 };
    rcMsg.right = m_pRenderer->GetBufferWidth();
    rcMsg.bottom = rcMsg.top + iMsgCY;

    // Draw the text
    m_pRenderer->BeginText();

    RenderStatus(&rcStatus);
    if (viz.bShowMarkers)
        RenderMarker(&rcMarker, m_sMarker.c_str());
    if (m_bZoomMove)
        RenderMessage(&rcMsg, TEXT("- Left-click and drag to move the screen\n- Right-click and drag to zoom horizontally\n- Press Escape to abort changes\n- Press Ctrl+V to save changes"));

    m_pRenderer->EndText();
}

void MainScreen::RenderStatusLine(const char* left, const char* format, ...) {
    va_list varargs;
    va_start(varargs, format);

    char buf[1024] = {};
    vsnprintf_s(buf, sizeof(buf), format, varargs);

    ImGui::Text("%s", left);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(buf).x - ImGui::GetScrollX() - ImGui::GetStyle().WindowPadding.x);
    ImGui::Text("%s", buf);

    va_end(varargs);
}

void MainScreen::RenderStatus(LPRECT prcStatus)
{
    constexpr float width = 156.0f;
    ImGui::SetNextWindowPos(ImVec2(m_pRenderer->GetBufferWidth() - width + 1, -1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::Begin("stats", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

    // Time
    const MIDI::MIDIInfo& mInfo = m_MIDI.GetInfo();
    if (m_llStartTime >= 0)
        RenderStatusLine("Time:", "%lld:%04.1lf / %lld:%04.1lf",
            m_llStartTime / 60000000, (m_llStartTime % 60000000) / 1000000.0,
            mInfo.llTotalMicroSecs / 60000000, (mInfo.llTotalMicroSecs % 60000000) / 1000000.0);
    else
        RenderStatusLine("Time:", "\t-%lld:%04.1lf / %lld:%04.1lf",
            -m_llStartTime / 60000000, (-m_llStartTime % 60000000) / 1000000.0,
            mInfo.llTotalMicroSecs / 60000000, (mInfo.llTotalMicroSecs % 60000000) / 1000000.0);
    
    // Tempo
    RenderStatusLine("Tempo:", "%.3lf", 60000000.0 / m_iMicroSecsPerBeat);

    // Framerate
    if (m_bShowFPS && !m_bDumpFrames)
        RenderStatusLine("FPS:", "%.1lf", m_dFPS);

    // Nerd stats
    Config& config = Config::GetConfig();
    VizSettings viz = config.GetVizSettings();
    if (viz.bNerdStats) {
        long long nps = 0;
        for (int i = 0; i < m_dNPSNotes.size(); i++)
            nps += std::get<1>(m_dNPSNotes[i]);
        RenderStatusLine("NPS:", "%lld", nps);
        RenderStatusLine("Rendered:", "%llu", m_pRenderer->GetRenderedNotesCount());
    }

    ImGui::End();
}

void MainScreen::RenderMarker(LPRECT prcPos, const char* sStr)
{
    /*
    InflateRect(prcPos, -6, -3);

    OffsetRect(prcPos, 2, 1);
    m_pRenderer->DrawText(sStr, D3D12Renderer::Small, prcPos, 0, 0xFF404040);
    OffsetRect(prcPos, -2, -1);
    m_pRenderer->DrawText(sStr, D3D12Renderer::Small, prcPos, 0, 0xFFFFFFFF);
    */

    if (strlen(sStr)) {
        ImGui::SetNextWindowPos(ImVec2(-1.0f, -1.0f));
        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
        ImGui::Begin("marker", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::Text("%s", sStr);

        ImGui::End();
    }
}

void MainScreen::RenderMessage(LPRECT prcMsg, TCHAR* sMsg)
{
    RECT rcMsg = { 0 };
    D3D12Renderer::FontSize eFontSize = D3D12Renderer::Medium;
    m_pRenderer->DrawText(sMsg, eFontSize, &rcMsg, DT_CALCRECT, 0xFF000000);
    if (rcMsg.right > m_pRenderer->GetBufferWidth())
    {
        eFontSize = D3D12Renderer::Small;
        m_pRenderer->DrawText(sMsg, eFontSize, &rcMsg, DT_CALCRECT, 0xFF000000);
    }

    OffsetRect(&rcMsg, 2 + prcMsg->left + (prcMsg->right - prcMsg->left - rcMsg.right) / 2,
        2 + prcMsg->top + (prcMsg->bottom - prcMsg->top - rcMsg.bottom) / 2);
    m_pRenderer->DrawText(sMsg, eFontSize, &rcMsg, 0, 0xFF404040);
    OffsetRect(&rcMsg, -2, -2);
    m_pRenderer->DrawText(sMsg, eFontSize, &rcMsg, 0, 0xFFFFFFFF);
}