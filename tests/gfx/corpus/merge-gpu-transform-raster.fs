/*{
  "DESCRIPTION": "Raw raster painting the vertex normal as colour, n * 0.5 + 0.5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "in_normal", "SEMANTIC": "normal" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_normal" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_normal" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main()
{
    isf_FragColor = vec4(v_normal.xyz * 0.5 + 0.5, 1.0);
}
