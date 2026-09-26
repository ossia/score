/*{
  "DESCRIPTION": "Procedural raw raster that copies its cabled texture with texelFetch at gl_FragCoord, the way 3dgs.tile's 08_Composite reads 07_TileRender's image. Used by GfxE6ComputeImageOrientation.",
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
    isf_FragColor = texelFetch(src, ivec2(gl_FragCoord.xy), 0);
}
