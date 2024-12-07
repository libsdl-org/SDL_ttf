cbuffer _28_30 : register(b0, space1)
{
    row_major float4x4 _30_m0 : packoffset(c0);
    row_major float4x4 _30_m1 : packoffset(c4);
};


static float4 gl_Position;
static float4 _9;
static float4 _11;
static float2 _15;
static float2 _17;
static float3 _40;

struct SPIRV_Cross_Input
{
    float3 _40 : TEXCOORD0;
    float4 _11 : TEXCOORD1;
    float2 _17 : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 _9 : TEXCOORD0;
    float2 _15 : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    _9 = _11;
    _15 = _17;
    gl_Position = mul(float4(_40, 1.0f), mul(_30_m1, _30_m0));
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    _11 = stage_input._11;
    _17 = stage_input._17;
    _40 = stage_input._40;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output._9 = _9;
    stage_output._15 = _15;
    return stage_output;
}
