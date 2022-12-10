/*************************************************************************************************
*
> File: Renderer.h
*
> Description: Defines rendering objects. Only one for now.
*
> Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

using Microsoft::WRL::ComPtr;

enum class Pipeline {
    Rect,
    Note,
};

struct RectVertex {
    float x;
    float y;
    DWORD color;
};

struct NoteData {
    uint8_t key;
    uint8_t channel;
    uint16_t track;
    float pos;
    float length;
};

struct TrackColor {
    DWORD primary;
    DWORD dark;
    DWORD darker;
};

struct RootConstants {
    float proj[4][4];
    float notes_y;
    float notes_cy;
    float white_cx;
    float timespan;
};
static_assert(sizeof(RootConstants) % 4 == 0);

constexpr unsigned MaxTrackColors = 1024;
struct FixedSizeConstants {
    float note_x[128];
    float bends[16];
};

class D3D12Renderer {
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
    void SetPipeline(Pipeline pipeline);
    RootConstants& GetRootConstants() { return m_RootConstants; };
    FixedSizeConstants& GetFixedSizeConstants() { return m_FixedConstants; };
    TrackColor* GetTrackColors() { return m_TrackColors; };
    inline void PushNoteData(NoteData data) { m_vNotesIntermediate.push_back(data); };
    size_t GetRenderedNotesCount() { return m_vNotesIntermediate.size(); };
    void SplitRect() { m_iRectSplit = (int)m_vRectsIntermediate.size(); }

    void ImguiStartFrame() {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

private:
    std::tuple<HRESULT, const char*> CreateWindowDependentObjects(HWND hWnd);

    static constexpr unsigned FrameCount = 2;
    static constexpr unsigned RectsPerPass = 10000; // Relatively low limit, but not many rects are supposed to be rendered anyway
    static constexpr unsigned NotesPerPass = 5000000;
    static constexpr unsigned IndexBufferCount = max(RectsPerPass, NotesPerPass) * 6;
    static constexpr unsigned GenericUploadSize = sizeof(FixedSizeConstants) + MaxTrackColors * 16 * sizeof(TrackColor);

    int m_iBufferWidth = 0;
    int m_iBufferHeight = 0;
    bool m_bLimitFPS = false;

    HWND m_hWnd = NULL;
    ComPtr<IDXGIFactory2> m_pFactory;
    ComPtr<IDXGIAdapter2> m_pAdapter;
    ComPtr<ID3D12Device> m_pDevice;
    ComPtr<ID3D12CommandQueue> m_pCommandQueue;
    ComPtr<IDXGISwapChain3> m_pSwapChain;
    ComPtr<ID3D12DescriptorHeap> m_pRTVDescriptorHeap;
    UINT m_uRTVDescriptorSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pDSVDescriptorHeap;
    UINT m_uDSVDescriptorSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pSRVDescriptorHeap;
    UINT m_uSRVDescriptorSize = 0;
    ComPtr<ID3D12DescriptorHeap> m_pImGuiSRVDescriptorHeap;
    ComPtr<ID3D12Resource> m_pRenderTargets[FrameCount];
    ComPtr<ID3D12Resource> m_pDepthBuffer;
    ComPtr<ID3D12CommandAllocator> m_pCommandAllocator[FrameCount];
    ComPtr<ID3D12RootSignature> m_pRectRootSignature;
    ComPtr<ID3D12PipelineState> m_pRectPipelineState;
    ComPtr<ID3D12RootSignature> m_pNoteRootSignature;
    ComPtr<ID3D12PipelineState> m_pNotePipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
    ComPtr<ID3D12Fence> m_pFence;
    HANDLE m_hFenceEvent = NULL;
    ComPtr<ID3D12Resource> m_pIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    ComPtr<ID3D12Resource> m_pVertexBuffers[FrameCount];
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferViews[FrameCount];
    ComPtr<ID3D12Resource> m_pNoteBuffers[FrameCount];
    ComPtr<ID3D12Resource> m_pGenericUpload;
    ComPtr<ID3D12Resource> m_pFixedBuffer;
    ComPtr<ID3D12Resource> m_pTrackColorBuffer;
    RootConstants m_RootConstants = {};
    FixedSizeConstants m_FixedConstants = {};
    FixedSizeConstants m_OldFixedConstants = {};
    TrackColor m_TrackColors[MaxTrackColors * 16] = {};
    TrackColor m_OldTrackColors[MaxTrackColors * 16] = {};

    UINT m_uFrameIndex = 0;
    UINT64 m_pFenceValues[FrameCount] = {};

    std::vector<RectVertex> m_vRectsIntermediate;
    std::vector<NoteData> m_vNotesIntermediate;
    int m_iRectSplit = -1;
};