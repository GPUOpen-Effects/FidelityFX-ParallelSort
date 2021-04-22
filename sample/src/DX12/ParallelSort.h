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
#include <d3d12.h>

using namespace CAULDRON_DX12;

// Uncomment the following line to enable developer mode which compiles in data verification mechanism
//#define DEVELOPERMODE

struct ParallelSortRenderCB // If you change this, also change struct ParallelSortRenderCB in ParallelSortVerify.hlsl
{
    int32_t Width;
    int32_t Height;
    int32_t SortWidth;
    int32_t SortHeight;
};

// Convenience struct for passing resource/UAV pairs around
typedef struct RdxDX12ResourceInfo
{
    ID3D12Resource* pResource;          ///< Pointer to the resource -- used for barriers and syncs (must NOT be nullptr)
    D3D12_GPU_DESCRIPTOR_HANDLE resourceGPUHandle;  ///< The GPU Descriptor Handle to use for binding the resource
} RdxDX12ResourceInfo;

namespace CAULDRON_DX12
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

    void Sort(ID3D12GraphicsCommandList* pCommandList, bool isBenchmarking, float benchmarkTime);
#ifdef DEVELOPERMODE
    void WaitForValidationResults();
#endif // DEVELOPERMODE
    void CopySourceDataForFrame(ID3D12GraphicsCommandList* pCommandList);
    void DrawGui();
    void DrawVisualization(ID3D12GraphicsCommandList* pCommandList, uint32_t RTWidth, uint32_t RTHeight);

    // Temp -- For command line overrides
    static void OverrideKeySet(int ResolutionOverride);
    static void OverridePayload();
    // Temp -- For command line overrides

private:
    void CreateKeyPayloadBuffers();
    void CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, ID3D12PipelineState*& pPipeline);
#ifdef DEVELOPERMODE
    void CreateValidationResources(ID3D12GraphicsCommandList* pCommandList, RdxDX12ResourceInfo* pKeyDstInfo);
#endif // DEVELOPERMODE

    // Temp -- For command line overrides
    static int KeySetOverride;
    static bool PayloadOverride;
    // Temp -- For command line overrides

    Device*             m_pDevice = nullptr;
    UploadHeap*         m_pUploadHeap = nullptr;
    ResourceViewHeaps*  m_pResourceViewHeaps = nullptr;
    DynamicBufferRing*  m_pConstantBufferRing = nullptr;
    uint32_t            m_MaxNumThreadgroups = 320; // Use a generic thread group size when not on AMD hardware (taken from experiments to determine best performance threshold)
    
    // Sample resources
    Texture             m_SrcKeyBuffers[3];     // 32 bit source key buffers (for 1080, 2K, 4K resolution)
    CBV_SRV_UAV         m_SrcKeyUAVTable;       // 32 bit source key UAVs (for 1080, 2K, 4K resolution)

    Texture             m_SrcPayloadBuffers;    // 32 bit source payload buffers
    CBV_SRV_UAV         m_SrcPayloadUAV;        // 32 bit source payload UAVs

    Texture             m_DstKeyBuffers[2];     // 32 bit destination key buffers (when not doing in place writes)
    CBV_SRV_UAV         m_DstKeyUAVTable;       // 32 bit destination key UAVs

    Texture             m_DstPayloadBuffers[2]; // 32 bit destination payload buffers (when not doing in place writes)
    CBV_SRV_UAV         m_DstPayloadUAVTable;   // 32 bit destination payload UAVs

    // Resources         for parallel sort algorithm
    Texture             m_FPSScratchBuffer;             // Sort scratch buffer
    CBV_SRV_UAV         m_FPSScratchUAV;                // UAV needed for sort scratch buffer
    Texture             m_FPSReducedScratchBuffer;      // Sort reduced scratch buffer
    CBV_SRV_UAV         m_FPSReducedScratchUAV;         // UAV needed for sort reduced scratch buffer
        
    ID3D12RootSignature* m_pFPSRootSignature            = nullptr;
    ID3D12PipelineState* m_pFPSCountPipeline            = nullptr;
    ID3D12PipelineState* m_pFPSCountReducePipeline      = nullptr;
    ID3D12PipelineState* m_pFPSScanPipeline             = nullptr;
    ID3D12PipelineState* m_pFPSScanAddPipeline          = nullptr;
    ID3D12PipelineState* m_pFPSScatterPipeline          = nullptr;
    ID3D12PipelineState* m_pFPSScatterPayloadPipeline   = nullptr;
        
    // Resources for indirect execution of algorithm
    Texture             m_IndirectKeyCounts;            // Buffer to hold num keys for indirect dispatch
    CBV_SRV_UAV         m_IndirectKeyCountsUAV;         // UAV needed for num keys buffer
    Texture             m_IndirectConstantBuffer;       // Buffer to hold radix sort constant buffer data for indirect dispatch
    CBV_SRV_UAV         m_IndirectConstantBufferUAV;    // UAV needed for indirect constant buffer
    Texture             m_IndirectCountScatterArgs;     // Buffer to hold dispatch arguments used for Count/Scatter parts of the algorithm
    CBV_SRV_UAV         m_IndirectCountScatterArgsUAV;  // UAV needed for count/scatter args buffer
    Texture             m_IndirectReduceScanArgs;       // Buffer to hold dispatch arguments used for Reduce/Scan parts of the algorithm
    CBV_SRV_UAV         m_IndirectReduceScanArgsUAV;    // UAV needed for reduce/scan args buffer
        
    ID3D12CommandSignature* m_pFPSCommandSignature;
    ID3D12PipelineState*    m_pFPSIndirectSetupParametersPipeline = nullptr;
        
    // Resources for verification render
    ID3D12RootSignature* m_pRenderRootSignature = nullptr;
    ID3D12PipelineState* m_pRenderResultVerificationPipeline = nullptr;
    Texture                 m_Validate4KTexture;
    Texture                 m_Validate2KTexture;
    Texture                 m_Validate1080pTexture;
    CBV_SRV_UAV             m_ValidateTextureSRV;

    // For correctness validation
    ID3D12Resource*         m_ReadBackBufferResource;           // For sort validation
    ID3D12Fence*            m_ReadBackFence;                    // To know when we can check sort results
    HANDLE                  m_ReadBackFenceEvent;
#ifdef DEVELOPERMODE
    bool                    m_UIValidateSortResults = false;    // Validate the results
#endif // DEVELOPERMODE

    // Options for UI and test to run
    int m_UIResolutionSize = 0;
    bool m_UISortPayload = false;
    bool m_UIIndirectSort = false;
    int m_UIVisualOutput = 0;
};