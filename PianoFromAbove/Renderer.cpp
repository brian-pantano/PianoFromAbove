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
#include "NoteVertexShader.h"
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
    m_hWnd = hWnd;
    m_bLimitFPS = bLimitFPS;
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

#ifdef _DEBUG
    // Break on errors
    ComPtr<ID3D12InfoQueue> info_queue;
    if (SUCCEEDED(m_pDevice->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    }
#endif

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

    // Create render target view descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = FrameCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_pRTVDescriptorHeap));
    if (FAILED(res))
        return std::make_tuple(res, "CreateDescriptorHeap (RTV)");
    m_uRTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create depth stencil view descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_pDSVDescriptorHeap));
    if (FAILED(res))
        return std::make_tuple(res, "CreateDescriptorHeap (DSV)");
    m_uDSVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // Create shader resource view heap
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = FrameCount,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    res = m_pDevice->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&m_pSRVDescriptorHeap));
    if (FAILED(res))
        return std::make_tuple(res, "CreateDescriptorHeap (SRV)");
    m_uSRVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Allocate note shader resource views
    D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = m_pSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FrameCount; i++) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = NotesPerPass,
                .StructureByteStride = sizeof(NoteData),
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            }
        };

        m_pDevice->CreateShaderResourceView(m_pNoteBuffers[i].Get(), &srv_desc, srv_handle);
        srv_handle.ptr += m_uSRVDescriptorSize;
    }

    // Create command allocators
    for (int i = 0; i < FrameCount; i++) {
        res = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator[i]));
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommandAllocator (direct)");
    }
    res = m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_pBundleAllocator));
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandAllocator (bundle)");

    // Create rectangle root signature
    ComPtr<ID3DBlob> rect_serialized;
    D3D12_ROOT_PARAMETER rect_root_sig_params[] = {
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = sizeof(RootConstants) / 4,
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

    // Create rect pipeline
    D3D12_INPUT_ELEMENT_DESC rect_vertex_input[] = {
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
    D3D12_GRAPHICS_PIPELINE_STATE_DESC rect_pipeline_desc = {
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
            .pInputElementDescs = rect_vertex_input,
            .NumElements = _countof(rect_vertex_input),
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
    res = m_pDevice->CreateGraphicsPipelineState(&rect_pipeline_desc, IID_PPV_ARGS(&m_pRectPipelineState));
    if (FAILED(res))
        return std::make_tuple(res, "CreateGraphicsPipelineState (rect)");
    
    // Create note root signature
    ComPtr<ID3DBlob> note_serialized;
    /*
    D3D12_ROOT_PARAMETER note_root_sig_params[] = {
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = sizeof(RootConstants) / 4,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        },
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor {
                .ShaderRegister = 1,
                .RegisterSpace = 0,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        }
    };
    */
    CD3DX12_ROOT_PARAMETER1 note_root_sig_params[2];
    note_root_sig_params[0].InitAsConstants(sizeof(RootConstants) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    note_root_sig_params[1].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC note_root_sig_desc = {
        .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
        .Desc_1_1 = {
            .NumParameters = _countof(note_root_sig_params),
            .pParameters = note_root_sig_params,
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS,
        }
    };
    res = D3D12SerializeVersionedRootSignature(&note_root_sig_desc, &note_serialized, nullptr);
    if (FAILED(res))
        return std::make_tuple(res, "D3D12SerializeRootSignature (note)");
    res = m_pDevice->CreateRootSignature(0, note_serialized->GetBufferPointer(), note_serialized->GetBufferSize(), IID_PPV_ARGS(&m_pNoteRootSignature));
    if (FAILED(res))
        return std::make_tuple(res, "CreateRootSignature (note)");

    // Create note pipeline
    auto note_pipeline_desc = rect_pipeline_desc;
    note_pipeline_desc.pRootSignature = m_pNoteRootSignature.Get();
    note_pipeline_desc.VS = {
        .pShaderBytecode = g_pNoteVertexShader,
        .BytecodeLength = sizeof(g_pNoteVertexShader),
    };
    note_pipeline_desc.DepthStencilState = {
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
    };
    note_pipeline_desc.InputLayout = {
        .NumElements = 0,
    };
    res = m_pDevice->CreateGraphicsPipelineState(&note_pipeline_desc, IID_PPV_ARGS(&m_pNotePipelineState));
    if (FAILED(res))
        return std::make_tuple(res, "CreateGraphicsPipelineState (note)");

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
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            nullptr,
            IID_PPV_ARGS(&m_pVertexBuffers[i])
        );
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommittedResource (vertex buffer)");
        m_pVertexBuffers[i]->SetName(L"Vertex buffer");
        m_VertexBufferViews[i].BufferLocation = m_pVertexBuffers[i]->GetGPUVirtualAddress();
        m_VertexBufferViews[i].SizeInBytes = vertex_buffer_desc.Width;
        m_VertexBufferViews[i].StrideInBytes = sizeof(RectVertex);
    }

    // Create dynamic note buffers
    auto note_buffer_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto note_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(NotesPerPass * sizeof(NoteData));
    for (int i = 0; i < FrameCount; i++) {
        res = m_pDevice->CreateCommittedResource(
            &note_buffer_heap,
            D3D12_HEAP_FLAG_NONE,
            &note_buffer_desc,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(&m_pNoteBuffers[i])
        );
        if (FAILED(res))
            return std::make_tuple(res, "CreateCommittedResource (note buffer)");
        m_pNoteBuffers[i]->SetName(L"Note buffer");
    }

    // Create index buffer
    auto index_buffer_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferCount * sizeof(uint32_t));
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
    m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
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
    std::vector<uint32_t> index_buffer_vec;
    index_buffer_vec.resize(IndexBufferCount);
    for (int i = 0; i < IndexBufferCount / 6; i++) {
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
        .RowPitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint32_t)),
        .SlicePitch = (LONG_PTR)(index_buffer_vec.size() * sizeof(uint32_t)),
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

    // Make the swap chain
    auto res2 = CreateWindowDependentObjects(hWnd);
    if (FAILED(std::get<0>(res2)))
        return res2;

    // Create rect render state initialization bundle
    res = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_pBundleAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pRectBundle));
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandList (rect bundle)");

    m_pRectBundle->SetPipelineState(m_pRectPipelineState.Get());
    m_pRectBundle->SetGraphicsRootSignature(m_pRectRootSignature.Get());
    m_pRectBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pRectBundle->IASetIndexBuffer(&m_IndexBufferView);
    m_pRectBundle->Close();

    // Create note render state initialization bundle
    res = m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_pBundleAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pNoteBundle));
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommandList (note bundle)");

    m_pNoteBundle->SetPipelineState(m_pNotePipelineState.Get());
    m_pNoteBundle->SetGraphicsRootSignature(m_pNoteRootSignature.Get());
    m_pNoteBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pNoteBundle->IASetVertexBuffers(0, 0, nullptr);
    m_pNoteBundle->IASetIndexBuffer(&m_IndexBufferView);
    m_pNoteBundle->Close();

    return std::make_tuple(S_OK, "");
}

