// A regular hexagon of radius 0.8 as a triangle fan of 8 vertices: the
// centre, then the rim from angle 0 round to angle 0 again.
void main()
{
    isf_vertShaderInit();
    vec2 p = vec2(0.0);
    if(gl_VertexIndex > 0)
    {
        const float a = float(gl_VertexIndex - 1) * 3.14159265 / 3.0;
        p = 0.8 * vec2(cos(a), sin(a));
    }
    gl_Position = vec4(p, 0.5, 1.0);
    isf_vertShaderFinish();
}
