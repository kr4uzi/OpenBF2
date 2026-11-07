// [TEMP] [CHOSEN, not recovered] Nonliteral states must retain their FX2 payloads.
// [RETIRE-WHEN: fx-state-expression-evaluator] Keep until evaluator coverage replaces this control.
bool EnableBlend = false;
float Bias = 0.125;
float4 Pixel() : COLOR0 { return float4(1, 1, 1, 1); }
technique Expressions {
    pass Dynamic {
        AlphaBlendEnable = (EnableBlend);
        DepthBias = (Bias * 2);
        PixelShader = compile ps_2_0 Pixel();
    }
}
