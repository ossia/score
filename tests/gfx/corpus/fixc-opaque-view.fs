/*{
  "DESCRIPTION": "Opaque view of the input: left half shows its rgb, right half its alpha as grey.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main()
{
    vec4 c = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
    gl_FragColor = isf_FragNormCoord.x < 0.5 ? vec4(c.rgb, 1.0) : vec4(c.aaa, 1.0);
}
