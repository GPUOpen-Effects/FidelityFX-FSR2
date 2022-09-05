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
#include "../DX12/stdafx.h"

#define SORT_BITS_PER_PASS			4
#define SORT_BIN_COUNT				(1 << SORT_BITS_PER_PASS)
#define THREADGROUP_SIZE			64
#define ELEMENTS_PER_THREAD			4	// (256 / THREADGROUP_SIZE)
#define ITEMS_PER_WI 16
#define INV_ITEMS_PER_WI 1/16

struct ParallelSortRenderCB	// If you change this, also change struct ParallelSortRenderCB in ParallelSortVerify.hlsl
{
	int32_t Width;
	int32_t Height;
	int32_t SortWidth;
	int32_t SortHeight;
};

// Convenience struct for passing resource/UAV pairs around
typedef struct RdxDX12ResourceInfo
{
	ID3D12Resource* pResource;			///< Pointer to the resource -- used for barriers and syncs (must NOT be nullptr)
	D3D12_GPU_DESCRIPTOR_HANDLE	resourceGPUHandle;	///< The GPU Descriptor Handle to use for binding the resource
} RdxDX12ResourceInfo;

class FFXParallelSort
{
public:
	void OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, Texture* elementCount, Texture* listA, Texture* listB);
	void OnDestroy();

	void Draw(ID3D12GraphicsCommandList* pCommandList);

private:

	void CompileRadixPipeline(const char* shaderFile, const DefineList* defines, const char* entryPoint, ID3D12PipelineState*& pPipeline);

	Device*					m_pDevice = nullptr;
	UploadHeap*				m_pUploadHeap = nullptr;
	ResourceViewHeaps*		m_pResourceViewHeaps = nullptr;
	DynamicBufferRing*		m_pConstantBufferRing = nullptr;
	uint32_t				m_MaxNumThreadgroups = 320;	// Use a generic thread group size when not on AMD hardware (taken from experiments to determine best performance threshold)
	
	// Sample resources
    Texture*    m_SrcKeyBuffer = nullptr;
    Texture*    m_SrcPayloadBuffer = nullptr;
	CBV_SRV_UAV	m_ElementCountSRV;
    CBV_SRV_UAV	m_SrcKeyUAV;		// 32 bit source key UAVs
	CBV_SRV_UAV	m_SrcPayloadUAV;		// 32 bit source payload UAVs

    Texture     m_DstKeyTempBuffer[ 2 ];
    CBV_SRV_UAV m_DstKeyUAVTable;		// 32 bit destination key UAVs

	Texture	    m_DstPayloadTempBuffer[ 2 ];
	CBV_SRV_UAV	m_DstPayloadUAVTable;	// 32 bit destination payload UAVs

	// Resources for parallel sort algorithm
	Texture		m_FPSScratchBuffer;				// Sort scratch buffer
	CBV_SRV_UAV	m_FPSScratchUAV;				// UAV needed for sort scratch buffer
	Texture		m_FPSReducedScratchBuffer;		// Sort reduced scratch buffer
	CBV_SRV_UAV	m_FPSReducedScratchUAV;			// UAV needed for sort reduced scratch buffer
		
	ID3D12RootSignature* m_pFPSRootSignature			= nullptr;
	ID3D12PipelineState* m_pFPSCountPipeline			= nullptr;
	ID3D12PipelineState* m_pFPSCountReducePipeline		= nullptr;
	ID3D12PipelineState* m_pFPSScanPipeline				= nullptr;
	ID3D12PipelineState* m_pFPSScanAddPipeline			= nullptr;
	ID3D12PipelineState* m_pFPSScatterPipeline			= nullptr;
	ID3D12PipelineState* m_pFPSScatterPayloadPipeline	= nullptr;
		
	// Resources for indirect execution of algorithm
	Texture		m_IndirectKeyCounts;			// Buffer to hold num keys for indirect dispatch
	CBV_SRV_UAV	m_IndirectKeyCountsUAV;			// UAV needed for num keys buffer
	Texture		m_IndirectConstantBuffer;		// Buffer to hold radix sort constant buffer data for indirect dispatch
	CBV_SRV_UAV	m_IndirectConstantBufferUAV;	// UAV needed for indirect constant buffer
	Texture		m_IndirectCountScatterArgs;		// Buffer to hold dispatch arguments used for Count/Scatter parts of the algorithm
	CBV_SRV_UAV	m_IndirectCountScatterArgsUAV;	// UAV needed for count/scatter args buffer
	Texture		m_IndirectReduceScanArgs;		// Buffer to hold dispatch arguments used for Reduce/Scan parts of the algorithm
	CBV_SRV_UAV	m_IndirectReduceScanArgsUAV;	// UAV needed for reduce/scan args buffer
		
	ID3D12CommandSignature* m_pFPSCommandSignature;
	ID3D12PipelineState*	m_pFPSIndirectSetupParametersPipeline = nullptr;
};