// This file is part of the FidelityFX SDK.
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <codecvt>  // convert string to wstring
#include <DirectXMath.h>
#include <cstring>
#include "../ffx_fsr2.h"
#include "ffx_fsr2_dx11.h"
#include "shaders/ffx_fsr2_shaders_dx11.h"  // include all the precompiled D3D11 shaders for the FSR2 passes
#include "../ffx_fsr2_private.h"

// DX11 prototypes for functions in the backend interface
FfxErrorCode GetDeviceCapabilitiesDX11(FfxFsr2Interface* backendInterface, FfxDeviceCapabilities* deviceCapabilities, FfxDevice device);
FfxErrorCode CreateBackendContextDX11(FfxFsr2Interface* backendInterface, FfxDevice device);
FfxErrorCode DestroyBackendContextDX11(FfxFsr2Interface* backendInterface);
FfxErrorCode CreateResourceDX11(FfxFsr2Interface* backendInterface, const FfxCreateResourceDescription* createResourceDescription, FfxResourceInternal* outTexture);
FfxErrorCode RegisterResourceDX11(FfxFsr2Interface* backendInterface, const FfxResource* inResource, FfxResourceInternal* outResourceInternal);
FfxErrorCode UnregisterResourcesDX11(FfxFsr2Interface* backendInterface);
FfxResourceDescription GetResourceDescriptionDX11(FfxFsr2Interface* backendInterface, FfxResourceInternal resource);
FfxErrorCode DestroyResourceDX11(FfxFsr2Interface* backendInterface, FfxResourceInternal resource);
FfxErrorCode CreatePipelineDX11(FfxFsr2Interface* backendInterface, FfxFsr2Pass passId, const FfxPipelineDescription* desc, FfxPipelineState* outPass);
FfxErrorCode DestroyPipelineDX11(FfxFsr2Interface* backendInterface, FfxPipelineState* pipeline);
FfxErrorCode ScheduleGpuJobDX11(FfxFsr2Interface* backendInterface, const FfxGpuJobDescription* job);
FfxErrorCode ExecuteGpuJobsDX11(FfxFsr2Interface* backendInterface, FfxCommandList commandList);

#define FSR2_MAX_QUEUED_FRAMES  ( 4)
#define FSR2_MAX_RESOURCE_COUNT (64)
#define FSR2_DESC_RING_SIZE     (FSR2_MAX_QUEUED_FRAMES * FFX_FSR2_PASS_COUNT * FSR2_MAX_RESOURCE_COUNT)
#define FSR2_MAX_BARRIERS       (16)
#define FSR2_MAX_GPU_JOBS       (32)
#define FSR2_MAX_SAMPLERS       ( 2)
#define UPLOAD_JOB_COUNT        (16)

typedef struct BackendContext_DX11 {
    
    // store for resources and resourceViews
    typedef struct Resource
    {
#ifdef _DEBUG
        wchar_t                 resourceName[64] = {};
#endif
        ID3D11Resource*         resourcePtr;
        FfxResourceDescription  resourceDescriptor;
        FfxResourceStates       state;
        uint32_t                srvDescIndex;
        uint32_t                uavDescIndex;
        uint32_t                uavDescCount;
    } Resource;

    ID3D11Device*               device = nullptr;

    FfxGpuJobDescription        gpuJobs[FSR2_MAX_GPU_JOBS] = {};
    uint32_t                    gpuJobCount;

    uint32_t                    nextStaticResource;
    uint32_t                    nextDynamicResource;
    Resource                    resources[FSR2_MAX_RESOURCE_COUNT];
    ID3D11ShaderResourceView*   descHeapSrvCpu[FSR2_MAX_RESOURCE_COUNT];

    uint32_t                    nextStaticUavDescriptor;
    uint32_t                    nextDynamicUavDescriptor;
    ID3D11UnorderedAccessView*  descHeapUavCpu[FSR2_MAX_RESOURCE_COUNT];
    ID3D11UnorderedAccessView*  descHeapUavGpu[FSR2_MAX_RESOURCE_COUNT];

    ID3D11SamplerState*         pointClampSampler = nullptr;
    ID3D11SamplerState*         linearClampSampler = nullptr;

    ID3D11Buffer*               constantBuffers[FSR2_MAX_RESOURCE_COUNT];
    uint32_t                    nextConstantBuffer;

} BackendContext_DX11;

typedef struct Pipeline_DX11
{
    ID3D11ComputeShader* shader;
} Pipeline_DX11;

ID3D11SamplerState* nullSampler = nullptr;
ID3D11ShaderResourceView* nullSRV = nullptr;
ID3D11UnorderedAccessView* nullUAV = nullptr;
ID3D11Buffer* nullCB = nullptr;

FFX_API size_t ffxFsr2GetScratchMemorySizeDX11()
{
    return FFX_ALIGN_UP(sizeof(BackendContext_DX11), sizeof(uint64_t));
}

// populate interface with DX11 pointers.
FfxErrorCode ffxFsr2GetInterfaceDX11(
    FfxFsr2Interface* outInterface,
    ID3D11Device* device,
    void* scratchBuffer,
    size_t scratchBufferSize)
{
    FFX_RETURN_ON_ERROR(
        outInterface,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        scratchBuffer,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        scratchBufferSize >= ffxFsr2GetScratchMemorySizeDX11(),
        FFX_ERROR_INSUFFICIENT_MEMORY);

    outInterface->fpGetDeviceCapabilities = GetDeviceCapabilitiesDX11;
    outInterface->fpCreateBackendContext = CreateBackendContextDX11;
    outInterface->fpDestroyBackendContext = DestroyBackendContextDX11;
    outInterface->fpCreateResource = CreateResourceDX11;
    outInterface->fpRegisterResource = RegisterResourceDX11;
    outInterface->fpUnregisterResources = UnregisterResourcesDX11;
    outInterface->fpGetResourceDescription = GetResourceDescriptionDX11;
    outInterface->fpDestroyResource = DestroyResourceDX11;
    outInterface->fpCreatePipeline = CreatePipelineDX11;
    outInterface->fpDestroyPipeline = DestroyPipelineDX11;
    outInterface->fpScheduleGpuJob = ScheduleGpuJobDX11;
    outInterface->fpExecuteGpuJobs = ExecuteGpuJobsDX11;
    outInterface->scratchBuffer = scratchBuffer;
    outInterface->scratchBufferSize = scratchBufferSize;

    return FFX_OK;
}

