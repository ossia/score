/*{
  "DESCRIPTION": "PER_CUBE_FACE raw raster without a depth OUTPUT: two full-screen quads per face, green at window depth 0.8 then red at 0.2. With the default reverse-Z test (greater) every face stays green.",
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
  "PIPELINE_STATE": { "CULL_MODE": "none", "VERTEX_COUNT": 12, "TOPOLOGY": "triangles" }
}*/
void main()
{
  isf_FragColor = v_color;
}
