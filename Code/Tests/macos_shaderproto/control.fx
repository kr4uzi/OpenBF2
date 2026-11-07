// [TEMP] [CHOSEN, not recovered] Original synthetic shader pipeline control.
// This fixture proves tool connectivity only; it is not a BF2 effect replacement.
float4 Tint = float4(1.0, 0.5, 0.25, 1.0);

float4 Vertex(float4 position : POSITION) : POSITION
{
    return position;
}

float4 Pixel() : COLOR0
{
    return Tint;
}

technique Control
{
    pass P0
    {
        AlphaBlendEnable = false;
        VertexShader = compile vs_2_0 Vertex();
        PixelShader = compile ps_2_0 Pixel();
    }
}
