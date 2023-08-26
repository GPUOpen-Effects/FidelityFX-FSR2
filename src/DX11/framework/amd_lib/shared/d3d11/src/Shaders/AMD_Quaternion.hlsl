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

#ifndef AMD_LIB_QUATERNION_HLSL
#define AMD_LIB_QUATERNION_HLSL

float4 MakeQuaternion(float angle_radian, float3 axis)
{
  // create quaternion using angle and rotation axis
  float4 quaternion;
  float halfAngle = 0.5f * angle_radian;
  float sinHalf = sin(halfAngle);

  quaternion.w = cos(halfAngle);
  quaternion.xyz = sinHalf * axis.xyz;

  return quaternion;
}

float4 InverseQuaternion(float4 q)
{
  float lengthSqr = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;

  if (lengthSqr < 0.001)
  {
    return float4(0, 0, 0, 1.0f);
  }

  q.x = -q.x / lengthSqr;
  q.y = -q.y / lengthSqr;
  q.z = -q.z / lengthSqr;
  q.w =  q.w / lengthSqr;

  return q;
}

float3 MultQuaternionAndVector(float4 q, float3 v)
{
  float3 uv, uuv;
  float3 qvec = float3(q.x, q.y, q.z);
  uv = cross(qvec, v);
  uuv = cross(qvec, uv);
  uv *= (2.0f * q.w);
  uuv *= 2.0f;

  return v + uv + uuv;
}

float4 MultQuaternionAndQuaternion(float4 qA, float4 qB)
{
  float4 q;

  q.w = qA.w * qB.w - qA.x * qB.x - qA.y * qB.y - qA.z * qB.z;
  q.x = qA.w * qB.x + qA.x * qB.w + qA.y * qB.z - qA.z * qB.y;
  q.y = qA.w * qB.y + qA.y * qB.w + qA.z * qB.x - qA.x * qB.z;
  q.z = qA.w * qB.z + qA.z * qB.w + qA.x * qB.y - qA.y * qB.x;

  return q;
}

float4 AngularVelocityToSpin(float4 orientation, float3 angular_veloctiy)
{
  return 0.5f * MultQuaternionAndQuaternion(float4(angular_veloctiy.xyz, 0), orientation);
}

float3 MultWorldInertiaInvAndVector(float4 orientation, float3 inertia, float3 vec)
{
  float4 inv_orientation = float4(-orientation.xyz, orientation.w) / length(orientation);
  float3 inv_inertia = 1.0f / inertia;

  float3 InertiaInv_RotT_vec = inv_inertia * MultQuaternionAndVector(inv_orientation, vec );
  float3 Rot_InertiaInv_RotT_vec = MultQuaternionAndVector(orientation, InertiaInv_RotT_vec );

  return Rot_InertiaInv_RotT_vec;
}

#endif // AMD_LIB_QUATERNION_HLSL
