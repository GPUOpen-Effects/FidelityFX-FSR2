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
// File: FilterCommon.hlsl
//
// Common defines for separable filtering kernels
//--------------------------------------------------------------------------------------


// Defines passed in at compile time
//#define LDS_PRECISION               ( 8 , 16, or 32 )
//#define USE_APPROXIMATE_FILTER      ( 0, 1 )
//#define USE_COMPUTE_SHADER
//#define KERNEL_RADIUS               ( 16 )   // Must be an even number


// Defines that control the CS logic of the kernel
#define KERNEL_DIAMETER             ( KERNEL_RADIUS * 2 + 1 )
#define RUN_LINES                   ( 2 )   // Needs to match g_uRunLines in SeparableFilter11.cpp
#define RUN_SIZE                    ( 128 ) // Needs to match g_uRunSize in SeparableFilter11.cpp
#define KERNEL_DIAMETER_MINUS_ONE   ( KERNEL_DIAMETER - 1 )
#define RUN_SIZE_PLUS_KERNEL        ( RUN_SIZE + KERNEL_DIAMETER_MINUS_ONE )
#define PIXELS_PER_THREAD           ( 4 )
#define NUM_THREADS                 ( RUN_SIZE / PIXELS_PER_THREAD )
#define SAMPLES_PER_THREAD          ( RUN_SIZE_PLUS_KERNEL / NUM_THREADS )
#define EXTRA_SAMPLES               ( RUN_SIZE_PLUS_KERNEL - ( NUM_THREADS * SAMPLES_PER_THREAD ) )


// The samplers
SamplerState g_PointSampler : register (s0);
SamplerState g_LinearClampSampler : register (s1);


// Adjusts the sampling step size if using approximate filtering
#if ( USE_APPROXIMATE_FILTER == 1 )

    #define STEP_SIZE ( 2 )

#else

    #define STEP_SIZE ( 1 )

#endif


// Constant buffer used by the CS & PS
cbuffer cbSF : register( b0 )
{
    float4    g_f4OutputSize;   // x = Width, y = Height, z = Inv Width, w = Inv Height
}


// Input structure used by the PS
struct PS_RenderQuadInput
{
    float4 f4Position : SV_POSITION;
    float2 f2TexCoord : TEXCOORD0;
};

#if ( REQUIRE_HDR == 1 )

    //--------------------------------------------------------------------------------------
    // Packs a float2 to a unit
    //--------------------------------------------------------------------------------------
    uint Float2ToUint( float2 f2Value )
    {
        return ( f32tof16( f2Value.x ) ) + ( f32tof16( f2Value.y ) << 16 );
    }


    //--------------------------------------------------------------------------------------
    // Unpacks a uint to a float2
    //--------------------------------------------------------------------------------------
    float2 UintToFloat2( uint uValue )
    {
        return float2( f16tof32( uValue ), f16tof32( uValue >> 16 ) );
    }

#else

    //--------------------------------------------------------------------------------------
    // Packs a float2 to a unit
    //--------------------------------------------------------------------------------------
    uint Float2ToUint( float2 f2Value )
    {
        return ( ( ( (uint)( f2Value.y * 65535.0f ) ) << 16 ) |
                 ( (uint)( f2Value.x * 65535.0f ) ) );
    }


    //--------------------------------------------------------------------------------------
    // Unpacks a uint to a float2
    //--------------------------------------------------------------------------------------
    float2 UintToFloat2( uint uValue )
    {
        return float2(  ( uValue & 0x0000FFFF ) / 65535.0f,
                      ( ( uValue & 0xFFFF0000 ) >> 16 ) / 65535.0f );
    }

#endif

//--------------------------------------------------------------------------------------
// Packs a float4 to a unit
//--------------------------------------------------------------------------------------
uint Float4ToUint( float4 f4Value )
{
    return (    ( ( (uint)( f4Value.w * 255.0f ) ) << 24 ) |
                ( ( (uint)( f4Value.z * 255.0f ) ) << 16 ) |
                ( ( (uint)( f4Value.y * 255.0f ) ) << 8 ) |
                ( (uint)( f4Value.x * 255.0f ) ) );
}


//--------------------------------------------------------------------------------------
// Unpacks a uint to a float4
//--------------------------------------------------------------------------------------
float4 UintToFloat4( uint uValue )
{
    return float4(  ( uValue & 0x000000FF ) / 255.0f,
                    ( ( uValue & 0x0000FF00 ) >> 8 ) / 255.0f,
                    ( ( uValue & 0x00FF0000 ) >> 16 ) / 255.0f,
                    ( ( uValue & 0xFF000000 ) >> 24 ) / 255.0f );
}


//--------------------------------------------------------------------------------------
// Packs a float3 to a unit
//--------------------------------------------------------------------------------------
uint Float3ToUint( float3 f3Value )
{
    return (    ( ( (uint)( f3Value.z * 255.0f ) ) << 16 ) |
                ( ( (uint)( f3Value.y * 255.0f ) ) << 8 ) |
                ( (uint)( f3Value.x * 255.0f ) ) );
}


//--------------------------------------------------------------------------------------
// Unpacks a uint to a float3
//--------------------------------------------------------------------------------------
float3 UintToFloat3( uint uValue )
{
    return float3(  ( uValue & 0x000000FF ) / 255.0f,
                    ( ( uValue & 0x0000FF00 ) >> 8 ) / 255.0f,
                    ( ( uValue & 0x00FF0000 ) >> 16 ) / 255.0f );
}


//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
