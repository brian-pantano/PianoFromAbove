#include "rect.hlsli"

cbuffer consts : register(b0) {
    float4x4 proj;
}

PSInput main(float2 position : POSITION, float4 color : COLOR) {
    PSInput result;

    // TODO: don't hardcode
    result.position = mul(proj, float4(position, 0, 1));
    result.color = color;

    return result;
}