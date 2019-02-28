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
using namespace std;

#include <boost\circular_buffer.hpp>
using namespace boost;

#include "ProtoBuf\MetaData.pb.h"
#include "Renderer.h"
#include "MIDI.h"
#include "Misc.h"

//Abstract base class
class GameState
{
public:
    enum GameError { Success = 0, BadPointer, OutOfMemory, DirectXError, NoInputDevice, BadInputDevice };
    enum State { Intro = 0, Splash, Practice = 160, Play, Learn };
    enum LearnMode { Adaptive, Waiting };

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
    ChannelSettings() { bHidden = bMuted = bScored = false; SetColor( 0x00000000 ); }
    void SetColor();
    void SetColor( unsigned int iColor, double dDark = 0.5, double dVeryDark = 0.2 );

    bool bHidden, bMuted, bScored;
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
    void SetChannelSettings( const vector< bool > &vScored, const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor );

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

class GameScore
{
public:
    static const long long OkTime = 250000;
    static const long long GoodTime = 150000;
    static const long long GreatTime = 50000;

    static const int MissedScore = -50;
    static const int IncorrectScore = -50;
    static const int OkScore = 50;
    static const int GoodScore = 100;
    static const int GreatScore = 300;

    static const wchar_t *MissedText;
    static const wchar_t *IncorrectText;
    static const wchar_t *OkText;
    static const wchar_t *GoodText;
    static const wchar_t *GreatText;

    static const unsigned MissedColor = 0xFFED1C24;
    static const unsigned IncorrectColor = 0xFFED1C24;
    static const unsigned OkColor = 0xFF3F48CC;
    static const unsigned GoodColor = 0xFF22B14C;
    static const unsigned GreatColor = 0xFFFFFF00;

    GameScore() { Reset(); }
    void Reset() { m_Score.Clear(); }

    void Missed();
    void Incorrect();
    void Hit( MIDIChannelEvent::InputQuality eHitQuality );
    MIDIChannelEvent::InputQuality HitQuality( long long llError, double dSpeed );
    int AddToTop10 ( PFAData::FileInfo *pFileInfo );

    int GetScore() const { return m_Score.score(); }
    int GetMult() const { return m_Score.mult(); }
    
    int GetMissed() const { return m_Score.missed(); }
    int GetOk() const { return m_Score.ok(); }
    int GetGood() const { return m_Score.good(); }
    int GetGreat() const { return m_Score.great(); }

private:
    PFAData::Score m_Score;
};

class TextPath
{
public:
    struct TextPathVertex { float x, y, t; int a; };

    void SetPath( const vector< TextPathVertex > &vPath ) { m_vPath = vPath; }
    void SetFont( Renderer::FontSize fFont ) { m_fFont = fFont; }
    void Reset( float xOffset, float yOffset, unsigned iColor, const wchar_t *sText )
        { m_t = 0.0f; m_iPos = 0; m_xOffset = xOffset; m_yOffset = yOffset; m_iColor = iColor; m_sText = sText; }

    bool IsAlive() const { return m_iPos < static_cast< int >( m_vPath.size() ) - 1; }
    void Kill() { m_iPos = (int)m_vPath.size() - 1; }

    void Logic( long long llElapsed );
    void Render( Renderer *pRenderer, float xOffset, float yOffset );

private:
    vector< TextPathVertex > m_vPath;
    Renderer::FontSize m_fFont;
    float m_xOffset, m_yOffset, m_t, m_x, m_y;
    int m_iPos;
    unsigned m_iColor;
    const wchar_t *m_sText;
};

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
    void ScoreChannel( int iTrack, int iChannel, bool bScored ) { m_vTrackSettings[iTrack].aChannels[iChannel].bScored = bScored; m_bScored |= bScored; }
    void MuteChannel( int iTrack, int iChannel, bool bMuted ) { m_vTrackSettings[iTrack].aChannels[iChannel].bMuted = bMuted; }
    void HideChannel( int iTrack, int iChannel, bool bHidden ) { m_vTrackSettings[iTrack].aChannels[iChannel].bHidden = bHidden; }
    void ColorChannel( int iTrack, int iChannel, unsigned int iColor, bool bRandom = false );
    ChannelSettings* GetChannelSettings( int iChannel );
    void SetChannelSettings( const vector< bool > &vScored, const vector< bool > &vMuted, const vector< bool > &vHidden, const vector< unsigned > &vColor );

private:
    typedef vector< pair< long long, int > > eventvec_t;

    // Initialization
    void InitNoteMap( const vector< MIDIEvent* > &vEvents );
    void InitColors();
    void InitLabels();
    void InitState();
    void InitLearning( bool bResetMinTime = true );

    // Logic
    void UpdateState( int iPos );
    void PlayMetronome( double dVolumeCorrect );
    void ProcessInput();
    void JumpTo( long long llStartTime, bool bUpdateGUI = true, bool bInitLearning = true );
    void FindInputPos();
    void PlaySkippedEvents( eventvec_t::const_iterator itOldProgramChange );
    void AdvanceIterators( long long llTime, bool bIsJump );
    MIDIMetaEvent* GetPrevious( eventvec_t::const_iterator &itCurrent,
                                const eventvec_t &vEventMap, int iDataLen );

