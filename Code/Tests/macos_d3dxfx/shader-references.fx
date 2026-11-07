// [TEMP] [CHOSEN, not recovered] FX2 named shaders and constant array references.
// [RETIRE-WHEN: macos-effect-cache-input-tests] Keep until equivalent runtime gates exist.
float4 VertexA(float4 p : POSITION) : POSITION { return p; }
float4 VertexB(float4 p : POSITION) : POSITION { return p * 2; }
float4 Pixel() : COLOR0 { return float4(1, 0, 0, 1); }
VertexShader SelectedVertex = compile vs_1_1 VertexA();
VertexShader VertexChoices[2] = {compile vs_1_1 VertexA(), compile vs_1_1 VertexB()};
PixelShader SelectedPixel = compile ps_1_1 Pixel();
technique References {
    pass Scalar { VertexShader = (SelectedVertex); PixelShader = (SelectedPixel); }
    pass Array { VertexShader = (VertexChoices[1]); PixelShader = (SelectedPixel); }
    pass Null { VertexShader = NULL; PixelShader = NULL; }
}
