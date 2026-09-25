void main()
{
    isf_vertShaderInit();

    uint base = per_draws.data[draw_id].skeleton_offset;
    mat4 skin = mat4(1.0);
    float wsum = weights_0.x + weights_0.y + weights_0.z + weights_0.w;
    if(base != 0xFFFFFFFFu && wsum > 1e-5)
    {
        vec4 w = weights_0 / wsum;
        skin = w.x * joint_matrices.mats[base + joints_0.x]
             + w.y * joint_matrices.mats[base + joints_0.y]
             + w.z * joint_matrices.mats[base + joints_0.z]
             + w.w * joint_matrices.mats[base + joints_0.w];
    }
    vec4 p = skin * vec4(position, 1.0);
    gl_Position = vec4(p.xy, 0.0, 1.0);

    isf_vertShaderFinish();
}
