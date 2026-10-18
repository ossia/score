/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "A procedural triangle fan: PIPELINE_STATE TOPOLOGY triangle_fan, VERTEX_COUNT 8, no vertex data.",
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "TOPOLOGY": "triangle_fan", "VERTEX_COUNT": 8
  }
}*/
void main() { isf_FragColor = vec4(1.0); }
