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

//--------------------------------------------------------------------------------------
// File: LineRender.h
//
// Helper to render 3d colored lines.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_LINE_RENDER_H
#define AMD_SDK_LINE_RENDER_H

// forward declarations
namespace DirectX
{
    struct BoundingBox;
}

namespace AMD
{

    class LineRender
    {
    public:

        LineRender();
        ~LineRender();

        void OnCreateDevice( ID3D11Device* pDevice, ID3D11DeviceContext* pImmediateContext, int nMaxLines );
        void OnDestroyDevice();

        void AddLine( const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const D3DCOLOR& color );
        void AddLines( const DirectX::XMFLOAT3* pPoints, int nNumLines, const D3DCOLOR& color );
        void AddBox( const DirectX::BoundingBox& box, const D3DCOLOR& color );

        void Render( const DirectX::XMMATRIX& viewProj );

    private:

        struct Vertex
        {
            DirectX::XMFLOAT3   m_Position;
            DWORD               m_Color;
        };

        struct ConstantBuffer
        {
            DirectX::XMMATRIX   m_ViewProj;
        };

        ID3D11DeviceContext*    m_pImmediateContext;
        ID3D11InputLayout*      m_pInputLayout;
        ID3D11VertexShader*     m_pVertexShader;
        ID3D11PixelShader*      m_pPixelShader;
        ID3D11Buffer*           m_pConstantBuffer;
        ID3D11Buffer*           m_pVertexBuffer;
        Vertex*                 m_pCPUCopy;
        int                     m_MaxLines;
        int                     m_NumLines;
    };

}

#endif
