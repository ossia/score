void main()
{
    gl_Position = clipSpaceCorrMatrix * MODEL_MATRIX * vec4(position.xyz, 1.0);

    // GL clip convention BEFORE clipSpaceCorrMatrix: y == +1 is the TOP.
    // uv therefore has y == 1 on the top row of the delivered image, matching
    // the isf_FragNormCoord convention that GfxMrtPattern.cpp asserts against.
    v_uv = position.xy * 0.5 + 0.5;
}
