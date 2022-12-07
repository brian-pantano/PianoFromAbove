/*************************************************************************************************
*
* File: Renderer.cpp
*
* Description: Implements the rendering objects. Just a wrapper to Direct3D.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "d3dx12/d3dx12.h"
#ifdef _DEBUG
#include <dxgidebug.h>
#endif
#include "RectPixelShader.h"
#include "RectVertexShader.h"
#include "Renderer.h"

#define SAFE_RELEASE(x) if (x) x->Release();

D3D12Renderer::D3D12Renderer() {}

D3D12Renderer::~D3D12Renderer() {
    // HACK: This hangs for some reason. Maybe it's because the hWnd gets destroyed before here?
    //WaitForGPU();

    for (int i = 0; i < FrameCount; i++) {
        SAFE_RELEASE(m_pRenderTargets[i]);
        SAFE_RELEASE(m_pCommandAllocator[i]);
    }
    SAFE_RELEASE(m_pFactory);
    SAFE_RELEASE(m_pAdapter);
    SAFE_RELEASE(m_pDevice);
    SAFE_RELEASE(m_pCommandQueue);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pRTVDescriptorHeap);
    SAFE_RELEASE(m_pRectRootSignature);
    SAFE_RELEASE(m_pRectPipelineState);
    SAFE_RELEASE(m_pCommandList);
    SAFE_RELEASE(m_pFence);
    if (m_hFenceEvent)
        CloseHandle(m_hFenceEvent);
    SAFE_RELEASE(m_pIndexBuffer);

#ifdef _DEBUG
    IDXGIDebug1* dxgi_debug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug)))) {
        dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        dxgi_debug->Release();
    }
#endif
}

std::tuple<HRESULT, const char*> D3D12Renderer::Init(HWND hWnd, bool bLimitFPS) {
    HRESULT res;
#ifdef _DEBUG
    // Initialize D3D12 debug interface
    ID3D12Debug* d3d12_debug = nullptr;
    res = D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12_debug));
    if (FAILED(res))
        return std::make_tuple(res, "D3D12GetDebugInterface");
    d3d12_debug->EnableDebugLayer();
    d3d12_debug->Release();
#endif

    // Create DXGI factory
#ifdef _DEBUG
    res = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pFactory));
#else
    res = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_pFactory));
#endif
    if (FAILED(res))
        return std::make_tuple(res, "CreateDXGIFactory1");

    // Create device
    // TODO: Allow device selection for people with multiple GPUs
    IDXGIAdapter1* adapter1 = nullptr;
    for (UINT i = 0; m_pFactory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; i++) {
        res = adapter1->QueryInterface(IID_PPV_ARGS(&m_pAdapter));
        if (FAILED(res))
            continue;

        DXGI_ADAPTER_DESC2 desc = {};
        res = m_pAdapter->GetDesc2(&desc);
        if (FAILED(res))
            return std::make_tuple(res, "GetDesc2");

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        res = D3D12CreateDevice(m_pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pDevice));
        if (FAILED(res))
            return std::make_tuple(res, "D3D12CreateDevice");
        break;
    }
    if (m_pDevice == nullptr) {
        MessageBoxW(NULL, L"Couldn't find a suitable D3D12 device.", L"DirectX Error", MB_ICONERROR);
        exit(1);
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_pCommandQueue));
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandQueue");

    // Create swap chain
    IDXGISwapChain1* temp_swapchain = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {
        .Width = 0,
        .Height = 0,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {
            .Count = 1,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = FrameCount,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = 0,
    };
    res = m_pFactory->CreateSwapChainForHwnd(m_pCommandQueue, hWnd, &swap_chain_desc, nullptr, nullptr, &temp_swapchain);
    if (FAILED(res))
        return std::make_tuple(res, "CreateSwapChainForHwnd");
    res = temp_swapchain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
    if (FAILED(res))
        return std::make_tuple(res, "IDXGISwapChain1 -> IDXGISwapChain3");

    // Read backbuffer width and height
    // TODO: Handle resizing
    DXGI_SWAP_CHAIN_DESC1 actual_swap_desc = {};
    res = m_pSwapChain->GetDesc1(&actual_swap_desc);
    if (FAILED(res))
        return std::make_tuple(res, "GetDesc1");
    m_iBufferWidth = actual_swap_desc.Width;
    m_iBufferHeight = actual_swap_desc.Height;

    // Disable ALT+ENTER
    // TODO: Make fullscreen work
    m_pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    // Create render target view descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = FrameCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_pRTVDescriptorHeap));
    if (FAILED(res))
        return std::make_tuple(res, "CreateDescriptorHeap");
    m_uRTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FrameCount; i++) {
        res = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
        if (FAILED(res))
            return std::make_tuple(res, "GetBuffer");
        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i], nullptr, rtv_handle);
        rtv_handle.ptr += m_uRTVDescriptorSize;
    }

    // Create command allocators
    for (int i = 0; i < FrameCount; i++) {
        res = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator[i]));
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommandAllocator");
    }

    // Create rectangle root signature
    ID3DBlob* rect_serialized = nullptr;
    D3D12_ROOT_SIGNATURE_DESC rect_root_sig_desc = {
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
    };
    res = D3D12SerializeRootSignature(&rect_root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rect_serialized, nullptr);
    if (FAILED(res))
        return std::make_tuple(res, "D3D12SerializeRootSignature (rectangle)");
    res = m_pDevice->CreateRootSignature(0, rect_serialized->GetBufferPointer(), rect_serialized->GetBufferSize(), IID_PPV_ARGS(&m_pRectRootSignature));
    if (FAILED(res))
        return std::make_tuple(res, "CreateRootSignature (rectangle)");
    rect_serialized->Release();

    // Create pipeline state
    D3D12_INPUT_ELEMENT_DESC vertex_input[] = {
        {
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = 0,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        {
            .SemanticName = "COLOR",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .InputSlot = 0,
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {
        .pRootSignature = m_pRectRootSignature,
        .VS = {
            .pShaderBytecode = g_pRectVertexShader,
            .BytecodeLength = sizeof(g_pRectVertexShader),
        },
        .PS = {
            .pShaderBytecode = g_pRectPixelShader,
            .BytecodeLength = sizeof(g_pRectPixelShader),
        },
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
        .InputLayout = {
            .pInputElementDescs = vertex_input,
            .NumElements = _countof(vertex_input),
        },
        .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = {
            DXGI_FORMAT_R8G8B8A8_UNORM,
        },
        .DSVFormat = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = {
            .Count = 1
        },
    };
    res = m_pDevice->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&m_pRectPipelineState));
    if (FAILED(res))
        return std::make_tuple(res, "CreateGraphicsPipelineState");

    // Create command list
    res = m_pDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_pCommandList));
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandList1");

    // Create synchronization fence
    res = m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence));
    if (FAILED(res))
        return std::make_tuple(res, "CreateFence");
    m_pFenceValues[m_uFrameIndex]++;

    // Create synchronization fence event
    m_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Create index buffer
    auto index_buffer_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(RectsPerPass * 6 * sizeof(uint16_t));
    res = m_pDevice->CreateCommittedResource(
        &index_buffer_heap,
        D3D12_HEAP_FLAG_NONE,
        &index_buffer_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_pIndexBuffer)
    );
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommittedResource2 (index buffer)");
    m_pIndexBuffer->SetName(L"Index buffer");

    // Create index upload buffer
    ID3D12Resource2* index_buffer_upload = nullptr;
    auto index_buffer_upload_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    res = m_pDevice->CreateCommittedResource(
        &index_buffer_upload_heap,
        D3D12_HEAP_FLAG_NONE,
        &index_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&index_buffer_upload)
    );
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommittedResource2 (index upload buffer)");
    index_buffer_upload->SetName(L"Index upload buffer");

    // Generate index buffer data
    std::vector<uint16_t> index_buffer_vec;
    index_buffer_vec.resize(RectsPerPass * 6);
    for (int i = 0; i < RectsPerPass; i++) {
        index_buffer_vec[i * 6] = i * 4;
        index_buffer_vec[i * 6 + 1] = i * 4 + 1;
        index_buffer_vec[i * 6 + 2] = i * 4 + 2;
        index_buffer_vec[i * 6 + 3] = i * 4;
        index_buffer_vec[i * 6 + 4] = i * 4 + 2;
        index_buffer_vec[i * 6 + 5] = i * 4 + 3;
    }

    // Reset the command list
    m_pCommandAllocator[m_uFrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocator[m_uFrameIndex], nullptr);

    // Upload index buffer to GPU
    D3D12_SUBRESOURCE_DATA index_buffer_data = {
        .pData = index_buffer_vec.data(),
        .RowPitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint16_t)),
        .SlicePitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint16_t)),
    };
    UpdateSubresources<1>(m_pCommandList, m_pIndexBuffer, index_buffer_upload, 0, 0, 1, &index_buffer_data);

    // Finalize index buffer
    auto index_buffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pIndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    m_pCommandList->ResourceBarrier(1, &index_buffer_barrier);

    // Close the command list
    res = m_pCommandList->Close();
    if (FAILED(res))
        return std::make_tuple(res, "Closing command list for initial buffer upload");

    // Execute the command list
    m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pCommandList);

    // Wait for everything to finish
    if (FAILED(WaitForGPU()))
        return std::make_tuple(res, "WaitForGPU");

    index_buffer_upload->Release();
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

HRESULT D3D12Renderer::ClearAndBeginScene(DWORD color) {
    // Clear the intermediate buffers
    m_vRectsIntermediate.clear();

    // Get the resources for the current frame
    auto cmd_allocator = m_pCommandAllocator[m_uFrameIndex];
    auto render_target = m_pRenderTargets[m_uFrameIndex];

    // Reset the command list
    cmd_allocator->Reset();
    m_pCommandList->Reset(cmd_allocator, m_pRectPipelineState);

    // Set up render state
    D3D12_VIEWPORT viewport = {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (float)m_iBufferWidth,
        .Height = (float)m_iBufferHeight,
        .MinDepth = 0.0,
        .MaxDepth = 1.0,
    };
    D3D12_RECT scissor = { 0, 0, m_iBufferWidth, m_iBufferHeight };
    m_pCommandList->SetGraphicsRootSignature(m_pRectRootSignature);
    m_pCommandList->RSSetViewports(1, &viewport);
    m_pCommandList->RSSetScissorRects(1, &scissor);
    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Transition backbuffer state to render target
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_target, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pCommandList->ResourceBarrier(1, &barrier);

    // Bind to output merger
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_uFrameIndex, m_uRTVDescriptorSize);
    m_pCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    // Send a clear render target command
    float float_color[4] = { (float)((color >> 16) & 0xFF) / 255.0f, (float)((color >> 8) & 0xFF) / 255.0f, (float)(color & 0xFF) / 255.0f, 1.0f };
    /*
    // Seizure-inducing debug stuff
    switch (m_uFrameIndex) {
    case 0:
        float_color[0] = 1.0;
        float_color[1] = 0.0;
        float_color[2] = 0.0;
        break;
    case 1:
        float_color[0] = 0.0;
        float_color[1] = 1.0;
        float_color[2] = 0.0;
        break;
    case 2:
        float_color[0] = 0.0;
        float_color[1] = 0.0;
        float_color[2] = 1.0;
        break;
    }
    */
    m_pCommandList->ClearRenderTargetView(rtv, float_color, 0, nullptr);

    return S_OK;
}

