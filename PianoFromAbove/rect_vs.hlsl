#include "rect.hlsli"

ConstantBuffer<ConstantBufferData> consts : register(b0);

PSInput main(float2 position : POSITION, float4 color : COLOR) {
    PSInput result;

    result.position = mul(consts.proj, float4(position, 0, 1));
    result.color = color;

    return result;
}