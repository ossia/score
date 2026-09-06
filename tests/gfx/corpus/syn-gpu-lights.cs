/*{
  "DESCRIPTION": "GPU-driven dynamic lights (stage-2 exemplar, test copy). A pool of 16 light slots on a 4x4 grid of the 64x64 frame. Pass 0 (one thread, the 'light manager') reads the 'alive' and 'tick' controls and publishes three things the CPU never sees: the indirect DISPATCH arguments {alive,1,1} for pass 1, the DRAW COUNT (geo_indirect_count = alive), and the dead-slot filler for all 16 command slots. Pass 1 is dispatched INDIRECTLY from those arguments -- one workgroup per alive light -- and writes each light's screen quad (cell (tick+g) mod 16, an 8x8 px quad centered in its 16x16 px cell; every coordinate a power of two, exact in binary float) plus its draw command. The engine consumes the commands with drawIndirectCount: count read from the GPU-written buffer, clamped to the 16-slot capacity. Nothing on the CPU knows how many lights are alive or where they are. The 'poison' control makes dead slots draw sentinel-blue quads at their own cell: a backend that ignores the GPU count and draws the full capacity paints blue, so the consuming test can PROVE the count was honored rather than silently falling back. Real light at cell c reads back {255, 16*c, 0, 255}; poison reads {0, 0, 255, 255}.",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INDIRECT", "PARTICLES"],
  "INPUTS": [
    { "NAME": "alive",  "TYPE": "long", "DEFAULT": 5, "MIN": 0, "MAX": 16 },
    { "NAME": "tick",   "TYPE": "long", "DEFAULT": 0, "MIN": 0, "MAX": 64 },
    { "NAME": "poison", "TYPE": "long", "DEFAULT": 0, "MIN": 0, "MAX": 1 }
  ],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "192",
      "INDIRECT": { "COUNT": 16, "DRAW_COUNT": true },
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
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL",   "WORKGROUPS": [1, 1, 1] } },
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "INDIRECT", "TARGET": "args", "WORKGROUPS": [16, 1, 1] } }
  ]
}*/

// Cell c of the 4x4 grid: an 8x8 px quad centered in the 16x16 px cell.
// x0 = -1 + (c%4) * 0.5, quad x in [x0 + 0.125, x0 + 0.375]; same on y with
// c/4. 0.5, 0.125, 0.375 are exact powers-of-two sums: no rounding anywhere.
void write_quad(uint firstVertex, uint cell, vec4 col)
{
  float x0 = -1.0 + float(cell % 4u) * 0.5 + 0.125;
  float y0 = -1.0 + float(cell / 4u) * 0.5 + 0.125;
  float x1 = x0 + 0.25;
  float y1 = y0 + 0.25;
  geo_position_out[firstVertex + 0u] = vec4(x0, y0, 0.0, 1.0);
  geo_position_out[firstVertex + 1u] = vec4(x1, y0, 0.0, 1.0);
  geo_position_out[firstVertex + 2u] = vec4(x1, y1, 0.0, 1.0);
  geo_position_out[firstVertex + 3u] = vec4(x0, y0, 0.0, 1.0);
  geo_position_out[firstVertex + 4u] = vec4(x1, y1, 0.0, 1.0);
  geo_position_out[firstVertex + 5u] = vec4(x0, y1, 0.0, 1.0);
  for(uint v = 0u; v < 6u; v++)
    geo_color_out[firstVertex + v] = col;
}

void main()
{
  if(PASSINDEX == 0)
  {
    // The light manager: one thread decides everything the GPU pipeline
    // needs this frame. The CPU-side geometry stays a constant 192 vertices
    // and 16 command slots.
    if(gl_GlobalInvocationID.x != 0u)
      return;
    uint n = uint(clamp(alive, 0, 16));

    // 1. How many workgroups pass 1 runs: EXECUTION_MODEL INDIRECT reads
    //    this. On backends without dispatchIndirect the engine dispatches
    //    the declared WORKGROUPS ceiling instead and pass 1 self-bounds.
    args.xyz[0] = n;
    args.xyz[1] = 1u;
    args.xyz[2] = 1u;
    args.xyz[3] = 0u;

    // 2. How many commands the draw consumes: INDIRECT.DRAW_COUNT exposes
    //    this u32; drawIndirectCount clamps it to the 16-slot capacity.
    geo_indirect_count = n;

    // 3. Dead-slot filler, slots [n, 16). Zeroed by default -- the contract
    //    that keeps the capacity-draw fallback rungs pixel-identical. With
    //    'poison' set (test use), a dead slot draws its sentinel quad
    //    (vertices [96+6j, 96+6j+6), written below): visible if and only if
    //    something draws past the GPU count.
    bool p = (poison != 0);
    for(uint j = 0u; j < 16u; j++)
    {
      if(j >= n)
      {
        geo_indirect[j].vertexCount   = p ? 6u : 0u;
        geo_indirect[j].instanceCount = p ? 1u : 0u;
        geo_indirect[j].firstVertex   = p ? 96u + 6u * j : 0u;
        geo_indirect[j].baseVertex    = 0;
        geo_indirect[j].firstInstance = 0u;
      }
      write_quad(96u + 6u * j, j, vec4(0.0, 0.0, 1.0, 1.0));
    }
    return;
  }

  // Pass 1, one workgroup per ALIVE light -- the workgroup count came from
  // args.xyz, written on the GPU one dispatch earlier. Under the CPU ceiling
  // fallback all 16 run; the self-bound below makes both paths identical.
  uint g = gl_WorkGroupID.x;
  if(g >= args.xyz[0])
    return;

  // The dynamic part: which cell this light occupies moves with 'tick', and
  // its color encodes the CELL (G = 16*cell when read back as RGBA8), so a
  // consumer can assert both placement and identity exactly.
  uint cell = (uint(tick) + g) % 16u;
  write_quad(6u * g, cell, vec4(1.0, float(cell) * 16.0 / 255.0, 0.0, 1.0));

  geo_indirect[g].vertexCount   = 6u;
  geo_indirect[g].instanceCount = 1u;
  geo_indirect[g].firstVertex   = 6u * g;
  geo_indirect[g].baseVertex    = 0;
  geo_indirect[g].firstInstance = 0u;
}
