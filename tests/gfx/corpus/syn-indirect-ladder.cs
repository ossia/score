/*{
  "DESCRIPTION": "Fallback-ladder fixture for the GPU-decided draw count (Qt 6.13 drawIndirectCount) and indirect dispatch (Qt 6.13 dispatchIndirect). The geometry declares 16 quads worth of vertices (96): quads 0..7 are REAL strips on pixel columns [4q, 4q+4) of a 64x64 frame (color R=1, G=q/255), quads 8..15 are POISON strips on columns [32+4(q-8), ...) (color B=1). Pass 0 (MANUAL 1 workgroup) writes the dispatch-args buffer {n,1,1}, the GPU draw count n, and fills ALL 8 command slots with dead-slot filler: zeros when 'poison' is 0 (the producer contract every capacity-draw fallback rung relies on), or a poison-quad draw when 'poison' is 1 (so a rung that ignores the GPU count and draws the full capacity paints sentinel blue). Pass 1 (EXECUTION_MODEL INDIRECT on 'args', CPU ceiling WORKGROUPS [8,1,1]) overwrites slots [0,n) with the real strip draws -- one workgroup per slot, self-bounded by reading args so the GPU path and the ceiling fallback do identical work. Pass 2 (PER_VERTEX) writes all 96 positions+colors every frame. Only the command/count machinery decides what is visible: a draw that bypasses the indirect buffer entirely would paint all 16 quads.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INDIRECT", "TEST-EXECUTION-MODEL"],
  "INPUTS": [
    { "NAME": "count",  "TYPE": "long", "DEFAULT": 3, "MIN": 0, "MAX": 8 },
    { "NAME": "poison", "TYPE": "long", "DEFAULT": 0, "MIN": 0, "MAX": 1 }
  ],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "96",
      "INDIRECT": { "COUNT": 8, "DRAW_COUNT": true },
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

    geo_indirect_count = n;

    bool p = (poison != 0);
    for(uint i = 0u; i < 8u; i++)
    {
      // Dead filler. poison=0: all-zero command (draws nothing) -- the
      // producer contract the capacity-draw rungs rely on. poison=1: a
      // 6-vertex draw of POISON quad i (vertices [6*(8+i), 6*(8+i)+6)),
      // visible only if a rung draws past the GPU count.
      geo_indirect[i].vertexCount   = p ? 6u : 0u;
      geo_indirect[i].instanceCount = p ? 1u : 0u;
      geo_indirect[i].firstVertex   = p ? 6u * (8u + i) : 0u;
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
