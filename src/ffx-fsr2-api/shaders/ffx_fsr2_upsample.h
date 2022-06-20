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

#ifndef FFX_FSR2_UPSAMPLE_H
#define FFX_FSR2_UPSAMPLE_H

FfxFloat32 SmoothStep(FfxFloat32 x, FfxFloat32 a, FfxFloat32 b)
{
    x = clamp((x - a) / (b - a), 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

FFX_STATIC const FfxUInt32 iLanczos2SampleCount = 16;

void DeringingWithMinMax(UPSAMPLE_F3 fDeringingMin, UPSAMPLE_F3 fDeringingMax, FFX_PARAMETER_INOUT UPSAMPLE_F3 fColor, FFX_PARAMETER_OUT FfxFloat32 fRangeSimilarity)
{
    fRangeSimilarity = fDeringingMin.x / fDeringingMax.x;
    fColor = clamp(fColor, fDeringingMin, fDeringingMax);
}

void Deringing(RectificationBoxData clippingBox, FFX_PARAMETER_INOUT UPSAMPLE_F3 fColor)
{
    fColor = clamp(fColor, clippingBox.aabbMin, clippingBox.aabbMax);
}

UPSAMPLE_F GetUpsampleLanczosWeight(UPSAMPLE_F2 fSrcSampleOffset, UPSAMPLE_F2 fKernelWeight)
{
    UPSAMPLE_F2 fSrcSampleOffsetBiased = UPSAMPLE_F2(fSrcSampleOffset * fKernelWeight);
    UPSAMPLE_F fSampleWeight = Lanczos2ApproxSq(dot(fSrcSampleOffsetBiased, fSrcSampleOffsetBiased));		// TODO: check other distances (l0, l1, linf...)

    return fSampleWeight;
}

UPSAMPLE_F Pow3(UPSAMPLE_F x)
{
    return x * x * x;
}

UPSAMPLE_F4 ComputeUpsampledColorAndWeight(FFX_MIN16_I2 iPxHrPos, UPSAMPLE_F2 fKernelWeight, FFX_PARAMETER_INOUT RectificationBoxData clippingBox)
{
    // We compute a sliced lanczos filter with 2 lobes (other slices are accumulated temporaly)
    FfxFloat32x2 fDstOutputPos = FfxFloat32x2(iPxHrPos) + FFX_BROADCAST_FLOAT32X2(0.5f);      // Destination resolution output pixel center position
    FfxFloat32x2 fSrcOutputPos = fDstOutputPos * DownscaleFactor();                   // Source resolution output pixel center position
    FfxInt32x2 iSrcInputPos = FfxInt32x2(floor(fSrcOutputPos));                     // TODO: what about weird upscale factors...

    UPSAMPLE_F3 fSamples[iLanczos2SampleCount];

    FfxFloat32x2 fSrcUnjitteredPos = (FfxFloat32x2(iSrcInputPos) + FFX_BROADCAST_FLOAT32X2(0.5f)) - Jitter();                // This is the un-jittered position of the sample at offset 0,0
    
    UPSAMPLE_I2 offsetTL;
    offsetTL.x = (fSrcUnjitteredPos.x > fSrcOutputPos.x) ? UPSAMPLE_I(-2) : UPSAMPLE_I(-1);
    offsetTL.y = (fSrcUnjitteredPos.y > fSrcOutputPos.y) ? UPSAMPLE_I(-2) : UPSAMPLE_I(-1);

    RectificationBox fRectificationBox;

    //Load samples
    // If fSrcUnjitteredPos.y > fSrcOutputPos.y, indicates offsetTL.y = -2, sample offset Y will be [-2, 1], clipbox will be rows [1, 3].
    // Flip row# for sampling offset in this case, so first 0~2 rows in the sampled array can always be used for computing the clipbox.
    // This reduces branch or cmove on sampled colors, but moving this overhead to sample position / weight calculation time which apply to less values.
    const FfxBoolean bFlipRow = fSrcUnjitteredPos.y > fSrcOutputPos.y;
    const FfxBoolean bFlipCol = fSrcUnjitteredPos.x > fSrcOutputPos.x;

    UPSAMPLE_F2 fOffsetTL = UPSAMPLE_F2(offsetTL);

    FFX_UNROLL
    for (FfxInt32 row = 0; row < 4; row++) {

        FFX_UNROLL
        for (FfxInt32 col = 0; col < 4; col++) {
            FfxInt32 iSampleIndex = col + (row << 2);

            FfxInt32x2 sampleColRow = FfxInt32x2(bFlipCol ? (3 - col) : col, bFlipRow ? (3 - row) : row);
            FfxInt32x2 iSrcSamplePos = FfxInt32x2(iSrcInputPos) + offsetTL + sampleColRow;

            const FfxInt32x2 sampleCoord = ClampLoad(iSrcSamplePos, FfxInt32x2(0, 0), FfxInt32x2(RenderSize()));

            fSamples[iSampleIndex] = LoadPreparedInputColor(FFX_MIN16_I2(sampleCoord));
        }
    }

    RectificationBoxReset(fRectificationBox, fSamples[0]);

    UPSAMPLE_F3 fColor = UPSAMPLE_F3(0.f, 0.f, 0.f);
    UPSAMPLE_F fWeight = UPSAMPLE_F(0.f);
    UPSAMPLE_F2 fBaseSampleOffset = UPSAMPLE_F2(fSrcUnjitteredPos - fSrcOutputPos);

    FFX_UNROLL
    for (FfxUInt32 iSampleIndex = 0; iSampleIndex < iLanczos2SampleCount; ++iSampleIndex)
    {
        FfxInt32 row = FfxInt32(iSampleIndex >> 2);
        FfxInt32 col = FfxInt32(iSampleIndex & 3);

        const FfxInt32x2 sampleColRow = FfxInt32x2(bFlipCol ? (3 - col) : col, bFlipRow ? (3 - row) : row);
        const UPSAMPLE_F2 fOffset = fOffsetTL + UPSAMPLE_F2(sampleColRow);
        UPSAMPLE_F2 fSrcSampleOffset = fBaseSampleOffset + fOffset;

        FfxInt32x2 iSrcSamplePos = FfxInt32x2(iSrcInputPos) + FfxInt32x2(offsetTL) + sampleColRow;

        UPSAMPLE_F fSampleWeight = UPSAMPLE_F(IsOnScreen(FFX_MIN16_I2(iSrcSamplePos), FFX_MIN16_I2(RenderSize()))) * GetUpsampleLanczosWeight(fSrcSampleOffset, fKernelWeight);

        // Update rectification box
        if(all(FFX_LESS_THAN(FfxInt32x2(col, row), FFX_BROADCAST_INT32X2(3))))
        {
            //update clipping box in non-locked areas
            const UPSAMPLE_F fSrcSampleOffsetSq = dot(fSrcSampleOffset, fSrcSampleOffset);
            UPSAMPLE_F fBoxSampleWeight = UPSAMPLE_F(1) - ffxSaturate(fSrcSampleOffsetSq / UPSAMPLE_F(3));
            fBoxSampleWeight *= fBoxSampleWeight;
            RectificationBoxAddSample(fRectificationBox, fSamples[iSampleIndex], fBoxSampleWeight);
        }

        fWeight += fSampleWeight;
        fColor += fSampleWeight * fSamples[iSampleIndex];
    }

    // Normalize for deringing (we need to compare colors)
    fColor = fColor / (abs(fWeight) > FSR2_EPSILON ? fWeight : UPSAMPLE_F(1.f));

    RectificationBoxComputeVarianceBoxData(fRectificationBox);
    clippingBox = RectificationBoxGetData(fRectificationBox);

    Deringing(RectificationBoxGetData(fRectificationBox), fColor);

    if (any(FFX_LESS_THAN(fKernelWeight, UPSAMPLE_F2_BROADCAST(1.0f)))) {
        fWeight = UPSAMPLE_F(averageLanczosWeightPerFrame);
    }

    return UPSAMPLE_F4(fColor, ffxMax(UPSAMPLE_F(0), fWeight));
}

#endif //!defined( FFX_FSR2_UPSAMPLE_H )
