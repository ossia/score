/*{
  "DESCRIPTION": "Declares its own attribute bar and an auxiliary buffer on geoOut, and forwards the upstream attribute foo through COPY_FROM.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geoOut", "TYPE": "geometry", "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "bar", "SEMANTIC": "bar", "TYPE": "uint", "ACCESS": "write_only" },
        { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "COPY_FROM": { "GEOMETRY": "geoIn", "ATTRIBUTE": "foo" } }
      ],
      "AUXILIARY": [ { "NAME": "scratch", "ACCESS": "read_write", "SIZE": "$VERTEX_COUNT_geoIn * 4", "LAYOUT": [ { "NAME": "values", "TYPE": "uint[]" } ] } ]
    },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "foo", "TYPE": "float", "ACCESS": "read_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geoIn" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_WRITE(geoOut, bar).length())) return;
    ISF_WRITE(geoOut, bar)[i] = 7u;
    scratch.values[i] = 5u;
}
