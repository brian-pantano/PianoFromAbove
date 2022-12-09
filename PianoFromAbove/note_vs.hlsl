#include "rect.hlsli"
#include "tables.hlsli"

struct NoteData {
    uint packed;
    float pos;
    float length;
};

ConstantBuffer<ConstantBufferData> consts : register(b0);
StructuredBuffer<NoteData> note_data : register(t1);

PSInput main(uint id : SV_VertexID) {
    PSInput result;
    uint note_index = id / 8;
    uint vertex = id % 4;
    bool outline = id % 8 >= 4;
    
    uint packed = note_data[note_index].packed;
    uint note = packed & 0xFF;
    uint chan = (packed >> 8) & 0xFF;
    uint track = (packed >> 16) & 0xFFFF;
    bool sharp = ((1 << (note % 12)) & 0x54A) != 0;

    float deflate = outline ? 0 : clamp(round(consts.white_cx * 0.15f / 2.0f), 1.0f, 3.0f);
    float x = consts.notes_x + consts.white_cx * note_x[note] + deflate;
    float y = round(consts.notes_y + consts.notes_cy * (1.0f - note_data[note_index].pos / consts.timespan) + deflate);
    float cx = (sharp ? consts.white_cx * 0.65f : consts.white_cx) - deflate * 2.0;
    float cy = round(consts.notes_cy * (note_data[note_index].length / consts.timespan) - deflate * 2.0);

    float2 position = float2(x, y);
    position.y -= cy * float(vertex < 2);
    position.x += cx * float(vertex == 1 || vertex == 2);

    result.position = mul(consts.proj, float4(position, !sharp * 0.5, 1));
    result.color = outline ? float4(0, 0, 0, 0) : float4(1, 1, 1, 0);

    return result;
}