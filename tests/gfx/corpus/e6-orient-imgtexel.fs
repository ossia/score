/*{
  "DESCRIPTION": "Procedural raw raster that copies its cabled texture with IMG_TEXEL at gl_FragCoord, the integer read that keeps IMG_PIXEL's row convention on every backend. Used by GfxE6ComputeImageOrientation.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "src", "TYPE": "image", "VISIBILITY": "fragment" }
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
    isf_FragColor = IMG_TEXEL(src, ivec2(gl_FragCoord.xy));
}
