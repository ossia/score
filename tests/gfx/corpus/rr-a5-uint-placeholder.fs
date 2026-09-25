/*{
  "DESCRIPTION": "Reads two storage image AUXILIARY entries nothing publishes: an r32ui 3D image and an r32i 2D array image. Green when both read 0, red otherwise. Used by GfxRawRasterPlaceholderFormatA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "grid", "TYPE": "storage_3d", "FORMAT": "r32ui", "ACCESS": "read_only" },
    { "NAME": "layers", "TYPE": "storage_image_array", "FORMAT": "r32i", "ACCESS": "read_only" }
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
    uint g = imageLoad(grid, ivec3(0)).r;
    int l = imageLoad(layers, ivec3(0)).r;
    isf_FragColor = (g == 0u && l == 0) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
