/*{
  "DESCRIPTION": "GfxRawRasterFixK (N49): the same triangle with SAMPLES 1 declared on its colour OUTPUT, so it renders single-sampled under a multisampled renderer and the edge stays hard.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "color", "TYPE": "color", "SAMPLES": 1 } ],
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
