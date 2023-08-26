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

#ifndef AMD_LIB_TEXTURE_2D_H
#define AMD_LIB_TEXTURE_2D_H

#include <d3d11.h>

// forward declarations
struct AGSContext;

namespace AMD
{
    class Texture2D
    {
    public:
        ID3D11Texture2D *           _t2d;
        ID3D11ShaderResourceView *  _srv;
        ID3D11ShaderResourceView *  _srv_cube;
        ID3D11RenderTargetView *    _rtv;
        ID3D11RenderTargetView *    _rtv_cube[6];
        ID3D11DepthStencilView *    _dsv;
        ID3D11DepthStencilView *    _dsv_cube[6];
        ID3D11DepthStencilView *    _dsv_ro;
        ID3D11UnorderedAccessView * _uav;

        unsigned int                _width;
        unsigned int                _height;
        unsigned int                _array;
        unsigned int                _mips;
        unsigned int                _sample;

        Texture2D();
        ~Texture2D();

        void Release();

        HRESULT CreateSurface( ID3D11Device * pDevice,
            unsigned int uWidth,
            unsigned int uHeight,
            unsigned int uSampleCount /*= 1*/,
            unsigned int uArraySize /*= 1*/,
            unsigned int uMipLevels /*= 1*/,
            DXGI_FORMAT T2D_Format /* = DXGI_FORMAT_UNKNOWN */,
            DXGI_FORMAT SRV_Format /* = DXGI_FORMAT_UNKNOWN */,
            DXGI_FORMAT RTV_Format /* = DXGI_FORMAT_UNKNOWN */,
            DXGI_FORMAT DSV_Format /* = DXGI_FORMAT_UNKNOWN */,
            DXGI_FORMAT UAV_Format /* = DXGI_FORMAT_UNKNOWN */,
            DXGI_FORMAT DSV_RO_Format /* = DXGI_FORMAT_UNKNOWN */,
            D3D11_USAGE usage /* = D3D11_USAGE_DEFAULT */,
            bool bCube,
            unsigned int pitch /* = 0 */,
            void * data /*= NULL */,
            AGSContext * agsContext /* = NULL*/,
            int cfxTransferType /*= (AGSAfrTransferType)0*/);
    };
}

#endif
