/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn back to front, premultiplied: red alpha 0.5 at window depth 0.3, then green alpha 0.6 at 0.6. Two targets, one additive, one max-blended over a 0.25 clear; the resolve reads both.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "sum" }, { "TYPE": "vec4", "NAME": "peak" } ],
  "INPUTS": [],
  "LAYER": {
    "TARGETS": [
      { "NAME": "sum", "FORMAT": "rgba16f", "BLEND": { "SRC": "one", "DST": "one" } },
      { "NAME": "peak", "FORMAT": "rgba16f", "CLEAR": [0.25, 0.25, 0.25, 0.25], "BLEND": { "SRC": "one", "DST": "one", "OP": "max" } }
    ],
    "RESOLVE": { "COMPOSITE": "replace" }
  },
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
#if defined(ISF_RESOLVE_PASS)
void main()
{
  vec4 s = LAYER_TEXEL(sum);
  vec4 p = LAYER_TEXEL(peak);
  isf_FragColor = vec4(s.r + s.g - 0.5 * s.a, p.g, p.b, 1.0);
}
#else
void main()
{
  sum = v_color;
  peak = v_color;
}
#endif
