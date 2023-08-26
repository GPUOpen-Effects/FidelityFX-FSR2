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

#include <assert.h>

#include "AMD_LIB.h"

#ifndef AMD_LIB_MINIMAL
#include "amd_ags.h"
#endif

#pragma warning( disable : 4100 ) // disable unreferenced formal parameter warnings for /W4 builds

namespace AMD
{
    Texture2D::Texture2D()
        : _t2d(NULL)
        , _srv(NULL)
        , _srv_cube(NULL)
        , _rtv(NULL)
        , _dsv(NULL)
        , _dsv_ro(NULL)
        , _uav(NULL)
        , _width(0)
        , _height(0)
        , _array(0)
        , _mips(0)
        , _sample(0)
    {
        for (int i = 0; i < 6; i++)
        {
            _rtv_cube[i] = NULL;
            _dsv_cube[i] = NULL;
        }
    };

    Texture2D::~Texture2D()
    {
        Release();
    }

    void Texture2D::Release()
    {
        _width = 0;
        _height = 0;
        _array = 0;
        _mips = 0;
        _sample = 0;

        for (int i = 0; i < 6; i++)
        {
            AMD_SAFE_RELEASE(_rtv_cube[i]);
            AMD_SAFE_RELEASE(_dsv_cube[i]);
        }

        AMD_SAFE_RELEASE(_srv_cube);
        AMD_SAFE_RELEASE(_t2d);
        AMD_SAFE_RELEASE(_srv);
        AMD_SAFE_RELEASE(_rtv);
        AMD_SAFE_RELEASE(_dsv);
        AMD_SAFE_RELEASE(_uav);
        AMD_SAFE_RELEASE(_dsv_ro);
    }

    HRESULT Texture2D::CreateSurface(ID3D11Device * pDevice,
        unsigned int uWidth,
        unsigned int uHeight,
        unsigned int uSampleCount,
        unsigned int uArraySize,
        unsigned int uMipLevels,
        DXGI_FORMAT T2D_Format,
        DXGI_FORMAT SRV_Format,
        DXGI_FORMAT RTV_Format,
        DXGI_FORMAT DSV_Format,
        DXGI_FORMAT UAV_Format,
        DXGI_FORMAT DSV_RO_Format,
        D3D11_USAGE usage,
        bool bCube,
        unsigned int pitch,
        void * data,
        AGSContext * agsContext,
        int cfxTransferType)
    {
        HRESULT hr = S_OK;

        if (DXGI_FORMAT_UNKNOWN != T2D_Format)
        {
            D3D11_TEXTURE2D_DESC t2d_desc;
            memset(&t2d_desc, 0, sizeof(t2d_desc));

            if (NULL == _t2d)
            {
                t2d_desc.Width = uWidth;
                t2d_desc.Height = uHeight;
                t2d_desc.MipLevels = 1;
                t2d_desc.ArraySize = uArraySize;
                t2d_desc.SampleDesc.Count = uSampleCount;
                t2d_desc.SampleDesc.Quality = (uSampleCount > 1) ? (D3D11_STANDARD_MULTISAMPLE_PATTERN) : (0);
                t2d_desc.Usage = usage;
                t2d_desc.Format = T2D_Format;

                t2d_desc.BindFlags = 0;
                if (SRV_Format != DXGI_FORMAT_UNKNOWN)    { t2d_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;  }
                if (RTV_Format != DXGI_FORMAT_UNKNOWN)    { t2d_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;    }
                if (DSV_Format != DXGI_FORMAT_UNKNOWN)    { t2d_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;    }
                if (UAV_Format != DXGI_FORMAT_UNKNOWN)    { t2d_desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS; }
                if (DSV_RO_Format != DXGI_FORMAT_UNKNOWN) { t2d_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;    }

                if (bCube) { t2d_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; }

                D3D11_SUBRESOURCE_DATA subresource_data;
                memset(&subresource_data, 0, sizeof(subresource_data));
                subresource_data.pSysMem = data;
                subresource_data.SysMemPitch = pitch;

#ifndef AMD_LIB_MINIMAL
                if (agsContext != NULL)
                {
                    hr = agsDriverExtensions_CreateTexture2D(agsContext, &t2d_desc, data != NULL ? &subresource_data : NULL, &_t2d, (AGSAfrTransferType)cfxTransferType);
                }
                else
#endif
                {
                    hr = pDevice->CreateTexture2D(&t2d_desc, data != NULL ? &subresource_data : NULL, &_t2d);
                }

                assert(S_OK == hr);

                _width = uWidth;
                _height = uHeight;
                _array = uArraySize;
                _mips = uMipLevels;
                _sample = uSampleCount;
            }

            if (DXGI_FORMAT_UNKNOWN != SRV_Format)
            {
                AMD_SAFE_RELEASE(_srv);

                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
                memset(&srv_desc, 0, sizeof(srv_desc));
                srv_desc.Format = SRV_Format;

                if (uArraySize == 1)
                {
                    if (uSampleCount == 1)
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        srv_desc.Texture2D.MostDetailedMip = 0;
                        srv_desc.Texture2D.MipLevels = _mips;
                    }
                    else
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                        assert(_mips == 1);
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        srv_desc.Texture2DArray.MostDetailedMip = 0;
                        srv_desc.Texture2DArray.MipLevels = _mips;
                        srv_desc.Texture2DArray.ArraySize = uArraySize;
                        srv_desc.Texture2DArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                        srv_desc.Texture2DMSArray.ArraySize = uArraySize;
                        srv_desc.Texture2DMSArray.FirstArraySlice = 0;
                        assert(_mips == 1);
                    }
                }

                hr = pDevice->CreateShaderResourceView(_t2d, &srv_desc, &_srv);
                assert(S_OK == hr);
            }

