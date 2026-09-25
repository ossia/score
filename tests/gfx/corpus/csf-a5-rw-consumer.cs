/*{
  "DESCRIPTION": "Adds 0.1 to the upstream colour at its own index, on a copy of the upstream data refreshed every frame. Used by GfxCsfReadWriteCopyA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= uint(ISF_READ(geo, color).length()))
        return;
    vec4 c = ISF_READ(geo, color)[idx];
    ISF_WRITE(geo, color)[idx] = vec4(c.rgb + vec3(0.1), 1.0);
}
