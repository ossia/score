/*{
  "DESCRIPTION": "Flat-white raw raster with an explicit DEPTH_COMPARE and no depth OUTPUT, so it draws onto the sink's render target.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE", "TEST-DEPTH"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "less",
    "CULL_MODE": "none"
  }
}*/

void main()
{
    isf_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
