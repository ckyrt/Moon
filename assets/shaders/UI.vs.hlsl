// UI Vertex Shader - 全屏四边形
struct VSInput {
    float2 Pos : ATTRIB0;
    float2 UV  : ATTRIB1;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.Pos = float4(input.Pos, 0.0, 1.0);
    output.UV = input.UV;
    return output;
}
