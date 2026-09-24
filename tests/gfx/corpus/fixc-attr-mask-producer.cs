/*{
  "DESCRIPTION": "Writes 1.0 into out_mask, declared with the non-standard SEMANTIC face_mask. Paired with fixc-attr-mask-consumer.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "4",
      "ATTRIBUTES": [ { "NAME": "out_mask", "SEMANTIC": "face_mask", "TYPE": "float", "ACCESS": "write_only" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } } ]
}*/
void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(ISF_WRITE(geo, out_mask).length())) return;
    ISF_WRITE(geo, out_mask)[i] = 1.0;
}
