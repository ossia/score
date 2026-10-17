/*{
  "DESCRIPTION": "Image passthrough through IMG_PIXEL with a compound coordinate expression. IMG_PIXEL divides its coordinate by RENDERSIZE; an unparenthesised macro argument divides only the last term, so on the SPIR-V backends every pixel read the same texel. Used by GfxComputeImageOrientation.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = IMG_PIXEL(inputImage, isf_FragNormCoord * RENDERSIZE + 0.0 * RENDERSIZE);
}