void TIF(HRESULT result)
{
    if (FAILED(result))
    {
        wchar_t errorMessage[256];
        memset(errorMessage, 0, 256);
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, 255, NULL);
        char errA[256];
        size_t returnSize;
        wcstombs_s(&returnSize, errA, 255, errorMessage, 255);
#ifdef _DEBUG
        int32_t msgboxID = MessageBoxW(NULL, errorMessage, L"Error", MB_OK);
#endif
        throw 1;
    }
}

// fix up format in case resource passed to FSR2 was created as typeless
static DXGI_FORMAT convertFormat(DXGI_FORMAT format)
{
    switch (format)
    {
        // Handle Depth
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_UNORM;

        // Handle color: assume FLOAT for 16 and 32 bit channels, else UNORM
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R32G32_TYPELESS:
            return DXGI_FORMAT_R32G32_FLOAT;
        case DXGI_FORMAT_R16G16_TYPELESS:
            return DXGI_FORMAT_R16G16_FLOAT;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case DXGI_FORMAT_R32_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R8G8_TYPELESS:
            return DXGI_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R16_TYPELESS:
            return DXGI_FORMAT_R16_FLOAT;
        case DXGI_FORMAT_R8_TYPELESS:
            return DXGI_FORMAT_R8_UNORM;
        default:
            return format;
    }
}

ID3D11Resource* getDX11ResourcePtr(BackendContext_DX11* backendContext, int32_t resourceIndex)
{
    FFX_ASSERT(NULL != backendContext);
    return reinterpret_cast<ID3D11Resource*>(backendContext->resources[resourceIndex].resourcePtr);
}

// Create a FfxFsr2Device from a ID3D11Device*
FfxDevice ffxGetDeviceDX11(ID3D11Device* dx11Device)
{
    FFX_ASSERT(NULL != dx11Device);
    return reinterpret_cast<FfxDevice>(dx11Device);
}

