void main()
{
    isf_vertShaderInit();
    gl_Position = clipSpaceCorrMatrix * vec4(position.xyz, 1.0);
    v_normal = vec4(in_normal.xyz, 0.0);
    isf_vertShaderFinish();
}
