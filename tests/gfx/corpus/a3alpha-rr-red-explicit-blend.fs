/*{
  "DESCRIPTION": "Procedural raw raster declaring PIPELINE_STATE.BLEND disabled: the explicit blend wins over the ALPHA-derived over, so it replaces what the target holds.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "VERTEX_COUNT": 3, "TOPOLOGY": "triangles", "BLEND": false }
}*/
void main()
{
    isf_FragColor = vec4(0.5, 0.0, 0.0, 0.5);
}
