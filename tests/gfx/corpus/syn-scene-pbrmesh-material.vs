void main()
{
    isf_vertShaderInit();
    v_color = scene_materials.data[per_draws.data[draw_id].material_index * 5u];
    gl_Position = vec4(position.xy, 0.0, 1.0);
    isf_vertShaderFinish();
}