DXGI_FORMAT ffxGetDX11FormatFromSurfaceFormat(FfxSurfaceFormat surfaceFormat)
{
    switch (surfaceFormat)
    {
        case(FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS):
            return DXGI_FORMAT_R32G32B32A32_TYPELESS;
        case(FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT):
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case(FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT):
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case(FFX_SURFACE_FORMAT_R16G16B16A16_UNORM):
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case(FFX_SURFACE_FORMAT_R32G32_FLOAT):
            return DXGI_FORMAT_R32G32_FLOAT;
        case(FFX_SURFACE_FORMAT_R32_UINT):
            return DXGI_FORMAT_R32_UINT;
        case(FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS):
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case(FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case(FFX_SURFACE_FORMAT_R11G11B10_FLOAT):
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case(FFX_SURFACE_FORMAT_R16G16_FLOAT):
            return DXGI_FORMAT_R16G16_FLOAT;
        case(FFX_SURFACE_FORMAT_R16G16_UINT):
            return DXGI_FORMAT_R16G16_UINT;
        case(FFX_SURFACE_FORMAT_R16_FLOAT):
            return DXGI_FORMAT_R16_FLOAT;
        case(FFX_SURFACE_FORMAT_R16_UINT):
            return DXGI_FORMAT_R16_UINT;
        case(FFX_SURFACE_FORMAT_R16_UNORM):
            return DXGI_FORMAT_R16_UNORM;
        case(FFX_SURFACE_FORMAT_R16_SNORM):
            return DXGI_FORMAT_R16_SNORM;
        case(FFX_SURFACE_FORMAT_R8_UNORM):
            return DXGI_FORMAT_R8_UNORM;
        case(FFX_SURFACE_FORMAT_R8_UINT):
            return DXGI_FORMAT_R8_UINT;
        case(FFX_SURFACE_FORMAT_R8G8_UNORM):
            return DXGI_FORMAT_R8G8_UNORM;
        case(FFX_SURFACE_FORMAT_R32_FLOAT):
            return DXGI_FORMAT_R32_FLOAT;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

UINT ffxGetDX11BindFlags(FfxResourceUsage flags)
{
    UINT dx11BindFlags = 0;
    if (flags & FFX_RESOURCE_USAGE_RENDERTARGET) dx11BindFlags |= D3D11_BIND_RENDER_TARGET;
    if (flags & FFX_RESOURCE_USAGE_UAV) dx11BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    return dx11BindFlags;
}

FfxSurfaceFormat ffxGetSurfaceFormatDX11(DXGI_FORMAT format)
{
    switch (format)
    {
        case(DXGI_FORMAT_R32G32B32A32_TYPELESS):
            return FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
        case(DXGI_FORMAT_R32G32B32A32_FLOAT):
            return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
        case(DXGI_FORMAT_R16G16B16A16_FLOAT):
            return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
        case(DXGI_FORMAT_R16G16B16A16_UNORM):
            return FFX_SURFACE_FORMAT_R16G16B16A16_UNORM;
        case(DXGI_FORMAT_R32G32_FLOAT):
            return FFX_SURFACE_FORMAT_R32G32_FLOAT;
        case(DXGI_FORMAT_R32_UINT):
            return FFX_SURFACE_FORMAT_R32_UINT;
        case(DXGI_FORMAT_R8G8B8A8_TYPELESS):
            return FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
        case(DXGI_FORMAT_R8G8B8A8_UNORM):
            return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
        case(DXGI_FORMAT_R11G11B10_FLOAT):
            return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
        case(DXGI_FORMAT_R16G16_FLOAT):
            return FFX_SURFACE_FORMAT_R16G16_FLOAT;
        case(DXGI_FORMAT_R16G16_UINT):
            return FFX_SURFACE_FORMAT_R16G16_UINT;
        case(DXGI_FORMAT_R16_FLOAT):
            return FFX_SURFACE_FORMAT_R16_FLOAT;
        case(DXGI_FORMAT_R16_UINT):
            return FFX_SURFACE_FORMAT_R16_UINT;
        case(DXGI_FORMAT_R16_UNORM):
            return FFX_SURFACE_FORMAT_R16_UNORM;
        case(DXGI_FORMAT_R16_SNORM):
            return FFX_SURFACE_FORMAT_R16_SNORM;
        case(DXGI_FORMAT_R8_UNORM):
            return FFX_SURFACE_FORMAT_R8_UNORM;
        case(DXGI_FORMAT_R8_UINT):
            return FFX_SURFACE_FORMAT_R8_UINT;
        default:
            return FFX_SURFACE_FORMAT_UNKNOWN;
    }
}

// register a DX11 resource to the backend
FfxResource ffxGetResourceDX11(FfxFsr2Context* context, ID3D11Resource* dx11Resource, const wchar_t* name, FfxResourceStates state)
{
    FfxResource resource = {};
    resource.resource = reinterpret_cast<void*>(dx11Resource);
    resource.state = state;
    resource.descriptorData = 0;

    if (dx11Resource) {
        resource.description.flags = FFX_RESOURCE_FLAGS_NONE;
        resource.description.width = 1;
        resource.description.height = 1;
        resource.description.depth = 1;
        resource.description.mipCount = 1;
        resource.description.format = FFX_SURFACE_FORMAT_UNKNOWN;

        D3D11_RESOURCE_DIMENSION resDimension;
        dx11Resource->GetType(&resDimension);

        switch (resDimension)
        {
        case D3D11_RESOURCE_DIMENSION_BUFFER:
            resource.description.type = FFX_RESOURCE_TYPE_BUFFER;
            D3D11_BUFFER_DESC bufferDesc;
            reinterpret_cast<ID3D11Buffer*>(dx11Resource)->GetDesc(&bufferDesc);
            resource.description.width = bufferDesc.ByteWidth;
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            resource.description.type = FFX_RESOURCE_TYPE_TEXTURE1D;
            D3D11_TEXTURE1D_DESC tex1DDesc;
            reinterpret_cast<ID3D11Texture1D*>(dx11Resource)->GetDesc(&tex1DDesc);
            resource.description.width = tex1DDesc.Width;
            resource.description.mipCount = tex1DDesc.MipLevels;
            resource.description.format = ffxGetSurfaceFormatDX11(tex1DDesc.Format);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            resource.description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
            D3D11_TEXTURE2D_DESC tex2DDesc;
            reinterpret_cast<ID3D11Texture2D*>(dx11Resource)->GetDesc(&tex2DDesc);
            resource.description.width = tex2DDesc.Width;
            resource.description.height = tex2DDesc.Height;
            resource.description.mipCount = tex2DDesc.MipLevels;
            resource.description.format = ffxGetSurfaceFormatDX11(tex2DDesc.Format);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            resource.description.type = FFX_RESOURCE_TYPE_TEXTURE3D;
            D3D11_TEXTURE3D_DESC tex3DDesc;
            reinterpret_cast<ID3D11Texture3D*>(dx11Resource)->GetDesc(&tex3DDesc);
            resource.description.width = tex3DDesc.Width;
            resource.description.height = tex3DDesc.Height;
            resource.description.depth = tex3DDesc.Depth;
            resource.description.mipCount = tex3DDesc.MipLevels;
            resource.description.format = ffxGetSurfaceFormatDX11(tex3DDesc.Format);
            break;
        default:
            break;
        }
    }
#ifdef _DEBUG
    if (name) {
        wcscpy_s(resource.name, name);
    }
#endif

    return resource;
}

ID3D11Resource* ffxGetDX11ResourcePtr(FfxFsr2Context* context, uint32_t resId)
{
    FfxFsr2Context_Private* contextPrivate = (FfxFsr2Context_Private*)(context);
    BackendContext_DX11* backendContext = (BackendContext_DX11*)(contextPrivate->contextDescription.callbacks.scratchBuffer);

    if (resId > FFX_FSR2_RESOURCE_IDENTIFIER_INPUT_TRANSPARENCY_AND_COMPOSITION_MASK)
        return backendContext->resources[contextPrivate->uavResources[resId].internalIndex].resourcePtr;
    else // Input resources are present only in srvResources array
        return backendContext->resources[contextPrivate->srvResources[resId].internalIndex].resourcePtr;
}

FfxErrorCode RegisterResourceDX11(
    FfxFsr2Interface* backendInterface,
    const FfxResource* inFfxResource,
    FfxResourceInternal* outFfxResourceInternal
)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)(backendInterface->scratchBuffer);
    ID3D11Device* dx11Device = reinterpret_cast<ID3D11Device*>(backendContext->device);
    ID3D11Resource* dx11Resource = reinterpret_cast<ID3D11Resource*>(inFfxResource->resource);

    FfxResourceStates state = inFfxResource->state;

    if (dx11Resource == nullptr)
    {
        outFfxResourceInternal->internalIndex = FFX_FSR2_RESOURCE_IDENTIFIER_NULL;
        return FFX_OK;
    }

    FFX_ASSERT(backendContext->nextDynamicResource > backendContext->nextStaticResource);
    outFfxResourceInternal->internalIndex = backendContext->nextDynamicResource--;

    BackendContext_DX11::Resource* backendResource = &backendContext->resources[outFfxResourceInternal->internalIndex];
    backendResource->resourcePtr = dx11Resource;
    backendResource->state = state;
#ifdef _DEBUG
    const wchar_t* name = inFfxResource->name;
    if(name)
        wcscpy_s(backendResource->resourceName, name);
#endif

    // create resource views
    if (dx11Resource)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        bool allowUAV = false;

        D3D11_RESOURCE_DIMENSION resDimension;
        dx11Resource->GetType(&resDimension);
        switch (resDimension)
        {
            case D3D11_RESOURCE_DIMENSION_BUFFER:
                D3D11_BUFFER_DESC bufferDesc;
                reinterpret_cast<ID3D11Buffer*>(dx11Resource)->GetDesc(&bufferDesc);
                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
                backendResource->resourceDescriptor.type = FFX_RESOURCE_TYPE_BUFFER;
                backendResource->resourceDescriptor.width = uint32_t(bufferDesc.ByteWidth);
                allowUAV = bufferDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                D3D11_TEXTURE1D_DESC tex1DDesc;
                reinterpret_cast<ID3D11Texture1D*>(dx11Resource)->GetDesc(&tex1DDesc);
                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                srvDesc.Texture1D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex1DDesc.Format);
                srvDesc.Format = convertFormat(tex1DDesc.Format);
                backendResource->resourceDescriptor.type = FFX_RESOURCE_TYPE_TEXTURE1D;
                backendResource->resourceDescriptor.format = ffxGetSurfaceFormatDX11(tex1DDesc.Format);
                backendResource->resourceDescriptor.width = uint32_t(tex1DDesc.Width);
                backendResource->resourceDescriptor.mipCount = uint32_t(tex1DDesc.MipLevels);
                allowUAV = tex1DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                D3D11_TEXTURE2D_DESC tex2DDesc;
                reinterpret_cast<ID3D11Texture2D*>(dx11Resource)->GetDesc(&tex2DDesc);
                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex2DDesc.Format);
                srvDesc.Format = convertFormat(tex2DDesc.Format);
                backendResource->resourceDescriptor.type = FFX_RESOURCE_TYPE_TEXTURE2D;
                backendResource->resourceDescriptor.format = ffxGetSurfaceFormatDX11(tex2DDesc.Format);
                backendResource->resourceDescriptor.width = uint32_t(tex2DDesc.Width);
                backendResource->resourceDescriptor.height = uint32_t(tex2DDesc.Height);
                backendResource->resourceDescriptor.mipCount = uint32_t(tex2DDesc.MipLevels);
                allowUAV = tex2DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                D3D11_TEXTURE3D_DESC tex3DDesc;
                reinterpret_cast<ID3D11Texture3D*>(dx11Resource)->GetDesc(&tex3DDesc);
                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex3DDesc.Format);
                srvDesc.Format = convertFormat(tex3DDesc.Format);
                backendResource->resourceDescriptor.type = FFX_RESOURCE_TYPE_TEXTURE3D;
                backendResource->resourceDescriptor.format = ffxGetSurfaceFormatDX11(tex3DDesc.Format);
                backendResource->resourceDescriptor.width = uint32_t(tex3DDesc.Width);
                backendResource->resourceDescriptor.height = uint32_t(tex3DDesc.Height);
                backendResource->resourceDescriptor.depth = uint32_t(tex3DDesc.Depth);
                backendResource->resourceDescriptor.mipCount = uint32_t(tex3DDesc.MipLevels);
                allowUAV = tex3DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            default:
                break;
        }

        // set up resorce view descriptors
        if (resDimension != D3D11_RESOURCE_DIMENSION_BUFFER)
        {
            // CPU readable
            TIF(dx11Device->CreateShaderResourceView(dx11Resource, &srvDesc, &backendContext->descHeapSrvCpu[outFfxResourceInternal->internalIndex]));

            // UAV
            if (allowUAV)
            {
                int uavDescriptorsCount = backendResource->resourceDescriptor.mipCount;
                FFX_ASSERT(backendContext->nextDynamicUavDescriptor - uavDescriptorsCount + 1 > backendContext->nextStaticResource);

                backendResource->uavDescCount = uavDescriptorsCount;
                backendResource->uavDescIndex = backendContext->nextDynamicUavDescriptor - uavDescriptorsCount + 1;

                for (int mip = 0; mip < uavDescriptorsCount; ++mip)
                {
                    uavDesc.Texture2D.MipSlice = mip;

                    TIF(dx11Device->CreateUnorderedAccessView(dx11Resource, &uavDesc, &backendContext->descHeapUavCpu[backendResource->uavDescIndex + mip]));
                    TIF(dx11Device->CreateUnorderedAccessView(dx11Resource, &uavDesc, &backendContext->descHeapUavGpu[backendResource->uavDescIndex + mip]));
                }
                backendContext->nextDynamicUavDescriptor -= uavDescriptorsCount;
            }
        }        
    }
    return FFX_OK;
}

