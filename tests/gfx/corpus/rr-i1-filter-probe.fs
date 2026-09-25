/*{
  "DESCRIPTION": "Fills the frame with the red of its image input sampled at u = 0.4, v = 0.5. Used by GfxInputSamplerFilterI1 to see the inlet's filter change.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "src", "TYPE": "image" }
  ],
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
    isf_FragColor = vec4(textureLod(src, vec2(0.4, 0.5), 0.0).r, 0.0, 0.0, 1.0);
}
