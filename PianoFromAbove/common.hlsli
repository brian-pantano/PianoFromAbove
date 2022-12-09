struct RootSignatureData {
    float4x4 proj;
    float notes_y;
    float notes_cy;
    float white_cx;
    float timespan;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};