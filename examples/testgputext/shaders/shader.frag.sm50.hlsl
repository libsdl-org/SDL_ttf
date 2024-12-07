Texture2D<float4> _16 : register(t0);
SamplerState __16_sampler : register(s0);

static float4 _9;
static float4 _11;
static float2 _20;

struct SPIRV_Cross_Input
{
    float4 _11 : TEXCOORD0;
    float2 _20 : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 _9 : SV_Target0;
};

void frag_main()
{
    _9 = _11 * _16.Sample(__16_sampler, _20);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    _11 = stage_input._11;
    _20 = stage_input._20;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output._9 = _9;
    return stage_output;
}
