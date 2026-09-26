/*{
  "DESCRIPTION": "Filter on an upstream geometry: keeps its positions, paints it yellow, and declares TOPOLOGY triangles, which replaces the topology a filter otherwise inherits from its upstream.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-GEOMETRY"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "TOPOLOGY": "triangles",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
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
    geo_color_out[idx] = vec4(1.0, 1.0, 0.0, 1.0);
}
