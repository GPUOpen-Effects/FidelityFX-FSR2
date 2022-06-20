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

FFX_MIN16_F4 WrapDepthClipMask(FFX_MIN16_I2 iPxSample)
{
    return FFX_MIN16_F4(LoadDepthClip(iPxSample).r, 0, 0, 0);
}

DeclareCustomFetchBilinearSamples(FetchDepthClipMaskSamples, WrapDepthClipMask)
DeclareCustomTextureSample(DepthClipMaskSample, Bilinear, FetchDepthClipMaskSamples)

FFX_MIN16_F4 WrapTransparencyAndCompositionMask(FFX_MIN16_I2 iPxSample)
{
    return FFX_MIN16_F4(LoadTransparencyAndCompositionMask(iPxSample).r, 0, 0, 0);
}

DeclareCustomFetchBilinearSamples(FetchTransparencyAndCompositionMaskSamples, WrapTransparencyAndCompositionMask)
DeclareCustomTextureSample(TransparencyAndCompositionMaskSample, Bilinear, FetchTransparencyAndCompositionMaskSamples)

FfxFloat32x4 WrapLumaStabilityFactor(FFX_MIN16_I2 iPxSample)
{
    return FfxFloat32x4(LoadLumaStabilityFactor(iPxSample), 0, 0, 0);
}

DeclareCustomFetchBilinearSamples(FetchLumaStabilitySamples, WrapLumaStabilityFactor)
DeclareCustomTextureSample(LumaStabilityFactorSample, Bilinear, FetchLumaStabilitySamples)

FfxFloat32 GetPxHrVelocity(FfxFloat32x2 fMotionVector)
{
    return length(fMotionVector * DisplaySize());
}

void Accumulate(FFX_MIN16_I2 iPxHrPos, FFX_PARAMETER_INOUT FfxFloat32x4 fHistory, FFX_PARAMETER_IN FfxFloat32x4 fUpsampled, FFX_PARAMETER_IN FfxFloat32 fDepthClipFactor, FFX_PARAMETER_IN FfxFloat32 fHrVelocity)
{

    fHistory.w = fHistory.w + fUpsampled.w;

    fUpsampled.rgb = YCoCgToRGB(fUpsampled.rgb);

    const FfxFloat32 fAlpha = fUpsampled.w / fHistory.w;
    fHistory.rgb = ffxLerp(fHistory.rgb, fUpsampled.rgb, fAlpha);

    FfxFloat32 fMaxAverageWeight = ffxLerp(MaxAccumulationWeight(), accumulationMaxOnMotion, ffxSaturate(fHrVelocity * 10.0f));
    fHistory.w = ffxMin(fHistory.w, fMaxAverageWeight);
}

void RectifyHistory(
    RectificationBoxData clippingBox,
    inout FfxFloat32x4 fHistory,
    FFX_PARAMETER_IN LOCK_STATUS_T fLockStatus,
    FFX_PARAMETER_IN UPSAMPLE_F fDepthClipFactor,
    FFX_PARAMETER_IN UPSAMPLE_F fLumaStabilityFactor,
    FFX_PARAMETER_IN UPSAMPLE_F fLuminanceDiff,
    FFX_PARAMETER_IN UPSAMPLE_F fUpsampleWeight,
    FFX_PARAMETER_IN FfxFloat32 fLockContributionThisFrame)
{
    UPSAMPLE_F fScaleFactorInfluence = UPSAMPLE_F(1.0f / DownscaleFactor().x - 1);
    UPSAMPLE_F fBoxScale = UPSAMPLE_F(1.0f) + (UPSAMPLE_F(0.5f) * fScaleFactorInfluence);

    FFX_MIN16_F3 fScaledBoxVec = clippingBox.boxVec * fBoxScale;
    UPSAMPLE_F3 boxMin = clippingBox.boxCenter - fScaledBoxVec;
    UPSAMPLE_F3 boxMax = clippingBox.boxCenter + fScaledBoxVec;
    UPSAMPLE_F3 boxCenter = clippingBox.boxCenter;
    UPSAMPLE_F boxVecSize = length(clippingBox.boxVec);

    boxMin = ffxMax(clippingBox.aabbMin, boxMin);
    boxMax = ffxMin(clippingBox.aabbMax, boxMax);

    UPSAMPLE_F3 distToClampOutside = UPSAMPLE_F3(ffxMax(ffxMax(UPSAMPLE_F3_BROADCAST(0.0f), boxMin - UPSAMPLE_F3(fHistory.xyz)), ffxMax(UPSAMPLE_F3_BROADCAST(0.0f), UPSAMPLE_F3(fHistory.xyz) - boxMax)));

    if (any(FFX_GREATER_THAN(distToClampOutside, UPSAMPLE_F3_BROADCAST(0.0f)))) {

        const UPSAMPLE_F3 clampedHistorySample = clamp(UPSAMPLE_F3(fHistory.xyz), boxMin, boxMax);

        UPSAMPLE_F3 clippedHistoryToBoxCenter = abs(clampedHistorySample - boxCenter);
        UPSAMPLE_F3 historyToBoxCenter = abs(UPSAMPLE_F3(fHistory.xyz) - boxCenter);
        UPSAMPLE_F3 HistoryColorWeight;
        HistoryColorWeight.x = historyToBoxCenter.x > UPSAMPLE_F(0) ? clippedHistoryToBoxCenter.x / historyToBoxCenter.x : UPSAMPLE_F(0.0f);
        HistoryColorWeight.y = historyToBoxCenter.y > UPSAMPLE_F(0) ? clippedHistoryToBoxCenter.y / historyToBoxCenter.y : UPSAMPLE_F(0.0f);
        HistoryColorWeight.z = historyToBoxCenter.z > UPSAMPLE_F(0) ? clippedHistoryToBoxCenter.z / historyToBoxCenter.z : UPSAMPLE_F(0.0f);

        UPSAMPLE_F3 fHistoryContribution = HistoryColorWeight;

        // only lock luma
        fHistoryContribution += UPSAMPLE_F3_BROADCAST(ffxMax(UPSAMPLE_F(fLockContributionThisFrame), fLumaStabilityFactor));
        fHistoryContribution *= (fDepthClipFactor * fDepthClipFactor);

        fHistory.xyz = FfxFloat32x3(ffxLerp(clampedHistorySample.xyz, fHistory.xyz, ffxSaturate(fHistoryContribution)));
    }
}

