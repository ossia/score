void main()
{
    // Always first: the engine's vertex prologue.
    isf_vertShaderInit();

    gl_Position = clipSpaceCorrMatrix
                * VIEWPROJECTION_MATRIX
                * MODEL_MATRIX
                * vec4(position.xyz, 1.0);
    v_color = color;

    // Always last: owns the one clip-space Y negation on Metal and D3D.
    isf_vertShaderFinish();
}
