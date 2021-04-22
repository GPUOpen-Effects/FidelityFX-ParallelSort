// ParallelSort.cpp
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

#include "stdafx.h"
#include "../../../FFX-ParallelSort/FFX_ParallelSort.h"

#include <numeric>
#include <random>
#include <vector>

static const uint32_t NumKeys[] = { 1920 * 1080, 2560 * 1440, 3840 * 2160 };

//////////////////////////////////////////////////////////////////////////
    
//////////////////////////////////////////////////////////////////////////
// For doing command-line based benchmark runs
int FFXParallelSort::KeySetOverride = -1;
void FFXParallelSort::OverrideKeySet(int ResolutionOverride)
{
    KeySetOverride = ResolutionOverride;
}
bool FFXParallelSort::PayloadOverride = false;
void FFXParallelSort::OverridePayload()
{
    PayloadOverride = true;
}
//////////////////////////////////////////////////////////////////////////

// Create all of the sort data for the sample
void FFXParallelSort::CreateKeyPayloadBuffers()
{
    std::vector<uint32_t> KeyData1080(NumKeys[0]);
    std::vector<uint32_t> KeyData2K(NumKeys[1]);
    std::vector<uint32_t> KeyData4K(NumKeys[2]);
                
    // Populate the buffers with linear access index
    std::iota(KeyData1080.begin(), KeyData1080.end(), 0);
    std::iota(KeyData2K.begin(), KeyData2K.end(), 0);
    std::iota(KeyData4K.begin(), KeyData4K.end(), 0);

    // Shuffle the data
    std::shuffle(KeyData1080.begin(), KeyData1080.end(), std::mt19937{ std::random_device{}() });
    std::shuffle(KeyData2K.begin(), KeyData2K.end(), std::mt19937{ std::random_device{}() });
    std::shuffle(KeyData4K.begin(), KeyData4K.end(), std::mt19937{ std::random_device{}() });
        
    // 1080p
    CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys[0], D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_SrcKeyBuffers[0].InitBuffer(m_pDevice, "SrcKeys1080", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
    // 2K
    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys[1], D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_SrcKeyBuffers[1].InitBuffer(m_pDevice, "SrcKeys2K", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
    // 4K
    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys[2], D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_SrcKeyBuffers[2].InitBuffer(m_pDevice, "SrcKeys4K", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
    m_SrcPayloadBuffers.InitBuffer(m_pDevice, "SrcPayloadBuffer", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
        
    // The DstKey and DstPayload buffers will be used as src/dst when sorting. A copy of the 
    // source key/payload will be copied into them before hand so we can keep our original values
    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys[2], D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_DstKeyBuffers[0].InitBuffer(m_pDevice, "DstKeyBuf0", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_DstKeyBuffers[1].InitBuffer(m_pDevice, "DstKeyBuf1", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_DstPayloadBuffers[0].InitBuffer(m_pDevice, "DstPayloadBuf0", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_DstPayloadBuffers[1].InitBuffer(m_pDevice, "DstPayloadBuf1", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // Copy data in

    // 1080
    uint8_t* pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[0] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData1080.data() , sizeof(uint32_t) * NumKeys[0]);
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_SrcKeyBuffers[0].GetResource(), 0, m_pUploadHeap->GetResource(), pKeyDataBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t) * NumKeys[0]);
        
    // 2K
    pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[1] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData2K.data(), sizeof(uint32_t) * NumKeys[1]);
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_SrcKeyBuffers[1].GetResource(), 0, m_pUploadHeap->GetResource(), pKeyDataBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t) * NumKeys[1]);
        
    // 4K
    pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[2] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData4K.data(), sizeof(uint32_t) * NumKeys[2]);
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_SrcKeyBuffers[2].GetResource(), 0, m_pUploadHeap->GetResource(), pKeyDataBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t) * NumKeys[2]);
    uint8_t* pPayloadDataBuffer = m_pUploadHeap->Suballocate(NumKeys[2] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pPayloadDataBuffer, KeyData4K.data(), sizeof(uint32_t) * NumKeys[2]);    // Copy the 4k source data for payload (it doesn't matter what the payload is as we really only want it to measure cost of copying/sorting)
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_SrcPayloadBuffers.GetResource(), 0, m_pUploadHeap->GetResource(), pPayloadDataBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t) * NumKeys[2]);
        

    // Once we are done copying the data, put in barriers to transition the source resources to 
    // copy source (which is what they will stay for the duration of app runtime)
    CD3DX12_RESOURCE_BARRIER Barriers[6] = { CD3DX12_RESOURCE_BARRIER::Transition(m_SrcKeyBuffers[2].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_SrcPayloadBuffers.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_SrcKeyBuffers[1].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE), 
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_SrcKeyBuffers[0].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE), 
                                                 
                                                // Copy the data into the dst[0] buffers for use on first frame
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyBuffers[0].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadBuffers[0].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST) };
    m_pUploadHeap->GetCommandList()->ResourceBarrier(6, Barriers);

    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_DstKeyBuffers[0].GetResource(), 0, m_SrcKeyBuffers[m_UIResolutionSize].GetResource(), 0, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_DstPayloadBuffers[0].GetResource(), 0, m_SrcPayloadBuffers.GetResource(), 0, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);

    // Put the dst buffers back to UAVs for sort usage
    Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyBuffers[0].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadBuffers[0].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_pUploadHeap->GetCommandList()->ResourceBarrier(2, Barriers);

    // Create UAVs
    m_SrcKeyBuffers[2].CreateBufferUAV(2, nullptr, &m_SrcKeyUAVTable);
    m_SrcKeyBuffers[1].CreateBufferUAV(1, nullptr, &m_SrcKeyUAVTable);
    m_SrcKeyBuffers[0].CreateBufferUAV(0, nullptr, &m_SrcKeyUAVTable);
    m_SrcPayloadBuffers.CreateBufferUAV(0, nullptr, &m_SrcPayloadUAV);
    m_DstKeyBuffers[0].CreateBufferUAV(0, nullptr, &m_DstKeyUAVTable);
    m_DstKeyBuffers[1].CreateBufferUAV(1, nullptr, &m_DstKeyUAVTable);
    m_DstPayloadBuffers[0].CreateBufferUAV(0, nullptr, &m_DstPayloadUAVTable);
    m_DstPayloadBuffers[1].CreateBufferUAV(1, nullptr, &m_DstPayloadUAVTable);
}

