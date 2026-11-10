// [TEMP] [CHOSEN, not recovered] D3DX9_25 struct-parameter oracle fixture (RaCommon.fx's Light).
struct Light { float3 pos; float3 dir; float4 color; float4 specularColor; float attenuation; };
struct Small { float2 uv; float s; int n; bool b; };
Light Lights[2];
Light One;
Small Mix;
float3 Vec3;
float4 Vec4;
float4x4 Mat;
float4 vs(float4 p : POSITION) : POSITION
{
    return float4(Lights[0].pos + Lights[1].color.xyz * Lights[0].dir + Mix.uv.xyy * Mix.s, 1) + mul(p, Mat) + One.color;
}
float4 vs2(float4 p : POSITION) : POSITION { return float4(Lights[0].color.xyz, 1) + One.pos.xyzz; }
technique T { pass P { VertexShader = compile vs_2_0 vs(); } }
technique U { pass P { VertexShader = compile vs_2_0 vs2(); } }
