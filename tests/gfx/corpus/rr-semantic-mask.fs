/*{
  "DESCRIPTION": "Raw raster reading in_mask through the non-standard SEMANTIC face_mask and painting it.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "in_mask", "SEMANTIC": "face_mask", "REQUIRED": false }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_mask" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_mask" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" }
}*/
void main()
{
    isf_FragColor = vec4(v_mask.rgb, 1.0);
}