// Compile specified radix sort shader and create pipeline
void FFXParallelSort::CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, ID3D12PipelineState*& pPipeline)
{
    std::string CompileFlags("-T cs_6_0");
#ifdef _DEBUG
    CompileFlags += " -Zi -Od";
#endif // _DEBUG

    D3D12_SHADER_BYTECODE shaderByteCode = {};
    CompileShaderFromFile(shaderFile, defines, entryPoint, CompileFlags.c_str(), &shaderByteCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = shaderByteCode;
    descPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    descPso.pRootSignature = m_pFPSRootSignature;
    descPso.NodeMask = 0;

    ThrowIfFailed(m_pDevice->GetDevice()->CreateComputePipelineState(&descPso, IID_PPV_ARGS(&pPipeline)));
    SetName(pPipeline, entryPoint);
}

// Parallel Sort initialization
void FFXParallelSort::OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, SwapChain* pSwapChain)
{
    m_pDevice = pDevice;
    m_pUploadHeap = pUploadHeap;
    m_pResourceViewHeaps = pResourceViewHeaps;
    m_pConstantBufferRing = pConstantBufferRing;
    m_MaxNumThreadgroups = 800;

    // Overrides for testing
    if (KeySetOverride >= 0)
        m_UIResolutionSize = KeySetOverride;
    if (PayloadOverride)
        m_UISortPayload = true; 

    // Allocate UAVs to use for data
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(3, &m_SrcKeyUAVTable);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_SrcPayloadUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_DstKeyUAVTable);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_DstPayloadUAVTable);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_FPSScratchUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_FPSReducedScratchUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectKeyCountsUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectConstantBufferUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectCountScatterArgsUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectReduceScanArgsUAV);
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(3, &m_ValidateTextureSRV);

    // Create resources to test with. Sorts will be done for 1080p, 2K, and 4K resolution data sets
    CreateKeyPayloadBuffers();

    // We are just going to fudge the indirect execution parameters for each resolution
    CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * 3, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_IndirectKeyCounts.InitBuffer(m_pDevice, "IndirectKeyCounts", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
    m_IndirectKeyCounts.CreateBufferUAV(0, nullptr, &m_IndirectKeyCountsUAV);
    uint8_t* pNumKeysBuffer = m_pUploadHeap->Suballocate(sizeof(uint32_t) * 3, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    memcpy(pNumKeysBuffer, NumKeys, sizeof(uint32_t) * 3);
    m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_IndirectKeyCounts.GetResource(), 0, m_pUploadHeap->GetResource(), pNumKeysBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t) * 3);
    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectKeyCounts.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_pUploadHeap->GetCommandList()->ResourceBarrier(1, &Barrier);

    // Create resources for sort validation (image that goes from shuffled to sorted)
    m_Validate1080pTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate1080p.png", false, 1.f, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
    m_Validate1080pTexture.CreateSRV(0, &m_ValidateTextureSRV, 0);
    m_Validate2KTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate2K.png", false, 1.f, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_Validate2KTexture.CreateSRV(1, &m_ValidateTextureSRV, 0);
    m_Validate4KTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate4K.png", false, 1.f, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_Validate4KTexture.CreateSRV(2, &m_ValidateTextureSRV, 0);

    // Finish up
    m_pUploadHeap->FlushAndFinish();

    // Allocate the scratch buffers needed for radix sort
    uint32_t scratchBufferSize;
    uint32_t reducedScratchBufferSize;
    FFX_ParallelSort_CalculateScratchResourceSize(NumKeys[2], scratchBufferSize, reducedScratchBufferSize);
        
    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_FPSScratchBuffer.InitBuffer(m_pDevice, "Scratch", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_FPSScratchBuffer.CreateBufferUAV(0, nullptr, &m_FPSScratchUAV);

    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(reducedScratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_FPSReducedScratchBuffer.InitBuffer(m_pDevice, "ReducedScratch", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_FPSReducedScratchBuffer.CreateBufferUAV(0, nullptr, &m_FPSReducedScratchUAV);

    // Allocate the buffers for indirect execution of the algorithm
    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(FFX_ParallelSortCB), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_IndirectConstantBuffer.InitBuffer(m_pDevice, "IndirectConstantBuffer", &ResourceDesc, sizeof(FFX_ParallelSortCB), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_IndirectConstantBuffer.CreateBufferUAV(0, nullptr, &m_IndirectConstantBufferUAV);

    ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * 3, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_IndirectCountScatterArgs.InitBuffer(m_pDevice, "IndirectCount_Scatter_DispatchArgs", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_IndirectCountScatterArgs.CreateBufferUAV(0, nullptr, &m_IndirectCountScatterArgsUAV);
    m_IndirectReduceScanArgs.InitBuffer(m_pDevice, "IndirectReduceScanArgs", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_IndirectReduceScanArgs.CreateBufferUAV(0, nullptr, &m_IndirectReduceScanArgsUAV);

    // Create root signature for Radix sort passes
    {
        D3D12_DESCRIPTOR_RANGE descRange[15];
        D3D12_ROOT_PARAMETER rootParams[16];

        // Constant buffer table (always have 1)
        descRange[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[0].Descriptor = { descRange[0].BaseShaderRegister, descRange[0].RegisterSpace };

        // Constant buffer to setup indirect params (indirect)
        descRange[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[1].Descriptor = { descRange[1].BaseShaderRegister, descRange[1].RegisterSpace };

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[2].Constants = { 2, 0, 1 };

        // SrcBuffer (sort or scan)
        descRange[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[3].DescriptorTable = { 1, &descRange[2] };

        // ScrPayload (sort only)
        descRange[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[4].DescriptorTable = { 1, &descRange[3] };

        // Scratch (sort only)
        descRange[4] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 2, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[5].DescriptorTable = { 1, &descRange[4] };

        // Scratch (reduced)
        descRange[5] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 3, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[6].DescriptorTable = { 1, &descRange[5] };

        // DstBuffer (sort or scan)
        descRange[6] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 4, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[7].DescriptorTable = { 1, &descRange[6] };

        // DstPayload (sort only)
        descRange[7] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 5, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[8].DescriptorTable = { 1, &descRange[7] };

        // ScanSrc
        descRange[8] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 6, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[9].DescriptorTable = { 1, &descRange[8] };

        // ScanDst
        descRange[9] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 7, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[10].DescriptorTable = { 1, &descRange[9] };

        // ScanScratch
        descRange[10] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 8, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[11].DescriptorTable = { 1, &descRange[10] };

        // NumKeys (indirect)
        descRange[11] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 9, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[12].DescriptorTable = { 1, &descRange[11] };

        // CBufferUAV (indirect)
        descRange[12] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 10, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[13].DescriptorTable = { 1, &descRange[12] };
            
        // CountScatterArgs (indirect)
        descRange[13] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 11, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[14].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[14].DescriptorTable = { 1, &descRange[13] };
            
        // ReduceScanArgs (indirect)
        descRange[14] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 12, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        rootParams[15].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[15].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams[15].DescriptorTable = { 1, &descRange[14] };

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 16;
        rootSigDesc.pParameters = rootParams;
        rootSigDesc.NumStaticSamplers = 0;
        rootSigDesc.pStaticSamplers = nullptr;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* pOutBlob, * pErrorBlob = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
        ThrowIfFailed(pDevice->GetDevice()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pFPSRootSignature)));
        SetName(m_pFPSRootSignature, "FPS_Signature");

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();

        // Also create the command signature for the indirect version
        D3D12_INDIRECT_ARGUMENT_DESC dispatch = {};
        dispatch.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC desc = {};
        desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        desc.NodeMask = 0;
        desc.NumArgumentDescs = 1;
        desc.pArgumentDescs = &dispatch;

        ThrowIfFailed(pDevice->GetDevice()->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pFPSCommandSignature)));
        m_pFPSCommandSignature->SetName(L"FPS_CommandSignature");
    }

    // Create root signature for Render of RadixBuffer info
    {
        CD3DX12_DESCRIPTOR_RANGE    DescRange[3];
        CD3DX12_ROOT_PARAMETER      RTSlot[3];

        // Constant buffer
        DescRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);
        RTSlot[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

        // UAV for RadixBufer
        DescRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
        RTSlot[1].InitAsDescriptorTable(1, &DescRange[1], D3D12_SHADER_VISIBILITY_ALL);

        // SRV for Validation texture
        DescRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
        RTSlot[2].InitAsDescriptorTable(1, &DescRange[2], D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_ROOT_SIGNATURE_DESC descRootSignature = CD3DX12_ROOT_SIGNATURE_DESC();
        descRootSignature.NumParameters = 3;
        descRootSignature.pParameters = RTSlot;
        descRootSignature.NumStaticSamplers = 0;
        descRootSignature.pStaticSamplers = nullptr;
        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* pOutBlob, * pErrorBlob = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob));
        ThrowIfFailed(pDevice->GetDevice()->CreateRootSignature(0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_pRenderRootSignature)));
        SetName(m_pRenderRootSignature, "FPS_RenderResults_Signature");

        pOutBlob->Release();
        if (pErrorBlob)
            pErrorBlob->Release();
    }

    //////////////////////////////////////////////////////////////////////////
    // Create pipelines for radix sort
    {
        // Create all of the necessary pipelines for Sort and Scan
        
        // SetupIndirectParams (indirect only)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_SetupIndirectParameters", m_pFPSIndirectSetupParametersPipeline);

        // Radix count (sum table generation)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_Count", m_pFPSCountPipeline);
        // Radix count reduce (sum table reduction for offset prescan)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_CountReduce", m_pFPSCountReducePipeline);
        // Radix scan (prefix scan)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_Scan", m_pFPSScanPipeline);
        // Radix scan add (prefix scan + reduced prefix scan addition)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_ScanAdd", m_pFPSScanAddPipeline);
        // Radix scatter (key redistribution)
        CompileRadixPipeline("ParallelSortCS.hlsl", nullptr, "FPS_Scatter", m_pFPSScatterPipeline);
        
        // Radix scatter with payload (key and payload redistribution)
        DefineList defines;
        defines["kRS_ValueCopy"] = std::to_string(1);
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scatter", m_pFPSScatterPayloadPipeline);
    }

    //////////////////////////////////////////////////////////////////////////
    // Create pipelines for render pass
    {
#ifdef _DEBUG
        std::string CompileFlagsVS("-T vs_6_0 -Zi -Od");
        std::string CompileFlagsPS("-T ps_6_0 -Zi -Od");
#else
        std::string CompileFlagsVS("-T vs_6_0");
        std::string CompileFlagsPS("-T ps_6_0");
#endif // _DEBUG
        
        D3D12_SHADER_BYTECODE shaderByteCodeVS = {};
        CompileShaderFromFile("ParallelSortVerify.hlsl", nullptr, "FullscreenVS", CompileFlagsVS.c_str(), &shaderByteCodeVS);

        D3D12_SHADER_BYTECODE shaderByteCodePS = {};
        CompileShaderFromFile("ParallelSortVerify.hlsl", nullptr, "RenderSortValidationPS", CompileFlagsPS.c_str(), &shaderByteCodePS);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
        descPso.InputLayout = { nullptr, 0 };
        descPso.pRootSignature = m_pRenderRootSignature;
        descPso.VS = shaderByteCodeVS;
        descPso.PS = shaderByteCodePS;
        descPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        descPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        descPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        descPso.BlendState.RenderTarget[0].BlendEnable = FALSE;
        descPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        descPso.DepthStencilState.DepthEnable = FALSE;
        descPso.SampleMask = UINT_MAX;
        descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        descPso.NumRenderTargets = 1;
        descPso.RTVFormats[0] = pSwapChain->GetFormat();
        descPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        descPso.SampleDesc.Count = 1;
        descPso.NodeMask = 0;
        ThrowIfFailed(m_pDevice->GetDevice()->CreateGraphicsPipelineState(&descPso, IID_PPV_ARGS(&m_pRenderResultVerificationPipeline)));
        SetName(m_pRenderResultVerificationPipeline, "RenderFPSResults_Pipeline");
    }
}

// Parallel Sort termination
void FFXParallelSort::OnDestroy()
{
    // Release verification render resources
    m_pRenderResultVerificationPipeline->Release();
    m_pRenderRootSignature->Release();
    m_Validate4KTexture.OnDestroy();
    m_Validate2KTexture.OnDestroy();
    m_Validate1080pTexture.OnDestroy();

    // Release radix sort indirect resources
    m_IndirectKeyCounts.OnDestroy();
    m_IndirectConstantBuffer.OnDestroy();
    m_IndirectCountScatterArgs.OnDestroy();
    m_IndirectReduceScanArgs.OnDestroy();
    m_pFPSCommandSignature->Release();
    m_pFPSIndirectSetupParametersPipeline->Release();

    // Release radix sort algorithm resources
    m_FPSScratchBuffer.OnDestroy();
    m_FPSReducedScratchBuffer.OnDestroy();
    m_pFPSRootSignature->Release();
    m_pFPSCountPipeline->Release();
    m_pFPSCountReducePipeline->Release();
    m_pFPSScanPipeline->Release();
    m_pFPSScanAddPipeline->Release();
    m_pFPSScatterPipeline->Release();
    m_pFPSScatterPayloadPipeline->Release();

    // Release all of our resources
    m_SrcKeyBuffers[0].OnDestroy();
    m_SrcKeyBuffers[1].OnDestroy();
    m_SrcKeyBuffers[2].OnDestroy();
    m_SrcPayloadBuffers.OnDestroy();
    m_DstKeyBuffers[0].OnDestroy();
    m_DstKeyBuffers[1].OnDestroy();
    m_DstPayloadBuffers[0].OnDestroy();
    m_DstPayloadBuffers[1].OnDestroy();
}

// This allows us to validate that the sorted data is actually in ascending order. Only used when doing algorithm changes.
#ifdef DEVELOPERMODE
void FFXParallelSort::CreateValidationResources(ID3D12GraphicsCommandList* pCommandList, RdxDX12ResourceInfo* pKeyDstInfo)
{
    // Create the read-back resource
    CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys[m_UIResolutionSize], D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(m_pDevice->GetDevice()->CreateCommittedResource(&readBackHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                    nullptr, IID_PPV_ARGS(&m_ReadBackBufferResource)));
    m_ReadBackBufferResource->SetName(L"Validation Read-back Buffer");

    // And the fence for us to wait on
    ThrowIfFailed(m_pDevice->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_ReadBackFence)));
    m_ReadBackFence->SetName(L"Validation Read-back Fence");
                
    // Transition, copy, and transition back
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pKeyDstInfo->pResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
    pCommandList->CopyBufferRegion(m_ReadBackBufferResource, 0, pKeyDstInfo->pResource, 0, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pKeyDstInfo->pResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void FFXParallelSort::WaitForValidationResults()
{
    if (!m_ReadBackFence && !m_ReadBackBufferResource)
        return;

    // Insert the fence to wait on and create the event to trigger when it's been processed
    ThrowIfFailed(m_pDevice->GetGraphicsQueue()->Signal(m_ReadBackFence, 1));
    m_ReadBackFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_ReadBackFence->SetEventOnCompletion(1, m_ReadBackFenceEvent);

    // Wait for fence to have been processed
    WaitForSingleObject(m_ReadBackFenceEvent, INFINITE);
    CloseHandle(m_ReadBackFenceEvent);

    // Validate data ...
    Trace("Validating Data");

    D3D12_RANGE range;
    range.Begin = 0;
    range.End = sizeof(uint32_t) * NumKeys[m_UIResolutionSize];
    void* pData;
    m_ReadBackBufferResource->Map(0, &range, &pData);

    uint32_t* SortedData = (uint32_t*)pData;
        
    // Do the validation
    uint32_t keysToValidate = NumKeys[m_UIResolutionSize];
    bool dataValid = true;

    for (uint32_t i = 0; i < keysToValidate - 1; i++)
    {
        if (SortedData[i] > SortedData[i + 1])
        {
            std::string message = "Sort invalidated. Entry ";
            message += std::to_string(i);
            message += " is larger next entry.\n";
            Trace(message);
            dataValid = false;
        }
    }

    m_ReadBackBufferResource->Unmap(0, nullptr);

    if (dataValid)
        Trace("Data Valid");

    // We are done with the fence and the read-back buffer
    m_ReadBackBufferResource->Release();
    m_ReadBackBufferResource = nullptr;
    m_ReadBackFence->Release();
    m_ReadBackFence = nullptr;
}
#endif // DEVELOPERMODE

// Because we are sorting the data every frame, need to reset to unsorted version of data before running sort
void FFXParallelSort::CopySourceDataForFrame(ID3D12GraphicsCommandList* pCommandList)
{
    // Copy the contents the source buffer to the dstBuffer[0] each frame in order to not 
    // lose our original data

    // Copy the data into the dst[0] buffers for use on first frame
    CD3DX12_RESOURCE_BARRIER Barriers[2] = { CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyBuffers[0].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST),
                                                CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadBuffers[0].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST) };
    pCommandList->ResourceBarrier(2, Barriers);

    pCommandList->CopyBufferRegion(m_DstKeyBuffers[0].GetResource(), 0, m_SrcKeyBuffers[m_UIResolutionSize].GetResource(), 0, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    pCommandList->CopyBufferRegion(m_DstPayloadBuffers[0].GetResource(), 0, m_SrcPayloadBuffers.GetResource(), 0, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);

    // Put the dst buffers back to UAVs for sort usage
    Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyBuffers[0].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadBuffers[0].GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    pCommandList->ResourceBarrier(2, Barriers);
}

// Perform Parallel Sort (radix-based sort)
void FFXParallelSort::Sort(ID3D12GraphicsCommandList* pCommandList, bool isBenchmarking, float benchmarkTime)
{
    bool bIndirectDispatch = m_UIIndirectSort;

    std::string markerText = "FFXParallelSort";
    if (bIndirectDispatch) markerText += " Indirect";
    UserMarker marker(pCommandList, markerText.c_str());

    FFX_ParallelSortCB  constantBufferData = { 0 };

    // Bind the descriptor heaps
    ID3D12DescriptorHeap* pDescriptorHeap = m_pResourceViewHeaps->GetCBV_SRV_UAVHeap();
    pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);

    // Bind the root signature
    pCommandList->SetComputeRootSignature(m_pFPSRootSignature);

    // Fill in the constant buffer data structure (this will be done by a shader in the indirect version)
    uint32_t NumThreadgroupsToRun;
    uint32_t NumReducedThreadgroupsToRun;
    if (!bIndirectDispatch)
    {
        uint32_t NumberOfKeys = NumKeys[m_UIResolutionSize];
        FFX_ParallelSort_SetConstantAndDispatchData(NumberOfKeys, m_MaxNumThreadgroups, constantBufferData, NumThreadgroupsToRun, NumReducedThreadgroupsToRun);
    }
    else
    {
        struct SetupIndirectCB
        {
            uint32_t NumKeysIndex;
            uint32_t MaxThreadGroups;
        };
        SetupIndirectCB IndirectSetupCB;
        IndirectSetupCB.NumKeysIndex = m_UIResolutionSize;
        IndirectSetupCB.MaxThreadGroups = m_MaxNumThreadgroups;
            
        // Copy the data into the constant buffer
        D3D12_GPU_VIRTUAL_ADDRESS constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(SetupIndirectCB), &IndirectSetupCB);
        pCommandList->SetComputeRootConstantBufferView(1, constantBuffer);  // SetupIndirect Constant buffer

        // Bind other buffer
        pCommandList->SetComputeRootDescriptorTable(12, m_IndirectKeyCountsUAV.GetGPU());           // Key counts
        pCommandList->SetComputeRootDescriptorTable(13, m_IndirectConstantBufferUAV.GetGPU());      // Indirect Sort Constant Buffer
        pCommandList->SetComputeRootDescriptorTable(14, m_IndirectCountScatterArgsUAV.GetGPU());    // Indirect Sort Count/Scatter Args
        pCommandList->SetComputeRootDescriptorTable(15, m_IndirectReduceScanArgsUAV.GetGPU());      // Indirect Sort Reduce/Scan Args
            
        // Dispatch
        pCommandList->SetPipelineState(m_pFPSIndirectSetupParametersPipeline);
        pCommandList->Dispatch(1, 1, 1);

        // When done, transition the args buffers to INDIRECT_ARGUMENT, and the constant buffer UAV to Constant buffer
        CD3DX12_RESOURCE_BARRIER barriers[5];
        barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(m_IndirectCountScatterArgs.GetResource());
        barriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(m_IndirectReduceScanArgs.GetResource());
        barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectConstantBuffer.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        barriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectCountScatterArgs.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        barriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectReduceScanArgs.GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        pCommandList->ResourceBarrier(5, barriers);
    }

    // Setup resource/UAV pairs to use during sort
    RdxDX12ResourceInfo KeySrcInfo = { m_DstKeyBuffers[0].GetResource(), m_DstKeyUAVTable.GetGPU(0) };
    RdxDX12ResourceInfo PayloadSrcInfo = { m_DstPayloadBuffers[0].GetResource(), m_DstPayloadUAVTable.GetGPU(0) };
    RdxDX12ResourceInfo KeyTmpInfo = { m_DstKeyBuffers[1].GetResource(), m_DstKeyUAVTable.GetGPU(1) };
    RdxDX12ResourceInfo PayloadTmpInfo = { m_DstPayloadBuffers[1].GetResource(), m_DstPayloadUAVTable.GetGPU(1) };
    RdxDX12ResourceInfo ScratchBufferInfo = { m_FPSScratchBuffer.GetResource(), m_FPSScratchUAV.GetGPU() };
    RdxDX12ResourceInfo ReducedScratchBufferInfo = { m_FPSReducedScratchBuffer.GetResource(), m_FPSReducedScratchUAV.GetGPU() };

    // Buffers to ping-pong between when writing out sorted values
    const RdxDX12ResourceInfo* ReadBufferInfo(&KeySrcInfo), * WriteBufferInfo(&KeyTmpInfo);
    const RdxDX12ResourceInfo* ReadPayloadBufferInfo(&PayloadSrcInfo), * WritePayloadBufferInfo(&PayloadTmpInfo);
    bool bHasPayload = m_UISortPayload;

    // Setup barriers for the run
    CD3DX12_RESOURCE_BARRIER barriers[3];
        
    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    for (uint32_t Shift = 0; Shift < 32u; Shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
    {
        // Update the bit shift
        pCommandList->SetComputeRoot32BitConstant(2, Shift, 0);

        // Copy the data into the constant buffer
        D3D12_GPU_VIRTUAL_ADDRESS constantBuffer;
        if (bIndirectDispatch)
            constantBuffer = m_IndirectConstantBuffer.GetResource()->GetGPUVirtualAddress();
        else
            constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(FFX_ParallelSortCB), &constantBufferData);

        // Bind to root signature
        pCommandList->SetComputeRootConstantBufferView(0, constantBuffer);                      // Constant buffer
        pCommandList->SetComputeRootDescriptorTable(3, ReadBufferInfo->resourceGPUHandle);      // SrcBuffer 
        pCommandList->SetComputeRootDescriptorTable(5, ScratchBufferInfo.resourceGPUHandle);    // Scratch buffer

        // Sort Count
        {
            pCommandList->SetPipelineState(m_pFPSCountPipeline);

            if (bIndirectDispatch)
            {
                pCommandList->ExecuteIndirect(m_pFPSCommandSignature, 1, m_IndirectCountScatterArgs.GetResource(), 0, nullptr, 0);
            }
            else
            {
                pCommandList->Dispatch(NumThreadgroupsToRun, 1, 1);
            }
        }

        // UAV barrier on the sum table
        barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(ScratchBufferInfo.pResource);
        pCommandList->ResourceBarrier(1, barriers);

        pCommandList->SetComputeRootDescriptorTable(6, ReducedScratchBufferInfo.resourceGPUHandle); // Scratch reduce buffer

        // Sort Reduce
        {
            pCommandList->SetPipelineState(m_pFPSCountReducePipeline);

            if (bIndirectDispatch)
            {
                pCommandList->ExecuteIndirect(m_pFPSCommandSignature, 1, m_IndirectReduceScanArgs.GetResource(), 0, nullptr, 0);
            }
            else
            {
                pCommandList->Dispatch(NumReducedThreadgroupsToRun, 1, 1);
            }

            // UAV barrier on the reduced sum table
            barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(ReducedScratchBufferInfo.pResource);
            pCommandList->ResourceBarrier(1, barriers);
        }

        // Sort Scan
        {
            // First do scan prefix of reduced values
            pCommandList->SetComputeRootDescriptorTable(9, ReducedScratchBufferInfo.resourceGPUHandle);
            pCommandList->SetComputeRootDescriptorTable(10, ReducedScratchBufferInfo.resourceGPUHandle);

            pCommandList->SetPipelineState(m_pFPSScanPipeline);
            if (!bIndirectDispatch)
            {
                assert(NumReducedThreadgroupsToRun < FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE && "Need to account for bigger reduced histogram scan");
            }
            pCommandList->Dispatch(1, 1, 1);

            // UAV barrier on the reduced sum table
            barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(ReducedScratchBufferInfo.pResource);
            pCommandList->ResourceBarrier(1, barriers);

            // Next do scan prefix on the histogram with partial sums that we just did
            pCommandList->SetComputeRootDescriptorTable(9, ScratchBufferInfo.resourceGPUHandle);
            pCommandList->SetComputeRootDescriptorTable(10, ScratchBufferInfo.resourceGPUHandle);
            pCommandList->SetComputeRootDescriptorTable(11, ReducedScratchBufferInfo.resourceGPUHandle);

            pCommandList->SetPipelineState(m_pFPSScanAddPipeline);
            if (bIndirectDispatch)
            {
                pCommandList->ExecuteIndirect(m_pFPSCommandSignature, 1, m_IndirectReduceScanArgs.GetResource(), 0, nullptr, 0);
            }
            else
            {
                pCommandList->Dispatch(NumReducedThreadgroupsToRun, 1, 1);
            }
        }

        // UAV barrier on the sum table
        barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(ScratchBufferInfo.pResource);
        pCommandList->ResourceBarrier(1, barriers);

        if (bHasPayload)
        {
            pCommandList->SetComputeRootDescriptorTable(4, ReadPayloadBufferInfo->resourceGPUHandle);   // ScrPayload
            pCommandList->SetComputeRootDescriptorTable(8, WritePayloadBufferInfo->resourceGPUHandle);  // DstPayload
        }

        pCommandList->SetComputeRootDescriptorTable(7, WriteBufferInfo->resourceGPUHandle);         // DstBuffer 

        // Sort Scatter
        {
            pCommandList->SetPipelineState(bHasPayload ? m_pFPSScatterPayloadPipeline : m_pFPSScatterPipeline);

            if (bIndirectDispatch)
            {
                pCommandList->ExecuteIndirect(m_pFPSCommandSignature, 1, m_IndirectCountScatterArgs.GetResource(), 0, nullptr, 0);
            }
            else
            {
                pCommandList->Dispatch(NumThreadgroupsToRun, 1, 1);
            }
        }
            
        // Finish doing everything and barrier for the next pass
        int numBarriers = 0;
        barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(WriteBufferInfo->pResource);
        if (bHasPayload)
            barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(WritePayloadBufferInfo->pResource);
        pCommandList->ResourceBarrier(numBarriers, barriers);

        // Swap read/write sources
        std::swap(ReadBufferInfo, WriteBufferInfo);
        if (bHasPayload)
            std::swap(ReadPayloadBufferInfo, WritePayloadBufferInfo);
    }

    // When we are all done, transition indirect buffers back to UAV for the next frame (if doing indirect dispatch)
    if (bIndirectDispatch)
    {
        barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectCountScatterArgs.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectReduceScanArgs.GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectConstantBuffer.GetResource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pCommandList->ResourceBarrier(3, barriers);
    }

    // Do we need to validate the results? If so, create a read back buffer to use for this frame
#ifdef DEVELOPERMODE
    if (m_UIValidateSortResults && !isBenchmarking)
    {
        CreateValidationResources(pCommandList, &KeySrcInfo);
        // Only do this for 1 frame
        m_UIValidateSortResults = false;
    }
#endif // DEVELOPERMODE
}

