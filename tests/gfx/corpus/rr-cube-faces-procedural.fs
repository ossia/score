/*{
  "DESCRIPTION": "Procedural PER_CUBE_FACE raw raster: each face is painted with red = PASSINDEX * 40 / 255, so +X reads 0 and +Z (face 4) reads 160. Wired straight to a window, the window shows the +Z face.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-CUBEMAP", "TEST-EXECUTION-MODEL"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "faces", "TYPE": "color", "FORMAT": "rgba8", "CUBEMAP": true,
      "LAYERS": 6, "WIDTH": 32, "HEIGHT": 32 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "faces" },
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    isf_FragColor = vec4(float(PASSINDEX * 40) / 255.0, 1.0, 0.0, 1.0);
}
