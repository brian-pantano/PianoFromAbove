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

D3D12Renderer::D3D12Renderer() {}

D3D12Renderer::~D3D12Renderer() {
    for (int i = 0; i < s_iFrameCount; i++) {
        if (m_pRenderTargets[i])
            m_pRenderTargets[i]->Release();
    }
    if (m_pDebug)
        m_pDebug->Release();
    if (m_pFactory)
        m_pFactory->Release();
    if (m_pAdapter)
        m_pAdapter->Release();
    if (m_pDevice)
        m_pDevice->Release();
    if (m_pCommandQueue)
        m_pCommandQueue->Release();
    if (m_pSwapChain)
        m_pSwapChain->Release();
    if (m_pRTVDescriptorHeap)
        m_pRTVDescriptorHeap->Release();
    if (m_pCommandAllocator)
        m_pCommandAllocator->Release();
}

std::tuple<HRESULT, const char*> D3D12Renderer::Init(HWND hWnd, bool bLimitFPS) {
    HRESULT res;
#ifdef _DEBUG
    // Initialize D3D12 debug interface
    res = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&m_pDebug);
    if (FAILED(res))
        return std::make_tuple(res, "D3D12GetDebugInterface");
    m_pDebug->EnableDebugLayer();
#endif

    // Create DXGI factory
    res = CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&m_pFactory);
    if (FAILED(res))
        return std::make_tuple(res, "CreateDXGIFactory1");

    // Create D3D12 device
    // TODO: Allow device selection for people with multiple GPUs
    for (UINT i = 0; m_pFactory->EnumAdapters1(i, &m_pAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc = {};
        res = m_pAdapter->GetDesc1(&desc);
        if (FAILED(res))
            return std::make_tuple(res, "GetDesc1");

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        res = D3D12CreateDevice(m_pAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&m_pDevice);
        if (FAILED(res))
            return std::make_tuple(res, "D3D12CreateDevice");
    }
    if (m_pDevice == nullptr) {
        MessageBoxW(NULL, L"Couldn't find a suitable D3D12 device.", L"DirectX Error", MB_ICONERROR);
        exit(1);
    }

    // Create D3D12 command queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateCommandQueue(&queue_desc, __uuidof(ID3D12CommandQueue), (void**)&m_pCommandQueue);
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandQueue");

    // Create DXGI swap chain
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {
        .Width = 0,
        .Height = 0,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {
            .Count = 1,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = s_iFrameCount,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = 0,
    };
    res = m_pFactory->CreateSwapChainForHwnd(m_pCommandQueue, hWnd, &swap_chain_desc, nullptr, nullptr, &m_pSwapChain);
    if (FAILED(res))
        return std::make_tuple(res, "CreateSwapChainForHwnd");

    // Disable ALT+ENTER
    // TODO: Make fullscreen work
    m_pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    // Create D3D12 render target view descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = s_iFrameCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateDescriptorHeap(&heap_desc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pRTVDescriptorHeap);
    if (FAILED(res))
        return std::make_tuple(res, "CreateDescriptorHeap");
    m_uRTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < s_iFrameCount; i++) {
        res = m_pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_pRenderTargets[i]);
        if (FAILED(res))
            return std::make_tuple(res, "GetBuffer");
        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtv_handle);
        rtv_handle.ptr += m_uRTVDescriptorSize;
    }

    // Create command allocator
    res = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_pCommandAllocator);
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandAllocator");;
    return std::make_tuple(S_OK, "");
}

HRESULT D3D12Renderer::ResetDeviceIfNeeded() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::ResetDevice() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::Clear(DWORD color) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::BeginScene() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::EndScene() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::Present() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::BeginText() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawTextW(const WCHAR* sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawTextA(const CHAR* sText, FontSize fsFont, LPRECT rcPos, DWORD dwFormat, DWORD dwColor, INT iChars) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::EndText() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawRect(float x, float y, float cx, float cy, DWORD color) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawRect(float x, float y, float cx, float cy, DWORD c1, DWORD c2, DWORD c3, DWORD c4) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD color) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::DrawSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD c1, DWORD c2, DWORD c3, DWORD c4) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::RenderBatch(bool bWithDepth) {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::SetLimitFPS(bool bLimitFPS) {
    // TODO
    m_bLimitFPS = bLimitFPS;
    return S_OK;
}