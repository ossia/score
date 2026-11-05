void main()
{
  isf_vertShaderInit();
  // The shader keeps isf_FragNormCoord for its own use; IMG_THIS_PIXEL must
  // still read at the quad position init wrote.
  isf_FragNormCoord = vec2(0.0);
  isf_vertShaderFinish();
}
