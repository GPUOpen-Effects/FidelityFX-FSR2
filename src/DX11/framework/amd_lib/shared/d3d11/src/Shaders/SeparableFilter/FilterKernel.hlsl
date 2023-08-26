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
// File: FilterKernel.hlsl
//
// Defines the compute kernel for a separable filter. It calls the macros defined by the
// user supplied HLSL such as GaussianFilter.hlsl and BilateralFilter.hlsl
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Samples from inputs defined by the SampleFromInput macro
//--------------------------------------------------------------------------------------
RAWDataItem Sample( int2 i2Position, float2 f2Offset )
{
    RAWDataItem RDI;
    float2 f2SamplePosition = float2( i2Position ) + float2( 0.5f, 0.5f );

    #if ( USE_APPROXIMATE_FILTER == 1 )

        f2SamplePosition += f2Offset;

    #endif

    f2SamplePosition *= g_f4OutputSize.zw;
    SAMPLE_FROM_INPUT( g_LinearClampSampler, f2SamplePosition, RDI )

    return RDI;
}

#if ( USE_COMPUTE_SHADER == 1 )

    //--------------------------------------------------------------------------------------
    // Macro for caching LDS reads, this has the effect of drastically reducing reads from the
    // LDS by up to 4x
    //--------------------------------------------------------------------------------------
    #define CACHE_LDS_READS( _iIteration, _iLineOffset, _iPixelOffset, _RDI ) \
        /* Trickle LDS values down within the GPRs*/ \
        [unroll] for ( iPixel = 0; iPixel < PIXELS_PER_THREAD - STEP_SIZE; ++iPixel ) { \
            _RDI[iPixel] = _RDI[iPixel + STEP_SIZE]; } \
        /* Load new LDS value(s) */ \
        [unroll] for ( iPixel = 0; iPixel < STEP_SIZE; ++iPixel ) { \
            READ_FROM_LDS( _iLineOffset, ( _iPixelOffset + _iIteration + iPixel ), _RDI[(PIXELS_PER_THREAD - STEP_SIZE + iPixel)] ) }


    //--------------------------------------------------------------------------------------
    // Defines the filter kernel logic. User supplies macro's for custom filter
    //--------------------------------------------------------------------------------------
    void ComputeFilterKernel( int iPixelOffset, int iLineOffset, int2 i2Center, int2 i2Inc )
    {
        CS_Output Output = (CS_Output)0;
        KernelData KD[PIXELS_PER_THREAD];
        int iPixel, iIteration;
        RAWDataItem RDI[PIXELS_PER_THREAD];

        #if ( USE_APPROXIMATE_FILTER == 1 )

            // Read the kernel center values in directly from the input surface(s), as the LDS
            // values are pre-filtered, and therefore do not represent the kernel center
            [unroll]
            for ( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
            {
                float2 f2SamplePosition = ( float2( i2Center + ( iPixel * i2Inc ) ) + float2( 0.5f, 0.5f ) ) * g_f4OutputSize.zw;
                SAMPLE_FROM_INPUT( g_PointSampler, f2SamplePosition, RDI[iPixel] )
            }

        #else

            // Read the kernel center values in from the LDS
            [unroll]
            for ( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
            {
                READ_FROM_LDS( iLineOffset, ( iPixelOffset + KERNEL_RADIUS + iPixel ), RDI[iPixel] )
            }

        #endif


        // Macro defines what happens at the kernel center
        KERNEL_CENTER( KD, iPixel, PIXELS_PER_THREAD, Output, RDI )

        // Prime the GPRs for the first half of the kernel
        [unroll]
        for ( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
        {
            READ_FROM_LDS( iLineOffset, ( iPixelOffset + iPixel ), RDI[iPixel] )
        }

        // Increment the LDS offset by PIXELS_PER_THREAD
        iPixelOffset += PIXELS_PER_THREAD;

        // First half of the kernel
        [unroll]
        for ( iIteration = 0; iIteration < KERNEL_RADIUS; iIteration += STEP_SIZE )
        {
            // Macro defines what happens for each kernel iteration
            KERNEL_ITERATION( iIteration, KD, iPixel, PIXELS_PER_THREAD, Output, RDI )

            // Macro to cache LDS reads in GPRs
            CACHE_LDS_READS( iIteration, iLineOffset, iPixelOffset, RDI )
        }

        // Prime the GPRs for the second half of the kernel
        [unroll]
        for ( iPixel = 0; iPixel < PIXELS_PER_THREAD; ++iPixel )
        {
            READ_FROM_LDS( iLineOffset, ( iPixelOffset - PIXELS_PER_THREAD + iIteration + 1 + iPixel ), RDI[iPixel] )
        }

        // Second half of the kernel
        [unroll]
        for ( iIteration = KERNEL_RADIUS + 1; iIteration < KERNEL_DIAMETER; iIteration += STEP_SIZE )
        {
            // Macro defines what happens for each kernel iteration
            KERNEL_ITERATION( iIteration, KD, iPixel, PIXELS_PER_THREAD, Output, RDI )

            // Macro to cache LDS reads in GPRs
            CACHE_LDS_READS( iIteration, iLineOffset, iPixelOffset, RDI )
        }

        // Macros define final weighting and output
        KERNEL_FINAL_WEIGHT( KD, iPixel, PIXELS_PER_THREAD, Output )
        KERNEL_OUTPUT( i2Center, i2Inc, iPixel, PIXELS_PER_THREAD, Output, KD )
    }

#endif


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
