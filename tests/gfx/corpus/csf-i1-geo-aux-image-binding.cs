/*{
  "DESCRIPTION": "A Params block, five sampled textures, a write-only output image and a geometry attribute ahead of a storage-image AUXILIARY on the geometry input. Numbered in RESOURCES order, the output image lands on binding 8 and the auxiliary image on 10, past the 8 image units NVIDIA's OpenGL exposes. The output image is red. Used by GfxIsfCsfImageBindingI1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "t0", "TYPE": "texture" },
    { "NAME": "t1", "TYPE": "texture" },
    { "NAME": "t2", "TYPE": "texture" },
    { "NAME": "t3", "TYPE": "texture" },
    { "NAME": "t4", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "64", "HEIGHT": "64" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false } ],
      "AUXILIARY": [ { "NAME": "vox", "TYPE": "storage_image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": 4, "HEIGHT": 4 } ] }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 64 || pos.y >= 64)
        return;
    vec2 uv = (vec2(pos) + 0.5) / 64.0;
    vec4 s = texture(t0, uv) + texture(t1, uv) + texture(t2, uv)
           + texture(t3, uv) + texture(t4, uv);
    vec4 v = imageLoad(vox, ivec2(0));
    if(pos == ivec2(0))
        imageStore(vox, ivec2(0), vec4(1.0));
    imageStore(outputImage, pos, vec4(gain, 0.0, 0.0, 1.0) + 0.0 * s + 0.0 * v);
}