// dispose dynamic resources: This should be called at the end of the frame
FfxErrorCode UnregisterResourcesDX11(FfxFsr2Interface* backendInterface)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)(backendInterface->scratchBuffer);

    ID3D11Device* dx11Device = reinterpret_cast<ID3D11Device*>(backendContext->device);

    for (auto i = backendContext->nextDynamicResource + 1; i < FSR2_MAX_RESOURCE_COUNT; ++i)
    {
        if (backendContext->descHeapSrvCpu[i])
        {
            backendContext->descHeapSrvCpu[i]->Release();
            backendContext->descHeapSrvCpu[i] = nullptr;
        }
    }

    for (auto i = backendContext->nextDynamicUavDescriptor + 1; i < FSR2_MAX_RESOURCE_COUNT; ++i)
    {
        if (backendContext->descHeapUavCpu[i])
        {
            backendContext->descHeapUavCpu[i]->Release();
            backendContext->descHeapUavCpu[i] = nullptr;
        }
        if (backendContext->descHeapUavGpu[i])
        {
            backendContext->descHeapUavGpu[i]->Release();
            backendContext->descHeapUavGpu[i] = nullptr;
        }
    }

    backendContext->nextDynamicResource = FSR2_MAX_RESOURCE_COUNT - 1;
    backendContext->nextDynamicUavDescriptor = FSR2_MAX_RESOURCE_COUNT - 1;

    return FFX_OK;
}

// query device capabilities to select the optimal shader permutation
FfxErrorCode GetDeviceCapabilitiesDX11(FfxFsr2Interface* backendInterface, FfxDeviceCapabilities* deviceCapabilities, FfxDevice device)
{
    ID3D11Device* dx11Device = reinterpret_cast<ID3D11Device*>(device);

    FFX_UNUSED(backendInterface);
    FFX_ASSERT(NULL != deviceCapabilities);
    FFX_ASSERT(NULL != dx11Device);

    // check if we have 16bit floating point.
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT minPrecision = {};
    if (SUCCEEDED(dx11Device->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &minPrecision, sizeof(minPrecision)))) {

        deviceCapabilities->fp16Supported = !!(minPrecision.PixelShaderMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT);
    }

    return FFX_OK;
}

// initialize the DX11 backend
FfxErrorCode CreateBackendContextDX11(FfxFsr2Interface* backendInterface, FfxDevice device)
{
    HRESULT result = S_OK;
    ID3D11Device* dx11Device = reinterpret_cast<ID3D11Device*>(device);

    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != dx11Device);

    // set up some internal resources we need (space for resource views and constant buffers)
    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    std::memset(backendContext, 0, sizeof(*backendContext));

    if (dx11Device != NULL)
    {
        dx11Device->AddRef();
        backendContext->device = dx11Device;
    }
    
    // init resource linked list
    backendContext->nextStaticResource = 1;
    backendContext->nextDynamicResource = FSR2_MAX_RESOURCE_COUNT - 1;
    backendContext->nextStaticUavDescriptor = 0;
    backendContext->nextDynamicUavDescriptor = FSR2_MAX_RESOURCE_COUNT - 1;

    backendContext->resources[0] = {};

    const D3D11_SAMPLER_DESC pointClampSamplerDesc {
        D3D11_FILTER_MIN_MAG_MIP_POINT,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        0,
        16,
        D3D11_COMPARISON_NEVER,
        { 1.f, 1.f, 1.f, 1.f },
        0.f,
        D3D11_FLOAT32_MAX,
    };

    const D3D11_SAMPLER_DESC linearClampSamplerDesc {
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        0,
        16,
        D3D11_COMPARISON_NEVER,
        { 1.f, 1.f, 1.f, 1.f },
        0.f,
        D3D11_FLOAT32_MAX,
    };

    dx11Device->CreateSamplerState(&pointClampSamplerDesc, &backendContext->pointClampSampler);
    dx11Device->CreateSamplerState(&linearClampSamplerDesc, &backendContext->linearClampSampler);

    for (auto i = 0; i < FSR2_MAX_RESOURCE_COUNT; ++i)
    {
        backendContext->constantBuffers[i] = nullptr;
    }

    return FFX_OK;
}

