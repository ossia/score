/*{
  "DESCRIPTION": "Self-feedback array probe: a two-layer output that writes layer 0 of its array input plus 1/32 grey. The array input grabs the producer's texture directly.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "prev", "TYPE": "image", "ARRAY": true }
  ],
  "OUTPUTS": [
    { "NAME": "layered", "LAYERS": 2 }
  ]
}*/
void main()
{
    float v = texture(prev, vec3(isf_FragNormCoord, 0.0)).r;
    v = min(v + 1.0 / 32.0, 1.0);
    layered = vec4(v, v, v, 1.0);
}
