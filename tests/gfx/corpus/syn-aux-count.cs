/*{
  "DESCRIPTION": "One viewport-covering triangle whose geometry carries an auxiliary storage buffer `items` of 24 vec4 elements. The consumer rr-aux-count.fs sizes its MANUAL invocation count from $COUNT_items.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-AUXILIARY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" }
      ],
      "AUXILIARY": [
        {
          "NAME": "items",
          "ACCESS": "read_write",
          "SIZE": "24",
          "LAYOUT": [
            { "NAME": "data", "TYPE": "vec4[]" }
          ]
        }
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
