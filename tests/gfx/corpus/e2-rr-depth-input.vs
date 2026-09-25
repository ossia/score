void main()
{
    isf_vertShaderInit();

    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2((idx & 1) != 0 ? 3.0 : -1.0, (idx & 2) != 0 ? 3.0 : -1.0);
    v_uv = vec2((ndc.x + 1.0) * 0.5, 1.0 - (ndc.y + 1.0) * 0.5);
    gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);

    isf_vertShaderFinish();
}
