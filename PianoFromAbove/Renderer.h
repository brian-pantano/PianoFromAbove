/*************************************************************************************************
*
* File: Renderer.h
*
* Description: Defines rendering objects. Only one for now.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include <vector>

class D3D12Renderer
{
public:
    enum FontSize { Small, SmallBold, SmallComic, Medium, Large };

    D3D12Renderer();
    ~D3D12Renderer();

    HRESULT Init( HWND hWnd, bool bLimitFPS );
    HRESULT ResetDeviceIfNeeded();
    HRESULT ResetDevice();
    HRESULT Clear( DWORD color );
    HRESULT BeginScene();
    HRESULT EndScene();
    HRESULT Present();
    HRESULT BeginText();
    HRESULT DrawTextW( const WCHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars = -1 );
    HRESULT DrawTextA( const CHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars = -1 );
    HRESULT EndText();
    HRESULT DrawRect( float x, float y, float cx, float cy, DWORD color );
    HRESULT DrawRect( float x, float y, float cx, float cy,
                      DWORD c1, DWORD c2, DWORD c3, DWORD c4 );
    HRESULT DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD color );
    HRESULT DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                       DWORD c1, DWORD c2, DWORD c3, DWORD c4 );
    HRESULT RenderBatch(bool bWithDepth = false);

    bool GetLimitFPS() const { return m_bLimitFPS; }
    HRESULT SetLimitFPS( bool bLimitFPS );

    int GetBufferWidth() const { return m_iBufferWidth; }
    int GetBufferHeight() const { return m_iBufferHeight; }

protected:
    int m_iBufferWidth, m_iBufferHeight;
    bool m_bLimitFPS;
};