void main()
{
    // Procedural fullscreen triangle from gl_VertexIndex (no geometry input).
    // MULTIVIEW draws it into every view/layer in one pass. Transform by the
    // engine-provided pass-through multiview matrix isf_mv.viewProjection[VIEW_
    // INDEX] (identity when the SRB is correct). This is what makes the binding
    // collision OBSERVABLE: a missing or aliased descriptor for the multiview
    // UBO makes this read resolve to zero -> gl_Position collapses to the
    // origin -> a degenerate triangle -> nothing rasterized (black).
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vec4 pos = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    gl_Position = isf_mv.viewProjection[VIEW_INDEX] * pos;
}
