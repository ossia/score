/*{
  "DESCRIPTION": "GfxRawRasterOutputAndVertexInput: the same triangle with SAMPLES 4 declared on its colour OUTPUT and no depth OUTPUT, so it renders multisampled under a single-sampled renderer and the edge is antialiased.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "color", "TYPE": "color", "SAMPLES": 4 } ],
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
