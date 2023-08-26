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

#ifndef AMD_LIB_TYPES_H
#define AMD_LIB_TYPES_H

namespace AMD
{
    typedef          long long int64;
    typedef          int       int32;
    typedef          short     int16;
    typedef          char      int8;
    typedef          char      byte;

    typedef          long long sint64;
    typedef          int       sint32;
    typedef          short     sint16;
    typedef          char      sint8;
    typedef          int       sint;
    typedef          short     sshort;
    typedef          char      sbyte;
    typedef          char      schar;

    typedef unsigned long long uint64;
    typedef unsigned int       uint32;
    typedef unsigned short     uint16;
    typedef unsigned char      uint8;

    typedef          float     real32;
    typedef          double    real64;

    typedef unsigned long long ulong;
    typedef unsigned int       uint;
    typedef unsigned short     ushort;
    typedef unsigned char      ubyte;
    typedef unsigned char      uchar;

    template <class T>
    T MIN(T a, T b)
    {
        return (a > b) ? b : a;
    }

    template <class T>
    T MAX(T a, T b)
    {
        return (a > b) ? a : b;
    }

    // The common return codes
    typedef enum RETURN_CODE_t
    {
        RETURN_CODE_SUCCESS,
        RETURN_CODE_FAIL,
        RETURN_CODE_INVALID_DEVICE,
        RETURN_CODE_INVALID_DEVICE_CONTEXT,
        RETURN_CODE_COUNT,
    } RETURN_CODE;
}

#ifndef AMD_DECLARE_BASIC_VECTOR_TYPE
# define AMD_DECLARE_BASIC_VECTOR_TYPE                                                      \
    struct float2   { union { struct { float x, y; }; float v[2]; }; };                     \
    struct float3   { union { struct { float x, y, z; }; float v[3]; }; };                  \
    struct float4   { union { struct { float x, y, z, w; }; float v[4]; }; };               \
    struct float4x4 { union { float4 r[4]; float m[16]; }; };                               \
    struct uint2    { union { struct { unsigned int x, y; }; unsigned int v[2]; }; };       \
    struct uint3    { union { struct { unsigned int x, y, z; }; unsigned int v[3]; }; };    \
    struct uint4    { union { struct { unsigned int x, y, z, w; }; unsigned int v[4]; }; }; \
    struct sint2    { union { struct { int x, y; }; int v[2]; }; };                         \
    struct sint3    { union { struct { int x, y, z; }; int v[3]; }; };                      \
    struct sint4    { union { struct { int x, y, z, w; }; int v[4]; }; };                   \
    struct sshort2  { union { struct { short x, y; }; short v[2]; }; };                     \
    struct sshort3  { union { struct { short x, y, z; }; short v[3]; }; };                  \
    struct sshort4  { union { struct { short x, y, z, w; }; short v[4]; }; };               \
    struct sbyte2   { union { struct { signed char x, y; }; signed char v[2]; }; };         \
    struct sbyte3   { union { struct { signed char x, y, z; }; signed char v[3]; }; };      \
    struct sbyte4   { union { struct { signed char x, y, z, w; }; signed char v[4]; }; };
#endif // AMD_DECLARE_BASIC_VECTOR_TYPE

#ifndef AMD_DECLARE_CAMERA_TYPE
# define AMD_DECLARE_CAMERA_TYPE                                                            \
    typedef struct Camera_t                                                                 \
    {                                                                                       \
        float4x4                           m_View;                                          \
        float4x4                           m_Projection;                                    \
        float4x4                           m_ViewProjection;                                \
        float4x4                           m_View_Inv;                                      \
        float4x4                           m_Projection_Inv;                                \
        float4x4                           m_ViewProjection_Inv;                            \
        float3                             m_Position;                                      \
        float                              m_Fov;                                           \
        float3                             m_Direction;                                     \
        float                              m_FarPlane;                                      \
        float3                             m_Right;                                         \
        float                              m_NearPlane;                                     \
        float3                             m_Up;                                            \
        float                              m_Aspect;                                        \
        float4                             m_Color;                                         \
    } Camera;
#endif // AMD_DECLARE_CAMERA_TYPE

#ifndef AMD_DECLARE_MODEL_TYPE
# define AMD_DECLARE_MODEL_TYPE                                                             \
    typedef struct MODEL_t                                                                  \
    {                                                                                       \
        float4x4                           m_World;                                         \
        float4x4                           m_World_Inv;                                     \
        float4x4                           m_WorldView;                                     \
        float4x4                           m_WorldView_Inv;                                 \
        float4x4                           m_WorldViewProjection;                           \
        float4x4                           m_WorldViewProjection_Inv;                       \
        float4                             m_Position;                                      \
        float4                             m_Orientation;                                   \
        float4                             m_Scale;                                         \
        float4                             m_Ambient;                                       \
        float4                             m_Diffuse;                                       \
        float4                             m_Specular;                                      \
    } MODEL;
#endif // AMD_DECLARE_MODEL_TYPE

#ifndef AMD_DECLARE_RESOURCE_2D_DESC_TYPE
# define AMD_DECLARE_RESOURCE_2D_DESC_TYPE                                                  \
    typedef struct RESOURCE_2D_DESC_t                                                       \
    {                                                                                       \
        float2                             m_Size;                                          \
        float2                             m_Size_Inv;                                      \
        float2                             m_Scale;                                         \
        float2                             m_Offset;                                        \
    } RESOURCE_2D_DESC;
#endif // AMD_DECLARE_RESOURCE_2D_DESC_TYPE

#ifndef AMD_SAFE_DELETE
#define AMD_SAFE_DELETE(p)        { delete (p);     (p) = nullptr; }
#endif
#ifndef AMD_SAFE_DELETE_ARRAY
#define AMD_SAFE_DELETE_ARRAY(p)  { delete [] (p);   (p) = nullptr; }
#endif
#ifndef AMD_SAFE_RELEASE
#define AMD_SAFE_RELEASE(p)       { if (p) { (p)->Release(); } (p) = nullptr; }
#endif

#define AMD_FUNCTION_WIDEN2(x)    L ## x
#define AMD_FUNCTION_WIDEN(x)     AMD_FUNCTION_WIDEN2(x)
#define AMD_FUNCTION_WIDE_NAME    AMD_FUNCTION_WIDEN(__FUNCTION__)
#define AMD_FUNCTION_NAME         __FUNCTION__

#define AMD_PITCHED_SIZE(x, y)    ( (x+y-1) / y )

#define AMD_ARRAY_SIZE( arr )     ( sizeof(arr) / sizeof(arr[0]) )

#define AMD_PI                    3.141592654f
#define AMD_ROT_ANGLE             ( AMD_PI / 180.f )

#define AMD_COMPILE_TIME_ASSERT(condition, name) unsigned char g_AMD_CompileTimeAssertExpression ## name [ (condition) ? 1 : -1];

#endif //AMD_LIB_TYPES_H
