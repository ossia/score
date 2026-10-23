/*{
  "DESCRIPTION": "GPU-only particle chain, emitter (loop owner). Owns an 8-slot pool and passes the fed-back state through. state.x is the alive flag: slot i comes alive the frame after slot i-1 was alive, and slot 0 on the first frame, so the live count grows by one slot per frame, decided only by the fed-back pool. state.y is the brightness the force node accumulates. The live count never reaches the CPU.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY", "TEST-INDIRECT"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "8",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "state",    "SEMANTIC": "pop_state", "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/

void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i >= 8u)
    return;
  vec4 s = ISF_READ(geo, state)[i];
  bool prevAlive = (i == 0u) || (ISF_READ(geo, state)[i - 1u].x > 0.5);
  if(s.x < 0.5 && prevAlive)
    s = vec4(1.0, 0.0, 0.0, 0.0);
  ISF_WRITE(geo, state)[i] = s;
  ISF_WRITE(geo, position)[i] = vec4(0.0, 0.0, 0.0, 1.0);
}
