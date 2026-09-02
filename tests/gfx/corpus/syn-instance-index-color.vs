void main()
{
    // Scene/instance transforms ride the instance-rate 'translation' vertex
    // attribute (RATE: instance in syn-instance-count-user.cs), NOT
    // MODEL_MATRIX -- MODEL_MATRIX stays identity here and is kept only for
    // the house convention (see syn-rr-mrt-pattern.vs).
    vec4 p = vec4(position.xy + translation.xy, 0.0, 1.0);
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * p;

    // Two independent per-instance identities:
    //  - v_buf_id:  what the reallocated translation buffer SAYS this
    //    instance is (translation.w == i/255, written by the PER_INSTANCE
    //    compute pass). Constant across the instance's primitives, so plain
    //    interpolation is exact.
    //  - v_draw_id: what the DRAW CALL says this instance is
    //    (gl_InstanceIndex, vertex-stage-only -> handed to the fragment
    //    stage through a flat int varying).
    // A stale buffer after a count change breaks the first; a wrong
    // cb.draw(..., instances) breaks the second.
    v_buf_id = translation.w;
    v_draw_id = gl_InstanceIndex;
}
