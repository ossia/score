void main()
{
    isf_vertShaderInit();
    gl_Position = clipSpaceCorrMatrix * vec4(position.xyz, 1.0);
    v_mask = in_mask;
    isf_vertShaderFinish();
}
