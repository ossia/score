/*{
  "DESCRIPTION": "Premultiplied passthrough: the average of the four *_PREMULTIPLIED sampling macros of a uniform input.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "ALPHA": "premultiplied",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main()
{
    vec4 a = IMG_THIS_PIXEL_PREMULTIPLIED(inputImage);
    vec4 b = IMG_THIS_NORM_PIXEL_PREMULTIPLIED(inputImage);
    vec4 c = IMG_NORM_PIXEL_PREMULTIPLIED(inputImage, isf_FragNormCoord);
    vec4 d = IMG_PIXEL_PREMULTIPLIED(inputImage, isf_FragNormCoord * RENDERSIZE);
    gl_FragColor = (a + b + c + d) * 0.25;
}
