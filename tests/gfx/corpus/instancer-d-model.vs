void main()
{
    isf_vertShaderInit();

    PerDraw pd = per_draws.data[draw_id];
    mat3 instLinear = pd.normal[3].x > 0.5
        ? mat3(inst_matrix0.xyz, inst_matrix1.xyz, inst_matrix2.xyz)
        : mat3(1.0);
    v_custom = pd.normal[3].z > 0.5 ? inst_custom0 : vec4(1.0);
    gl_Position = clipSpaceCorrMatrix * pd.model
                  * vec4(instLinear * position + inst_translation, 1.0);

    isf_vertShaderFinish();
}
