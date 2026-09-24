/*{
  "DESCRIPTION": "Six sampled images ahead of an INPUTS storage image and an AUXILIARY storage image. Allocated after the samplers, the two storage images land on bindings 9 and 10, past the 8 image units NVIDIA's OpenGL exposes, and the shader does not compile there. Used by GfxStorageImageLowBindingN93: the frame is red on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [
    { "NAME": "t0", "TYPE": "image" },
    { "NAME": "t1", "TYPE": "image" },
    { "NAME": "t2", "TYPE": "image" },
    { "NAME": "t3", "TYPE": "image" },
    { "NAME": "t4", "TYPE": "image" },
    { "NAME": "t5", "TYPE": "image" },
    { "NAME": "img_in", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "rgba8", "VISIBILITY": "fragment" }
  ],
  "AUXILIARY": [
    { "NAME": "img_aux", "TYPE": "storage_image", "ACCESS": "write_only", "FORMAT": "rgba8" }
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
    vec4 s = texture(t0, vec2(0.5)) + texture(t1, vec2(0.5)) + texture(t2, vec2(0.5))
           + texture(t3, vec2(0.5)) + texture(t4, vec2(0.5)) + texture(t5, vec2(0.5))
           + imageLoad(img_in, ivec2(0));
    imageStore(img_aux, ivec2(0), vec4(1.0));
    isf_FragColor = vec4(1.0, 0.0, 0.0, 1.0) + 0.0 * s;
}
