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
#include "../FFX-ParallelSort/FFX_ParallelSort.h"

static const uint32_t NumKeys = { 400*1024 };


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

void FFXParallelSort::OnCreate(Device* pDevice, ResourceViewHeaps* pResourceViewHeaps, DynamicBufferRing* pConstantBufferRing, UploadHeap* pUploadHeap, Texture* elementCount, Texture* listA, Texture* listB)
{
	m_pDevice = pDevice;
	m_pUploadHeap = pUploadHeap;
	m_pResourceViewHeaps = pResourceViewHeaps;
	m_pConstantBufferRing = pConstantBufferRing;
    m_SrcKeyBuffer = listA;
    m_SrcPayloadBuffer = listB;
    m_MaxNumThreadgroups = 800;

    // Allocate UAVs to use for data
    m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_ElementCountSRV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_SrcKeyUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_SrcPayloadUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_DstKeyUAVTable);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(2, &m_DstPayloadUAVTable);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_FPSScratchUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_FPSReducedScratchUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectKeyCountsUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectConstantBufferUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectCountScatterArgsUAV);
	m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_IndirectReduceScanArgsUAV);

	// The DstKey and DstPayload buffers will be used as src/dst when sorting. A copy of the 
	// source key/payload will be copied into them before hand so we can keep our original values
	CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * NumKeys, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_DstKeyTempBuffer[0].InitBuffer(m_pDevice, "DstKeyTempBuf0", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
    m_DstKeyTempBuffer[1].InitBuffer(m_pDevice, "DstKeyTempBuf1", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_DstPayloadTempBuffer[0].InitBuffer(m_pDevice, "DstPayloadTempBuf0", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
    m_DstPayloadTempBuffer[1].InitBuffer(m_pDevice, "DstPayloadTempBuf1", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	{
		CD3DX12_RESOURCE_BARRIER Barriers[4] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyTempBuffer[0].GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_DstKeyTempBuffer[1].GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadTempBuffer[0].GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_DstPayloadTempBuffer[1].GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		m_pUploadHeap->GetCommandList()->ResourceBarrier(4, Barriers);
	}

	// Create UAVs
	listA->CreateBufferUAV(0, nullptr, &m_SrcKeyUAV);
	listB->CreateBufferUAV(0, nullptr, &m_SrcPayloadUAV);
    m_DstKeyTempBuffer[0].CreateBufferUAV(0, nullptr, &m_DstKeyUAVTable);
    m_DstPayloadTempBuffer[0].CreateBufferUAV(0, nullptr, &m_DstPayloadUAVTable);
	m_DstKeyTempBuffer[1].CreateBufferUAV(1, nullptr, &m_DstKeyUAVTable);
	m_DstPayloadTempBuffer[1].CreateBufferUAV(1, nullptr, &m_DstPayloadUAVTable);

    elementCount->CreateSRV( 0, &m_ElementCountSRV, 0 );

	// We are just going to fudge the indirect execution parameters for each resolution
	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_IndirectKeyCounts.InitBuffer(m_pDevice, "IndirectKeyCounts", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_IndirectKeyCounts.CreateBufferUAV(0, nullptr, &m_IndirectKeyCountsUAV);
	uint8_t* pNumKeysBuffer = m_pUploadHeap->Suballocate(sizeof(uint32_t), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	memcpy(pNumKeysBuffer, &NumKeys, sizeof(uint32_t) );
	m_pUploadHeap->GetCommandList()->CopyBufferRegion(m_IndirectKeyCounts.GetResource(), 0, m_pUploadHeap->GetResource(), pNumKeysBuffer - m_pUploadHeap->BasePtr(), sizeof(uint32_t));
	CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectKeyCounts.GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_pUploadHeap->GetCommandList()->ResourceBarrier(1, &Barrier);

	// Allocate the scratch buffers needed for radix sort
	uint32_t scratchBufferSize;
	uint32_t reducedScratchBufferSize;
	FFX_ParallelSort_CalculateScratchResourceSize(NumKeys, scratchBufferSize, reducedScratchBufferSize);
		
	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(scratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_FPSScratchBuffer.InitBuffer(m_pDevice, "Scratch", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_FPSScratchBuffer.CreateBufferUAV(0, nullptr, &m_FPSScratchUAV);

	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(reducedScratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_FPSReducedScratchBuffer.InitBuffer(m_pDevice, "ReducedScratch", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_FPSReducedScratchBuffer.CreateBufferUAV(0, nullptr, &m_FPSReducedScratchUAV);

	// Allocate the buffers for indirect execution of the algorithm
	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(FFX_ParallelSortCB), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_IndirectConstantBuffer.InitBuffer(m_pDevice, "IndirectConstantBuffer", &ResourceDesc, sizeof(FFX_ParallelSortCB), D3D12_RESOURCE_STATE_COMMON);
	m_IndirectConstantBuffer.CreateBufferUAV(0, nullptr, &m_IndirectConstantBufferUAV);

	ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * 3, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	m_IndirectCountScatterArgs.InitBuffer(m_pDevice, "IndirectCount_Scatter_DispatchArgs", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_IndirectCountScatterArgs.CreateBufferUAV(0, nullptr, &m_IndirectCountScatterArgsUAV);
	m_IndirectReduceScanArgs.InitBuffer(m_pDevice, "IndirectCount_Scatter_DispatchArgs", &ResourceDesc, sizeof(uint32_t), D3D12_RESOURCE_STATE_COMMON);
	m_IndirectReduceScanArgs.CreateBufferUAV(0, nullptr, &m_IndirectReduceScanArgsUAV);

	{
		CD3DX12_RESOURCE_BARRIER Barriers[5] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_FPSScratchBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_FPSReducedScratchBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectConstantBuffer.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectCountScatterArgs.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(m_IndirectReduceScanArgs.GetResource(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		m_pUploadHeap->GetCommandList()->ResourceBarrier(5, Barriers);
	}
	// Create root signature for Radix sort passes
	{
		D3D12_DESCRIPTOR_RANGE descRange[16];
		D3D12_ROOT_PARAMETER rootParams[17];

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

        descRange[15] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
		rootParams[16].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rootParams[16].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[16].DescriptorTable = { 1, &descRange[15] };

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.NumParameters = 17;
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

	//////////////////////////////////////////////////////////////////////////
	// Create pipelines for radix sort
	{
		// Create all of the necessary pipelines for Sort and Scan
		DefineList defines;
			
		// SetupIndirectParams (indirect only)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_SetupIndirectParameters", m_pFPSIndirectSetupParametersPipeline);

		// Radix count (sum table generation)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Count", m_pFPSCountPipeline);
		// Radix count reduce (sum table reduction for offset prescan)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_CountReduce", m_pFPSCountReducePipeline);
		// Radix scan (prefix scan)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scan", m_pFPSScanPipeline);
		// Radix scan add (prefix scan + reduced prefix scan addition)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_ScanAdd", m_pFPSScanAddPipeline);
		// Radix scatter (key redistribution)
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scatter", m_pFPSScatterPipeline);
		// Radix scatter with payload (key and payload redistribution)
		defines["kRS_ValueCopy"] = std::to_string(1);
		CompileRadixPipeline("ParallelSortCS.hlsl", &defines, "FPS_Scatter", m_pFPSScatterPayloadPipeline);
	}
}

void FFXParallelSort::OnDestroy()
{
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
	m_DstKeyTempBuffer[0].OnDestroy();
    m_DstKeyTempBuffer[1].OnDestroy();
	m_DstPayloadTempBuffer[0].OnDestroy();
    m_DstPayloadTempBuffer[1].OnDestroy();
}


void FFXParallelSort::Draw(ID3D12GraphicsCommandList* pCommandList)
{
	bool bIndirectDispatch = true;

	std::string markerText = "FFXParallelSort";
	if (bIndirectDispatch) markerText += " Indirect";
	UserMarker marker(pCommandList, markerText.c_str());

	FFX_ParallelSortCB	constantBufferData = { 0 };

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
		uint32_t NumberOfKeys = NumKeys;
		FFX_ParallelSort_SetConstantAndDispatchData(NumberOfKeys, m_MaxNumThreadgroups, constantBufferData, NumThreadgroupsToRun, NumReducedThreadgroupsToRun);
	}
	else
	{
		struct SetupIndirectCB
		{
			uint32_t MaxThreadGroups;
		};
		SetupIndirectCB IndirectSetupCB;
		IndirectSetupCB.MaxThreadGroups = m_MaxNumThreadgroups;
			
		// Copy the data into the constant buffer
		D3D12_GPU_VIRTUAL_ADDRESS constantBuffer = m_pConstantBufferRing->AllocConstantBuffer(sizeof(SetupIndirectCB), &IndirectSetupCB);
		pCommandList->SetComputeRootConstantBufferView(1, constantBuffer);	// SetupIndirect Constant buffer

		// Bind other buffer
		pCommandList->SetComputeRootDescriptorTable(12, m_IndirectKeyCountsUAV.GetGPU());			// Key counts
		pCommandList->SetComputeRootDescriptorTable(13, m_IndirectConstantBufferUAV.GetGPU());		// Indirect Sort Constant Buffer
		pCommandList->SetComputeRootDescriptorTable(14, m_IndirectCountScatterArgsUAV.GetGPU());	// Indirect Sort Count/Scatter Args
		pCommandList->SetComputeRootDescriptorTable(15, m_IndirectReduceScanArgsUAV.GetGPU());		// Indirect Sort Reduce/Scan Args
        pCommandList->SetComputeRootDescriptorTable(16, m_ElementCountSRV.GetGPU());		        // Indirect Sort Reduce/Scan Args
			
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
	RdxDX12ResourceInfo KeySrcInfo = { m_SrcKeyBuffer->GetResource(), m_SrcKeyUAV.GetGPU(0) };
	RdxDX12ResourceInfo PayloadSrcInfo = { m_SrcPayloadBuffer->GetResource(), m_SrcPayloadUAV.GetGPU(0) };
	RdxDX12ResourceInfo KeyTmpInfo = { m_DstKeyTempBuffer[1].GetResource(), m_DstKeyUAVTable.GetGPU(1) };
	RdxDX12ResourceInfo PayloadTmpInfo = { m_DstPayloadTempBuffer[1].GetResource(), m_DstPayloadUAVTable.GetGPU(1) };
	RdxDX12ResourceInfo ScratchBufferInfo = { m_FPSScratchBuffer.GetResource(), m_FPSScratchUAV.GetGPU() };
	RdxDX12ResourceInfo ReducedScratchBufferInfo = { m_FPSReducedScratchBuffer.GetResource(), m_FPSReducedScratchUAV.GetGPU() };

	// Buffers to ping-pong between when writing out sorted values
	const RdxDX12ResourceInfo* ReadBufferInfo(&KeySrcInfo), * WriteBufferInfo(&KeyTmpInfo);
	const RdxDX12ResourceInfo* ReadPayloadBufferInfo(&PayloadSrcInfo), * WritePayloadBufferInfo(&PayloadTmpInfo);
	bool bHasPayload = true;

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
		pCommandList->SetComputeRootConstantBufferView(0, constantBuffer);						// Constant buffer
		pCommandList->SetComputeRootDescriptorTable(3, ReadBufferInfo->resourceGPUHandle);		// SrcBuffer 
		pCommandList->SetComputeRootDescriptorTable(5, ScratchBufferInfo.resourceGPUHandle);	// Scratch buffer

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

		pCommandList->SetComputeRootDescriptorTable(6, ReducedScratchBufferInfo.resourceGPUHandle);	// Scratch reduce buffer

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
			pCommandList->SetComputeRootDescriptorTable(4, ReadPayloadBufferInfo->resourceGPUHandle);	// ScrPayload
			pCommandList->SetComputeRootDescriptorTable(8, WritePayloadBufferInfo->resourceGPUHandle);	// DstPayload
		}

		pCommandList->SetComputeRootDescriptorTable(7, WriteBufferInfo->resourceGPUHandle);				// DstBuffer 

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
}