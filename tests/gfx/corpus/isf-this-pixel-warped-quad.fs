/*{
  "DESCRIPTION": "IMG_THIS_PIXEL on a quad the vertex shader mirrors horizontally and shrinks to half size around the centre. As in VVISF, the read is in the image's own pixels at the quad's position before the warp, so the input is drawn mirrored and halved inside the centre of the frame. Used by GfxIsfImgPixel.",
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
