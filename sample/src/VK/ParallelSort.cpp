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

//////////////////////////////////////////////////////////////////////////
// Helper functions for Vulkan

// Transition barrier
VkBufferMemoryBarrier BufferTransition(VkBuffer buffer, VkAccessFlags before, VkAccessFlags after, uint32_t size)
{
    VkBufferMemoryBarrier bufferBarrier = {};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask = before;
    bufferBarrier.dstAccessMask = after;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer;
    bufferBarrier.size = size;

    return bufferBarrier;
}

// Constant buffer binding
void FFXParallelSort::BindConstantBuffer(VkDescriptorBufferInfo& GPUCB, VkDescriptorSet& DescriptorSet, uint32_t Binding/*=0*/, uint32_t Count/*=1*/)
{
    VkWriteDescriptorSet write_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write_set.pNext = nullptr;
    write_set.dstSet = DescriptorSet;
    write_set.dstBinding = Binding;
    write_set.dstArrayElement = 0;
    write_set.descriptorCount = Count;
    write_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write_set.pImageInfo = nullptr;
    write_set.pBufferInfo = &GPUCB;
    write_set.pTexelBufferView = nullptr;
    vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &write_set, 0, nullptr);
}

// UAV Buffer binding
void FFXParallelSort::BindUAVBuffer(VkBuffer* pBuffer, VkDescriptorSet& DescriptorSet, uint32_t Binding/*=0*/, uint32_t Count/*=1*/)
{
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    for (uint32_t i = 0; i < Count; i++)
    {
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = pBuffer[i];
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        bufferInfos.push_back(bufferInfo);
    }

    VkWriteDescriptorSet write_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write_set.pNext = nullptr;
    write_set.dstSet = DescriptorSet;
    write_set.dstBinding = Binding;
    write_set.dstArrayElement = 0;
    write_set.descriptorCount = Count;
    write_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_set.pImageInfo = nullptr;
    write_set.pBufferInfo = bufferInfos.data();
    write_set.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &write_set, 0, nullptr);
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

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.memoryTypeBits = 0;
    allocCreateInfo.pool = VK_NULL_HANDLE;
    allocCreateInfo.preferredFlags = 0;
    allocCreateInfo.requiredFlags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
        
    // 1080p
    bufferCreateInfo.size = sizeof(uint32_t) * NumKeys[0];
    allocCreateInfo.pUserData = "SrcKeys1080";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_SrcKeyBuffers[0], &m_SrcKeyBufferAllocations[0], nullptr))
    {
        Trace("Failed to create buffer for SrcKeys1080");
    }
    // 2K
    bufferCreateInfo.size = sizeof(uint32_t) * NumKeys[1];
    allocCreateInfo.pUserData = "SrcKeys2K";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_SrcKeyBuffers[1], &m_SrcKeyBufferAllocations[1], nullptr))
    {
        Trace("Failed to create buffer for SrcKeys2K");
    }
    // 4K
    bufferCreateInfo.size = sizeof(uint32_t) * NumKeys[2];
    allocCreateInfo.pUserData = "SrcKeys4K";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_SrcKeyBuffers[2], &m_SrcKeyBufferAllocations[2], nullptr))
    {
        Trace("Failed to create buffer for SrcKeys4K");
    }
    allocCreateInfo.pUserData = "SrcPayloadBuffer";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_SrcPayloadBuffers, &m_SrcPayloadBufferAllocation, nullptr))
    {
        Trace("Failed to create buffer for SrcPayloadBuffer");
    }

    // Clear out transfer bit on remaining buffers
    bufferCreateInfo.usage &= ~VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // The DstKey and DstPayload buffers will be used as src/dst when sorting. A copy of the 
    // source key/payload will be copied into them before hand so we can keep our original values
    bufferCreateInfo.size = sizeof(uint32_t) * NumKeys[2];
    allocCreateInfo.pUserData = "DstKeyBuf0";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_DstKeyBuffers[0], &m_DstKeyBufferAllocations[0], nullptr))
    {
        Trace("Failed to create buffer for DstKeyBuf0");
    }

    allocCreateInfo.pUserData = "DstKeyBuf1";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_DstKeyBuffers[1], &m_DstKeyBufferAllocations[1], nullptr))
    {
        Trace("Failed to create buffer for DstKeyBuf1");
    }

    allocCreateInfo.pUserData = "DstPayloadBuf0";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_DstPayloadBuffers[0], &m_DstPayloadBufferAllocations[0], nullptr))
    {
        Trace("Failed to create buffer for DstPayloadBuf0");
    }

    allocCreateInfo.pUserData = "DstPayloadBuf1";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_DstPayloadBuffers[1], &m_DstPayloadBufferAllocations[1], nullptr))
    {
        Trace("Failed to create buffer for DstPayloadBuf1");
    }

    // Copy data in
    VkBufferCopy copyInfo = { 0 };
    // 1080
    uint8_t* pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[0] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData1080.data() , sizeof(uint32_t) * NumKeys[0]);
    copyInfo.srcOffset = pKeyDataBuffer - m_pUploadHeap->BasePtr();
    copyInfo.size = sizeof(uint32_t) * NumKeys[0];
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_pUploadHeap->GetResource(), m_SrcKeyBuffers[0], 1, &copyInfo);
        
    // 2K
    pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[1] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData2K.data(), sizeof(uint32_t) * NumKeys[1]);
    copyInfo.srcOffset = pKeyDataBuffer - m_pUploadHeap->BasePtr();
    copyInfo.size = sizeof(uint32_t) * NumKeys[1];
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_pUploadHeap->GetResource(), m_SrcKeyBuffers[1], 1, &copyInfo);
        
    // 4K
    pKeyDataBuffer = m_pUploadHeap->Suballocate(NumKeys[2] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pKeyDataBuffer, KeyData4K.data(), sizeof(uint32_t) * NumKeys[2]);
    copyInfo.srcOffset = pKeyDataBuffer - m_pUploadHeap->BasePtr();
    copyInfo.size = sizeof(uint32_t) * NumKeys[2];
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_pUploadHeap->GetResource(), m_SrcKeyBuffers[2], 1, &copyInfo);

    uint8_t* pPayloadDataBuffer = m_pUploadHeap->Suballocate(NumKeys[2] * sizeof(uint32_t), sizeof(uint32_t));
    memcpy(pPayloadDataBuffer, KeyData4K.data(), sizeof(uint32_t) * NumKeys[2]);    // Copy the 4k source data for payload (it doesn't matter what the payload is as we really only want it to measure cost of copying/sorting)
    copyInfo.srcOffset = pPayloadDataBuffer - m_pUploadHeap->BasePtr();
    copyInfo.size = sizeof(uint32_t) * NumKeys[2];
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_pUploadHeap->GetResource(), m_SrcPayloadBuffers, 1, &copyInfo);      

    // Once we are done copying the data, put in barriers to transition the source resources to 
    // copy source (which is what they will stay for the duration of app runtime)
    VkBufferMemoryBarrier Barriers[6] = { BufferTransition(m_SrcKeyBuffers[2], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sizeof(uint32_t) * NumKeys[2]),
                                            BufferTransition(m_SrcPayloadBuffers, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sizeof(uint32_t) * NumKeys[2]),
                                            BufferTransition(m_SrcKeyBuffers[1], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sizeof(uint32_t) * NumKeys[1]), 
                                            BufferTransition(m_SrcKeyBuffers[0], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sizeof(uint32_t) * NumKeys[0]), 
        
                                        // Copy the data into the dst[0] buffers for use on first frame
                                            BufferTransition(m_DstKeyBuffers[0], VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]) ,
                                            BufferTransition(m_DstPayloadBuffers[0], VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]) };

    vkCmdPipelineBarrier(m_pUploadHeap->GetCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 6, Barriers, 0, nullptr);

    copyInfo.srcOffset = 0;
    copyInfo.size = sizeof(uint32_t) * NumKeys[m_UIResolutionSize];
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_SrcKeyBuffers[m_UIResolutionSize], m_DstKeyBuffers[0], 1, &copyInfo);
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_SrcPayloadBuffers, m_DstPayloadBuffers[0], 1, &copyInfo);

    // Put the dst buffers back to UAVs for sort usage
    Barriers[0] = BufferTransition(m_DstKeyBuffers[0], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    Barriers[1] = BufferTransition(m_DstPayloadBuffers[0], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    vkCmdPipelineBarrier(m_pUploadHeap->GetCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 2, Barriers, 0, nullptr);
}

// Compile specified radix sort shader and create pipeline
void FFXParallelSort::CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, VkPipeline& pPipeline)
{
    std::string CompileFlags("-T cs_6_0");
#ifdef _DEBUG
    CompileFlags += " -Zi -Od";
#endif // _DEBUG

    VkPipelineShaderStageCreateInfo stage_create_info = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

    VkResult vkResult = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_COMPUTE_BIT, shaderFile, entryPoint, "-T cs_6_0", defines, &stage_create_info);
    stage_create_info.flags = 0;
    assert(vkResult == VK_SUCCESS);

    VkComputePipelineCreateInfo create_info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    create_info.pNext = nullptr;
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = 0;
    create_info.flags = 0;
    create_info.layout = m_SortPipelineLayout;
    create_info.stage = stage_create_info;
    vkResult = vkCreateComputePipelines(m_pDevice->GetDevice(), VK_NULL_HANDLE, 1, &create_info, nullptr, &pPipeline);
    assert(vkResult == VK_SUCCESS);
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

    // Create resources to test with. Sorts will be done for 1080p, 2K, and 4K resolution data sets
    CreateKeyPayloadBuffers();

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; // | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.memoryTypeBits = 0;
    allocCreateInfo.pool = VK_NULL_HANDLE;
    allocCreateInfo.preferredFlags = 0;
    allocCreateInfo.requiredFlags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_UNKNOWN;

    // We are just going to fudge the indirect execution parameters for each resolution
    bufferCreateInfo.size = sizeof(uint32_t) * 3;
    allocCreateInfo.pUserData = "IndirectKeyCounts";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_IndirectKeyCounts, &m_IndirectKeyCountsAllocation, nullptr))
    {
        Trace("Failed to create buffer for IndirectKeyCounts");
    }

    VkBufferCopy copyInfo = { 0 };
    uint8_t* pNumKeysBuffer = m_pUploadHeap->Suballocate(sizeof(uint32_t) * 3, sizeof(uint32_t));
    memcpy(pNumKeysBuffer, NumKeys, sizeof(uint32_t) * 3);
    copyInfo.srcOffset = pNumKeysBuffer - m_pUploadHeap->BasePtr();
    copyInfo.size = sizeof(uint32_t) * 3;
    vkCmdCopyBuffer(m_pUploadHeap->GetCommandList(), m_pUploadHeap->GetResource(), m_IndirectKeyCounts, 1, &copyInfo);

    VkBufferMemoryBarrier barrier = BufferTransition(m_IndirectKeyCounts, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
    vkCmdPipelineBarrier(m_pUploadHeap->GetCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
        
    // Create resources for sort validation (image that goes from shuffled to sorted)
    m_Validate1080pTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate1080p.png", false,VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    m_Validate1080pTexture.CreateSRV(&m_ValidationImageViews[0], 0);
    m_Validate2KTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate2K.png", false, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    m_Validate2KTexture.CreateSRV(&m_ValidationImageViews[1], 0);
    m_Validate4KTexture.InitFromFile(m_pDevice, m_pUploadHeap, "Validate4K.png", false, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    m_Validate4KTexture.CreateSRV(&m_ValidationImageViews[2], 0);

    // Finish up
    m_pUploadHeap->FlushAndFinish();

    // Allocate the scratch buffers needed for radix sort
    FFX_ParallelSort_CalculateScratchResourceSize(NumKeys[2], m_ScratchBufferSize, m_ReducedScratchBufferSize);
        
    bufferCreateInfo.size = m_ScratchBufferSize;
    allocCreateInfo.pUserData = "Scratch";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_FPSScratchBuffer, &m_FPSScratchBufferAllocation, nullptr))
    {
        Trace("Failed to create buffer for Scratch");
    }
        
    bufferCreateInfo.size = m_ReducedScratchBufferSize;
    allocCreateInfo.pUserData = "ReducedScratch";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_FPSReducedScratchBuffer, &m_FPSReducedScratchBufferAllocation, nullptr))
    {
        Trace("Failed to create buffer for ReducedScratch");
    }
        
    // Allocate the buffers for indirect execution of the algorithm
        
    bufferCreateInfo.size = sizeof(uint32_t) * 3;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocCreateInfo.pUserData = "IndirectCount_Scatter_DispatchArgs";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_IndirectCountScatterArgs, &m_IndirectCountScatterArgsAllocation, nullptr))
    {
        Trace("Failed to create buffer for IndirectCount_Scatter_DispatchArgs");
    }
        
    allocCreateInfo.pUserData = "IndirectReduceScanArgs";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_IndirectReduceScanArgs, &m_IndirectReduceScanArgsAllocation, nullptr))
    {
        Trace("Failed to create buffer for IndirectCount_Scatter_DispatchArgs");
    }
        
    bufferCreateInfo.size = sizeof(FFX_ParallelSortCB);
    bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocCreateInfo.pUserData = "IndirectConstantBuffer";
    if (VK_SUCCESS != vmaCreateBuffer(m_pDevice->GetAllocator(), &bufferCreateInfo, &allocCreateInfo, &m_IndirectConstantBuffer, &m_IndirectConstantBufferAllocation, nullptr))
    {
        Trace("Failed to create buffer for IndirectConstantBuffer");
    }

    // Create Pipeline layout for Sort pass
    {
        // Create binding for Radix sort passes
        VkDescriptorSetLayoutBinding layout_bindings_set_0[] = {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }   // Constant buffer table
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_1[] = {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }   // Constant buffer to setup indirect params (indirect)
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_InputOutputs[] = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // SrcBuffer (sort)
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // DstBuffer (sort)
            { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // ScrPayload (sort only)
            { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // DstPayload (sort only)
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_Scan[] = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // ScanSrc
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // ScanDst
            { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // ScanScratch
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_Scratch[] = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // Scratch (sort only)
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // Scratch (reduced)
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_Indirect[] = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // NumKeys (indirect)
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // CBufferUAV (indirect)
            { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },  // CountScatterArgs (indirect)
            { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }   // ReduceScanArgs (indirect)
        };

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptor_set_layout_create_info.pNext = nullptr;
        descriptor_set_layout_create_info.flags = 0;
        descriptor_set_layout_create_info.pBindings = layout_bindings_set_0;
        descriptor_set_layout_create_info.bindingCount = 1;
        VkResult vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutConstants);
        assert(vkResult == VK_SUCCESS);
        bool bDescriptorAlloc = true;
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstants, &m_SortDescriptorSetConstants[0]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstants, &m_SortDescriptorSetConstants[1]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstants, &m_SortDescriptorSetConstants[2]);
        assert(bDescriptorAlloc == true);

        descriptor_set_layout_create_info.pBindings = layout_bindings_set_1;
        descriptor_set_layout_create_info.bindingCount = 1;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutConstantsIndirect);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstantsIndirect, &m_SortDescriptorSetConstantsIndirect[0]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstantsIndirect, &m_SortDescriptorSetConstantsIndirect[1]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutConstantsIndirect, &m_SortDescriptorSetConstantsIndirect[2]);
        assert(bDescriptorAlloc == true);

        descriptor_set_layout_create_info.pBindings = layout_bindings_set_InputOutputs;
        descriptor_set_layout_create_info.bindingCount = 4;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutInputOutputs);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutInputOutputs, &m_SortDescriptorSetInputOutput[0]);
        assert(bDescriptorAlloc == true);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutInputOutputs, &m_SortDescriptorSetInputOutput[1]);
        assert(bDescriptorAlloc == true);

        descriptor_set_layout_create_info.pBindings = layout_bindings_set_Scan;
        descriptor_set_layout_create_info.bindingCount = 3;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutScan);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutScan, &m_SortDescriptorSetScanSets[0]);
        assert(bDescriptorAlloc == true);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutScan, &m_SortDescriptorSetScanSets[1]);
        assert(bDescriptorAlloc == true);

        descriptor_set_layout_create_info.pBindings = layout_bindings_set_Scratch;
        descriptor_set_layout_create_info.bindingCount = 2;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutScratch);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutScratch, &m_SortDescriptorSetScratch);
        assert(bDescriptorAlloc == true);

        descriptor_set_layout_create_info.pBindings = layout_bindings_set_Indirect;
        descriptor_set_layout_create_info.bindingCount = 4;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_SortDescriptorSetLayoutIndirect);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_SortDescriptorSetLayoutIndirect, &m_SortDescriptorSetIndirect);
        assert(bDescriptorAlloc == true);

        // Create constant range representing our static constant
        VkPushConstantRange constant_range;
        constant_range.stageFlags = VK_SHADER_STAGE_ALL;
        constant_range.offset = 0;
        constant_range.size = 4;

        // Create the pipeline layout (Root signature)
        VkPipelineLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layout_create_info.pNext = nullptr;
        layout_create_info.flags = 0;
        layout_create_info.setLayoutCount = 6;
        VkDescriptorSetLayout layouts[] = { m_SortDescriptorSetLayoutConstants, m_SortDescriptorSetLayoutConstantsIndirect, m_SortDescriptorSetLayoutInputOutputs, 
                                            m_SortDescriptorSetLayoutScan, m_SortDescriptorSetLayoutScratch, m_SortDescriptorSetLayoutIndirect };
        layout_create_info.pSetLayouts = layouts;
        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = &constant_range;
        VkResult bCreatePipelineLayout = vkCreatePipelineLayout(m_pDevice->GetDevice(), &layout_create_info, nullptr, &m_SortPipelineLayout);
        assert(bCreatePipelineLayout == VK_SUCCESS);
    }

    // Create Pipeline layout for Render of RadixBuffer info
    {
        // Create binding for Radix sort passes
        VkDescriptorSetLayoutBinding layout_bindings_set_0[] = {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }   // Constant buffer table
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_1[] = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }   // Sort Buffer
        };

        VkDescriptorSetLayoutBinding layout_bindings_set_2[] = {
            { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr }    // ValidationTexture
        };

        // Create descriptor set layout and descriptor set
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        descriptor_set_layout_create_info.pNext = nullptr;
        descriptor_set_layout_create_info.flags = 0;
        descriptor_set_layout_create_info.bindingCount = 1;
        descriptor_set_layout_create_info.pBindings = layout_bindings_set_0;
        VkResult vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_RenderDescriptorSetLayout0);
        assert(vkResult == VK_SUCCESS);
        bool bDescriptorAlloc = m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout0, &m_RenderDescriptorSet0);
        assert(bDescriptorAlloc == true);
        descriptor_set_layout_create_info.pBindings = layout_bindings_set_1;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_RenderDescriptorSetLayout1);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout1, &m_RenderDescriptorSet1[0]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout1, &m_RenderDescriptorSet1[1]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout1, &m_RenderDescriptorSet1[2]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout1, &m_RenderDescriptorSet1[3]);
        assert(bDescriptorAlloc == true);
        descriptor_set_layout_create_info.pBindings = layout_bindings_set_2;
        vkResult = vkCreateDescriptorSetLayout(m_pDevice->GetDevice(), &descriptor_set_layout_create_info, nullptr, &m_RenderDescriptorSetLayout2);
        assert(vkResult == VK_SUCCESS);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout2, &m_RenderDescriptorSet2[0]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout2, &m_RenderDescriptorSet2[1]);
        bDescriptorAlloc &= m_pResourceViewHeaps->AllocDescriptor(m_RenderDescriptorSetLayout2, &m_RenderDescriptorSet2[2]);
        assert(bDescriptorAlloc == true);

        // Create the pipeline layout (Root signature)
        VkPipelineLayoutCreateInfo layout_create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layout_create_info.pNext = nullptr;
        layout_create_info.flags = 0;
        layout_create_info.setLayoutCount = 3;
        VkDescriptorSetLayout layouts[] = { m_RenderDescriptorSetLayout0, m_RenderDescriptorSetLayout1, m_RenderDescriptorSetLayout2 };
        layout_create_info.pSetLayouts = layouts;
        layout_create_info.pushConstantRangeCount = 0;
        layout_create_info.pPushConstantRanges = nullptr;
        VkResult bCreatePipelineLayout = vkCreatePipelineLayout(m_pDevice->GetDevice(), &layout_create_info, nullptr, &m_RenderPipelineLayout);
        assert(bCreatePipelineLayout == VK_SUCCESS);
    }
        
    //////////////////////////////////////////////////////////////////////////
    // Create pipelines for radix sort
    {
        // Create all of the necessary pipelines for Sort and Scan

        // SetupIndirectParams (indirect only)
        DefineList defines;
        defines["VK_Const"] = std::to_string(1);
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_SetupIndirectParameters", m_FPSIndirectSetupParametersPipeline);

        // Radix count (sum table generation)
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Count", m_FPSCountPipeline);
        // Radix count reduce (sum table reduction for offset prescan)
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_CountReduce", m_FPSCountReducePipeline);
        // Radix scan (prefix scan)
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scan", m_FPSScanPipeline);
        // Radix scan add (prefix scan + reduced prefix scan addition)
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_ScanAdd", m_FPSScanAddPipeline);
        // Radix scatter (key redistribution)
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scatter", m_FPSScatterPipeline);
        
        // Radix scatter with payload (key and payload redistribution)
        defines["kRS_ValueCopy"] = std::to_string(1);
        CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scatter", m_FPSScatterPayloadPipeline);
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

        // VS
        VkPipelineShaderStageCreateInfo stage_create_info_VS = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        VkResult vkResult = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_VERTEX_BIT, "ParallelSortVerify.hlsl", "FullscreenVS", CompileFlagsVS.c_str(), nullptr, &stage_create_info_VS);
        stage_create_info_VS.flags = 0;
        assert(vkResult == VK_SUCCESS);
        // PS
        VkPipelineShaderStageCreateInfo stage_create_info_PS = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vkResult = VKCompileFromFile(m_pDevice->GetDevice(), VK_SHADER_STAGE_FRAGMENT_BIT, "ParallelSortVerify.hlsl", "RenderSortValidationPS", CompileFlagsPS.c_str(), nullptr, &stage_create_info_PS);
        stage_create_info_PS.flags = 0;
        assert(vkResult == VK_SUCCESS);

        // Pipeline creation
        VkGraphicsPipelineCreateInfo create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        create_info.pNext = nullptr;
        create_info.flags = 0;
        create_info.stageCount = 2;
        VkPipelineShaderStageCreateInfo stages[] = { stage_create_info_VS, stage_create_info_PS };
        create_info.pStages = stages;

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = NULL;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 0;
        vi.pVertexBindingDescriptions = nullptr;
        vi.vertexAttributeDescriptionCount = 0;
        vi.pVertexAttributeDescriptions = nullptr;
        create_info.pVertexInputState = &vi;

        VkPipelineInputAssemblyStateCreateInfo ia;
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = NULL;
        ia.flags = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        create_info.pInputAssemblyState = &ia;
        create_info.pTessellationState = nullptr;

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext = NULL;
        vp.flags = 0;
        vp.viewportCount = 1;
        vp.scissorCount = 1;
        vp.pScissors = NULL;
        vp.pViewports = NULL;
        create_info.pViewportState = &vp;

        VkPipelineRasterizationStateCreateInfo rs;
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.pNext = NULL;
        rs.flags = 0;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.depthBiasEnable = VK_FALSE;
        rs.depthBiasConstantFactor = 0;
        rs.depthBiasClamp = 0;
        rs.depthBiasSlopeFactor = 0;
        rs.lineWidth = 1.0f;
        create_info.pRasterizationState = &rs;

        VkPipelineMultisampleStateCreateInfo ms;
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext = NULL;
        ms.flags = 0;
        ms.pSampleMask = NULL;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        ms.sampleShadingEnable = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;
        ms.minSampleShading = 0.0;
        create_info.pMultisampleState = &ms;

        VkPipelineDepthStencilStateCreateInfo ds;
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.pNext = NULL;
        ds.flags = 0;
        ds.depthTestEnable = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;
        ds.back.failOp = VK_STENCIL_OP_KEEP;
        ds.back.passOp = VK_STENCIL_OP_KEEP;
        ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.back.compareMask = 0;
        ds.back.reference = 0;
        ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
        ds.back.writeMask = 0;
        ds.minDepthBounds = 0;
        ds.maxDepthBounds = 0;
        ds.stencilTestEnable = VK_FALSE;
        ds.front = ds.back;
        create_info.pDepthStencilState = &ds;

        VkPipelineColorBlendAttachmentState att_state[1];
        att_state[0].colorWriteMask = 0xf;
        att_state[0].blendEnable = VK_FALSE;
        att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
        att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
        att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        VkPipelineColorBlendStateCreateInfo cb;
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.flags = 0;
        cb.pNext = NULL;
        cb.attachmentCount = 1;
        cb.pAttachments = att_state;
        cb.logicOpEnable = VK_FALSE;
        cb.logicOp = VK_LOGIC_OP_NO_OP;
        cb.blendConstants[0] = 1.0f;
        cb.blendConstants[1] = 1.0f;
        cb.blendConstants[2] = 1.0f;
        cb.blendConstants[3] = 1.0f;
        create_info.pColorBlendState = &cb;

        std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = NULL;
        dynamicState.pDynamicStates = dynamicStateEnables.data();
        dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
        create_info.pDynamicState = &dynamicState;

        create_info.layout = m_RenderPipelineLayout;
        create_info.renderPass = pSwapChain->GetRenderPass();
        create_info.subpass = 0;
        create_info.basePipelineHandle = VK_NULL_HANDLE;
        create_info.basePipelineIndex = 0;

        vkResult = vkCreateGraphicsPipelines(m_pDevice->GetDevice(), m_pDevice->GetPipelineCache(), 1, &create_info, NULL, &m_RenderResultVerificationPipeline);
        assert(vkResult == VK_SUCCESS);
    }

    // Do binding setups
    {
        VkBuffer BufferMaps[4];

        // Map inputs/outputs
        BufferMaps[0] = m_DstKeyBuffers[0];
        BufferMaps[1] = m_DstKeyBuffers[1];
        BufferMaps[2] = m_DstPayloadBuffers[0];
        BufferMaps[3] = m_DstPayloadBuffers[1];
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[0], 0, 4);

        BufferMaps[0] = m_DstKeyBuffers[1];
        BufferMaps[1] = m_DstKeyBuffers[0];
        BufferMaps[2] = m_DstPayloadBuffers[1];
        BufferMaps[3] = m_DstPayloadBuffers[0];
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[1], 0, 4);

        // Map scan sets (reduced, scratch)
        BufferMaps[0] = BufferMaps[1] = m_FPSReducedScratchBuffer;
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[0], 0, 2);

        BufferMaps[0] = BufferMaps[1] = m_FPSScratchBuffer;
        BufferMaps[2] = m_FPSReducedScratchBuffer;
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScanSets[1], 0, 3);

        // Map Scratch areas (fixed)
        BufferMaps[0] = m_FPSScratchBuffer;
        BufferMaps[1] = m_FPSReducedScratchBuffer;
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetScratch, 0, 2);

        // Map indirect buffers
        BufferMaps[0] = m_IndirectKeyCounts;
        BufferMaps[1] = m_IndirectConstantBuffer;
        BufferMaps[2] = m_IndirectCountScatterArgs;
        BufferMaps[3] = m_IndirectReduceScanArgs;
        BindUAVBuffer(BufferMaps, m_SortDescriptorSetIndirect, 0, 4);

        // Bind validation textures
        for (int i = 0; i < 3; ++i)
        {
            VkDescriptorImageInfo imageinfo;
            imageinfo.imageView = m_ValidationImageViews[i];
            imageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageinfo.sampler = VK_NULL_HANDLE;

            VkWriteDescriptorSet write_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write_set.pNext = nullptr;
            write_set.dstSet = m_RenderDescriptorSet2[i];
            write_set.dstBinding = 0;
            write_set.dstArrayElement = 0;
            write_set.descriptorCount = 1;
            write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write_set.pImageInfo = &imageinfo;
            write_set.pBufferInfo = nullptr;
            write_set.pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &write_set, 0, nullptr);
        }

        // Bind buffers from which we will pull the indices into the image buffer
        BindUAVBuffer(&m_SrcKeyBuffers[0], m_RenderDescriptorSet1[0]);
        BindUAVBuffer(&m_SrcKeyBuffers[1], m_RenderDescriptorSet1[1]);
        BindUAVBuffer(&m_SrcKeyBuffers[2], m_RenderDescriptorSet1[2]);
        BindUAVBuffer(&m_DstKeyBuffers[0], m_RenderDescriptorSet1[3]);
    }
}

