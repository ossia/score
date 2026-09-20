void main()
{
    isf_vertShaderInit();

    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);

    isf_vertShaderFinish();
}
