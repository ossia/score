/*{
  "DESCRIPTION": "Binds its upstream texture directly (STATIC) and samples it at LOD 10: the producer's own mip chain, (0.5, 0.5, 0) from rr-generate-mips-single.fs, or red at u = 0.1 when the producer has none. Used by GfxRawRasterGenerateMips (shader board R32).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SAMPLER"],
  "INPUTS": [ { "NAME": "src", "TYPE": "image", "STATIC": true, "MIPMAP_MODE": "linear", "FILTER": "linear" } ]
}*/
void main()
{
    gl_FragColor = vec4(textureLod(src, vec2(0.1, 0.5), 10.0).rgb, 1.0);
}
