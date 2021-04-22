// samplerenderer.h
// 
// Copyright(c) 2021 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include "stdafx.h"
#include "PostProc/MagnifierPS.h"

struct UIState;

// We are queuing (BackBufferCount + 0.5) frames, so we need to triple buffer the resources that get modified each frame
static const int BackBufferCount = 3;

using namespace CAULDRON_DX12;

//
// This class deals with the GPU side of the sample.
class Renderer
{
public:
    
    void OnCreate(Device* pDevice, SwapChain* pSwapChain, float FontSize);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain* pSwapChain, uint32_t Width, uint32_t Height);
    void OnDestroyWindowSizeDependentResources();

    void OnUpdateDisplayDependentResources(SwapChain* pSwapChain);

    const std::vector<TimeStamp>& GetTimingValues() const { return m_TimeStamps; }
    std::string& GetScreenshotFileName() { return m_pScreenShotName; }

    void OnRender(const UIState *pState, SwapChain *pSwapChain, float Time, bool bIsBenchmarking);

    void RenderParallelSortUI() { m_ParallelSort.DrawGui(); }

private:
    Device*                         m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;
    D3D12_VIEWPORT                  m_Viewport;
    D3D12_RECT                      m_RectScissor;
    
    // Initialize helper classes
    ResourceViewHeaps               m_ResourceViewHeaps;
    UploadHeap                      m_UploadHeap;
    DynamicBufferRing               m_ConstantBufferRing;
    StaticBufferPool                m_VidMemBufferPool;
    CommandListRing                 m_CommandListRing;
    GPUTimestamps                   m_GPUTimer;

    FFXParallelSort                 m_ParallelSort;

    // GUI
    ImGUI                           m_ImGUI;

    // For benchmarking
    std::vector<TimeStamp>          m_TimeStamps;

    // screen shot
    std::string                     m_pScreenShotName = "";
    SaveTexture                     m_SaveTexture;
};
