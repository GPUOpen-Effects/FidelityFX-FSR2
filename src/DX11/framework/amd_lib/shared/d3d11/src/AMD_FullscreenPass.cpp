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

#include <string>

#include "AMD_LIB.h"

#include "AMD_FullscreenPass.h"

#include "Shaders\inc\VS_FULLSCREEN.inc"
#include "Shaders\inc\VS_SCREENQUAD.inc"

#include "Shaders\inc\PS_FULLSCREEN.inc"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

namespace AMD
{
    HRESULT CreateFullscreenPass(ID3D11VertexShader** ppVS, ID3D11Device* pDevice)
    {
        if ( pDevice == NULL || ppVS == NULL )
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }
        return pDevice->CreateVertexShader(VS_FULLSCREEN_Data, sizeof(VS_FULLSCREEN_Data), NULL, ppVS);
    }

    HRESULT CreateScreenQuadPass(ID3D11VertexShader** ppVS, ID3D11Device* pDevice)
    {
        if ( pDevice == NULL || ppVS == NULL )
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }
        return pDevice->CreateVertexShader(VS_SCREENQUAD_Data, sizeof(VS_SCREENQUAD_Data), NULL, ppVS);
    }

    HRESULT CreateFullscreenPass(ID3D11PixelShader** ppPS, ID3D11Device* pDevice)
    {
        if ( pDevice == NULL || ppPS == NULL )
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_INVALIDARG;
        }
        return pDevice->CreatePixelShader(PS_FULLSCREEN_Data, sizeof(PS_FULLSCREEN_Data), NULL, ppPS);
    }

    HRESULT RenderFullscreenPass(
        ID3D11DeviceContext*        pDeviceContext,
        D3D11_VIEWPORT              Viewport,
        ID3D11VertexShader*         pVS,
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
        ID3D11RasterizerState *     pOutputRS)
    {
        return RenderFullscreenInstancedPass(pDeviceContext,
                                             Viewport,
                                             pVS, NULL, pPS,
                                             pScissor, uNumSR,
                                             ppCB, uNumCBs,
                                             ppSamplers, uNumSamplers,
                                             ppSRVs, uNumSRVs,
                                             ppRTVs, uNumRTVs,
                                             ppUAVs, uStartUAV, uNumUAVs,
                                             pDSV, pOutputDSS, uStencilRef,
                                             pOutputBS, pOutputRS, 1);
    }

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
        ID3D11UnorderedAccessView** ppUAVs,    unsigned int uStartUAV, unsigned int uNumUAVs,
        ID3D11DepthStencilView*     pDSV,
        ID3D11DepthStencilState*    pOutputDSS, unsigned int uStencilRef,
        ID3D11BlendState *          pOutputBS,
        ID3D11RasterizerState *     pOutputRS,
        unsigned int                instanceCount)
    {
        float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
        ID3D11ShaderResourceView*  pNullSRV[8]    = { NULL };
        ID3D11RenderTargetView*    pNullRTV[8]    = { NULL };
        ID3D11UnorderedAccessView* pNullUAV[8]    = { NULL };
        ID3D11Buffer*              pNullBuffer[8] = { NULL };
        uint NullStride[8] = { 0 };
        uint NullOffset[8] = { 0 };

        if ((pDeviceContext == NULL || (pVS == NULL && pPS == NULL) || (ppRTVs == NULL && pDSV == NULL && ppUAVs == NULL)))
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid pointer argument in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        pDeviceContext->OMSetDepthStencilState( pOutputDSS, uStencilRef );
        if (ppUAVs == NULL)
        {
            pDeviceContext->OMSetRenderTargets( uNumRTVs, (ID3D11RenderTargetView*const*)ppRTVs, pDSV );
        }
        else
        {
            pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews( uNumRTVs, (ID3D11RenderTargetView*const*)ppRTVs, pDSV, uStartUAV, uNumUAVs, ppUAVs, NULL);
        }
        pDeviceContext->OMSetBlendState(pOutputBS, white, 0xFFFFFFFF);

        pDeviceContext->RSSetViewports( 1, &Viewport );
        pDeviceContext->RSSetScissorRects(uNumSR, pScissor);
        pDeviceContext->RSSetState( pOutputRS );

        pDeviceContext->PSSetConstantBuffers( 0, uNumCBs, ppCB);
        pDeviceContext->PSSetShaderResources( 0, uNumSRVs, ppSRVs );
        pDeviceContext->PSSetSamplers( 0, uNumSamplers, ppSamplers );

        pDeviceContext->IASetInputLayout( NULL );
        pDeviceContext->IASetVertexBuffers( 0, AMD_ARRAY_SIZE(pNullBuffer), pNullBuffer, NullStride, NullOffset );
        pDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        pDeviceContext->VSSetShader( pVS, NULL, 0 );
        pDeviceContext->GSSetShader( pGS, NULL, 0 );
        pDeviceContext->PSSetShader(pPS, NULL, 0);

        pDeviceContext->Draw( 3 * instanceCount, 0 );

        // Unbind RTVs and SRVs back to NULL (otherwise D3D will throw warnings)
        if (ppUAVs == NULL)
        {
            pDeviceContext->OMSetRenderTargets( AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL );
        }
        else
        {
            pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews( uNumRTVs, pNullRTV, NULL, uStartUAV, uNumUAVs, pNullUAV, NULL);
        }

        pDeviceContext->PSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );

        return S_OK;
    }

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
        int                        nCount)
    {
        float white[] = {1, 1, 1, 1};
        ID3D11ShaderResourceView* pNullSRV[8]    = { NULL };
        ID3D11RenderTargetView*   pNullRTV[8]    = { NULL };
        ID3D11Buffer*             pNullBuffer[8] = { NULL };
        UINT NullStride[8] = { 0 };
        UINT NullOffset[8] = { 0 };

        if ((pDeviceContext == NULL || pVS == NULL || pPS == NULL || (ppRTVs == NULL && pDSV == NULL)))
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid pointer argument in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        pDeviceContext->OMSetDepthStencilState( pOutputDSS, uStencilRef );
        pDeviceContext->OMSetRenderTargets( uNumRTVs, (ID3D11RenderTargetView*const*)ppRTVs, pDSV );
        pDeviceContext->OMSetBlendState(pOutputBS, white, 0xFFFFFFFF);

        pDeviceContext->RSSetViewports( 1, &Viewport );
        pDeviceContext->RSSetScissorRects(uNumSR, pScissor);
        pDeviceContext->RSSetState( pOutputRS );

        pDeviceContext->PSSetConstantBuffers( 0, uNumCBs, ppCB);
        pDeviceContext->PSSetShaderResources( 0, uNumSRVs, ppSRVs );
        pDeviceContext->PSSetSamplers( 0, uNumSamplers, ppSamplers );
        
        pDeviceContext->IASetInputLayout( NULL );
        pDeviceContext->IASetVertexBuffers( 0, AMD_ARRAY_SIZE(pNullBuffer), pNullBuffer, NullStride, NullOffset );
        pDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        pDeviceContext->VSSetShader( pVS, NULL, 0 );
        pDeviceContext->PSSetShader(pPS, NULL, 0);

        pDeviceContext->Draw( 6 * nCount, 0 );

        // Unbind RTVs and SRVs back to NULL (otherwise D3D will throw warnings)
        pDeviceContext->OMSetRenderTargets( AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL );
        pDeviceContext->PSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );

        return S_OK;
    }
}
