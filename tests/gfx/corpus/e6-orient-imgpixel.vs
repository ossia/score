const vec2 e6corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(e6corners[gl_VertexIndex % 3], 0.0, 1.0);
    isf_vertShaderFinish();
}
