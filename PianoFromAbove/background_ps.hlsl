#include "common.hlsli"

Texture2D tex : register(t0);
SamplerState tex_sampler : register(s0);

float4 main(BackgroundPSInput input) : SV_TARGET{
	float4 col = tex.Sample(tex_sampler, input.uv);
	return float4(col.rgb, 1.0 - col.a);
}