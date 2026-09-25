// NDC-derived cube capture, vertex stage. Pairs with cb-cube-dir-ndc.fs; the
// same fullscreen triangle as the csf-examples IBL presets.
void main()
{
    isf_vertShaderInit();

    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);
    v_uv = (ndc + 1.0) * 0.5;
    v_face = gl_ViewIndex;
    gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);

    isf_vertShaderFinish();
}
