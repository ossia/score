/*{
  "DESCRIPTION": "Raw raster with a writable AUXILIARY storage image that nothing produces, declared 7 x 5. Paints green when imageSize() reads 7 x 5, red otherwise (a 1 x 1 placeholder). Used by GfxRawRasterAuxStorageSize (shader board R46).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "scratch", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8",
      "WIDTH": 7, "HEIGHT": 5 }
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
    ivec2 s = imageSize(scratch);
    imageStore(scratch, ivec2(0), vec4(1.0));
    isf_FragColor = s == ivec2(7, 5) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
