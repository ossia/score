/*{
  "DESCRIPTION": "One viewport-covering triangle with a vec4 attribute out_mask declared with the non-standard SEMANTIC face_mask, set to green.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "out_mask", "SEMANTIC": "face_mask", "TYPE": "vec4", "ACCESS": "write_only" }
      ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 3u)
        return;
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    if(idx == 2u) p = vec2(-1.0, 3.0);
    ISF_WRITE(geo, position)[idx] = vec4(p, 0.0, 1.0);
    ISF_WRITE(geo, out_mask)[idx] = vec4(0.0, 1.0, 0.0, 1.0);
}
