void main()
{
    isf_vertShaderInit();
    const vec2 p[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(0.0, 0.0), vec2(-1.0, 1.0),
        vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(0.0, 0.0));
    gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
    isf_vertShaderFinish();
}
