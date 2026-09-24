/*{
  "DESCRIPTION": "GfxRawRasterFixK (N49): one white triangle with a slanted edge and no SAMPLES declared, so the renderer's MSAA applies and the edge is antialiased.",
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
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "TOPOLOGY": "triangles", "VERTEX_COUNT": 3
  }
}*/

void main()
{
    isf_FragColor = vec4(1.0);
}