// Parallel Sort termination
void FFXParallelSort::OnDestroy()
{
    // Release verification render resources
    vkDestroyPipelineLayout(m_pDevice->GetDevice(), m_RenderPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_RenderDescriptorSetLayout0, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet0);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_RenderDescriptorSetLayout1, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet1[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet1[1]);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet1[2]);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet1[3]);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_RenderDescriptorSetLayout2, nullptr);

    vkDestroyPipeline(m_pDevice->GetDevice(), m_RenderResultVerificationPipeline, nullptr);

    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet2[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet2[1]);
    m_pResourceViewHeaps->FreeDescriptor(m_RenderDescriptorSet2[2]);
    m_Validate4KTexture.OnDestroy();
    m_Validate2KTexture.OnDestroy();
    m_Validate1080pTexture.OnDestroy();
    vkDestroyImageView(m_pDevice->GetDevice(), m_ValidationImageViews[0], nullptr);
    vkDestroyImageView(m_pDevice->GetDevice(), m_ValidationImageViews[1], nullptr);
    vkDestroyImageView(m_pDevice->GetDevice(), m_ValidationImageViews[2], nullptr);

    // Release radix sort indirect resources
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_IndirectKeyCounts, m_IndirectKeyCountsAllocation);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_IndirectConstantBuffer, m_IndirectConstantBufferAllocation);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_IndirectCountScatterArgs, m_IndirectCountScatterArgsAllocation);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_IndirectReduceScanArgs, m_IndirectReduceScanArgsAllocation);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSIndirectSetupParametersPipeline, nullptr);

    // Release radix sort algorithm resources
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_FPSScratchBuffer, m_FPSScratchBufferAllocation);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_FPSReducedScratchBuffer, m_FPSReducedScratchBufferAllocation);

    vkDestroyPipelineLayout(m_pDevice->GetDevice(), m_SortPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutConstants, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstants[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstants[1]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstants[2]);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutConstantsIndirect, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstantsIndirect[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstantsIndirect[1]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetConstantsIndirect[2]);
    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutInputOutputs, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetInputOutput[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetInputOutput[1]);

    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutScan, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetScanSets[0]);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetScanSets[1]);

    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutScratch, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetScratch);

    vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_SortDescriptorSetLayoutIndirect, nullptr);
    m_pResourceViewHeaps->FreeDescriptor(m_SortDescriptorSetIndirect);

    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSCountPipeline, nullptr);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSCountReducePipeline, nullptr);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSScanPipeline, nullptr);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSScanAddPipeline, nullptr);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSScatterPipeline, nullptr);
    vkDestroyPipeline(m_pDevice->GetDevice(), m_FPSScatterPayloadPipeline, nullptr);

    // Release all of our resources
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_SrcKeyBuffers[0], m_SrcKeyBufferAllocations[0]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_SrcKeyBuffers[1], m_SrcKeyBufferAllocations[1]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_SrcKeyBuffers[2], m_SrcKeyBufferAllocations[2]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_SrcPayloadBuffers, m_SrcPayloadBufferAllocation);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_DstKeyBuffers[0], m_DstKeyBufferAllocations[0]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_DstKeyBuffers[1], m_DstKeyBufferAllocations[1]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_DstPayloadBuffers[0], m_DstPayloadBufferAllocations[0]);
    vmaDestroyBuffer(m_pDevice->GetAllocator(), m_DstPayloadBuffers[1], m_DstPayloadBufferAllocations[1]);
}

