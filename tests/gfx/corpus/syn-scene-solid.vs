void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * position;
    gl_PointSize = 2.0;
}
