/*************************************************************************************************
*
* File: GameState.h
*
* Description: Defines the game states and objects rendered into the graphics window
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include <map>
#include <string>
#include <functional>
using namespace std;

//#include "ProtoBuf\MetaData.pb.h"
#include "Renderer.h"
#include "MIDI.h"
#include "Misc.h"
#include "robin_hood.h"

//Abstract base class
class GameState
{
public:
    enum GameError { Success = 0, BadPointer, OutOfMemory, DirectXError };
    enum State { Intro = 0, Splash, Practice };

    //Static methods
    static const wstring Errors[];
    static GameError ChangeState( GameState *pNextState, GameState **pDestObj );

    //Constructors
    GameState( HWND hWnd, Renderer *pRenderer ) : m_hWnd( hWnd ), m_pRenderer( pRenderer ), m_pNextState( NULL ) {};
    virtual ~GameState( void ) {};

    // Initialize after all other game states have been deleted
    virtual GameError Init() = 0;

    //Handle events
    virtual GameError MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;

    //Run logic
    virtual GameError Logic() = 0;

    //Render scene
    virtual GameError Render() = 0;

    //Null for same state, 
    GameState *NextState() { return m_pNextState; };

    void SetHWnd( HWND hWnd ) { m_hWnd = hWnd; }
    void SetRenderer( Renderer *pRenderer ) { m_pRenderer = pRenderer; }

protected:
    //Windows info
    HWND m_hWnd;

    //Rendering device
    Renderer *m_pRenderer;

    GameState *m_pNextState;

    static const int QueueSize = 50;
};

struct ChannelSettings
{
    ChannelSettings() { bHidden = bMuted = false; SetColor( 0x00000000 ); }
    void SetColor();
    void SetColor( unsigned int iColor, double dDark = 0.5, double dVeryDark = 0.2 );

    bool bHidden, bMuted;
    unsigned int iPrimaryRGB, iDarkRGB, iVeryDarkRGB, iOrigBGR;
};
struct TrackSettings { ChannelSettings aChannels[16]; };

class SplashScreen : public GameState
{
public:
    SplashScreen( HWND hWnd, Renderer *pRenderer );

    GameError MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    GameError Init();
    GameError Logic();
    GameError Render();

private:
    void InitNotes( const vector< MIDIEvent* > &vEvents );
    void InitState();
    void ColorChannel( int iTrack, int iChannel, unsigned int iColor, bool bRandom = false );
    void SetChannelSettings( const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor );

    void UpdateState( int iPos );

    void RenderGlobals();
    void RenderNotes();
    void RenderNote( int iPos );
    float GetNoteX( int iNote );

    // MIDI info
    MIDI m_MIDI; // The song to display
    vector< MIDIChannelEvent* > m_vEvents; // The channel events of the song
    int m_iStartPos;
    int m_iEndPos;
    long long m_llStartTime;
    vector< int > m_vState;  // The notes that are on at time m_llStartTime.
    Timer m_Timer; // Frame timers
    double m_dVolume;
    bool m_bPaused;
    bool m_bMute;

    MIDIOutDevice m_OutDevice;

    static const float SharpRatio;
    static const long long TimeSpan = 3000000;
    vector< TrackSettings > m_vTrackSettings;

    // Computed in RenderGlobal
    int m_iStartNote, m_iEndNote; // Start and end notes of the songs
    float m_fNotesX, m_fNotesY, m_fNotesCX, m_fNotesCY; // Notes position
    int m_iAllWhiteKeys; // Number of white keys are on the screen
    float m_fWhiteCX; // Width of the white keys
    long long m_llRndStartTime; // Rounded start time to make stuff drop at the same time
};

class IntroScreen : public GameState
{
public:
    IntroScreen( HWND hWnd, Renderer *pRenderer ) : GameState( hWnd, pRenderer ) {}

    GameError MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    GameError Init();
    GameError Logic();
    GameError Render();
};

class CustomHashFunc {
public:
    unsigned operator() (MIDIChannelEvent* key) const {
        return (uint64_t)key & 0xFFFFFFFF;
    }
};

class CustomKeyEqualFunc {
public:
    bool operator() (MIDIChannelEvent* a, MIDIChannelEvent* b) const {
        return a == b;
    }
};

typedef struct {
    size_t queue_pos; // where to write the generated vertex data
    const MIDIChannelEvent* note;
} thread_work_t;

class MainScreen : public GameState
{
public:
    static const float KBPercent;

    MainScreen( wstring sMIDIFile, State eGameMode, HWND hWnd, Renderer *pRenderer );

    // GameState functions
    GameError MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    GameError Init();
    GameError Logic( void );
    GameError Render( void );

    // Info
    bool IsValid() const { return m_MIDI.IsValid(); }
    const MIDI& GetMIDI() const { return m_MIDI; }

    // Settings
    void ToggleMuted( int iTrack, int iChannel ) { m_vTrackSettings[iTrack].aChannels[iChannel].bMuted =
                                                  !m_vTrackSettings[iTrack].aChannels[iChannel].bMuted; }
    void ToggleHidden( int iTrack, int iChannel ) { m_vTrackSettings[iTrack].aChannels[iChannel].bHidden =
                                                   !m_vTrackSettings[iTrack].aChannels[iChannel].bHidden; }
    void MuteChannel( int iTrack, int iChannel, bool bMuted ) { m_vTrackSettings[iTrack].aChannels[iChannel].bMuted = bMuted; }
    void HideChannel( int iTrack, int iChannel, bool bHidden ) { m_vTrackSettings[iTrack].aChannels[iChannel].bHidden = bHidden; }
    void ColorChannel( int iTrack, int iChannel, unsigned int iColor, bool bRandom = false );
    ChannelSettings* GetChannelSettings( int iChannel );
    void SetChannelSettings( const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor );

private:
    typedef vector< pair< long long, int > > eventvec_t;

    // Initialization
    void InitNoteMap( const vector< MIDIEvent* > &vEvents );
    void InitColors();
    void InitState();

    // Logic
    void UpdateState( int iPos );
    void JumpTo(long long llStartTime, bool bUpdateGUI = true);
    void PlaySkippedEvents(eventvec_t::const_iterator itOldProgramChange);
    void AdvanceIterators( long long llTime, bool bIsJump );
    MIDIMetaEvent* GetPrevious( eventvec_t::const_iterator &itCurrent,
                                const eventvec_t &vEventMap, int iDataLen );

    // MIDI helpers
    int GetCurrentTick( long long llStartTime );
    int GetCurrentTick( long long llStartTime, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat );
    long long GetTickTime( int iTick );
    long long GetTickTime( int iTick, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat );
    int GetBeat( int iTick, int iBeatType, int iLastTempoTick );
    int GetBeatTick( int iTick, int iBeatType, int iLastTempoTick );
    long long GetMinTime() const { return m_MIDI.GetInfo().llFirstNote - 3000000; }
    long long GetMaxTime() const { return m_MIDI.GetInfo().llTotalMicroSecs + 500000; }

    // Rendering
    void RenderGlobals();
    void RenderLines();
    void RenderNotes();
    void RenderNote(thread_work_t& work);
    void GenNoteXTable();
    float GetNoteX( int iNote );
    void RenderKeys();
    void RenderBorder();
    void RenderText();
    void RenderStatus( LPRECT prcPos );
    void RenderMessage( LPRECT prcMsg, TCHAR *sMsg );

    // MIDI info
    MIDI m_MIDI; // The song to display
    vector< MIDIChannelEvent* > m_vEvents; // The channel events of the song
    vector< MIDIMetaEvent* > m_vMetaEvents; // The meta events of the song
    eventvec_t m_vNoteOns; // Map: note->time->Event pos. Used for fast(er) random access to the song.
    eventvec_t m_vNonNotes; // Tracked for jumping
    eventvec_t m_vProgramChange; // Tracked so we don't jump over them during random access
    eventvec_t m_vTempo; // Tracked for drawing measure lines
    eventvec_t m_vSignature; // Measure lines again
    eventvec_t::const_iterator m_itNextProgramChange;
    eventvec_t::const_iterator m_itNextTempo;
    eventvec_t::const_iterator m_itNextSignature;
    int m_iMicroSecsPerBeat, m_iLastTempoTick; // Tempo
    long long m_llLastTempoTime; // Tempo
    int m_iBeatsPerMeasure, m_iBeatType, m_iClocksPerMet, m_iLastSignatureTick; // Time signature

    // Playback
    State m_eGameMode;
    int m_iStartPos, m_iEndPos; // Postions of the start and end events that occur in the current window
    long long m_llStartTime, m_llTimeSpan;  // Times of the start and end events of the current window
    int m_iStartTick; // Tick that corresponds with m_llStartTime. Used to help with beat and metronome detection
    vector<int> m_vState[128];  // The notes that are on at time m_llStartTime.
    int m_pNoteState[128]; // The last note that was turned on
    double m_dSpeed; // Speed multiplier
    bool m_bPaused; // Paused state
    Timer m_Timer; // Frame timers
    bool m_bMute;
    double m_dVolume;
    size_t m_iNotesPlayed = 0;

    // FPS variables
    bool m_bShowFPS;
    int m_iFPSCount;
    long long m_llFPSTime;
    double m_dFPS;

    // Devices
    MIDIOutDevice m_OutDevice;

    // Visual
    static const float SharpRatio;
    static const float KeyRatio;
    bool m_bShowKB;
    int m_eKeysShown;
    ChannelSettings m_csBackground;
    ChannelSettings m_csKBRed, m_csKBWhite, m_csKBSharp, m_csKBBackground;
    vector< TrackSettings > m_vTrackSettings;
    vector< thread_work_t > m_vThreadWork;

    float m_fZoomX, m_fOffsetX, m_fOffsetY;
    float m_fTempZoomX, m_fTempOffsetX, m_fTempOffsetY;
    bool m_bZoomMove, m_bTrackPos, m_bTrackZoom;
    POINT m_ptStartZoom, m_ptLastPos;

    float notex_table[128];

    // Computed in RenderGlobal
    int m_iStartNote, m_iEndNote; // Start and end notes of the songs
    float m_fNotesX, m_fNotesY, m_fNotesCX, m_fNotesCY; // Notes position
    int m_iAllWhiteKeys; // Number of white keys are on the screen
    float m_fWhiteCX; // Width of the white keys
    float m_fRndStartTime; // Rounded start time to make stuff drop at the same time
    
    // Frame dumping stuff
    bool m_bDumpFrames = false;
    std::vector<unsigned char> m_vImageData;
    size_t m_lluCurrentFrame;
    HANDLE m_hVideoPipe;
};