// Because we are sorting the data every frame, need to reset to unsorted version of data before running sort
void FFXParallelSort::CopySourceDataForFrame(VkCommandBuffer commandList)
{
    // Copy the contents the source buffer to the dstBuffer[0] each frame in order to not 
    // lose our original data
    VkBufferMemoryBarrier Barriers[2] = { 
        BufferTransition(m_DstKeyBuffers[0], VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]) ,
        BufferTransition(m_DstPayloadBuffers[0], VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize])
    };
    vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 2, Barriers, 0, nullptr);

    VkBufferCopy copyInfo = { 0 };
    copyInfo.srcOffset = 0;
    copyInfo.size = sizeof(uint32_t) * NumKeys[m_UIResolutionSize];
    vkCmdCopyBuffer(commandList, m_SrcKeyBuffers[m_UIResolutionSize], m_DstKeyBuffers[0], 1, &copyInfo);
    vkCmdCopyBuffer(commandList, m_SrcPayloadBuffers, m_DstPayloadBuffers[0], 1, &copyInfo);

    // Put the dst buffers back to UAVs for sort usage
    Barriers[0] = BufferTransition(m_DstKeyBuffers[0], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    Barriers[1] = BufferTransition(m_DstPayloadBuffers[0], VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
    vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 2, Barriers, 0, nullptr);
}

