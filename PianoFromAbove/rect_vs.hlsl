#include "rect.hlsli"

PSInput main(float4 position : POSITION, float4 color : COLOR) {
    PSInput result;

    result.position = position;
    result.color = color;

    return result;
}