    // Learning
    void NextTrack();
    bool DoWaiting( long long llNextStartTime, long long llElapsed );
    bool DoTransition( long long llElapsed, long long llOldStartTime );

    // MIDI helpers
    int GetCurrentTick( long long llStartTime );
    int GetCurrentTick( long long llStartTime, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat );
    long long GetTickTime( int iTick );
    long long GetTickTime( int iTick, int iLastTempoTick, long long llLastTempoTime, int iMicroSecsPerBeat );
    int GetBeat( int iTick, int iBeatType, int iLastTempoTick );
    int GetBeatTick( int iTick, int iBeatType, int iLastTempoTick );
    int GetMetTick( int iTick, int iClocksPerMet, int iLastSignatureTick );
    long long GetMinTime() const { return m_MIDI.GetInfo().llFirstNote - 3000000; }
    long long GetMaxTime() const { return m_MIDI.GetInfo().llTotalMicroSecs + 500000; }

    // Rendering
    void RenderGlobals();
    void RenderLines();
    void RenderNotes();
    void RenderNote( int iPos );
    void RenderLabels();
    bool RenderLabel( int iPos, bool bSetState );
    float GetNoteX( int iNote );
    void RenderKeys();
    void RenderBorder();
    void RenderText();
    void RenderStatus( LPRECT prcPos );
    void RenderTop10( LPRECT prcTop10, int pColBorders[9] );
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
    int m_iStartInputPos, m_iEndInputPos; // Defines the input range for hitting a note
    long long m_llStartTime, m_llTimeSpan;  // Times of the start and end events of the current window
    int m_iStartTick; // Tick that corresponds with m_llStartTime. Used to help with beat and metronome detection
    vector< int > m_vState;  // The notes that are on at time m_llStartTime.
    int m_pNoteState[128]; // The last note that was turned on
    int m_pInputState[128]; // The input state
    double m_dSpeed; // Speed multiplier
    bool m_bPaused; // Paused state
    Timer m_Timer; // Frame timers
    bool m_bMute;
    double m_dVolume;
    long long m_llEndLoop;

    // Learning
    static const long long TestTime = 7500000;
    static const int FlashTime = 500000;
    static const int TransitionTime = 2000000;
    static const int TransitionPct = 20;
    LearnMode m_eLearnMode;
    bool m_bInTransition, m_bForceWait;
    int m_iLearnOrdinal, m_iLearnTrack, m_iLearnChannel, m_iLearnPos;
    int m_iNotesAlpha, m_iNotesTime, m_iWaitingAlpha, m_iWaitingTime;
    int m_iGoodCount;
    circular_buffer< pair< int, int > > m_cbLastNotes;
    long long m_llTransitionTime, m_llMinTime;

    // Scoring and notifications
    GameScore m_Score;
    wchar_t m_sBuf[128];
    TextPath m_tpMessage, m_tpLongMessage;
    TextPath m_tpParticles[128];
    PFAData::FileInfo *m_pFileInfo;

    // Labeling
    int m_iHotNote, m_iNextHotNote, m_iSelectedNote;
    bool m_bHaveMouse;
    
    // FPS variables
    bool m_bShowFPS;
    int m_iFPSCount;
    long long m_llFPSTime;
    double m_dFPS;

    // Devices
    MIDIOutDevice m_OutDevice;
    MIDIInDevice m_InDevice;

    // Metronome
    static const int HiWoodBlock = 76;
    static const int LowWoodBlock = 77;
    int m_iLastMetronomeNote, m_iNextBeatTick, m_iNextMetTick;

    // Visual
    static const float SharpRatio;
    static const float KeyRatio;
    bool m_bShowKB, m_bNoteLabels, m_bScored, m_bInstructions;
    int m_iShowTop10;
    int m_eKeysShown;
    ChannelSettings m_csBackground;
    ChannelSettings m_csKBRed, m_csKBWhite, m_csKBSharp, m_csKBBackground, m_csKBBadNote;
    vector< TrackSettings > m_vTrackSettings;

    float m_fZoomX, m_fOffsetX, m_fOffsetY;
    float m_fTempZoomX, m_fTempOffsetX, m_fTempOffsetY;
    bool m_bZoomMove, m_bTrackPos, m_bTrackZoom;
    POINT m_ptStartZoom, m_ptLastPos;

    // Computed in RenderGlobal
    int m_iStartNote, m_iEndNote; // Start and end notes of the songs
    float m_fNotesX, m_fNotesY, m_fNotesCX, m_fNotesCY; // Notes position
    int m_iAllWhiteKeys; // Number of white keys are on the screen
    float m_fWhiteCX; // Width of the white keys
    long long m_llRndStartTime; // Rounded start time to make stuff drop at the same time
};