/*{
  "DESCRIPTION": "Six AUXILIARY storage images: they take bindings 3 to 8, and binding 8 is past the 8 image units NVIDIA's OpenGL exposes. Used by GfxStorageImageUnitsA5: OpenGL warns once, other backends stay silent.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "img_a", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "img_b", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "img_c", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "img_d", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "img_e", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" },
    { "NAME": "img_f", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" }
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
    imageStore(img_a, ivec2(0), vec4(1.0));
    imageStore(img_b, ivec2(0), vec4(1.0));
    imageStore(img_c, ivec2(0), vec4(1.0));
    imageStore(img_d, ivec2(0), vec4(1.0));
    imageStore(img_e, ivec2(0), vec4(1.0));
    imageStore(img_f, ivec2(0), vec4(1.0));
    isf_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
