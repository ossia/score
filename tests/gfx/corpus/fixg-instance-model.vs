void main()
{
    isf_vertShaderInit();

    // The library rasterizers' placement of an instance: its translation in the
    // prototype's space, then the draw's own model matrix.
    gl_Position = clipSpaceCorrMatrix * per_draws.data[draw_id].model
                  * vec4(position + translation, 1.0);

    isf_vertShaderFinish();
}
