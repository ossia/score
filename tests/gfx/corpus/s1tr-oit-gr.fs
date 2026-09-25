/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn front to back, premultiplied: green alpha 0.6 at window depth 0.6, then red alpha 0.5 at 0.3. Weighted blended order-independent transparency (McGuire & Bavoil 2013), weight 1 + 9 * nearness.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "accum" }, { "TYPE": "vec4", "NAME": "reveal" } ],
  "INPUTS": [],
  "LAYER": {
    "TARGETS": [
      { "NAME": "accum", "FORMAT": "rgba16f", "BLEND": { "SRC": "one", "DST": "one" } },
      { "NAME": "reveal", "FORMAT": "r16f", "CLEAR": [1, 0, 0, 0], "BLEND": { "SRC_COLOR": "zero", "DST_COLOR": "one_minus_src_color", "SRC_ALPHA": "zero", "DST_ALPHA": "one_minus_src_alpha" } }
    ],
    "RESOLVE": { "COMPOSITE": "over" }
  },
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
#if defined(ISF_RESOLVE_PASS)
void main()
{
  vec4 a = LAYER_TEXEL(accum);
  float coverage = 1.0 - LAYER_TEXEL(reveal).r;
  isf_FragColor = vec4(a.rgb / max(a.a, 1e-5) * coverage, coverage);
}
#else
void main()
{
  float w = 1.0 + 9.0 * isf_depth_nearness(gl_FragCoord.z);
  accum = v_color * w;
  reveal = vec4(v_color.a);
}
#endif
