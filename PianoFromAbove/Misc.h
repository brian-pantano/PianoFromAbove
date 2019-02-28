/*************************************************************************************************
*
* File: Misc.h
*
* Description: Defines small helper objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <string>
using namespace std;

//The timer
class Timer
{
public:
    // Initializes variables
    Timer();
    ~Timer();

    // The various clock actions
    void Start();
    void Pause();
    void Unpause();

    // Gets the timer's time
    double GetSecs();
    long long GetMicroSecs();
    long long GetTicks();
    long long GetTicksPerSec() { return m_llTicksPerSec; }

    // Status accessors
    bool IsStarted() { return m_bStarted; }
    bool IsPaused() { return m_bPaused; }

private:
    static const long long m_llPrecisionLimit = 1000000000ll;
    static const long long m_llPrecisionThrottle = 1000ll;

    // Timer stuff
    long long GetRawTicks();
    long long m_llStartTicks;
    long long m_llTicksPerSec;
    bool m_bHighPrecision;
    bool m_bTooPrecise;

    // Ticks stored when the timer was paused
    long long m_llPausedTicks;

    // Timer status
    bool m_bStarted;
    bool m_bPaused;
};

//-----------------------------------------------------------------------------
// Small utility functions
//-----------------------------------------------------------------------------

class Util
{
public:
    static wchar_t* StringToWstring( const string &s );
    static char* WstringToString( const wstring &s );
    static void ParseLongHex( const string &sText, string &sVal );
    static bool MD5( const unsigned char *pData, int iSize, string &sOut );
    static unsigned RandColor();
    static void RGBtoHSV( int R, int G, int B, int &H, int &S, int &V );
    static void HSVtoRGB( int H, int S, int V, int &R, int &G, int &B );
    static void CommaPrintf( TCHAR buf[32], int iVal );
private:
    static char m_sBuf[16384];
    static wchar_t m_wsBuf[16384];
};

//-----------------------------------------------------------------------------
// The thread safe queue (TSQueue) class. Only safe for a single producer and
// a single consumer
//-----------------------------------------------------------------------------

template < typename T >
class TSQueue
{
public:
    TSQueue() : m_iWrite(0), m_iRead(0) { }
    bool Push( const T &tElement );
    bool Pop( T &tElement );

    void ForcePush( const T &tElement ) { while ( !Push( tElement) ); }

private:
    static const int QueueSize = 1024;
    T m_tQueue[QueueSize]; // Does this need to be volatile? Unsure. Doesn't work with MSG if volatile.
    volatile int m_iWrite;
    volatile int m_iRead;
};

template< class T >
bool TSQueue<T>::Push( const T &tElement )
{
    int iNextElement = ( m_iWrite + 1 ) % QueueSize;

    // Is the queue full?
    if ( iNextElement == m_iRead ) return false;

    // Push the element. Order of execution is very important.
    m_tQueue[m_iWrite] = tElement;
    m_iWrite = iNextElement;

    return true;
}

template< class T >
bool TSQueue<T>::Pop( T &tElement )
{
    // Is the queue empty?
    if ( m_iWrite == m_iRead ) return false;

    // Compute where to read. Read. Update read pointer. Order very important.
    int iNextElement = ( m_iRead + 1 ) % QueueSize;
    tElement = m_tQueue[m_iRead];
    m_iRead = iNextElement;

    return true;
}