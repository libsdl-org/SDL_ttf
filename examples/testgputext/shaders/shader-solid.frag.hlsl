struct PSInput {
    float4 color : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    output.color = input.color;
    return output;
}