            if (bCube && DXGI_FORMAT_UNKNOWN != SRV_Format)
            {
                AMD_SAFE_RELEASE(_srv_cube);

                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
                memset(&srv_desc, 0, sizeof(srv_desc));
                srv_desc.Format = SRV_Format;

                if (uArraySize == 6)
                {
                    if (uSampleCount == 1)
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                        srv_desc.TextureCube.MostDetailedMip = 0;
                        srv_desc.TextureCube.MipLevels = _mips;
                    }
                    else
                    {
                        assert(0);
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                        srv_desc.TextureCubeArray.MostDetailedMip = 0;
                        srv_desc.TextureCubeArray.MipLevels = _mips;
                        srv_desc.TextureCubeArray.NumCubes = uArraySize / 6;
                        srv_desc.TextureCubeArray.First2DArrayFace = 0;
                    }
                    else
                    {
                        assert(0);
                    }
                }

                hr = pDevice->CreateShaderResourceView(_t2d, &srv_desc, &_srv_cube);
                assert(S_OK == hr);
            }

            if (DXGI_FORMAT_UNKNOWN != RTV_Format)
            {
                AMD_SAFE_RELEASE(_rtv);

                D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
                memset(&rtv_desc, 0, sizeof(rtv_desc));
                rtv_desc.Format = RTV_Format;

                if (uArraySize == 1)
                {
                    if (uSampleCount == 1)
                    {
                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                        rtv_desc.Texture2D.MipSlice = 0;
                    }
                    else
                    {
                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                        rtv_desc.Texture2DArray.MipSlice = 0;
                        rtv_desc.Texture2DArray.ArraySize = uArraySize;
                        rtv_desc.Texture2DArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        rtv_desc.Texture2DMSArray.ArraySize = uArraySize;
                        rtv_desc.Texture2DMSArray.FirstArraySlice = 0;
                    }
                }


                hr = pDevice->CreateRenderTargetView(_t2d, &rtv_desc, &_rtv);
                assert(S_OK == hr);
            }

            if (bCube && DXGI_FORMAT_UNKNOWN != RTV_Format)
            {
                for (int i = 0; i < 6; i++)
                {
                    AMD_SAFE_RELEASE(_rtv_cube[i]);
                }

                D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
                memset(&rtv_desc, 0, sizeof(rtv_desc));
                rtv_desc.Format = RTV_Format;

                if (uArraySize == 6)
                {
                    if (uSampleCount == 1)
                    {

                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                        rtv_desc.Texture2DArray.MipSlice = 0;
                        rtv_desc.Texture2DArray.ArraySize = 1;

                        for (int i = 0; i < 6; i++)
                        {
                            rtv_desc.Texture2DArray.FirstArraySlice = i;
                            hr = pDevice->CreateRenderTargetView(_t2d, &rtv_desc, &_rtv_cube[i]);
                            assert(S_OK == hr);
                        }
                    }
                    else // cube textures can't have MS
                    {
                        assert(0);
                    }
                }
                else // cube arrays not yet supported
                {
                    assert(0);
                }
            }

            if (DXGI_FORMAT_UNKNOWN != DSV_Format)
            {
                AMD_SAFE_RELEASE(_dsv);

                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
                memset(&dsv_desc, 0, sizeof(dsv_desc));
                dsv_desc.Format = DSV_Format;

                if (uArraySize == 1)
                {
                    if (uSampleCount == 1)
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                        dsv_desc.Texture2D.MipSlice = 0;
                    }
                    else
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                        dsv_desc.Texture2DArray.MipSlice = 0;
                        dsv_desc.Texture2DArray.ArraySize = uArraySize;
                        dsv_desc.Texture2DArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        dsv_desc.Texture2DMSArray.ArraySize = uArraySize;
                        dsv_desc.Texture2DMSArray.FirstArraySlice = 0;
                    }
                }

                hr = pDevice->CreateDepthStencilView(_t2d, &dsv_desc, &_dsv);
                assert(S_OK == hr);
            }

            if (bCube && DXGI_FORMAT_UNKNOWN != DSV_Format)
            {
                for (int i = 0; i < 6; i++)
                {
                    AMD_SAFE_RELEASE(_dsv_cube[i]);
                }

                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
                memset(&dsv_desc, 0, sizeof(dsv_desc));
                dsv_desc.Format = DSV_Format;

                if (uArraySize == 6)
                {
                    if (uSampleCount == 1)
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                        dsv_desc.Texture2DArray.MipSlice = 0;
                        dsv_desc.Texture2DArray.ArraySize = 1;

                        for (int i = 0; i < 6; i++)
                        {
                            dsv_desc.Texture2DArray.FirstArraySlice = i;
                            hr = pDevice->CreateDepthStencilView(_t2d, &dsv_desc, &_dsv_cube[i]);
                            assert(S_OK == hr);
                        }
                    }
                    else // cube textures can't have MS
                    {
                        assert(0);
                    }
                }
                else // cube arrays not yet supported
                {
                    assert(0);
                }
            }

            if (DXGI_FORMAT_UNKNOWN != DSV_RO_Format)
            {
                AMD_SAFE_RELEASE(_dsv_ro);

                D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
                memset(&dsv_desc, 0, sizeof(dsv_desc));
                dsv_desc.Format = DSV_RO_Format;
                if (uArraySize == 1)
                {
                    if (uSampleCount == 1)
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                        dsv_desc.Texture2D.MipSlice = 0;
                    }
                    else
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                        dsv_desc.Texture2DArray.MipSlice = 0;
                        dsv_desc.Texture2DArray.ArraySize = uArraySize;
                        dsv_desc.Texture2DArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        dsv_desc.Texture2DMSArray.ArraySize = uArraySize;
                        dsv_desc.Texture2DMSArray.FirstArraySlice = 0;
                    }
                }
                dsv_desc.Flags = D3D11_DSV_READ_ONLY_DEPTH;

                hr = pDevice->CreateDepthStencilView(_t2d, &dsv_desc, &_dsv_ro);
                assert(S_OK == hr);
            }

            if (DXGI_FORMAT_UNKNOWN != UAV_Format)
            {
                AMD_SAFE_RELEASE(_uav);

                D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
                memset(&uav_desc, 0, sizeof(uav_desc));
                uav_desc.Format = UAV_Format;

                if (uArraySize == 1)
                {
                    if (uSampleCount == 1)
                    {
                        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                        uav_desc.Texture2D.MipSlice = 0;
                    }
                    else
                    {
                        assert(uSampleCount == 1); // UAV and Multisampling are incompatible
                    }
                }
                else
                {
                    if (uSampleCount == 1)
                    {
                        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                        uav_desc.Texture2DArray.MipSlice = 0;
                        uav_desc.Texture2DArray.ArraySize = uArraySize;
                        uav_desc.Texture2DArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        assert(uSampleCount == 1); // UAV and Multisampling are incompatible
                    }
                }

                hr = pDevice->CreateUnorderedAccessView(_t2d, &uav_desc, &_uav);
                assert(S_OK == hr);
            }
        }

        return hr;
    }
}
