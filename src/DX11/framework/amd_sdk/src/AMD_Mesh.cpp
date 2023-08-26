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

// DXUT helper code
#include "DXUT.h"
#include "DXUTmisc.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#include "AMD_Mesh.h"

#include <string>

#ifndef AMD_SDK_MINIMAL
#include "assimp/scene.h"
#include "assimp/importer.hpp"
#include "DDSTextureLoader.h"
#include "crc.h"
#endif

#pragma warning (disable : 4996)

namespace AMD
{
Mesh::Mesh()
    : _vertex(NULL)
    , _index(NULL)
    , _id(0)
{
    memcpy(_name, "default", sizeof("default"));
}

Mesh::~Mesh()
{
    Release();
}

HRESULT Mesh::Create(ID3D11Device * pDevice, const char * path, const char * name, bool sdkmesh)
{
    m_isSdkMesh = sdkmesh;

    if (sdkmesh)
    {
        char filename[256];
        sprintf(filename, "%s%s", path, name);
        std::string fname(filename);
        std::wstring wfname(fname.begin(), fname.end());

        return m_sdkMesh.Create(pDevice, wfname.c_str(), false);
    }
#ifdef AMD_SDK_MINIMAL
    else
    {
        // the minimal-dependencies version of AMD_SDK
        // only supports sdkmesh
        return E_FAIL;
    }
#else
    HRESULT             hr = S_OK;
    Assimp::Importer    importer;
    int                 num_vertices = 0;
    int                 num_faces = 0;

    std::string filename = std::string(path) + std::string(name);

    aiScene* scene = (aiScene*)importer.ReadFile(filename.c_str(), 0);

    if (!scene) { return E_FAIL; }

    _id = crcFast((const unsigned char *)filename.c_str(), (int)filename.length());

    if (scene->HasMeshes() && scene->mNumMeshes > 0)
    {
        for (int i = 0; i < (int)scene->mNumMeshes; i++)
        {
            num_vertices += scene->mMeshes[i]->mNumVertices;
            num_faces += scene->mMeshes[i]->mNumFaces;
        }

        if (num_vertices == 0 || num_faces == 0)  { return S_OK; }

        int current_vertex = 0;
        int current_face = 0;

        _material_group.resize(scene->mNumMeshes);
        _vertex.resize(num_vertices);
        _index.resize(num_faces * 3);

        ID3D11DeviceContext * pContext = NULL;
        pDevice->GetImmediateContext(&pContext);

        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            ID3D11Texture2D * t2d = NULL;
            ID3D11ShaderResourceView * srv = NULL;

            aiMesh * mesh = scene->mMeshes[i];
            aiMaterial * material = scene->mMaterials[mesh->mMaterialIndex];
            aiString c_texture_filename;
            material->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), c_texture_filename);

            std::string tex_filename = std::string(path) + std::string(c_texture_filename.C_Str());
            {
                WCHAR wc_texture_filename[1024];
                mbstowcs(wc_texture_filename, tex_filename.c_str(), tex_filename.length() + 1);

                unsigned int bind_flags = D3D11_BIND_SHADER_RESOURCE;
                hr = DirectX::CreateDDSTextureFromFileEx(pDevice, wc_texture_filename, 0, D3D11_USAGE_DEFAULT, bind_flags, 0, 0, true, (ID3D11Resource**)&t2d, &srv);
            }

            _material_group[i]._texture_index = (int)_t2d.size();
            _t2d.push_back(t2d);
            _srv.push_back(srv);

            _material_group[i]._first_index = current_face;
            _material_group[i]._index_count = mesh->mNumFaces * 3;

