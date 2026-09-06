/*{
  "DESCRIPTION": "DrawDispatch-1: non-indexed five-word command with distinct baseVertex and firstInstance. Expected exactly instance-buffer strips 3 and 4, not 0 and 1.",
  "ISFVSN": "2.0", "MODE": "COMPUTE_SHADER",
  "RESOURCES": [{
    "NAME": "geo", "TYPE": "geometry", "VERTEX_COUNT": "6", "INSTANCE_COUNT": "8",
    "INDIRECT": { "COUNT": 1 },
    "ATTRIBUTES": [
      { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
      { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
    ]
  }],
  "PASSES": [
    { "LOCAL_SIZE": [6,1,1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } },
    { "LOCAL_SIZE": [8,1,1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geo" } }
  ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(PASSINDEX == 0)
  {
    if(i >= 6u) return;
    vec2 p = vec2(-1.0, -1.0);
    if(i == 1u) p = vec2(-0.875, -1.0);
    if(i == 2u || i == 4u) p = vec2(-0.875, 1.0);
    if(i == 5u) p = vec2(-1.0, 1.0);
    geo_position_out[i] = vec4(p, 0.0, 1.0);
    if(i == 0u)
    {
      geo_indirect[0].vertexCount = 6u;
      geo_indirect[0].instanceCount = 2u;
      geo_indirect[0].firstVertex = 0u;
      geo_indirect[0].baseVertex = 3;
      geo_indirect[0].firstInstance = 3u;
    }
  }
  else if(i < 8u)
    geo_translation_out[i] = vec4(float(i) * 0.125, 0.0, 0.0, float(i) / 255.0);
}
