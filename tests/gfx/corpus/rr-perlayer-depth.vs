// Procedural PER_LAYER *depth* producer — vertex stage.
//
// No VERTEX_INPUTS: the fullscreen triangle is synthesised from gl_VertexIndex
// exactly as rr-perlayer.vs does (corpus/rr-perlayer.vs:3-5), so this shader
// needs no geometry node, no per_draws SSBO and no indirect buffer — unlike
// shadow_cascades.vert, which needs all three.
//
// The one thing it adds over rr-perlayer.vs: a per-layer clip-space z, so each
// PER_LAYER invocation writes a DIFFERENT, closed-form depth. PASSINDEX is the
// iteration index the runtime stamps into ProcessUBO
// (RenderedRawRasterPipelineNode.cpp:3123), which for EXECUTION_MODEL PER_LAYER
// is the array layer index 0..LAYERS-1; the same field shadow_cascades.vert:24
// uses as its cascade index. It is #defined onto the process UBO at
// 3rdparty/libisf/src/isf.cpp:95 / :114.
//
// Closed form (derivation in GfxPerLayerDepth.cpp's header):
//     z_gl(i) = 0.4 * i - 0.6      ->   -0.6, -0.2, +0.2, +0.6
//     window depth d(i) = 0.5 * z_gl(i) + 0.5  ->  0.2, 0.4, 0.6, 0.8
// on EVERY backend, because gl_Position goes through clipSpaceCorrMatrix:
// on OpenGL that matrix is the identity and glDepthRange maps NDC [-1,1] to
// [0,1]; on Vulkan/Metal/D3D it remaps clip z [-w,w] to [0,w] itself, "without
// further flipping" (Gfx/Graph/CameraMath.hpp:54-56). w is 1 here, so both
// routes land on the same 0.5*z+0.5.
void main()
{
    // Fullscreen triangle: p in {(0,0),(2,0),(0,2)} -> clip xy in
    // {(-1,-1),(3,-1),(-1,3)}, which covers the whole viewport.
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));

    float z_gl = 0.4 * float(PASSINDEX) - 0.6;

    gl_Position = clipSpaceCorrMatrix * vec4(p * 2.0 - 1.0, z_gl, 1.0);
}
