struct PSInput {
    float4 color : TEXCOORD0;
    float2 tex_coord : TEXCOORD1;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    float2 dd = (0.5 - abs(input.tex_coord - 0.5)) / fwidth(input.tex_coord);
    float aa = clamp(min(dd.x, dd.y), 0.0, 1.0);
    output.color = float4(input.color.rgb, input.color.a * aa);
    return output;
}
