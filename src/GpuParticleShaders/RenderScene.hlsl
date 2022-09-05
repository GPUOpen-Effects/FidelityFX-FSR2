//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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

#include "Globals.h"


struct VS_RenderSceneInput
{
    float3 f3Position	: POSITION;  
    float3 f3Normal		: NORMAL;     
    float2 f2TexCoord	: TEXCOORD0;
	float3 f3Tangent	: TANGENT;    
};

struct PS_RenderSceneInput
{
    float4 f4Position   : SV_Position;
	float2 f2TexCoord	: TEXCOORD0;
	float3 f3Normal     : NORMAL; 
	float3 f3Tangent    : TANGENT;
	float3 f3WorldPos	: TEXCOORD2;	
};

Texture2D g_txDiffuse	: register( t2 );
Texture2D g_txNormal	: register( t3 );

//=================================================================================================================================
// This shader computes standard transform and lighting
//=================================================================================================================================
PS_RenderSceneInput VS_RenderScene( VS_RenderSceneInput I )
{
    PS_RenderSceneInput O;
     
    // Transform the position from object space to homogeneous projection space
    O.f4Position = mul( float4( I.f3Position, 1.0f ), g_mViewProjection );

    O.f3WorldPos = I.f3Position;
    O.f3Normal  = normalize( I.f3Normal );
	O.f3Tangent = normalize( I.f3Tangent );
    
	// Pass through tex coords
	O.f2TexCoord = I.f2TexCoord;
    
    return O;    
}


//=================================================================================================================================
// This shader outputs the pixel's color by passing through the lit 
// diffuse material color
//=================================================================================================================================
float4 PS_RenderScene( PS_RenderSceneInput I ) : SV_Target0
{
	float4 f4Diffuse = g_txDiffuse.Sample( g_samWrapLinear, I.f2TexCoord );
    float fSpecMask = f4Diffuse.a;
    float3 f3Norm = g_txNormal.Sample( g_samWrapLinear, I.f2TexCoord ).xyz;
    f3Norm *= 2.0f;
    f3Norm -= float3( 1.0f, 1.0f, 1.0f );
    
    float3 f3Binorm = normalize( cross( I.f3Normal, I.f3Tangent ) );
    float3x3 f3x3BasisMatrix = float3x3( f3Binorm, I.f3Tangent, I.f3Normal );
    f3Norm = normalize( mul( f3Norm, f3x3BasisMatrix ) );
   
    // Diffuse lighting
    float4 f4Lighting = saturate( dot( f3Norm, g_SunDirection.xyz ) ) * g_SunColor;
    f4Lighting += g_AmbientColor;
    
    // Calculate specular power
    float3 f3ViewDir = normalize( g_EyePosition.xyz - I.f3WorldPos );
    float3 f3HalfAngle = normalize( f3ViewDir + g_SunDirection.xyz );
    float4 f4SpecPower1 = pow( saturate( dot( f3HalfAngle, f3Norm ) ), 32 ) * g_SunColor;
    
    return f4Lighting * f4Diffuse + ( f4SpecPower1 * fSpecMask );
}



//--------------------------------------------------------------------------------------
// PS for the sky
//--------------------------------------------------------------------------------------
float4 PS_Sky( PS_RenderSceneInput I ) : SV_Target
{
    float4 f4O;

    // Bog standard textured rendering
    f4O.xyz = g_txDiffuse.Sample( g_samWrapLinear, I.f2TexCoord ).xyz;
    f4O.w = 1.0f;

    return f4O;
}