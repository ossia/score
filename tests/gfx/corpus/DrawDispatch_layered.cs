/*{
  "DESCRIPTION": "DrawDispatch coverage probe: INDIRECT must retain layered-image binding. Entire 8-cube is a fixed color, so a mid-slice is an exact oracle. Qt dev source already fixes layered bindings; this is expected clean there, SDK-version dependent elsewhere.",
  "ISFVSN": "2.0", "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "volume", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "8", "HEIGHT": "8", "DEPTH": "8" },
    { "NAME": "args", "TYPE": "storage", "ACCESS": "read_write", "BUFFER_USAGE": "dispatch_args", "LAYOUT": [{ "NAME": "xyz", "TYPE": "uint[4]" }] }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1,1,1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1,1,1] } },
    { "LOCAL_SIZE": [1,1,1], "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [8,8,8] } }
  ]
}*/
void main()
{
  if(PASSINDEX == 0)
  {
    args.xyz[0] = 8u; args.xyz[1] = 8u; args.xyz[2] = 8u; args.xyz[3] = 0u;
    return;
  }
  uvec3 g = gl_WorkGroupID;
  if(any(greaterThanEqual(g, uvec3(args.xyz[0], args.xyz[1], args.xyz[2])))) return;
  imageStore(volume, ivec3(g), vec4(64.,128.,192.,255.)/255.);
}
