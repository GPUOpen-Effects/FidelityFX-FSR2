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
// File: AMD_Mesh.h
//
// Convenience wrapper for loading and drawing models with Assimp or DXUT sdkmesh.
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_MESH_H
#define AMD_SDK_MESH_H

#include <d3d11.h>
#include <vector>

#ifndef AMD_SAFE_RELEASE
#define AMD_SAFE_RELEASE(p) { if (p) { p->Release(); p = NULL; } }
#endif

class CDXUTSDKMesh;

namespace AMD
{
class Mesh
{
    ID3D11Buffer *                               _b1d_vertex;
    ID3D11Buffer *                               _b1d_index;
    std::vector<ID3D11Texture2D *>               _t2d;

    struct MaterialGroup
    {
        int _first_index;
        int _index_count;
        int _texture_index;

        MaterialGroup()
            : _first_index(0)
            , _index_count(0)
            , _texture_index(0)
        {}
    };

    struct Vertex
    {
        float position[3];
        float normal[3];
        float uv[2];
    };

    std::vector<MaterialGroup>        _material_group;

    char                              _name[128];
    int                               _id;

public:
    CDXUTSDKMesh                                 m_sdkMesh;
    bool                                         m_isSdkMesh;

    Mesh();
    ~Mesh();

    std::vector<Vertex>               _vertex;
    std::vector<int>                  _index;
    std::vector<ID3D11ShaderResourceView *>      _srv;

    HRESULT Create(ID3D11Device * pDevice, const char * path, const char * name, bool sdkmesh = false);

    HRESULT Render(ID3D11DeviceContext * pContext);
    HRESULT Release();

    ID3D11ShaderResourceView ** srv();
};

} // namespace AMD

#endif
