/*{
  "DESCRIPTION": "Four points with positions only. Paired with csf-default-probe.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "4", "TOPOLOGY": "points",
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_WRITE(geo, position).length())) return;
    ISF_WRITE(geo, position)[i] = vec4(float(i), 0, 0, 1);
}
