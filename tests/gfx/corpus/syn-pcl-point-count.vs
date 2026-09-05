void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);
    v_color = vec4(1.0, 0.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
