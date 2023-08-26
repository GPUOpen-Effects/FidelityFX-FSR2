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

#include "AMD_SaveRestoreState.h"

namespace AMD
{
    C_SaveRestore_IA::C_SaveRestore_IA(ID3D11DeviceContext * context)
        : m_IAPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
        , m_pInputLayout(NULL)
        , m_pIAIndexBuffer(NULL)
        , m_IAIndexBufferFormat(DXGI_FORMAT_UNKNOWN)
        , m_IAIndexBufferOffset(0)
        , m_pContext(context)
    {
        if (context == NULL) { return; }

        memset(m_pIAVertexBuffers, 0, sizeof(m_pIAVertexBuffers));
        memset(m_pIAVertexBuffersStrides, 0, sizeof(m_pIAVertexBuffersStrides));
        memset(m_pIAVertexBuffersOffsets, 0, sizeof(m_pIAVertexBuffersOffsets));

        m_pContext->IAGetInputLayout( &m_pInputLayout );
        m_pContext->IAGetIndexBuffer( &m_pIAIndexBuffer, &m_IAIndexBufferFormat, &m_IAIndexBufferOffset);
        m_pContext->IAGetPrimitiveTopology( &m_IAPrimitiveTopology );
        m_pContext->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, m_pIAVertexBuffers, m_pIAVertexBuffersStrides, m_pIAVertexBuffersOffsets);
    }

    C_SaveRestore_IA::~C_SaveRestore_IA()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->IASetInputLayout( m_pInputLayout );
        m_pContext->IASetIndexBuffer( m_pIAIndexBuffer, m_IAIndexBufferFormat, m_IAIndexBufferOffset );
        m_pContext->IASetPrimitiveTopology( m_IAPrimitiveTopology );
        m_pContext->IASetVertexBuffers( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, m_pIAVertexBuffers, m_pIAVertexBuffersStrides, m_pIAVertexBuffersOffsets );

        if (m_pInputLayout) {m_pInputLayout->Release(); m_pInputLayout = NULL;}
        if (m_pIAIndexBuffer) {m_pIAIndexBuffer->Release(); m_pIAIndexBuffer = NULL;}
        for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {
            if (m_pIAVertexBuffers[i]) {m_pIAVertexBuffers[i]->Release(); m_pIAVertexBuffers[i] = NULL;}
        }

        m_pContext = NULL;
    }

    C_SaveRestore_RS::C_SaveRestore_RS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pRSState(0)
        , m_RSRectCount(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
        , m_RSViewportCount(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
    {
        if (context == NULL) { return; }

        memset(m_RSRect, 0, sizeof(m_RSRect));
        memset(m_RSViewport, 0, sizeof(m_RSViewport));

        m_pContext->RSGetState(&m_pRSState);
        m_pContext->RSGetScissorRects(&m_RSRectCount, m_RSRect);
        m_pContext->RSGetViewports(&m_RSViewportCount, m_RSViewport);
    }

    C_SaveRestore_RS::~C_SaveRestore_RS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->RSSetState(m_pRSState);
        m_pContext->RSSetScissorRects(m_RSRectCount, m_RSRect);
        m_pContext->RSSetViewports(m_RSViewportCount, m_RSViewport);

        if (m_pRSState) {m_pRSState->Release(); m_pRSState = NULL;}
        m_RSRectCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        m_RSViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        m_pContext = NULL;
    }

    C_SaveRestore_OM::C_SaveRestore_OM(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pOMBlendState(0)
        , m_OMSampleMask(0)
        , m_pOMDepthStencilState(0)
        , m_OMStencilRef(0)
        , m_pOMDSV(0)
    {
        if (context == NULL) { return; }

        memset(m_OMBlendFactor, 0, sizeof(m_OMBlendFactor));
        memset(m_pOMRTV, 0, sizeof(m_pOMRTV));

        m_pContext->OMGetBlendState(&m_pOMBlendState, m_OMBlendFactor, &m_OMSampleMask);
        m_pContext->OMGetDepthStencilState(&m_pOMDepthStencilState, &m_OMStencilRef);
        m_pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_pOMRTV, &m_pOMDSV);
    }

    C_SaveRestore_OM::~C_SaveRestore_OM()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->OMSetBlendState(m_pOMBlendState, m_OMBlendFactor, m_OMSampleMask);
        m_pContext->OMSetDepthStencilState(m_pOMDepthStencilState, m_OMStencilRef);
        m_pContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_pOMRTV, m_pOMDSV);

        if (m_pOMBlendState) {m_pOMBlendState->Release(); m_pOMBlendState = NULL;}
        if (m_pOMDepthStencilState) {m_pOMDepthStencilState->Release(); m_pOMDepthStencilState = NULL;}
        if (m_pOMDSV) {m_pOMDSV->Release(); m_pOMDSV = NULL;}
        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++ )
        {  if (m_pOMRTV[i]) {m_pOMRTV[i]->Release(); m_pOMRTV[i] = NULL;} }

        m_pContext = NULL;
    }

    C_SaveRestore_VS::C_SaveRestore_VS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pVSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pVSSamplers, 0, sizeof(m_pVSSamplers));
        memset(m_pVSConstantBuffer, 0, sizeof(m_pVSConstantBuffer));
        memset(m_pVSSRV, 0, sizeof(m_pVSSRV));

        m_pContext->VSGetShader(&m_pVSShader, NULL, NULL);
        m_pContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pVSSamplers);
        m_pContext->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pVSConstantBuffer);
        m_pContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pVSSRV);
    }

    C_SaveRestore_VS::~C_SaveRestore_VS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->VSSetShader(m_pVSShader, NULL, NULL);
        m_pContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pVSSamplers);
        m_pContext->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pVSConstantBuffer);
        m_pContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pVSSRV);

        if (m_pVSShader) {m_pVSShader->Release(); m_pVSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pVSSamplers[i]) {m_pVSSamplers[i]->Release(); m_pVSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pVSConstantBuffer[i]) {m_pVSConstantBuffer[i]->Release(); m_pVSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pVSSRV[i]) {m_pVSSRV[i]->Release(); m_pVSSRV[i] = NULL;} }

        m_pContext = NULL;
    }

    C_SaveRestore_HS::C_SaveRestore_HS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pHSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pHSSamplers, 0, sizeof(m_pHSSamplers));
        memset(m_pHSConstantBuffer, 0, sizeof(m_pHSConstantBuffer));
        memset(m_pHSSRV, 0, sizeof(m_pHSSRV));

        m_pContext->HSGetShader(&m_pHSShader, NULL, NULL);
        m_pContext->HSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pHSSamplers);
        m_pContext->HSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pHSConstantBuffer);
        m_pContext->HSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pHSSRV);
    }

    C_SaveRestore_HS::~C_SaveRestore_HS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->HSSetShader(m_pHSShader, NULL, NULL);
        m_pContext->HSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pHSSamplers);
        m_pContext->HSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pHSConstantBuffer);
        m_pContext->HSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pHSSRV);

        if (m_pHSShader) {m_pHSShader->Release(); m_pHSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pHSSamplers[i]) {m_pHSSamplers[i]->Release(); m_pHSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pHSConstantBuffer[i]) {m_pHSConstantBuffer[i]->Release(); m_pHSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pHSSRV[i]) {m_pHSSRV[i]->Release(); m_pHSSRV[i] = NULL;} }

        m_pContext = NULL;
    }

    C_SaveRestore_DS::C_SaveRestore_DS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pDSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pDSSamplers, 0, sizeof(m_pDSSamplers));
        memset(m_pDSConstantBuffer, 0, sizeof(m_pDSConstantBuffer));
        memset(m_pDSSRV, 0, sizeof(m_pDSSRV));

        m_pContext->DSGetShader(&m_pDSShader, NULL, NULL);
        m_pContext->DSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pDSSamplers);
        m_pContext->DSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pDSConstantBuffer);
        m_pContext->DSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pDSSRV);
    }

    C_SaveRestore_DS::~C_SaveRestore_DS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->DSSetShader(m_pDSShader, NULL, NULL);
        m_pContext->DSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pDSSamplers);
        m_pContext->DSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pDSConstantBuffer);
        m_pContext->DSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pDSSRV);

        if (m_pDSShader) {m_pDSShader->Release(); m_pDSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pDSSamplers[i]) {m_pDSSamplers[i]->Release(); m_pDSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pDSConstantBuffer[i]) {m_pDSConstantBuffer[i]->Release(); m_pDSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pDSSRV[i]) {m_pDSSRV[i]->Release(); m_pDSSRV[i] = NULL;} }

        m_pContext = NULL;
    }

    C_SaveRestore_GS::C_SaveRestore_GS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pGSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pGSSamplers, 0, sizeof(m_pGSSamplers));
        memset(m_pGSConstantBuffer, 0, sizeof(m_pGSConstantBuffer));
        memset(m_pGSSRV, 0, sizeof(m_pGSSRV));

        m_pContext->GSGetShader(&m_pGSShader, NULL, NULL);
        m_pContext->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pGSSamplers);
        m_pContext->GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pGSConstantBuffer);
        m_pContext->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pGSSRV);
    }

    C_SaveRestore_GS::~C_SaveRestore_GS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->GSSetShader(m_pGSShader, NULL, NULL);
        m_pContext->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pGSSamplers);
        m_pContext->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pGSConstantBuffer);
        m_pContext->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pGSSRV);

        if (m_pGSShader) {m_pGSShader->Release(); m_pGSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pGSSamplers[i]) {m_pGSSamplers[i]->Release(); m_pGSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pGSConstantBuffer[i]) {m_pGSConstantBuffer[i]->Release(); m_pGSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pGSSRV[i]) {m_pGSSRV[i]->Release(); m_pGSSRV[i] = NULL;} }

        m_pContext = NULL;
    }

    C_SaveRestore_PS::C_SaveRestore_PS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pPSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pPSSamplers, 0, sizeof(m_pPSSamplers));
        memset(m_pPSConstantBuffer, 0, sizeof(m_pPSConstantBuffer));
        memset(m_pPSSRV, 0, sizeof(m_pPSSRV));

        m_pContext->PSGetShader(&m_pPSShader, NULL, NULL);
        m_pContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pPSSamplers);
        m_pContext->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pPSConstantBuffer);
        m_pContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pPSSRV);
    }

    C_SaveRestore_PS::~C_SaveRestore_PS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->PSSetShader(m_pPSShader, NULL, NULL);
        m_pContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pPSSamplers);
        m_pContext->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pPSConstantBuffer);
        m_pContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pPSSRV);

        if (m_pPSShader) {m_pPSShader->Release(); m_pPSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pPSSamplers[i]) {m_pPSSamplers[i]->Release(); m_pPSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pPSConstantBuffer[i]) {m_pPSConstantBuffer[i]->Release(); m_pPSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pPSSRV[i]) {m_pPSSRV[i]->Release(); m_pPSSRV[i] = NULL;} }

        m_pContext= NULL;
    }

    C_SaveRestore_CS::C_SaveRestore_CS(ID3D11DeviceContext * context)
        : m_pContext(context)
        , m_pCSShader(0)
    {
        if (context == NULL) { return; }

        memset(m_pCSSamplers, 0, sizeof(m_pCSSamplers));
        memset(m_pCSConstantBuffer, 0, sizeof(m_pCSConstantBuffer));
        memset(m_pCSSRV, 0, sizeof(m_pCSSRV));
        memset(m_pCSUAV, 0, sizeof(m_pCSUAV));

        m_pContext->CSGetShader(&m_pCSShader, NULL, NULL);
        m_pContext->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pCSSamplers);
        m_pContext->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pCSConstantBuffer);
        m_pContext->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pCSSRV);
        m_pContext->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, m_pCSUAV);
    }
    C_SaveRestore_CS::~C_SaveRestore_CS()
    {
        if (m_pContext == NULL) { return; }

        m_pContext->CSSetShader(m_pCSShader, NULL, NULL);
        m_pContext->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_pCSSamplers);
        m_pContext->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, m_pCSConstantBuffer);
        m_pContext->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, m_pCSSRV);
        m_pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, m_pCSUAV, NULL);

        if (m_pCSShader) {m_pCSShader->Release(); m_pCSShader = NULL;}

        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++ )
        {  if (m_pCSSamplers[i]) {m_pCSSamplers[i]->Release(); m_pCSSamplers[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++ )
        {  if (m_pCSConstantBuffer[i]) {m_pCSConstantBuffer[i]->Release(); m_pCSConstantBuffer[i] = NULL;} }

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++ )
        {  if (m_pCSSRV[i]) {m_pCSSRV[i]->Release(); m_pCSSRV[i] = NULL;} }

        for (int i = 0; i < D3D11_PS_CS_UAV_REGISTER_COUNT; i++ )
        {  if (m_pCSUAV[i]) {m_pCSUAV[i]->Release(); m_pCSUAV[i] = NULL;} }

        m_pContext = NULL;
    }
}
