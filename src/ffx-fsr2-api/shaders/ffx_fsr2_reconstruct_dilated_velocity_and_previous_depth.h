// This file is part of the FidelityFX SDK.
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

#ifndef FFX_FSR2_RECONSTRUCT_DILATED_VELOCITY_AND_PREVIOUS_DEPTH_H
#define FFX_FSR2_RECONSTRUCT_DILATED_VELOCITY_AND_PREVIOUS_DEPTH_H

void ReconstructPrevDepth(FFX_MIN16_I2 iPxPos, FfxFloat32 fDepth, FfxFloat32x2 fMotionVector, FFX_MIN16_I2 iPxDepthSize)
{
    FfxFloat32x2 fDepthUv = (FfxFloat32x2(iPxPos) + 0.5f) / iPxDepthSize;
    FfxFloat32x2 fPxPrevPos = (fDepthUv + fMotionVector) * FfxFloat32x2(iPxDepthSize)-0.5f;
    FFX_MIN16_I2 iPxPrevPos = FFX_MIN16_I2(floor(fPxPrevPos));
    FfxFloat32x2 fPxFrac = ffxFract(fPxPrevPos);

    const FfxFloat32 bilinearWeights[2][2] = {
        {
            (1 - fPxFrac.x) * (1 - fPxFrac.y),
            (fPxFrac.x) * (1 - fPxFrac.y)
        },
        {
            (1 - fPxFrac.x) * (fPxFrac.y),
            (fPxFrac.x) * (fPxFrac.y)
        }
    };

    // Project current depth into previous frame locations.
    // Push to all pixels having some contribution if reprojection is using bilinear logic.
    for (FfxInt32 y = 0; y <= 1; ++y) {
        for (FfxInt32 x = 0; x <= 1; ++x) {
            
            FFX_MIN16_I2 offset = FFX_MIN16_I2(x, y);
            FfxFloat32 w = bilinearWeights[y][x];

            if (w > reconstructedDepthBilinearWeightThreshold) {

                FFX_MIN16_I2 storePos = iPxPrevPos + offset;
                if (IsOnScreen(storePos, iPxDepthSize)) {
                    StoreReconstructedDepth(storePos, fDepth);
                }
            }
        }
    }
}

void FindNearestDepth(FFX_PARAMETER_IN FFX_MIN16_I2 iPxPos, FFX_PARAMETER_IN FFX_MIN16_I2 iPxSize, FFX_PARAMETER_OUT FfxFloat32 fNearestDepth, FFX_PARAMETER_OUT FFX_MIN16_I2 fNearestDepthCoord)
{
    const FfxInt32 iSampleCount = 9;
    const FFX_MIN16_I2 iSampleOffsets[iSampleCount] = {
        FFX_MIN16_I2(+0, +0),
        FFX_MIN16_I2(+1, +0),
        FFX_MIN16_I2(+0, +1),
        FFX_MIN16_I2(+0, -1),
        FFX_MIN16_I2(-1, +0),
        FFX_MIN16_I2(-1, +1),
        FFX_MIN16_I2(+1, +1),
        FFX_MIN16_I2(-1, -1),
        FFX_MIN16_I2(+1, -1),
    };

    // pull out the depth loads to allow SC to batch them
    FfxFloat32 depth[9];
    FfxInt32 iSampleIndex = 0;
    FFX_UNROLL
    for (iSampleIndex = 0; iSampleIndex < iSampleCount; ++iSampleIndex) {

        FFX_MIN16_I2 iPos = iPxPos + iSampleOffsets[iSampleIndex];
        depth[iSampleIndex] = LoadInputDepth(iPos);
    }

    // find closest depth
    fNearestDepthCoord = iPxPos;
    fNearestDepth = depth[0];
    FFX_UNROLL
    for (iSampleIndex = 1; iSampleIndex < iSampleCount; ++iSampleIndex) {

        FFX_MIN16_I2 iPos = iPxPos + iSampleOffsets[iSampleIndex];
        if (IsOnScreen(iPos, iPxSize)) {

            FfxFloat32 fNdDepth = depth[iSampleIndex];
#if FFX_FSR2_OPTION_INVERTED_DEPTH
            if (fNdDepth > fNearestDepth) {
#else
            if (fNdDepth < fNearestDepth) {
#endif
                fNearestDepthCoord = iPos;
                fNearestDepth = fNdDepth;
            }
        }
    }
}

void ReconstructPrevDepthAndDilateMotionVectors(FFX_MIN16_I2 iPxLrPos)
{
    FFX_MIN16_I2 iPxLrSize = FFX_MIN16_I2(RenderSize());
    FFX_MIN16_I2 iPxHrSize = FFX_MIN16_I2(DisplaySize());

    FfxFloat32 fDilatedDepth;
    FFX_MIN16_I2 iNearestDepthCoord;

    FindNearestDepth(iPxLrPos, iPxLrSize, fDilatedDepth, iNearestDepthCoord);

#if FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS
    FfxFloat32x2 fDilatedMotionVector = LoadInputMotionVector(iNearestDepthCoord);
#else
    FfxFloat32x2 fSrcJitteredPos = FfxFloat32x2(iNearestDepthCoord) + 0.5f - Jitter();
    FfxFloat32x2 fLrPosInHr = (fSrcJitteredPos / iPxLrSize) * iPxHrSize;
    FfxFloat32x2 fHrPos = floor(fLrPosInHr) + 0.5;

    FfxFloat32x2 fDilatedMotionVector = LoadInputMotionVector(FFX_MIN16_I2(fHrPos));
#endif

    StoreDilatedDepth(iPxLrPos, fDilatedDepth);
    StoreDilatedMotionVector(iPxLrPos, fDilatedMotionVector);

    ReconstructPrevDepth(iPxLrPos, fDilatedDepth, fDilatedMotionVector, iPxLrSize);
}

#endif //!defined( FFX_FSR2_RECONSTRUCT_DILATED_VELOCITY_AND_PREVIOUS_DEPTH_H )