            aiVector3D * position = mesh->HasPositions() ? mesh->mVertices : NULL;
            aiVector3D * normal = mesh->HasNormals() ? mesh->mNormals : NULL;
            aiVector3D * uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0] : NULL;

            for (unsigned int j = 0; j < mesh->mNumVertices; j++)
            {
                if (position != NULL)
                {
                    memcpy( &_vertex[current_vertex + j].position, &position[j], sizeof( float ) * 3 );
                }

                if (normal != NULL)
                {
                    memcpy( &_vertex[current_vertex + j].normal, &normal[j], sizeof( float ) * 3 );
                }

                if (uv != NULL)
                {
                    memcpy( &_vertex[current_vertex + j].uv, &uv[j], sizeof( float ) * 2 );
                }
            }

            if (mesh->HasFaces())
            {
                aiFace * f = mesh->mFaces;
                for (unsigned int j = 0; j < mesh->mNumFaces; j++)
                {
                    _index[current_face + j * 3 + 0] = current_vertex + f[j].mIndices[0];
                    _index[current_face + j * 3 + 1] = current_vertex + f[j].mIndices[1];
                    _index[current_face + j * 3 + 2] = current_vertex + f[j].mIndices[2];
                }
            }

            current_face += mesh->mNumFaces * 3;
            current_vertex += mesh->mNumVertices;
        }

        AMD_SAFE_RELEASE(pContext);

        CD3D11_BUFFER_DESC vertexDesc, indexDesc;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
        vertexDesc.ByteWidth = sizeof(Vertex) * num_vertices;
        vertexDesc.CPUAccessFlags = 0;
        vertexDesc.MiscFlags = 0;
        vertexDesc.StructureByteStride = 0;
        vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexDesc.ByteWidth = sizeof(int) * num_faces * 3;
        indexDesc.CPUAccessFlags = 0;
        indexDesc.MiscFlags = 0;
        indexDesc.StructureByteStride = 0;
        indexDesc.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA vertexData, indexData;
        memset(&vertexData, 0, sizeof(vertexData));
        memset(&indexData, 0, sizeof(indexData));

        vertexData.pSysMem = &_vertex[0];
        indexData.pSysMem = &_index[0];

        hr = pDevice->CreateBuffer(&vertexDesc, &vertexData, &_b1d_vertex);
        hr = pDevice->CreateBuffer(&indexDesc, &indexData, &_b1d_index);
    }

    return hr;
#endif
}

HRESULT Mesh::Render(ID3D11DeviceContext * pContext)
{
    if (m_isSdkMesh)
    {
        m_sdkMesh.Render(pContext, 0);
        return S_OK;
    }
#ifdef AMD_SDK_MINIMAL
    else
    {
        // the minimal-dependencies version of AMD_SDK
        // only supports sdkmesh
        return E_FAIL;
    }
#else
    unsigned int stride = sizeof(Vertex), offset = 0;
    pContext->IASetIndexBuffer(_b1d_index, DXGI_FORMAT_R32_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->IASetVertexBuffers(0, 1, &_b1d_vertex, &stride, &offset);

    for (unsigned int i = 0; i < _material_group.size(); i++)
    {
        pContext->PSSetShaderResources(0, 1, &_srv[_material_group[i]._texture_index]);

        pContext->DrawIndexed(_material_group[i]._index_count, _material_group[i]._first_index, 0);
    }

    return S_OK;
#endif
}

HRESULT Mesh::Release()
{
    if (m_isSdkMesh)
    {
        m_sdkMesh.Destroy();
        return S_OK;
    }
#ifdef AMD_SDK_MINIMAL
    else
    {
        // the minimal-dependencies version of AMD_SDK
        // only supports sdkmesh
        return E_FAIL;
    }
#else
    AMD_SAFE_RELEASE(_b1d_vertex);
    AMD_SAFE_RELEASE(_b1d_index);

    for (unsigned int i = 0; i < _t2d.size(); i++)
    {
        AMD_SAFE_RELEASE(_t2d[i]);
        AMD_SAFE_RELEASE(_srv[i]);
    }
    _t2d.clear();
    _srv.clear();

    return S_OK;
#endif
}

ID3D11ShaderResourceView ** Mesh::srv()
{
#ifndef AMD_SDK_MINIMAL
    if (_srv.size())
    {
        return &_srv[0];
    }
    else
#endif
    {
        return NULL;
    }
}

} // namespace AMD
