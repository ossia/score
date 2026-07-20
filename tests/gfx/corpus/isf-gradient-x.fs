/*{
  "DESCRIPTION": "Horizontal red gradient: red = x. Used as a known texture for sampler address-mode tests (sampling beyond [0,1] wraps with REPEAT, pins to the right edge = 1.0 with CLAMP_TO_EDGE).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(isf_FragNormCoord.x, 0.2, 0.2, 1.0);
}
