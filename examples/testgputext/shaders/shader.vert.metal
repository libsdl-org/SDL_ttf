#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _28
{
    float4x4 _m0;
    float4x4 _m1;
};

struct main0_out
{
    float4 m_9 [[user(locn0)]];
    float2 m_15 [[user(locn1)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 m_40 [[attribute(0)]];
    float4 m_11 [[attribute(1)]];
    float2 m_17 [[attribute(2)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant _28& _30 [[buffer(0)]])
{
    main0_out out = {};
    out.m_9 = in.m_11;
    out.m_15 = in.m_17;
    out.gl_Position = (_30._m0 * _30._m1) * float4(in.m_40, 1.0);
    return out;
}

