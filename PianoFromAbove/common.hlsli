struct RootSignatureData {
    float4x4 proj;
    float notes_y;
    float notes_cy;
    float white_cx;
    float timespan;
};

#define MAX_TRACK_COLORS 1024
struct TrackColor {
    uint colors[3]; // primary, dark, darker
};

struct FixedSizeData {
    float note_x[128];
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};