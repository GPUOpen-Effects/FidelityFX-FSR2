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
// File: Geometry.cpp
//
// Classes for geometric primitives and collision and visibility testing
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "Geometry.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Helper function to normalize a plane
//--------------------------------------------------------------------------------------
static void NormalizePlane( XMFLOAT4* pPlaneEquation )
{
    float mag;

    mag = sqrtf( pPlaneEquation->x * pPlaneEquation->x +
                 pPlaneEquation->y * pPlaneEquation->y +
                 pPlaneEquation->z * pPlaneEquation->z );

    pPlaneEquation->x = pPlaneEquation->x / mag;
    pPlaneEquation->y = pPlaneEquation->y / mag;
    pPlaneEquation->z = pPlaneEquation->z / mag;
    pPlaneEquation->w = pPlaneEquation->w / mag;
}


//--------------------------------------------------------------------------------------
// Extract all 6 plane equations from frustum denoted by supplied matrix
//--------------------------------------------------------------------------------------
void ExtractPlanesFromFrustum( XMFLOAT4* pPlaneEquation, const XMMATRIX* pMatrix, bool bNormalize )
{
    XMFLOAT4X4 TempMat;
    XMStoreFloat4x4( &TempMat, *pMatrix);

    // Left clipping plane
    pPlaneEquation[0].x = TempMat._14 + TempMat._11;
    pPlaneEquation[0].y = TempMat._24 + TempMat._21;
    pPlaneEquation[0].z = TempMat._34 + TempMat._31;
    pPlaneEquation[0].w = TempMat._44 + TempMat._41;

    // Right clipping plane
    pPlaneEquation[1].x = TempMat._14 - TempMat._11;
    pPlaneEquation[1].y = TempMat._24 - TempMat._21;
    pPlaneEquation[1].z = TempMat._34 - TempMat._31;
    pPlaneEquation[1].w = TempMat._44 - TempMat._41;

    // Top clipping plane
    pPlaneEquation[2].x = TempMat._14 - TempMat._12;
    pPlaneEquation[2].y = TempMat._24 - TempMat._22;
    pPlaneEquation[2].z = TempMat._34 - TempMat._32;
    pPlaneEquation[2].w = TempMat._44 - TempMat._42;

    // Bottom clipping plane
    pPlaneEquation[3].x = TempMat._14 + TempMat._12;
    pPlaneEquation[3].y = TempMat._24 + TempMat._22;
    pPlaneEquation[3].z = TempMat._34 + TempMat._32;
    pPlaneEquation[3].w = TempMat._44 + TempMat._42;

    // Near clipping plane
    pPlaneEquation[4].x = TempMat._13;
    pPlaneEquation[4].y = TempMat._23;
    pPlaneEquation[4].z = TempMat._33;
    pPlaneEquation[4].w = TempMat._43;

    // Far clipping plane
    pPlaneEquation[5].x = TempMat._14 - TempMat._13;
    pPlaneEquation[5].y = TempMat._24 - TempMat._23;
    pPlaneEquation[5].z = TempMat._34 - TempMat._33;
    pPlaneEquation[5].w = TempMat._44 - TempMat._43;

    // Normalize the plane equations, if requested
    if ( bNormalize )
    {
        NormalizePlane( &pPlaneEquation[0] );
        NormalizePlane( &pPlaneEquation[1] );
        NormalizePlane( &pPlaneEquation[2] );
        NormalizePlane( &pPlaneEquation[3] );
        NormalizePlane( &pPlaneEquation[4] );
        NormalizePlane( &pPlaneEquation[5] );
    }
}
