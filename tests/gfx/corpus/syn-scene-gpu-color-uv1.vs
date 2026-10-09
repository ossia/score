void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(position.xy, 0.0, 1.0);
    v_color = color_0;
    v_uv1 = texcoord_1;
    isf_vertShaderFinish();
}
