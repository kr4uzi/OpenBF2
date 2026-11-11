// [TEMP] [CHOSEN, not recovered] D3DX9_25 oracle fixture: matrix members inside structs
// (RaShaderBMCommon.fx's UserData) and their constant-table register packing.
struct UD { float4x4 uvMatrix[7] : UVMatrix; };
UD UserData;
struct MX { float3x3 a; float4x3 b; float2 v; row_major float4x4 r; float3x4 c; };
MX Mixed;
UD Static;
float4 vs(float4 p : POSITION, float4 t : TEXCOORD0) : POSITION
{
    float3x3 m = (float3x3)UserData.uvMatrix[t.w];
    float3 a = mul(t.xyz, m) + mul(t.xyz, Mixed.a) + mul(p, Mixed.b) + mul(p, Mixed.r).xyz + mul(t.xyz, Mixed.c).xyz;
    return float4(a + Mixed.v.xyx, 1) + mul(p, (float3x3)Static.uvMatrix[2]).xyzz;
}
technique T { pass P { VertexShader = compile vs_2_0 vs(); } }
