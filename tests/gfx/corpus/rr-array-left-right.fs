/*{
  "DESCRIPTION": "Two-layer 64x64 colour TextureArray, every layer red on its left half (x < 32) and green on its right half.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-SAMPLER"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "OUTPUTS": [
    { "NAME": "layered", "TYPE": "color", "FORMAT": "rgba8",
      "LAYERS": 2, "WIDTH": 64, "HEIGHT": 64 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "layered" },
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  },
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = gl_FragCoord.x < 32.0 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
}
