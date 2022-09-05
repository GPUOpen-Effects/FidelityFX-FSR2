//
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
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


// Particle structures
// ===================

struct GPUParticlePartA
{
    uint2   m_PackedTintAndAlpha;                           // The color and opacity
    uint2   m_PackedVelocityXYEmitterNDotLAndRotation;      // Normalized view space velocity XY used for streak extrusion. The lighting term for the while emitter in Z. The rotation angle in W.

    uint    m_StreakLengthAndEmitterProperties;             //  0-15: fp16 streak length
                                                            // 16-23: The index of the emitter
                                                            // 24-29: Atlas index
                                                            // 30:    Whether or not the emitter supports velocity-based streaks
                                                            // 31:    Whether or not the particle is sleeping (ie, don't update position)
    float   m_Rotation;                                     // Uncompressed rotation - some issues with using fp16 rotation (also, saves unpacking it)
    uint    m_CollisionCount;                               // Keep track of how many times the particle has collided
    uint    m_pad;
};

struct GPUParticlePartB
{
    float3  m_Position;             // World space position
    float   m_Mass;                 // Mass of particle

    float3  m_Velocity;             // World space velocity
    float   m_Lifespan;             // Lifespan of the particle.

    float   m_DistanceToEye;        // The distance from the particle to the eye
    float   m_Age;                  // The current age counting down from lifespan to zero
    float   m_StartSize;            // The size at spawn time
    float   m_EndSize;              // The time at maximum age
};
