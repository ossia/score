/*{
  "DESCRIPTION": "One viewport-covering triangle whose geometry carries an auxiliary storage buffer `items` of `count` vec4 elements. Changing `count` at run time changes the auxiliary's size. Used by GfxAuxSizeA5.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    { "NAME": "count", "TYPE": "long", "DEFAULT": 24, "MIN": 1, "MAX": 200 },
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
          "SIZE": "$count",
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
