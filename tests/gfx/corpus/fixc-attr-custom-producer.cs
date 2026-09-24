/*{
  "DESCRIPTION": "Writes 1.0 into attribute foo declared with SEMANTIC custom (match by NAME). Paired with fixc-attr-foo-consumer.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "4",
      "ATTRIBUTES": [ { "NAME": "foo", "SEMANTIC": "custom", "TYPE": "float", "ACCESS": "write_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_WRITE(geo, foo).length())) return;
    ISF_WRITE(geo, foo)[i] = 1.0;
}
