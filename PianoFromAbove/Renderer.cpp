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

D3D12Renderer::D3D12Renderer() {
    // TODO
    m_iBufferWidth = 0;
    m_iBufferHeight = 0;
    m_bLimitFPS = false;
}

D3D12Renderer::~D3D12Renderer() {
    // TODO
}

HRESULT D3D12Renderer::Init(HWND hWnd, bool bLimitFPS) {
    // TODO
    return S_OK;
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