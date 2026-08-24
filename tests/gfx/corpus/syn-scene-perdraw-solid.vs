void main()
{
    // No camera in this rig, so the per-object model matrix alone places the
    // geometry -- the scene-path equivalent of syn-scene-solid.vs's
    // clipSpaceCorrMatrix * MODEL_MATRIX * position.
    gl_Position
        = clipSpaceCorrMatrix * per_draws.data[draw_id].model * vec4(position, 1.0);
    gl_PointSize = 2.0;
}
