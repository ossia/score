void main()
{
    v_mm = vec4(MODEL_MATRIX[3].xyz, 1.0);
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * position;
    gl_PointSize = 2.0;
}
