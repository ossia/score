/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn back to front, premultiplied: red alpha 0.5 at window depth 0.3, then green alpha 0.6 at 0.6. The resolve replaces the consumer with (its depth, the layer green, 0).",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" } ],
  "INPUTS": [],
  "LAYER": { "RESOLVE": { "COMPOSITE": "replace", "DEPTH_INPUT": "sceneDepth" } },
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
#if defined(ISF_RESOLVE_PASS)
void main()
{
  isf_FragColor = vec4(LAYER_TEXEL(sceneDepth).r, LAYER_TEXEL(color).g, 0.0, 1.0);
}
#else
void main()
{
  color = v_color;
}
#endif
