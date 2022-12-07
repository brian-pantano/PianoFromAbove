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
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <vector>

struct RectVertex {
    float x;
    float y;
    DWORD color;
};

class D3D12Renderer
{
public:
    enum FontSize { Small, SmallBold, SmallComic, Medium, Large };

    D3D12Renderer();
    ~D3D12Renderer();

    std::tuple<HRESULT, const char*> Init( HWND hWnd, bool bLimitFPS );
    HRESULT ResetDeviceIfNeeded();
    HRESULT ResetDevice();
    HRESULT ClearAndBeginScene( DWORD color );
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

    HRESULT WaitForGPU();
    std::wstring GetAdapterName();

private:
    static constexpr unsigned FrameCount = 3;
    static constexpr unsigned RectsPerPass = 10922; // Relatively low limit, allows using a 16-bit index buffer

    int m_iBufferWidth = 0;
    int m_iBufferHeight = 0;
    bool m_bLimitFPS = false;

    IDXGIFactory4* m_pFactory = nullptr;
    IDXGIAdapter3* m_pAdapter = nullptr;
    ID3D12Device9* m_pDevice = nullptr;
    ID3D12CommandQueue* m_pCommandQueue = nullptr;
    IDXGISwapChain3* m_pSwapChain = nullptr;
    ID3D12DescriptorHeap* m_pRTVDescriptorHeap = nullptr;
    UINT m_uRTVDescriptorSize = 0;
    ID3D12Resource2* m_pRenderTargets[FrameCount] = {};
    ID3D12CommandAllocator* m_pCommandAllocator[FrameCount] = {};
    ID3D12RootSignature* m_pRectRootSignature = nullptr;
    ID3D12PipelineState* m_pRectPipelineState = nullptr;
    ID3D12GraphicsCommandList6* m_pCommandList = nullptr;
    ID3D12Fence1* m_pFence = nullptr;
    HANDLE m_hFenceEvent = NULL;
    ID3D12Resource* m_pIndexBuffer = nullptr;

    UINT m_uFrameIndex = 0;
    UINT64 m_pFenceValues[FrameCount] = {};

    std::vector<RectVertex> m_vRectsIntermediate;
};