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

#include <string>
#include <stdio.h>

#include "AMD_Types.h"
#include "AMD_Serialize.h"

#pragma warning (disable : 4996)

namespace AMD
{
    AMD_COMPILE_TIME_ASSERT( sizeof(uchar)  == 1, size_of_uchar);
    AMD_COMPILE_TIME_ASSERT( sizeof(ushort) == 2, size_of_ushort);
    AMD_COMPILE_TIME_ASSERT( sizeof(uint)   == 4, size_of_uint);
    AMD_COMPILE_TIME_ASSERT( sizeof(ulong)  == 8, size_of_ulong);
    AMD_COMPILE_TIME_ASSERT( sizeof(real32) == 4, size_of_real32);
    AMD_COMPILE_TIME_ASSERT( sizeof(real64) == 8, size_of_real64);

    void serialize_string(FILE * file, char * name)
    {
        fprintf(file, "%s\n", name);
    }

    void serialize_float(FILE * file, const char * name, float * v)
    {
        fprintf(file, "#%s = %.10f; \n", name, v[0]);
        serialize_uint(file, name, (uint *) v);
    }

    void serialize_float2(FILE * file, const char * name, float * v)
    {
        fprintf(file, "#%s = %.10f %.10f; \n", name, v[0], v[1]);
        serialize_uint2(file, name, (uint *) v);
    }

    void serialize_float3(FILE * file, const char * name, float * v)
    {
        fprintf(file, "#%s = %.10f %.10f %.10f; \n", name, v[0], v[1], v[2]);
        serialize_uint3(file, name, (uint *) v);
    }

    void serialize_float4(FILE * file, const char * name, float * v)
    {
        fprintf(file, "#%s = %.10f %.10f %.10f %.10f; \n", name, v[0], v[1], v[2], v[3]);
        serialize_uint4(file, name, (uint *) v);
    }

    void serialize_float4x4(FILE * file, const char * name, float * v)
    {
        std::string row_name;
        row_name = std::string(name) + "[0]";
        serialize_float4(file, row_name.c_str(), &v[0]);
        row_name = std::string(name) + "[1]";
        serialize_float4(file, row_name.c_str(), &v[4]);
        row_name = std::string(name) + "[2]";
        serialize_float4(file, row_name.c_str(), &v[8]);
        row_name = std::string(name) + "[3]";
        serialize_float4(file, row_name.c_str(), &v[12]);
    }

    void serialize_uint(FILE * file, const char * name, uint32 * v)
    {
        fprintf(file, "%s = %X; \n", name, v[0]);
    }

    void serialize_uint2(FILE * file, const char * name, uint32 * v)
    {
        fprintf(file, "%s = %X %X; \n", name, v[0], v[1]);
    }

    void serialize_uint3(FILE * file, const char * name, uint32 * v)
    {
        fprintf(file, "%s = %X %X %X; \n", name, v[0], v[1], v[2]);
    }

    void serialize_uint4(FILE * file, const char * name, uint32 * v)
    {
        fprintf(file, "%s = %X %X %X %X; \n", name, v[0], v[1], v[2], v[3]);
    }

    void deserialize_string(FILE * file, char * name)
    {
        fscanf(file, "%s\n", name);
    }

    void deserialize_float(FILE * file, char * name, float * v, bool use_float)
    {
        float f[4];
        fscanf(file, "#%s = %f; \n", name, &v[0]);

        if (use_float)
        {
            deserialize_uint(file, name, (uint *) f);
        }
        else
        {
            deserialize_uint(file, name, (uint *) v);
        }
    }

    void deserialize_float2(FILE * file, char * name, float * v, bool use_float)
    {
        float f[4];
        fscanf(file, "#%s = %f %f; \n", name, &v[0], &v[1]);

        if (use_float)
        {
            deserialize_uint2(file, name, (uint *) f);
        }
        else
        {
            deserialize_uint2(file, name, (uint *) v);
        }
    }

    void deserialize_float3(FILE * file, char * name, float * v, bool use_float)
    {
        float f[4];
        fscanf(file, "#%s = %f %f %f; \n", name, &v[0], &v[1], &v[2]);

        if (use_float)
        {
            deserialize_uint3(file, name, (uint *) f);
        }
        else
        {
            deserialize_uint3(file, name, (uint *) v);
        }
    }

    void deserialize_float4(FILE * file, char * name, float * v, bool use_float)
    {
        float f[4];
        fscanf(file, "#%s = %f %f %f %f; \n", name, &v[0], &v[1], &v[2], &v[3]);

        if (use_float)
        {
            deserialize_uint4(file, name, (uint *) f);
        }
        else
        {
            deserialize_uint4(file, name, (uint *) v);
        }
    }

    void deserialize_float4x4(FILE * file, char * name, float * v, bool use_float)
    {
        AMD::deserialize_float4(file, name, &v[0], use_float);
        AMD::deserialize_float4(file, name, &v[4], use_float);
        AMD::deserialize_float4(file, name, &v[8], use_float);
        AMD::deserialize_float4(file, name, &v[12], use_float);
    }

    void deserialize_uint(FILE * file, char * name, uint32 * v)
    {
        fscanf(file, "%s = %X; \n", name, &v[0]);
    }

    void deserialize_uint2(FILE * file, char * name, uint32 * v)
    {
        fscanf(file, "%s = %X %X; \n", name, &v[0], &v[1]);
    }

    void deserialize_uint3(FILE * file, char * name, uint32 * v)
    {
        fscanf(file, "%s = %X %X %X; \n", name, &v[0], &v[1], &v[2]);
    }

    void deserialize_uint4(FILE * file, char * name, uint32 * v)
    {
        fscanf(file, "%s = %X %X %X %X; \n", name, &v[0], &v[1], &v[2], &v[3]);
    }
}
