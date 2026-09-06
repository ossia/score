/*{
  "DESCRIPTION": "GPU-written indirect draw count. The geometry resource declares a FIXED 'INSTANCE_COUNT': '8' -- the CPU-side geometry_spec always says 8 instances -- plus 'INDIRECT': { 'COUNT': 1 }, which makes the engine allocate a zero-initialized 5-word indirect command SSBO (RenderedCSFNode.cpp:3964-3988) and expose it to this compute source as geo_indirect[] (libisf emits struct DrawIndirectCommand { uint vertexCount; uint instanceCount; uint firstVertex; uint firstInstance; int baseVertex; } for a non-indexed geometry, so words 0..3 are a native QRhiDrawIndirectCommand). The PER_VERTEX pass writes one quad covering NDC x in [-1, -0.875] (pixel columns [0,4) at width 64) and, from invocation 0, writes the SINGLE indirect command with instanceCount = clamp(count, 0, 8) taken from the 'count' long control. The PER_INSTANCE pass writes a translation for ALL 8 instances (x offset i * 0.125 -> instance i owns pixel columns [4i, 4i+4); translation.w = i/255 carries the identity). Because the CPU-side instance count never moves, the ONLY way the drawn strip count can follow the control is through the GPU-written indirect command consumed by drawIndirect / the buffer-readback CPU fallback. baseVertex and firstInstance are both written 0, so this fixture is insensitive to which of the two word orders the producer uses; DrawDispatch-1 is the fixture that pins the non-indexed order.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INSTANCING", "TEST-INDIRECT"],
  "INPUTS": [
    { "NAME": "count", "TYPE": "long", "DEFAULT": 3, "MIN": 0, "MAX": 8 }
  ],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "INSTANCE_COUNT": "8",
      "INDIRECT": { "COUNT": 1 },
      "ATTRIBUTES": [
        { "NAME": "position",    "SEMANTIC": "position",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [6, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX",   "TARGET": "geo" } },
    { "LOCAL_SIZE": [8, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geo" } }
  ]
}*/

void main()
{
  if(PASSINDEX == 0)
  {
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 6u)
      return;

    // One quad: NDC x in [-1.0, -0.875] (exactly one 1/16th column of the
    // frame; 0.125 and -0.875 are exact in binary floating point), y in
    // [-1, 1] so the strip spans the full height. Same layout as
    // syn-instance-count-user.cs; the CSF output geometry declares cull_mode
    // none, so winding is not load-bearing.
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(-0.875, -1.0);
    if(idx == 2u) p = vec2(-0.875,  1.0);
    if(idx == 3u) p = vec2(-1.0,   -1.0);
    if(idx == 4u) p = vec2(-0.875,  1.0);
    if(idx == 5u) p = vec2(-1.0,    1.0);

    geo_position_out[idx] = vec4(p, 0.0, 1.0);

    // The point of this shader: the indirect draw command is computed at
    // runtime, on the GPU, from the 'count' control. The CPU-side
    // geometry_spec keeps saying instances = 8; only this write can make the
    // drawn count differ from 8. Re-written every frame (the pass re-runs
    // per frame), so moving the control between renders moves the count.
    //
    // baseVertex and firstInstance are both 0 on purpose: this fixture is
    // about the instance count, so it stays insensitive to where in the
    // 20-byte record firstInstance lives. The non-indexed record puts it in
    // word 3 (words 0..3 are a native QRhiDrawIndirectCommand) and the CPU
    // readback fallback reads the same word; DrawDispatch-1 is the fixture
    // that pins that order with a non-zero firstInstance.
    if(idx == 0u)
    {
      uint n = uint(clamp(count, 0, 8));
      geo_indirect[0].vertexCount   = 6u;
      geo_indirect[0].instanceCount = n;
      geo_indirect[0].firstVertex   = 0u;
      geo_indirect[0].baseVertex    = 0;
      geo_indirect[0].firstInstance = 0u;
    }
    return;
  }

  // PER_INSTANCE: one invocation per CPU-side instance (dispatch sized from
  // the binding's instance_count == 8, RenderedCSFNode.cpp runInitialPasses).
  // All 8 translations are written every frame, whatever 'count' says: the
  // instance-rate vertex buffer must stay fully populated so that any drawn
  // instance i -- however many the indirect command asks for -- reads a
  // well-defined translation. Guard with the literal 8 (INSTANCE_COUNT is
  // fixed, no $USER, so no synthesized geo_instance_count uniform is relied
  // upon here).
  uint idx = gl_GlobalInvocationID.x;
  if(idx >= 8u)
    return;

  // x offset i * 0.125 puts instance i in NDC column [-1 + 0.125 i, -1 + 0.125 (i+1)]
  // == pixel columns [4i, 4i+4) at width 64. Exact: 0.125 = 2^-3.
  // translation.w carries the instance identity (i/255) for the G channel of
  // syn-instance-index-color.fs.
  geo_translation_out[idx]
      = vec4(float(idx) * 0.125, 0.0, 0.0, float(idx) / 255.0);
}
