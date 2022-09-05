// ParallelSort.cpp
// 
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define FFX_CPP
#include "ParallelSort.h"
#include "../../FFX-ParallelSort/FFX_ParallelSort.h"

static const uint32_t NumKeys = { 400*1024 };

//////////////////////////////////////////////////////////////////////////
// Helper for Vulkan
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

void FFXParallelSort::OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, Buffer* elementCount, Buffer* listA, Buffer* listB, Buffer* listA2, Buffer* listB2)
{
	m_pDevice = pDevice;
	m_pUploadHeap = pUploadHeap;
	m_pResourceViewHeaps = pResourceViewHeaps;
	m_pConstantBufferRing = pConstantBufferRing;
    m_SrcKeyBuffer = listA;
    m_SrcPayloadBuffer = listB;
    m_DstKeyBuffer = listA2;
    m_DstPayloadBuffer = listB2;

	m_MaxNumThreadgroups = 800;	

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

	// Allocate the scratch buffers needed for radix sort
	FFX_ParallelSort_CalculateScratchResourceSize(NumKeys, m_ScratchBufferSize, m_ReducedScratchBufferSize);
		
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
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }	// Constant buffer table
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_1[] = {
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }	// Constant buffer to setup indirect params (indirect)
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_InputOutputs[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// SrcBuffer (sort)
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// DstBuffer (sort)
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// ScrPayload (sort only)
			{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// DstPayload (sort only)
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_Scan[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// ScanSrc
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// ScanDst
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// ScanScratch
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_Scratch[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// Scratch (sort only)
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// Scratch (reduced)
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_Indirect[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// NumKeys (indirect)
			{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// CBufferUAV (indirect)
			{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr },	// CountScatterArgs (indirect)
			{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }	// ReduceScanArgs (indirect)
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
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }	// Constant buffer table
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_1[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr }	// Sort Buffer
		};

		VkDescriptorSetLayoutBinding layout_bindings_set_2[] = {
			{ 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr }	// ValidationTexture
		};
	}
		
	//////////////////////////////////////////////////////////////////////////
	// Create pipelines for radix sort
	{
		// Create all of the necessary pipelines for Sort and Scan
		DefineList defines;
		defines[ "API_VULKAN" ] = "";

		// SetupIndirectParams (indirect only)
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

	// Do binding setups
	{
		VkBuffer BufferMaps[4];

		// Map inputs/outputs
		BufferMaps[0] = m_SrcKeyBuffer->Resource();
		BufferMaps[1] = m_DstKeyBuffer->Resource();
		BufferMaps[2] = m_SrcPayloadBuffer->Resource();
		BufferMaps[3] = m_DstPayloadBuffer->Resource();
		BindUAVBuffer(BufferMaps, m_SortDescriptorSetInputOutput[0], 0, 4);

		BufferMaps[0] = m_DstKeyBuffer->Resource();
		BufferMaps[1] = m_SrcKeyBuffer->Resource();
		BufferMaps[2] = m_DstPayloadBuffer->Resource();
		BufferMaps[3] = m_SrcPayloadBuffer->Resource();
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
        elementCount->SetDescriptorSet( 0, m_SortDescriptorSetIndirect, false );
		BufferMaps[0] = m_IndirectConstantBuffer;
		BufferMaps[1] = m_IndirectCountScatterArgs;
		BufferMaps[2] = m_IndirectReduceScanArgs;
		BindUAVBuffer(BufferMaps, m_SortDescriptorSetIndirect, 1, 3);
	}
}

void FFXParallelSort::OnDestroy()
{
	// Release radix sort indirect resources
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
}


void FFXParallelSort::Draw(VkCommandBuffer commandList)
{
	// To control which descriptor set to use for updating data
	static uint32_t frameCount = 0;
	uint32_t frameConstants = (++frameCount) % 3;

	std::string markerText = "FFXParallelSortIndirect";
	SetPerfMarkerBegin(commandList, markerText.c_str());

	// Buffers to ping-pong between when writing out sorted values
	VkBuffer* ReadBufferInfo = &m_SrcKeyBuffer->Resource();
    VkBuffer* WriteBufferInfo(&m_DstKeyBuffer->Resource());
	VkBuffer* ReadPayloadBufferInfo(&m_SrcPayloadBuffer->Resource()), * WritePayloadBufferInfo(&m_DstPayloadBuffer->Resource());
	bool bHasPayload = true;

	// Setup barriers for the run
	VkBufferMemoryBarrier Barriers[3];

	// Fill in the constant buffer data structure (this will be done by a shader in the indirect version)
	{
		struct SetupIndirectCB
		{
			uint32_t MaxThreadGroups;
		};
		SetupIndirectCB IndirectSetupCB;
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
	{
		//constantBuffer = m_IndirectConstantBuffer.GetResource()->GetGPUVirtualAddress();
		VkDescriptorBufferInfo constantBuffer;
		constantBuffer.buffer = m_IndirectConstantBuffer;
		constantBuffer.offset = 0;
		constantBuffer.range = VK_WHOLE_SIZE;
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

			vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs, 0);					
		}

		// UAV barrier on the sum table
		Barriers[0] = BufferTransition(m_FPSScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
		vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
			
		// Sort Reduce
		{
			vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSCountReducePipeline);
				
			vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs, 0);

			// UAV barrier on the reduced sum table
			Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
			vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
		}

		// Sort Scan
		{
			// First do scan prefix of reduced values
			vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[0], 0, nullptr);
			vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSScanPipeline);

			vkCmdDispatch(commandList, 1, 1, 1);

			// UAV barrier on the reduced sum table
			Barriers[0] = BufferTransition(m_FPSReducedScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ReducedScratchBufferSize);
			vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
				
			// Next do scan prefix on the histogram with partial sums that we just did
			vkCmdBindDescriptorSets(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_SortPipelineLayout, 3, 1, &m_SortDescriptorSetScanSets[1], 0, nullptr);
				
			vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, m_FPSScanAddPipeline);
			vkCmdDispatchIndirect(commandList, m_IndirectReduceScanArgs, 0);
		}

		// UAV barrier on the sum table
		Barriers[0] = BufferTransition(m_FPSScratchBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, m_ScratchBufferSize);
		vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, Barriers, 0, nullptr);
			
		// Sort Scatter
		{
			vkCmdBindPipeline(commandList, VK_PIPELINE_BIND_POINT_COMPUTE, bHasPayload ? m_FPSScatterPayloadPipeline : m_FPSScatterPipeline);

			vkCmdDispatchIndirect(commandList, m_IndirectCountScatterArgs, 0);
		}
			
		// Finish doing everything and barrier for the next pass
		int numBarriers = 0;
		Barriers[numBarriers++] = BufferTransition(*WriteBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys);
		if (bHasPayload)
			Barriers[numBarriers++] = BufferTransition(*WritePayloadBufferInfo, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, sizeof(uint32_t) * NumKeys);
		vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, numBarriers, Barriers, 0, nullptr);
			
		// Swap read/write sources
		std::swap(ReadBufferInfo, WriteBufferInfo);
		if (bHasPayload)
			std::swap(ReadPayloadBufferInfo, WritePayloadBufferInfo);
		inputSet = !inputSet;
	}

	// When we are all done, transition indirect buffers back to UAV for the next frame (if doing indirect dispatch)
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
