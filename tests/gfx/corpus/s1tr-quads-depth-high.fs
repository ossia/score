/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn back to front, premultiplied: red alpha 0.5 at window depth 0.3, then green alpha 0.6 at 0.6. Colour and alpha-weighted depth targets; the resolve writes the expected depth where the alpha reaches the threshold input (0.9).",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "color" }, { "TYPE": "vec4", "NAME": "zsum" } ],
  "INPUTS": [ { "NAME": "threshold", "TYPE": "float", "DEFAULT": 0.9 } ],
  "LAYER": {
    "TARGETS": [ { "NAME": "color" }, { "NAME": "zsum", "FORMAT": "r32f" } ],
    "RESOLVE": { "DEPTH_WRITE": true }
  },
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
#if defined(ISF_RESOLVE_PASS)
void main()
{
  vec4 c = LAYER_TEXEL(color);
  isf_FragColor = c;
  gl_FragDepth = c.a >= threshold ? LAYER_TEXEL(zsum).r / c.a : ISF_DEPTH_FAR;
}
#else
void main()
{
  color = v_color;
  zsum = vec4(gl_FragCoord.z * v_color.a, 0.0, 0.0, v_color.a);
}
#endif