// Perform Parallel Sort (radix-based sort)
void FFXParallelSort::Sort(VkCommandBuffer commandList, bool isBenchmarking, float benchmarkTime)
{
    bool bIndirectDispatch = m_UIIndirectSort;

    // To control which descriptor set to use for updating data
    static uint32_t frameCount = 0;
    uint32_t frameConstants = (++frameCount) % 3;

    std::string markerText = "FFXParallelSort";
    if (bIndirectDispatch) markerText += " Indirect";
    SetPerfMarkerBegin(commandList, markerText.c_str());

    // Buffers to ping-pong between when writing out sorted values
    VkBuffer* ReadBufferInfo(&m_DstKeyBuffers[0]), * WriteBufferInfo(&m_DstKeyBuffers[1]);
    VkBuffer* ReadPayloadBufferInfo(&m_DstPayloadBuffers[0]), * WritePayloadBufferInfo(&m_DstPayloadBuffers[1]);
    bool bHasPayload = m_UISortPayload;

    // Setup barriers for the run
    VkBufferMemoryBarrier Barriers[3];
    FFX_ParallelSortCB  constantBufferData = { 0 };

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
        VkDescriptorBufferInfo constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(SetupIndirectCB), (void*)&IndirectSetupCB);
        BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstantsIndirect[frameConstants]);
            
        // Dispatch
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 1, 1, &m_SortDescriptorSetConstantsIndirect[frameConstants], 0, nullptr);
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 5, 1, &m_SortDescriptorSetIndirect, 0, nullptr);
        vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSIndirectSetupParametersPipeline);
        vkCmdDispatch(commandList, 1, 1, 1);
            
        // When done, transition the args buffers to INDIRECT_ARGUMENT, and the constant buffer UAV to Constant buffer
        VkBufferMemoryBarrier barriers[5];
        barriers[0] = BufferTransition(m_IndirectCountScatterArgs, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
        barriers[1] = BufferTransition(m_IndirectReduceScanArgs, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
        barriers[2] = BufferTransition(m_IndirectConstantBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, sizeof(FFX_ParallelSortCB));
        barriers[3] = BufferTransition(m_IndirectCountScatterArgs, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, sizeof(uint32_t) * 3);
        barriers[4] = BufferTransition(m_IndirectReduceScanArgs, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, sizeof(uint32_t) * 3);
        vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 5, barriers, 0, nullptr);
    }

    // Bind the scratch descriptor sets
    vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 4, 1, &m_SortDescriptorSetScratch, 0, nullptr);

    // Copy the data into the constant buffer and bind
    if (bIndirectDispatch)
    {
        //constantBuffer = m_IndirectConstantBuffer.GetResource()->GetGPUVirtualAddress();
        VkDescriptorBufferInfo constantBuffer;
        constantBuffer.buffer = m_IndirectConstantBuffer;
        constantBuffer.offset = 0;
        constantBuffer.range = VK_WHOLE_SIZE;
        BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstants[frameConstants]);
    }
    else
    {
        VkDescriptorBufferInfo constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(FFX_ParallelSortCB), (void*)&constantBufferData);
        BindConstantBuffer(constantBuffer, m_SortDescriptorSetConstants[frameConstants]);
    }
    // Bind constants
    vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 0, 1, &m_SortDescriptorSetConstants[frameConstants], 0, nullptr);
        
    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    uint32_t inputSet = 0;
    for (uint32_t Shift = 0; Shift < 32u; Shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
    {
        // Update the bit shift
        vkCmdPushConstants(commandList, m_SortPipelineLayout, VK_SHADER_STAGE_ALL, 0, 4, &Shift);

        // Bind input/output for this pass
        vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 2, 1, &m_SortDescriptorSetInputOutput[inputSet], 0, nullptr);

        // Sort Count
        {
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSCountPipeline);

            if (bIndirectDispatch)
                vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs, 0);                  
            else
                vkCmdDispatch(commandList, NumThreadgroupsToRun, 1, 1);
        }

        // UAV barrier on the sum table
        Barriers[0] = BufferTransition(m_FPSScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
        vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
            
        // Sort Reduce
        {
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSCountReducePipeline);
                
            if (bIndirectDispatch)
                vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs, 0);
            else
                vkCmdDispatch(commandList, NumReducedThreadgroupsToRun, 1, 1);
                    
            // UAV barrier on the reduced sum table
            Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
        }

        // Sort Scan
        {
            // First do scan prefix of reduced values
            vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[0], 0, nullptr);
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSScanPipeline);

            if (!bIndirectDispatch)
            {
                assert(NumReducedThreadgroupsToRun < FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE && "Need to account for bigger reduced histogram scan");
            }
            vkCmdDispatch(commandList, 1, 1, 1);

            // UAV barrier on the reduced sum table
            Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
            vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
                
            // Next do scan prefix on the histogram with partial sums that we just did
            vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[1], 0, nullptr);
                
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSScanAddPipeline);
            if (bIndirectDispatch)
                vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs, 0);
            else
                vkCmdDispatch(commandList, NumReducedThreadgroupsToRun, 1, 1);
        }

        // UAV barrier on the sum table
        Barriers[0] = BufferTransition(m_FPSScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
        vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
            
        // Sort Scatter
        {
            vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, bHasPayload ? m_FPSScatterPayloadPipeline : m_FPSScatterPipeline);

            if (bIndirectDispatch)
                vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs, 0);
            else
                vkCmdDispatch(commandList, NumThreadgroupsToRun, 1, 1);
        }
            
        // Finish doing everything and barrier for the next pass
        int numBarriers = 0;
        Barriers[numBarriers++] = BufferTransition(*WriteBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]);
        if (bHasPayload)
            Barriers[numBarriers++] = BufferTransition(*WritePayloadBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[2]);
        vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, numBarriers, Barriers, 0, nullptr);
            
        // Swap read/write sources
        std::swap(ReadBufferInfo, WriteBufferInfo);
        if (bHasPayload)
            std::swap(ReadPayloadBufferInfo, WritePayloadBufferInfo);
        inputSet = !inputSet;
    }

    // When we are all done, transition indirect buffers back to UAV for the next frame (if doing indirect dispatch)
    if (bIndirectDispatch)
    {
        VkBufferMemoryBarrier barriers[3];
        barriers[0] = BufferTransition(m_IndirectConstantBuffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(FFX_ParallelSortCB));
        barriers[1] = BufferTransition(m_IndirectCountScatterArgs, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
        barriers[2] = BufferTransition(m_IndirectReduceScanArgs, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * 3);
        vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 3, barriers, 0, nullptr);
    }

    // Close out the perf capture
    SetPerfMarkerEnd(commandList);
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

        ImGui::RadioButton("Render Unsorted Keys", &m_UIVisualOutput, 0);
        ImGui::RadioButton("Render Sorted Keys", &m_UIVisualOutput, 1);
    }
}

