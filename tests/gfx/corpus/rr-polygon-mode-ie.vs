void main()
{
    isf_vertShaderInit();
    vec2 p = gl_VertexIndex == 0 ? vec2(-0.8, -0.8)
           : gl_VertexIndex == 1 ? vec2(0.8, -0.8)
                                 : vec2(0.0, 0.8);
    gl_Position = vec4(p, 0.0, 1.0);
    isf_vertShaderFinish();
}
