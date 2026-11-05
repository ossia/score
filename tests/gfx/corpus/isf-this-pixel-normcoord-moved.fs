/*{
  "DESCRIPTION": "IMG_THIS_PIXEL behind a vertex shader that overwrites isf_FragNormCoord after isf_vertShaderInit. As in VVISF, the read stays at the quad position init computed, so the input is copied unchanged. Used by GfxIsfImgPixel.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/
void main()
{
  gl_FragColor = IMG_THIS_PIXEL(inputImage);
}
