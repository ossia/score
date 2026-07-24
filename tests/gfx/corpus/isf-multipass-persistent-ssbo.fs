/*{
  "DESCRIPTION": "Tests the Step 6 persistent-SSBO ping-pong across a multi-pass ISF. Pass 0 increments a 1-element counter: counter.v = counter_prev.v + 1u. Pass 1 reads counter.v (this frame's write) and renders red = counter.v % 256 / 255 — so red slowly climbs from black to bright, wraps, and restarts. Validates that (a) collectGraphicsStorageResources assigns the correct two binding slots for current+prev, (b) the SRBs of both inner passes AND their altPasses get the same storage bindings, (c) swapPersistentSSBOsState runs exactly once per frame even with multi-pass. Wire: this → Window. Expected: smooth ramp of red over time. If swap broken: red stays at 0 (prev never updates) or flickers (double-swap). If binding broken: GPU validation error referencing slot N.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS", "TEST-PERSISTENT", "TEST-BINDING"],
  "INPUTS": [
    { "NAME": "counter", "TYPE": "storage", "ACCESS": "read_write", "PERSISTENT": true,
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "v", "TYPE": "uint" } ]
    }
  ],
  "PASSES": [
    { "TARGET": "pass0" },
    {}
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    if(PASSINDEX == 0)
    {
        // Exactly one invocation updates the counter.
        if(gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0)
            counter.v = counter_prev.v + 1u;
        // Pass 0's color target isn't sampled; fill it for RenderDoc clarity.
        isf_FragColor = vec4(uv, 0.2, 1.0);
    }
    else
    {
        float r = float(counter.v % 256u) / 255.0;
        isf_FragColor = vec4(r, uv.y * 0.3, uv.x * 0.3, 1.0);
    }
}
