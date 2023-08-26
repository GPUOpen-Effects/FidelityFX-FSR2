//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "AMD_LIB.h"

namespace AMD
{
    Buffer::Buffer()
        : _b1d(NULL)
        , _srv(NULL)
        , _uav(NULL)
        , _staging_counter_b1d(NULL)
        , _staging_b1d(NULL)
        , _size_in_bytes(0)
    {};
    Buffer::~Buffer()
    {
        Release();
    }

    void Buffer::Release()
    {
        AMD_SAFE_RELEASE(_b1d);
        AMD_SAFE_RELEASE(_srv);
        AMD_SAFE_RELEASE(_uav);
        AMD_SAFE_RELEASE(_staging_counter_b1d);
        AMD_SAFE_RELEASE(_staging_b1d);
    }

    HRESULT Buffer::CreateBuffer(ID3D11Device * device,
        unsigned int                            uSizeInBytes,
        unsigned int                            uSizeOfStructure,
        unsigned int                            uCPUAccess,
        unsigned int                            uBindFlags,
        D3D11_USAGE                             usage,
        DXGI_FORMAT                             SRV_Format,
        DXGI_FORMAT                             UAV_Format,
        unsigned int                            uUAVFlags,
        unsigned int                            uB1DFlags,
        void *                                  data)
    {
        HRESULT hr = S_OK;

        if (NULL == _b1d)
        {
            D3D11_SUBRESOURCE_DATA subresource_data;
            memset(&subresource_data, 0, sizeof(subresource_data));
            subresource_data.pSysMem = data;

            D3D11_BUFFER_DESC b1d_desc;
            memset(&b1d_desc, 0, sizeof(b1d_desc));
            b1d_desc.BindFlags = uBindFlags;
            b1d_desc.ByteWidth = uSizeInBytes;
            b1d_desc.CPUAccessFlags = uCPUAccess;
            b1d_desc.Usage = usage;
            b1d_desc.StructureByteStride = (uB1DFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) ? uSizeOfStructure : 0;
            b1d_desc.MiscFlags = uB1DFlags;

            hr = device->CreateBuffer(&b1d_desc, data ? &subresource_data : NULL, &_b1d);
            assert(S_OK == hr);

            b1d_desc.BindFlags = 0;
            b1d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            b1d_desc.Usage = D3D11_USAGE_STAGING;
            b1d_desc.MiscFlags = 0;
            hr = device->CreateBuffer(&b1d_desc, NULL, &_staging_b1d);
            assert(S_OK == hr);
            _size_in_bytes = uSizeInBytes;

            D3D11_BUFFER_DESC staging_desc;
            memset(&staging_desc, 0, sizeof(staging_desc));
            staging_desc.ByteWidth = sizeof(unsigned int);
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            hr = device->CreateBuffer(&staging_desc, NULL, &_staging_counter_b1d);
            assert(S_OK == hr);
        }

        if (uBindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            AMD_SAFE_RELEASE(_srv);

            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
            memset(&srv_desc, 0, sizeof(srv_desc));
            srv_desc.Format = SRV_Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.FirstElement = 0;
            srv_desc.Buffer.NumElements = uSizeInBytes / uSizeOfStructure;
            hr = device->CreateShaderResourceView(_b1d, &srv_desc, &_srv);
            assert(S_OK == hr);
        }

        if (uBindFlags & D3D11_BIND_UNORDERED_ACCESS)
        {
            AMD_SAFE_RELEASE(_uav);

            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
            memset(&uav_desc, 0, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
            uav_desc.Format = UAV_Format;
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.FirstElement = 0;
            uav_desc.Buffer.NumElements = uSizeInBytes / uSizeOfStructure;
            uav_desc.Buffer.Flags = uUAVFlags;
            hr = device->CreateUnorderedAccessView(_b1d, &uav_desc, &_uav);
            assert(S_OK == hr);
        }

        return hr;
    }

    int Buffer::UAVCounter(ID3D11DeviceContext * context)
    {
        int counter = 0;

        // Copy the UAV counter to a staging resource
        context->CopyStructureCount(_staging_counter_b1d, 0, _uav);

        // Map the staging resource
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        context->Map(_staging_counter_b1d, 0, D3D11_MAP_READ, 0, &MappedResource);
        {
            // Read the data
            counter = *(int*)MappedResource.pData;
        }
        context->Unmap(_staging_counter_b1d, 0);

        return counter;
    }

    void Buffer::ReadStgBuffer(ID3D11DeviceContext * context, void * ptr)
    {
        context->CopyResource(_staging_b1d, _b1d);

        D3D11_MAPPED_SUBRESOURCE subres;
        context->Map(_staging_b1d, 0, D3D11_MAP_READ, 0, &subres);
        memcpy(ptr, subres.pData, _size_in_bytes);
        context->Unmap(_staging_b1d, 0);
    }

}
