/*{
  "DESCRIPTION": "PER_CUBE_FACE raw raster without a depth OUTPUT: two full-screen quads per face, red at window depth 0.2 then green at 0.8. DEPTH_COMPARE less: every face stays red; without a depth test the last quad, green, shows.",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "faces", "TYPE": "color", "FORMAT": "rgba8", "CUBEMAP": true,
      "LAYERS": 6, "WIDTH": 32, "HEIGHT": 32 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "faces" },
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_COMPARE": "less", "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
void main()
{
  isf_FragColor = v_color;
}
