/*{
  "DESCRIPTION": "Red 0 on the left half, 1 on the right half. Rendered into a 2x2 input target it gives one black and one red texel per row, so a sample at u = 0.4 reads 0 through a nearest sampler and 0.3 through a linear one. Used by GfxInputSamplerFilterI1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": []
}*/
void main()
{
    gl_FragColor = vec4(step(0.5, isf_FragNormCoord.x), 0.0, 0.0, 1.0);
}
