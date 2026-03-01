// Debug grid line pixel shader

struct PSInput {
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput i) : SV_TARGET
{
    return i.Color;
}
