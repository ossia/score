/*{
  "DESCRIPTION": "Samples its image input at u = 1.25 with the WRAP its JSON declares, clamp_to_edge: the right edge (green from isf-left-red-right-green.fs). The inlet's default address mode, repeat, would read u = 0.25 (red). Used by GfxIsfSamplerJson (shader board U34).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SAMPLER"],
  "INPUTS": [ { "NAME": "src", "TYPE": "image", "WRAP": "clamp_to_edge" } ]
}*/
void main()
{
    gl_FragColor = IMG_NORM_PIXEL(src, vec2(1.25, 0.5));
}
