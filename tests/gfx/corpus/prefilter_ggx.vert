// Fullscreen-triangle per cube face. MULTIVIEW=6 fires this 6× per draw
// — gl_ViewIndex=0..5 selects which of the 6 LAYERS of the prefiltered
// output array this invocation rasterises into. The mip level iteration
// is driven by EXECUTION_MODEL:PER_MIP.
//
// clipSpaceCorrMatrix + the non-GL conditional Y-flip match the engine-
// wide ossia convention (see isf.cpp's vertexInitFunc).
void main()
{
    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);
    v_uv = (ndc + 1.0) * 0.5;
    v_face = gl_ViewIndex;
    gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
    gl_Position.y = -gl_Position.y;
#endif
}
