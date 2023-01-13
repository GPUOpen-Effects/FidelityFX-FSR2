# FidelityFX Super Resolution 2.1 (FSR 2.1.2)

Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

![Screenshot](screenshot.png)

AMD FidelityFX Super Resolution 2 (FSR 2) is an open source, high-quality solution for producing high resolution frames from lower resolution inputs.

You can find the binaries for FidelityFX FSR in the release section on GitHub.

# Super Resolution 2

### Table of contents

- [Introduction](#introduction)
    - [Shading language requirements](#shading-language-requirements)
- [Quick start checklist](#quick-start-checklist)
- [Integration guidelines](#integration-guidelines)
    - [Quality modes](#quality-modes)
    - [Performance](#performance)
    - [Memory requirements](#memory-requirements)
    - [Input resources](#input-resources)
    - [Depth buffer configurations](#depth-buffer-configurations)
    - [Providing motion vectors](#providing-motion-vectors)
    - [Reactive mask](#reactive-mask)
    - [Transparency & composition mask](#transparency-and-composition-mask)
    - [Automatically generating reactivity](#automatically-generating-reactivity)
    - [Placement in the frame](#placement-in-the-frame)
    - [Host API](#host-api)
	- [Modular backend](#modular-backend)
    - [Memory management](#memory-management)
    - [Temporal antialiasing](#temporal-antialiasing)
    - [Camera jitter](#camera-jitter)
	- [Camera jump cuts](#camera-jump-cuts)
    - [Mipmap biasing](#mipmap-biasing)
    - [Frame Time Delta Input](#frame-time-delta-input)
    - [HDR support](#hdr-support)
    - [Falling back to 32bit floating point](#falling-back-to-32bit-floating-point)
    - [64-wide wavefronts](#64-wide-wavefronts)
- [The technique](#the-technique)
    - [Algorithm structure](#algorithm-structure)
    - [Compute luminance pyramid](#compute-luminance-pyramid)
    - [Adjust input color](#adjust-input-color)
    - [Reconstruct & dilate](#reconstruct-and-dilate)
    - [Depth clip](#depth-clip)
    - [Create locks](#create-locks)
    - [Reproject & accumulate](#reproject-accumulate)
    - [Robust Contrast Adaptive Sharpening (RCAS)](#robust-contrast-adaptive-sharpening-rcas)
- [Building the sample](#building-the-sample)
- [Limitations](#limitations)
- [Version history](#version-history)
- [References](#references)

# Introduction
**FidelityFX Super Resolution 2** (or **FSR2** for short) is a cutting-edge upscaling technique developed from the ground up to produce high resolution frames from lower resolution inputs.

![alt text](docs/media/super-resolution-temporal/overview.svg "A diagram showing the input resources to the super resolution (temporal) algorithm.")

FSR2 uses temporal feedback to reconstruct high-resolution images while maintaining and even improving image quality compared to native rendering.

FSR2 can enable “practical performance” for costly render operations, such as hardware ray tracing.

## Shading language requirements
`HLSL` `CS_6_2` `CS_6_6*`

\* - CS_6_6 is used on some hardware which supports 64-wide wavefronts.

# Quick start checklist
To use FSR2 you should follow the steps below:

1. Double click [`GenerateSolutions.bat`](build/GenerateSolutions.bat) in the [`build`](build) directory.

2. Open the solution matching your API, and build the solution.

3. Copy the API library from `bin/ffx_fsr2_api` into the folder containing a folder in your project which contains third-party libraries.

4. Copy the library matching the FSR2 backend you want to use, e.g.: `bin/ffx_fsr2_api/ffx_fsr2_api_dx12_x64.lib` for DirectX12.

5. Copy the following core API header files from src/ffx-fsr2-api into your project: `ffx_fsr2.h`, `ffx_types.h`, `ffx_error.h`, `ffx_fsr2_interface.h`, `ffx_util.h`, `shaders/ffx_fsr2_common.h`, and `shaders/ffx_fsr2_resources.h`. Care should be taken to maintain the relative directory structure at the destination of the file copying.

6. Copy the header files for the API backend of your choice, e.g. for DirectX12 you would copy `dx12/ffx_fsr2_dx12.h` and `dx12/shaders/ffx_fsr2_shaders_dx12.h`. Care should be taken to maintain the relative directory structure at the destination of the file copying.

7. Include the `ffx_fsr2.h` header file in your codebase where you wish to interact with FSR2.

8. Create a backend for your target API. E.g. for DirectX12 you should call [`ffxFsr2GetInterfaceDX12`](src/ffx-fsr2-api/dx12/ffx_fsr2_dx12.h#L55). A scratch buffer should be allocated of the size returned by calling [`ffxFsr2GetScratchMemorySizeDX12`](src/ffx-fsr2-api/dx12/ffx_fsr2_dx12.h#L40) and the pointer to that buffer passed to [`ffxFsr2GetInterfaceDX12`](src/ffx-fsr2-api/dx12/ffx_fsr2_dx12.h#L55).

9. Create a FSR2 context by calling [`ffxFsr2ContextCreate`](src/ffx-fsr2-api/ffx_fsr2.h#L215). The parameters structure should be filled out matching the configuration of your application. See the API reference documentation for more details.

10. Each frame you should call [`ffxFsr2ContextDispatch`](src/ffx-fsr2-api/ffx_fsr2.h#L256) to launch FSR2 workloads. The parameters structure should be filled out matching the configuration of your application. See the API reference documentation for more details, and ensure the [`frameTimeDelta` field is provided in milliseconds](#frame-time-delta-input).

11. When your application is terminating (or you wish to destroy the context for another reason) you should call [`ffxFsr2ContextDestroy`](src/ffx-fsr2-api/ffx_fsr2.h#L279). The GPU should be idle before calling this function.

12. Sub-pixel jittering should be applied to your application's projection matrix. This should be done when performing the main rendering of your application. You should use the [`ffxFsr2GetJitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L424) function to compute the precise jitter offsets. See [Camera jitter](#camera-jitter) section for more details.

13. For the best upscaling quality it is strongly advised that you populate the [Reactive mask](#reactive-mask) and [Transparency & composition mask](#transparency-and-composition-mask) according to our guidelines. You can also use [`ffxFsr2ContextGenerateReactiveMask`](src/ffx-fsr2-api/ffx_fsr2.h#L267) as a starting point.

14. Applications should expose [scaling modes](#scaling-modes), in their user interface in the following order: Quality, Balanced, Performance, and (optionally) Ultra Performance.

15. Applications should also expose a sharpening slider to allow end users to achieve additional quality.

# Integration guidelines

## Scaling modes
For the convenience of end users, the FSR2 API provides a number of preset scaling ratios which are named.

| Quality           | Per-dimension scaling factor |    
|-------------------|------------------------------|
| Quality           | 1.5x                         |
| Balanced          | 1.7x                         |
| Performance       | 2.0x                         |
| Ultra performance | 3.0x                         |

We strongly recommend that applications adopt consistent naming and scaling ratios in their user interface. This is to ensure that user experience is consistent for your application's users which may have experience of other applications using FSR2. 

## Performance
Depending on your target hardware and operating configuration FSR2 will operate at different performance levels.

The table below summarizes the measured performance of FSR2 on a variety of hardware in DX12.

| Target resolution | Quality          | RX 6950 XT | RX 6900 XT | RX 6800 XT | RX 6800 | RX 6700 XT | RX 6600 XT | RX 5700 XT | RX Vega 56 | RX 590 |  
|-------------------|------------------|------------|------------|------------|---------|------------|------------|------------|------------|--------|
| 3840x2160         | Quality (1.5x)   | 1.1ms      | 1.2ms      | 1.2ms      | 1.3ms   | 1.8ms      | 3.0ms      | 2.4ms      | 4.8ms      | 5.3ms  |
|                   | Balanced (1.7x)  | 1.0ms      | 1.0ms      | 1.1ms      | 1.2ms   | 1.6ms      | 2.7ms      | 2.1ms      | 4.3ms      | 4.8ms  |
|                   | Performance (2x) | 0.8ms      | 0.9ms      | 0.9ms      | 1.1ms   | 1.5ms      | 2.3ms      | 1.9ms      | 3.5ms      | 4.2ms  |
|                   | Ultra perf. (3x) | 0.7ms      | 0.7ms      | 0.7ms      | 1.0ms   | 1.3ms      | 1.7ms      | 1.6ms      | 2.8ms      | 3.5ms  |
| 2560x1440         | Quality (1.5x)   | 0.4ms      | 0.4ms      | 0.5ms      | 0.6ms   | 0.8ms      | 1.2ms      | 1.0ms      | 1.8ms      | 2.3ms  |
|                   | Balanced (1.7x)  | 0.4ms      | 0.4ms      | 0.4ms      | 0.5ms   | 0.7ms      | 1.0ms      | 0.9ms      | 1.7ms      | 2.1ms  |
|                   | Performance (2x) | 0.4ms      | 0.4ms      | 0.4ms      | 0.5ms   | 0.7ms      | 0.9ms      | 0.8ms      | 1.4ms      | 1.9ms  |
|                   | Ultra perf. (3x) | 0.3ms      | 0.3ms      | 0.3ms      | 0.4ms   | 0.6ms      | 0.7ms      | 0.7ms      | 1.2ms      | 1.6ms  |
| 1920x1080         | Quality (1.5x)   | 0.3ms      | 0.3ms      | 0.3ms      | 0.3ms   | 0.4ms      | 0.6ms      | 0.6ms      | 1.0ms      | 1.3ms  |
|                   | Balanced (1.7x)  | 0.2ms      | 0.2ms      | 0.2ms      | 0.3ms   | 0.4ms      | 0.6ms      | 0.5ms      | 0.9ms      | 1.2ms  |
|                   | Performance (2x) | 0.2ms      | 0.2ms      | 0.2ms      | 0.3ms   | 0.4ms      | 0.5ms      | 0.5ms      | 0.8ms      | 1.1ms  |
|                   | Ultra perf. (3x) | 0.2ms      | 0.2ms      | 0.2ms      | 0.2ms   | 0.3ms      | 0.4ms      | 0.4ms      | 0.7ms      | 0.9ms  |

Figures are rounded to the nearest 0.1ms and are without additional [`sharpness`](src/ffx-fsr2-api/ffx_fsr2.h#L129).

## Memory requirements
Using FSR2 requires some additional GPU local memory to be allocated for consumption by the GPU. When using the FSR2 API, this memory is allocated when the FSR2 context is created, and is done so via the series of callbacks which comprise the backend interface. This memory is used to store intermediate surfaces which are computed by the FSR2 algorithm as well as surfaces which are persistent across many frames of the application. The table below includes the amount of memory used by FSR2 under various operating conditions. The "Working set" column indicates the total amount of memory used by FSR2 as the algorithm is executing on the GPU; this is the amount of memory FSR2 will require to run. The "Persistent memory" column indicates how much of the "Working set" column is required to be left intact for subsequent frames of the application; this memory stores the temporal data consumed by FSR2. The "Aliasable memory" column indicates how much of the "Working set" column may be aliased by surfaces or other resources used by the application outside of the operating boundaries of FSR2.

You can take control of resource creation in FSR2 by overriding the resource creation and destruction parts of the FSR2 backend interface. This means that for a perfect integration of FSR2, additional memory which is equal to the "Persistent memory" column of the table below is required depending on your operating conditions.

| Resolution | Quality                | Working set (MB) | Persistent memory (MB) | Aliasable memory (MB)   |  
| -----------|------------------------|------------------|------------------------|-------------------------|
| 3840x2160  | Quality (1.5x)         | 302MB            | 218MB                  | 85MB                    |
|            | Balanced (1.7x)        | 279MB            | 214MB                  | 65MB                    |
|            | Performance (2x)       | 260MB            | 211MB                  | 49MB                    |
|            | Ultra performance (3x) | 228MB            | 206MB                  | 22MB                    |
| 2560x1440  | Quality (1.5x)         | 140MB            | 100MB                  | 40MB                    |
|            | Balanced (1.7x)        | 129MB            | 98MB                   | 33MB                    |
|            | Performance (2x)       | 119MB            | 97MB                   | 24MB                    |
|            | Ultra performance (3x) | 105MB            | 95MB                   | 10MB                    |
| 1920x1080  | Quality (1.5x)         | 78MB             | 56MB                   | 22MB                    |
|            | Balanced (1.7x)        | 73MB             | 55MB                   | 18MB                    |
|            | Performance (2x)       | 69MB             | 54MB                   | 15MB                    |
|            | Ultra performance (3x) | 59MB             | 53MB                   | 6MB                     |

Figures are rounded up to nearest MB and are without additional [`sharpness`](src/ffx-fsr2-api/ffx_fsr2.h#L129). Figures are approximations using an RX 6700XT GPU in DX12 and are subject to change.

For details on how to manage FSR2's memory requirements please refer to the section of this document dealing with [Memory management](#memory-management).

## Input resources
FSR2 is a temporal algorithm, and therefore requires access to data from both the current and previous frame. The following table enumerates all external inputs required by FSR2.

> The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. All resources are from the current rendered frame, for DirectX(R)12 and Vulkan(R) applications all input resources should be transitioned to [`D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE`](https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states) and [`VK_ACCESS_SHADER_READ_BIT`](https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkAccessFlagBits.html) respectively before calling [`ffxFsr2ContextDispatch`](src/ffx-fsr2-api/ffx_fsr2.h#L256).

| Name            | Resolution                   |  Format                            | Type      | Notes                                          |  
| ----------------|------------------------------|------------------------------------|-----------|------------------------------------------------|
| Color buffer    | Render                       | `APPLICATION SPECIFIED`            | Texture   | The render resolution color buffer for the current frame provided by the application. If the contents of the color buffer are in high dynamic range (HDR), then the [`FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE`](src/ffx-fsr2-api/ffx_fsr2.h#L88) flag should be set in  the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure. |
| Depth buffer    | Render                       | `APPLICATION SPECIFIED (1x FLOAT)` | Texture   | The render resolution depth buffer for the current frame provided by the application. The data should be provided as a single floating point value, the precision of which is under the application's control. The configuration of the depth should be communicated to FSR2 via the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166). You should set the [`FFX_FSR2_ENABLE_DEPTH_INVERTED`](src/ffx-fsr2-api/ffx_fsr2.h#L91) flag if your depth buffer is inverted (that is [1..0] range), and you should set the [`FFX_FSR2_ENABLE_DEPTH_INFINITE`](src/ffx-fsr2-api/ffx_fsr2.h#L92) flag if your depth buffer has an infinite far plane. If the application provides the depth buffer in `D32S8` format, then FSR2 will ignore the stencil component of the buffer, and create an `R32_FLOAT` resource to address the depth buffer. On GCN and RDNA hardware, depth buffers are stored separately from stencil buffers. |
| Motion vectors  | Render or presentation       | `APPLICATION SPECIFIED (2x FLOAT)` | Texture   | The 2D motion vectors for the current frame provided by the application in [**(<-width, -height>**..**<width, height>**] range. If your application renders motion vectors with a different range, you may use the [`motionVectorScale`](src/ffx-fsr2-api/ffx_fsr2.h#L126) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure to adjust them to match the expected range for FSR2. Internally, FSR2 uses 16-bit quantities to represent motion vectors in many cases, which means that while motion vectors with greater precision can be provided, FSR2 will not benefit from the increased precision. The resolution of the motion vector buffer should be equal to the render resolution, unless the [`FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS`](src/ffx-fsr2-api/ffx_fsr2.h#L89) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166), in which case it should be equal to the presentation resolution. |
| Reactive mask   | Render                       | `R8_UNORM`                         | Texture   | As some areas of a rendered image do not leave a footprint in the depth buffer or include motion vectors, FSR2 provides support for a reactive mask texture which can be used to indicate to FSR2 where such areas are. Good examples of these are particles, or alpha-blended objects which do not write depth or motion vectors. If this resource is not set, then FSR2's shading change detection logic will handle these cases as best it can, but for optimal results, this resource should be set. For more information on the reactive mask please refer to the [Reactive mask](#reactive-mask) section.  |
| Exposure        | 1x1                          | `R32_FLOAT`                        | Texture   | A 1x1 texture containing the exposure value computed for the current frame. This resource is optional, and may be omitted if the [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L93) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166).  |

## Depth buffer configurations
It is strongly recommended that an inverted, infinite depth buffer is used with FSR2. However, alternative depth buffer configurations are supported. An application should inform the FSR2 API of its depth buffer configuration by setting the appropriate flags during the creation of the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164). The table below contains the appropriate flags.

| FSR2 flag                        | Note                                                                                       |
|----------------------------------|--------------------------------------------------------------------------------------------|
| [`FFX_FSR2_ENABLE_DEPTH_INVERTED`](src/ffx-fsr2-api/ffx_fsr2.h#L91) | A bit indicating that the input depth buffer data provided is inverted [max..0].           |
| [`FFX_FSR2_ENABLE_DEPTH_INFINITE`](src/ffx-fsr2-api/ffx_fsr2.h#L92) | A bit indicating that the input depth buffer data provided is using an infinite far plane. |


## Providing motion vectors

### Space
A key part of a temporal algorithm (be it antialiasing or upscaling) is the provision of motion vectors. FSR2 accepts motion vectors in 2D which encode the motion from a pixel in the current frame to the position of that same pixel in the previous frame. FSR2 expects that motion vectors are provided by the application in [**<-width, -height>**..**<width, height>**] range; this matches screenspace. For example, a motion vector for a pixel in the upper-left corner of the screen with a value of <width, height> would represent a motion that traversed the full width and height of the input surfaces, originating from the bottom-right corner.

![alt text](docs/media/super-resolution-temporal/motion-vectors.svg "A diagram showing a 2D motion vector.")

If your application computes motion vectors in another space - for example normalized device coordinate space - then you may use the [`motionVectorScale`](src/ffx-fsr2-api/ffx_fsr2.h#L126) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure to instruct FSR2 to adjust them to match the expected range for FSR2. The code examples below illustrate how motion vectors may be scaled to screen space. The example HLSL and C++ code below illustrates how NDC-space motion vectors can be scaled using the FSR2 host API.

```HLSL
// GPU: Example of application NDC motion vector computation
float2 motionVector = (previousPosition.xy / previousPosition.w) - (currentPosition.xy / currentPosition.w);

// CPU: Matching FSR 2.0 motionVectorScale configuration
dispatchParameters.motionVectorScale.x = (float)renderWidth;
dispatchParameters.motionVectorScale.y = (float)renderHeight;
```

### Precision & resolution
Internally, FSR2 uses 16bit quantities to represent motion vectors in many cases, which means that while motion vectors with greater precision can be provided, FSR2 will not currently benefit from the increased precision. The resolution of the motion vector buffer should be equal to the render resolution, unless the [`FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS`](src/ffx-fsr2-api/ffx_fsr2.h#L89) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166), in which case it should be equal to the presentation resolution.

### Coverage
FSR2 will perform better quality upscaling when more objects provide their motion vectors. It is therefore advised that all opaque, alpha-tested and alpha-blended objects should write their motion vectors for all covered pixels. If vertex shader effects are applied - such as scrolling UVs - these calculations should also be factored into the calculation of motion for the best results. For alpha-blended objects it is also strongly advised that the alpha value of each covered pixel is stored to the corresponding pixel in the [reactive mask](#reactive-mask). This will allow FSR2 to perform better handling of alpha-blended objects during upscaling. The reactive mask is especially important for alpha-blended objects where writing motion vectors might be prohibitive, such as particles.

## Reactive mask
In the context of FSR2, the term "reactivity" means how much influence the samples rendered for the current frame have over the production of the final upscaled image. Typically, samples rendered for the current frame contribute a relatively modest amount to the result computed by FSR2; however, there are exceptions. To produce the best results for fast moving, alpha-blended objects, FSR2 requires the [Reproject & accumulate](#reproject-accumulate) stage to become more reactive for such pixels. As there is no good way to determine from either color, depth or motion vectors which pixels have been rendered using alpha blending, FSR2 performs best when applications explicitly mark such areas.

Therefore, it is strongly encouraged that applications provide a reactive mask to FSR2. The reactive mask guides FSR2 on where it should reduce its reliance on historical information when compositing the current pixel, and instead allow the current frame's samples to contribute more to the final result. The reactive mask allows the application to provide a value from [0..1] where 0 indicates that the pixel is not at all reactive (and should use the default FSR2 composition strategy), and a value of 1 indicates the pixel should be fully reactive. 

While there are other applications for the reactive mask, the primary application for the reactive mask is producing better results of upscaling images which include alpha-blended objects. A good proxy for reactiveness is actually the alpha value used when compositing an alpha-blended object into the scene, therefore, applications should write `alpha` to the reactive mask. It should be noted that it is unlikely that a reactive value of close to 1 will ever produce good results. Therefore, we recommend clamping the maximum reactive value to around 0.9.

If a [Reactive mask](#reactive-mask) is not provided to FSR2 (by setting the [`reactive`](src/ffx-fsr2-api/ffx_fsr2.h#L122) field of [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) to `NULL`) then an internally generated 1x1 texture with a cleared reactive value will be used.

## Transparency & composition mask
In addition to the [Reactive mask](#reactive-mask), FSR2 provides for the application to denote areas of other specialist rendering which should be accounted for during the upscaling process. Examples of such special rendering include areas of raytraced reflections or animated textures.

While the [Reactive mask](#reactive-mask) adjusts the accumulation balance, the [Transparency & composition mask](#transparency-and-composition-mask) adjusts the pixel locks created by FSR2. A pixel with a value of 0 in the [Transparency & composition mask](#ttransparency-and-composition-mask) does not perform any additional modification to the lock for that pixel. Conversely, a value of 1 denotes that the lock for that pixel should be completely removed.

If a [Transparency & composition mask](#transparency-and-composition-mask) is not provided to FSR2 (by setting the [`transparencyAndComposition`](#src/ffx-fsr2-api/ffx_fsr2.h#L123) field of [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) to `NULL`) then an internally generated 1x1 texture with a cleared transparency and composition value will be used.

## Automatically generating reactivity
To help applications generate the [Reactive mask](#reactive-mask) and the [Transparency & composition mask](#transparency-and-composition-mask), FSR2 provides an optional helper API. Under the hood, the API launches a compute shader which computes these values for each pixel using a luminance-based heuristic.

Applications wishing to do this can call the [`ffxFsr2ContextGenerateReactiveMask`](src/ffx-fsr2-api/ffx_fsr2.h#L267) function and should pass two versions of the color buffer, one containing opaque only geometry, and the other containing both opaque and alpha-blended objects.

In version 2.1, this helper changed slightly in order to give developers more options when items such as decals were used, which may have resulted in shimmer on certain surfaces. A "binaryValue" can now be set in the FfxFsr2GenerateReactiveDescription struct, to provide a specific value to be written into the reactive mask instead of 1.0f, which can be too high.

## Exposure
FSR2 provides two values which control the exposure used when performing upscaling. They are as follows:

1. **Pre-exposure** a value by which we divide the input signal to get back to the original signal produced by the game before any packing into lower precision render targets.
2. **Exposure** a value which is multiplied against the result of the pre-exposed color value.

The exposure value should match that which the application uses during any subsequent tonemapping passes performed by the application. This means FSR2 will operate consistently with what is likely to be visible in the final tonemapped image. 

> In various stages of the FSR2 algorithm described in this document, FSR2 will compute its own exposure value for internal use. It is worth noting that all outputs from FSR2 will have this internal tonemapping reversed before the final output is written. Meaning that FSR2 returns results in the same domain as the original input signal.

Poorly selected exposure values can have a drastic impact on the final quality of FSR2's upscaling. Therefore, it is recommended that [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L93) is used by the application, unless there is a particular reason not to. When [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L93) is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure, the exposure calculation shown in the HLSL code below is used to compute the exposure value, this matches the exposure response of ISO 100 film stock.

```HLSL
float ComputeAutoExposureFromAverageLog(float averageLogLuminance)
{
	const float averageLuminance = exp(averageLogLuminance);
	const float S = 100.0f; // ISO arithmetic speed
	const float K = 12.5f;
	const float exposureIso100 = log2((averageLuminance * S) / K);
	const float q = 0.65f;
	const float luminanceMax = (78.0f / (q * S)) * pow(2.0f, exposureIso100);
	return 1 / luminanceMax;
}
```

## Placement in the frame
The primary goal of FSR2 is to improve application rendering performance by using a temporal upscaling algorithm relying on a number of inputs. Therefore, its placement in the pipeline is key to ensuring the right balance between the highest quality visual quality and great performance.

![alt text](docs/media/super-resolution-temporal/pipeline-placement.svg "A diagram showing the placement of FidelityFX Super Resolution (Temporal) in the wider rendering pipeline.")

With any image upscaling approach is it important to understand how to place other image-space algorithms with respect to the upscaling algorithm. Placing these other image-space effects before the upscaling has the advantage that they run at a lower resolution, which of course confers a performance advantage onto the application. However, it may not be appropriate for some classes of image-space techniques. For example, many applications may introduce noise or grain into the final image, perhaps to simulate a physical camera. Doing so before an upscaler might cause the upscaler to amplify the noise, causing undesirable artifacts in the resulting upscaled image. The following table divides common real-time image-space techniques into two columns. 'Post processing A' contains all the techniques which typically would run before FSR2's upscaling, meaning they would all run at render resolution. Conversely, the 'Post processing B' column contains all the techniques which are recommend to run after FSR2, meaning they would run at the larger, presentation resolution.

| Post processing A              | Post processing B    |
|--------------------------------|----------------------|
| Screenspace reflections        | Film grain           |
| Screenspace ambient occlusion  | Chromatic aberration |
| Denoisers (shadow, reflections)| Vignette             |
| Exposure (optional)            | Tonemapping          |
|                                | Bloom                |
|                                | Depth of field       |
|                                | Motion blur          |

Please note that the recommendations here are for guidance purposes only and depend on the precise characteristics of your application's implementation.

## Host API
While it is possible to generate the appropriate intermediate resources, compile the shader code, set the bindings, and submit the dispatches, it is much easier to use the FSR2 host API which is provided.

To use to the API, you should link the FSR2 libraries (more on which ones shortly) and include the `ffx_fsr2.h` header file, which in turn has the following header dependencies:

```
ffx_assert.h
ffx_error.h
ffx_fsr2_interface.h
ffx_types.h
ffx_util.h
```

To use the FSR2 API, you should link `ffx_fsr2_api_x64.lib` which will provide the symbols for the application-facing APIs. However, FSR2's API has a modular backend, which means that different graphics APIs and platforms may be targeted through the use of a matching backend. Therefore, you should further include the backend lib matching your requirements, referencing the table below.

| Target              | Library name            |
|---------------------|-------------------------|
| DirectX(R)12        | `ffx_fsr2_dx12_x64.lib` |
| Vulkan(R)           | `ffx_fsr2_vk_x64.lib`   | 

> Please note the modular architecture of the FSR2 API allows for custom backends to be implemented. See the [Modular backend](#modular-backend) section for more details.

To begin using the API, the application should first create a [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164) structure. This structure should be located somewhere with a lifetime approximately matching that of your backbuffer; somewhere on the application's heap is usually a good choice. By calling [`ffxFsr2ContextCreate`](src/ffx-fsr2-api/ffx_fsr2.h#L213) the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164) structure will be populated with the data it requires. Moreover, a number of calls will be made from [`ffxFsr2ContextCreate`](src/ffx-fsr2-api/ffx_fsr2.h#L213) to the backend which is provided to [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164) as part of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L101) structure. These calls will perform such tasks as creating intermediate resources required by FSR2 and setting up shaders and their associated pipeline state. The FSR2 API does not perform any dynamic memory allocation.

Each frame of your application where upscaling is required, you should call [`ffxFsr2ContextDispatch`](src/ffx-fsr2-api/ffx_fsr2.h#L254). This function accepts the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164) structure that was created earlier in the application's lifetime as well as a description of precisely how upscaling should be performed and on which data. This description is provided by the application filling out a [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L114) structure.

Destroying the context is performed by calling [`ffxFsr2ContextDestroy`](src/ffx-fsr2-api/ffx_fsr2.h#L277). Please note, that the GPU should be idle before attempting to call [`ffxFsr2ContextDestroy`](src/ffx-fsr2-api/ffx_fsr2.h#L277), and the function does not perform implicit synchronization to ensure that resources being accessed by FSR2 are not currently in flight. The reason for this choice is to avoid FSR2 introducing additional GPU flushes for applications who already perform adequate synchronization at the point where they might wish to destroy the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L164), this allows an application to perform the most efficient possible creation and teardown of the FSR2 API when required.

There are additional helper functions which are provided as part of the FSR2 API. These helper functions perform tasks like the computation of sub-pixel jittering offsets, as well as the calculation of rendering resolutions based on dispatch resolutions and the default [scaling modes](#scaling-modes) provided by FSR2.

For more exhaustive documentation of the FSR2 API, you can refer to the API reference documentation provided.

## Modular backend
The design of the FSR2 API means that the core implementation of the FSR2 algorithm is unaware upon which rendering API it sits. Instead, FSR2 calls functions provided to it through an interface, allowing different backends to be used with FSR2. This design also allows for applications integrating FSR2 to provide their own backend implementation, meaning that platforms which FSR2 does not currently support may be targeted by implementing a handful of functions. Moreover, applications which have their own rendering abstractions can also implement their own backend, taking control of all aspects of FSR2's underlying function, including memory management, resource creation, shader compilation, shader resource bindings, and the submission of FSR2 workloads to the graphics device.

![alt text](docs/media/super-resolution-temporal/api-architecture.svg "A diagram showing the high-level architecture of the FSR2 API.")

Out of the box, the FSR2 API will compile into multiple libraries following the separation already outlined between the core API and the backends. This means if you wish to use the backends provided with FSR2 you should link both the core FSR2 API lib as well the backend matching your requirements.

> The public release of FSR2 comes with DirectX(R)12 and Vulkan(R) backends, but other backends are available upon request. Talk with your AMD Developer Technology representative for more information.

## Memory management
If the FSR2 API is used with one of the supplied backends (e.g: DirectX(R)12 or Vulkan(R)) then all the resources required by FSR2 are created as committed resources directly using the graphics device provided by the host application. However, by overriding the create and destroy family of functions present in the backend interface it is possible for an application to more precisely control the memory management of FSR2.

To do this, you can either provide a full custom backend to FSR2 via the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure passed to [`ffxFsr2ContextCreate`](src/ffx-fsr2-api/ffx_fsr2.h#L215) function, or you can retrieve the backend for your desired API and override the resource creation and destruction functions to handle them yourself. To do this, simply overwrite the [`fpCreateResource`](src/ffx-fsr2-api/ffx_fsr2_interface.h#L360) and [`fpDestroyResource`](src/ffx-fsr2-api/ffx_fsr2_interface.h#L364) function pointers.

``` CPP
// Setup DX12 interface.
const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX12();
void* scratchBuffer = malloc(scratchBufferSize);
FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&contextDescription.callbacks, m_pDevice->GetDevice(), scratchBuffer, scratchBufferSize);
FFX_ASSERT(errorCode == FFX_OK);

// Override the resource creation and destruction.
contextDescription.callbacks.createResource = myCreateResource;
contextDescription.callbacks.destroyResource = myDestroyResource;

// Set up the context description.
contextDescription.device = ffxGetDeviceDX12(m_pDevice->GetDevice());
contextDescription.maxRenderSize.width = renderWidth;
contextDescription.maxRenderSize.height = renderHeight;
contextDescription.displaySize.width = displayWidth;
contextDescription.displaySize.height = displayHeight;
contextDescription.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE
                         | FFX_FSR2_ENABLE_DEPTH_INVERTED
                         | FFX_FSR2_ENABLE_AUTO_EXPOSURE;

// Create the FSR2 context.
errorCode = ffxFsr2ContextCreate(&context, &contextDescription);
FFX_ASSERT(errorCode == FFX_OK);
```

One interesting advantage to an application taking control of the memory management required for FSR2 is that resource aliasing maybe performed, which can yield a memory saving. The table present in [Memory requirements](#memory-requirements) demonstrates the savings available through using this technique. In order to realise the savings shown in this table, an appropriate area of memory - the contents of which are not required to survive across a call to the FSR2 dispatches - should be found to share with the aliasable resources required for FSR2. Each [`FfxFsr2CreateResourceFunc`](src/ffx-fsr2-api/ffx_fsr2_interface.h#L399) call made by FSR2's core API through the FSR2 backend interface will contains a set of flags as part of the [`FfxCreateResourceDescription`](src/ffx-fsr2-api/ffx_types.h#L251) structure. If the [`FFX_RESOURCE_FLAGS_ALIASABLE`](src/ffx-fsr2-api/ffx_types.h#L101) is set in the [`flags`](src/ffx-fsr2-api/ffx_types.h#L208) field this indicates that the resource may be safely aliased with other resources in the rendering frame.

## Temporal Antialiasing
Temporal antialiasing (TAA) is a technique which uses the output of previous frames to construct a higher quality output from the current frame. As FSR2 has a similar goal - albeit with the additional goal of also increasing the resolution of the rendered image - there is no longer any need to include a separate TAA pass in your application.

## Camera jitter
FSR2 relies on the application to apply sub-pixel jittering while rendering - this is typically included in the projection matrix of the camera. To make the application of camera jitter simple, the FSR2 API provides a small set of utility function which computes the sub-pixel jitter offset for a particular frame within a sequence of separate jitter offsets.

``` CPP
int32_t ffxFsr2GetJitterPhaseCount(int32_t renderWidth, int32_t displayWidth);
FfxErrorCode ffxFsr2GetJitterOffset(float* outX, float* outY, int32_t jitterPhase, int32_t sequenceLength);
```

Internally, these function implement a Halton[2,3] sequence [[Halton](#references)]. The goal of the Halton sequence is to provide spatially separated points, which cover the available space.

![alt text](docs/media/super-resolution-temporal/jitter-space.svg "A diagram showing how to map sub-pixel jitter offsets to projection offsets.")

It is important to understand that the values returned from the [`ffxFsr2GetJitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L424) are in unit pixel space, and in order to composite this correctly into a projection matrix we must convert them into projection offsets. The diagram above shows a single pixel in unit pixel space, and in projection space. The code listing below shows how to correctly composite the sub-pixel jitter offset value into a projection matrix.

``` CPP
const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(renderWidth, displayWidth);

float jitterX = 0;
float jitterY = 0;
ffxFsr2GetJitterOffset(&jitterX, &jitterY, index, jitterPhaseCount);

// Calculate the jittered projection matrix.
const float jitterX = 2.0f * jitterX / (float)renderWidth;
const float jitterY = -2.0f * jitterY / (float)renderHeight;
const Matrix4 jitterTranslationMatrix = translateMatrix(Matrix3::identity, Vector3(jitterX, jitterY, 0));
const Matrix4 jitteredProjectionMatrix = jitterTranslationMatrix * projectionMatrix;
```

Jitter should be applied to *all* rendering. This includes opaque, alpha transparent, and raytraced objects. For rasterized objects, the sub-pixel jittering values calculated by the [`ffxFsr2GetJitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L422) function can be applied to the camera projection matrix which is ultimately used to perform transformations during vertex shading. For raytraced rendering, the sub-pixel jitter should be applied to the ray's origin - often the camera's position.

Whether you elect to use the recommended [`ffxFsr2GetJitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L424) function or your own sequence generator, you must set the [`jitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L125) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure to inform FSR2 of the jitter offset that has been applied in order to render each frame. Moreover, if not using the recommended [`ffxFsr2GetJitterOffset`](src/ffx-fsr2-api/ffx_fsr2.h#L424) function, care should be taken that your jitter sequence never generates a null vector; that is value of 0 in both the X and Y dimensions.

The table below shows the jitter sequence length for each of the default quality modes.

 | Quality mode      | Scaling factor          | Sequence length |
 |-------------------|-------------------------|-----------------|
 | Quality           | 1.5x (per dimension)    | 18              |
 | Balanced          | 1.7x (per dimension)    | 23              |
 | Performance       | 2.0x (per dimension)    | 32              |
 | Ultra performance | 3.0x (per dimension)    | 72              |
 | Custom            | [1..n]x (per dimension) | `ceil(8 * n^2)` |

## Camera jump cuts
Most applications with real-time rendering have a large degree of temporal consistency between any two consecutive frames. However, there are cases where a change to a camera's transformation might cause an abrupt change in what is rendered. In such cases, FSR2 is unlikely to be able to reuse any data it has accumulated from previous frames, and should clear this data such to exclude it from consideration in the compositing process. In order to indicate to FSR2 that a jump cut has occurred with the camera you should set the [`reset`](src/ffx-fsr2-api/ffx_fsr2.h#L132) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure to `true` for the first frame of the discontinuous camera transformation.

Rendering performance may be slightly less than typical frame-to-frame operation when using the reset flag, as FSR2 will clear some additional internal resources.

## Mipmap biasing
Applying a negative mipmap biasing will typically generate an upscaled image with better texture detail. We recommend applying the following formula to your Mipmap bias:  

``` CPP
mipBias = log2(renderResolution/displayResolution) - 1.0;
```

It is suggested that applications adjust the MIP bias for specific high-frequency texture content which is susceptible to showing temporal aliasing issues.

The following table illustrates the mipmap biasing factor which results from evaluating the above pseudocode for the scaling ratios matching the suggested quality modes that applications should expose to end users.
 
 | Quality mode      | Scaling factor        | Mipmap bias |
 |-------------------|-----------------------|-------------|
 | Quality           | 1.5X (per dimension)  | -1.58       |
 | Balanced          | 1.7X (per dimension)  | -1.76       |
 | Performance       | 2.0X (per dimension)  | -2.0        |
 | Ultra performance | 3.0X (per dimension)  | -2.58       |

## Frame Time Delta Input
The FSR2 API requires [`frameTimeDelta`](src/ffx-fsr2-api/ffx_fsr2.h#L130) be provided by the application through the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure. This value is in __milliseconds__: if running at 60fps, the value passed should be around __16.6f__.

The value is used within the temporal component of the FSR 2 auto-exposure feature. This allows for tuning of the history accumulation for quality purposes.

## HDR support
High dynamic range images are supported in FSR2. To enable this, you should set the [`FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE`](src/ffx-fsr2-api/ffx_fsr2.h#L88) bit in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure. Images should be provided to FSR2 in linear color space.

> Support for additional color spaces might be provided in a future revision of FSR2.

## Falling back to 32-bit floating point
FSR2 was designed to take advantage of half precision (FP16) hardware acceleration to achieve the highest possible performance. However, to provide the maximum level of compatibility and flexibility for applications, FSR2 also includes the ability to compile the shaders using full precision (FP32) operations.

It is recommended to use the FP16 version of FSR2 on all hardware which supports it. You can query your graphics card's level of support for FP16 by querying the [`D3D12_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT`](https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_shader_min_precision_support) capability in DirectX(R)12 - you should check that the `D3D[11/12]_SHADER_MIN_PRECISION_16_BIT` is set, and if it is not, fallback to the FP32 version of FSR2. For Vulkan, if [`VkPhysicalDeviceFloat16Int8FeaturesKHR::shaderFloat16`](https://khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceShaderFloat16Int8FeaturesKHR.html) is not set, then you should fallback to the FP32 version of FSR2. Similarly, if [`VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess`](https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPhysicalDevice16BitStorageFeatures.html) is not set, you should also fallback to the FP32 version of FSR2.

To enable the FP32 path in the FSR2 shader source code, you should define `FFX_HALF` to be `1`. In order to share the majority of the algorithm's source code between both FP16 and FP32 (ensuring a high level of code sharing to support ongoing maintenance), you will notice that the FSR2 shader source code uses a set of type macros which facilitate easy switching between 16-bit and 32-bit base types in the shader source.

| FidelityFX type | FP32        | FP16            |
|-----------------|-------------|-----------------|
| `FFX_MIN16_F`   | `float`     | `min16float`    |
| `FFX_MIN16_F2`  | `float2`    | `min16float2`   |
| `FFX_MIN16_F3`  | `float3`    | `min16float3`   |
| `FFX_MIN16_F4`  | `float4`    | `min16float4`   |

The table above enumerates the mappings between the abstract FidelityFX SDK types, and the underlaying intrinsic type which will be substituted depending on the configuration of the shader source during compilation.

## 64-wide wavefronts
Modern GPUs execute collections of threads - called wavefronts - together in a SIMT fashion. The precise number of threads which constitute a single wavefront is a hardware-specific quantity. Some hardware, such as AMD's GCN and RDNA-based GPUs support collecting 64 threads together into a single wavefront. Depending on the precise characteristics of an algorithm's execution, it may be more or less advantageous to prefer a specific wavefront width. With the introduction of Shader Model 6.6, Microsoft added the ability to specific the width of a wavefront via HLSL. For hardware, such as RDNA which supports both 32 and 64 wide wavefront widths, this is a very useful tool for optimization purposes, as it provides a clean and portable way to ask the driver software stack to execute a wavefront with a specific width.

For DirectX(R)12 based applications which are running on RDNA and RDNA2-based GPUs and using the Microsoft Agility SDK, the FSR2 host API will select a 64-wide wavefront width.

# The technique

## Algorithm structure
The FSR2 algorithm is implemented in a series of stages, which are as follows:

1. Compute luminance pyramid
2. Adjust input color
3. Reconstruct & dilate
4. Depth clip
5. Create locks
6. Reproject & accumulate
7. Robust Contrast Adaptive Sharpening (RCAS)

Each pass stage of the algorithm is laid out in the sections following this one, but the data flow for the complete FSR2 algorithm is shown in the diagram below.

![alt text](docs/media/super-resolution-temporal/algorithm-structure.svg "A diagram showing all passes in the FSR2 algorithm.")

## Compute luminance pyramid

The compute luminance pyramid stage has two responsibilities:

1. To produce a lower resolution version of the input color's luminance. This is used by shading change detection in the accumulation pass.
2. To produce a 1x1 exposure texture which is optionally used by the exposure calculations of the [Adjust input color](#adjust-input-color) stage to apply tonemapping, and the [Reproject & Accumulate](#project-and-accumulate) stage for reversing local tonemapping ahead of producing an output from FSR2.


### Resource inputs
The following table contains all resources consumed by the [Compute luminance pyramid](#compute-luminance-pyramid) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name            | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Color buffer    | Current frame   | Render       | `APPLICATION SPECIFIED` | Texture   | The render resolution color buffer for the current frame provided by the application. If the contents of the color buffer are in high dynamic range (HDR), then the [`FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE`](src/ffx-fsr2-api/ffx_fsr2.h#L87) flag should be set in  the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure. |

### Resource outputs
The following table contains all resources produced or modified by the [Compute luminance pyramid](#compute-luminance-pyramid) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name                        | Temporal layer  | Resolution       |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|------------------|-------------------------|-----------|----------------------------------------------|
| Exposure                    | Current frame   | 1x1              | `R32_FLOAT`             | Texture   | A 1x1 texture containing the exposure value computed for the current frame. This resource is optional, and may be omitted if the [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L92) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166).  |
| Current luminance           | Current frame   | `Render * 0.5`   | `R16_FLOAT`             | Texture   | A texture at 50% of render resolution texture which contains the luminance of the current frame. |

### Description
The [Compute luminance pyramid](#compute-luminance-pyramid) stage is implemented using FidelityFX [Single Pass Downsampler](single-pass-downsampler.md), an optimized technique for producing mipmap chains using a single compute shader dispatch. Instead of the conventional (full) pyramidal approach, SPD provides a mechanism to produce a specific set of mipmap levels for an arbitrary input texture, as well as performing arbitrary calculations on that data as we store it to the target location in memory. In FSR2, we are interested in producing in upto two intermediate resources depending on the configuration of the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166). The first resource is a low-resolution representation of the current luminance, this is used later in FSR2 to attempt to detect shading changes. The second is the exposure value, and while it is always computed, it is only used by subsequent stages if the [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L93) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure upon context creation. The exposure value - either from the application, or the [Compute luminance pyramid](#compute-luminance-pyramid) stage - is used in the [Adjust input color](#adjust-input-color) stage of FSR2, as well as by the [Reproject & Accumulate](#project-and-accumulate) stage.

![alt text](docs/media/super-resolution-temporal/auto-exposure.svg "A diagram showing the mipmap levels written by auto-exposure.")

As used by FSR2, SPD is configured to write only to the 2nd (half resolution) and last (1x1) mipmap level. Moreover, different calculations are applied at each of these levels to calculate the quantities required by subsequent stages of the FSR2 algorithm. This means the rest of the mipmap chain is not required to be backed by GPU local memory (or indeed any type of memory).

The 2nd mipmap level contains current luminance, the value of which is computed during the downsampling of the color buffer using the following HLSL:

``` HLSL
float3 rgb = LoadInputColor(tex);
float3 rgb2y = float3(0.2126, 0.7152, 0.0722);
float logLuma = log(max(FSR2_EPSILON, dot(rgb2y, rgb)));
```

The last mipmap level is computed using the following HLSL:

``` HLSL
float ComputeAutoExposureFromAverageLog(float averageLogLuminance)
{
	const float averageLuminance = exp(averageLogLuminance);
	const float S = 100.0f; // ISO arithmetic speed
	const float K = 12.5f;
	const float exposureIso100 = log2((averageLuminance * S) / K);
	const float q = 0.65f;
	const float luminanceMax = (78.0f / (q * S)) * pow(2.0f, exposureIso100);
	return 1 / luminanceMax;
}
```
	
## Adjust input color

There are several types of adjustments which FSR2 performs on the input colors, these are as follows:

1. The input color is divided by the pre-exposure value.
2. The input color is multiplied by the exposure value.
3. The exposed color is then converted to the YCoCg color space [[**YCoCg**](#references)].

Please note that manipulations to the color values provided by the application are strictly internal to FSR2, meaning that the results produced by FSR2 are always converted by into the requested color space (typically linear).

### Resource inputs
The following table contains all resources consumed by the [Adjust input color](#Adjust-input-color) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name            | Temporal layer  | Resolution   |  Format                   | Type      | Notes                                        |  
| ----------------|-----------------|--------------|---------------------------|-----------|----------------------------------------------|
| Color buffer    | Current frame   | Render       | `APPLICATION SPECIFIED`   | Texture   | The render resolution color buffer for the current frame provided by the application. If the contents of the color buffer are in high dynamic range (HDR), then the [`FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE`](src/ffx-fsr2-api/ffx_fsr2.h#L88) flag should be set in  the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure. |
| Exposure        | Current frame   | 1x1          | ``R32_FLOAT``             | Texture   | A 1x1 texture containing the exposure value computed for the current frame. This resource can be supplied by the application, or computed by the [Compute luminance pyramid](#compute-luminance-pyramid) stage of FSR2 if the [`FFX_FSR2_ENABLE_AUTO_EXPOSURE`](src/ffx-fsr2-api/ffx_fsr2.h#L93) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure.  |

### Resource outputs
The following table contains all resources produced or modified by the [Adjust input color](#Adjust-input-color) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name                        | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Adjusted color buffer       | Current frame   | Render       | `R16G16B16A16_FLOAT`    | Texture   | A texture containing the adjusted version of the application's color buffer. The tonemapping operator may not be the same as any tonemapping operator included in the application, and is instead a local, reversible operator used throughout FSR2. This buffer is stored in YCoCg format. |
| Luminance history           | Many frames     | Render       | `R8G8B8A8_UNORM`        | Texture   | A texture containing three frames of luminance history, as well as a stability factor encoded in the alpha channel. |
| Previous depth buffer       | Current frame   | Render       | `R32_UNORM`            | Texture   | A texture containing a reconstructed and dilated depth values. This surface is cleared by the [Adjust input color](#adjust-input-color) stage. Please note: When viewing this texture in a capture tool (such as [RenderDoc](https://renderdoc.org)) it may not display correctly. This is because the format of this texture is ``R32_UNORM`` and contains IEEE754 floating point values, which have been written after performing a bitcast using the [`asuint`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-asuint) intrinsic function. See the note in [Adjust input color](#adjust-input-color) for more details on the specifics of how this works. |

### Description
The majority of the FSR2 algorithm operates in YCoCg color space. In order to avoid repeatedly calculating conversions from the color space used by the application, FSR2 implements a dedicated stage which applies all adjustments to the color once, with the results then being cached to an adjusted color texture which other passes may then read. As part of the adjustment process, FSR2 also calculates a luminance history buffer. 

As the luminance buffer is persistent (it is not available for aliasing, or cleared each frame), we have access to four frames of history during the [Adjust input color](#Adjust-input-color) stage on any one frame. However, at the end of the [Adjust input color](#Adjust-input-color) stage, the luminance history values are shifted down, meaning that subsequent stages of FSR2 have access to the three most recent frames of luminance (the current frame, and the two frames before it). Therefore, if we denote the current frame as n, then the values stored in the luminance history buffer are as follows.

| Channel | Frame index (Start of adjust input color stage) | Frame index (End of adjust input color stage) |
|---------|-------------------------------------------------|-----------------------------------------------|
| Red     | n-1                                             | n                                             |
| Green   | n-2                                             | n - 1                                         |
| Blue    | n-3                                             | n - 2                                         |

The alpha channel of the luminance history buffer contains a measure of the stability of the luminance over the current frame, and the three frames that came before it. This is computed in the following way:

``` HLSL
float stabilityValue = 1.0f;
for (int i = 0; i < 3; i++) {
    stabilityValue = min(stabilityValue, MinDividedByMax(currentFrameLuma, currentFrameLumaHistory[i]));
}
```

In additional to its color adjustment responsibilities already outlined, this stage also has the responsibility for clearing the reprojected depth buffer to a known value, ready for the [Reconstruct & dilate](#reconstruct-and-dilate) stage on the next frame of the application. The buffer must be cleared, as [Reconstruct & dilate](#reconstruct-and-dilate) will populate it using atomic operations. Depending on the configuration of the depth buffer, an appropriate clearing value is selected.

The format of the previous depth buffer is `R32_UINT` which allows the use of [`InterlockedMax`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/interlockedmax) and [`InterlockedMin`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/interlockedmin) operations to be performed from the [Reconstruct & dilate](#reconstruct-and-dilate) stage of FSR2. This is done with the resulting integer values returned by converting depth values using the [`asint`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-asint) functions. This works because depth values are always greater than 0, meaning that the monotonicity of IEEE754 floating point values when interpreted as integers is guaranteed.

## Reconstruct and dilate
The reconstruct & dilate stage consumes the applications depth buffer and motion vectors, and produces a reconstructed and dilated depth buffer for the previous frame, together with a dilated set of motion vectors in UV space. The stage runs at render resolution.
 
![alt text](docs/media/super-resolution-temporal/vector-dilation.svg "A diagram showing how a motion vector is dilated based on the depth value.")

### Resource inputs
The following table contains all of the resources which are required by the reconstruct & dilate stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                        |  Temporal layer | Resolution |  Format                            | Type      | Notes                                          |  
| ----------------------------|-----------------|------------|------------------------------------|-----------|------------------------------------------------|
| Depth buffer                | Current frame   | Render     | `APPLICATION SPECIFIED (1x FLOAT)` | Texture   | The render resolution depth buffer for the current frame provided by the application. The data should be provided as a single floating point value, the precision of which is under the application's control. The configuration of the depth should be communicated to FSR2 via the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166). You should set the [`FFX_FSR2_ENABLE_DEPTH_INVERTED`](src/ffx-fsr2-api/ffx_fsr2.h#L91) flag if your depth buffer is inverted (that is [1..0] range), and you should set the  flag if your depth buffer has as infinite far plane. If the application provides the depth buffer in `D32S8` format, then FSR2 will ignore the stencil component of the buffer, and create an `R32_FLOAT` resource to address the depth buffer. On GCN and RDNA hardware, depth buffers are stored separately from stencil buffers. |
| Motion vectors              | Current fraame  | Render or presentation       | `APPLICATION SPECIFIED (2x FLOAT)` | Texture   | The 2D motion vectors for the current frame provided by the application in [*(<-width, -height>*..*<width, height>*] range. If your application renders motion vectors with a different range, you may use the [`motionVectorScale`](src/ffx-fsr2-api/ffx_fsr2.h#L126) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure to adjust them to match the expected range for FSR2. Internally, FSR2 uses 16bit quantities to represent motion vectors in many cases, which means that while motion vectors with greater precision can be provided, FSR2 will not benefit from the increased precision. The resolution of the motion vector buffer should be equal to the render resolution, unless the [`FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS`](src/ffx-fsr2-api/ffx_fsr2.h#L89) flag is set in the [`flags`](src/ffx-fsr2-api/ffx_fsr2.h#L104) field of the [`FfxFsr2ContextDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L102) structure when creating the [`FfxFsr2Context`](src/ffx-fsr2-api/ffx_fsr2.h#L166), in which case it should be equal to the presentation resolution. |

### Resource outputs
The following table contains all of the resources which are produced by the reconstruct & dilate stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                                | Temporal layer  | Resolution |  Format                | Type      | Notes                                  |  
| ------------------------------------|-----------------|------------|------------------------|-----------|------------------------------------------------|
| Previous depth buffer               | Current frame   | Render     | `R32_UNORM`            | Texture   | A texture containing the reconstructed previous frame depth values. This surface should first be cleared, see the [Adjust input color](#adjust-input-color) stage for details. Please note: When viewing this texture in a capture tool (such as [RenderDoc](https://renderdoc.org)) it may not display correctly. This is because the format of this texture is ``R32_UNORM`` and contains IEEE754 floating point values, which have been written after performing a bitcast using the ``asuint`` intrinsic function. See the note in [Reproject & accumulate](#reproject-accumulate) for more details on the specifics of how this works. |
| Dilated depth                       | Current frame   | Render     | `R16_UINT`             | Texture    | A texture containing dilated depth values computed from the application's depth buffer. |
| Dilated motion vectors              | Current frame   | Render     | `R16G16_FLOAT`         | Texture    | A texture containing dilated 2D motion vectors computed from the application's 2D motion vector buffer. The red and green channel contains the two-dimensional motion vectors in NDC space. |

### Description
The first step of the [Reconstruct & dilate](#reconstruct-and-dilate) stage is to compute the dilated depth values and motion vectors from the application's depth values and motion vectors for the current frame. Dilated depth values and motion vectors emphasise the edges of geometry which has been rendered into the depth buffer. This is because the edges of geometry will often introduce discontinuities into a contiguous series of depth values, meaning that as depth values and motion vectors are dilated, they will naturally follow the contours of the geometric edges present in the depth buffer. In order to compute the dilated depth values and motion vectors, FSR2 looks at the depth values for a 3x3 neighbourhood for each pixel and then selects the depth values and motion vectors in that neighbourhood where the depth value is nearest to the camera. In the diagram below, you can see how the central pixel of the 3x3 kernel is updated with the depth value and motion vectors from the pixel with the largest depth value - the pixel on the central, right hand side.

As this stage is the first time that motion vectors are consumed by FSR2, this is where motion vector scaling is applied if using the FSR2 host API. Motion vector scaling factors provided via the [`motionVectorScale`](src/ffx-fsr2-api/ffx_fsr2.h#L126) field of the [`FfxFsr2DispatchDescription`](src/ffx-fsr2-api/ffx_fsr2.h#L115) structure and allows you to transform non-screenspace motion vectors into screenspace motion vectors which FSR2 expects.

``` CPP
// An example of how to manipulate motion vector scaling factors using the FSR2 host API. 
FfxFsr2DispatchParameters dispatchParams = { 0 };
dispatchParams.motionVectorScale.x = renderWidth;
dispatchParams.motionVectorScale.y = renderHeight;
```

With the dilated motion vectors, we can now move to the second part of the [Reconstruct & dilate](#reconstruct-and-dilate) stage, which is to estimate the position of each pixel in the current frame's depth buffer in the previous frame. This is done by applying the dilated motion vector computed for a pixel, to its depth buffer value. As it is possible for many pixels to reproject into the same pixel in the previous depth buffer, atomic operations are used in order to resolve the value of the nearest depth value for each pixel. This is done using the [`InterlockedMax`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/interlockedmax) or [`InterlockedMin`](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/interlockedmin) operation (the choice depending on if the application's depth buffer is inverted or not). The use of cumulative operations to resolve the contents of the previous depth buffer implies that the reconstructed depth buffer resource must always be cleared to a known value, which is performed in the [Reproject & accumulate](#reproject-accumulate) stage. This is performed on frame N for frame N + 1.

![alt text](docs/media/super-resolution-temporal/reconstruct-previous-depth.svg "A diagram showing a dilated motion vector being applied to a depth value.")

When using the FSR2 API, the application's depth buffer and the application's velocity buffer must be specified as separate resources as per the [Resource inputs](#resource-inputs) table above. However, if you are undertaking a bespoke integration into your application, this constraint may be relaxed. Take care that the performance characteristics of this pass do not change if moving to a format for the motion vector texture which is more sparse, e.g.: as part of a packed g-buffer in a deferred renderer.

## Depth clip 
The goal of the [Depth clip](#depth-clip) stage is to produce a mask which indicates disoccluded areas of the current frame. 

This stage runs at render resolution.

### Resource inputs
The following table contains all the resources which are consumed by the [Depth clip](#depth-clip) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                                | Temporal layer  | Resolution |  Format                | Type      | Notes                                  |  
| ------------------------------------|-----------------|------------|------------------------|-----------|------------------------------------------------|
| Previous depth buffer               | Current frame   | Render     | `R32_UNORM`            | Texture   | A texture containing the reconstructed previous frame depth values. This surface should first be cleared, see the [Reproject & accumulate](#reproject-accumulate) stage for details. Please note: When viewing this texture in a capture tool (such as [RenderDoc](https://renderdoc.org)) it may not display correctly. This is because the format of this texture is ``R32_UINT`` and contains IEEE754 floating point values, which have been written after performing a bitcast using the ``asuint`` intrinsic function. See the note in [Reproject & accumulate](#reproject-accumulate) for more details on the specifics of how this works. |
| Dilated depth                       | Current frame   | Render     | `R16_UINT`             | Texture    | A texture containing dilated depth values computed from the application's depth buffer. |
| Dilated motion vectors              | Current frame   | Render     | `R16G16_FLOAT`         | Texture    | A texture containing dilated 2D motion vectors computed from the application's 2D motion vector buffer. The red and green channel contains the two-dimensional motion vectors in NDC space, and the alpha channel contains the depth value used by the [Depth clip](#depth-clip) stage. |

### Resource outputs
The following table contains all the resources which are produced by the [Depth clip](#depth-clip) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                                | Temporal layer  | Resolution |  Format                | Type      | Notes                                  |  
| ------------------------------------|-----------------|------------|------------------------|-----------|----------------------------------------|
| Disocclusion mask                   | Current frame   | Render     | `R8_FLOAT`             | Texture   | A texture containing a value indicating how much the pixel has been disoccluded. A value of 0 means that the pixel was entirely occluded in the previous frame, and values greater than zero mean that the pixel was visible to an extent proportional to the value. Therefore when examining the mask in a graphics debugging tool, the darker areas in the disocclusion mask indicate areas which are more disoccluded. |

### Description
To generate the disocclusion mask, the depth value must be computed for each pixel from the previous camera's position and the new camera's position.  In the diagram below, you can see a camera moving from an initial position (labelled P0) to a new position (labelled P1). As it does so, the shaded area behind the sphere becomes disoccluded - that is it becomes visible from the camera at P1 and was previously occluded from the point of view of P0.

![alt text](docs/media/super-resolution-temporal/disocclusion.svg "A diagram showing a disoccluded area as a camera moves from position 0 to position 1.")

With both values depth values, we can compare the delta between them against the Akeley separation value [[Akeley-06](#references)]. Intuitively, the Akeley separation constant provides a minimum distance between two objects represented in a floating point depth buffer which allow you to say - with a high degree of certainty - that the objects were originally distinct from one another. In the diagram below you can see that the mid-grey and dark-grey objects have a delta which is larger than the kSep value which has been computed for the application's depth buffer configuration. However, the distance from the light-gray object to the mid-grey object does not exceed the computed kSep value, and therefore we are unable to conclude if this object is distinct.

![alt text](docs/media/super-resolution-temporal/k-sep.svg "A diagram showing the concept behind the constant of separation.")

The value stored in the disocclusion mask is in the range [0..1], where 1 maps to a value greater than or equal to the Akeley separation value.

## Create locks
This stage is responsible for creating new locks on pixels which are consumed in the [Reproject & Accumulate](#reproject-accumulate) stage. This stage runs at render resolution.

### Resource inputs
The following table contains all resources consumed by the [Create locks](#create-locks) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name                        | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Adjusted color buffer       | Current frame   | Render       | `R16G16B16A16_FLOAT`    | Texture   | A texture containing the adjusted version of the application's color buffer. The tonemapping operator may not be the same as any tonemapping operator included in the application, and is instead a local, reversible operator used throughout FSR2. This buffer is stored in YCoCg format. |
| Lock status                 | Current frame   | Presentation | `R16G16_FLOAT`          | Texture   | A mask which indicates whether or not to perform color rectification on a pixel, can be thought of as a lock on the pixel to stop rectification from removing the detail. Please note: This texture is part of an array of two textures along with the Lock status texture which is used as an input to this stage. The selection of which texture in the array is used for input and output is swapped each frame. The red channel contains the time remaining on the pixel lock, and the Y channel contains the luminance of the pixel at the time when the lock was created. |

### Resource outputs
The following table contains all resources produced or modified by the [Create locks](#create-locks) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user.

| Name                        | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Lock status                 | Current frame   | Presentation | `R16G16_FLOAT`          | Texture   | A mask which indicates whether or not to perform color rectification on a pixel, can be thought of as a lock on the pixel to stop rectification from removing the detail. Please note: This texture is part of an array of two textures along with the Lock status texture which is used as an input to this stage. The selection of which texture in the array is used for input and output is swapped each frame. The red channel contains the time remaining on the pixel lock, and the Y channel contains the luminance of the pixel at the time when the lock was created. The [Create locks](#create-locks) stage updates only a subset of this resource. |

### Description
Intuitively, a pixel lock is a mechanism to stop color rectification from being applied to a pixel. The net effect of this locking is that more of the previous frame's color data is used when computing the final, super resolution pixel color in the [Reproject & accumulate](#reproject-accumulate) stage. The lock status texture contains two values which together compose a pixel lock. The red channel of the lock status texture contains the remaining lifetime of a pixel lock. This value is decremented by the initial lock length divided by the total length of the jitter sequence. When a lock reaches zero, it is considered to be expired. The green channel of the lock status texture contains the luminance of the pixel at the time the lock was created, but it is only populated during the reprojection stage of [Reproject & accumulate](#reproject-accumulate) stage. The luminance value is ultimately used in the [Reproject & Accumulate](#reproject-accumulate) stage as part of the shading change detection, this allows FSR2 to unlock a pixel if there is discontinuous change to the pixel's appearance (e.g.: an abrupt change to the shading of the pixel).

When creating locks, the 3x3 neighbourhood of luminance values is compared against a threshold. The result of this comparison determines if a new lock should be created. The use of the neighbourhood allows us to detect thin features in the input image which should be locked in order to preserve details in the final super resolution image; such as wires, or chain linked fences.

## Reproject & accumulate
This stage undertakes the following steps:

1. The current frame's color buffer is upsampled using Lanczos filtering.
2. The previous frame's output color and lock status buffers are reprojected, as if they were viewed from the current camera's perspective. 
3. Various cleanup steps to the historical color data.
4. The historical color data, and the upscaled color data from the current frame are accumulated.
5. The output is (optionally) tonemapped ready for RCAS sharpening.

This stage runs at presentation resolution.

### Resource inputs
The following table contain all resources required by the [Reproject & accumulate](#reproject-accumulate) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                                | Temporal layer  | Resolution   |  Format                | Type      | Notes                                  |  
| ------------------------------------|-----------------|--------------|------------------------|-----------|----------------------------------------|
| Disocclusion mask                   | Current frame   | Render       | `R8_UNORM`             | Texture   | A texture containing a value indicating how much the pixel has been disoccluded. |
| Dilated motion vectors              | Current frame   | Render       | `R16G16_FLOAT`         | Texture   | A texture containing dilated motion vectors computed from the application's velocity buffer. The red and green channel contains the two-dimensional motion vectors in UV space. |
| Reactive mask                       | Current frame   | Render       | `R8_UNORM`             | Texture   | As some areas of a rendered image do not leave a footprint in the depth buffer or include motion vectors, FSR2 provides support for a reactive mask texture which can be used to indicate to FSR2 where such areas are. Good examples of these are particles, or alpha-blended objects which do not write depth or motion vectors. If this resource is not set, then FSR2's shading change detection logic will handle these cases as best it can, but for optimal results, this resource should be set. For more information on the reactive mask please refer to the [Reactive mask](#reactive-mask) section.  |
| Output buffer               | Previous frame  | Presentation | ``R16G16B16A16_FLOAT``  | Texture   | The output buffer produced by the FSR2 algorithm running in the previous frame. Please note: This buffer is used internally by FSR2, and is distinct from the presentation buffer which is derived from the output buffer, and has [RCAS](#robust-contrast-adpative-sharpening-rcas) applied. Please note: This texture is part of an array of two textures along with the Output buffer texture which is produced by the [Reproject & accumulate](#reproject-accumulate) stage. The selection of which texture in the array is used for input and output is swapped each frame. |
| Current luminance                     | Current frame   | `Render * 0.5`   | `R16_FLOAT`             | Texture   | A texture at 50% of render resolution texture which contains the luminance of the current frame. |
| Luminance history                     | Many frames     | Render       | `R8G8B8A8_UNORM`        | Texture   | A texture containing three frames of luminance history, as well as a stability factor encoded in the alpha channel. |
| Adjusted color buffer                 | Current frame   | Render       | `R16G16B16A16_FLOAT`    | Texture   | A texture containing the adjusted version of the application's color buffer. The tonemapping operator may not be the same as any tonemapping operator included in the application, and is instead a local, reversible operator used throughout FSR2. This buffer is stored in YCoCg format. |
| Lock status                         | Previous frame  | Presentation | `R16G16_FLOAT`         | Texture   | A mask which indicates not to perform color clipping on a pixel, can be thought of as a lock on the pixel to stop clipping removing the detail.  For a more detailed description of the pixel locking mechanism please refer to the [Create locks](#create-locks) stage. Please note: This texture is part of an array of two textures along with the Lock status texture which is used as an output from this stage. The selection of which texture in the array is used for input and output is swapped each frame. |



### Resource outputs
This table contains the resources produced by the [Reproject & accumulate](#reproject-accumulate) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                        | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Output buffer               | Current frame   | Presentation | `R16G16B16A16_FLOAT`    | Texture   | The output buffer produced by the [Reproject & accumulate](#reproject-accumulate) stage for the current frame. Please note: This buffer is used internally by FSR2, and is distinct from the presentation buffer which is produced as an output from this stage after applying RCAS. Please note: This texture is part of an array of two textures along with the Output buffer texture which is consumed by the [Reproject & accumulate](#reproject-accumulate) stage. The selection of which texture in the array is used for input and output is swapped each frame. |
| Reprojected locks           | Current frame   | Render       | `R16G16_FLOAT`          | Texture   | The reprojected lock status texture. |

### Description
The reproject & accumulate stage of FSR2 is the most complicated and expensive stage in the algorithm. It brings together the results from many of the previous algorithmic steps and accumulates the reprojected color data from the previous frame together with the upsampled color data from the current frame. Please note the description in this documentation is designed to give you an intuition for the steps involved in this stage and does not necessarily match the implementation precisely.

![alt text](docs/media/super-resolution-temporal/reproject-and-accumulate-structure.svg "A diagram showing all phases in the rerpoject & accumulate portion of the FSR2 algorithm.")

The first step of the [Reproject & accumulate](#reproject-accumulate) stage is to assess each pixel for changes in its shading. If we are in a locked area, the luminance at the time the lock was created is compared to FSR2's shading change threshold. In a non-locked area, both the current frame and historical luminance values are used to make this determination. Shading change determination is a key part of FSR2's [Reproject & accumulate](#reproject-accumulate) stage, and feeds into many of the other parts of this stage.

![alt text](docs/media/super-resolution-temporal/upsample-with-lanczos.svg "A diagram showing upsampling of the current frame's input using Lanczos.")

Next we must upsample the adjusted color. To perform upsampling, the adjusted color's pixel position serves as the center of a 5x5 Lanczos resampling kernel [[Lanczos]](#references). In the diagram above, you can see that the Lanczos functions are centered around the display resolution sample `S`. The point in each pixel - labelled `P` - denotes the render resolution jittered sample position for which we calculate the Lanczos weights. Looking above and to the right of the 5x5 pixel neighbourhood, you can see the `Lanczos(x, 2)` resampling kernel being applied to the render resolution samples in the 5x5 grid of pixels surrounding the pixel position. It is worth noting that while conceptually the neighbourhood is 5x5, in the implementation only a 4x4 is actually sampled, due to the zero weighted contributions of those pixels on the periphery of the neighbourhood. The implementation of the Lanczos kernel may vary by GPU product. On RDNA2-based products, we use a look-up-table (LUT) to encode the `sinc(x)` function. This helps to produce a more harmonious balance between ALU and memory in the [Reproject & accumulate](#reproject-accumulate) stage. As the upsample step has access to the 5x5 neighbourhood of pixels, it makes sense from an efficiency point of view to also calculate the YCoCg bounding box - which is used during color rectification - at this point. The diagram below shows a 2D YCo bounding box being constructed from a 3x3 neighbourhood around the current pixel, in reality the bounding box also has a third dimension for Cg.

![alt text](docs/media/super-resolution-temporal/calculate-bounding-box.svg "A diagram showing how a YCoCg bounding box is computed from the current frame's adjust color samples.")

Reprojection is another key part of the [Reproject & accumulate](#reproject-accumulate) stage. To perform reprojection, the dilated motion vectors produced by the [Reconstruct & dilate](#reconstruct-and-dilate) stage are sampled and then applied to the output buffer from the previous frame's execution of FSR2. The left of the diagram below shows two-dimensional motion vector **M** being applied to the current pixel position. On the right, you can see the `Lanczos(x, 2)` resampling kernel being applied to the 5x5 grid of pixels surrounding the translated pixel position. As with the upsampling step, the implementation of the Lanczos kernel may vary by GPU product. The result of the reprojection is a presentation resolution image which contains all the data from the previous frame that could be mapped into the current frame. However, it is not just the previous frame's output color that is reprojected. As FSR2 relies on a mechanism whereby each pixel may be locked to enhance its temporal stability, the locks must also be reprojected from the previous frame into the current frame. This is done in much the same way as the reprojection of the color data, but also combines the results of the shading change detection step we performed on the various luminance values, both current and historical. 

![alt text](docs/media/super-resolution-temporal/reproject-mvs.svg "A diagram showing the 5x5 Lanczos sampling kernel applied to a pixel position determined by translating the current pixel position by the motion vectors.")

It is now time to update our locks. The first task for update locks is to look for locks which were created during this frame's [Create locks](#create-locks) stage that are not reprojected, and instead have the luminance value of the current frame written to the green channel of the reprojected locks texture. All that remains then is to discern which locks are trustworthy for the current frame and pass those on to the color rectification step. The truthworthiness determination is done by comparing the luminance values within a neighbourhood of pixels in the current luminance texture. If the luminance separation between these values is large, then we should not trust the lock.

With our lock updates applied and their trustworthiness determined, we can move on to color rectification which is the next crucial step of FSR2's [Reproject & accumulate](#reproject-accumulate) stage. During this stage, a final color is determined from the pixel's historical data which will then be blended with the current frame's upsampled color in order to form the final accumulated super-resolution color. The determination of the final historical color and its contribution is chiefly controlled by two things:

1. Reducing the influence of the historical samples for areas which are disoccluded. This is undertaken by modulating the color value by the disocclusion mask.
2. Reducing the influence of the historical samples (marked S<sub>h</sub> in the diagram below) are far from the current frame color's bounding box (computed during the upsampling phase of the [Reproject & accumulate](#reproject-accumulate) stage).

![alt text](docs/media/super-resolution-temporal/clamp-to-box.svg "A diagram showing a historical color sample being clamped to the YCoCg bounding box for the current frame.")

The final step of the [Reproject & accumulate](#reproject-accumulate) stage is to accumulate the current frame's upsampled color with the rectified historical color data. By default, FSR2 will typically blend the current frame with a relatively low linear interpolation factor - that is relatively little of the current frame will be included in the final output. However, this can be altered based on the contents of the application provided reactivity mask. See the [reactive mask](#reactive-mask) section for further details.


## Robust Contrast Adaptive Sharpening (RCAS)
Robust Contrast Adaptive Sharpening (RCAS) was originally introduced in FidelityFX Super Resolution 1.0 as an additional sharpening pass to help generate additional clarity and sharpeness in the final upscaled image. RCAS is a derivative of the popular Contrast Adaptive Sharpening (CAS) algorithm, but with some key differences which make it more suitable for upscaling. Whereas CAS uses a simplified mechanism to convert local contrast into a variable amount of sharpness, conversely RCAS uses a more exact mechanism, solving for the maximum local sharpness possible before clipping. Additionally, RCAS also has a built-in process to limit the sharpening of what it detects as possible noise. Support for some scaling (which was included in CAS) is not included in RCAS, therefore it should run at presentation resolution.

### Resource inputs
This table contains the resources consumed by the [Robust Contrast Adaptive Sharpening (RCAS)](#robust-contrast-adaptive-sharpening-rcas) stage.

> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                        | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                        |  
| ----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Output buffer               | Previous frame  | Presentation | `R16G16B16A16_FLOAT`    | Texture   | The output buffer produced by the [Reproject & Accumulate](#reproject-accumulate) stage for the current frame. Please note: This buffer is used internally by FSR2, and is distinct from the presentation buffer which is produced as an output from this stage after applying RCAS. Please note: This texture is part of an array of two textures along with the Output buffer texture which is consumed by the [Reproject & Accumulate](#reproject-accumulate) stage. The selection of which texture in the array is used for input and output is swapped each frame. |

### Resource outputs
> The temporal layer indicates which frame the data should be sourced from. 'Current frame' means that the data should be sourced from resources created for the frame that is to be presented next. 'Previous frame' indicates that the data should be sourced from resources which were created for the frame that has just presented. The resolution column indicates if the data should be at 'rendered' resolution or 'presentation' resolution. 'Rendered' resolution indicates that the resource should match the resolution at which the application is performing its rendering. Conversely, 'presentation' indicates that the resolution of the target should match that which is to be presented to the user. 

| Name                         | Temporal layer  | Resolution   |  Format                 | Type      | Notes                                       |  
| -----------------------------|-----------------|--------------|-------------------------|-----------|----------------------------------------------|
| Presentation buffer          | Previous frame  | Presentation | Application specific    | Texture   | The presentation buffer produced by the completed FSR2 algorithm for the current frame. |


### Description
RCAS operates on data sampled using a 5-tap filter configured in a cross pattern. See the diagram below.

![alt text](docs/media/super-resolution-temporal/rcas-weights.svg "A diagram showing the weights RCAS applies to neighbourhood pixels.")

With the samples retreived, RCAS then chooses the 'w' which results in no clipping, limits 'w', and multiplies by the 'sharp' amount. The solution above has issues with MSAA input as the steps along the gradient cause edge detection issues. To help stabilize the results of RCAS, it uses 4x the maximum and 4x the minimum (depending on equation) in place of the individual taps, as well as switching from 'm' to either the minimum or maximum (depending on side), to help in energy conservation.

# Building the sample

## Prerequisites

To build the FSR2 sample, please follow the following instructions:

1) Install the following tools:

- [CMake 3.16](https://cmake.org/download/)
- Install the "Desktop Development with C++" workload
- [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/)
- [Windows 10 SDK 10.0.18362.0](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk)
- [Git 2.32.0](https://git-scm.com/downloads)

2) Generate the solutions:
    ```
    > cd <installation path>\build
    > GenerateSolutions.bat
    ```

3) Open the solutions in the DX12 or Vulkan directory (depending on your preference), compile and run.

4) There is an off-screen version which currently runs on Vulkan,to use this,you need to provide:
- Color textures (RGBA)
- Motion vectors and depth textures(RG for motion vectors and B for depth,and please remain the A channel)

# Limitations

FSR 2 requires a GPU with typed UAV load support.

# Version history

| Version        | Date              | Notes                                                        | 
| ---------------|-------------------|--------------------------------------------------------------|
| **2.1.0**      | 2022-09-06        | Release of FidelityFX Super Resolution 2.1.                  |
| **2.0.1**      | 2022-06-22        | Initial release of FidelityFX Super Resolution 2.0.          |


# References
[**Akeley-06**] Kurt Akeley and Jonathan Su, **"Minimum Triangle Separation for Correct Z-Buffer Occlusion"**, 
[http://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/akeley06_triseparation.pdf](https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/akeley06_triseparation.pdf)

[**Lanczos**] Lanczos resampling, **"Lanczos resampling"**, [https://en.wikipedia.org/wiki/Lanczos_resampling](https://en.wikipedia.org/wiki/Lanczos_resampling)

[**Halton**] Halton sequence, **"Halton sequence"**, [https://en.wikipedia.org/wiki/Halton_sequence](https://en.wikipedia.org/wiki/Halton_sequence)

[**YCoCg**] YCoCg Color Space, [https://en.wikipedia.org/wiki/YCoCg](https://en.wikipedia.org/wiki/YCoCg)
