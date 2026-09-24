/*{
  "DESCRIPTION": "One fullscreen triangle whose three vertices carry red, green and blue through a flat varying. Used by GfxProvokingVertex: flat shading takes the first vertex on every backend, so the whole frame is red.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_col", "INTERPOLATION": "flat" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_col", "INTERPOLATION": "flat" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
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
    isf_FragColor = v_col;
}
