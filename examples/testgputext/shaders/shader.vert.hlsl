cbuffer uniforms : register(b0, space1)
{
    row_major float4x4 proj_view : packoffset(c0);
    row_major float4x4 model : packoffset(c4);
};

struct VSInput
{
    float3 position : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 tex_coord : TEXCOORD2;
};

struct VSOutput
{
    float4 color : TEXCOORD0;
    float2 tex_coord : TEXCOORD1;
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.color = input.color;
    output.tex_coord = input.tex_coord;
    output.position = mul(float4(input.position, 1.0f), mul(model, proj_view));
    return output;
}