// Renders the image with the sorted/unsorted indicies for visual representation
void FFXParallelSort::DrawVisualization(VkCommandBuffer commandList, uint32_t RTWidth, uint32_t RTHeight)
{
    // Setup the constant buffer
    ParallelSortRenderCB ConstantBuffer;
    ConstantBuffer.Width = RTWidth;
    ConstantBuffer.Height = RTHeight;
    static const uint32_t SortWidths[] = { 1920, 2560, 3840 };
    static const uint32_t SortHeights[] = { 1080, 1440, 2160 };
    ConstantBuffer.SortWidth = SortWidths[m_UIResolutionSize];
    ConstantBuffer.SortHeight = SortHeights[m_UIResolutionSize];

    // Bind constant buffer
    VkDescriptorBufferInfo GPUCB = m_pConstantBufferRing->AllocConstantBuffer(sizeof(ParallelSortRenderCB), (void*)&ConstantBuffer);
    BindConstantBuffer(GPUCB, m_RenderDescriptorSet0);
        
    // If we are showing unsorted values, need to transition the source data buffer from copy source to UAV and back
    int descriptorIndex = 0;
    if (!m_UIVisualOutput)
    {
        VkBufferMemoryBarrier Barrier = BufferTransition(m_SrcKeyBuffers[m_UIResolutionSize], VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
        vkCmdPipelineBarrier(m_pUploadHeap->GetCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);
        descriptorIndex = m_UIResolutionSize;
    }
    else
        descriptorIndex = 3;

    // Bind buffer from which we will pull the indices into the image buffer        
    VkDescriptorSet descriptorSets[] = { m_RenderDescriptorSet0, m_RenderDescriptorSet1[descriptorIndex], m_RenderDescriptorSet2[m_UIResolutionSize] };

    // Bind pipeline layout and descriptor sets
    vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RenderResultVerificationPipeline); 
    vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_GRAPHICS, m_RenderPipelineLayout, 0, 3, descriptorSets, 0, nullptr);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = (float)RTHeight;
    viewport.width = (float)RTWidth;
    viewport.height = -(float)(RTHeight);
    viewport.minDepth = (float)0.0f;
    viewport.maxDepth = (float)1.0f;

    // Create scissor rectangle
    VkRect2D scissor;
    scissor.extent.width = RTWidth;
    scissor.extent.height = RTHeight;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    // Draw
    vkCmdSetViewport(commandList, 0, 1, &viewport);
    vkCmdSetScissor(commandList, 0, 1, &scissor);
    vkCmdDraw(commandList, 3, 1, 0, 0);

    // If we are showing unsorted values, need to transition the source data buffer from copy source to UAV and back
    if (!m_UIVisualOutput)
    {
        VkBufferMemoryBarrier Barrier = BufferTransition(m_SrcKeyBuffers[m_UIResolutionSize], VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, sizeof(uint32_t) * NumKeys[m_UIResolutionSize]);
        vkCmdPipelineBarrier(m_pUploadHeap->GetCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);
    }
}