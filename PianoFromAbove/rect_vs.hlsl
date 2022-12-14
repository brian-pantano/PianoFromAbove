#include "common.hlsli"

ConstantBuffer<RootSignatureData> consts : register(b0);

RectPSInput main(float2 position : POSITION, float4 color : COLOR) {
    RectPSInput result;

    result.position = mul(consts.proj, float4(position, 0, 1));
    result.color = color;

    return result;
}