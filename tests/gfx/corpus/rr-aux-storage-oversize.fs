/*{
  "DESCRIPTION": "Raw raster with a writable AUXILIARY storage image declared 1048576 x 4, past every device's TextureSizeMax: the node clamps it. Paints green when imageSize() reads a clamped width and the declared height, red otherwise. Used by GfxRawRasterSizeClamp.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "scratch", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8",
      "WIDTH": 1048576, "HEIGHT": 4 }
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
    isf_FragColor = s.x >= 1024 && s.x < 1048576 && s.y == 4 ? vec4(0.0, 1.0, 0.0, 1.0)
                                                            : vec4(1.0, 0.0, 0.0, 1.0);
}
