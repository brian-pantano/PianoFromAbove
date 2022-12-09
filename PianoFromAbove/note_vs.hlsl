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

    float x = consts.white_cx * note_x[note];
    float y = consts.notes_y + consts.notes_cy * (1.0f - note_data[note_index].pos / consts.timespan);
    float cx = is_black[note % 12] ? consts.white_cx * 0.65f : consts.white_cx;
    float cy = consts.notes_cy * (note_data[note_index].length / consts.timespan);

    float2 position;
    switch (vertex) {
    case 0: position = float2(x, y); break;
    case 1: position = float2(x + cx, y); break;
    case 2: position = float2(x + cx, y + cy); break;
    case 3: position = float2(x, y + cy); break;
    }

    result.position = mul(float4(position, 0, 1), consts.proj);
    result.color = float4(1, 1, 1, 1);

    return result;
}