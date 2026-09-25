void main()
{
    isf_vertShaderInit();

    // Procedural fullscreen triangle from gl_VertexIndex (no geometry input).
    // MULTIVIEW draws it into every view/layer in one pass.
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);

    isf_vertShaderFinish();
}
