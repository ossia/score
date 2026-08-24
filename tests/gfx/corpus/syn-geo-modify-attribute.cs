/*{
  "DESCRIPTION": "Modifies an attribute in flight: the upstream triangle is green, and this rewrites the colour to red. Drawn with raw-raster-basic the triangle must be red, so a filter that silently forwarded the upstream buffer instead of writing its own is visible as green.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-GEOMETRY-MODIFY"],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "out_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "out_color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "in_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "in_color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_only" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    uint count = uint(ISF_READ(geoIn, in_position).length());
    if(idx >= count)
        return;

    ISF_WRITE(geoOut, out_position)[idx] = ISF_READ(geoIn, in_position)[idx];

    // Swap the upstream green for red: reading the input proves the read path
    // works, and the written value proves the write beat the forward.
    vec4 src = ISF_READ(geoIn, in_color)[idx];
    ISF_WRITE(geoOut, out_color)[idx] = vec4(src.g, 0.0, 0.0, 1.0);
}
