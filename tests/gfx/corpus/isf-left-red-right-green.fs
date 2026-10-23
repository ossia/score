/*{
  "DESCRIPTION": "Left half red, right half green. Producer for GfxIsfSamplerJson.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SAMPLER"],
  "INPUTS": []
}*/
void main()
{
    gl_FragColor = isf_FragNormCoord.x < 0.5 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
}
