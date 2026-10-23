/*{
  "DESCRIPTION": "Samples its image input at LOD 10 with the MIPMAP_MODE its JSON declares (linear). From isf-left-red-right-green.fs, the top of a mip chain is the average, (0.5, 0.5, 0); without a mip chain LOD 10 clamps to level 0 and reads red at u = 0.1. Used by GfxIsfSamplerJson (shader board U37).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SAMPLER"],
  "INPUTS": [ { "NAME": "src", "TYPE": "image", "MIPMAP_MODE": "linear", "FILTER": "linear" } ]
}*/
void main()
{
    gl_FragColor = vec4(textureLod(src, vec2(0.1, 0.5), 10.0).rgb, 1.0);
}
