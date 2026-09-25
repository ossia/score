/*{
  "DESCRIPTION": "A Params block and six storage images: the images take bindings 3 to 8, and binding 8 is past the 8 image units NVIDIA's OpenGL exposes. Used by GfxStorageImageUnitsA5: OpenGL warns once, other backends stay silent.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "s0", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "s1", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "s2", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "s3", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "s4", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if(pos.x >= 16 || pos.y >= 16)
        return;
    vec4 s = imageLoad(s0, pos) + imageLoad(s1, pos) + imageLoad(s2, pos) + imageLoad(s3, pos)
           + imageLoad(s4, pos);
    imageStore(outputImage, pos, vec4(gain, 0.0, 0.0, 1.0) + 0.0 * s);
}
