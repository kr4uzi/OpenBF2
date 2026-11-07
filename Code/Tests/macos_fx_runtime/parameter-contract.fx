// [TEMP] [CHOSEN, not recovered] D3DX compiler reflection/default-value oracle.
float3 Tint[2] < string UIName = "Tint"; > = {float3(1,2,3),float3(4,5,6)};
column_major float2x3 Columns = {1,2,3,4,5,6};
row_major float2x3 Rows = {1,2,3,4,5,6};
float Bias = 0.125;
bool EnableBlend = false;
float4 Pixel() : COLOR0 { return float4(Tint[0] * Bias, 1); }
technique Contract {
    pass Dynamic {
        AlphaBlendEnable = (EnableBlend);
        DepthBias = (Bias * 2);
        PixelShaderConstantF[3] = (Tint[1]);
        PixelShader = compile ps_2_0 Pixel();
    }
}
