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

#ifndef AMD_LIB_FULLSCREEN_PASS_HLSL
#define AMD_LIB_FULLSCREEN_PASS_HLSL

struct PS_FullscreenInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

struct GS_FullscreenIndexRTInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    uint   rtIndex  : RT_INDEX;
};

struct PS_FullscreenIndexRTInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    uint   rtIndex  : SV_RenderTargetArrayIndex;
};

// The ScreenQuad shader is used to clear a subregion of a depth map
// Cleaning a rectangle in depth map is otherwise problematic
PS_FullscreenInput vsScreenQuad(uint vertexId : SV_VERTEXID)
{

    PS_FullscreenInput vertex[6] =
    {
        { -1.0f, -1.0f, 1.0f, 1.0f }, { 0.0f,  1.0f },
        { -1.0f,  1.0f, 1.0f, 1.0f }, { 0.0f,  0.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f }, { 1.0f,  1.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f }, { 1.0f,  1.0f },
        { -1.0f,  1.0f, 1.0f, 1.0f }, { 0.0f,  0.0f },
        {  1.0f,  1.0f, 1.0f, 1.0f }, { 1.0f,  0.0f },
    };

    return vertex[vertexId % 6];
}

PS_FullscreenInput vsFullscreen(uint vertexId : SV_VERTEXID)
{

    PS_FullscreenInput vertex[3] =
    {
        { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f,  1.0f },
        { -1.0f,  3.0f, 0.0f, 1.0f }, { 0.0f, -1.0f },
        {  3.0f, -1.0f, 0.0f, 1.0f }, { 2.0f,  1.0f }
    };

    return vertex[vertexId % 3];
}

GS_FullscreenIndexRTInput vsFullscreenIndexRT(uint indexId : SV_VERTEXID)
{
    GS_FullscreenIndexRTInput vertex[3] =
    {
        { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f,  1.0f }, 0,
        { -1.0f,  3.0f, 0.0f, 1.0f }, { 0.0f, -1.0f }, 0,
        {  3.0f, -1.0f, 0.0f, 1.0f }, { 2.0f,  1.0f }, 0
    };

    uint vertexId = indexId % 3;
    uint instanceId = indexId / 3;

    GS_FullscreenIndexRTInput Out = vertex[vertexId];
    Out.rtIndex = instanceId;

    return Out;
}

[maxvertexcount(3)]
void gsFullscreenIndexRT(triangle GS_FullscreenIndexRTInput In[3], inout TriangleStream<PS_FullscreenIndexRTInput> OutStream)
{
    PS_FullscreenIndexRTInput Out;

    for (int i = 0; i < 3; i++)
    {
        PS_FullscreenIndexRTInput Out;
        Out.position = In[i].position;
        Out.texCoord = In[i].texCoord;
        Out.rtIndex = In[i].rtIndex;
        OutStream.Append(Out);
    }
    OutStream.RestartStrip();
}

SamplerState g_ssFullscreen   : register(s0);
Texture2D    g_t2dFullscreen  : register(t0);

float4 psFullscreen(PS_FullscreenInput In) : SV_Target0
{
    return g_t2dFullscreen.Sample(g_ssFullscreen, In.texCoord).xyzw;
}

#endif // AMD_LIB_FULLSCREEN_PASS_HLSL
