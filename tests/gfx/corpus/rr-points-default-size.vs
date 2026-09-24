void main()
{
    isf_vertShaderInit();
    vec2 cell = vec2(float(gl_VertexIndex % 4), float(gl_VertexIndex / 4));
    gl_Position = vec4((cell + 0.5) / 2.0 - 1.0, 0.0, 1.0);
    isf_vertShaderFinish();
}
