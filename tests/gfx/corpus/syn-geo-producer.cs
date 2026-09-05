/*{
  "DESCRIPTION": "Emits one viewport-covering triangle in a known colour, as the geometry source for the synthetic raw-raster cases. Fixed VERTEX_COUNT so a consumer's draw count is predictable: three vertices, solid green.",
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
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
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

    // Covers the whole viewport from three vertices.
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(3.0, -1.0);
    if(idx == 2u) p = vec2(-1.0, 3.0);

    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    geo_color_out[idx] = vec4(0.0, 1.0, 0.0, 1.0);
}
