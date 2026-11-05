/*{
  "DESCRIPTION": "Loads a read_write 16x16 image before storing into it: each texel keeps what it held and adds a quarter of green. A storage image that starts at zero reads (0, 64, 0) on the first frame; one holding the memory of an earlier texture reads that instead. Used by GfxCsfStorageImageZero.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "state", "TYPE": "image", "ACCESS": "read_write", "FORMAT": "rgba8", "WIDTH": "16", "HEIGHT": "16" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [16, 16, 1] } }
  ]
}*/

void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  vec4 v = imageLoad(state, p);
  imageStore(state, p, vec4(v.r, v.g + 0.25, v.b, 1.0));
}
