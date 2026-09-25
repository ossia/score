/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn back to front, premultiplied: red alpha 0.5 at window depth 0.3, then green alpha 0.6 at 0.6. QUEUE transparent, depth test on, depth write off, no LAYER.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "QUEUE": "transparent",
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles", "DEPTH_TEST": true, "DEPTH_WRITE": false }
}*/
void main()
{
  isf_FragColor = v_color;
}
