/*{
  "DESCRIPTION": "Four points whose weight is 0 and whose offset is (0.1, 0.2, 0.3). Paired with csf-default-probe.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "4", "TOPOLOGY": "points",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "weight", "TYPE": "float", "ACCESS": "write_only" },
        { "NAME": "offset", "TYPE": "vec3", "ACCESS": "write_only" }
      ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_WRITE(geo, position).length())) return;
    ISF_WRITE(geo, position)[i] = vec4(float(i), 0, 0, 1);
    ISF_WRITE(geo, weight)[i] = 0.0;
    ISF_WRITE(geo, offset)[i] = vec3(0.1, 0.2, 0.3);
}
