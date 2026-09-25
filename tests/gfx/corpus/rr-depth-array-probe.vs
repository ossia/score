void main()
{
    isf_vertShaderInit();
    int idx = gl_VertexIndex % 3;
    gl_Position = vec4(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0, 0.0, 1.0);
    isf_vertShaderFinish();
}