// deinitialize the DX11 backend
FfxErrorCode DestroyBackendContextDX11(FfxFsr2Interface* backendInterface)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    for (auto i = 0; i < FSR2_MAX_RESOURCE_COUNT; ++i)
    {
        if (backendContext->descHeapSrvCpu[i])
            backendContext->descHeapSrvCpu[i]->Release();
        if (backendContext->descHeapUavCpu[i])
            backendContext->descHeapUavCpu[i]->Release();
        if (backendContext->descHeapUavGpu[i])
            backendContext->descHeapUavGpu[i]->Release();
    }

    for (uint32_t currentStaticResourceIndex = 0; currentStaticResourceIndex < backendContext->nextStaticResource; ++currentStaticResourceIndex)
    {
        if (backendContext->resources[currentStaticResourceIndex].resourcePtr)
        {
            backendContext->resources[currentStaticResourceIndex].resourcePtr->Release();
            backendContext->resources[currentStaticResourceIndex].resourcePtr = nullptr;
        }
    }
    backendContext->nextStaticResource = 0;

    if (backendContext->pointClampSampler)
    {
        backendContext->pointClampSampler->Release();
        backendContext->pointClampSampler = nullptr;
    }
    if (backendContext->linearClampSampler)
    {
        backendContext->linearClampSampler->Release();
        backendContext->linearClampSampler = nullptr;
    }

    for (auto i = 0; i < FSR2_MAX_RESOURCE_COUNT; ++i)
    {
        if (backendContext->constantBuffers[i] != nullptr)
        {
            backendContext->constantBuffers[i]->Release();
        }
    }

    if (backendContext->device != NULL) {

        backendContext->device->Release();
        backendContext->device = NULL;
    }

    return FFX_OK;
}

// create a internal resource that will stay alive until effect gets shut down
FfxErrorCode CreateResourceDX11(
    FfxFsr2Interface* backendInterface,
    const FfxCreateResourceDescription* createResourceDescription,
    FfxResourceInternal* outTexture)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != createResourceDescription);
    FFX_ASSERT(NULL != outTexture);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    ID3D11Device* dx11Device = backendContext->device;

    FFX_ASSERT(NULL != dx11Device);

    FFX_ASSERT(backendContext->nextStaticResource + 1 < backendContext->nextDynamicResource);

    outTexture->internalIndex = backendContext->nextStaticResource++;
    BackendContext_DX11::Resource* backendResource = &backendContext->resources[outTexture->internalIndex];
    backendResource->resourceDescriptor = createResourceDescription->resourceDescription;

    // Initial data should be uploaded on resource creation for DX11, no additional upload heap type should be used
    FFX_ASSERT(createResourceDescription->heapType != FFX_HEAP_TYPE_UPLOAD);

    backendResource->state = createResourceDescription->initalState;

#ifdef _DEBUG
    wcscpy_s(backendResource->resourceName, createResourceDescription->name);
