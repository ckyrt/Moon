// BRDF Integration LUT - Vertex Shader
// Fullscreen triangle for offline BRDF LUT generation

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

void main(uint vertexId : SV_VertexID, out VSOutput output) {
    // Fullscreen triangle trick
    // vertexId: 0, 1, 2
    // UV: (0,0), (2,0), (0,2) -> clips to (0,0), (1,0), (0,1)
    output.UV = float2((vertexId << 1) & 2, vertexId & 2);
    output.Pos = float4(output.UV * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
