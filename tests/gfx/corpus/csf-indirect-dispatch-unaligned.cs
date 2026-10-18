/*{
  "DESCRIPTION": "INDIRECT dispatch with an OFFSET of 2: indirect dispatch arguments must be 4-byte aligned. The engine must not hand that to the device; the pass runs with its WORKGROUPS ceiling [8, 8, 1] instead, and paints every texel of the 8x8 output red. Used by GfxCsfIndirectDispatchBounds.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "args", "TYPE": "storage", "ACCESS": "read_write", "BUFFER_USAGE": "dispatch_args",
      "LAYOUT": [ { "NAME": "xyz", "TYPE": "uint[8]" } ] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [8, 8, 1], "OFFSET": 2 } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    for(int i = 0; i < args.xyz.length(); i++)
      args.xyz[i] = 8u;
    return;
  }
  imageStore(outputImage, ivec2(gl_WorkGroupID.xy), vec4(1.0, 0.0, 0.0, 1.0));
}
