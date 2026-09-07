/*{
  "DESCRIPTION": "Zero-count-slot fixture: the SAME 16-quad geometry as syn-indirect-ladder.cs, but the geometry resource declares NO DRAW_COUNT, so no \"_indirect_draw_count\" auxiliary is published and the CPU-readback rung (R4) has nothing to clamp with. Pass 0 zeroes all 8 command slots -- the producer contract the capacity rungs rely on -- and pass 1 overwrites slots [0, count) with the real strip draws. The dead slots therefore survive readback as all-zero commands and reach the draw loop. On the indirect rungs the GPU discards them; the CPU rung would issue drawIndexed(0), which Metal's API validation aborts on. Real strip q occupies pixel columns [4q, 4q+4) reading back {255, q, 0, 255}; nothing else may paint.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INDIRECT", "TEST-EXECUTION-MODEL"],
  "INPUTS": [
    { "NAME": "count",  "TYPE": "long", "DEFAULT": 3, "MIN": 0, "MAX": 8 }
  ],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "96",
      "INDIRECT": { "COUNT": 8 },
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" }
      ]
    },
    {
      "NAME": "args",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "BUFFER_USAGE": "dispatch_args",
      "LAYOUT": [ { "NAME": "xyz", "TYPE": "uint[4]" } ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1],  "EXECUTION_MODEL": { "TYPE": "MANUAL",   "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [1, 1, 1],  "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [8, 1, 1] } },
    { "LOCAL_SIZE": [96, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    // One invocation. Publish the indirect dispatch arguments (n workgroups
    // for pass 1), the GPU-side draw count, and the dead-slot filler for all
    // 8 command slots -- pass 1 then overwrites the live ones.
    if(gl_GlobalInvocationID.x != 0u)
      return;
    uint n = uint(clamp(count, 0, 8));
    args.xyz[0] = n;
    args.xyz[1] = 1u;
    args.xyz[2] = 1u;
    args.xyz[3] = 0u;

    // No geo_indirect_count here: this fixture publishes no DRAW_COUNT, so
    // the CPU rung cannot clamp and must survive the zeroed dead slots.
    for(uint i = 0u; i < 8u; i++)
    {
      geo_indirect[i].vertexCount   = 0u;
      geo_indirect[i].instanceCount = 0u;
      geo_indirect[i].firstVertex   = 0u;
      geo_indirect[i].baseVertex    = 0;
      geo_indirect[i].firstInstance = 0u;
    }
    return;
  }

  if(PASSINDEX == 1)
  {
    // Indirectly dispatched: n workgroups on the GPU path. On the CPU
    // ceiling fallback all 8 run, so self-bound by re-reading args -- both
    // paths then write exactly the same [0, n) slots.
    uint g = gl_WorkGroupID.x;
    if(g >= args.xyz[0])
      return;
    geo_indirect[g].vertexCount   = 6u;
    geo_indirect[g].instanceCount = 1u;
    geo_indirect[g].firstVertex   = 6u * g;
    geo_indirect[g].baseVertex    = 0;
    geo_indirect[g].firstInstance = 0u;
    return;
  }

  // PER_VERTEX: all 96 vertices written every frame, visibility is decided
  // solely by the commands. Quad q occupies NDC x [x0, x0 + 0.125]
  // (exact: 0.125 = 2^-3), full height.
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
  geo_position_out[idx] = vec4(pos, 0.0, 1.0);
  // Real strips: R marker + G identity. Poison strips: pure blue sentinel.
  geo_color_out[idx] = (q < 8u)
      ? vec4(1.0, float(q) / 255.0, 0.0, 1.0)
      : vec4(0.0, 0.0, 1.0, 1.0);
}
