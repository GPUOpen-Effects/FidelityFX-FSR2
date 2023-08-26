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
// File: HelperFunctions.cpp
//
// Various helper functions useful in many samples
//--------------------------------------------------------------------------------------


// DXUT helper code
#include "DXUT.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include "HelperFunctions.h"


//--------------------------------------------------------------------------------------
// Utility function that can optionally create the following objects:
// ID3D11Texture2D (only does this if the pointer is NULL)
// ID3D11ShaderResourceView
// ID3D11RenderTargetView
// ID3D11UnorderedAccessView
//--------------------------------------------------------------------------------------
HRESULT AMD::CreateSurface( ID3D11Texture2D** ppTexture, ID3D11ShaderResourceView** ppTextureSRV,
    ID3D11RenderTargetView** ppTextureRTV, ID3D11UnorderedAccessView** ppTextureUAV,
    DXGI_FORMAT Format, unsigned int uWidth, unsigned int uHeight, unsigned int uSampleCount )
{
    HRESULT hr = D3D_OK;

    if (ppTexture)
    {
        D3D11_TEXTURE2D_DESC Desc;
        ZeroMemory( &Desc, sizeof( Desc ) );

        if (NULL == *ppTexture)
        {
            Desc.Width = uWidth;
            Desc.Height = uHeight;
            Desc.MipLevels = 1;
            Desc.ArraySize = 1;
            Desc.Format = Format;
            Desc.SampleDesc.Count = uSampleCount;
            Desc.SampleDesc.Quality = 0;//( uSampleCount > 1 ) ? ( D3D10_STANDARD_MULTISAMPLE_PATTERN ) : ( 0 );
            Desc.Usage = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (ppTextureUAV)
            {
                Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            }
            hr = DXUTGetD3D11Device()->CreateTexture2D( &Desc, NULL, ppTexture );
            assert( D3D_OK == hr );
        }

        if (ppTextureSRV)
        {
            SAFE_RELEASE( *ppTextureSRV );

            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            ZeroMemory( &SRVDesc, sizeof( D3D11_SHADER_RESOURCE_VIEW_DESC ) );
            SRVDesc.Format = Format;
            SRVDesc.ViewDimension = (uSampleCount > 1) ? (D3D11_SRV_DIMENSION_TEXTURE2DMS) : (D3D11_SRV_DIMENSION_TEXTURE2D);
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.MipLevels = 1;
            hr = DXUTGetD3D11Device()->CreateShaderResourceView( *ppTexture, &SRVDesc, ppTextureSRV );
            assert( D3D_OK == hr );
        }

        if (ppTextureRTV)
        {
            SAFE_RELEASE( *ppTextureRTV );

            D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
            ZeroMemory( &RTVDesc, sizeof( D3D11_RENDER_TARGET_VIEW_DESC ) );
            RTVDesc.Format = Format;
            RTVDesc.ViewDimension = (uSampleCount > 1) ? (D3D11_RTV_DIMENSION_TEXTURE2DMS) : (D3D11_RTV_DIMENSION_TEXTURE2D);
            RTVDesc.Texture2D.MipSlice = 0;
            hr = DXUTGetD3D11Device()->CreateRenderTargetView( *ppTexture, &RTVDesc, ppTextureRTV );
            assert( D3D_OK == hr );
        }

        if (ppTextureUAV)
        {
            SAFE_RELEASE( *ppTextureUAV );

            D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
            ZeroMemory( &UAVDesc, sizeof( D3D11_UNORDERED_ACCESS_VIEW_DESC ) );
            UAVDesc.Format = Format;
            UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            UAVDesc.Buffer.FirstElement = 0;
            UAVDesc.Buffer.NumElements = Desc.Width * Desc.Height;
            hr = DXUTGetD3D11Device()->CreateUnorderedAccessView( *ppTexture, &UAVDesc, ppTextureUAV );
            assert( D3D_OK == hr );
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Creates a depth stencil surface and optionally creates the following objects:
// ID3D11ShaderResourceView
// ID3D11DepthStencilView
//--------------------------------------------------------------------------------------
HRESULT AMD::CreateDepthStencilSurface( ID3D11Texture2D** ppDepthStencilTexture, ID3D11ShaderResourceView** ppDepthStencilSRV,
    ID3D11DepthStencilView** ppDepthStencilView, DXGI_FORMAT DSFormat, DXGI_FORMAT SRVFormat,
    unsigned int uWidth, unsigned int uHeight, unsigned int uSampleCount )
{
    HRESULT hr = D3D_OK;
    DXGI_FORMAT TextureFormat;

    if (ppDepthStencilTexture)
    {
        switch (DSFormat)
        {
        case DXGI_FORMAT_D32_FLOAT:
            TextureFormat = DXGI_FORMAT_R32_TYPELESS;
            break;

        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            TextureFormat = DXGI_FORMAT_R24G8_TYPELESS;
            break;

        case DXGI_FORMAT_D16_UNORM:
            TextureFormat = DXGI_FORMAT_R16_TYPELESS;
            break;

        default:
            TextureFormat = DXGI_FORMAT_UNKNOWN;
            break;
        }
        assert( TextureFormat != DXGI_FORMAT_UNKNOWN );

        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC descDepth;
        descDepth.Width = uWidth;
        descDepth.Height = uHeight;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = TextureFormat;
        descDepth.SampleDesc.Count = uSampleCount;
        descDepth.SampleDesc.Quality = 0;//( uSampleCount > 1 ) ? ( D3D10_STANDARD_MULTISAMPLE_PATTERN ) : ( 0 );
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        if (NULL != ppDepthStencilSRV)
        {
            if ((descDepth.SampleDesc.Count == 1) || (DXUTGetD3D11DeviceFeatureLevel() >= D3D_FEATURE_LEVEL_10_1))
            {
                descDepth.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            }
        }

        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = DXUTGetD3D11Device()->CreateTexture2D( &descDepth, NULL, ppDepthStencilTexture );
        assert( D3D_OK == hr );

        if (NULL != ppDepthStencilView)
        {
            // Create the depth stencil view
            D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
            descDSV.Format = DSFormat;
            if (descDepth.SampleDesc.Count > 1)
            {
                descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            }
            descDSV.Texture2D.MipSlice = 0;
            descDSV.Flags = 0;

            hr = DXUTGetD3D11Device()->CreateDepthStencilView( (ID3D11Resource*)*ppDepthStencilTexture, &descDSV, ppDepthStencilView );
            assert( D3D_OK == hr );
        }

        if (NULL != ppDepthStencilSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRDesc;
            SRDesc.Format = SRVFormat;
            SRDesc.ViewDimension = (uSampleCount > 1) ? (D3D11_SRV_DIMENSION_TEXTURE2DMS) : (D3D11_SRV_DIMENSION_TEXTURE2D);
            SRDesc.Texture2D.MostDetailedMip = 0;
            SRDesc.Texture2D.MipLevels = 1;

            hr = DXUTGetD3D11Device()->CreateShaderResourceView( (ID3D11Resource*)*ppDepthStencilTexture, &SRDesc, ppDepthStencilSRV );
            assert( D3D_OK == hr );
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Captures a frame and dumps it to disk
//--------------------------------------------------------------------------------------
void AMD::CaptureFrame( ID3D11Texture2D* pCaptureTexture, WCHAR* pszCaptureFileName )
{
    assert( NULL != pCaptureTexture );
    assert( NULL != pszCaptureFileName );

    // Retrieve RT resource
    ID3D11Resource *pRTResource;
    DXUTGetD3D11RenderTargetView()->GetResource( &pRTResource );

    // Retrieve a Texture2D interface from resource
    ID3D11Texture2D* RTTexture;
    pRTResource->QueryInterface( __uuidof(ID3D11Texture2D), (LPVOID*)&RTTexture );

    // Check if RT is multisampled or not
    D3D11_TEXTURE2D_DESC TexDesc;
    RTTexture->GetDesc( &TexDesc );
    if (TexDesc.SampleDesc.Count > 1)
    {
        // RT is multisampled, need resolving before dumping to disk

        // Create single-sample RT of the same type and dimensions
        DXGI_SAMPLE_DESC SingleSample = { 1, 0 };
        TexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        TexDesc.MipLevels = 1;
        TexDesc.Usage = D3D11_USAGE_DEFAULT;
        TexDesc.CPUAccessFlags = 0;
        TexDesc.BindFlags = 0;
        TexDesc.SampleDesc = SingleSample;

        ID3D11Texture2D *pSingleSampleTexture;
        DXUTGetD3D11Device()->CreateTexture2D( &TexDesc, NULL, &pSingleSampleTexture );

        DXUTGetD3D11DeviceContext()->ResolveSubresource( pSingleSampleTexture, 0, RTTexture, 0, TexDesc.Format );

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource( pCaptureTexture, pSingleSampleTexture );

        DXUTSaveTextureToFile( DXUTGetD3D11DeviceContext(), pCaptureTexture, false, pszCaptureFileName );

        SAFE_RELEASE( pSingleSampleTexture );
    }
    else
    {
        // Single sample case

        // Copy RT into STAGING texture
        DXUTGetD3D11DeviceContext()->CopyResource( pCaptureTexture, pRTResource );

        DXUTSaveTextureToFile( DXUTGetD3D11DeviceContext(), pCaptureTexture, false, pszCaptureFileName );
    }

    SAFE_RELEASE( RTTexture );

    SAFE_RELEASE( pRTResource );
}


//--------------------------------------------------------------------------------------
// Allows the app to render individual meshes of an sdkmesh
// and override the primitive topology (useful for tessellated rendering of SDK meshes )
//--------------------------------------------------------------------------------------
void AMD::RenderMesh( CDXUTSDKMesh* pDXUTMesh, UINT uMesh, D3D11_PRIMITIVE_TOPOLOGY PrimType,
    UINT uDiffuseSlot, UINT uNormalSlot, UINT uSpecularSlot )
{
#define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

    assert( NULL != pDXUTMesh );

    SDKMESH_MESH* pMesh = pDXUTMesh->GetMesh( uMesh );

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if (pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS)
    {
        return;
    }

    for (UINT64 i = 0; i < pMesh->NumVertexBuffers; i++)
    {
        pVB[i] = pDXUTMesh->GetVB11( uMesh, (UINT)i );
        Strides[i] = pDXUTMesh->GetVertexStride( uMesh, (UINT)i );
        Offsets[i] = 0;
    }

    ID3D11Buffer* pIB = pDXUTMesh->GetIB11( pMesh->IndexBuffer );
    DXGI_FORMAT ibFormat = pDXUTMesh->GetIBFormat11( pMesh->IndexBuffer );

    DXUTGetD3D11DeviceContext()->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    DXUTGetD3D11DeviceContext()->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;

    for (UINT uSubset = 0; uSubset < pMesh->NumSubsets; uSubset++)
    {
        pSubset = pDXUTMesh->GetSubset( uMesh, uSubset );

        if (D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED == PrimType)
        {
            PrimType = pDXUTMesh->GetPrimitiveType11( (SDKMESH_PRIMITIVE_TYPE)pSubset->PrimitiveType );
        }

        DXUTGetD3D11DeviceContext()->IASetPrimitiveTopology( PrimType );

        pMat = pDXUTMesh->GetMaterial( pSubset->MaterialID );
        if (uDiffuseSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pDiffuseRV11 ))
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uDiffuseSlot, 1, &pMat->pDiffuseRV11 );
        }

        if (uNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ))
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uNormalSlot, 1, &pMat->pNormalRV11 );
        }

        if (uSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ))
        {
            DXUTGetD3D11DeviceContext()->PSSetShaderResources( uSpecularSlot, 1, &pMat->pSpecularRV11 );
        }

        UINT IndexCount = (UINT)pSubset->IndexCount;
        UINT IndexStart = (UINT)pSubset->IndexStart;
        UINT VertexStart = (UINT)pSubset->VertexStart;

        DXUTGetD3D11DeviceContext()->DrawIndexed( IndexCount, IndexStart, VertexStart );
    }
}


