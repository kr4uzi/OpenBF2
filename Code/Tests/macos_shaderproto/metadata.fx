// [TEMP] [CHOSEN, not recovered] Effect parser control without programmable shaders.
float4 Tint : DIFFUSE = float4(1.0, 0.5, 0.25, 1.0);
technique Metadata
{
    pass P0
    {
        AlphaBlendEnable = false;
        ZWriteEnable = true;
        SrcBlend = SrcAlpha;
    }
}
