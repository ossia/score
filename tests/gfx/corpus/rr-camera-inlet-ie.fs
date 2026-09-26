/*{
  "DESCRIPTION": "A white square, world x in [0.3, 0.9], y in [-0.3, 0.3], z = 0, placed through VIEWPROJECTION_MATRIX. Used by GfxRawRasterCameraInletIE: with the identity stand-in camera it covers NDC x in [0.3, 0.9]; seen by a 60-degree camera at (0, 0, 3) it covers NDC x in [0.17, 0.52].",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 6,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    isf_FragColor = vec4(vec3(gain), 1.0);
}
