/*{
  "DESCRIPTION": "Passthrough declared ALPHA premultiplied: forwards the premultiplied texels of its input unchanged.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "ALPHA": "premultiplied",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main()
{
    gl_FragColor = IMG_NORM_PIXEL_PREMULTIPLIED(inputImage, isf_FragNormCoord);
}
