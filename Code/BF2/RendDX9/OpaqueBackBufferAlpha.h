// [CHOSEN, not recovered] Retail fix "The back buffer is presented opaque" (docs/retail-fixes.md).
// Shader tokens and geometry of the alpha-only draw that the renderer issues before every Present
// (RendDX9Draw.cpp). The GPU harness (Code/Tests/macos_metal_device) draws the same tokens, so the
// draw is tested through the Metal translation without the engine.
#pragma once

namespace dice { namespace hfe { namespace dx9rend { namespace opaque_alpha {

// vs.1.1: dcl_position0 v0; mov oPos, v0 - clip-space positions pass through. The same stream
// D3DX9_25 assembles for the renderer's antialiasing probes (RendDX9Init.cpp).
const unsigned int kVertexShader[] =
{
    0xFFFE0101u, 0x0000001Fu, 0x80000000u, 0x900F0000u,
    0x00000001u, 0xC00F0000u, 0x90E40000u, 0x0000FFFFu
};

// ps.1.1: def c0, 1, 1, 1, 1; mov r0, c0 - output alpha 1. The colour write mask keeps RGB.
const unsigned int kPixelShader[] =
{
    0xFFFF0101u, 0x00000051u, 0xA00F0000u, 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u,
    0x00000001u, 0x800F0000u, 0xA0E40000u, 0x0000FFFFu
};

// A triangle strip beyond the whole clip space, FLOAT3 positions with a 12-byte stride. D3D9's
// half-pixel offset moves a [-1, 1] quad half a pixel up and left, which leaves the samples of the
// last column and row uncovered under multisampling; the overshoot is clipped by the viewport.
const float kQuad[12] =
{
    -2.0f,  2.0f, 0.5f,
     2.0f,  2.0f, 0.5f,
    -2.0f, -2.0f, 0.5f,
     2.0f, -2.0f, 0.5f
};
const unsigned int kQuadStride = 12;
const unsigned int kQuadPrimitives = 2;

// The render states of the draw, {D3DRENDERSTATETYPE, value}: alpha writes only, no depth,
// stencil, alpha test, blending, fog or scissor, no culling, solid fill.
const unsigned int kRenderStates[][2] =
{
    { 168u, 0x8u },   // D3DRS_COLORWRITEENABLE = D3DCOLORWRITEENABLE_ALPHA
    {   7u, 0u },     // D3DRS_ZENABLE = D3DZB_FALSE
    {  14u, 0u },     // D3DRS_ZWRITEENABLE
    {  52u, 0u },     // D3DRS_STENCILENABLE
    {  15u, 0u },     // D3DRS_ALPHATESTENABLE
    {  27u, 0u },     // D3DRS_ALPHABLENDENABLE
    {  28u, 0u },     // D3DRS_FOGENABLE
    { 174u, 0u },     // D3DRS_SCISSORTESTENABLE
    {  22u, 1u },     // D3DRS_CULLMODE = D3DCULL_NONE
    {   8u, 3u }      // D3DRS_FILLMODE = D3DFILL_SOLID
};
const unsigned int kRenderStateCount = sizeof(kRenderStates) / sizeof(kRenderStates[0]);

} } } }
