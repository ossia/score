void main()
{
    isf_vertShaderInit();

    mat4 model = per_draws.data[draw_id].model;
    gl_Position = clipSpaceCorrMatrix
                * VIEWPROJECTION_MATRIX
                * model
                * vec4(position, 1.0);

    isf_vertShaderFinish();
}
