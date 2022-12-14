#include "common.hlsli"

float4 main(RectPSInput input) : SV_TARGET {
    return input.color;
}