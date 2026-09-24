/*{
  "DESCRIPTION": "Sixteen points on a 4x4 grid from a vertex shader that never writes gl_PointSize. Used by GfxPointsDefaultSize: an unwritten point size is undefined on Metal, so each point must still cover a single pixel.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 16,
    "TOPOLOGY": "points"
  }
}*/
void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
