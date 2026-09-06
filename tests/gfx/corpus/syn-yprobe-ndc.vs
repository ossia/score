void main()
{
    // Standard procedural fullscreen triangle (same construct as
    // mv-uniform-collision.vs / rr-perlayer.vs), then squashed into the +Y
    // half: y_ndc in [0, +1], full width. Nothing is drawn below y_ndc = 0,
    // so the lit row band answers "where does NDC +Y land in the readback".
    vec2 uv = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vec2 ndc = uv * 2.0 - 1.0;
    ndc.y = ndc.y * 0.5 + 0.5;
    gl_Position = clipSpaceCorrMatrix * vec4(ndc, 0.0, 1.0);
}
