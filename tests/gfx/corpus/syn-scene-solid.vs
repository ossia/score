void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);
    gl_PointSize = 2.0;
}
