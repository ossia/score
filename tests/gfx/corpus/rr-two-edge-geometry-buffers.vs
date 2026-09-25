void main()
{
    isf_vertShaderInit();
    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);
    gl_Position = vec4(ndc + vec2(0.0) * position.x, 0.0, 1.0);
    isf_vertShaderFinish();
}