std::tuple<HRESULT, const char*> D3D12Renderer::CreateWindowDependentObjects(HWND hWnd) {
    HRESULT res;
    if (m_pSwapChain) {
        // Wait for the GPU to finish any remaining work
        WaitForGPU();

        // Release the current render target views
        for (int i = 0; i < FrameCount; i++) {
            m_pRenderTargets[i].Reset();
            m_pFenceValues[i] = m_pFenceValues[m_uFrameIndex];
        }

        // Resize the swap chain
        res = m_pSwapChain->ResizeBuffers(FrameCount, 0, 0, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        if (FAILED(res))
            return std::make_tuple(res, "ResizeBuffers");
    } else {
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
            .Scaling = DXGI_SCALING_NONE,
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
    }

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

    // Create render target views
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < FrameCount; i++) {
        res = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]));
        if (FAILED(res))
            return std::make_tuple(res, "GetBuffer");
        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtv_handle);
        m_pRenderTargets[i]->SetName(L"Render target");
        rtv_handle.ptr += m_uRTVDescriptorSize;
    }

    // Create depth buffer view
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto dsv_heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto dsv_res_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_iBufferWidth, m_iBufferHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
        .Format = DXGI_FORMAT_D32_FLOAT,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE,
    };
    D3D12_CLEAR_VALUE clear_value = {
        .Format = DXGI_FORMAT_D32_FLOAT,
        .DepthStencil = {
            .Depth = 1.0f,
            .Stencil = 0,
        }
    };
    res = m_pDevice->CreateCommittedResource(
        &dsv_heap,
        D3D12_HEAP_FLAG_NONE,
        &dsv_res_desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear_value,
        IID_PPV_ARGS(&m_pDepthBuffer)
    );
    if (FAILED(res))
        return std::make_tuple(res, "CreateCommittedResource (depth buffer)");
    m_pDevice->CreateDepthStencilView(m_pDepthBuffer.Get(), &dsv_desc, dsv_handle);

    // Reset the current frame index
    m_uFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    // Set up root constants
    // https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_dx12.cpp#L99
    float L = 0;
    float R = m_iBufferWidth;
    float T = 0;
    float B = m_iBufferHeight;
    float mvp[4][4] = {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };
    memcpy(m_RootConstants.mvp, mvp, sizeof(mvp));

    return std::make_tuple(S_OK, "");
}

