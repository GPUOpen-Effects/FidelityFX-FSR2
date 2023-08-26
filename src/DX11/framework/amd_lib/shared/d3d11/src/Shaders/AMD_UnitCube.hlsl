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

struct PS_UnitCubeInput
{
  float4                ss_position         : SV_Position;
};

struct UnitCubeTransform
{
  float4x4              m_tr;
  float4x4              m_inverse;
  float4x4              m_forward;
  float4                m_color;
};

cbuffer CB_UNIT_CUBE_TRANSFORM              :  register (c0)
{
    UnitCubeTransform   g_UnitCubeTransform;
}

SamplerState g_ssUnitCube                   : register(s0);
Texture2D    g_t2dUnitCube                  : register(t0);

PS_UnitCubeInput vsClipSpaceCube( uint vertex_id : SV_VERTEXID )
{
  float4 vertex[] =
  {
    { -1.0000, -1.0000,  1.0000, 1 },
    { -1.0000, -1.0000, -0.0000, 1 },
    {  1.0000, -1.0000, -0.0000, 1 },
    {  1.0000, -1.0000,  1.0000, 1 },
    { -1.0000,  1.0000,  1.0000, 1 },
    {  1.0000,  1.0000,  1.0000, 1 },
    {  1.0000,  1.0000, -0.0000, 1 },
    { -1.0000,  1.0000, -0.0000, 1 }
  };

  int index[]=
  {
     1, 2, 3,
     3, 4, 1,
     5, 6, 7,
     7, 8, 5,
     1, 4, 6,
     6, 5, 1,
     4, 3, 7,
     7, 6, 4,
     3, 2, 8,
     8, 7, 3,
     2, 1, 5,
     5, 8, 2
  };

  PS_UnitCubeInput Output;
  Output.ss_position = mul (vertex[index[vertex_id] - 1], g_UnitCubeTransform.m_tr );

  return Output;
}

PS_UnitCubeInput vsUnitCube( uint vertex_id : SV_VERTEXID )
{
  float4 vertex[] =
  {
    { -1.0000, -1.0000,  1.0000, 1 },
    { -1.0000, -1.0000, -1.0000, 1 },
    {  1.0000, -1.0000, -1.0000, 1 },
    {  1.0000, -1.0000,  1.0000, 1 },
    { -1.0000,  1.0000,  1.0000, 1 },
    {  1.0000,  1.0000,  1.0000, 1 },
    {  1.0000,  1.0000, -1.0000, 1 },
    { -1.0000,  1.0000, -1.0000, 1 }
  };

  int index[]=
  {
     1, 2, 3,
     3, 4, 1,
     5, 6, 7,
     7, 8, 5,
     1, 4, 6,
     6, 5, 1,
     4, 3, 7,
     7, 6, 4,
     3, 2, 8,
     8, 7, 3,
     2, 1, 5,
     5, 8, 2
  };

  PS_UnitCubeInput Output;
  Output.ss_position = mul (vertex[index[vertex_id] - 1], g_UnitCubeTransform.m_tr );

  return Output;
}

float4 psUnitCube( PS_UnitCubeInput In ) : SV_Target0
{
  return g_UnitCubeTransform.m_color;
}
