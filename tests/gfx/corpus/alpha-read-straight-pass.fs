/*{
  "DESCRIPTION": "Straight passthrough (no ALPHA key): the average of IMG_THIS_PIXEL, IMG_THIS_NORM_PIXEL, IMG_NORM_PIXEL and IMG_PIXEL of a uniform input.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main()
{
    vec4 a = IMG_THIS_PIXEL(inputImage);
    vec4 b = IMG_THIS_NORM_PIXEL(inputImage);
    vec4 c = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
    vec4 d = IMG_PIXEL(inputImage, isf_FragNormCoord * RENDERSIZE);
    gl_FragColor = (a + b + c + d) * 0.25;
}
