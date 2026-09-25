/*{
  "DESCRIPTION": "Self-feedback depth grab probe: a raw raster with a colour and a depth output and a STATIC (grabbing) image input. Red is the input sampled at the centre, green is 0.25. Its own depth output cabled into the input must read an empty texture, not the depth attachment it is rendering into.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "prev", "TYPE": "image", "STATIC": true }
  ],
  "OUTPUTS": [
    { "NAME": "color", "TYPE": "color" },
    { "NAME": "depth", "TYPE": "depth" }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "always",
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    float v = texture(prev, vec2(0.5)).r;
    isf_FragColor = vec4(v, 0.25, 0.0, 1.0);
}
