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
// File: AMD_LIB.h
//
// Library include file, to drag in all AMD LIB helper classes and functions.
//--------------------------------------------------------------------------------------
#ifndef AMD_LIB_H
#define AMD_LIB_H

// AMD helper classes and functions
#include "AMD_Types.h"

#include "../src/AMD_Common.h"
#include "../src/AMD_Texture2D.h"
#include "../src/AMD_Buffer.h"
#include "../src/AMD_Rand.h"
#include "../src/AMD_SaveRestoreState.h"
#include "../src/AMD_FullscreenPass.h"
#include "../src/AMD_UnitCube.h"

#ifdef _DEBUG
#include "../src/AMD_Serialize.h"
#include "../src/DirectXTex/DDSTextureLoader.h"
#include "../src/DirectXTex/ScreenGrab.h"
#endif

#endif
