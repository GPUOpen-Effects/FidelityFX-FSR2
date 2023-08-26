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
// File: HorizontalFilter.hlsl
//
// Implements the horizontal pass of the kernel.
//--------------------------------------------------------------------------------------


#if ( USE_COMPUTE_SHADER == 1 )

    //--------------------------------------------------------------------------------------
    // Compute shader implementing the horizontal pass of a separable filter
    //--------------------------------------------------------------------------------------
    [numthreads( NUM_THREADS, RUN_LINES, 1 )]
    void CSFilterX( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID )
    {
        // Sampling and line offsets from group thread IDs
        int iSampleOffset = GTid.x * SAMPLES_PER_THREAD;
        int iLineOffset = GTid.y;

        // Group and pixel coords from group IDs
        int2 i2GroupCoord = int2( ( Gid.x * RUN_SIZE ) - KERNEL_RADIUS, ( Gid.y * RUN_LINES ) );
        int2 i2Coord = int2( i2GroupCoord.x + iSampleOffset, i2GroupCoord.y );

        // Sample and store to LDS
        [unroll]
        for ( int i = 0; i < SAMPLES_PER_THREAD; ++i )
        {
            WRITE_TO_LDS( Sample( i2Coord + int2( i, GTid.y ), float2( 0.5f, 0.0f ) ), iLineOffset, iSampleOffset + i )
        }

        // Optionally load some extra texels as required by the exact kernel size
        if ( GTid.x < EXTRA_SAMPLES )
        {
            WRITE_TO_LDS( Sample( i2GroupCoord + int2( RUN_SIZE_PLUS_KERNEL - 1 - GTid.x, GTid.y ), float2( 0.5f, 0.0f ) ), iLineOffset, RUN_SIZE_PLUS_KERNEL - 1 - GTid.x )
        }

        // Sync threads
        GroupMemoryBarrierWithGroupSync();

        // Adjust pixel offset for computing at PIXELS_PER_THREAD
        int iPixelOffset = GTid.x * PIXELS_PER_THREAD;
        i2Coord = int2( i2GroupCoord.x + iPixelOffset, i2GroupCoord.y );

        // Since we start with the first thread position, we need to increment the coord by KERNEL_RADIUS
        i2Coord.x += KERNEL_RADIUS;

        // Ensure we don't compute pixels off screen
        if ( i2Coord.x < g_f4OutputSize.x )
        {
            int2 i2Center = i2Coord + int2( 0, GTid.y );
            int2 i2Inc = int2( 1, 0 );

            // Compute the filter kernel using LDS values
            ComputeFilterKernel( iPixelOffset, iLineOffset, i2Center, i2Inc );
        }
    }

#else // USE_COMPUTE_SHADER

    //--------------------------------------------------------------------------------------
    // Pixel shader implementing the horizontal pass of a separable filter
    //--------------------------------------------------------------------------------------
    PS_Output PSFilterX( PS_RenderQuadInput I ) : SV_TARGET
    {
        PS_Output Output = (PS_Output)0;
        RAWDataItem RDI[1];
        int iPixel, iIteration;
        KernelData KD[1];

        // Load the center sample(s)
        int2 i2KernelCenter = int2( g_f4OutputSize.xy * I.f2TexCoord );
        RDI[0] = Sample( int2( i2KernelCenter.x, i2KernelCenter.y ), float2( 0.0f, 0.0f ) );

        // Macro defines what happens at the kernel center
        KERNEL_CENTER( KD, iPixel, 1, Output, RDI )

        i2KernelCenter.x -= KERNEL_RADIUS;

        // First half of the kernel
        [unroll]
        for ( iIteration = 0; iIteration < KERNEL_RADIUS; iIteration += STEP_SIZE )
        {
            // Load the sample(s) for this iteration
            RDI[0] = Sample( int2( i2KernelCenter.x + iIteration, i2KernelCenter.y ), float2( 0.5f, 0.0f ) );

            // Macro defines what happens for each kernel iteration
            KERNEL_ITERATION( iIteration, KD, iPixel, 1, Output, RDI )
        }

        // Second half of the kernel
        [unroll]
        for ( iIteration = KERNEL_RADIUS + 1; iIteration < KERNEL_DIAMETER; iIteration += STEP_SIZE )
        {
            // Load the sample(s) for this iteration
            RDI[0] = Sample( int2( i2KernelCenter.x + iIteration, i2KernelCenter.y ), float2( 0.5f, 0.0f ) );

            // Macro defines what happens for each kernel iteration
            KERNEL_ITERATION( iIteration, KD, iPixel, 1, Output, RDI )
        }

        // Macros define final weighting
        KERNEL_FINAL_WEIGHT( KD, iPixel, 1, Output )

        return Output;
    }

#endif // USE_COMPUTE_SHADER


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
