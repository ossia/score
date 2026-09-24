const vec2 corners[3] = vec2[3](vec2(-1.0, -1.0), vec2(1.0, -0.3), vec2(-1.0, 1.0));
void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(corners[gl_VertexIndex % 3], 0.5, 1.0);
    isf_vertShaderFinish();
}
