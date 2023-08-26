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

#ifndef AMD_LIB_SERIALIZE_H
#define AMD_LIB_SERIALIZE_H

#include "AMD_Types.h"

// forward declarations
struct _iobuf;
typedef struct _iobuf FILE;

namespace AMD
{
    void serialize_string(FILE * file, char * name);

    void serialize_float(FILE * file, const char * name, float * v);
    void serialize_float2(FILE * file, const char * name, float * v);
    void serialize_float3(FILE * file, const char * name, float * v);
    void serialize_float4(FILE * file, const char * name, float * v);
    void serialize_uint(FILE * file, const char * name, uint32 * v);
    void serialize_uint2(FILE * file, const char * name, uint32 * v);
    void serialize_uint3(FILE * file, const char * name, uint32 * v);
    void serialize_uint4(FILE * file, const char * name, uint32 * v);
    void serialize_float4x4(FILE * file, const char * name, float * v);

    void deserialize_float(FILE * file, char * name, float * v, bool use_float =  false);
    void deserialize_float2(FILE * file, char * name, float * v, bool use_float =  false);
    void deserialize_float3(FILE * file, char * name, float * v, bool use_float =  false);
    void deserialize_float4(FILE * file, char * name, float * v, bool use_float =  false);
    void deserialize_uint(FILE * file, char * name, uint32 * v);
    void deserialize_uint2(FILE * file, char * name, uint32 * v);
    void deserialize_uint3(FILE * file, char * name, uint32 * v);
    void deserialize_uint4(FILE * file, char * name, uint32 * v);
    void deserialize_float4x4(FILE * file, char * name, float * v, bool use_float =  false);

    void deserialize_string(FILE * file, char * name);
}


#define AMD_SERIALIZE_STRING_FLOAT_PTR(x)    #x, (float *) &x
#define AMD_SERIALIZE_STRING_UINT_PTR(x)     #x, (uint  *) &x


#endif //AMD_LIB_SERIALIZE_H
