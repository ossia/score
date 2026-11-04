/*{
  "DESCRIPTION": "Reads an input whose inlet render size is smaller than the output. The top half of the frame shows IMG_THIS_NORM_PIXEL(inputImage), the input stretched over the frame; the bottom half shows IMG_THIS_PIXEL(inputImage), the input pixel-aligned at the frame's origin and clamped past its size. A compositing shader (Video Mixer) needs the first form for layers rendered at a lower resolution. Used by GfxIsfImgPixel.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/
void main()
{
  if(isf_FragNormCoord.y > 0.5)
    gl_FragColor = IMG_THIS_NORM_PIXEL(inputImage);
  else
    gl_FragColor = IMG_THIS_PIXEL(inputImage);
}
