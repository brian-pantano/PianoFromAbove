/*************************************************************************************************
*
* File: Renderer.cpp
*
* Description: Implements the rendering objects. Just a wrapper to Direct3D.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#include "Renderer.h"

HRESULT Renderer::SetLimitFPS( bool bLimitFPS )
{
    if ( bLimitFPS != m_bLimitFPS )
    {
        m_bLimitFPS = bLimitFPS;
        return ResetDevice();
    }
    return S_OK;
}

D3D9Renderer::~D3D9Renderer()
{
    DestroyDeviceObjects();

    if( m_pTextSprite ) m_pTextSprite->Release();
    if( m_pSmallFont ) m_pSmallFont->Release();
    if( m_pSmallBoldFont ) m_pSmallBoldFont->Release();
    if( m_pSmallComicFont ) m_pSmallComicFont->Release();
    if( m_pMediumFont ) m_pMediumFont->Release();
    if( m_pLargeFont ) m_pLargeFont->Release();
    
    if( m_pd3dDevice ) m_pd3dDevice->Release();

    if( m_pD3D ) m_pD3D->Release();
}

void D3D9Renderer::DestroyDeviceObjects()
{
    m_pTextSprite->OnLostDevice();
    m_pSmallFont->OnLostDevice();
    m_pSmallBoldFont->OnLostDevice();
    m_pSmallComicFont->OnLostDevice();
    m_pMediumFont->OnLostDevice();
    m_pLargeFont->OnLostDevice();

    if( m_pVertexBuffer ) m_pVertexBuffer->Release();
    if( m_pStaticVertexBuffer ) ReleaseStaticBuffer();

    m_bIsDeviceValid = false;
}

HRESULT D3D9Renderer::Init( HWND hWnd, bool bLimitFPS )
{
    HRESULT hr;

    // Create the D3D object.
    if( NULL == ( m_pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice
    ZeroMemory( &m_d3dPP, sizeof( D3DPRESENT_PARAMETERS ) );
    m_d3dPP.Windowed = TRUE;
    m_d3dPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dPP.BackBufferFormat = D3DFMT_UNKNOWN;
    m_d3dPP.BackBufferWidth = 0;
    m_d3dPP.BackBufferHeight = 0;
    m_d3dPP.PresentationInterval = ( bLimitFPS ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE );
    
    // Create the D3DDevice
    if( FAILED( hr = m_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, //D3DDEVTYPE_REF
                                           D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                           &m_d3dPP, &m_pd3dDevice ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateSprite( m_pd3dDevice, &m_pTextSprite ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( m_pd3dDevice, 15, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Tahoma", &m_pSmallFont ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( m_pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Tahoma", &m_pSmallBoldFont ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( m_pd3dDevice, 20, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Comic Sans MS", &m_pSmallComicFont ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( m_pd3dDevice, 25, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Tahoma", &m_pMediumFont ) ) )
        return hr;

    if( FAILED( hr = D3DXCreateFont( m_pd3dDevice, 35, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                     L"Tahoma", &m_pLargeFont ) ) )
        return hr;

    if ( FAILED( hr = RestoreDeviceObjects() ) )
        return hr;

    m_iBufferWidth = m_d3dPP.BackBufferWidth;
    m_iBufferHeight = m_d3dPP.BackBufferHeight;
    m_bLimitFPS = bLimitFPS;
    m_bIsDeviceValid = true;

    return S_OK;
}

HRESULT D3D9Renderer::RestoreDeviceObjects()
{
    HRESULT hr;

    if ( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( VertexBufferSize,
                                                        D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, SCREEN_VERTEX::FVF,
                                                        D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL) ) )
        return hr;
    m_iTriangle = 0;

    m_pTextSprite->OnResetDevice();
    m_pSmallFont->OnResetDevice();
    m_pSmallBoldFont->OnResetDevice();
    m_pSmallComicFont->OnResetDevice();
    m_pMediumFont->OnResetDevice();
    m_pLargeFont->OnResetDevice();

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );

    m_pd3dDevice->SetFVF( SCREEN_VERTEX::FVF );

    return S_OK;
}

HRESULT D3D9Renderer::ResetDeviceIfNeeded()
{
    if ( !m_bIsDeviceValid )
    {
        HRESULT hr = m_pd3dDevice->TestCooperativeLevel();
        if ( hr == D3DERR_DEVICENOTRESET )
            hr = ResetDevice();
        if ( FAILED( hr ) )
            return hr;
    }
    return S_OK;
}

HRESULT D3D9Renderer::ResetDevice()
{
    HRESULT hr;
    
    // Destroy the objects and reinitialize
    if ( m_bIsDeviceValid )
        DestroyDeviceObjects();

    // Reset the device
    m_d3dPP.BackBufferHeight = 0;
    m_d3dPP.BackBufferWidth = 0;
    m_d3dPP.PresentationInterval = ( m_bLimitFPS ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE );
    if ( FAILED( hr = m_pd3dDevice->Reset( &m_d3dPP ) ) )
        return hr;

    // Restore the device objects
    if ( FAILED( hr = RestoreDeviceObjects() ) )
        return hr;

    m_iBufferWidth = m_d3dPP.BackBufferWidth;
    m_iBufferHeight = m_d3dPP.BackBufferHeight;
    m_bIsDeviceValid = true;
    return S_OK;
}

HRESULT D3D9Renderer::Clear( DWORD color )
{
    return m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0 );
}

HRESULT D3D9Renderer::BeginScene()
{
    return m_pd3dDevice->BeginScene();
}

HRESULT D3D9Renderer::EndScene()
{
    FlushBuffer();
    return m_pd3dDevice->EndScene();
}

HRESULT D3D9Renderer::BeginText()
{
    FlushBuffer();
    return m_pTextSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );
}

HRESULT D3D9Renderer::DrawTextW( const WCHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars )
{
    LPD3DXFONT pFont = ( fsFont == Small ? m_pSmallFont :
                         fsFont == SmallBold ? m_pSmallBoldFont :
                         fsFont == SmallComic ? m_pSmallComicFont :
                         fsFont == Medium ? m_pMediumFont :
                         fsFont == Large ? m_pLargeFont : m_pMediumFont );
    
    if ( !pFont->DrawTextW( m_pTextSprite, sText, iChars, rcPos, dwFormat, D3DXCOLOR( dwColor ) ) )
        return E_FAIL;
    
    return S_OK;
}

HRESULT D3D9Renderer::DrawTextA( const CHAR *sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars )
{
    LPD3DXFONT pFont = ( fsFont == Small ? m_pSmallFont :
                         fsFont == SmallBold ? m_pSmallBoldFont :
                         fsFont == SmallComic ? m_pSmallComicFont :
                         fsFont == Medium ? m_pMediumFont :
                         fsFont == Large ? m_pLargeFont : m_pMediumFont );
    
    if ( !pFont->DrawTextA( m_pTextSprite, sText, -1, rcPos, dwFormat, D3DXCOLOR( dwColor ) ) )
        return E_FAIL;
    
    return S_OK;
}

HRESULT D3D9Renderer::EndText()
{
    return m_pTextSprite->End();
}

HRESULT D3D9Renderer::Present()
{
    HRESULT hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    if ( hr == D3DERR_DEVICELOST )
        DestroyDeviceObjects();
    return hr;
}

HRESULT D3D9Renderer::DrawRect( float x, float y, float cx, float cy, DWORD color )
{
    return DrawRect( x, y, cx, cy, color, color, color, color );
}

HRESULT D3D9Renderer::DrawRect( float x, float y, float cx, float cy,
                                DWORD c1, DWORD c2, DWORD c3, DWORD c4 )
{
    x -= 0.5f;
    y -= 0.5f;

    SCREEN_VERTEX vertices[6] =
    {
        x,  y,            0.5f, 1.0f, c1,
        x + cx, y,        0.5f, 1.0f, c2,
        x + cx, y + cy,   0.5f, 1.0f, c3,
        x,  y,            0.5f, 1.0f, c1,
        x + cx, y + cy,   0.5f, 1.0f, c3,
        x,  y + cy,       0.5f, 1.0f, c4
    };

    return Blit( vertices, 2 );
}

HRESULT D3D9Renderer::DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD color )
{
    return DrawSkew( x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color );
}

HRESULT D3D9Renderer::DrawSkew( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                                DWORD c1, DWORD c2, DWORD c3, DWORD c4 )
{
    SCREEN_VERTEX vertices[6] =
    {
        x1 - 0.5f, y1 - 0.5f, 0.5f, 1.0f, c1,
        x2 - 0.5f, y2 - 0.5f, 0.5f, 1.0f, c2,
        x3 - 0.5f, y3 - 0.5f, 0.5f, 1.0f, c3,
        x1 - 0.5f, y1 - 0.5f, 0.5f, 1.0f, c1,
        x3 - 0.5f, y3 - 0.5f, 0.5f, 1.0f, c3,
        x4 - 0.5f, y4 - 0.5f, 0.5f, 1.0f, c4
    };

    return Blit( vertices, 2 );
}

HRESULT D3D9Renderer::Blit( SCREEN_VERTEX *vertices, int iTriangles )
{
    if ( m_bStatic )
    {
        memcpy( m_pStaticVertexData + m_iStaticTriangle * 3 * sizeof( SCREEN_VERTEX ), vertices, iTriangles * 3 * sizeof( SCREEN_VERTEX ) );
        m_iStaticTriangle += 2;
    }
    else
    {
        PrepBuffer( iTriangles );
        memcpy( m_pVertexData + m_iTriangle * 3 * sizeof( SCREEN_VERTEX ), vertices, iTriangles * 3 * sizeof( SCREEN_VERTEX ) );
        m_iTriangle += 2;
    }
    return S_OK;
}

HRESULT D3D9Renderer::PrepBuffer( int iTriangles )
{
    if ( m_iTriangle > MaxTriangles )
        return E_FAIL;
    if ( m_iTriangle == 0 )
        return m_pVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pVertexData ), D3DLOCK_DISCARD );
    if ( m_iTriangle + iTriangles <= MaxTriangles )
        return S_OK;

    FlushBuffer();
    return m_pVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pVertexData ), D3DLOCK_DISCARD );
}

HRESULT D3D9Renderer::FlushBuffer()
{
    if ( m_iTriangle == 0 )
        return S_OK;

    m_pVertexBuffer->Unlock();
    m_pd3dDevice->SetStreamSource( 0, m_pVertexBuffer, 0, sizeof( SCREEN_VERTEX ) );
    HRESULT hr = m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_iTriangle );
    m_iTriangle = 0;
    return hr;
}

HRESULT D3D9Renderer::BeginStaticBuffer( int iTriangles )
{
    HRESULT hr;

    if (iTriangles > m_iStaticMaxTriangles)
    {
        ReleaseStaticBuffer();
        if ( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( sizeof( SCREEN_VERTEX ) * 3 * iTriangles,
                                                            D3DUSAGE_WRITEONLY, SCREEN_VERTEX::FVF,
                                                            D3DPOOL_DEFAULT, &m_pStaticVertexBuffer, NULL) ) )
            return hr;
    }

    if ( FAILED( hr = m_pStaticVertexBuffer->Lock( 0, 0, reinterpret_cast< void** >( &m_pStaticVertexData ), 0 ) ) )
        return hr;

    m_bStatic = true;
    m_iStaticTriangle = 0;
    m_iStaticMaxTriangles = iTriangles;
    return S_OK;
}

HRESULT D3D9Renderer::EndStaticBuffer()
{
    m_bStatic = false;
    return m_pStaticVertexBuffer->Unlock();
}

HRESULT D3D9Renderer::DrawStaticBuffer() {
    if ( m_iStaticTriangle == 0 )
        return S_OK;

    FlushBuffer();
    m_pd3dDevice->SetStreamSource( 0, m_pStaticVertexBuffer, 0, sizeof( SCREEN_VERTEX ) );
    return m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_iStaticTriangle );
}

HRESULT D3D9Renderer::ReleaseStaticBuffer()
{
    m_bStatic = false;
    m_iStaticTriangle = m_iStaticMaxTriangles = 0;
    return m_pStaticVertexBuffer->Release();
}