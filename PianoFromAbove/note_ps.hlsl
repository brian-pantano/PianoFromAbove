#include "common.hlsli"

ConstantBuffer<RootSignatureData> root : register(b0);

float4 main(NotePSInput input) : SV_TARGET {
    return (abs(input.position.x - input.edges.x) <= root.deflate ||
            abs(input.position.x - input.edges.z) <= root.deflate ||
            abs(input.position.y - input.edges.y) <= root.deflate ||
            abs(input.position.y - input.edges.w) <= root.deflate) ? input.border : input.color;
}