#endif

    // Create SRVs and UAVs
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        bool allowUAV = false;
        unsigned int mipCount = 0;

        D3D11_SUBRESOURCE_DATA initData = { createResourceDescription->initData, 0, 0 };
        D3D11_SUBRESOURCE_DATA* pInitData = createResourceDescription->initData ? &initData : nullptr;

        switch (createResourceDescription->resourceDescription.type)
        {
            case FFX_RESOURCE_TYPE_BUFFER:
                ID3D11Buffer* bufferPtr;
                D3D11_BUFFER_DESC bufDesc;
                bufDesc.ByteWidth = (createResourceDescription->resourceDescription.width + 15) & (~0x0F);    // up to multiples of 16
                bufDesc.Usage = D3D11_USAGE_DYNAMIC;
                bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER | ffxGetDX11BindFlags(createResourceDescription->usage);
                bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                TIF(dx11Device->CreateBuffer(&bufDesc, pInitData, &bufferPtr));
                backendResource->resourcePtr = bufferPtr;

                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

                bufferPtr->SetPrivateData(WKPDID_D3DDebugObjectNameW, UINT(wcslen(createResourceDescription->name) * sizeof(wchar_t)), createResourceDescription->name);
                backendResource->resourcePtr = bufferPtr;
                allowUAV = bufDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case FFX_RESOURCE_TYPE_TEXTURE1D:
                ID3D11Texture1D* tex1DPtr;
                D3D11_TEXTURE1D_DESC tex1DDesc;
                tex1DDesc.Width = createResourceDescription->resourceDescription.width;
                tex1DDesc.MipLevels = createResourceDescription->resourceDescription.mipCount;
                tex1DDesc.ArraySize = 1;
                tex1DDesc.Format = ffxGetDX11FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
                tex1DDesc.Usage = D3D11_USAGE_DEFAULT;
                tex1DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | ffxGetDX11BindFlags(createResourceDescription->usage);
                tex1DDesc.CPUAccessFlags = 0;
                tex1DDesc.MiscFlags = 0;
                TIF(dx11Device->CreateTexture1D(&tex1DDesc, pInitData, &tex1DPtr));

                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                srvDesc.Texture1D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex1DDesc.Format);
                srvDesc.Format = convertFormat(tex1DDesc.Format);

                tex1DPtr->SetPrivateData(WKPDID_D3DDebugObjectNameW, UINT(wcslen(createResourceDescription->name) * sizeof(wchar_t)), createResourceDescription->name);
                backendResource->resourcePtr = tex1DPtr;
                tex1DPtr->GetDesc(&tex1DDesc);
                mipCount = tex1DDesc.MipLevels;
                allowUAV = tex1DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case FFX_RESOURCE_TYPE_TEXTURE2D:
                ID3D11Texture2D* tex2DPtr;
                D3D11_TEXTURE2D_DESC tex2DDesc;
                tex2DDesc.Width = createResourceDescription->resourceDescription.width;
                tex2DDesc.Height = createResourceDescription->resourceDescription.height;
                tex2DDesc.MipLevels = createResourceDescription->resourceDescription.mipCount;
                tex2DDesc.ArraySize = 1;
                tex2DDesc.Format = ffxGetDX11FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
                tex2DDesc.SampleDesc.Count = 1;
                tex2DDesc.SampleDesc.Quality = 0;
                tex2DDesc.Usage = D3D11_USAGE_DEFAULT;
                tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | ffxGetDX11BindFlags(createResourceDescription->usage);
                tex2DDesc.CPUAccessFlags = 0;
                tex2DDesc.MiscFlags = 0;
                initData.SysMemPitch = createResourceDescription->initDataSize / createResourceDescription->resourceDescription.height;
                TIF(dx11Device->CreateTexture2D(&tex2DDesc, pInitData, &tex2DPtr));

                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex2DDesc.Format);
                srvDesc.Format = convertFormat(tex2DDesc.Format);

                tex2DPtr->SetPrivateData(WKPDID_D3DDebugObjectNameW, UINT(wcslen(createResourceDescription->name) * sizeof(wchar_t)), createResourceDescription->name);
                backendResource->resourcePtr = tex2DPtr;
                tex2DPtr->GetDesc(&tex2DDesc);
                mipCount = tex2DDesc.MipLevels;
                allowUAV = tex2DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            case FFX_RESOURCE_TYPE_TEXTURE3D:
                ID3D11Texture3D* tex3DPtr;
                D3D11_TEXTURE3D_DESC tex3DDesc;
                tex3DDesc.Width = createResourceDescription->resourceDescription.width;
                tex3DDesc.Height = createResourceDescription->resourceDescription.height;
                tex3DDesc.Depth = createResourceDescription->resourceDescription.depth;
                tex3DDesc.MipLevels = createResourceDescription->resourceDescription.mipCount;
                tex3DDesc.Format = ffxGetDX11FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
                tex3DDesc.Usage = D3D11_USAGE_DEFAULT;
                tex3DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | ffxGetDX11BindFlags(createResourceDescription->usage);
                tex3DDesc.CPUAccessFlags = 0;
                tex3DDesc.MiscFlags = 0;
                initData.SysMemSlicePitch = createResourceDescription->initDataSize / (createResourceDescription->resourceDescription.height * createResourceDescription->resourceDescription.depth);
                initData.SysMemPitch = createResourceDescription->initDataSize / createResourceDescription->resourceDescription.depth;
                TIF(dx11Device->CreateTexture3D(&tex3DDesc, pInitData, &tex3DPtr));

                uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MipLevels = -1;
                uavDesc.Format = convertFormat(tex3DDesc.Format);
                srvDesc.Format = convertFormat(tex3DDesc.Format);

                tex3DPtr->SetPrivateData(WKPDID_D3DDebugObjectNameW, UINT(wcslen(createResourceDescription->name) * sizeof(wchar_t)), createResourceDescription->name);
                backendResource->resourcePtr = tex3DPtr;
                tex3DPtr->GetDesc(&tex3DDesc);
                mipCount = tex3DDesc.MipLevels;
                allowUAV = tex3DDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS;
                break;
            default:
                break;
        }

        if (createResourceDescription->resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER)
        {
            // CPU readable
            TIF(dx11Device->CreateShaderResourceView(backendResource->resourcePtr, &srvDesc, &backendContext->descHeapSrvCpu[outTexture->internalIndex]));

            // UAV
            if (createResourceDescription->usage & FFX_RESOURCE_USAGE_UAV)
            {
                int uavDescriptorCount = mipCount;
                FFX_ASSERT(backendContext->nextStaticUavDescriptor + uavDescriptorCount < backendContext->nextDynamicResource);

                backendResource->uavDescCount = uavDescriptorCount;
                backendResource->uavDescIndex = backendContext->nextStaticUavDescriptor;

                for (int mip = 0; mip < uavDescriptorCount; ++mip)
                {
                    uavDesc.Texture2D.MipSlice = mip;

                    TIF(dx11Device->CreateUnorderedAccessView(backendResource->resourcePtr, &uavDesc, &backendContext->descHeapUavCpu[backendResource->uavDescIndex + mip]));
                    TIF(dx11Device->CreateUnorderedAccessView(backendResource->resourcePtr, &uavDesc, &backendContext->descHeapUavGpu[backendResource->uavDescIndex + mip]));
                }
                backendContext->nextStaticUavDescriptor += uavDescriptorCount;
            }
        }
    }
    return FFX_OK;
}

FfxResourceDescription GetResourceDescriptionDX11(
    FfxFsr2Interface* backendInterface, 
    FfxResourceInternal resource)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;

    if (resource.internalIndex != -1)
    {
        FfxResourceDescription desc = backendContext->resources[resource.internalIndex].resourceDescriptor;
        return desc;
    }
    else
    {
        FfxResourceDescription desc = {};
        return desc;
    }
}

FfxErrorCode CreatePipelineDX11(
    FfxFsr2Interface* backendInterface,
    FfxFsr2Pass pass,
    const FfxPipelineDescription* pipelineDescription,
    FfxPipelineState* outPipeline)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != pipelineDescription);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    ID3D11Device* dx11Device = backendContext->device;

    Pipeline_DX11* pipeline = new Pipeline_DX11;

    FFX_ASSERT(pipelineDescription->rootConstantBufferCount <= FFX_MAX_NUM_CONST_BUFFERS);
    size_t rootConstantsSize = pipelineDescription->rootConstantBufferCount;

    // always use LUT
    bool useLut = true;

    // check if we have 16bit floating point.
    bool supportedFP16 = false;
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT minPrecision = {};
    if (SUCCEEDED(dx11Device->CheckFeatureSupport(D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &minPrecision, sizeof(minPrecision)))) {

        supportedFP16 = !!(minPrecision.PixelShaderMinPrecision & D3D11_SHADER_MIN_PRECISION_16_BIT);
    }

    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (pipelineDescription->contextFlags & FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE) ? FSR2_SHADER_PERMUTATION_HDR_COLOR_INPUT : 0;
    flags |= (pipelineDescription->contextFlags & FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) ? 0 : FSR2_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS;
    flags |= (pipelineDescription->contextFlags & FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) ? FSR2_SHADER_PERMUTATION_JITTER_MOTION_VECTORS : 0;
    flags |= (pipelineDescription->contextFlags & FFX_FSR2_ENABLE_DEPTH_INVERTED) ? FSR2_SHADER_PERMUTATION_DEPTH_INVERTED : 0;
    flags |= (pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN) ? FSR2_SHADER_PERMUTATION_ENABLE_SHARPENING : 0;
    flags |= (useLut) ? FSR2_SHADER_PERMUTATION_USE_LANCZOS_TYPE : 0;
    //flags |= (canForceWave64) ? FSR2_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (supportedFP16 && (pass != FFX_FSR2_PASS_RCAS)) ? FSR2_SHADER_PERMUTATION_ALLOW_FP16 : 0;

    ID3D11ComputeShader* shader = nullptr;
    Fsr2ShaderBlobDX11 shaderBlob = fsr2GetPermutationBlobByIndexDX11(pass, flags);
    FFX_ASSERT(shaderBlob.data && shaderBlob.size);

    if (shaderBlob.data)
    {
        HRESULT hr = dx11Device->CreateComputeShader(shaderBlob.data, shaderBlob.size, NULL, &shader);
        FFX_ASSERT(hr == S_OK);
    }

    pipeline->shader = shader;
    outPipeline->pipeline = pipeline;

    // populate the pass.
    outPipeline->srvCount = shaderBlob.srvCount;
    outPipeline->uavCount = shaderBlob.uavCount;
    outPipeline->constCount = shaderBlob.cbvCount;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    for (uint32_t srvIndex = 0; srvIndex < outPipeline->srvCount; ++srvIndex)
    {
        outPipeline->srvResourceBindings[srvIndex].slotIndex = shaderBlob.boundSRVResources[srvIndex];
        wcscpy_s(outPipeline->srvResourceBindings[srvIndex].name, converter.from_bytes(shaderBlob.boundSRVResourceNames[srvIndex]).c_str());
    }
    for (uint32_t uavIndex = 0; uavIndex < outPipeline->uavCount; ++uavIndex)
    {
        outPipeline->uavResourceBindings[uavIndex].slotIndex = shaderBlob.boundUAVResources[uavIndex];
        wcscpy_s(outPipeline->uavResourceBindings[uavIndex].name, converter.from_bytes(shaderBlob.boundUAVResourceNames[uavIndex]).c_str());
    }
    for (uint32_t cbIndex = 0; cbIndex < outPipeline->constCount; ++cbIndex)
    {
        outPipeline->cbResourceBindings[cbIndex].slotIndex = shaderBlob.boundCBVResources[cbIndex];
        wcscpy_s(outPipeline->cbResourceBindings[cbIndex].name, converter.from_bytes(shaderBlob.boundCBVResourceNames[cbIndex]).c_str());
    }

    return FFX_OK;
}

