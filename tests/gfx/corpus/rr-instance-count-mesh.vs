void main()
{
    isf_vertShaderInit();
    // Quadrant centre of this instance; the incoming fullscreen triangle,
    // (-1,-1) (3,-1) (-1,3), scaled by 0.1 covers it.
    vec2 c = vec2((gl_InstanceIndex & 1) == 0 ? -0.5 : 0.5,
                  (gl_InstanceIndex & 2) == 0 ? -0.5 : 0.5);
    gl_Position = vec4(c + position.xy * 0.1, 0.0, 1.0);
    isf_vertShaderFinish();
}
