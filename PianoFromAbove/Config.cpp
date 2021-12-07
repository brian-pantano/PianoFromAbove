/*************************************************************************************************
*
* File: Config.cpp
*
* Description: Implements the configuration objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <Windows.h>
#include <Shlobj.h>
#include <TChar.h>

#include <fstream>
using namespace std;

#include "Config.h"
#include "Misc.h"
//-----------------------------------------------------------------------------
// Main Config class
//-----------------------------------------------------------------------------

Config &Config::GetConfig()
{
    static Config instance;
    return instance;
}

Config::Config()
{
    LoadDefaultValues();
    LoadConfigValues();
}

string Config::GetFolder()
{
    char sAppData[MAX_PATH];
    if ( FAILED( SHGetFolderPathA( NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, sAppData ) ) )
        return string();

    strcat_s( sAppData, "\\" );
    strcat_s( sAppData, APPNAME );
    if ( GetFileAttributesA( sAppData ) == INVALID_FILE_ATTRIBUTES )
        if ( !CreateDirectoryA( sAppData, NULL ) )
            return string();

    return sAppData;
}

void Config::LoadDefaultValues()
{
    m_VisualSettings.LoadDefaultValues();
    m_AudioSettings.LoadDefaultValues();
    m_VideoSettings.LoadDefaultValues();
    m_ControlsSettings.LoadDefaultValues();
    m_PlaybackSettings.LoadDefaultValues();
    m_ViewSettings.LoadDefaultValues();
    m_VizSettings.LoadDefaultValues();
}

void Config::LoadConfigValues()
{
    // Where to load?
    string sPath = GetFolder();
    if ( sPath.length() == 0 ) return;

    // Load it
    TiXmlDocument doc( sPath + "\\Config.xml" );
    if ( !doc.LoadFile() ) return;

    // Get the root element
    TiXmlElement *txRoot = doc.FirstChildElement();
    if ( !txRoot ) return;

    LoadConfigValues( txRoot );

    // Custom settings need to be loaded from a separate file, otherwise stock PFA will reset them
    doc = TiXmlDocument(sPath + "\\pfavizkhang.xml");
    if (!doc.LoadFile())
        return;

    txRoot = doc.FirstChildElement();
    if (!txRoot)
        return;

    m_VizSettings.LoadConfigValues(txRoot);
}

void Config::LoadConfigValues( TiXmlElement *txRoot )
{
    m_VisualSettings.LoadConfigValues( txRoot );
    m_AudioSettings.LoadConfigValues( txRoot );
    m_VideoSettings.LoadConfigValues( txRoot );
    m_ControlsSettings.LoadConfigValues( txRoot );
    m_PlaybackSettings.LoadConfigValues( txRoot );
    m_ViewSettings.LoadConfigValues( txRoot );
}

bool Config::SaveConfigValues()
{
    // Where to save?
    string sPath = GetFolder();
    if ( sPath.length() == 0 ) return false;

    // Create the XML document
    TiXmlDocument doc;
    TiXmlDeclaration *decl = new TiXmlDeclaration( "1.0", "", "" );
    doc.LinkEndChild( decl );
    TiXmlElement *txRoot = new TiXmlElement( APPNAMENOSPACES );
    doc.LinkEndChild( txRoot );

    // Save each of the config
    SaveConfigValues( txRoot );

    // Write it!
    bool bStockRet = doc.SaveFile( sPath + "\\Config.xml" );

    // Same as in LoadConfigValues
    doc = TiXmlDocument();
    decl = new TiXmlDeclaration("1.0", "", "");
    doc.LinkEndChild(decl);
    txRoot = new TiXmlElement(APPNAMENOSPACES);
    doc.LinkEndChild(txRoot);

    m_VizSettings.SaveConfigValues(txRoot);

    return bStockRet && doc.SaveFile(sPath + "\\pfavizkhang.xml");
}

bool Config::SaveConfigValues( TiXmlElement *txRoot )
{
    bool bSaved = true;
    bSaved &= m_VisualSettings.SaveConfigValues( txRoot );
    bSaved &= m_AudioSettings.SaveConfigValues( txRoot );
    bSaved &= m_VideoSettings.SaveConfigValues( txRoot );
    bSaved &= m_ControlsSettings.SaveConfigValues( txRoot );
    bSaved &= m_PlaybackSettings.SaveConfigValues( txRoot );
    bSaved &= m_ViewSettings.SaveConfigValues( txRoot );
    return bSaved;
}

//-----------------------------------------------------------------------------
// LoadDefaultValues
//-----------------------------------------------------------------------------

void VisualSettings::LoadDefaultValues()
{
    this->eKeysShown = All;
    this->bAlwaysShowControls = false;
    this->bAssociateFiles = false;
    this->iFirstKey = 0;
    this->iLastKey = 127;

    iBkgColor = 0x00303030;
    int R, G, B, H = 0, S = 80, V = 100;
    int iColors = sizeof( this->colors ) / sizeof( this->colors[0] );
    for ( int i = 10, count = 0; count < iColors; i = ( i + 7 ) % iColors, count++ )
    {
        Util::HSVtoRGB( 360 * i / iColors, S, V, R, G, B );
        this->colors[count] = RGB( R, G, B );
    }
    swap( this->colors[2], this->colors[4] );
}

void AudioSettings::LoadDefaultValues()
{
    this->iOutDevice = -1;
    LoadMIDIDevices();
}

void VideoSettings::LoadDefaultValues()
{
    this->bLimitFPS = true;
    this->bShowFPS = false;
    this->eRenderer = Direct3D;
}

void ControlsSettings::LoadDefaultValues()
{
    this->dFwdBackSecs = 3.0;
    this->dSpeedUpPct = 10.0;
}

void PlaybackSettings::LoadDefaultValues()
{
    this->m_ePlayMode = GameState::Splash;
    this->m_bMute = false;
    this->m_bPlayable = false;
    this->m_bPaused = true;
    this->m_dSpeed = 1.0;
    this->m_dNSpeed = 1.0;
    this->m_dVolume = 1.0;
}

void ViewSettings::LoadDefaultValues()
{
    this->m_bLibrary = true;
    this->m_bControls = true;
    this->m_bKeyboard = true;
    this->m_bOnTop = false;
    this->m_bFullScreen = false;
    this->m_bZoomMove = false;
    this->m_fOffsetX = 0.0f;
    this->m_fOffsetY = 0.0f;
    this->m_fZoomX = 1.0f;
    this->m_iMainLeft = CW_USEDEFAULT;
    this->m_iMainTop = CW_USEDEFAULT;
    this->m_iMainWidth = 960;
    this->m_iMainHeight = 589;
    this->m_iLibWidth = 0;
}

void VizSettings::LoadDefaultValues() {
    this->bTickBased = false;
    this->bShowMarkers = true;
    this->eMarkerEncoding = MarkerEncoding::CP1252;
    this->bNerdStats = false;
    this->sSplashMIDI = L"";
    this->bVisualizePitchBends = false;
    this->bDumpFrames = false;
    this->iBarColor = 0x00FF0080;
}

void AudioSettings::LoadMIDIDevices()
{
    wstring oldOutDev( this->iOutDevice >= 0 ? this->vMIDIOutDevices[this->iOutDevice] : L"" );
    this->iOutDevice = -1;
    this->vMIDIOutDevices.clear();
    int iNumOutDevs = midiOutGetNumDevs();
    for ( int i = 0; i < iNumOutDevs; i++ )
    {
        MIDIOUTCAPS moc;
        midiOutGetDevCaps( i, &moc, sizeof( MIDIOUTCAPS ) );
        this->vMIDIOutDevices.push_back( moc.szPname );

        if ( this->sDesiredOut == this->vMIDIOutDevices[i] )
            this->iOutDevice = i;
        if ( oldOutDev == this->vMIDIOutDevices[i] && this->iOutDevice < 0 )
            this->iOutDevice = i;
    }
    if ( this->iOutDevice < 0 )
        this->iOutDevice = iNumOutDevs - 1;
}

//-----------------------------------------------------------------------------
// LoadConfigValues
//-----------------------------------------------------------------------------

void VisualSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txVisual = txRoot->FirstChildElement( "Visual" );
    if ( !txVisual ) return;

    // Attributes
    int iAttrVal;
    if ( txVisual->QueryIntAttribute( "KeysShown", &iAttrVal ) == TIXML_SUCCESS )
        this->eKeysShown = static_cast< KeysShown >( iAttrVal );
    if ( txVisual->QueryIntAttribute( "AlwaysShowControls", &iAttrVal ) == TIXML_SUCCESS )
        this->bAlwaysShowControls = ( iAttrVal != 0 );
    if ( txVisual->QueryIntAttribute( "AssociateFiles", &iAttrVal ) == TIXML_SUCCESS )
        this->bAssociateFiles = ( iAttrVal != 0 );

    //Colors
    int r, g, b, i = 0;
    TiXmlElement *txColors = txVisual->FirstChildElement( "Colors" );
    if ( txColors )
        for ( TiXmlElement *txColor = txColors->FirstChildElement( "Color" );
              txColor && i < sizeof( this->colors ) / sizeof( this->colors[0] );
              txColor = txColor->NextSiblingElement( "Color" ), i++ )
            if ( txColor->QueryIntAttribute( "R", &r ) == TIXML_SUCCESS &&
                 txColor->QueryIntAttribute( "G", &g ) == TIXML_SUCCESS &&
                 txColor->QueryIntAttribute( "B", &b ) == TIXML_SUCCESS )
                this->colors[i] = ( ( r & 0xFF ) << 0 ) | ( ( g & 0xFF ) << 8 ) | ( ( b & 0xFF ) << 16 );
    
    TiXmlElement *txBkgColor = txVisual->FirstChildElement( "BkgColor" );
    if ( txBkgColor )
        if ( txBkgColor->QueryIntAttribute( "R", &r ) == TIXML_SUCCESS &&
             txBkgColor->QueryIntAttribute( "G", &g ) == TIXML_SUCCESS &&
             txBkgColor->QueryIntAttribute( "B", &b ) == TIXML_SUCCESS )
            this->iBkgColor = ( ( r & 0xFF ) << 0 ) | ( ( g & 0xFF ) << 8 ) | ( ( b & 0xFF ) << 16 );
}

void AudioSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txAudio = txRoot->FirstChildElement( "Audio" );
    if ( !txAudio ) return;

    string sMIDIOutDevice;
    if ( txAudio->QueryStringAttribute( "MIDIOutDevice", &sMIDIOutDevice ) == TIXML_SUCCESS )
    {
        this->sDesiredOut = Util::StringToWstring( sMIDIOutDevice );
        for ( size_t i = 0; i < this->vMIDIOutDevices.size(); i++ )
            if ( this->vMIDIOutDevices[i] == this->sDesiredOut )
                this->iOutDevice = (int)i;
    }
}

void VideoSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txVideo = txRoot->FirstChildElement( "Video" );
    if ( !txVideo ) return;

    int iAttrVal;
    if ( txVideo->QueryIntAttribute( "ShowFPS", &iAttrVal ) == TIXML_SUCCESS )
        this->bShowFPS = ( iAttrVal != 0 );
    if ( txVideo->QueryIntAttribute( "LimitFPS", &iAttrVal ) == TIXML_SUCCESS )
        this->bLimitFPS = ( iAttrVal != 0 );
    if ( txVideo->QueryIntAttribute( "Renderer", &iAttrVal ) == TIXML_SUCCESS )
        this->eRenderer = static_cast< Renderer >( iAttrVal );
}

void ControlsSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txControls = txRoot->FirstChildElement( "Controls" );
    if ( !txControls ) return;

    txControls->QueryDoubleAttribute( "FwdBackSecs", &this->dFwdBackSecs );
    txControls->QueryDoubleAttribute( "SpeedUpPct", &this->dSpeedUpPct );
}

void PlaybackSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txPlayback = txRoot->FirstChildElement( "Playback" );
    if ( !txPlayback ) return;

    int iAttrVal;
    if ( txPlayback->QueryIntAttribute( "Mute", &iAttrVal ) == TIXML_SUCCESS )
        m_bMute = ( iAttrVal != 0 );
    txPlayback->QueryDoubleAttribute( "NoteSpeed", &m_dNSpeed);
    txPlayback->QueryDoubleAttribute( "Volume", &m_dVolume );
}

void ViewSettings::LoadConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txView = txRoot->FirstChildElement( "View" );
    if ( !txView ) return;

    int iAttrVal;
    if (txView->QueryIntAttribute("Library", &iAttrVal) == TIXML_SUCCESS)
        m_bLibrary = (iAttrVal != 0);
    if ( txView->QueryIntAttribute( "Controls", &iAttrVal ) == TIXML_SUCCESS )
        m_bControls = ( iAttrVal != 0 );
    if ( txView->QueryIntAttribute( "Keyboard", &iAttrVal ) == TIXML_SUCCESS )
        m_bKeyboard = ( iAttrVal != 0 );
    if ( txView->QueryIntAttribute( "OnTop", &iAttrVal ) == TIXML_SUCCESS )
        m_bOnTop = ( iAttrVal != 0 );
    txView->QueryFloatAttribute( "OffsetX", &m_fOffsetX );
    txView->QueryFloatAttribute( "OffsetY", &m_fOffsetY );
    txView->QueryFloatAttribute( "ZoomX", &m_fZoomX );
    txView->QueryIntAttribute( "MainLeft", &m_iMainLeft );
    txView->QueryIntAttribute( "MainTop", &m_iMainTop );
    txView->QueryIntAttribute( "MainWidth", &m_iMainWidth );
    txView->QueryIntAttribute( "MainHeight", &m_iMainHeight );
    txView->QueryIntAttribute( "LibWidth", &m_iLibWidth );
}

void VizSettings::LoadConfigValues(TiXmlElement* txRoot) {
    TiXmlElement* txViz = txRoot->FirstChildElement("Viz");
    if (!txViz)
        return;

    int iAttrVal;
    if (txViz->QueryIntAttribute("TickBased", &iAttrVal) == TIXML_SUCCESS)
        bTickBased = (iAttrVal != 0);
    if (txViz->QueryIntAttribute("ShowMarkers", &iAttrVal) == TIXML_SUCCESS)
        bShowMarkers = (iAttrVal != 0);
    if (txViz->QueryIntAttribute("NerdStats", &iAttrVal) == TIXML_SUCCESS)
        bNerdStats = (iAttrVal != 0);
    if (txViz->QueryIntAttribute("VisualizePitchBends", &iAttrVal) == TIXML_SUCCESS)
        bVisualizePitchBends = (iAttrVal != 0);
    if (txViz->QueryIntAttribute("DumpFrames", &iAttrVal) == TIXML_SUCCESS)
        bDumpFrames = (iAttrVal != 0);
    std::string sTempStr;
    txViz->QueryStringAttribute("SplashMIDI", &sTempStr);
    sSplashMIDI = Util::StringToWstring(sTempStr);
    txViz->QueryIntAttribute("MarkerEncoding", (int*)&eMarkerEncoding);
    eMarkerEncoding = min(MarkerEncoding::CP1252, max(eMarkerEncoding, MarkerEncoding::UTF8));

    int r, g, b = 0;
    TiXmlElement* txBarColor = txViz->FirstChildElement("BarColor");
    if (txBarColor)
        if (txBarColor->QueryIntAttribute("R", &r) == TIXML_SUCCESS &&
            txBarColor->QueryIntAttribute("G", &g) == TIXML_SUCCESS &&
            txBarColor->QueryIntAttribute("B", &b) == TIXML_SUCCESS)
            iBarColor = ((r & 0xFF) << 0) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16);
}

//-----------------------------------------------------------------------------
// SaveConfigValues
//-----------------------------------------------------------------------------

bool VisualSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txVisual = new TiXmlElement( "Visual" );
    txRoot->LinkEndChild( txVisual );
    txVisual->SetAttribute( "KeysShown", this->eKeysShown );
    txVisual->SetAttribute( "AlwaysShowControls", this->bAlwaysShowControls );
    txVisual->SetAttribute( "AssociateFiles", this->bAssociateFiles );
    txVisual->SetAttribute( "FirstKey", this->iFirstKey );
    txVisual->SetAttribute( "LastKey", this->iLastKey );

    TiXmlElement *txColors = new TiXmlElement( "Colors" );
    txVisual->LinkEndChild( txColors );
    for ( int i = 0; i < sizeof( this->colors ) / sizeof( this->colors[0] ); i++ )
    {
        TiXmlElement *txColor = new TiXmlElement( "Color" );
        txColors->LinkEndChild( txColor );
        txColor->SetAttribute( "R", ( this->colors[i] >>  0 ) & 0xFF );
        txColor->SetAttribute( "G", ( this->colors[i] >>  8 ) & 0xFF );
        txColor->SetAttribute( "B", ( this->colors[i] >> 16 ) & 0xFF );
    }

    TiXmlElement *txBkgColor = new TiXmlElement( "BkgColor" );
    txVisual->LinkEndChild( txBkgColor );
    txBkgColor->SetAttribute( "R", ( this->iBkgColor >>  0 ) & 0xFF );
    txBkgColor->SetAttribute( "G", ( this->iBkgColor >>  8 ) & 0xFF );
    txBkgColor->SetAttribute( "B", ( this->iBkgColor >> 16 ) & 0xFF );

    return true;
}

bool AudioSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txAudio = new TiXmlElement( "Audio" );
    txRoot->LinkEndChild( txAudio );

    if ( this->sDesiredOut.length() > 0 )
        txAudio->SetAttribute( "MIDIOutDevice", Util::WstringToString( this->sDesiredOut ) );

    return true;
}

bool VideoSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txVideo = new TiXmlElement( "Video" );
    txRoot->LinkEndChild( txVideo );
    txVideo->SetAttribute( "Renderer", this->eRenderer );
    txVideo->SetAttribute( "ShowFPS", this->bShowFPS );
    txVideo->SetAttribute( "LimitFPS", this->bLimitFPS );
    return true;
}

bool ControlsSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txControls = new TiXmlElement( "Controls" );
    txRoot->LinkEndChild( txControls );
    txControls->SetDoubleAttribute( "FwdBackSecs", this->dFwdBackSecs );
    txControls->SetDoubleAttribute( "SpeedUpPct", this->dSpeedUpPct );
    return true;
}

bool PlaybackSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txPlayback = new TiXmlElement( "Playback" );
    txRoot->LinkEndChild( txPlayback );
    txPlayback->SetAttribute( "Mute", m_bMute );
    txPlayback->SetDoubleAttribute( "Volume", m_dVolume );
    txPlayback->SetDoubleAttribute( "NoteSpeed", m_dNSpeed );
    return true;
}

bool ViewSettings::SaveConfigValues( TiXmlElement *txRoot )
{
    TiXmlElement *txView = new TiXmlElement( "View" );
    txRoot->LinkEndChild( txView );
    txView->SetAttribute("Library", m_bLibrary);
    txView->SetAttribute( "Controls", m_bControls );
    txView->SetAttribute( "Keyboard", m_bKeyboard );
    txView->SetAttribute( "OnTop", m_bOnTop );
    txView->SetDoubleAttribute( "OffsetX", m_fOffsetX );
    txView->SetDoubleAttribute( "OffsetY", m_fOffsetY );
    txView->SetDoubleAttribute( "ZoomX", m_fZoomX );
    txView->SetAttribute( "MainLeft", m_iMainLeft );
    txView->SetAttribute( "MainTop", m_iMainTop );
    txView->SetAttribute( "MainWidth", m_iMainWidth );
    txView->SetAttribute( "MainHeight", m_iMainHeight );
    txView->SetAttribute( "LibWidth", m_iLibWidth );
    return true;
}

bool VizSettings::SaveConfigValues(TiXmlElement* txRoot) {
    TiXmlElement* txViz = new TiXmlElement("Viz");
    txRoot->LinkEndChild(txViz);
    txViz->SetAttribute("TickBased", bTickBased);
    txViz->SetAttribute("ShowMarkers", bShowMarkers);
    txViz->SetAttribute("MarkerEncoding", eMarkerEncoding);
    txViz->SetAttribute("NerdStats", bNerdStats);
    txViz->SetAttribute("SplashMIDI", Util::WstringToString(sSplashMIDI));
    txViz->SetAttribute("VisualizePitchBends", bVisualizePitchBends);
    txViz->SetAttribute("DumpFrames", bDumpFrames);

    TiXmlElement* txBarColor = new TiXmlElement("BarColor");
    txViz->LinkEndChild(txBarColor);
    txBarColor->SetAttribute("R", (iBarColor >> 0) & 0xFF);
    txBarColor->SetAttribute("G", (iBarColor >> 8) & 0xFF);
    txBarColor->SetAttribute("B", (iBarColor >> 16) & 0xFF);
    return true;
}