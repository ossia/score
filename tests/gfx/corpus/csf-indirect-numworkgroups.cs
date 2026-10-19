/*{
  "DESCRIPTION": "Pass 0 writes the dispatch arguments {3, 2, 1}; pass 1 is an INDIRECT dispatch of them with a WORKGROUPS ceiling [8, 8, 1]. Each workgroup of pass 1 stores gl_NumWorkGroups / 255 at its gl_WorkGroupID (IMG_STORE: top-left origin on every backend) in the 8x8 output: the 3x2 block must read (3, 2, 1). Used by GfxCsfIndirectNumWorkGroups.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "args", "TYPE": "storage", "ACCESS": "read_write", "BUFFER_USAGE": "dispatch_args",
      "LAYOUT": [ { "NAME": "xyz", "TYPE": "uint[3]" } ] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [8, 8, 1] } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    args.xyz[0] = 3u;
    args.xyz[1] = 2u;
    args.xyz[2] = 1u;
    return;
  }
  IMG_STORE(outputImage, ivec2(gl_WorkGroupID.xy), vec4(vec3(gl_NumWorkGroups) / 255.0, 1.0));
}