void WriteUpscaledOutput(FFX_MIN16_I2 iPxHrPos, FfxFloat32x3 fUpscaledColor)
{
    StoreUpscaledOutput(iPxHrPos, fUpscaledColor);
}

FfxFloat32 GetLumaStabilityFactor(FfxFloat32x2 fHrUv, FfxFloat32 fHrVelocity)
{
    FfxFloat32 fLumaStabilityFactor = SampleLumaStabilityFactor(fHrUv);

    // Only apply on still, have to reproject luma history resource if we want it to work on motion
    fLumaStabilityFactor *= FfxFloat32(fHrVelocity < 0.1f);

    return fLumaStabilityFactor;
}

FfxFloat32 GetLockContributionThisFrame(FfxFloat32x2 fUvCoord, FfxFloat32 fAccumulationMask, FfxFloat32 fParticleMask, LOCK_STATUS_T fLockStatus)
{
    const UPSAMPLE_F fNormalizedLockLifetime = GetNormalizedRemainingLockLifetime(fLockStatus);

    // Rectify on lock frame
    FfxFloat32 fLockContributionThisFrame = ffxSaturate(fNormalizedLockLifetime * UPSAMPLE_F(4));

    fLockContributionThisFrame *= (1.0f - fParticleMask);
    //Take down contribution in transparent areas
    fLockContributionThisFrame *= FfxFloat32(fAccumulationMask.r > 0.1f);

    return fLockContributionThisFrame;
}

void FinalizeLockStatus(FFX_MIN16_I2 iPxHrPos, LOCK_STATUS_T fLockStatus, FfxFloat32 fUpsampledWeight)
{
    // Increase trust
    const UPSAMPLE_F fTrustIncreaseLanczosMax = UPSAMPLE_F(12); // same increase no matter the MaxAccumulationWeight() value.
    const UPSAMPLE_F fTrustIncrease = UPSAMPLE_F(fUpsampledWeight / fTrustIncreaseLanczosMax);
    fLockStatus[LOCK_TRUST] = ffxMin(LOCK_STATUS_F1(1), fLockStatus[LOCK_TRUST] + fTrustIncrease);

    // Decrease lock lifetime
    const UPSAMPLE_F fLifetimeDecreaseLanczosMax = UPSAMPLE_F(JitterSequenceLength()) * UPSAMPLE_F(averageLanczosWeightPerFrame);
    const UPSAMPLE_F fLifetimeDecrease = UPSAMPLE_F(fUpsampledWeight / fLifetimeDecreaseLanczosMax);
    fLockStatus[LOCK_LIFETIME_REMAINING] = ffxMax(LOCK_STATUS_F1(0), fLockStatus[LOCK_LIFETIME_REMAINING] - fLifetimeDecrease);

    StoreLockStatus(iPxHrPos, fLockStatus);
}

