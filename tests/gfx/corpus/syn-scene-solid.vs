void main()
{
    isf_vertShaderInit();

    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * position;
    gl_PointSize = 2.0;

    isf_vertShaderFinish();
}
