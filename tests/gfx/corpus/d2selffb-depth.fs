/*{
  "DESCRIPTION": "Self-feedback depth probe: one fullscreen triangle at depth 0.5 with depth writes on. Red is the depth sampled from the input, green is the input's green plus 1/32. Fed by itself, red reads the depth this node wrote the frame before and green climbs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image", "DEPTH": true }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "greater",
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    float d = IMG_DEPTH_PIXEL(inputImage, gl_FragCoord.xy);
    float g = min(IMG_PIXEL(inputImage, gl_FragCoord.xy).g + 1.0 / 32.0, 1.0);
    isf_FragColor = vec4(d, g, 0.0, 1.0);
}
