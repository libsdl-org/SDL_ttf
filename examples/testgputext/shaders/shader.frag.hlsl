Texture2D<float4> tex : register(t0, space2);
SamplerState samp : register(s0, space2);

struct PSInput {
    float4 color : TEXCOORD0;
    float2 tex_coord : TEXCOORD1;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    output.color = input.color * tex.Sample(samp, input.tex_coord);
    return output;
}
