#include "common.hlsli"
#include "tables.hlsli"

struct NoteData {
    uint packed;
    float pos;
    float length;
};

ConstantBuffer<RootSignatureData> root : register(b0);
StructuredBuffer<FixedSizeData> consts1 : register(t1);
StructuredBuffer<TrackColor> colors : register(t2);
StructuredBuffer<NoteData> note_data : register(t3);

float3 unpack_color(uint col) {
    return float3(float((col >> 16) & 0xFF) / 255.0, float((col >> 8) & 0xFF) / 255.0, float(col & 0xFF) / 255.0);
}

[RootSignature("RootConstants(b0, num32BitConstants=20), SRV(t1), SRV(t2), SRV(t3)")]
PSInput main(uint id : SV_VertexID) {
    PSInput result;
    uint note_index = id / 8;
    uint vertex = id % 4;
    bool outline = id % 8 >= 4;
    
    uint packed = note_data[note_index].packed;
    uint note = packed & 0xFF;
    uint chan = (packed >> 8) & 0xFF;
    uint track = ((packed >> 16) & 0xFFFF) % MAX_TRACK_COLORS;
    bool sharp = ((1 << (note % 12)) & 0x54A) != 0;

    float deflate = outline ? 0 : clamp(round(root.white_cx * 0.15f / 2.0f), 1.0f, 3.0f);
    float x = consts1[0].note_x[note] + deflate;
    //float x = note_x[note] * root.white_cx + deflate;
    float y = round(root.notes_y + root.notes_cy * (1.0f - note_data[note_index].pos / root.timespan) + deflate);
    float cx = (sharp ? root.white_cx * 0.65f : root.white_cx) - deflate * 2.0;
    float cy = max(round(root.notes_cy * (note_data[note_index].length / root.timespan) - deflate * 2.0), 0);

    bool right = vertex == 1 || vertex == 2;
    float2 position = float2(x, y);
    position.y -= cy * float(vertex < 2);
    position.x += cx * float(right);

    uint color_idx = outline ? 2 : right;
    result.position = mul(root.proj, float4(position, !sharp * 0.5, 1));
    result.color = float4(unpack_color(colors[track * 16 + chan].colors[color_idx]), 0);
    //result.color = outline ? float4(0, 0, 0, 0) : float4(1, 1, 1, 0);

    return result;
}