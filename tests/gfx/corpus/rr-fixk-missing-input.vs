void main()
{
    isf_vertShaderInit();
    gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.0) + 0.0 * fixk_absent;
    isf_vertShaderFinish();
}