//--------------------------------------------------------------------------------------
// Debug function which copies a GPU buffer to a CPU readable buffer
//--------------------------------------------------------------------------------------
ID3D11Buffer* AMD::CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof( desc ) );
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    pDevice->CreateBuffer( &desc, NULL, &debugbuf );

    pd3dImmediateContext->CopyResource( debugbuf, pBuffer );

    return debugbuf;
}


//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file,
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT AMD::CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint,
    LPCSTR szShaderModel, ID3DBlob** ppBlobOut, const D3D10_SHADER_MACRO* pDefines )
{
    HRESULT   hr = E_FAIL;
    ID3DBlob* error = NULL;

    hr = D3DCompileFromFile( szFileName, pDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE, szEntryPoint, szShaderModel, 0, 0, ppBlobOut, &error );
    if (hr != S_OK || ppBlobOut == NULL || *ppBlobOut == NULL)
    {
        if (error != NULL)
        {
            OutputDebugStringA( (const char *)error->GetBufferPointer() );
        }
        else
        {
            char errorString[128];
            sprintf_s( errorString, "Unknown error compiling shader: (%s, %s)\n", szEntryPoint, szShaderModel );
            OutputDebugStringA( errorString );
        }
    }
    SAFE_RELEASE( error );
    return hr;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval
//--------------------------------------------------------------------------------------
bool AMD::IsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
    int nArgLen = (int)wcslen( strArg );
    int nCmdLen = (int)wcslen( strCmdLine );

    if (nCmdLen >= nArgLen &&
        _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 &&
        (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' || strCmdLine[nArgLen] == L'='))
    {
        strCmdLine += nArgLen;
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Helper function for command line retrieval.  Updates strCmdLine and strFlag
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool AMD::GetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
    if (*strCmdLine == L':' || *strCmdLine == L'=')
    {
        strCmdLine++; // Skip ':'

        // Place NULL terminator in strFlag after current token
        wcscpy_s( strFlag, 256, strCmdLine );
        WCHAR* strSpace = strFlag;
        while (*strSpace && (*strSpace > L' '))
        {
            strSpace++;
        }
        *strSpace = 0;

        // Update strCmdLine
        strCmdLine += wcslen( strFlag );
        return true;
    }
    else
    {
        strFlag[0] = 0;
        return false;
    }
}


//--------------------------------------------------------------------------------------
// Helper function to parse the command line
//--------------------------------------------------------------------------------------
void AMD::ParseCommandLine( CmdLineParams* pCmdLineParams )
{
    assert( NULL != pCmdLineParams );

    // set some defaults
    pCmdLineParams->DriverType = D3D_DRIVER_TYPE_HARDWARE;
    pCmdLineParams->uWidth = 1929;
    pCmdLineParams->uHeight = 1200;
    pCmdLineParams->bCapture = false;
    swprintf_s( pCmdLineParams->strCaptureFilename, L"FrameCapture.bmp" );
    pCmdLineParams->iExitFrame = -1;
    pCmdLineParams->bRenderHUD = true;

    // Perform application-dependant command line processing
    WCHAR* strCmdLine = GetCommandLine();
    WCHAR strFlag[MAX_PATH];
    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( strCmdLine, &nNumArgs );
    for (int iArg = 1; iArg < nNumArgs; iArg++)
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if (*strCmdLine == L'/' || *strCmdLine == L'-')
        {
            strCmdLine++;

            if (IsNextArg( strCmdLine, L"device" ))
            {
                if (GetCmdParam( strCmdLine, strFlag ))
                {
                    if ((wcscmp( strFlag, L"HAL" ) == 0) || (wcscmp( strFlag, L"TNLHAL" ) == 0))
                    {
                        pCmdLineParams->DriverType = D3D_DRIVER_TYPE_HARDWARE;
                    }
                    else
                    {
                        pCmdLineParams->DriverType = D3D_DRIVER_TYPE_REFERENCE;
                    }
                    continue;
                }
            }

            if (IsNextArg( strCmdLine, L"width" ))
            {
                if (GetCmdParam( strCmdLine, strFlag ))
                {
                    pCmdLineParams->uWidth = _wtoi( strFlag );
                }
                continue;
            }

            if (IsNextArg( strCmdLine, L"height" ))
            {
                if (GetCmdParam( strCmdLine, strFlag ))
                {
                    pCmdLineParams->uHeight = _wtoi( strFlag );
                }
                continue;
            }

            if (IsNextArg( strCmdLine, L"capturefilename" ))
            {
                if (GetCmdParam( strCmdLine, strFlag ))
                {
                    swprintf_s( pCmdLineParams->strCaptureFilename, L"%s", strFlag );
                    pCmdLineParams->bCapture = true;
                }
                continue;
            }

            if (IsNextArg( strCmdLine, L"nogui" ))
            {
                pCmdLineParams->bRenderHUD = false;
                continue;
            }

            if (IsNextArg( strCmdLine, L"exitframe" ))
            {
                if (GetCmdParam( strCmdLine, strFlag ))
                {
                    pCmdLineParams->iExitFrame = _wtoi( strFlag );
                }
                continue;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Helper function to check for file existance
//--------------------------------------------------------------------------------------
bool AMD::FileExists( WCHAR* pFileName )
{
    assert( NULL != pFileName );

    FILE* pFile = NULL;
    _wfopen_s( &pFile, pFileName, L"rb" );
    if (pFile)
    {
        fclose( pFile );

        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Creates a cube with vertex format of
// 32x3     Position
// 32x3     Normal
// 16x2     UV
// 28 byte stride
//--------------------------------------------------------------------------------------
void AMD::CreateCube( float fSize, ID3D11Buffer** ppVertexBuffer, ID3D11Buffer** ppIndexBuffer )
{
    if (ppVertexBuffer)
    {
        struct Position
        {
            float x, y, z;
        };

        float x = fSize;
        float y = fSize;
        float z = fSize;

        const Position corners[8] =
        {
            { x, -y, z },
            { x, -y, -z },
            { -x, -y, z },
            { -x, -y, -z },
            { x, y, z },
            { x, y, -z },
            { -x, y, z },
            { -x, y, -z }
        };

        const DirectX::XMFLOAT3 normalDirs[] =
        {
            DirectX::XMFLOAT3( 0.0f, 0.0f, 1.0f ),
            DirectX::XMFLOAT3( 0.0f, 0.0f, -1.0f ),
            DirectX::XMFLOAT3( 1.0f, 0.0f, 0.0f ),
            DirectX::XMFLOAT3( -1.0f, 0.0f, 0.0f ),
            DirectX::XMFLOAT3( 0.0f, 1.0f, 0.0f ),
            DirectX::XMFLOAT3( 0.0f, -1.0f, 0.0f )
        };

        struct Vertex
        {
            Position            position;
            DirectX::XMFLOAT3   normal;
            USHORT              uv[2];
        };

        Vertex vertices[24];

        // +z
        vertices[0].position = corners[4];
        vertices[1].position = corners[6];
        vertices[2].position = corners[0];
        vertices[3].position = corners[2];

        // -z
        vertices[4].position = corners[7];
        vertices[5].position = corners[5];
        vertices[6].position = corners[3];
        vertices[7].position = corners[1];

        // +x
        vertices[8].position = corners[5];
        vertices[9].position = corners[4];
        vertices[10].position = corners[1];
        vertices[11].position = corners[0];

        // -x
        vertices[12].position = corners[6];
        vertices[13].position = corners[7];
        vertices[14].position = corners[2];
        vertices[15].position = corners[3];

        // +y
        vertices[16].position = corners[7];
        vertices[17].position = corners[6];
        vertices[18].position = corners[5];
        vertices[19].position = corners[4];

        // -y
        vertices[20].position = corners[0];
        vertices[21].position = corners[2];
        vertices[22].position = corners[1];
        vertices[23].position = corners[3];

        int f = 0;
        for (int i = 0; i < 6; i++)
        {
            vertices[f + 0].uv[0] = 0x3C00;
            vertices[f + 0].uv[1] = 0;
            vertices[f + 1].uv[0] = 0;
            vertices[f + 1].uv[1] = 0;
            vertices[f + 2].uv[0] = 0x3C00;
            vertices[f + 2].uv[1] = 0x3C00;
            vertices[f + 3].uv[0] = 0;
            vertices[f + 3].uv[1] = 0x3C00;

            DirectX::XMFLOAT3 normal = normalDirs[i];
            vertices[f + 0].normal = normal;
            vertices[f + 1].normal = normal;
            vertices[f + 2].normal = normal;
            vertices[f + 3].normal = normal;

            f += 4;
        }

        D3D11_BUFFER_DESC bd;
        ZeroMemory( &bd, sizeof( bd ) );
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = sizeof( vertices );
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = vertices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;

        DXUTGetD3D11Device()->CreateBuffer( &bd, &data, ppVertexBuffer );
    }

    if (ppIndexBuffer)
    {
        USHORT indices[36];
        USHORT* idx = indices;
        int f = 0;
        for (int i = 0; i < 6; i++)
        {
            idx[0] = USHORT( f + 2 );
            idx[1] = USHORT( f + 0 );
            idx[2] = USHORT( f + 1 );
            idx[3] = USHORT( f + 3 );
            idx[4] = USHORT( f + 2 );
            idx[5] = USHORT( f + 1 );
            f += 4;
            idx += 6;
        }

        D3D11_BUFFER_DESC bd;
        ZeroMemory( &bd, sizeof( bd ) );
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = sizeof( indices );
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data;
        data.pSysMem = indices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;
        DXUTGetD3D11Device()->CreateBuffer( &bd, &data, ppIndexBuffer );
    }
}

//--------------------------------------------------------------------------------------
// Convert single-precision float to half-precision float,
// returned as a 16-bit unsigned value
//--------------------------------------------------------------------------------------
unsigned short AMD::ConvertF32ToF16( float fValueToConvert )
{
    // single precision floating point format:
    // |1|   8    |          23           |
    // |s|eeeeeeee|mmmmmmmmmmmmmmmmmmmmmmm|

    // half precision floating point format:
    // |1|  5  |    10    |
    // |s|eeeee|mmmmmmmmmm|

    unsigned int uFloatBits = (*(unsigned int *)&fValueToConvert);

    // note, this functions does not handle values that are too large (i.e. overflow),
    // nor does it handle NaNs or infinity
    int  nExponent = (int)((uFloatBits & 0x7F800000u) >> 23) - 127 + 15;
    assert( nExponent < 31 );

    // if the resulting value would be a denorm or underflow, then just return a (signed) zero
    if (nExponent <= 0)
    {
        return (unsigned short)((uFloatBits & 0x80000000u) >> 16);
    }

    // else, exponent is in the range [1,30], and so we can represent
    // the value to convert as a normalized 16-bit float (with some loss of precision, of course)
    unsigned int uSignBit = (uFloatBits & 0x80000000u) >> 16;
    unsigned int uExponentBits = (unsigned int)nExponent << 10;
    unsigned int uMantissaBits = (uFloatBits & 0x007FFFFFu) >> 13;

    return (unsigned short)(uSignBit | uExponentBits | uMantissaBits);
}
