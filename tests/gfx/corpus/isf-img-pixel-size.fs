/*{
  "DESCRIPTION": "Pass 0 renders a 4x4 buffer, left half red, right half black. Pass 1 samples it in its own pixels: the top half of the frame shows IMG_PIXEL(small, (3, 2)), a texel of the black half; the bottom half shows IMG_THIS_PIXEL(small), the buffer pixel-aligned at the frame's origin (columns 0-1 red, 2-3 black). Normalised by RENDERSIZE instead, both read the red half. Used by GfxIsfImgPixel.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [
    { "TARGET": "small", "WIDTH": "4", "HEIGHT": "4" },
    { }
  ]
}*/
void main()
{
  if(PASSINDEX == 0)
  {
    gl_FragColor = isf_FragNormCoord.x < 0.5 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  if(isf_FragNormCoord.y > 0.5)
    gl_FragColor = IMG_PIXEL(small, vec2(3.5, 2.5));
  else
    gl_FragColor = IMG_THIS_PIXEL(small);
}
