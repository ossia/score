void main()
{
    isf_vertShaderInit();

    v_uv = position.xy * 0.5 + 0.5;
    gl_Position = vec4(position.xy, 0.0, 1.0);

    isf_vertShaderFinish();
}
