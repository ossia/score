void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);
    v_color = color;
    v_worldPos = position.xyz;

}