HRESULT D3D12Renderer::ResetDeviceIfNeeded() {
    // TODO
    return S_OK;
}

HRESULT D3D12Renderer::ResetDevice() {
    auto res = CreateWindowDependentObjects(m_hWnd);
    if (FAILED(std::get<0>(res)))
        return std::get<0>(res);
    return S_OK;
}

HRESULT D3D12Renderer::ClearAndBeginScene(DWORD color) {
    // Clear the intermediate buffers
    m_vRectsIntermediate.clear();
    m_vNotesIntermediate.clear();
    m_iRectSplit = -1;

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
    SetPipeline(Pipeline::Rect);
    m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferViews[m_uFrameIndex]);
    m_pCommandList->RSSetViewports(1, &viewport);
    m_pCommandList->RSSetScissorRects(1, &scissor);
    m_pCommandList->SetGraphicsRoot32BitConstants(0, sizeof(m_RootConstants) / 4, &m_RootConstants, 0);

    // Transition backbuffer state to render target
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pRenderTargets[m_uFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pCommandList->ResourceBarrier(1, &barrier);

    // Bind to output merger
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_uFrameIndex, m_uRTVDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsv(m_pDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    m_pCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

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
    m_pCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_pCommandList->ClearRenderTargetView(rtv, float_color, 0, nullptr);

    return S_OK;
}

HRESULT D3D12Renderer::EndScene() {
    // Flush the intermediate rect buffer
    // TODO: Handle more than RectsPerPass
    HRESULT res = S_OK;
    auto rect_count = min(m_vRectsIntermediate.size(), RectsPerPass * 4);
    auto rect_split = min(m_iRectSplit < 0 ? rect_count : m_iRectSplit, RectsPerPass * 4);
    if (!m_vRectsIntermediate.empty()) {
        D3D12_RANGE rect_range = {
            .Begin = 0,
            .End = rect_count * sizeof(RectVertex),
        };
        RectVertex* vertices = nullptr;
        res = m_pVertexBuffers[m_uFrameIndex]->Map(0, &rect_range, (void**)&vertices);
        if (FAILED(res))
            return res;
        memcpy(vertices, m_vRectsIntermediate.data(), rect_count * sizeof(RectVertex));
        m_pVertexBuffers[m_uFrameIndex]->Unmap(0, &rect_range);

        // Draw the first rect batch
        m_pCommandList->DrawIndexedInstanced(rect_split / 4 * 6, 1, 0, 0, 0);
    }

    // Flush the intermediate note buffer
    // TODO: Handle more than NotesPerPass
    if (!m_vNotesIntermediate.empty()) {
        SetPipeline(Pipeline::Note);
        m_pCommandList->SetGraphicsRootShaderResourceView(1, m_pNoteBuffers[m_uFrameIndex]->GetGPUVirtualAddress());
        auto note_count = min(m_vNotesIntermediate.size(), NotesPerPass);
        D3D12_RANGE note_range = {
            .Begin = 0,
            .End = note_count * sizeof(NoteData),
        };
        NoteData* notes = nullptr;
        res = m_pNoteBuffers[m_uFrameIndex]->Map(0, &note_range, (void**)&notes);
        if (FAILED(res))
            return res;
        memcpy(notes, m_vNotesIntermediate.data(), note_count * sizeof(NoteData));
        m_pNoteBuffers[m_uFrameIndex]->Unmap(0, &note_range);

        // Draw the notes
        m_pCommandList->DrawIndexedInstanced(note_count * 6 * 2, 1, 0, 0, 0);
    }

    // Draw the second rect batch
    if (rect_count > rect_split) {
        SetPipeline(Pipeline::Rect);
        m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferViews[m_uFrameIndex]);
        m_pCommandList->DrawIndexedInstanced((rect_count - rect_split) / 4 * 6, 1, rect_split / 4 * 6, 0, 0);
    }

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

void D3D12Renderer::SetPipeline(Pipeline pipeline) {
    switch (pipeline) {
    case Pipeline::Rect:
        m_pCommandList->ExecuteBundle(m_pRectBundle.Get());
        m_pCommandList->SetGraphicsRootSignature(m_pRectRootSignature.Get()); // RenderDoc crash workaround
        break;
    case Pipeline::Note:
        m_pCommandList->ExecuteBundle(m_pNoteBundle.Get());
        m_pCommandList->SetGraphicsRootSignature(m_pNoteRootSignature.Get()); // RenderDoc crash workaround
        break;
    }
}