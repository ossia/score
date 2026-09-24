/*{
  "DESCRIPTION": "GfxRawRasterFixK: copies its input to its output pixel for pixel. Its input render target is allocated at the renderer's sample count.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = IMG_THIS_PIXEL(inputImage);
}
