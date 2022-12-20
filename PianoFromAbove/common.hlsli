struct RootSignatureData {
    float4x4 proj;
    float deflate;
    float notes_y;
    float notes_cy;
    float white_cx;
    float timespan;
};

struct RectPSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct NotePSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float4 edges : TEXCOORD0; // left, top, right, bottom
    float4 border : COLOR1;
};

struct BackgroundPSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};