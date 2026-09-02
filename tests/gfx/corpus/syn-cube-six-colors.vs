// Fullscreen-triangle per face. MULTIVIEW=6 fires this 6 times in one
// draw; gl_ViewIndex selects which layer of the target the fragment
// stage writes into. No INPUTS — just propagate the view index so the
// fragment stage can colour-code it.
//
// Lifted verbatim from the `test_cube_colors` process embedded in the
// user diagnostic score 2026/test-cubemap-output.score (see the paired
// .fs for the self-declared oracle).
void main()
{
    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);
    v_face = gl_ViewIndex;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
