/*{
  "DESCRIPTION": "Writes its geometry on frame 0 only: a viewport-covering triangle, colour 0.2 grey, and a `state` auxiliary of three vec4 at 0.2. Later frames dispatch but write nothing. Used by GfxCsfReadWriteCopyA5 as an init-once upstream.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "write_only" }
      ],
      "AUXILIARY": [
        { "NAME": "state", "ACCESS": "read_write", "SIZE": "3", "LAYOUT": [ { "NAME": "data", "TYPE": "vec4[]" } ] }
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
    if(idx >= 3u || FRAMEINDEX != 0)
        return;
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    if(idx == 2u) p = vec2(-1.0, 3.0);
    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    geo_color_out[idx] = vec4(0.2, 0.2, 0.2, 1.0);
    geo_state.data[idx] = vec4(0.2);
}
