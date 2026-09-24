void main()
{
    isf_vertShaderInit();
    gl_Position = vec4(position.xy, 0.0, 1.0);
    isf_vertShaderFinish();
}
