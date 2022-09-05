// ParallelSort.h
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

#pragma once
#include "../vk/stdafx.h"
#include "BufferHelper.h"


struct ParallelSortRenderCB	// If you change this, also change struct ParallelSortRenderCB in ParallelSortVerify.hlsl
{
	int32_t Width;
	int32_t Height;
	int32_t SortWidth;
	int32_t SortHeight;
};


class FFXParallelSort
{
public:
	void OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, Buffer* elementCount, Buffer* listA, Buffer* listB, Buffer* listA2, Buffer* listB2);
	void OnDestroy();

	void Draw(VkCommandBuffer commandList);

private:
	void CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, VkPipeline& pPipeline);
	void BindConstantBuffer(VkDescriptorBufferInfo& GPUCB, VkDescriptorSet& DescriptorSet, uint32_t Binding = 0, uint32_t Count = 1);
	void BindUAVBuffer(VkBuffer* pBuffer, VkDescriptorSet& DescriptorSet, uint32_t Binding = 0, uint32_t Count = 1);


	Device*					m_pDevice = nullptr;
	UploadHeap*				m_pUploadHeap = nullptr;
	ResourceViewHeaps*		m_pResourceViewHeaps = nullptr;
	DynamicBufferRing*		m_pConstantBufferRing = nullptr;
	uint32_t				m_MaxNumThreadgroups = 800;

	uint32_t				m_ScratchBufferSize;
	uint32_t				m_ReducedScratchBufferSize;
	
    Buffer*         m_SrcKeyBuffer = nullptr;
    Buffer*         m_SrcPayloadBuffer = nullptr;

    Buffer*         m_DstKeyBuffer = nullptr;
    Buffer*         m_DstPayloadBuffer = nullptr;

    VkBuffer		m_FPSScratchBuffer;				// Sort scratch buffer
	VmaAllocation   m_FPSScratchBufferAllocation;

	VkBuffer		m_FPSReducedScratchBuffer;		// Sort reduced scratch buffer
	VmaAllocation   m_FPSReducedScratchBufferAllocation;

	VkDescriptorSetLayout	m_SortDescriptorSetLayoutConstants;
	VkDescriptorSet			m_SortDescriptorSetConstants[3];
	VkDescriptorSetLayout	m_SortDescriptorSetLayoutConstantsIndirect;
	VkDescriptorSet			m_SortDescriptorSetConstantsIndirect[3];

	VkDescriptorSetLayout	m_SortDescriptorSetLayoutInputOutputs;
	VkDescriptorSetLayout	m_SortDescriptorSetLayoutScan;
	VkDescriptorSetLayout	m_SortDescriptorSetLayoutScratch;
	VkDescriptorSetLayout	m_SortDescriptorSetLayoutIndirect;

	VkDescriptorSet			m_SortDescriptorSetInputOutput[2];
	VkDescriptorSet			m_SortDescriptorSetScanSets[2];
	VkDescriptorSet			m_SortDescriptorSetScratch;
	VkDescriptorSet			m_SortDescriptorSetIndirect;
	VkPipelineLayout		m_SortPipelineLayout;

	VkPipeline m_FPSCountPipeline;
	VkPipeline m_FPSCountReducePipeline;
	VkPipeline m_FPSScanPipeline;
	VkPipeline m_FPSScanAddPipeline;
	VkPipeline m_FPSScatterPipeline;
	VkPipeline m_FPSScatterPayloadPipeline;

	// Resources for indirect execution of algorithm
	VkBuffer		m_IndirectConstantBuffer;		// Buffer to hold radix sort constant buffer data for indirect dispatch
	VmaAllocation   m_IndirectConstantBufferAllocation;
	VkBuffer		m_IndirectCountScatterArgs;		// Buffer to hold dispatch arguments used for Count/Scatter parts of the algorithm
	VmaAllocation   m_IndirectCountScatterArgsAllocation;
	VkBuffer		m_IndirectReduceScanArgs;		// Buffer to hold dispatch arguments used for Reduce/Scan parts of the algorithm
	VmaAllocation   m_IndirectReduceScanArgsAllocation;
		
	VkPipeline					m_FPSIndirectSetupParametersPipeline;
};