// Render Parallel Sort related GUI
void FFXParallelSort::DrawGui()
{
    if (ImGui::CollapsingHeader("FFX Parallel Sort", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const char* ResolutionSizeStrings[] = { "1920x1080", "2560x1440", "3840x2160" };

        ImVec2 textSize = ImGui::CalcTextSize("3840x2160");
        if (KeySetOverride < 0)
        {
            ImGui::PushItemWidth(textSize.x * 2);
            ImGui::Combo("Sort Buffer Resolution", &m_UIResolutionSize, ResolutionSizeStrings, _countof(ResolutionSizeStrings));
            ImGui::PopItemWidth();
        }

        ImGui::Checkbox("Sort Payload", &m_UISortPayload);
        ImGui::Checkbox("Use Indirect Execution", &m_UIIndirectSort);
#ifdef DEVELOPERMODE
        if (ImGui::Button("Validate Sort Results"))
            m_UIValidateSortResults = true;
#endif // DEVELOPERMODE

        ImGui::RadioButton("Render Unsorted Keys", &m_UIVisualOutput, 0);
        ImGui::RadioButton("Render Sorted Keys", &m_UIVisualOutput, 1);
    }
}

// Renders the image with the sorted/unsorted indicies for visual representation
void FFXParallelSort::DrawVisualization(ID3D12GraphicsCommandList* pCommandList, uint32_t RTWidth, uint32_t RTHeight)
{
    // Setup the constant buffer
    ParallelSortRenderCB ConstantBuffer;
    ConstantBuffer.Width = RTWidth;
    ConstantBuffer.Height = RTHeight;
    static const uint32_t SortWidths[] = { 1920, 2560, 3840 };
    static const uint32_t SortHeights[] = { 1080, 1440, 2160 };
    ConstantBuffer.SortWidth = SortWidths[m_UIResolutionSize];
    ConstantBuffer.SortHeight = SortHeights[m_UIResolutionSize];

    // Bind root signature and descriptor heaps
    ID3D12DescriptorHeap* pDescriptorHeap = m_pResourceViewHeaps->GetCBV_SRV_UAVHeap();
    pCommandList->SetDescriptorHeaps(1, &pDescriptorHeap);
    pCommandList->SetGraphicsRootSignature(m_pRenderRootSignature);

    // Bind constant buffer
    D3D12_GPU_VIRTUAL_ADDRESS GPUCB = m_pConstantBufferRing->AllocConstantBuffer(sizeof(ParallelSortRenderCB), &ConstantBuffer);
    pCommandList->SetGraphicsRootConstantBufferView(0, GPUCB);

    // If we are showing unsorted values, need to transition the source data buffer from copy source to UAV and back
    if (!m_UIVisualOutput)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SrcKeyBuffers[m_UIResolutionSize].GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        pCommandList->ResourceBarrier(1, &barrier);
        pCommandList->SetGraphicsRootDescriptorTable(1, m_SrcKeyUAVTable.GetGPU(m_UIResolutionSize));
    }
    else
        pCommandList->SetGraphicsRootDescriptorTable(1, m_DstKeyUAVTable.GetGPU(0));

    // Bind validation texture
    pCommandList->SetGraphicsRootDescriptorTable(2, m_ValidateTextureSRV.GetGPU(m_UIResolutionSize));

    D3D12_VIEWPORT vp = {};
    vp.Width = (float)RTWidth;
    vp.Height = (float)RTHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = vp.TopLeftY = 0.0f;
    pCommandList->RSSetViewports(1, &vp);

    // Set the shader and dispatch
    pCommandList->IASetVertexBuffers(0, 0, nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->SetPipelineState(m_pRenderResultVerificationPipeline);
    pCommandList->DrawInstanced(3, 1, 0, 0);

    // If we are showing unsorted values, need to transition the source data buffer from copy source to UAV and back
    if (!m_UIVisualOutput)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_SrcKeyBuffers[m_UIResolutionSize].GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        pCommandList->ResourceBarrier(1, &barrier);
    }
}
