/*{
  "DESCRIPTION": "Adds an attribute the upstream geometry does not have: geoIn carries position only, geoOut declares position and colour, and the shader computes the colour. Drawn with raw-raster-basic the triangle is blue, a value that exists nowhere upstream, so a pass-through of the input cannot produce it.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-GEOMETRY-ADD"],
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
        { "NAME": "in_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" }
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
    ISF_WRITE(geoOut, out_color)[idx] = vec4(0.0, 0.0, 1.0, 1.0);
}
