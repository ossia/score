/*{
  "DESCRIPTION": "ISF that copies its input with IMG_TEXEL at gl_FragCoord. Used by GfxE6ComputeImageOrientation.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = IMG_TEXEL(inputImage, ivec2(gl_FragCoord.xy));
}
