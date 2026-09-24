/*{
  "DESCRIPTION": "In-place modifier: adds 0.25 to position.x and 0.25 to color.r. With a static upstream the result must be the same every frame.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_READ(geo, position).length())) return;
    vec4 p = ISF_READ(geo, position)[i];
    vec4 c = ISF_READ(geo, color)[i];
    ISF_WRITE(geo, position)[i] = vec4(p.x + 0.25, p.yzw);
    ISF_WRITE(geo, color)[i] = vec4(c.r + 0.25, c.gba);
}
