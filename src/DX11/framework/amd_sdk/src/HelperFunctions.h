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
// File: HelperFunctions.h
//
// Various helper functions.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_HELPER_FUNCTIONS_H
#define AMD_SDK_HELPER_FUNCTIONS_H

namespace AMD
{
    // Cmd line params structure
    typedef struct CmdLineParams_t
    {
        D3D_DRIVER_TYPE DriverType;
        unsigned int uWidth;
        unsigned int uHeight;
        bool bCapture;
        WCHAR strCaptureFilename[256];
        int iExitFrame;
        bool bRenderHUD;
    } CmdLineParams;


//--------------------------------------------------------------------------------------
// Utility function that can optionally create the following objects:
// ID3D11Texture2D (only does this if the pointer is NULL)
// ID3D11ShaderResourceView
// ID3D11RenderTargetView
// ID3D11UnorderedAccessView
//--------------------------------------------------------------------------------------
HRESULT CreateSurface( ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppTextureSRV,
                       ID3D11RenderTargetView** ppTextureRTV, ID3D11UnorderedAccessView** ppTextureUAV,
                       DXGI_FORMAT Format, unsigned int uWidth, unsigned int uHeight, unsigned int uSampleCount );


//--------------------------------------------------------------------------------------
// Creates a depth stencil surface and optionally creates the following objects:
// ID3D11ShaderResourceView
// ID3D11DepthStencilView
//--------------------------------------------------------------------------------------
HRESULT CreateDepthStencilSurface( ID3D11Texture2D** ppDepthStencilTexture, ID3D11ShaderResourceView** ppDepthStencilSRV,
                                   ID3D11DepthStencilView** ppDepthStencilView, DXGI_FORMAT DSFormat, DXGI_FORMAT SRVFormat,
                                   unsigned int uWidth, unsigned int uHeight, unsigned int uSampleCount );


//--------------------------------------------------------------------------------------
// Capture a frame and dump it to disk
//--------------------------------------------------------------------------------------
void CaptureFrame( ID3D11Texture2D* pCaptureTexture, WCHAR* pszCaptureFileName );


//--------------------------------------------------------------------------------------
// Allows the app to render individual meshes of an sdkmesh
// and override the primitive topology (useful for tessellated rendering of SDK meshes )
//--------------------------------------------------------------------------------------
void RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh,
                D3D11_PRIMITIVE_TOPOLOGY PrimType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
                UINT uDiffuseSlot = INVALID_SAMPLER_SLOT, UINT uNormalSlot = INVALID_SAMPLER_SLOT,
                UINT uSpecularSlot = INVALID_SAMPLER_SLOT );


//--------------------------------------------------------------------------------------
// Debug function which copies a GPU buffer to a CPU readable buffer
//--------------------------------------------------------------------------------------
ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                      ID3D11Buffer* pBuffer );


//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file,
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint,
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const D3D10_SHADER_MACRO* pDefines );


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );


//--------------------------------------------------------------------------------------
// Helper function to parse the command line
//--------------------------------------------------------------------------------------
void ParseCommandLine( CmdLineParams* pCmdLineParams );


//--------------------------------------------------------------------------------------
// Check for file existance
//--------------------------------------------------------------------------------------
bool FileExists( WCHAR* pFileName );


//--------------------------------------------------------------------------------------
// Creates a cube with vertex format of
// 32x3     Position
// 32x3     Normal
// 16x2     UV
// 28 byte stride
//--------------------------------------------------------------------------------------
void CreateCube( float fSize, ID3D11Buffer** ppVertexBuffer, ID3D11Buffer** ppIndexBuffer );


//--------------------------------------------------------------------------------------
// Convert single-precision float to half-precision float,
// returned as a 16-bit unsigned value
//--------------------------------------------------------------------------------------
unsigned short ConvertF32ToF16(float fValueToConvert);


} // namespace AMD


#endif
