// FidelityFX Super Resolution Sample
//
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


#pragma once


#include "stdafx.h"


class AnimatedTextures
{
public:

    AnimatedTextures() {}
    virtual ~AnimatedTextures() {}

    void OnCreate( Device& device, UploadHeap& uploadHeap, StaticBufferPool& bufferPool, ResourceViewHeaps& resourceViewHeaps, DynamicBufferRing& constantBufferRing );
    void OnDestroy();

    void Render( ID3D12GraphicsCommandList* pCommandList, float frameTime, float speed, bool compositionMask, const Camera& camera );

private:

    ResourceViewHeaps*          m_pResourceViewHeaps = nullptr;
    DynamicBufferRing*          m_constantBufferRing = nullptr;

    ID3D12RootSignature*        m_pRootSignature = nullptr;
    ID3D12PipelineState*        m_pPipelines[2] = {};
    D3D12_INDEX_BUFFER_VIEW     m_indexBuffer = {};

    Texture                     m_textures[3] = {};
    CBV_SRV_UAV                 m_descriptorTable = {};

    float                       m_scrollFactor = 0.0f;
    float                       m_rotationFactor = 0.0f;
    float                       m_flipTimer = 0.0f;
};