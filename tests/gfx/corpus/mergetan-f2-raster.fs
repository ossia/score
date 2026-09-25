/*{
  "DESCRIPTION": "Raw raster painting the vertex tangent: red is tangent.x, green is the handedness tangent.w, both as v * 0.5 + 0.5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "in_tangent", "SEMANTIC": "tangent" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_tangent" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_tangent" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main()
{
    isf_FragColor = vec4(v_tangent.x * 0.5 + 0.5, v_tangent.w * 0.5 + 0.5, 0.0, 1.0);
}
