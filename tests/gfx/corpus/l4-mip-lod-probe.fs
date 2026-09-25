/*{
  "DESCRIPTION": "Samples its input at mip level 3 (GfxMipToggleL4.cpp). With a generated mip chain a one-pixel checkerboard reads back as 0.5 grey; without one the lookup clamps to level 0 and returns the checkerboard itself.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = vec4(textureLod(inputImage, isf_FragNormCoord, 3.0).rgb, 1.0);
}