FfxErrorCode ScheduleGpuJobDX11(
    FfxFsr2Interface* backendInterface,
    const FfxGpuJobDescription* job)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != job);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;

    FFX_ASSERT(backendContext->gpuJobCount < FSR2_MAX_GPU_JOBS);
    
    backendContext->gpuJobs[backendContext->gpuJobCount] = *job;

    if (job->jobType == FFX_GPU_JOB_COMPUTE) {

        // needs to copy SRVs and UAVs in case they are on the stack only
        FfxComputeJobDescription* computeJob = &backendContext->gpuJobs[backendContext->gpuJobCount].computeJobDescriptor;
        const uint32_t numConstBuffers = job->computeJobDescriptor.pipeline.constCount;
        for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < numConstBuffers; ++currentRootConstantIndex)
        {
            computeJob->cbs[currentRootConstantIndex].uint32Size = job->computeJobDescriptor.cbs[currentRootConstantIndex].uint32Size;
            memcpy(computeJob->cbs[currentRootConstantIndex].data, job->computeJobDescriptor.cbs[currentRootConstantIndex].data, computeJob->cbs[currentRootConstantIndex].uint32Size * sizeof(uint32_t));
        }
    }
    backendContext->gpuJobCount++;

    return FFX_OK;
}

static FfxErrorCode executeGpuJobCompute(BackendContext_DX11* backendContext, FfxGpuJobDescription* job, ID3D11Device* dx11Device, ID3D11DeviceContext* dx11DeviceContext)
{
    Pipeline_DX11* pipeline = reinterpret_cast<Pipeline_DX11*>(job->computeJobDescriptor.pipeline.pipeline);
    dx11DeviceContext->CSSetShader(pipeline->shader, nullptr, 0);

    // bind samplers
    dx11DeviceContext->CSSetSamplers(0, 1, &backendContext->pointClampSampler);
    dx11DeviceContext->CSSetSamplers(1, 1, &backendContext->linearClampSampler);

    // bind UAVs
    for (uint32_t currentPipelineUavIndex = 0; currentPipelineUavIndex < job->computeJobDescriptor.pipeline.uavCount; ++currentPipelineUavIndex)
    {
        uint32_t resourceIndex = job->computeJobDescriptor.uavs[currentPipelineUavIndex].internalIndex;
        uint32_t uavIndex = backendContext->resources[resourceIndex].uavDescIndex + job->computeJobDescriptor.uavMip[currentPipelineUavIndex];
        uint32_t slot = job->computeJobDescriptor.pipeline.uavResourceBindings[currentPipelineUavIndex].slotIndex;

        ID3D11UnorderedAccessView** uavs = &(backendContext->descHeapUavCpu[uavIndex]);
        dx11DeviceContext->CSSetUnorderedAccessViews(slot, 1, uavs, nullptr);
    }

    // bind SRVs
    for (uint32_t currentPipelineSrvIndex = 0; currentPipelineSrvIndex < job->computeJobDescriptor.pipeline.srvCount; ++currentPipelineSrvIndex)
    {
        uint32_t resourceIndex = job->computeJobDescriptor.srvs[currentPipelineSrvIndex].internalIndex;
        uint32_t slot = job->computeJobDescriptor.pipeline.srvResourceBindings[currentPipelineSrvIndex].slotIndex;

        ID3D11ShaderResourceView* srv = backendContext->descHeapSrvCpu[resourceIndex];
        dx11DeviceContext->CSSetShaderResources(slot, 1, &srv);
    }

    // bind constant buffers
    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < job->computeJobDescriptor.pipeline.constCount; ++currentRootConstantIndex)
    {
        int currentCbSlotIndex = job->computeJobDescriptor.pipeline.cbResourceBindings[currentRootConstantIndex].slotIndex;

        if (backendContext->constantBuffers[backendContext->nextConstantBuffer] == nullptr)
        {
            D3D11_BUFFER_DESC bufDesc;
            bufDesc.ByteWidth = FFX_MAX_CONST_SIZE * sizeof(uint32_t);
            bufDesc.Usage = D3D11_USAGE_DYNAMIC;
            bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bufDesc.MiscFlags = 0;
            TIF(dx11Device->CreateBuffer(&bufDesc, nullptr, &backendContext->constantBuffers[backendContext->nextConstantBuffer]));
        }

        D3D11_MAPPED_SUBRESOURCE mapped;
        TIF(dx11DeviceContext->Map(backendContext->constantBuffers[backendContext->nextConstantBuffer], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, job->computeJobDescriptor.cbs[currentCbSlotIndex].data, job->computeJobDescriptor.cbs[currentCbSlotIndex].uint32Size * sizeof(uint32_t));
        dx11DeviceContext->Unmap(backendContext->constantBuffers[backendContext->nextConstantBuffer], 0);

        dx11DeviceContext->CSSetConstantBuffers(currentRootConstantIndex, 1, &backendContext->constantBuffers[backendContext->nextConstantBuffer]);

        backendContext->nextConstantBuffer++;
    }

    // dispatch
    dx11DeviceContext->Dispatch(job->computeJobDescriptor.dimensions[0], job->computeJobDescriptor.dimensions[1], job->computeJobDescriptor.dimensions[2]);

    // unbind samplers
    dx11DeviceContext->CSSetSamplers(0, 1, &nullSampler);
    dx11DeviceContext->CSSetSamplers(1, 1, &nullSampler);

    // unbind UAVs
    for (uint32_t currentPipelineUavIndex = 0; currentPipelineUavIndex < job->computeJobDescriptor.pipeline.uavCount; ++currentPipelineUavIndex)
    {
        uint32_t resourceIndex = job->computeJobDescriptor.uavs[currentPipelineUavIndex].internalIndex;
        uint32_t slot = job->computeJobDescriptor.pipeline.uavResourceBindings[currentPipelineUavIndex].slotIndex;
        dx11DeviceContext->CSSetUnorderedAccessViews(slot, 1, &nullUAV, nullptr);
    }

    // unbind SRVs
    for (uint32_t currentPipelineSrvIndex = 0; currentPipelineSrvIndex < job->computeJobDescriptor.pipeline.srvCount; ++currentPipelineSrvIndex)
    {
        uint32_t slot = job->computeJobDescriptor.pipeline.srvResourceBindings[currentPipelineSrvIndex].slotIndex;
        dx11DeviceContext->CSSetShaderResources(slot, 1, &nullSRV);
    }

    // unbind constant buffers
    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < job->computeJobDescriptor.pipeline.constCount; ++currentRootConstantIndex)
    {
        dx11DeviceContext->CSSetConstantBuffers(currentRootConstantIndex, 1, &nullCB);
    }

    return FFX_OK;
}

