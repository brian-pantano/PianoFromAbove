/*************************************************************************************************
*
* File: Config.h
*
* Description: Defines the configuration objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <vector>
#include <map>
#include <string>

#include "ProtoBuf\MetaData.pb.h"
#include "tinyxml\tinyxml.h"

#include "MIDI.h"
#include "GameState.h"
#include "MainProcs.h"

#define APPNAME "Piano From Above"
#define APPNAMENOSPACES "PianoFromAbove"
#define CLASSNAME  TEXT( "PianoFromAbove" )
#define GFXCLASSNAME  TEXT( "PianoFromAboveGfx" )
#define POSNCLASSNAME  TEXT( "PianoFromAbovePosCtrl" )
#define MINWIDTH 640
#define MINHEIGHT 469

class ISettings;
class Config;
class SongLibrary;

class ISettings
{
public:
    virtual void LoadDefaultValues() = 0;
    virtual void LoadConfigValues( TiXmlElement *txRoot ) = 0;
    virtual bool SaveConfigValues( TiXmlElement *txRoot ) = 0;
};

struct VisualSettings : public ISettings
{
    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    enum KeysShown { All, Song, Custom } eKeysShown;
    int iFirstKey, iLastKey;
    bool bAlwaysShowControls, bAssociateFiles;
    unsigned int colors[16], iBkgColor;
};

struct AudioSettings : public ISettings
{
    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    void LoadMIDIDevices();
    vector< wstring > vMIDIInDevices;
    vector< wstring > vMIDIOutDevices;
    int iInDevice, iOutDevice;
    wstring sDesiredIn, sDesiredOut;
};

struct VideoSettings : public ISettings
{
    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    enum Renderer { Direct3D, OpenGL, GDI } eRenderer;
    bool bShowFPS, bLimitFPS;
};

struct ControlsSettings : public ISettings
{
    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    double dFwdBackSecs, dSpeedUpPct;
    int aKeyboardMap[128];
};

class PlaybackSettings : public ISettings
{
public:
    enum Metronome { Off, EveryBeat, EveryMeasure };

    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    void ToggleMute( bool bUpdateGUI = false ) { SetMute( !m_bMute, bUpdateGUI ); }
    void TogglePaused( bool bUpdateGUI = false ) { SetPaused( !m_bPaused, bUpdateGUI ); }
    void SetPosition( int iPosition ) { ::SetPosition( iPosition ); }
    void SetLoop( bool bClear ) { ::SetLoop( bClear ); }

    // Set accessors. A bit more advanced because they optionally update the GUI
    void SetPlayMode( GameState::State ePlayMode, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetPlayMode( ePlayMode ); m_ePlayMode = ePlayMode; }
    void SetLearnMode( GameState::LearnMode eLearnMode, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetLearnMode( eLearnMode ); m_eLearnMode = eLearnMode; }
    void SetPlayable( bool bPlayable, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetPlayable( bPlayable ); m_bPlayable = bPlayable; }
    void SetPaused( bool bPaused, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetPlayPauseStop( !bPaused, bPaused, false ); m_bPaused = bPaused; }
    void SetStopped( bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetPlayPauseStop( false, false, true ); m_bPaused = true; }
    void SetSpeed( double dSpeed, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetSpeed( dSpeed ); m_dSpeed = dSpeed; }
    void SetNSpeed( double dNSpeed, bool bUpdateGUI = false ) { dNSpeed = max(min(dNSpeed, 10.0), 0.005); if ( bUpdateGUI ) ::SetNSpeed( dNSpeed ); m_dNSpeed = dNSpeed; }
    void SetVolume( double dVolume, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetVolume( dVolume ); m_dVolume = dVolume; }
    void SetMute( bool bMute, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetMute( bMute ); m_bMute = bMute; }
    void SetMetronome( Metronome eMetronome, bool bUpdateGUI = false ) { if ( bUpdateGUI ) ::SetMetronome( eMetronome ); m_eMetronome = eMetronome; }

    // Get accessors. Simple.
    GameState::State GetPlayMode() const { return m_ePlayMode; }
    GameState::LearnMode GetLearnMode() const { return m_eLearnMode; }
    bool GetPlayable() const { return m_bPlayable; }
    bool GetPaused() const { return m_bPaused; }
    bool GetMute() const { return m_bMute; }
    double GetSpeed() const { return m_dSpeed; }
    double GetNSpeed() const { return m_dNSpeed; }
    double GetVolume() const { return m_dVolume; }
    Metronome GetMetronome() const { return m_eMetronome; }

private:
    GameState::State m_ePlayMode;
    GameState::LearnMode m_eLearnMode;
    bool m_bPlayable, m_bPaused;
    bool m_bMute;
    double m_dSpeed, m_dNSpeed, m_dVolume;
    Metronome m_eMetronome;
};

class ViewSettings : public ISettings
{
public:
    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues( TiXmlElement *txRoot );

    void ToggleLibrary( bool bUpdateGUI = false ) { SetLibrary( !m_bLibrary, bUpdateGUI ); }
    void ToggleControls( bool bUpdateGUI = false ) { SetControls( !m_bControls, bUpdateGUI ); }
    void ToggleKeyboard( bool bUpdateGUI = false ) { SetKeyboard( !m_bKeyboard, bUpdateGUI ); }
    void ToggleNoteLabels( bool bUpdateGUI = false ) { SetNoteLabels( !m_bNoteLabels, bUpdateGUI ); }
    void ToggleOnTop( bool bUpdateGUI = false ) { SetOnTop( !m_bOnTop, bUpdateGUI ); }
    void ToggleFullScreen( bool bUpdateGUI = false ) { SetFullScreen( !m_bFullScreen, bUpdateGUI ); }
    void ToggleZoomMove( bool bUpdateGUI = false ) { SetZoomMove( !m_bZoomMove, bUpdateGUI ); }

    void SetMainPos( int iMainLeft, int iMainTop ) { m_iMainLeft = iMainLeft; m_iMainTop = iMainTop; }
    void SetMainSize( int iMainWidth, int iMainHeight ) { m_iMainWidth = iMainWidth; m_iMainHeight = iMainHeight; }
    void SetOffsetX( float fOffsetX ) { m_fOffsetX = fOffsetX; }
    void SetOffsetY( float fOffsetY ) { m_fOffsetY = fOffsetY; }
    void SetZoomX( float fZoomX ) { m_fZoomX = fZoomX; }
    void SetLibWidth( int iLibWidth ) { m_iLibWidth = iLibWidth; }
    void SetLibrary( bool bLibrary, bool bUpdateGUI = false ) { m_bLibrary = bLibrary; if ( bUpdateGUI ) ::ShowLibrary( bLibrary ); }
    void SetControls( bool bControls, bool bUpdateGUI = false ) { m_bControls = bControls; if ( bUpdateGUI ) ::ShowControls( bControls ); }
    void SetKeyboard( bool bKeyboard, bool bUpdateGUI = false ) { m_bKeyboard = bKeyboard; if ( bUpdateGUI ) ::ShowKeyboard( bKeyboard ); }
    void SetNoteLabels( bool bNoteLabels, bool bUpdateGUI = false ) { m_bNoteLabels = bNoteLabels; if ( bUpdateGUI ) ::ShowNoteLabels( bNoteLabels ); }
    void SetOnTop( bool bOnTop, bool bUpdateGUI = false ) { m_bOnTop = bOnTop; if ( bUpdateGUI ) ::SetOnTop( bOnTop ); }
    void SetFullScreen( bool bFullScreen, bool bUpdateGUI = false ) { m_bFullScreen = bFullScreen; if ( bUpdateGUI ) ::SetFullScreen( bFullScreen ); }
    void SetZoomMove( bool bZoomMove, bool bUpdateGUI = false ) { m_bZoomMove = bZoomMove; if ( bUpdateGUI ) ::SetZoomMove( bZoomMove ); }
    void SetCurLabel( const string &sCurLabel ) { m_sCurLabel = sCurLabel; }

    int GetMainLeft() const { return m_iMainLeft; }
    int GetMainTop() const { return m_iMainTop; }
    int GetMainWidth() const { return m_iMainWidth; }
    int GetMainHeight() const { return m_iMainHeight; }
    int GetLibWidth() const { return m_iLibWidth; }
    float GetOffsetX() const { return m_fOffsetX; }
    float GetOffsetY() const { return m_fOffsetY; }
    float GetZoomX() const { return m_fZoomX; }
    bool GetLibrary() const { return m_bLibrary; }
    bool GetControls() const { return m_bControls; }
    bool GetKeyboard() const { return m_bKeyboard; }
    bool GetNoteLabels() const { return m_bNoteLabels; }
    bool GetOnTop() const { return m_bOnTop; }
    bool GetFullScreen() const { return m_bFullScreen; }
    bool GetZoomMove() const { return m_bZoomMove; }
    const string &GetCurLabel() const { return m_sCurLabel; }

private:
    bool m_bLibrary, m_bControls, m_bKeyboard, m_bNoteLabels, m_bOnTop, m_bFullScreen, m_bZoomMove;
    float m_fOffsetX, m_fOffsetY, m_fZoomX;
    int m_iMainLeft, m_iMainTop, m_iMainWidth, m_iMainHeight, m_iLibWidth;
    string m_sCurLabel;
};

class SongLibrary : public ISettings
{
public:
    ~SongLibrary() { clear(); }

    void LoadDefaultValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    void LoadMetaData();
    bool SaveConfigValues( TiXmlElement *txRoot );
    bool SaveMetaData();

    enum Source { File, Folder, FolderWSubdirs } eRenderer;

    int AddSource( const wstring &sSource, Source eSource, bool bExpand = true );
    int RemoveSource( const wstring &sSource );
    int ExpandSources();
    PFAData::File* SongLibrary::AddFile( const wstring &wsFilename, MIDI *pMidi = NULL );
    void clear();

    const map < wstring, Source > &GetSources() const { return m_mSources; }
    const map< wstring, vector< PFAData::File* >* > &GetFiles() const { return m_mFiles; }
    PFAData::FileInfo *GetInfo( int iPos ) { return m_Data.mutable_fileinfo( iPos ); }
    bool GetAlwaysAdd() const { return m_bAlwaysAdd; }
    int GetSortCol() const { return m_iSortCol; }

    void SetAlwaysAdd( bool bAlwaysAdd ) { m_bAlwaysAdd = bAlwaysAdd; }
    void SetSortCol( int iSortCol ) { m_iSortCol = iSortCol; }

private:
    int ExpandSource( const wstring &sSource, Source eSource );
    int ExpandSource( const wstring &sPath, Source eSource, vector< PFAData::File* > *pvFiles, wchar_t buf[] );

    bool m_bAlwaysAdd;
    int m_iSortCol;

    // Source maps
    map< wstring, Source > m_mSources;
    map< wstring, vector< PFAData::File* >* > m_mFiles;

    // Info maps
    map< pair< string, int >, PFAData::File* > m_mMD5s;
    map< string, int > m_mFileInfos;

    // DB
    PFAData::MetaData m_Data;
};

class Config : public ISettings
{
public:
    // Singleton
    static Config &GetConfig();
    static string GetFolder();

    // Interface
    void LoadDefaultValues();
    void LoadConfigValues();
    void LoadConfigValues( TiXmlElement *txRoot );
    bool SaveConfigValues();
    bool SaveConfigValues( TiXmlElement *txRoot );

    void LoadMIDIDevices() { m_AudioSettings.LoadMIDIDevices(); }

    const VisualSettings& GetVisualSettings() const { return m_VisualSettings; }
    const AudioSettings& GetAudioSettings() const { return m_AudioSettings; }
    const VideoSettings& GetVideoSettings() const { return m_VideoSettings; }
    const ControlsSettings& GetControlsSettings() const { return m_ControlsSettings; }
    SongLibrary& GetSongLibrary() { return m_SongLibrary; }
    PlaybackSettings& GetPlaybackSettings() { return m_PlaybackSettings; }
    ViewSettings& GetViewSettings() { return m_ViewSettings; }

    void SetVisualSettings(const VisualSettings &VisualSettings) { m_VisualSettings = VisualSettings; }
    void SetAudioSettings(const AudioSettings &audioSettings) { m_AudioSettings = audioSettings; }
    void SetVideoSettings(const VideoSettings &videoSettings) { m_VideoSettings = videoSettings; }
    void SetControlsSettings(const ControlsSettings &ControlsSettings) { m_ControlsSettings = ControlsSettings; }

private:
    // Singleton
    Config();
    ~Config() {}
    Config( const Config& );
    Config &operator=( const Config& );

    VisualSettings m_VisualSettings;
    AudioSettings m_AudioSettings;
    VideoSettings m_VideoSettings;
    ControlsSettings m_ControlsSettings;
    SongLibrary m_SongLibrary;
    PlaybackSettings m_PlaybackSettings;
    ViewSettings m_ViewSettings;
};