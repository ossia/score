/*{
  "DESCRIPTION": "COPY_FROM with ACCESS read_only: geoOut.color is declared COPY_FROM geoIn.color, so the runtime forwards the upstream colour buffer without this shader ever writing it. Positions are copied through by hand. Drawn with raw-raster-basic, the triangle is the producer's green when forwarding works and black when it does not.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-COPY-FROM"],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "out_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "out_color", "SEMANTIC": "color",   "TYPE": "vec4", "ACCESS": "read_only",
          "COPY_FROM": { "GEOMETRY": "geoIn", "ATTRIBUTE": "in_color" } }
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
}
