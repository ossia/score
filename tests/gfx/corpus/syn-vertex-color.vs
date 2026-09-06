void main()
{
    // Passthrough: positions come fully baked from the compute producer
    // (syn-indirect-ladder.cs); the per-vertex color carries the strip
    // identity the test asserts on. MODEL_MATRIX stays identity, kept for
    // the house convention (see syn-rr-mrt-pattern.vs).
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xy, 0.0, 1.0);
    v_color = color;
}
