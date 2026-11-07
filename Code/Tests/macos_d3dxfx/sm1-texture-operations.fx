// [TEMP] [CHOSEN, not recovered] Explicit SM1 texture operations, no game content.
// [RETIRE-WHEN: macos-effect-cache-input-tests] Keep until equivalent translation gates exist.
PixelShader Tex14 = asm {
    ps_1_4
    texcrd r2.xyz, t1
    texld r0, t0
    phase
    texld r1, r2
    mul r0, r0, r1
};
PixelShader Project14 = asm {
    ps_1_4
    texld r0, t2_dw.xyw
};
PixelShader Tex13 = asm {
    ps_1_3
    texcoord t0
    mov r0, t0
};
technique Textures {
    pass Explicit { PixelShader = (Tex14); }
    pass Project { PixelShader = (Project14); }
    pass Legacy { PixelShader = (Tex13); }
}