HRESULT D3D12Renderer::EndScene() {
    // Get the resources for the current frame
    auto render_target = m_pRenderTargets[m_uFrameIndex];

    // Transition backbuffer state to present
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_pCommandList->ResourceBarrier(1, &barrier);

    // Close the command list
    HRESULT res = m_pCommandList->Close();
    if (FAILED(res))
        return res;

    // Execute the command list
    m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_pCommandList);

    return S_OK;
}

HRESULT D3D12Renderer::Present() {
    // Present the frame
    DXGI_PRESENT_PARAMETERS present_params = {};
    HRESULT res = m_pSwapChain->Present1(0, 0, &present_params);
    if (FAILED(res))
        return res;

    // Signal the fence
    const UINT64 cur_fence_value = m_pFenceValues[m_uFrameIndex];
    res = m_pCommandQueue->Signal(m_pFence, cur_fence_value);
    if (FAILED(res))
        return res;

    // Update frame index
    m_uFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // Wait for the next frame to be ready
    if (m_pFence->GetCompletedValue() < m_pFenceValues[m_uFrameIndex]) {
        res = m_pFence->SetEventOnCompletion(m_pFenceValues[m_uFrameIndex], m_hFenceEvent);
        if (FAILED(res))
            return res;
        WaitForSingleObjectEx(m_hFenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame
    m_pFenceValues[m_uFrameIndex] = cur_fence_value + 1;
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
    return DrawRect(x, y, cx, cy, color, color, color, color);
}

HRESULT D3D12Renderer::DrawRect(float x, float y, float cx, float cy, DWORD c1, DWORD c2, DWORD c3, DWORD c4) {
    m_vRectsIntermediate.insert(m_vRectsIntermediate.end(), {
        {x,      y,      c1},
        {x + cx, y,      c2},
        {x + cx, y + cx, c3},
        {x,      y + cx, c4},
    });
    return S_OK;
}

HRESULT D3D12Renderer::DrawSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD color) {
    return DrawSkew(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);
}

HRESULT D3D12Renderer::DrawSkew(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, DWORD c1, DWORD c2, DWORD c3, DWORD c4) {
    m_vRectsIntermediate.insert(m_vRectsIntermediate.end(), {
        {x1, y1, c1},
        {x2, y2, c2},
        {x3, y3, c3},
        {x4, y4, c4},
    });
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

std::wstring D3D12Renderer::GetAdapterName() {
    if (m_pAdapter) {
        DXGI_ADAPTER_DESC2 desc = {};
        if (FAILED(m_pAdapter->GetDesc2(&desc)))
            return L"GetDesc2 failed";
        return desc.Description;
    }
    return L"None";
}

HRESULT D3D12Renderer::WaitForGPU() {
    // Signal the command queue
    HRESULT res = m_pCommandQueue->Signal(m_pFence, m_pFenceValues[m_uFrameIndex]);
    if (FAILED(res))
        return res;

    auto val = m_pFence->GetCompletedValue();
    if (val < m_pFenceValues[m_uFrameIndex]) {
        // Wait for the fence
        res = m_pFence->SetEventOnCompletion(m_pFenceValues[m_uFrameIndex], m_hFenceEvent);
        if (FAILED(res))
            return res;
        WaitForSingleObjectEx(m_hFenceEvent, INFINITE, FALSE);
    }

    // Increment the fence value for the current frame
    m_pFenceValues[m_uFrameIndex]++;

    return S_OK;
}