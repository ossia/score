void main()
{
    // MODEL_MATRIX stays identity for this scene (house convention, see
    // syn-scene-solid.vs); the per-instance translation is the only transform.
    vec4 p = vec4(position.xy + inst_translation.xy, 0.0, 1.0);
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * p;
    v_col = inst_color0;
}
