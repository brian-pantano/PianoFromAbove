/*************************************************************************************************
*
* File: MIDI.cpp
*
* Description: Implements the MIDI objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include "MIDI.h"
#include "robin_hood.h"
#include <fstream>
#include <stack>
#include <array>
#include <ppl.h>

//std::map<int, std::pair<std::vector<MIDIEvent*>::iterator, std::vector<MIDIEvent*>>> midi_map;

//-----------------------------------------------------------------------------
// MIDIPos functions
//-----------------------------------------------------------------------------

MIDIPos::MIDIPos( MIDI &midi ) : m_MIDI( midi )
{
    // Init file position
    m_iCurrTick = m_iCurrMicroSec = 0;

    // Init track positions
    size_t iTracks = m_MIDI.m_vTracks.size();
    for (size_t i = 0; i < iTracks; i++) {
        m_vTrackPos.push_back(0);
    }

    // Init SMPTE tempo
    if ( m_MIDI.m_Info.iDivision & 0x8000 )
    {
        int iFramesPerSec = -( ( m_MIDI.m_Info.iDivision | static_cast< int >( 0xFFFF0000 ) ) >> 8 ) * 100;
        if ( iFramesPerSec == 2900 ) iFramesPerSec = 2997;
        int iTicksPerFrame = m_MIDI.m_Info.iDivision & 0xFF;

        m_bIsStandard = false;
        m_iTicksPerBeat = m_iMicroSecsPerBeat = 0;
        m_iTicksPerSecond = iTicksPerFrame * iFramesPerSec;
    }
    // Init ticks per beat tempo (default to 120 BPM). x/y + 1/2 = (2x + y)/(2y)
    else
    {
        m_bIsStandard = true;
        m_iTicksPerSecond = 0;
        m_iTicksPerBeat = m_MIDI.m_Info.iDivision;
        m_iMicroSecsPerBeat = 500000;
    }
}

// Gets the next closest event as long as it occurs before iMicroSecs elapse
// Always get next event if iMicroSecs is negative
int MIDIPos::GetNextEvent( int iMicroSecs, MIDIEvent **pOutEvent )
{
    if ( !pOutEvent ) return 0;
    *pOutEvent = NULL;

    // Get the next closest event
    MIDIEvent *pMinEvent = NULL;

    if (m_MIDI.midi_map_times_pos != m_MIDI.midi_map_times.size()) {
        auto& pair = m_MIDI.midi_map[m_MIDI.midi_map_times[m_MIDI.midi_map_times_pos]];
        pMinEvent = *pair.first;
        if (++pair.first == pair.second.end())
            m_MIDI.midi_map_times_pos++;
    }

    // No min found. We're at the end of file
    if ( !pMinEvent )
        return 0;

    // Make sure the event doesn't occur after the requested time window
    int iMaxTickAllowed = m_iCurrTick;
    if ( m_bIsStandard )
        iMaxTickAllowed += ( static_cast< long long >( m_iTicksPerBeat ) * ( m_iCurrMicroSec + iMicroSecs ) ) / m_iMicroSecsPerBeat;
    else
        iMaxTickAllowed += ( static_cast< long long >( m_iTicksPerSecond ) * ( m_iCurrMicroSec + iMicroSecs ) ) / 1000000;

    if ( iMicroSecs < 0 || pMinEvent->GetAbsT() <= iMaxTickAllowed )
    {
        // How many micro seconds did we just process?
        *pOutEvent = pMinEvent;
        int iSpan = pMinEvent->GetAbsT() - m_iCurrTick;
        if ( m_bIsStandard )
            iSpan = ( static_cast< long long >( m_iMicroSecsPerBeat ) * iSpan ) / m_iTicksPerBeat - m_iCurrMicroSec;
        else
            iSpan = ( 1000000LL * iSpan ) / m_iTicksPerSecond - m_iCurrMicroSec;
        m_iCurrTick = pMinEvent->GetAbsT();
        m_iCurrMicroSec = 0;
        //m_vTrackPos[iMinPos]++;

        // Change the tempo going forward if we're at a SetTempo event
        if ( pMinEvent->GetEventType() == MIDIEvent::MetaEvent )
        {
            MIDIMetaEvent *pMetaEvent = reinterpret_cast< MIDIMetaEvent* >( pMinEvent );
            if ( pMetaEvent->GetMetaEventType() == MIDIMetaEvent::SetTempo && pMetaEvent->GetDataLen() == 3 )
                MIDI::Parse24Bit ( pMetaEvent->GetData(), 3, &m_iMicroSecsPerBeat );
        }

        return iSpan;
    }
    // No events to be found, but haven't hit end of file
    else
    {
        if ( m_bIsStandard )
            m_iCurrMicroSec = iMicroSecs + m_iCurrMicroSec -
                              ( static_cast< long long >( m_iMicroSecsPerBeat ) * ( iMaxTickAllowed - m_iCurrTick ) ) / m_iTicksPerBeat;
        else
            m_iCurrMicroSec = iMicroSecs + m_iCurrMicroSec -
                              ( 1000000LL * ( iMaxTickAllowed - m_iCurrTick ) ) / m_iTicksPerSecond;
        m_iCurrTick = iMaxTickAllowed;
        return iMicroSecs;
    }
}

int MIDIPos::GetNextEvents( int iMicroSecs, vector< MIDIEvent* > &vEvents )
{
    MIDIEvent *pEvent = NULL;
    int iTotal = 0;
    do
    {
        if ( iMicroSecs >= 0 )
            iTotal += GetNextEvent( iMicroSecs - iTotal, &pEvent );
        else
            iTotal += GetNextEvent( iMicroSecs, &pEvent );
        if ( pEvent ) vEvents.push_back( pEvent );
    }
    while ( pEvent );

    return iTotal;
}

//-----------------------------------------------------------------------------
// MIDI functions
//-----------------------------------------------------------------------------

MIDI::MIDI ( const wstring &sFilename )
{
    // Open the file
    ifstream ifs( sFilename, ios::in | ios::binary | ios::ate );
    if ( !ifs.is_open() )
        return;

    // Read it all in
    size_t iSize = ifs.tellg();
    unsigned char *pcMemBlock = new unsigned char[iSize];
    ifs.seekg( 0, ios::beg );
    ifs.read( reinterpret_cast< char* >( pcMemBlock ), iSize );
    ifs.close();

    // Parse it
    int iTotal = ParseMIDI ( pcMemBlock, iSize );
    m_Info.sFilename = sFilename;
    //Util::MD5( pcMemBlock, iSize, m_Info.sMd5 );
 
    // Clean up
    delete[] pcMemBlock;
}

MIDI::~MIDI( void )
{
    clear();
}

#define EVENT_POOL_MAX 1000000
MIDIChannelEvent* MIDI::AllocChannelEvent() {
    if (event_pools.size() == 0 || event_pools.back().size() == EVENT_POOL_MAX) {
        event_pools.emplace_back();
        event_pools.back().reserve(EVENT_POOL_MAX);
    }
    auto& pool = event_pools.back();
    pool.emplace_back();
    return &pool.back();
}


const wstring MIDI::Instruments[129] =
{
    L"Acoustic Grand Piano", L"Bright Acoustic Piano", L"Electric Grand Piano", L"Honky-tonk Piano", L"Electric Piano 1", 
    L"Electric Piano 2", L"Harpsichord", L"Clavi", L"Celesta", L"Glockenspiel", 
    L"Music Box", L"Vibraphone", L"Marimba", L"Xylophone", L"Tubular Bells", 
    L"Dulcimer", L"Drawbar Organ", L"Percussive Organ", L"Rock Organ", L"Church Organ", 
    L"Reed Organ", L"Accordion", L"Harmonica", L"Tango Accordion", L"Acoustic Guitar (nylon)", 
    L"Acoustic Guitar (steel)", L"Electric Guitar (jazz)", L"Electric Guitar (clean)", L"Electric Guitar (muted)", L"Overdriven Guitar", 
    L"Distortion Guitar", L"Guitar harmonics", L"Acoustic Bass", L"Electric Bass (finger)", L"Electric Bass (pick)", 
    L"Fretless Bass", L"Slap Bass 1", L"Slap Bass 2", L"Synth Bass 1", L"Synth Bass 2", 
    L"Violin", L"Viola", L"Cello", L"Contrabass", L"Tremolo Strings", 
    L"Pizzicato Strings", L"Orchestral Harp", L"Timpani", L"String Ensemble 1", L"String Ensemble 2", 
    L"SynthStrings 1", L"SynthStrings 2", L"Choir Aahs", L"Voice Oohs", L"Synth Voice", 
    L"Orchestra Hit", L"Trumpet", L"Trombone", L"Tuba", L"Muted Trumpet", 
    L"French Horn", L"Brass Section", L"SynthBrass 1", L"SynthBrass 2", L"Soprano Sax", 
    L"Alto Sax", L"Tenor Sax", L"Baritone Sax", L"Oboe", L"English Horn", 
    L"Bassoon", L"Clarinet", L"Piccolo", L"Flute", L"Recorder", 
    L"Pan Flute", L"Blown Bottle", L"Shakuhachi", L"Whistle", L"Ocarina", 
    L"Lead 1 (square)", L"Lead 2 (sawtooth)", L"Lead 3 (calliope)", L"Lead 4 (chiff)", L"Lead 5 (charang)", 
    L"Lead 6 (voice)", L"Lead 7 (fifths)", L"Lead 8 (bass + lead)", L"Pad 1 (new age)", L"Pad 2 (warm)", 
    L"Pad 3 (polysynth)", L"Pad 4 (choir)", L"Pad 5 (bowed)", L"Pad 6 (metallic)", L"Pad 7 (halo)", 
    L"Pad 8 (sweep)", L"FX 1 (rain)", L"FX 2 (soundtrack)", L"FX 3 (crystal)", L"FX 4 (atmosphere)", 
    L"FX 5 (brightness)", L"FX 6 (goblins)", L"FX 7 (echoes)", L"FX 8 (sci-fi)", L"Sitar", 
    L"Banjo", L"Shamisen", L"Koto", L"Kalimba", L"Bag pipe", 
    L"Fiddle", L"Shanai", L"Tinkle Bell", L"Agogo", L"Steel Drums", 
    L"Woodblock", L"Taiko Drum", L"Melodic Tom", L"Synth Drum", L"Reverse Cymbal", 
    L"Guitar Fret Noise", L"Breath Noise", L"Seashore", L"Bird Tweet", L"Telephone Ring",
    L"Helicopter", L"Applause", L"Gunshot", L"Various"
};

const wstring &MIDI::NoteName( int iNote )
{
    InitArrays();
    if ( iNote < 0 || iNote >= MIDI::KEYS ) return aNoteNames[MIDI::KEYS];
    return aNoteNames[iNote];
}

MIDI::Note MIDI::NoteVal( int iNote )
{
    InitArrays();
    if ( iNote < 0 || iNote >= MIDI::KEYS ) return C;
    return aNoteVal[iNote];
}

bool MIDI::IsSharp( int iNote )
{
    InitArrays();
    if ( iNote < 0 || iNote >= MIDI::KEYS ) return false;
    return aIsSharp[iNote];
}

// Number of white keys in [iMinNote, iMaxNote)
int MIDI::WhiteCount( int iMinNote, int iMaxNote )
{
    InitArrays();
    if ( iMinNote < 0 || iMinNote >= MIDI::KEYS || iMaxNote < 0 || iMaxNote >= MIDI::KEYS ) return false;
    return aWhiteCount[iMaxNote] - aWhiteCount[iMinNote];
}

wstring MIDI::aNoteNames[MIDI::KEYS + 1];
MIDI::Note MIDI::aNoteVal[MIDI::KEYS];
bool MIDI::aIsSharp[MIDI::KEYS];
int MIDI::aWhiteCount[MIDI::KEYS + 1];

void MIDI::InitArrays()
{
    static bool bValid = false;

    // Build the array of note names upon first call
    if ( !bValid )
    {
        wchar_t buf[10];
        wchar_t cNote = L'C';
        int iOctave = -1;
        bool bIsSharp = false;
        MIDI::Note eNote = MIDI::C;
        for ( int i = 0; i < MIDI::KEYS; i++ )
        {
            // Don't want sprintf because we're in c++ and string building is too slow. Manual construction!
            int iPos = 0;
            buf[iPos++] = cNote;
            if ( bIsSharp ) buf[iPos++] = L'#';
            if ( iOctave < 0 ) buf[iPos++] = L'-';
            buf[iPos++] = L'0' + abs( iOctave );
            buf[iPos++] = L'\0';

            aNoteNames[i] = buf;
            aNoteVal[i] = eNote;
            aIsSharp[i] = bIsSharp;

            // Advance counters
            if ( eNote == MIDI::B || eNote == MIDI::E || bIsSharp )
                cNote++;
            if ( eNote != MIDI::B && eNote != MIDI::E )
                bIsSharp = !bIsSharp;
            if ( eNote == MIDI::B )
                iOctave++;
            if ( eNote == MIDI::GS )
            {
                cNote = 'A';
                eNote = MIDI::A;
            }
            else
                eNote = static_cast< MIDI::Note >( eNote + 1 );
        }
        aWhiteCount[0] = 0;
        for ( int i = 1; i < MIDI::KEYS + 1; i++ )
            aWhiteCount[i] = aWhiteCount[i - 1] + !aIsSharp[i - 1];
        aNoteNames[MIDI::KEYS] = L"Invalid";
        bValid = true;
    }
}

void MIDI::clear( void )
{
    for ( vector< MIDITrack* >::iterator it = m_vTracks.begin(); it != m_vTracks.end(); ++it )
        delete *it;
    m_vTracks.clear();
    m_Info.clear();
    midi_map.clear();
    midi_map_times.clear();
    midi_map_times_pos = 0;
    event_pools.clear();
}

int MIDI::ParseMIDI( const unsigned char *pcData, size_t iMaxSize )
{
    char pcBuf[4];
    size_t iTotal;
    int iHdrSize;

    // Reset first. This is the only parsing function that resets/clears first.
    clear();

    // Read header info
    if ( ParseNChars( pcData, 4, iMaxSize, pcBuf ) != 4 ) return 0;
    if ( Parse32Bit( pcData + 4, iMaxSize - 4, &iHdrSize) != 4 ) return 0;
    iTotal = 8;

    // Check header info
    if ( strncmp( pcBuf, "MThd", 4 ) != 0 ) return 0;
    iHdrSize = max( iHdrSize, 6 ); // Allowing a bad header size. Some people ignore and hard code 6.
    
    //Read header
    iTotal += Parse16Bit( pcData + iTotal, iMaxSize - iTotal, &m_Info.iFormatType );
    iTotal += Parse16Bit( pcData + iTotal, iMaxSize - iTotal, &m_Info.iNumTracks );
    iTotal += Parse16Bit( pcData + iTotal, iMaxSize - iTotal, &m_Info.iDivision );

    // Check header
    if ( iTotal != 14 || m_Info.iFormatType < 0 || m_Info.iFormatType > 2 || m_Info.iDivision == 0 ) return 0;

    // Parse the rest of the file
    iTotal += iHdrSize - 6;
    return iTotal + ParseTracks( pcData + iTotal, iMaxSize - iTotal );
}

int MIDI::ParseTracks( const unsigned char *pcData, size_t iMaxSize )
{
    size_t iTotal = 0, iCount = 0, iTrack = m_vTracks.size();
    do
    {
        // Create and parse the track
        MIDITrack *track = new MIDITrack(*this);
        iCount = track->ParseTrack( pcData + iTotal, iMaxSize - iTotal, iTrack++ );

        // If Success, add it to the list
        if ( iCount > 0 )
        {
            m_vTracks.push_back( track );
            m_Info.AddTrackInfo( *track );
        }
        else
            delete track;

        iTotal += iCount;
    }
    while ( iMaxSize - iTotal > 0 && iCount > 0 && m_Info.iFormatType != 2 );

    return iTotal;
}

int MIDI::ParseEvents( const unsigned char *pcData, size_t iMaxSize )
{
    // Create and parse the track
    MIDITrack *track = new MIDITrack(*this);
    int iCount = track->ParseEvents( pcData, iMaxSize, static_cast< int >( m_vTracks.size() ) );

    // If Success, add it to the list
    if ( iCount > 0 ) {
        m_vTracks.push_back( track );
        m_Info.AddTrackInfo( *track );
    }
    else
        delete track;

    return iCount;
}

// Computes some of the MIDIInfo info
void MIDI::MIDIInfo::AddTrackInfo( const MIDITrack &mTrack )
{
    const MIDITrack::MIDITrackInfo &mti = mTrack.GetInfo();
    this->iTotalTicks = max( this->iTotalTicks, mti.iTotalTicks );
    this->iEventCount += mti.iEventCount;
    this->iNumChannels += mti.iNumChannels;
    this->iVolumeSum += mti.iVolumeSum;
    if ( mti.iNoteCount )
    {
        if ( !this->iNoteCount )
        {
            this->iMinNote = mti.iMinNote;
            this->iMaxNote = mti.iMaxNote;
            this->iMaxVolume = mti.iMaxVolume;
        }
        else
        {
            this->iMinNote = min( mti.iMinNote, this->iMinNote );
            this->iMaxNote = max( mti.iMaxNote, this->iMaxNote );
            this->iMaxVolume = max( mti.iMaxVolume, this->iMaxVolume );
        }
    }
    this->iNoteCount += mti.iNoteCount;
    if ( !( this->iDivision & 0x8000 ) && this->iDivision > 0 )
        this->iTotalBeats = this->iTotalTicks / this->iDivision;
}

// Sets absolute time variables. A lot of code for not much happening...
// Has to be EXACT. Even a little drift and things start messing up a few minutes in (metronome, etc)
void MIDI::PostProcess( vector< MIDIEvent* > *vEvents )
{
    // Iterator like class
    MIDIPos midiPos( *this );
    bool bIsStandard = midiPos.IsStandard();
    int iTicksPerBeat = midiPos.GetTicksPerBeat();
    int iTicksPerSecond = midiPos.GetTicksPerSecond();
    int iMicroSecsPerBeat = midiPos.GetMicroSecsPerBeat();
    int iLastTempoTick = 0;
    long long llLastTempoTime = 0;
    int iSimultaneous = 0;

    MIDIEvent *pEvent = NULL;
    long long llFirstNote = -1;
    long long llTime = 0;
    int i = 0;
    for ( midiPos.GetNextEvent( -1, &pEvent ); pEvent; midiPos.GetNextEvent( -1, &pEvent ) )
    {
        // Compute the exact time (off by at most a micro second... I don't feel like rounding)
        int iTick = pEvent->GetAbsT();
        if ( bIsStandard )
            llTime = llLastTempoTime + ( static_cast< long long >( iMicroSecsPerBeat ) * ( iTick - iLastTempoTick ) ) / iTicksPerBeat;
        else
            llTime = llLastTempoTime + ( 1000000LL * ( iTick - iLastTempoTick ) ) / iTicksPerSecond;
        pEvent->SetAbsMicroSec( llTime );

        if ( pEvent->GetEventType() == MIDIEvent::ChannelEvent )
        {
            MIDIChannelEvent *pChannelEvent = reinterpret_cast< MIDIChannelEvent* >( pEvent );
            pChannelEvent->SetSimultaneous(iSimultaneous);
            if ( pChannelEvent->GetSister() )
            {
                if ( pChannelEvent->GetChannelEventType() == MIDIChannelEvent::NoteOn &&
                     pChannelEvent->GetParam2() > 0 )
                {
                    if ( llFirstNote < 0  )
                        llFirstNote = llTime;
                    iSimultaneous++;
                }
                else
                    iSimultaneous--;
            }
        }
        else if ( pEvent->GetEventType() == MIDIEvent::MetaEvent )
        {
            MIDIMetaEvent *pMetaEvent = reinterpret_cast< MIDIMetaEvent* >( pEvent );
            if ( pMetaEvent->GetMetaEventType() == MIDIMetaEvent::SetTempo )
            {
                iTicksPerBeat = midiPos.GetTicksPerBeat();
                iTicksPerSecond = midiPos.GetTicksPerSecond();
                iMicroSecsPerBeat = midiPos.GetMicroSecsPerBeat();
                iLastTempoTick = iTick;
                llLastTempoTime = llTime;
            }
        }

        if ( vEvents ) vEvents->push_back( pEvent );
        i++;
    }

    m_Info.llTotalMicroSecs = llTime;
    m_Info.llFirstNote = max( 0LL, llFirstNote );

    midi_map.clear();
    midi_map_times.clear();
    midi_map_times_pos = 0;
}

void MIDI::ConnectNotes()
{
    std::vector<std::array<std::stack<MIDIChannelEvent*>, 128>> vStacks;
    vStacks.resize(m_vTracks.size() * 16);

    concurrency::parallel_for(size_t(0), m_vTracks.size(), [&](int track) {
        vector< MIDIEvent* >& vEvents = m_vTracks[track]->m_vEvents;
        for (size_t i = 0; i < vEvents.size(); i++) {
            if (vEvents[i]->GetEventType() == MIDIEvent::ChannelEvent)
            {
                MIDIChannelEvent* pEvent = reinterpret_cast<MIDIChannelEvent*>(vEvents[i]);
                MIDIChannelEvent::ChannelEventType eEventType = pEvent->GetChannelEventType();
                int iChannel = pEvent->GetChannel();
                int iNote = pEvent->GetParam1();
                int iVelocity = pEvent->GetParam2();
                auto& sStack = vStacks[track * 16 + iChannel][iNote];

                if (eEventType == MIDIChannelEvent::NoteOn && iVelocity > 0) {
                    sStack.push(pEvent);
                }
                else if (eEventType == MIDIChannelEvent::NoteOff || eEventType == MIDIChannelEvent::NoteOn) {
                    if (!sStack.empty()) {
                        auto pTop = sStack.top();
                        sStack.pop();
                        pTop->SetSister(pEvent);
                    }
                }
            }
        }
    });
}


//-----------------------------------------------------------------------------
// MIDITrack functions
//-----------------------------------------------------------------------------

MIDITrack::MIDITrack(MIDI& midi) : m_MIDI(midi) {};

MIDITrack::~MIDITrack( void )
{
    clear();
}

void MIDITrack::clear( void )
{
    // TODO: this is fucking awful
    for (auto it = m_vEvents.begin(); it != m_vEvents.end(); ++it) {
        if ((*it)->GetEventType() != MIDIEvent::EventType::ChannelEvent)
            delete* it;
    }
    m_vEvents.clear();
    m_TrackInfo.clear();
}

size_t MIDITrack::ParseTrack( const unsigned char *pcData, size_t iMaxSize, int iTrack )
{
    char pcBuf[4];
    size_t iTotal;
    int iTrkSize;

    // Reset first
    clear();

    // Read header
    if ( MIDI::ParseNChars( pcData, 4, iMaxSize, pcBuf ) != 4 )
        return 0;
    if ( MIDI::Parse32Bit( pcData + 4, iMaxSize - 4, &iTrkSize) != 4 )
        return 0;
    iTotal = 8;

    // Check header
    if ( strncmp( pcBuf, "MTrk", 4 ) != 0 )
        return 0;

    //return iTotal + ParseEvents( pcData + iTotal, iMaxSize - iTotal, iTrack );
    ParseEvents(pcData + iTotal, iMaxSize - iTotal, iTrack);
    return iTotal + iTrkSize;
}

size_t MIDITrack::ParseEvents( const unsigned char *pcData, size_t iMaxSize, int iTrack )
{
    int iDTCode = 0;
    size_t iTotal = 0, iCount = 0;
    MIDIEvent *pEvent = NULL;
    m_TrackInfo.iSequenceNumber = iTrack;

    do
    {
        // Create and parse the event
        iCount = 0;
        iDTCode = MIDIEvent::MakeNextEvent( m_MIDI, pcData + iTotal, iMaxSize - iTotal, iTrack, &pEvent );
        if ( iDTCode > 0 )
        {
            iCount = pEvent->ParseEvent( pcData + iDTCode + iTotal, iMaxSize - iDTCode - iTotal );
            if ( iCount > 0 )
            {
                iTotal += iDTCode + iCount;
                m_vEvents.push_back( pEvent );
                m_TrackInfo.AddEventInfo( *pEvent );
            }
            else
                delete pEvent;
        }
    }
    // Until we've parsed all the data, the last parse failed, or the event signals the end of track
    while ( iMaxSize - iTotal > 0 && iCount > 0 &&
            ( pEvent->GetEventType() != MIDIEvent::MetaEvent ||
              reinterpret_cast< MIDIMetaEvent* >( pEvent )->GetMetaEventType() != MIDIMetaEvent::EndOfTrack ) );

    std::sort(m_MIDI.midi_map_times.begin(), m_MIDI.midi_map_times.end());

    return iTotal;
}

// Computes some of the TrackInfo info
// DOES NOT DO: llTotalMicroSecs (because info's not available yet), iSequenceNumber default value (done in parse event)
void MIDITrack::MIDITrackInfo::AddEventInfo( const MIDIEvent &mEvent )
{
    //EventCount and TotalTicks
    this->iEventCount++;
    this->iTotalTicks = max( this->iTotalTicks, mEvent.GetAbsT() );

    switch ( mEvent.GetEventType() )
    {
        case MIDIEvent::MetaEvent:
        {
            const MIDIMetaEvent &mMetaEvent = reinterpret_cast< const MIDIMetaEvent & >( mEvent );
            MIDIMetaEvent::MetaEventType eMetaEventType = mMetaEvent.GetMetaEventType();
            switch ( eMetaEventType )
            {
                //SequenceName
                case MIDIMetaEvent::SequenceName:
                    this->sSequenceName.assign( reinterpret_cast< char* >( mMetaEvent.GetData() ), mMetaEvent.GetDataLen() );
                    break;
                //SequenceNumber
                case MIDIMetaEvent::SequenceNumber:
                    if ( mMetaEvent.GetDataLen() == 2)
                        MIDI::Parse16Bit( mMetaEvent.GetData(), 2, &this->iSequenceNumber );
                    break;
            }
            break;
        }
        case MIDIEvent::ChannelEvent:
        {
            const MIDIChannelEvent &mChannelEvent = reinterpret_cast< const MIDIChannelEvent & >( mEvent );
            MIDIChannelEvent::ChannelEventType eChannelEventType = mChannelEvent.GetChannelEventType();
            int iChannel = mChannelEvent.GetChannel();
            int iParam1 = mChannelEvent.GetParam1();
            int iParam2 = mChannelEvent.GetParam2();

            switch ( eChannelEventType )
            {
                case MIDIChannelEvent::NoteOn:
                    if ( iParam2 > 0 )
                    {
                        //MinNote and MaxNote
                        if ( !this->iNoteCount )
                        {
                            this->iMinNote = this->iMaxNote = iParam1;
                            this->iMaxVolume = iParam2;
                        }
                        else
                        {
                            this->iMinNote = min( iParam1, this->iMinNote );
                            this->iMaxNote = max( iParam1, this->iMaxNote );
                            this->iMaxVolume = max( iParam2, this->iMaxVolume );
                        }
                        //NoteCount
                        this->iNoteCount++;
                        this->iVolumeSum += iParam2;

                        //Channel info
                        if ( !this->aNoteCount[ iChannel ] )
                            this->iNumChannels++;
                        this->aNoteCount[ iChannel ]++;
                    }
                    break;
                // Should we break it down further?
                case MIDIChannelEvent::ProgramChange:
                    if ( this->aProgram[ iChannel ] != iParam1 )
                    {
                        if ( this->aNoteCount[ iChannel ] > 0 )
                            this->aProgram[ iChannel ] = 128; // Various
                        else
                            this->aProgram[ iChannel ] = iParam1;
                    }
                    break;
            }
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// MIDIEvent functions
//-----------------------------------------------------------------------------

MIDIEvent::EventType MIDIEvent::DecodeEventType( int iEventCode )
{
    if ( iEventCode < 0x80 ) return RunningStatus;
    if ( iEventCode < 0xF0 ) return ChannelEvent;
    if ( iEventCode < 0xFF ) return SysExEvent;
    return MetaEvent;
}

int MIDIEvent::MakeNextEvent( MIDI& midi, const unsigned char *pcData, size_t iMaxSize, int iTrack, MIDIEvent **pOutEvent )
{
    MIDIEvent *pPrevEvent = *pOutEvent;

    // Parse and check DT
    int iDT;
    int iTotal = MIDI::ParseVarNum ( pcData, iMaxSize, &iDT );
    if (iTotal == 0 || iMaxSize - iTotal < 1 ) return 0;

    // Parse and decode event code
    int iEventCode = pcData[iTotal];
    EventType eEventType = DecodeEventType( iEventCode );
    iTotal++;

    // Use previous event code for running status
    if ( eEventType == RunningStatus && pPrevEvent)
    {
        iEventCode = pPrevEvent->GetEventCode();
        eEventType = DecodeEventType( iEventCode );
        iTotal--;
    }

    // Make the object
    switch ( eEventType )
    {
        case ChannelEvent: *pOutEvent = midi.AllocChannelEvent(); break;
        case MetaEvent: *pOutEvent = new MIDIMetaEvent(); break;
        case SysExEvent: *pOutEvent = new MIDISysExEvent(); break;
    }

    (*pOutEvent)->m_eEventType = eEventType;
    (*pOutEvent)->m_iEventCode = iEventCode;
    (*pOutEvent)->m_iTrack = iTrack;
    (*pOutEvent)->m_iAbsT = iDT;
    if ( pPrevEvent ) (*pOutEvent)->m_iAbsT += pPrevEvent->m_iAbsT;

    auto& key = midi.midi_map[(*pOutEvent)->m_iAbsT];
    key.second.push_back(*pOutEvent);
    key.first = key.second.begin();
    if (key.second.size() == 1)
        midi.midi_map_times.push_back((*pOutEvent)->m_iAbsT);

    return iTotal;
}

int MIDIChannelEvent::ParseEvent( const unsigned char *pcData, size_t iMaxSize )
{
    // Split up the event code
    m_eChannelEventType = static_cast< ChannelEventType >( m_iEventCode >> 4 );
    m_cChannel = m_iEventCode & 0xF;

    // Parse one parameter
    if ( m_eChannelEventType == ProgramChange || m_eChannelEventType == ChannelAftertouch )
    {
        if ( iMaxSize < 1 ) return 0;
        m_cParam1 = pcData[0];
        m_cParam2 = 0;
        return 1;
    }
    // Parse two parameters
    else
    {
        if ( iMaxSize < 2 ) return 0;
        m_cParam1 = pcData[0];
        m_cParam2 = pcData[1];
        return 2;
    }
}

int MIDIMetaEvent::ParseEvent( const unsigned char *pcData, size_t iMaxSize )
{
    if ( iMaxSize < 1 ) return 0;

    // Parse the code and the length
    m_eMetaEventType = static_cast< MetaEventType >( pcData[0] );
    int iCount = MIDI::ParseVarNum( pcData + 1, iMaxSize - 1, &m_iDataLen );
    if ( iCount == 0 || iMaxSize < 1 + iCount + m_iDataLen ) return 0;

    // Get the data
    if ( m_iDataLen > 0 )
    {
        m_pcData = new unsigned char[m_iDataLen];
        memcpy( m_pcData, pcData + 1 + iCount, m_iDataLen );
    }

    return 1 + iCount + m_iDataLen;
}

// NOTE: this is INCOMPLETE. Data is parsed but not fully interpreted:
// divided messages don't know about each other
int MIDISysExEvent::ParseEvent( const unsigned char *pcData, size_t iMaxSize )
{
    if ( iMaxSize < 1 ) return 0;

    // Parse the code and the length
    int iCount = MIDI::ParseVarNum( pcData, iMaxSize, &m_iDataLen );
    if ( iCount == 0 || iMaxSize < iCount + m_iDataLen ) return 0;

    // Get the data
    if ( m_iDataLen > 0 )
    {
        m_pcData = new unsigned char[m_iDataLen];
        memcpy( m_pcData, pcData + iCount, m_iDataLen );
        if ( m_iEventCode == 0xF0 && m_pcData[ m_iDataLen - 1 ] != 0xF7 )
            m_bHasMoreData = true;
    }

    return iCount + m_iDataLen;
}


//-----------------------------------------------------------------------------
// Parsing helpers
//-----------------------------------------------------------------------------

//Parse a variable length number from MIDI data
int MIDI::ParseVarNum( const unsigned char *pcData, size_t iMaxSize, int *piOut )
{
    if ( !pcData || !piOut || iMaxSize <= 0 )
        return 0;

    *piOut = 0;
    int i = 0;
    do
    {
        *piOut = ( *piOut << 7 ) | ( pcData[i] & 0x7F );
        i++;
    }
    while ( i < 4 && i < iMaxSize && ( pcData[i - 1] & 0x80 ) );

    return i;
}

//Parse 32 bits of data. Big Endian.
int MIDI::Parse32Bit( const unsigned char *pcData, size_t iMaxSize, int *piOut )
{
    if ( !pcData || !piOut || iMaxSize < 4 )
        return 0;

    *piOut = pcData[0];
    *piOut = ( *piOut << 8 ) | pcData[1];
    *piOut = ( *piOut << 8 ) | pcData[2];
    *piOut = ( *piOut << 8 ) | pcData[3];

    return 4;
}

//Parse 24 bits of data. Big Endian.
int MIDI::Parse24Bit( const unsigned char *pcData, size_t iMaxSize, int *piOut )
{
    if ( !pcData || !piOut || iMaxSize < 3 )
        return 0;

    *piOut = pcData[0];
    *piOut = ( *piOut << 8 ) | pcData[1];
    *piOut = ( *piOut << 8 ) | pcData[2];

    return 3;
}

//Parse 16 bits of data. Big Endian.
int MIDI::Parse16Bit( const unsigned char *pcData, size_t iMaxSize, int *piOut )
{
    if ( !pcData || !piOut || iMaxSize < 2 )
        return 0;

    *piOut = pcData[0];
    *piOut = ( *piOut << 8 ) | pcData[1];

    return 2;
}

//Parse a bunch of characters
int MIDI::ParseNChars( const unsigned char *pcData, int iNChars, size_t iMaxSize, char *pcOut )
{
    if ( !pcData || !pcOut || iMaxSize <= 0 )
        return 0;

    size_t iSize = min( iNChars, iMaxSize );
    memcpy( pcOut, pcData, iSize );

    return iSize;
}

//-----------------------------------------------------------------------------
// Device classes
//-----------------------------------------------------------------------------

// Port management functions
int MIDIOutDevice::GetNumDevs() const
{
    return midiOutGetNumDevs();
}

wstring MIDIOutDevice::GetDevName( int iDev ) const
{
    MIDIOUTCAPS moc;
    if ( midiOutGetDevCaps( iDev, &moc, sizeof( MIDIOUTCAPS ) ) == MMSYSERR_NOERROR )
        return moc.szPname;
    return wstring();
}

bool MIDIOutDevice::Open( int iDev )
{
    if ( m_bIsOpen ) Close();
    m_iDevice = iDev;
    m_sDevice = GetDevName( iDev );

    MMRESULT mmResult = midiOutOpen( &m_hMIDIOut, iDev, ( DWORD_PTR )MIDIOutProc, ( DWORD_PTR )this, CALLBACK_FUNCTION );
    m_bIsOpen = ( mmResult == MMSYSERR_NOERROR );
    return m_bIsOpen;
}

void MIDIOutDevice::Close()
{
    if ( !m_bIsOpen ) return;

    midiOutReset( m_hMIDIOut );
    midiOutClose( m_hMIDIOut );
    m_bIsOpen = false;
}

// Specialized midi functions
void MIDIOutDevice::AllNotesOff()
{
    PlayEventAcrossChannels( 0xB0, 0x7B, 0x00 ); // All notes off
    PlayEventAcrossChannels( 0xB0, 0x40, 0x00 ); // Sustain off
}

void MIDIOutDevice::AllNotesOff( const vector< int > &vChannels )
{
    PlayEventAcrossChannels( 0xB0, 0x7B, 0x00, vChannels );
    PlayEventAcrossChannels( 0xB0, 0x40, 0x00, vChannels );
}

void MIDIOutDevice::SetVolume( double dVolume )
{
    DWORD dwVolume = static_cast< DWORD >( 0xFFFF * dVolume + 0.5 );
    midiOutSetVolume( m_hMIDIOut, dwVolume | ( dwVolume << 16 ) );
}

// Play events
bool MIDIOutDevice::PlayEventAcrossChannels( unsigned char cStatus, unsigned char cParam1, unsigned char cParam2 )
{
    if ( !m_bIsOpen ) return false;

    cStatus &= 0xF0;
    bool bResult = true;
    for ( int i = 0; i < 16; i++ )
        bResult &= PlayEvent( cStatus + i, cParam1, cParam2 );

    return bResult;
}

bool MIDIOutDevice::PlayEventAcrossChannels( unsigned char cStatus, unsigned char cParam1, unsigned char cParam2, const vector< int > &vChannels )
{
    if ( !m_bIsOpen ) return false;

    cStatus &= 0xF0;
    bool bResult = true;
    for ( vector< int >::const_iterator it = vChannels.begin(); it != vChannels.end(); ++it )
        bResult &= PlayEvent( cStatus + *it, cParam1, cParam2 );

    return bResult;
}

bool MIDIOutDevice::PlayEvent( unsigned char cStatus, unsigned char cParam1, unsigned char cParam2 )
{
    if ( !m_bIsOpen ) return false;
    return midiOutShortMsg( m_hMIDIOut, ( cParam2 << 16 ) + ( cParam1 << 8 ) + cStatus ) == MMSYSERR_NOERROR;
}

void CALLBACK MIDIOutDevice::MIDIOutProc( HMIDIOUT hmo, UINT wMsg, DWORD_PTR dwInstance,
                                          DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    MIDIOutDevice *pOutDevice = reinterpret_cast< MIDIOutDevice* >( dwInstance );

    switch ( wMsg )
    {
        case MOM_CLOSE:
        {
        }
    }
}