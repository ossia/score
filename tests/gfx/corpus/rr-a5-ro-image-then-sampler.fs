/*{
  "DESCRIPTION": "A read_only storage image declared ahead of a sampled image. The storage image has no sampler binding, so t0 is the first sampler; a stray sampler for img_in would put the cabled texture one binding late. Used by GfxRawRasterReadOnlyImageA5: the frame shows what is cabled into t0.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "img_in", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" },
    { "NAME": "t0", "TYPE": "image" }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    vec4 s = imageLoad(img_in, ivec2(0));
    isf_FragColor = vec4(texture(t0, vec2(0.5)).rgb, 1.0) + 0.0 * s;
}
