/*{
  "DESCRIPTION": "GPU-only particle chain, force. Adopts the pool. Pass 0 counts the live slots on the GPU and writes them as indirect dispatch arguments. Pass 1 is dispatched indirectly from those arguments, one workgroup per live slot, and brightens that slot by 0.04 per frame. On the CPU ceiling fallback all 8 workgroups run and self-bound by reading the arguments, so both paths do the same work. Live slots are always a prefix, so workgroup g is slot g.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY", "TEST-INDIRECT"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position",  "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "state",    "SEMANTIC": "pop_state", "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    },
    {
      "NAME": "args",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "BUFFER_USAGE": "dispatch_args",
      "LAYOUT": [ { "NAME": "xyz", "TYPE": "uint[4]" } ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL",   "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [8, 1, 1] } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    uint n = 0u;
    for(uint i = 0u; i < 8u; i++)
    {
      vec4 s = ISF_READ(geo, state)[i];
      if(s.x > 0.5)
        n++;
      // Carry every slot through, so pass 1 only adds to live ones.
      ISF_WRITE(geo, state)[i] = s;
    }
    args.xyz[0] = n;
    args.xyz[1] = 1u;
    args.xyz[2] = 1u;
    args.xyz[3] = 0u;
    return;
  }

  uint g = gl_WorkGroupID.x;
  if(g >= args.xyz[0])
    return;
  vec4 s = ISF_WRITE(geo, state)[g];
  s.y = min(s.y + 0.04, 1.0);
  ISF_WRITE(geo, state)[g] = s;
}
