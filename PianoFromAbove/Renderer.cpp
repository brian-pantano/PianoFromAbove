/*************************************************************************************************
*
* File: Renderer.cpp
*
* Description: Implements the rendering objects. Just a wrapper to Direct3D.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
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

    if (m_hFenceEvent)
        CloseHandle(m_hFenceEvent);

#ifdef _DEBUG
    ComPtr<IDXGIDebug1> dxgi_debug = nullptr;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
        dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
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

        res = D3D12CreateDevice(m_pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_pDevice));
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
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
    };
    res = m_pFactory->CreateSwapChainForHwnd(m_pCommandQueue.Get(), hWnd, &swap_chain_desc, nullptr, nullptr, &temp_swapchain);
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
        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += m_uRTVDescriptorSize;
    }

    // Create command allocators
    for (int i = 0; i < FrameCount; i++) {
        res = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator[i]));
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommandAllocator");
    }

    // Create rectangle root signature
    ComPtr<ID3DBlob> rect_serialized;
    D3D12_ROOT_PARAMETER rect_root_sig_params[] = {
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = 16,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        }
    };
    D3D12_ROOT_SIGNATURE_DESC rect_root_sig_desc = {
        .NumParameters = _countof(rect_root_sig_params),
        .pParameters = rect_root_sig_params,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
    };
    res = D3D12SerializeRootSignature(&rect_root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &rect_serialized, nullptr);
    if (FAILED(res))
        return std::make_tuple(res, "D3D12SerializeRootSignature (rectangle)");
    res = m_pDevice->CreateRootSignature(0, rect_serialized->GetBufferPointer(), rect_serialized->GetBufferSize(), IID_PPV_ARGS(&m_pRectRootSignature));
    if (FAILED(res))
        return std::make_tuple(res, "CreateRootSignature (rectangle)");

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
        .pRootSignature = m_pRectRootSignature.Get(),
        .VS = {
            .pShaderBytecode = g_pRectVertexShader,
            .BytecodeLength = sizeof(g_pRectVertexShader),
        },
        .PS = {
            .pShaderBytecode = g_pRectPixelShader,
            .BytecodeLength = sizeof(g_pRectPixelShader),
        },
        .BlendState = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
            .RenderTarget = {
                {
                    // PFA is weird and inverts blending operations (0 is opaque, 255 is transparent)
                    .BlendEnable = TRUE,
                    .LogicOpEnable = FALSE,
                    .SrcBlend = D3D12_BLEND_INV_SRC_ALPHA,
                    .DestBlend = D3D12_BLEND_SRC_ALPHA,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
                    .DestBlendAlpha = D3D12_BLEND_SRC_ALPHA,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
                }
            }
        },
        .SampleMask = UINT_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = {
            .DepthEnable = FALSE,
            .StencilEnable = FALSE,
        },
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

    // Create dynamic rect vertex buffers
    // Each in-flight frame has its own vertex buffer
    auto vertex_buffer_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto vertex_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(RectsPerPass * 6 * sizeof(RectVertex));
    for (int i = 0; i < FrameCount; i++) {
        res = m_pDevice->CreateCommittedResource(
            &vertex_buffer_heap,
            D3D12_HEAP_FLAG_NONE,
            &vertex_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_pVertexBuffers[i])
        );
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommittedResource (vertex buffer)");
        m_VertexBufferViews[i].BufferLocation = m_pVertexBuffers[i]->GetGPUVirtualAddress();
        m_VertexBufferViews[i].SizeInBytes = vertex_buffer_desc.Width;
        m_VertexBufferViews[i].StrideInBytes = sizeof(RectVertex);
    }

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
        return std::make_tuple(res, "CreateCommittedResource (index buffer)");
    m_pIndexBuffer->SetName(L"Index buffer");
    m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes = index_buffer_desc.Width;

    // Create index upload buffer
    ComPtr<ID3D12Resource2> index_buffer_upload = nullptr;
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
        return std::make_tuple(res, "CreateCommittedResource (index upload buffer)");
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
    m_pCommandList->Reset(m_pCommandAllocator[m_uFrameIndex].Get(), nullptr);

    // Upload index buffer to GPU
    D3D12_SUBRESOURCE_DATA index_buffer_data = {
        .pData = index_buffer_vec.data(),
        .RowPitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint16_t)),
        .SlicePitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint16_t)),
    };
    UpdateSubresources<1>(m_pCommandList.Get(), m_pIndexBuffer.Get(), index_buffer_upload.Get(), 0, 0, 1, &index_buffer_data);

    // Finalize index buffer
    auto index_buffer_barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pIndexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    m_pCommandList->ResourceBarrier(1, &index_buffer_barrier);

    // Close the command list
    res = m_pCommandList->Close();
    if (FAILED(res))
        return std::make_tuple(res, "Closing command list for initial buffer upload");

    // Execute the command list
    ID3D12CommandList* command_lists[] = { m_pCommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(1, command_lists);

    // Wait for everything to finish
    if (FAILED(WaitForGPU()))
        return std::make_tuple(res, "WaitForGPU");

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

    // Reset the command list
    m_pCommandAllocator[m_uFrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocator[m_uFrameIndex].Get(), m_pRectPipelineState.Get());

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
    // https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_dx12.cpp#L99
    float L = 0;
    float R = m_iBufferWidth;
    float T = 0;
    float B = m_iBufferHeight;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };
    m_pCommandList->SetGraphicsRootSignature(m_pRectRootSignature.Get());
    m_pCommandList->SetGraphicsRoot32BitConstants(0, 16, &mvp, 0);
    m_pCommandList->RSSetViewports(1, &viewport);
    m_pCommandList->RSSetScissorRects(1, &scissor);
    m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferViews[m_uFrameIndex]);
    m_pCommandList->IASetIndexBuffer(&m_IndexBufferView);

    // Transition backbuffer state to render target
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_uFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
    // Flush the intermediate rect buffer
    // TODO: Handle more than RectsPerPass
    auto count = min(m_vRectsIntermediate.size(), RectsPerPass * 4);
    D3D12_RANGE range = {
        .Begin = 0,
        .End = count * sizeof(RectVertex),
    };
    RectVertex* vertices = nullptr;
    HRESULT res = m_pVertexBuffers[m_uFrameIndex]->Map(0, &range, (void**)&vertices);
    if (FAILED(res))
        return res;
    memcpy(vertices, m_vRectsIntermediate.data(), count * sizeof(RectVertex));
    m_pVertexBuffers[m_uFrameIndex]->Unmap(0, &range);

    // Draw the rects
    m_pCommandList->DrawIndexedInstanced(count / 4 * 6, 1, 0, 0, 0);

    // Transition backbuffer state to present
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_uFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_pCommandList->ResourceBarrier(1, &barrier);

    // Close the command list
    res = m_pCommandList->Close();
    if (FAILED(res))
        return res;

    // Execute the command list
    ID3D12CommandList* command_lists[] = { m_pCommandList.Get() };
    m_pCommandQueue->ExecuteCommandLists(1, command_lists);

    return S_OK;
}

HRESULT D3D12Renderer::Present() {
    // Present the frame
    HRESULT res;
    if (m_bLimitFPS)
        res = m_pSwapChain->Present(1, 0);
    else
        res = m_pSwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    if (FAILED(res))
        return res;

    // Signal the fence
    const UINT64 cur_fence_value = m_pFenceValues[m_uFrameIndex];
    res = m_pCommandQueue->Signal(m_pFence.Get(), cur_fence_value);
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
        {x + cx, y + cy, c3},
        {x,      y + cy, c4},
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
    HRESULT res = m_pCommandQueue->Signal(m_pFence.Get(), m_pFenceValues[m_uFrameIndex]);
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