static FfxErrorCode executeGpuJobCopy(BackendContext_DX11* backendContext, FfxGpuJobDescription* job, ID3D11Device* dx11Device, ID3D11DeviceContext* dx11DeviceContext)
{
    ID3D11Resource* dx11ResourceSrc = getDX11ResourcePtr(backendContext, job->copyJobDescriptor.src.internalIndex);
    ID3D11Resource* dx11ResourceDst = getDX11ResourcePtr(backendContext, job->copyJobDescriptor.dst.internalIndex);

    dx11DeviceContext->CopySubresourceRegion(dx11ResourceDst, 0, 0, 0, 0, dx11ResourceSrc, 0, nullptr);

    return FFX_OK;
}

static FfxErrorCode executeGpuJobClearFloat(BackendContext_DX11* backendContext, FfxGpuJobDescription* job, ID3D11Device* dx11Device, ID3D11DeviceContext* dx11DeviceContext)
{
    uint32_t idx = job->clearJobDescriptor.target.internalIndex;
    BackendContext_DX11::Resource ffxResource = backendContext->resources[idx];
    ID3D11Resource* dx11Resource = reinterpret_cast<ID3D11Resource*>(ffxResource.resourcePtr);
    uint32_t index_uav = ffxResource.uavDescIndex;

    dx11DeviceContext->ClearUnorderedAccessViewFloat(backendContext->descHeapUavGpu[index_uav], job->clearJobDescriptor.color);

    return FFX_OK;
}

FfxErrorCode ExecuteGpuJobsDX11(
    FfxFsr2Interface* backendInterface,
    FfxCommandList commandList)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    ID3D11DeviceContext* dx11DeviceContext = reinterpret_cast<ID3D11DeviceContext*>(commandList);
    ID3D11Device* dx11Device = reinterpret_cast<ID3D11Device*>(backendContext->device);

    FfxErrorCode errorCode = FFX_OK;

    // reset in-use constant buffer pointer to top
    backendContext->nextConstantBuffer = 0;

    // execute all Gpujobs
    for (uint32_t currentGpuJobIndex = 0; currentGpuJobIndex < backendContext->gpuJobCount; ++currentGpuJobIndex)
    {
        FfxGpuJobDescription* GpuJob = &backendContext->gpuJobs[currentGpuJobIndex];

        switch (GpuJob->jobType)
        {
            case FFX_GPU_JOB_CLEAR_FLOAT:
                errorCode = executeGpuJobClearFloat(backendContext, GpuJob, dx11Device, dx11DeviceContext);
                break;
            case FFX_GPU_JOB_COPY:
                errorCode = executeGpuJobCopy(backendContext, GpuJob, dx11Device, dx11DeviceContext);
                break;
            case FFX_GPU_JOB_COMPUTE:
                errorCode = executeGpuJobCompute(backendContext, GpuJob, dx11Device, dx11DeviceContext);
                break;

            default:
                break;
        }
    }

    // check the execute function returned clearly.
    FFX_RETURN_ON_ERROR(
        errorCode == FFX_OK,
        FFX_ERROR_BACKEND_API_ERROR);

    backendContext->gpuJobCount = 0;

    return FFX_OK;
}

FfxErrorCode DestroyResourceDX11(
    FfxFsr2Interface* backendInterface,
    FfxResourceInternal resource)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_DX11* backendContext = (BackendContext_DX11*)backendInterface->scratchBuffer;
    ID3D11Resource* dx11Resource = getDX11ResourcePtr(backendContext, resource.internalIndex);
    if (dx11Resource) {

        TIF(dx11Resource->Release());
        backendContext->resources[resource.internalIndex].resourcePtr = nullptr;
        //ffxDX11ReleaseResource(backendInterface, *resource);
    }

    return FFX_OK;
}

FfxErrorCode DestroyPipelineDX11(
    FfxFsr2Interface* backendInterface,
    FfxPipelineState* pipeline)
{
    FFX_ASSERT(backendInterface != nullptr);
    if (!pipeline)
    {
        return FFX_OK;
    }

    Pipeline_DX11* pipeline_dx11 = reinterpret_cast<Pipeline_DX11*>(pipeline->pipeline);
    if (pipeline_dx11)
    {
        if (pipeline_dx11->shader)
        {
            pipeline_dx11->shader->Release();
            pipeline_dx11->shader = nullptr;
        }

        delete pipeline_dx11;
    }

    return FFX_OK;
}
