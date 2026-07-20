/*{
  "DESCRIPTION": "Tests MRT combined with a PERSISTENT storage_input — the Simple-path storage ping-pong happens in runInitialPasses (line 863) for MRT nodes, which is a different code site from the non-MRT ping-pong in runRenderPass (line 934). A 1-element counter increments once per frame; both color attachments sample it so you can verify both MRT slots see the swapped buffer. Wire: this → Window (outA); the outB port can also be wired to a second inspector. Expected: both outputs show a slowly rising red ramp from counter.v. If MRT storage broken: only outA has the ramp; outB is blank / stale. If swap broken: ramp flatlines (counter never visibly advances) or flickers (double swap).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MRT", "TEST-PERSISTENT", "TEST-BINDING"],
  "INPUTS": [
    { "NAME": "counter", "TYPE": "storage", "ACCESS": "read_write", "PERSISTENT": true,
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "v", "TYPE": "uint" } ]
    }
  ],
  "OUTPUTS": [
    { "NAME": "outA", "TYPE": "color" },
    { "NAME": "outB", "TYPE": "color" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    // Exactly one invocation increments the counter.
    if(gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0)
        counter.v = counter_prev.v + 1u;

    float ramp = float(counter.v % 256u) / 255.0;

    // Both MRT attachments read the same counter — if either is blank the
    // storage binding didn't reach that attachment's SRB.
    outA = vec4(ramp, uv.y * 0.4, 0.1, 1.0);
    outB = vec4(0.1, uv.x * 0.4, ramp, 1.0);
}
