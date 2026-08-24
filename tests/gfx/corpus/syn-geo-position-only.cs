/*{
  "DESCRIPTION": "Emits a viewport-covering triangle with position and nothing else. Upstream for the attribute-addition case: whatever colour the consumer draws must have been added downstream, because there is none here.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" }
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
    if(idx >= 3u)
        return;

    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    if(idx == 2u) p = vec2(-1.0, 3.0);

    geo_position_out[idx] = vec4(p, 0.0, 1.0);
}
