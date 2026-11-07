// [TEMP] [CHOSEN, not recovered] Selector preshaders read a live effect parameter.
int Choice = 0;
float4 A(float4 p : POSITION) : POSITION { return p; }
float4 B(float4 p : POSITION) : POSITION { return p * 2; }
VertexShader Choices[2] = {compile vs_1_1 A(),compile vs_1_1 B()};
technique Select {
    pass P0 { VertexShader = (Choices[Choice]); PixelShader = NULL; }
}
