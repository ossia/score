/*{
  "DESCRIPTION": "User-driven instance count: geometry with VERTEX_COUNT 6 (one quad = two triangles) and INSTANCE_COUNT taken verbatim from a $USER control port. The PER_VERTEX pass emits a quad covering NDC x in [-1, -0.875] (one 1/16th column) and the full y range; the PER_INSTANCE pass writes a per-instance translation that places instance i in column i (x offset i * 0.125) and stamps the instance's identity into translation.w as i/255. Rendered through a raw-raster consumer, instance i therefore owns pixel columns [4i, 4i+4) of a 64-wide frame, carrying its identity in closed form. Moving the count control mid-session must add/remove exactly the right columns (RenderedCSFNode.cpp resolveCountExpression re-reads the port every updateGeometryBindings; prev_instance_count != instance_count triggers the geometry rebuild).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-INSTANCING"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "INSTANCE_COUNT": "$USER",
      "ATTRIBUTES": [
        { "NAME": "position",    "SEMANTIC": "position",    "TYPE": "vec4", "ACCESS": "write_only", "RATE": "vertex" },
        { "NAME": "translation", "SEMANTIC": "translation", "TYPE": "vec4", "ACCESS": "write_only", "RATE": "instance" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [6, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX",   "TARGET": "geo" } },
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_INSTANCE", "TARGET": "geo" } }
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
    // [-1, 1] so the strip spans the full height. Two CCW triangles; the
    // CSF output geometry declares cull_mode none, so winding is not
    // load-bearing.
    vec2 p = vec2(-1.0, -1.0);
    if(idx == 1u) p = vec2(-0.875, -1.0);
    if(idx == 2u) p = vec2(-0.875,  1.0);
    if(idx == 3u) p = vec2(-1.0,   -1.0);
    if(idx == 4u) p = vec2(-0.875,  1.0);
    if(idx == 5u) p = vec2(-1.0,    1.0);

    geo_position_out[idx] = vec4(p, 0.0, 1.0);
    return;
  }

  // PER_INSTANCE: one invocation per instance (the dispatch is sized from the
  // binding's resolved instance_count, RenderedCSFNode.cpp runInitialPasses).
  // geo_instance_count is the synthesized int uniform the $USER spec creates
  // (libisf isf.cpp emit_synth_int "<name>_instance_count"); guard on it so a
  // rounded-up dispatch never writes past the buffer sized for instance_count.
  uint idx = gl_GlobalInvocationID.x;
  if(idx >= uint(geo_instance_count))
    return;

  // x offset i * 0.125 puts instance i in NDC column [-1 + 0.125 i, -1 + 0.125 (i+1)]
  // == pixel columns [4i, 4i+4) at width 64. Exact: 0.125 = 2^-3.
  // translation.w carries the instance identity AS STORED IN THIS BUFFER
  // (i / 255): if a reallocation leaves stale contents, the drawn identity
  // disagrees with the draw-call identity (gl_InstanceIndex) downstream.
  geo_translation_out[idx]
      = vec4(float(idx) * 0.125, 0.0, 0.0, float(idx) / 255.0);
}