UPSAMPLE_F ComputeMaxAccumulationWeight(UPSAMPLE_F fHrVelocity, UPSAMPLE_F fReactiveMax, UPSAMPLE_F fDepthClipFactor, UPSAMPLE_F fLuminanceDiff, LockState lockState) {

    UPSAMPLE_F normalizedMinimum = UPSAMPLE_F(accumulationMaxOnMotion) / UPSAMPLE_F(MaxAccumulationWeight());

    UPSAMPLE_F fReactiveMaxAccumulationWeight = UPSAMPLE_F(1) - fReactiveMax;
    UPSAMPLE_F fMotionMaxAccumulationWeight = ffxLerp(UPSAMPLE_F(1), normalizedMinimum, ffxSaturate(fHrVelocity * UPSAMPLE_F(10)));
    UPSAMPLE_F fDepthClipMaxAccumulationWeight = fDepthClipFactor;

    UPSAMPLE_F fLuminanceDiffMaxAccumulationWeight = ffxSaturate(ffxMax(normalizedMinimum, UPSAMPLE_F(1) - fLuminanceDiff));

    UPSAMPLE_F maxAccumulation = UPSAMPLE_F(MaxAccumulationWeight()) * ffxMin(
        ffxMin(fReactiveMaxAccumulationWeight, fMotionMaxAccumulationWeight),
        ffxMin(fDepthClipMaxAccumulationWeight, fLuminanceDiffMaxAccumulationWeight)
    );

    return (lockState.NewLock && !lockState.WasLockedPrevFrame) ? UPSAMPLE_F(accumulationMaxOnMotion) : maxAccumulation;
}

UPSAMPLE_F2 ComputeKernelWeight(in UPSAMPLE_F fHistoryWeight, in UPSAMPLE_F fDepthClipFactor, in UPSAMPLE_F fReactivityFactor) {
    UPSAMPLE_F fKernelSizeBias = ffxSaturate(ffxMax(UPSAMPLE_F(0), fHistoryWeight - UPSAMPLE_F(0.5)) / UPSAMPLE_F(3));

    //high bias on disocclusions

    UPSAMPLE_F fOneMinusReactiveMax = UPSAMPLE_F(1) - fReactivityFactor;
    UPSAMPLE_F2 fKernelWeight = UPSAMPLE_F(1) + (UPSAMPLE_F(1.0f) / UPSAMPLE_F2(DownscaleFactor()) - UPSAMPLE_F(1)) * UPSAMPLE_F(fKernelSizeBias) * fOneMinusReactiveMax;

    //average value on disocclusion, to help decrease high value sample importance wait for accumulation to kick in
    fKernelWeight *= FFX_BROADCAST_MIN_FLOAT16X2(UPSAMPLE_F(0.5) + fDepthClipFactor * UPSAMPLE_F(0.5));

    return ffxMin(FFX_BROADCAST_MIN_FLOAT16X2(1.99), fKernelWeight);
}

