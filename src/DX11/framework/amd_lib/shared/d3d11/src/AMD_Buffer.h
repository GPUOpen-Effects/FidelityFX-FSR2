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

#ifndef AMD_LIB_BUFFER_H
#define AMD_LIB_BUFFER_H

#include <d3d11.h>
#include <assert.h>

namespace AMD
{
    class Buffer
    {
    public:
        ID3D11Buffer              * _b1d;
        ID3D11ShaderResourceView  * _srv;
        ID3D11UnorderedAccessView * _uav;
        ID3D11Buffer              * _staging_b1d;
        ID3D11Buffer              * _staging_counter_b1d;

        uint                        _size_in_bytes;

        Buffer();
        ~Buffer();
        void    Release();
        HRESULT CreateBuffer( ID3D11Device * device,
            unsigned int                       uSizeInBytes,
            unsigned int                       uSizeOfStructure = 0,
            unsigned int                       uCPUAccess = 0,
            unsigned int                       uBindFlags = D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE                        usage = D3D11_USAGE_DEFAULT,
            DXGI_FORMAT                        SRV_Format = DXGI_FORMAT_UNKNOWN,
            DXGI_FORMAT                        UAV_Format = DXGI_FORMAT_UNKNOWN,
            unsigned int                       uUAVFlags  = 0,
            unsigned int                       uB1DFlags  = 0,
            void *                             data = NULL);

        int     UAVCounter( ID3D11DeviceContext * context );
        void    ReadStgBuffer( ID3D11DeviceContext * context, void * ptr);
    };
}

#endif // AMD_LIB_BUFFER_H
