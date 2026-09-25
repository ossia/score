// Camera-driven cube capture, vertex stage. Pairs with cb-cube-dir-camera.fs.
//
// A fullscreen triangle per view whose vertices carry the two world-space
// points their clip position unprojects to, at two depths, through
// camera[VIEW_INDEX]'s viewProjection: every fragment sees the world
// direction a real mesh drawn with clipSpaceCorrMatrix * VIEWPROJECTION would
// put at that pixel. The unprojected w depends only on the depth, so each
// point interpolates exactly across the triangle.
void main()
{
    isf_vertShaderInit();

    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);

    int base = VIEW_INDEX * 15;
    mat4 vp = mat4(
        camera.data[base + 8], camera.data[base + 9],
        camera.data[base + 10], camera.data[base + 11]);
    mat4 ivp = inverse(vp);
    v_near = ivp * vec4(ndc, 0.75, 1.0);
    v_far = ivp * vec4(ndc, 0.25, 1.0);

    gl_Position = clipSpaceCorrMatrix * vec4(ndc + vec2(0.0) * position.x, 0.5, 1.0);

    isf_vertShaderFinish();
}
