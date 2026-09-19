/*{
  "DESCRIPTION": "Adopts the owner's geometry and brightens it by a fixed step each frame, own-index only. If the loop carries, the picture gets brighter frame by frame.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= uint(ISF_READ(geo, color).length()))
        return;
    vec4 c = ISF_READ(geo, color)[idx];
    ISF_WRITE(geo, color)[idx] = vec4(clamp(c.rgb + vec3(0.02), 0.0, 1.0), 1.0);
}
