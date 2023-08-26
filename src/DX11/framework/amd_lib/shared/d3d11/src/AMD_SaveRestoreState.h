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

#ifndef AMD_LIB_SAVE_RESTORE_STATE_H
#define AMD_LIB_SAVE_RESTORE_STATE_H

#include <d3d11.h>

namespace AMD
{
    /*===========================================================================
    INPUT ASSEMBLY STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_IA
    {
        ID3D11InputLayout * m_pInputLayout;
        ID3D11Buffer* m_pIAIndexBuffer;
        DXGI_FORMAT m_IAIndexBufferFormat;
        UINT m_IAIndexBufferOffset;
        UINT m_pIAVertexBuffersOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        UINT m_pIAVertexBuffersStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        D3D11_PRIMITIVE_TOPOLOGY m_IAPrimitiveTopology;
        ID3D11Buffer* m_pIAVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_IA(ID3D11DeviceContext * context);
        ~C_SaveRestore_IA();
    };

    /*===========================================================================
    RASTERIZER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_RS
    {
        ID3D11RasterizerState * m_pRSState;
        D3D11_RECT m_RSRect[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT m_RSRectCount;
        UINT m_RSViewportCount;
        D3D11_VIEWPORT m_RSViewport[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_RS(ID3D11DeviceContext * context);
        ~C_SaveRestore_RS();
    };

    /*===========================================================================
    OUTPUT MERGER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_OM
    {
        ID3D11BlendState* m_pOMBlendState;
        float m_OMBlendFactor[4];
        UINT m_OMSampleMask;
        ID3D11DepthStencilState* m_pOMDepthStencilState;
        UINT m_OMStencilRef;
        ID3D11DepthStencilView* m_pOMDSV;
        ID3D11RenderTargetView* m_pOMRTV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_OM(ID3D11DeviceContext * context);
        ~C_SaveRestore_OM();
    };

    /*===========================================================================
    VERTEX SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_VS
    {
        ID3D11VertexShader* m_pVSShader;
        ID3D11SamplerState* m_pVSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer* m_pVSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pVSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_VS(ID3D11DeviceContext * context);
        ~C_SaveRestore_VS();
    };

    /*===========================================================================
    HULL SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_HS
    {
        ID3D11HullShader* m_pHSShader;
        ID3D11SamplerState* m_pHSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer* m_pHSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pHSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_HS(ID3D11DeviceContext * context);
        ~C_SaveRestore_HS();
    };

    /*===========================================================================
    DOMAIN SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_DS
    {
        ID3D11DomainShader* m_pDSShader;
        ID3D11SamplerState* m_pDSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer* m_pDSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pDSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_DS(ID3D11DeviceContext * context);
        ~C_SaveRestore_DS();
    };

    /*===========================================================================
    GEOMETRY SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_GS
    {
        ID3D11GeometryShader* m_pGSShader;
        ID3D11SamplerState* m_pGSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer* m_pGSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pGSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_GS(ID3D11DeviceContext * context);
        ~C_SaveRestore_GS();
    };

    /*===========================================================================
    PIXEL SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_PS
    {

        ID3D11PixelShader*        m_pPSShader;
        ID3D11SamplerState*       m_pPSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer*             m_pPSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pPSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11DeviceContext *     m_pContext;

    public:
        C_SaveRestore_PS(ID3D11DeviceContext * context);
        ~C_SaveRestore_PS();
    };

    /*===========================================================================
    COMPUTE SHADER STATE GUARD
    ===========================================================================*/
    class C_SaveRestore_CS
    {
        ID3D11ComputeShader* m_pCSShader;
        ID3D11SamplerState* m_pCSSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer* m_pCSConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11ShaderResourceView* m_pCSSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11UnorderedAccessView* m_pCSUAV[D3D11_PS_CS_UAV_REGISTER_COUNT];
        ID3D11DeviceContext * m_pContext;

    public:
        C_SaveRestore_CS(ID3D11DeviceContext * context);
        ~C_SaveRestore_CS();
    };
};

#endif
