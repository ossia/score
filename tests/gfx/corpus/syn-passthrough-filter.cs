/*{
  "DESCRIPTION": "Geometry filter that copies position and colour across by hand. Control for the COPY_FROM case: identical topology and identical consumer, with the forwarding done explicitly in the shader instead of declared.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC"],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_only" }
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
    uint count = uint(ISF_READ(geoIn, position).length());
    if(idx >= count)
        return;

    ISF_WRITE(geoOut, position)[idx] = ISF_READ(geoIn, position)[idx];
    ISF_WRITE(geoOut, color)[idx] = ISF_READ(geoIn, color)[idx];
}
