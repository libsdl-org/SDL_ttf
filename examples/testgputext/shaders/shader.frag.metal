#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct main0_out
{
    float4 m_9 [[color(0)]];
};

struct main0_in
{
    float4 m_11 [[user(locn0)]];
    float2 m_20 [[user(locn1)]];
};

fragment main0_out main0(main0_in in [[stage_in]], texture2d<float> _16 [[texture(0)]], sampler _16Smplr [[sampler(0)]])
{
    main0_out out = {};
    out.m_9 = in.m_11 * _16.sample(_16Smplr, in.m_20);
    return out;
}

