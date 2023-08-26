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

#include "Shaders\inc\VS_UNIT_CUBE.inc"
#include "Shaders\inc\VS_CLIP_SPACE_CUBE.inc"
#include "Shaders\inc\PS_UNIT_CUBE.inc"

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

namespace AMD
{
    HRESULT CreateUnitCube(ID3D11VertexShader ** ppVS, ID3D11Device *pDevice)
    {
        if (pDevice == NULL || ppVS == NULL)
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        return pDevice->CreateVertexShader(VS_UNIT_CUBE_Data, sizeof(VS_UNIT_CUBE_Data), NULL, ppVS);
    }


    HRESULT CreateClipSpaceCube(ID3D11VertexShader** ppVS, ID3D11Device* pDevice)
    {
        if (pDevice == NULL || ppVS == NULL)
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        return pDevice->CreateVertexShader(VS_CLIP_SPACE_CUBE_Data, sizeof(VS_CLIP_SPACE_CUBE_Data), NULL, ppVS);
    }


    HRESULT CreateUnitCube(ID3D11PixelShader** ppPS, ID3D11Device* pDevice)
    {
        if (pDevice == NULL || ppPS == NULL)
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid Device or Vertex Shader pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        return pDevice->CreatePixelShader(PS_UNIT_CUBE_Data, sizeof(PS_UNIT_CUBE_Data), NULL, ppPS);
    }

    HRESULT RenderUnitCube(ID3D11DeviceContext*       pd3dContext,
        D3D11_VIEWPORT                                  VP,
        D3D11_RECT*                                     pSR,   unsigned int nSRCount,
        ID3D11RasterizerState *                         pRS,
        ID3D11BlendState *                              pBS,   const float bsFactor[],
        ID3D11DepthStencilState*                        pDSS,  unsigned int stencilRef,
        ID3D11VertexShader*                             pVS,
        ID3D11HullShader*                               pHS,
        ID3D11DomainShader*                             pDS,
        ID3D11GeometryShader*                           pGS,
        ID3D11PixelShader*                              pPS,
        ID3D11Buffer**                                  ppCB,  unsigned int nCBStart,  unsigned int nCBCount,
        ID3D11SamplerState**                            ppSS,  unsigned int nSSStart,  unsigned int nSSCount,
        ID3D11ShaderResourceView**                      ppSRV, unsigned int nSRVStart, unsigned int nSRVCount,
        ID3D11RenderTargetView**                        ppRTV, unsigned int nRTVCount,
        ID3D11DepthStencilView*                         pDSV)
    {
        // Useful common locals
        ID3D11RenderTargetView *   const pNullRTV[8]    = { 0 };
        ID3D11ShaderResourceView * const pNullSRV[128]  = { 0 };
        ID3D11Buffer *             const pNullBuffer[8] = { 0 };

        // Unbind anything that could be still bound on input or output
        // If this doesn't happen, DX Runtime will spam with warnings
        pd3dContext->OMSetRenderTargets( AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL );
        pd3dContext->CSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );
        pd3dContext->VSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );
        pd3dContext->HSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );
        pd3dContext->DSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );
        pd3dContext->GSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );
        pd3dContext->PSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );

        UINT NullStride[8] = { 0 };
        UINT NullOffset[8] = { 0 };

        if ( pd3dContext == NULL || pVS == NULL || (ppRTV == NULL && pDSV == NULL) )
        {
            AMD_OUTPUT_DEBUG_STRING("Invalid interface pointers in function %s\n", AMD_FUNCTION_NAME);
            return E_POINTER;
        }

        pd3dContext->VSSetShader( pVS, NULL, 0 );
        pd3dContext->HSSetShader( pHS, NULL, 0 );
        pd3dContext->DSSetShader( pDS, NULL, 0 );
        pd3dContext->GSSetShader( pGS, NULL, 0 );
        pd3dContext->PSSetShader( pPS, NULL, 0 );

        if (nSSCount)
        {
            pd3dContext->VSSetSamplers( nSSStart, nSSCount, ppSS );
            pd3dContext->HSSetSamplers( nSSStart, nSSCount, ppSS );
            pd3dContext->DSSetSamplers( nSSStart, nSSCount, ppSS );
            pd3dContext->GSSetSamplers( nSSStart, nSSCount, ppSS );
            pd3dContext->PSSetSamplers( nSSStart, nSSCount, ppSS );
        }

        if (nSRVCount)
        {
            pd3dContext->VSSetShaderResources( nSRVStart, nSRVCount, ppSRV );
            pd3dContext->HSSetShaderResources( nSRVStart, nSRVCount, ppSRV );
            pd3dContext->DSSetShaderResources( nSRVStart, nSRVCount, ppSRV );
            pd3dContext->GSSetShaderResources( nSRVStart, nSRVCount, ppSRV );
            pd3dContext->PSSetShaderResources( nSRVStart, nSRVCount, ppSRV );
        }

        if (nCBCount)
        {
            pd3dContext->VSSetConstantBuffers( nCBStart, nCBCount, ppCB );
            pd3dContext->HSSetConstantBuffers( nCBStart, nCBCount, ppCB );
            pd3dContext->DSSetConstantBuffers( nCBStart, nCBCount, ppCB );
            pd3dContext->GSSetConstantBuffers( nCBStart, nCBCount, ppCB );
            pd3dContext->PSSetConstantBuffers( nCBStart, nCBCount, ppCB );
        }

        pd3dContext->OMSetDepthStencilState( pDSS, stencilRef );
        pd3dContext->OMSetRenderTargets( nRTVCount, (ID3D11RenderTargetView*const*)ppRTV, pDSV );
        pd3dContext->OMSetBlendState(pBS, bsFactor, 0xFFFFFFFF);

        pd3dContext->RSSetViewports( 1, &VP );
        pd3dContext->RSSetScissorRects( nSRCount, pSR );
        pd3dContext->RSSetState( pRS );

        pd3dContext->IASetInputLayout( NULL );
        pd3dContext->IASetVertexBuffers( 0, AMD_ARRAY_SIZE(pNullBuffer), pNullBuffer, NullStride, NullOffset );
        pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

        pd3dContext->Draw( 36, 0 );

        // Unbind RTVs and SRVs back to NULL (otherwise D3D will throw warnings)
        pd3dContext->OMSetRenderTargets( AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL );
        pd3dContext->PSSetShaderResources( 0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV );

        return S_OK;
    }
}
