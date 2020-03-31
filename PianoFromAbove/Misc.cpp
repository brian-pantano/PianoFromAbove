/*************************************************************************************************
*
* File: Misc.cpp
*
* Description: Implements small helper objects
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include <Windows.h>
#include <Wincrypt.h>
#include <tchar.h>

#include <algorithm>
using namespace std;

#include "Misc.h"

//-----------------------------------------------------------------------------
// The Timer class
//-----------------------------------------------------------------------------

Timer::~Timer()
{
    if ( !m_bManualTimer)
        timeEndPeriod( 1 );
}

void Timer::Init(bool manual) {
    // Get the frequency. This should be done in a static constructor. Oh well.
    LARGE_INTEGER liFreq = { 0 };
    m_bManualTimer = manual;
    m_llManualTicks = 0;
    if (!m_bManualTimer) {
        timeBeginPeriod(1);
        m_llTicksPerSec = 1000;
    }
    else {
        m_llManualTicksPerFrame = 100;
        m_llTicksPerSec = 10000;
    }

    // Initialize status
    m_bStarted = m_bPaused = false;
    m_llStartTicks = m_llPausedTicks = 0;
}

// Start/reset the timer
void Timer::Start()
{
    m_bStarted = true;
    m_bPaused = false;
    m_llStartTicks = GetRawTicks();
}

// Pause
void Timer::Pause()
{
    if ( m_bStarted && !m_bPaused )
    {
        m_bPaused = true;
        m_llPausedTicks = GetRawTicks() - m_llStartTicks;
    }
}

// Unpause or start
void Timer::Unpause()
{
    if ( !m_bStarted )
        Start();

    if ( !m_bPaused )
    {
        m_bPaused = false;
        m_llStartTicks = GetRawTicks() - m_llPausedTicks;
    }
}

// Elapsed ticks since start
long long Timer::GetTicks()
{
    if ( m_bStarted )
    {
        if ( m_bPaused )
            return m_llPausedTicks;
        else
            return GetRawTicks() - m_llStartTicks;
    }

    return 0;
}

// Elapsed micro seconds since start
// Assuming 3,000,000 ticks/sec this will overflow in 35 days
long long Timer::GetMicroSecs()
{
    return ( GetTicks() * 1000000LL ) / m_llTicksPerSec;
}

double Timer::GetSecs()
{
    return static_cast< double >( GetTicks() ) / m_llTicksPerSec;
}

// Private. Hits the API for the tick count.
long long Timer::GetRawTicks()
{
    if ( m_bManualTimer )
        return m_llManualTicks;
    else
        return timeGetTime();
}

void Timer::AddManualTime(long long time)
{
    m_llManualTicks += time;
}

void Timer::SetFrameRate(unsigned rate)
{
    m_llTicksPerSec = (long long)rate * 100;
    m_dFramerate = rate;
}

void Timer::IncrementFrame()
{
    m_llManualTicks += m_llManualTicksPerFrame;
}

//-----------------------------------------------------------------------------
// Small utility functions
//-----------------------------------------------------------------------------

// The next three functions assume the returned buffer will immediately be consumed by the string constructor
// Doesn't return string or wstring in order to minimize the number of copying.
char Util::m_sBuf[16384];
wchar_t Util::m_wsBuf[16384];

wchar_t* Util::StringToWstring( const string &s )
{
    int result = MultiByteToWideChar( CP_UTF8, 0, s.c_str(), -1, m_wsBuf, sizeof( m_wsBuf ) / sizeof( wchar_t ) );
    if ( !result ) m_wsBuf[0] = L'\0';
    return m_wsBuf;
}

char* Util::WstringToString( const wstring &s )
{
    int result = WideCharToMultiByte( CP_UTF8, 0, s.c_str(), -1, m_sBuf, sizeof( m_sBuf ), NULL, NULL );
    if ( !result ) m_sBuf[0] = '\0';
    return m_sBuf;
}

void Util::ParseLongHex( const string &sText, string &sVal )
{
    int iLen = ( (int)sText.length() + 1 ) / 2;
    sVal.clear();
    sVal.resize( iLen );
    
    int iStart = sText.length() % 2;
    for ( int i = iStart; i < iLen; i++ )
    {
        int byte = 0;
        if ( sscanf_s( sText.c_str() + 2 * i - iStart, "%2X", &byte ) > 0 )
            sVal[sVal.length() - 1 - i] = byte;
    }

    if ( iStart == 1 )
    {
        char buf[3] = { '0', sText[0] };
        int byte = 0;
        if ( sscanf_s( buf, "%2X", &byte ) > 0 )
            sVal[sVal.length() - 1] = byte;
    }
}

bool Util::MD5( const unsigned char *pData, int iSize, string &sOut )
{
    HCRYPTPROV hCryptProv;
    HCRYPTHASH hHash;
    BYTE pHash[32] = { 0 };
    DWORD iHashLen = sizeof( pHash );

    bool bSuccess = ( CryptAcquireContext( &hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET ) &&
                      CryptCreateHash( hCryptProv, CALG_MD5, 0, 0, &hHash ) && 
                      CryptHashData( hHash, pData, iSize, 0 ) &&
                      CryptGetHashParam( hHash, HP_HASHVAL, pHash, &iHashLen, 0 ) );

    CryptDestroyHash( hHash );
    CryptReleaseContext( hCryptProv, 0 );
    sOut = reinterpret_cast< char* >( pHash );
    return bSuccess;
}

unsigned Util::RandColor()
{
    int R, G, B;
    HSVtoRGB( rand() % 360, rand() % 40 + 60, rand() % 20 + 80, R, G, B );
    return ( B << 16 ) | ( G << 8 ) | R;
}

void Util::RGBtoHSV( int R, int G, int B, int &H, int &S, int &V )
{
    double dR = R / 255.0, dG = G / 255.0, dB = B / 255.0;
    double M = max ( max( dR, dG ), dB );
    double m = min ( min( dR, dG ), dB );
    double C = M - m;

    double dH = C ==  0 ? 0.0 :
                M == dR ? ( dG - dB ) / C :
                M == dG ? ( dB - dR ) / C + 2 :
                M == dB ? ( dR - dG ) / C + 4 : 0.0;
    if ( dH < 0 ) dH += 6.0;
    double dV = M;
    double dS = ( dV > 0.0 ? C / dV : 0.0 );

    H = static_cast< int >( dH * 60.0 + 0.5 ) % 360;
    V = static_cast< int >( dV * 100.0 + 0.5 );
    S = static_cast< int >( dS * 100.0 + 0.5 );
}

void Util::HSVtoRGB( int H, int S, int V, int &R, int &G, int &B )
{
    double dH = H / 60.0, dS = S / 100.0, dV = V / 100.0;
    double C = dV * dS;
    double m = dV - C;

    double dR1 = 0.0, dG1 = 0.0, dB1 = 0.0;
    if ( dH >= 0 && dH < 1.0 )
    {
        dR1 = C;
        dG1 = C * dH;
        dB1 = 0;
    }
    if ( dH >= 1 && dH < 2.0 )
    {
        dR1 = C * ( 2.0 - dH );
        dG1 = C;
        dB1 = 0;
    }
    if ( dH >= 2 && dH < 3.0 )
    {
        dR1 = 0;
        dG1 = C;
        dB1 = C * ( dH - 2.0 );
    }
    if ( dH >= 3 && dH < 4.0 )
    {
        dR1 = 0;
        dG1 = C * ( 4.0 - dH );
        dB1 = C;
    }
    if ( dH >= 4 && dH < 5.0 )
    {
        dR1 = C * ( dH - 4.0 );
        dG1 = 0;
        dB1 = C;
    }
    if ( dH >= 5 && dH < 6.0 )
    {
        dR1 = C;
        dG1 = 0;
        dB1 = C * ( 6.0 - dH );
    }

    R = static_cast< int >( ( dR1 + m ) * 255.0 + 0.5 );
    G = static_cast< int >( ( dG1 + m ) * 255.0 + 0.5 );
    B = static_cast< int >( ( dB1 + m ) * 255.0 + 0.5 );
}

void Util::CommaPrintf( TCHAR buf[32], int iVal )
{
    int iAbsVal = abs( iVal );
    if ( iAbsVal < 1000 ) _stprintf_s( buf, 32, TEXT( "%d" ), iVal );
    else if ( iAbsVal < 1000000 ) _stprintf_s( buf, 32, TEXT( "%d,%03d" ), iVal / 1000, iAbsVal % 1000 );
    else if ( iAbsVal < 1000000000 ) _stprintf_s( buf, 32, TEXT( "%d,%03d,%03d" ), iVal / 1000000, ( iAbsVal / 1000 ) % 1000, iAbsVal % 1000 );
    else _stprintf_s( buf, 32, TEXT( "%d,%03d,%03d,%03d" ), iVal / 1000000000, ( iAbsVal / 1000000 ) % 1000, ( iAbsVal / 1000 ) % 1000, iAbsVal % 1000 );
}