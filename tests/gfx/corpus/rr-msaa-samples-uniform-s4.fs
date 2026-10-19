/*{
  "DESCRIPTION": "GfxMsaaSamplesUniform: a full-screen triangle declaring SAMPLES 4 on its colour OUTPUT. Red is MSAA_SAMPLES / 16, the sample count the pass actually renders at.",
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
    isf_FragColor = vec4(float(MSAA_SAMPLES) / 16.0, 0.0, 0.0, 1.0);
}
