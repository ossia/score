void main()
{
    isf_vertShaderInit();
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    v_col = gl_VertexIndex == 0 ? vec4(1.0, 0.0, 0.0, 1.0)
          : gl_VertexIndex == 1 ? vec4(0.0, 1.0, 0.0, 1.0)
                                : vec4(0.0, 0.0, 1.0, 1.0);
    isf_vertShaderFinish();
}
