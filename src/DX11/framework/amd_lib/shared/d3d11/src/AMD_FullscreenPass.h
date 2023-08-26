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

#ifndef AMD_LIB_FULLSCREEN_PASS_H
#define AMD_LIB_FULLSCREEN_PASS_H

#include <d3d11.h>

namespace AMD
{
    extern "C++"
    {
        HRESULT CreateFullscreenPass(ID3D11VertexShader** ppVS, ID3D11Device* pDevice);
        HRESULT CreateScreenQuadPass(ID3D11VertexShader** ppVS, ID3D11Device* pDevice);
        HRESULT CreateFullscreenPass(ID3D11PixelShader** ppPS, ID3D11Device* pDevice);

        HRESULT RenderFullscreenPass(
            ID3D11DeviceContext*       pDeviceContext,
            D3D11_VIEWPORT             Viewport,
            ID3D11VertexShader*        pVS,
            ID3D11PixelShader*         pPS,
            D3D11_RECT*                pScissor,   unsigned int uNumSR,
            ID3D11Buffer**             ppCB,       unsigned int uNumCBs,
            ID3D11SamplerState**       ppSamplers, unsigned int uNumSamplers,
            ID3D11ShaderResourceView** ppSRVs,     unsigned int uNumSRVs,
            ID3D11RenderTargetView**   ppRTVs,     unsigned int uNumRTVs,
            ID3D11UnorderedAccessView**ppUAVs,     unsigned int uStartUAV, unsigned int uNumUAVs,
            ID3D11DepthStencilView*    pDSV,
            ID3D11DepthStencilState*   pOutputDSS, unsigned int uStencilRef,
            ID3D11BlendState *         pOutputBS,
            ID3D11RasterizerState *    pOutputRS);

        HRESULT RenderFullscreenInstancedPass(
            ID3D11DeviceContext*        pDeviceContext,
            D3D11_VIEWPORT              Viewport,
            ID3D11VertexShader*         pVS,
            ID3D11GeometryShader*       pGS,
            ID3D11PixelShader*          pPS,
            D3D11_RECT*                 pScissor,   unsigned int uNumSR,
            ID3D11Buffer**              ppCB,       unsigned int uNumCBs,
            ID3D11SamplerState**        ppSamplers, unsigned int uNumSamplers,
            ID3D11ShaderResourceView**  ppSRVs,     unsigned int uNumSRVs,
            ID3D11RenderTargetView**    ppRTVs,     unsigned int uNumRTVs,
            ID3D11UnorderedAccessView** ppUAVs,     unsigned int uStartUAV, unsigned int uNumUAVs,
            ID3D11DepthStencilView*     pDSV,
            ID3D11DepthStencilState*    pOutputDSS, unsigned int uStencilRef,
            ID3D11BlendState *          pOutputBS,
            ID3D11RasterizerState *     pOutputRS,
            unsigned int                instanceCount);

        HRESULT RenderFullscreenAlignedQuads(
            ID3D11DeviceContext*       pDeviceContext,
            D3D11_VIEWPORT             Viewport,
            ID3D11VertexShader*        pVS,
            ID3D11PixelShader*         pPS,
            D3D11_RECT*                pScissor,   unsigned int uNumSR,
            ID3D11Buffer**             ppCB,       unsigned int uNumCBs,
            ID3D11SamplerState**       ppSamplers, unsigned int uNumSamplers,
            ID3D11ShaderResourceView** ppSRVs,     unsigned int uNumSRVs,
            ID3D11RenderTargetView**   ppRTVs,     unsigned int uNumRTVs,
            ID3D11DepthStencilView*    pDSV,
            ID3D11DepthStencilState*   pOutputDSS, unsigned int uStencilRef,
            ID3D11BlendState *         pOutputBS,
            ID3D11RasterizerState *    pOutputRS,
            int                        nCount);
    }
}

#endif
