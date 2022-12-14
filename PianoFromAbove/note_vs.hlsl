#include "common.hlsli"

struct NoteData {
    uint packed;
    float pos;
    float length;
};

#define MAX_TRACK_COLORS 1024
struct TrackColor {
    uint colors[3]; // primary, dark, darker
};

struct FixedSizeData {
    float note_x[128];
    float bends[16];
};

ConstantBuffer<RootSignatureData> root : register(b0);
StructuredBuffer<FixedSizeData> fixed : register(t1);
StructuredBuffer<TrackColor> colors : register(t2);
StructuredBuffer<NoteData> note_data : register(t3);

float3 unpack_color(uint col) {
    return float3(float((col >> 16) & 0xFF) / 255.0, float((col >> 8) & 0xFF) / 255.0, float(col & 0xFF) / 255.0);
}

[RootSignature("RootConstants(b0, num32BitConstants=20), SRV(t1), SRV(t2), SRV(t3)")]
NotePSInput main(uint id : SV_VertexID) {
    NotePSInput result;
    uint note_index = id / 4;
    uint vertex = id % 4;
    
    uint packed = note_data[note_index].packed;
    uint note = packed & 0xFF;
    uint chan = (packed >> 8) & 0xFF;
    uint track = ((packed >> 16) & 0xFFFF) % MAX_TRACK_COLORS;
    bool sharp = ((1 << (note % 12)) & 0x54A) != 0;

    //float deflate = outline ? 0 : clamp(round(root.white_cx * 0.15f / 2.0f), 1.0f, 3.0f);
    float x = fixed[0].note_x[note] + fixed[0].bends[chan];
    float y = round(root.notes_y + root.notes_cy * (1.0f - note_data[note_index].pos / root.timespan));
    float cx = sharp ? root.white_cx * 0.65f : root.white_cx;
    float cy = max(round(root.notes_cy * note_data[note_index].length / root.timespan), 0);

    bool is_right = vertex == 1 || vertex == 2;
    float2 position = float2(x, y);
    position.y -= cy * float(vertex < 2);
    position.x += cx * float(is_right);
    
    // Better to offload this work to the vertex shader rather than pixel
    // TODO: Can this work be done per-note and not per-vertex?
    //float2 left_top = mul(root.proj, float4(x, y, 0.5, 1)).xy;
    //float2 right_bottom = mul(root.proj, float4(x + cx, y - cy, 0.5, 1)).xy;

    //uint color_idx = outline ? 2 : right;
    result.position = mul(root.proj, float4(position, !sharp * 0.5, 1));
    result.color = float4(unpack_color(colors[track * 16 + chan].colors[is_right]), 0);
    result.border = float4(unpack_color(colors[track * 16 + chan].colors[2]), 0);
    //result.edges = float4(left_top, right_bottom);
    result.edges = float4(x, y, x + cx, y - cy);
    //result.color = outline ? float4(0, 0, 0, 0) : float4(1, 1, 1, 0);

    return result;
}