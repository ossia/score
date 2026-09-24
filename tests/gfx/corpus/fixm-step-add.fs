/*{
  "DESCRIPTION": "Feedback probe: outputs its input plus one step of 1/32 grey, clamped. Wired into a loop, the level climbs one step per pass per frame.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    float v = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord).r;
    v = min(v + 1.0 / 32.0, 1.0);
    gl_FragColor = vec4(v, v, v, 1.0);
}
