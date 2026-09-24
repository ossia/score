void main()
{
    isf_vertShaderInit();

    // The raw-raster camera built-in. VIEWPROJECTION_MATRIX indexes the camera
    // block through VIEW_INDEX, which is 0 outside MULTIVIEW. MODEL_MATRIX is
    // identity under a scene chain -- placement there comes from per_draws --
    // so this measures the camera and nothing else.
    gl_Position = clipSpaceCorrMatrix * VIEWPROJECTION_MATRIX * MODEL_MATRIX * position;
    gl_PointSize = 2.0;

    isf_vertShaderFinish();
}
