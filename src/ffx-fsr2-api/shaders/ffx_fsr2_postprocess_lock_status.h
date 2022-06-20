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

#ifndef FFX_FSR2_POSTPROCESS_LOCK_STATUS_H
#define FFX_FSR2_POSTPROCESS_LOCK_STATUS_H

FFX_MIN16_F4 WrapShadingChangeLuma(FFX_MIN16_I2 iPxSample)
{
    return FFX_MIN16_F4(LoadMipLuma(iPxSample, LumaMipLevelToUse()), 0, 0, 0);
}

DeclareCustomFetchBilinearSamples(FetchShadingChangeLumaSamples, WrapShadingChangeLuma)
DeclareCustomTextureSample(ShadingChangeLumaSample, Bilinear, FetchShadingChangeLumaSamples)

FFX_MIN16_F GetShadingChangeLuma(FfxFloat32x2 fUvCoord)
{
    // const FfxFloat32 fShadingChangeLuma = exp(ShadingChangeLumaSample(fUvCoord, LumaMipDimensions()) * LumaMipRcp());
    const FFX_MIN16_F fShadingChangeLuma = FFX_MIN16_F(exp(SampleMipLuma(fUvCoord, LumaMipLevelToUse()) * FFX_MIN16_F(LumaMipRcp())));
    return fShadingChangeLuma;
}

LockState GetLockState(LOCK_STATUS_T fLockStatus)
{
    LockState state = { FFX_FALSE, FFX_FALSE };

    //Check if this is a new or refreshed lock
    state.NewLock = fLockStatus[LOCK_LIFETIME_REMAINING] < LOCK_STATUS_F1(0.0f);

    //For a non-refreshed lock, the lifetime is set to LockInitialLifetime()
    state.WasLockedPrevFrame = fLockStatus[LOCK_TRUST] != LOCK_STATUS_F1(0.0f);

    return state;
}

LockState PostProcessLockStatus(FFX_MIN16_I2 iPxHrPos, FFX_PARAMETER_IN FfxFloat32x2 fLrUvJittered, FFX_PARAMETER_IN FFX_MIN16_F fDepthClipFactor, FFX_PARAMETER_IN FfxFloat32 fHrVelocity,
    FFX_PARAMETER_INOUT FfxFloat32 fAccumulationTotalWeight, FFX_PARAMETER_INOUT LOCK_STATUS_T fLockStatus, FFX_PARAMETER_OUT FFX_MIN16_F fLuminanceDiff) {

    const LockState state = GetLockState(fLockStatus);

    fLockStatus[LOCK_LIFETIME_REMAINING] = abs(fLockStatus[LOCK_LIFETIME_REMAINING]);

    FFX_MIN16_F fShadingChangeLuma = GetShadingChangeLuma(fLrUvJittered);

    //init temporal shading change factor, init to -1 or so in reproject to know if "true new"?
    fLockStatus[LOCK_TEMPORAL_LUMA] = (fLockStatus[LOCK_TEMPORAL_LUMA] == LOCK_STATUS_F1(0.0f)) ? fShadingChangeLuma : fLockStatus[LOCK_TEMPORAL_LUMA];

    FFX_MIN16_F fPreviousShadingChangeLuma = fLockStatus[LOCK_TEMPORAL_LUMA];
    fLockStatus[LOCK_TEMPORAL_LUMA] = ffxLerp(fLockStatus[LOCK_TEMPORAL_LUMA], LOCK_STATUS_F1(fShadingChangeLuma), LOCK_STATUS_F1(0.5f));
    fLuminanceDiff = FFX_MIN16_F(1) - MinDividedByMax(fPreviousShadingChangeLuma, fShadingChangeLuma);

    if (fLuminanceDiff > FFX_MIN16_F(0.2f)) {
        KillLock(fLockStatus);
    }

    if (!state.NewLock && fLockStatus[LOCK_LIFETIME_REMAINING] >= LOCK_STATUS_F1(0))
    {
        const FFX_MIN16_F depthClipThreshold = FFX_MIN16_F(0.99f);
        if (fDepthClipFactor < depthClipThreshold)
        {
            KillLock(fLockStatus);
        }
    }

    return state;
}

#endif //!defined( FFX_FSR2_POSTPROCESS_LOCK_STATUS_H )
