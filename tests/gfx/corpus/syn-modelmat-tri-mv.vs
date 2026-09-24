void main()
{
    isf_vertShaderInit();

    int idx = gl_VertexIndex % 3;
    vec2 ndc = vec2(
        (idx & 1) != 0 ? 3.0 : -1.0,
        (idx & 2) != 0 ? 3.0 : -1.0);

    // Red/green carry MODEL_MATRIX's translation column, biased so that a
    // translation of exactly 0 -- an identity matrix correctly read -- encodes
    // as mid-grey. BLUE is a constant 1.0 draw-witness: it separates "the
    // matrix came through the wrong binding" (blue 255, red/green not 127)
    // from "this pass never drew at all" (all three 0).
    v_color = vec3(MODEL_MATRIX[3].x + 0.5, MODEL_MATRIX[3].y + 0.5, 1.0);

    // position is kept live so the declared VERTEX_INPUTS binding is used.
    // z = 0.5, not 0.0: the project-wide depth convention is reverse-Z with a
    // GREATER compare and a clear of 0.0, so a triangle at z = 0.0 is rejected.
    gl_Position = vec4(ndc + vec2(0.0) * position.x, 0.5, 1.0);

    isf_vertShaderFinish();
}
