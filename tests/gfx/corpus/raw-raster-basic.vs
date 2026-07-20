void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);
    v_color = color;
    gl_PointSize = 2.0;

}
