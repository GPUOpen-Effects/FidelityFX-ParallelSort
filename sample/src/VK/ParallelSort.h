// ParallelSort.h
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
#include "vulkan/vulkan.h"

using namespace CAULDRON_VK;

struct ParallelSortRenderCB // If you change this, also change struct ParallelSortRenderCB in ParallelSortVerify.hlsl
{
    int32_t Width;
    int32_t Height;
    int32_t SortWidth;
    int32_t SortHeight;
};

namespace CAULDRON_VK
{
    class Device;
    class ResourceViewHeaps;
    class DynamicBufferRing;
    class StaticBufferPool;
}

class FFXParallelSort
{
public:
    void OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, SwapChain* pSwapChain);
    void OnDestroy();

    void Sort(VkCommandBuffer commandList, bool isBenchmarking, float benchmarkTime);
    void CopySourceDataForFrame(VkCommandBuffer commandList);
    void DrawGui();
    void DrawVisualization(VkCommandBuffer commandList, uint32_t RTWidth, uint32_t RTHeight);

    // Temp -- For command line overrides
    static void OverrideKeySet(int ResolutionOverride);
    static void OverridePayload();
    // Temp -- For command line overrides

private:
    void CreateKeyPayloadBuffers();
    void CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, VkPipeline& pPipeline);
    void BindConstantBuffer(VkDescriptorBufferInfo& GPUCB, VkDescriptorSet& DescriptorSet, uint32_t Binding = 0, uint32_t Count = 1);
    void BindUAVBuffer(VkBuffer* pBuffer, VkDescriptorSet& DescriptorSet, uint32_t Binding = 0, uint32_t Count = 1);

    // Temp -- For command line overrides
    static int KeySetOverride;
    static bool PayloadOverride;
    // Temp -- For command line overrides

    Device*                 m_pDevice = nullptr;
    UploadHeap*             m_pUploadHeap = nullptr;
    ResourceViewHeaps*      m_pResourceViewHeaps = nullptr;
    DynamicBufferRing*      m_pConstantBufferRing = nullptr;
    uint32_t                m_MaxNumThreadgroups = 800;

    uint32_t                m_ScratchBufferSize;
    uint32_t                m_ReducedScratchBufferSize;
    
    // Sample resources
    VkBuffer                m_SrcKeyBuffers[3];     // 32 bit source key buffers (for 1080, 2K, 4K resolution)
    VmaAllocation           m_SrcKeyBufferAllocations[3];

    VkBuffer        m_SrcPayloadBuffers;    // 32 bit source payload buffers
    VmaAllocation   m_SrcPayloadBufferAllocation;

    VkBuffer        m_DstKeyBuffers[2];     // 32 bit destination key buffers (when not doing in place writes)
    VmaAllocation   m_DstKeyBufferAllocations[2];

    VkBuffer        m_DstPayloadBuffers[2]; // 32 bit destination payload buffers (when not doing in place writes)
    VmaAllocation   m_DstPayloadBufferAllocations[2];

    VkBuffer        m_FPSScratchBuffer;             // Sort scratch buffer
    VmaAllocation   m_FPSScratchBufferAllocation;

    VkBuffer        m_FPSReducedScratchBuffer;      // Sort reduced scratch buffer
    VmaAllocation   m_FPSReducedScratchBufferAllocation;

    VkDescriptorSetLayout   m_SortDescriptorSetLayoutConstants;
    VkDescriptorSet         m_SortDescriptorSetConstants[3];
    VkDescriptorSetLayout   m_SortDescriptorSetLayoutConstantsIndirect;
    VkDescriptorSet         m_SortDescriptorSetConstantsIndirect[3];

    VkDescriptorSetLayout   m_SortDescriptorSetLayoutInputOutputs;
    VkDescriptorSetLayout   m_SortDescriptorSetLayoutScan;
    VkDescriptorSetLayout   m_SortDescriptorSetLayoutScratch;
    VkDescriptorSetLayout   m_SortDescriptorSetLayoutIndirect;

    VkDescriptorSet         m_SortDescriptorSetInputOutput[2];
    VkDescriptorSet         m_SortDescriptorSetScanSets[2];
    VkDescriptorSet         m_SortDescriptorSetScratch;
    VkDescriptorSet         m_SortDescriptorSetIndirect;
    VkPipelineLayout        m_SortPipelineLayout;

    VkPipeline m_FPSCountPipeline;
    VkPipeline m_FPSCountReducePipeline;
    VkPipeline m_FPSScanPipeline;
    VkPipeline m_FPSScanAddPipeline;
    VkPipeline m_FPSScatterPipeline;
    VkPipeline m_FPSScatterPayloadPipeline;

    // Resources for indirect execution of algorithm
    VkBuffer        m_IndirectKeyCounts;            // Buffer to hold num keys for indirect dispatch
    VmaAllocation   m_IndirectKeyCountsAllocation;
    VkBuffer        m_IndirectConstantBuffer;       // Buffer to hold radix sort constant buffer data for indirect dispatch
    VmaAllocation   m_IndirectConstantBufferAllocation;
    VkBuffer        m_IndirectCountScatterArgs;     // Buffer to hold dispatch arguments used for Count/Scatter parts of the algorithm
    VmaAllocation   m_IndirectCountScatterArgsAllocation;
    VkBuffer        m_IndirectReduceScanArgs;       // Buffer to hold dispatch arguments used for Reduce/Scan parts of the algorithm
    VmaAllocation   m_IndirectReduceScanArgsAllocation;
        
    VkPipeline                  m_FPSIndirectSetupParametersPipeline;

    // Resources for verification render
    Texture                     m_Validate4KTexture;
    Texture                     m_Validate2KTexture;
    Texture                     m_Validate1080pTexture;
    VkImageView                 m_ValidationImageViews[3];

    VkDescriptorSetLayout       m_RenderDescriptorSetLayout0;
    VkDescriptorSet             m_RenderDescriptorSet0;
    VkDescriptorSetLayout       m_RenderDescriptorSetLayout1;
    VkDescriptorSet             m_RenderDescriptorSet1[4];
    VkDescriptorSetLayout       m_RenderDescriptorSetLayout2;
    VkDescriptorSet             m_RenderDescriptorSet2[3];
    VkPipelineLayout            m_RenderPipelineLayout;

    VkPipeline                  m_RenderResultVerificationPipeline;

    // Options for UI and test to run
    int m_UIResolutionSize = 0;
    bool m_UISortPayload = false;
    bool m_UIIndirectSort = false;
    int m_UIVisualOutput = 0;
};