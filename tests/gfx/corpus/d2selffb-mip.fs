/*{
  "DESCRIPTION": "Self-feedback mip probe: outputs the input's 1x1 mip level plus 1/32 grey. The input is 32x32 and sampled with a mip filter; the level only climbs if the input's mip chain follows its base level.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/
void main()
{
    float v = textureLod(inputImage, isf_FragNormCoord, 5.0).r;
    v = min(v + 1.0 / 32.0, 1.0);
    gl_FragColor = vec4(v, v, v, 1.0);
}
