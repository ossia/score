/*{
  "DESCRIPTION": "GPU-only particle chain, sprites. Reads the pool and owns 8 quads (48 vertices) drawn through GPU-written commands and a GPU-written draw count. Pass 0 sets the draw count to the live slot count and writes one command per live slot; dead slots get a poison command that draws a blue quad on columns 32..63, so a draw that ignores the GPU count shows blue. Pass 1 writes the quads: slot q is a full-height strip on pixel columns [4q, 4q+4) of a 64x64 frame, coloured R=1, G=brightness.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY", "TEST-INDIRECT"],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "96",
      "INDIRECT": { "COUNT": 8, "DRAW_COUNT": true },
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" }
      ]
    },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "state", "SEMANTIC": "pop_state", "TYPE": "vec4", "ACCESS": "read_only" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1],  "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [96, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geoOut" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    if(gl_GlobalInvocationID.x != 0u)
      return;
    uint n = 0u;
    for(uint i = 0u; i < 8u; i++)
      if(ISF_READ(geoIn, state)[i].x > 0.5)
        n++;
    geoOut_indirect_count = n;
    for(uint i = 0u; i < 8u; i++)
    {
      bool live = i < n;
      geoOut_indirect[i].vertexCount   = 6u;
      geoOut_indirect[i].instanceCount = 1u;
      geoOut_indirect[i].firstVertex   = live ? 6u * i : 6u * (8u + i);
      geoOut_indirect[i].baseVertex    = 0;
      geoOut_indirect[i].firstInstance = 0u;
    }
    return;
  }

  uint idx = gl_GlobalInvocationID.x;
  if(idx >= 96u)
    return;
  uint q = idx / 6u;
  uint c = idx % 6u;
  float x0 = -1.0 + float(q) * 0.125;
  float x1 = x0 + 0.125;
  vec2 pos = vec2(x0, -1.0);
  if(c == 1u) pos = vec2(x1, -1.0);
  if(c == 2u) pos = vec2(x1,  1.0);
  if(c == 3u) pos = vec2(x0, -1.0);
  if(c == 4u) pos = vec2(x1,  1.0);
  if(c == 5u) pos = vec2(x0,  1.0);
  ISF_WRITE(geoOut, position)[idx] = vec4(pos, 0.0, 1.0);
  ISF_WRITE(geoOut, color)[idx] = (q < 8u)
      ? vec4(1.0, ISF_READ(geoIn, state)[q].y, 0.0, 1.0)
      : vec4(0.0, 0.0, 1.0, 1.0);
}
