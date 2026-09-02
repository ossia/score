void main()
{
    mat4 M = per_draws.data[draw_id].model;
    v_mm = vec4(M[3].xyz, 1.0);
    // No camera in this rig (the chain has no Camera process), so the model
    // matrix alone places the geometry -- exactly as syn-scene-solid.vs does
    // with MODEL_MATRIX.
    gl_Position = clipSpaceCorrMatrix * M * vec4(position, 1.0);
    gl_PointSize = 2.0;
}
