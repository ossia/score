/*{
  "DESCRIPTION": "S1 LAYER fixture: two full-screen quads drawn front to back, premultiplied: green alpha 0.6 at window depth 0.6, then red alpha 0.5 at 0.3. LAYER with its default target and resolve.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "LAYER": {},
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
void main()
{
  isf_FragColor = v_color;
}
