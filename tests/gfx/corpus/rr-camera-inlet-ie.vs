void main()
{
    isf_vertShaderInit();
    const vec2 corner[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
    vec2 p = vec2(0.6, 0.0) + 0.3 * corner[gl_VertexIndex];
    gl_Position = clipSpaceCorrMatrix * VIEWPROJECTION_MATRIX * vec4(p, 0.0, 1.0);
    isf_vertShaderFinish();
}
