/*{
  "DESCRIPTION": "Passthrough of a STATIC input (the producer's texture bound as is): IMG_THIS_PIXEL returns the raw texel.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "ALPHA": "premultiplied",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "STATIC": true } ]
}*/
void main()
{
    gl_FragColor = IMG_THIS_PIXEL(inputImage);
}
