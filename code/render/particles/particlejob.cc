//------------------------------------------------------------------------------
//  particlejob.cc
//  (C) 2009 Radon Labs GmbH
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "jobs/jobs.h"
#include "math/float4.h"
#include "math/matrix44.h"
#include "particles/particle.h"

#if __NEBULA_NO_ASSERT__
#define ASSERT_POINT(p)
#define ASSERT_VECTOR(v)
#else
#define ASSERT_POINT(p)     n_assert((p).w() > 0.995f);  n_assert((p).w() < 1.005f)
#define ASSERT_VECTOR(v)    n_assert((v).w() > -0.005f); n_assert((v).w() < 0.005f)
#endif

namespace Particles
{

using namespace Math;

//------------------------------------------------------------------------------
/**

    NOTE: JobStepAndVlist does the same as JobStep, but it also generates the vertex 
          list. It could be in one function, but then we would always have to check a 
          boolean value for each particle. Thats bad for the SPU, since it doesnt
          have any branch prediction, and it would have to throw away commands 
          and fetch new commands from the program in each iteration
*/


/// entry point of the job
void ParticleJobFunc(const Jobs::JobFuncContext& ctx);
/// lookup samples at index "sampleIndex" in sample-table
const float* LookupEnvelopeSamples(const float sampleBuffer[ParticleSystemNumEnvelopeSamples*EmitterAttrs::NumEnvelopeAttrs], IndexT sampleIndex);
/// update bounding box (min, max) with vector v
void UpdateBbox(__Float4Arg v, float4 &min, float4 &max);
/// update the age of one particle
void ParticleUpdateAge(const JobUniformData *uniform, const Particle &in, Particle &out);
/// integrate the particle state with a given time-step
void ParticleStep(const JobUniformData *uniform, const Particle &in, Particle &out, JobSliceOutputData *sliceOutput);
/// update particle system step
void JobStep(const JobUniformData *uniform, unsigned int numParticles, const Particle *particles_input, Particle *particles_output, JobSliceOutputData *sliceOutput);

//------------------------------------------------------------------------------
/**
*/
__forceinline 
const float*
LookupEnvelopeSamples(const float sampleBuffer[ParticleSystemNumEnvelopeSamples*EmitterAttrs::NumEnvelopeAttrs], IndexT sampleIndex)
{
    n_assert(sampleIndex >= 0);
    n_assert(sampleIndex < ParticleSystemNumEnvelopeSamples);
    return sampleBuffer + (sampleIndex * EmitterAttrs::NumEnvelopeAttrs);
}

//------------------------------------------------------------------------------
/**
*/
__forceinline 
void
UpdateBbox(__Float4Arg v, float4 &min, float4 &max)
{
    min = float4::minimize(min, v);
    max = float4::maximize(max, v);
}

//------------------------------------------------------------------------------
/**
*/
__forceinline
void ParticleUpdateAge(const JobUniformData *uniform, const Particle &in, Particle &out)
{
    // update particle's age
    out.oneDivLifeTime = in.oneDivLifeTime;
    out.age = in.age + uniform->stepTime;
    out.relAge = in.relAge + uniform->stepTime * in.oneDivLifeTime;
}

//------------------------------------------------------------------------------
/**
*/
__forceinline
void ParticleStep(const JobUniformData *uniform, const Particle &in, Particle &out, JobSliceOutputData *sliceOutput)
{
    ++sliceOutput->numLivingParticles;

    // copy the *const* members
    out.startPosition = in.startPosition;
    out.uvMinMax = in.uvMinMax;
    out.rotationVariation = in.rotationVariation;
    out.sizeVariation = in.sizeVariation;

    const IndexT sampleIndex = IndexT(out.relAge * (float)(ParticleSystemNumEnvelopeSamples-1));
    n_assert(sampleIndex >= 0);            
    n_assert(sampleIndex < ParticleSystemNumEnvelopeSamples);

    const float* samples = LookupEnvelopeSamples(uniform->sampleBuffer, sampleIndex);
    
    // compute current particle acceleration
    Math::float4 acceleration = uniform->windVector * samples[EmitterAttrs::AirResistance];
    acceleration += uniform->gravity;
    acceleration *= samples[EmitterAttrs::Mass];

    float curStretchTime = 0.0f;

    // fix stretch time (if particle stretch is enabled
    if (uniform->stretchTime > 0.0f)
    {
        curStretchTime = (uniform->stretchTime > out.age) ? out.age : uniform->stretchTime;
    }

    // update position, velocity, rotation
    ASSERT_POINT(in.position);
    ASSERT_VECTOR(in.velocity);
    out.position = in.position + in.velocity * samples[EmitterAttrs::VelocityFactor] * uniform->stepTime;
    ASSERT_POINT(out.position);
	sliceOutput->bbox.extend(Math::bbox(out.position, Math::vector(samples[EmitterAttrs::Size])));
    out.velocity = in.velocity + acceleration * uniform->stepTime;
    ASSERT_VECTOR(out.velocity);
    if (uniform->stretchToStart)
    {
        // NOTE: don't support particle rotation in stretch modes
        out.stretchPosition = in.startPosition;
        out.rotation = in.rotation;
    }
    else if (curStretchTime > 0.0f)
    {
        // NOTE: don't support particle rotation in stretch modes
        // ???
        out.stretchPosition = out.position -
                              (out.velocity - acceleration * curStretchTime * 0.5f) *
                              (uniform->stretchTime * samples[EmitterAttrs::VelocityFactor]);
        out.rotation = in.rotation;
    }                
    else
    {
        out.stretchPosition = out.position;
        out.rotation = in.rotation + in.rotationVariation * samples[EmitterAttrs::RotationVelocity] * uniform->stepTime;
    }
    out.color.loadu(&(samples[EmitterAttrs::Red]));
	out.color.w() = n_clamp(out.color.w(), 0, 1);
    out.size = samples[EmitterAttrs::Size] * in.sizeVariation;
}

//------------------------------------------------------------------------------
/**
*/
void JobStep(const JobUniformData *uniform, unsigned int numParticles,
             const Particle *particles_input, Particle *particles_output, 
             JobSliceOutputData *sliceOutput)
{
    unsigned int i;
	sliceOutput->bbox.begin_extend();
    for (i = 0; i < numParticles; i++)
    {
        const Particle &in = particles_input[i];
        Particle &out = particles_output[i];
        ParticleUpdateAge(uniform, in, out);
        if (out.relAge < 1.0f)
        {
            ParticleStep(uniform, in, out, sliceOutput);
        }
    }
	sliceOutput->bbox.end_extend();
}

//------------------------------------------------------------------------------
/**
*/
void
ParticleJobFunc(const Jobs::JobFuncContext& ctx)
{
    const JobUniformData *uniform = (const JobUniformData *)ctx.uniforms[0];
    n_assert(ctx.uniformSizes[0] == sizeof(JobUniformData));

    const Particle *particles_input = (const Particle *)ctx.inputs[0];
    const unsigned int numParticles = ctx.inputSizes[0] / sizeof(Particle);

    Particle *particles_output = (Particle *)ctx.outputs[0];
    n_assert( (ctx.outputSizes[0] / sizeof(Particle)) == numParticles);

    JobSliceOutputData *sliceOutput = (JobSliceOutputData*)ctx.outputs[1];
    n_assert(ctx.outputSizes[1] == sizeof(JobSliceOutputData));

    sliceOutput->numLivingParticles = 0;
	sliceOutput->bbox = Math::bbox();

    n_assert(2 == ctx.numOutputs);
    JobStep(uniform, numParticles, particles_input, particles_output, sliceOutput);
}

} // namespace Particles