void Accumulate(FFX_MIN16_I2 iPxHrPos)
{
    const FfxFloat32x2 fSamplePosHr = iPxHrPos + 0.5f;
    const FfxFloat32x2 fPxLrPos = fSamplePosHr * DownscaleFactor();                   // Source resolution output pixel center position
    const FfxInt32x2 iPxLrPos = FfxInt32x2(floor(fPxLrPos));                             // TODO: what about weird upscale factors...

    const FfxFloat32x2 fSamplePosUnjitterLr = (FfxFloat32x2(iPxLrPos) + FfxFloat32x2(0.5f, 0.5f)) - Jitter();                // This is the un-jittered position of the sample at offset 0,0

    const FfxFloat32x2 fLrUvJittered = (fPxLrPos + Jitter()) / RenderSize();

    const FfxFloat32x2 fHrUv = (iPxHrPos + 0.5f) / DisplaySize();
    const FfxFloat32x2 fMotionVector = GetMotionVector(iPxHrPos, fHrUv);

    const FfxFloat32 fHrVelocity = GetPxHrVelocity(fMotionVector);
    const FfxFloat32 fDepthClipFactor = ffxSaturate(SampleDepthClip(fLrUvJittered));
    const FfxFloat32 fLumaStabilityFactor = GetLumaStabilityFactor(fHrUv, fHrVelocity);
    const FfxFloat32 fAccumulationMask = 1.0f - TransparencyAndCompositionMaskSample(fLrUvJittered, RenderSize()).r;

    FfxInt32x2 offsetTL;
    offsetTL.x = (fSamplePosUnjitterLr.x > fPxLrPos.x) ? FfxInt32(0) : FfxInt32(1);
    offsetTL.y = (fSamplePosUnjitterLr.y > fPxLrPos.y) ? FfxInt32(0) : FfxInt32(1);

    const UPSAMPLE_F fReactiveMax = UPSAMPLE_F(1) - Pow3(UPSAMPLE_F(1) - LoadReactiveMax(FFX_MIN16_I2(iPxLrPos + offsetTL)));

    FfxFloat32x4 fHistoryColorAndWeight = FfxFloat32x4(0.0f, 0.0f, 0.0f, 0.0f);
    LOCK_STATUS_T fLockStatus = CreateNewLockSample();
    FfxBoolean bIsExistingSample = FFX_TRUE;

    FfxFloat32x2 fReprojectedHrUv = FfxFloat32x2(0, 0);
    ComputeReprojectedUVs(iPxHrPos, fMotionVector, fReprojectedHrUv, bIsExistingSample);

    if (bIsExistingSample) {
        ReprojectHistoryColor(iPxHrPos, fReprojectedHrUv, fHistoryColorAndWeight);
        ReprojectHistoryLockStatus(iPxHrPos, fReprojectedHrUv, fLockStatus);
    }

    FFX_MIN16_F fLuminanceDiff = FFX_MIN16_F(0.0f);

    LockState lockState = PostProcessLockStatus(iPxHrPos, fLrUvJittered, FFX_MIN16_F(fDepthClipFactor), fHrVelocity, fHistoryColorAndWeight.w, fLockStatus, fLuminanceDiff);

    fHistoryColorAndWeight.w = ffxMin(fHistoryColorAndWeight.w, ComputeMaxAccumulationWeight(
        UPSAMPLE_F(fHrVelocity), fReactiveMax, UPSAMPLE_F(fDepthClipFactor), UPSAMPLE_F(fLuminanceDiff), lockState
    ));

    const UPSAMPLE_F fNormalizedLockLifetime = GetNormalizedRemainingLockLifetime(fLockStatus);

    // Kill accumulation based on shading change
    fHistoryColorAndWeight.w = ffxMin(fHistoryColorAndWeight.w, FFX_MIN16_F(ffxMax(0.0f, MaxAccumulationWeight() * ffxPow(UPSAMPLE_F(1) - fLuminanceDiff, 2.0f / 1.0f))));

    // Load upsampled input color
    RectificationBoxData clippingBox;

    FfxFloat32 fKernelBias = fAccumulationMask * ffxSaturate(ffxMax(0.0f, fHistoryColorAndWeight.w - 0.5f) / 3.0f);

    FfxFloat32 fReactiveWeighted = 0;

    // No trust in reactive areas
    fLockStatus[LOCK_TRUST] = ffxMin(fLockStatus[LOCK_TRUST], LOCK_STATUS_F1(1.0f) - LOCK_STATUS_F1(pow(fReactiveMax, 1.0f / 3.0f)));
    fLockStatus[LOCK_TRUST] = ffxMin(fLockStatus[LOCK_TRUST], LOCK_STATUS_F1(fDepthClipFactor));

    UPSAMPLE_F2 fKernelWeight = ComputeKernelWeight(UPSAMPLE_F(fHistoryColorAndWeight.w), UPSAMPLE_F(fDepthClipFactor), ffxMax((UPSAMPLE_F(1) - fLockStatus[LOCK_TRUST]), fReactiveMax));

    UPSAMPLE_F4 fUpsampledColorAndWeight = ComputeUpsampledColorAndWeight(iPxHrPos, fKernelWeight, clippingBox);


    FfxFloat32 fLockContributionThisFrame = GetLockContributionThisFrame(fHrUv, fAccumulationMask, fReactiveMax, fLockStatus);

    // Update accumulation and rectify history
    if (fHistoryColorAndWeight.w > 0.0f) {

        RectifyHistory(clippingBox, fHistoryColorAndWeight, fLockStatus, UPSAMPLE_F(fDepthClipFactor), UPSAMPLE_F(fLumaStabilityFactor), UPSAMPLE_F(fLuminanceDiff), fUpsampledColorAndWeight.w, fLockContributionThisFrame);

        fHistoryColorAndWeight.rgb = YCoCgToRGB(fHistoryColorAndWeight.rgb);
    }

    Accumulate(iPxHrPos, fHistoryColorAndWeight, fUpsampledColorAndWeight, fDepthClipFactor, fHrVelocity);

    //Subtract accumulation weight in reactive areas
    fHistoryColorAndWeight.w -= FfxFloat32(fUpsampledColorAndWeight.w * fReactiveMax);

#if FFX_FSR2_OPTION_HDR_COLOR_INPUT
    fHistoryColorAndWeight.rgb = InverseTonemap(fHistoryColorAndWeight.rgb);
#endif
    fHistoryColorAndWeight.rgb /= Exposure();

    FinalizeLockStatus(iPxHrPos, fLockStatus, fUpsampledColorAndWeight.w);

    StoreInternalColorAndWeight(iPxHrPos, fHistoryColorAndWeight);

    // Output final color when RCAS is disabled
#if FFX_FSR2_OPTION_APPLY_SHARPENING == 0
    WriteUpscaledOutput(iPxHrPos, fHistoryColorAndWeight.rgb);
#endif
}
