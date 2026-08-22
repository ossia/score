/*{
  "DESCRIPTION": "Tests multi-pass ISF with a non-persistent read_write storage_input. Pass 0 writes the current TIME into a 1-element SSBO (only the fragment at (0,0) actually writes, the others just render an RGB pattern). Pass 1 reads the SSBO and drives red from fract(timebuf.time * 0.5). Validates that the SSBO binding reaches BOTH passes' SRBs and that a same-frame write→read inside a single multi-pass node works. No upstream needed (storage_input non-persistent owns its buffer). Wire: this → Window. Expected: red slowly cycles 0→1→0; blue + green patterns. If broken: red stays at 0 (pass 1's SRB didn't get the SSBO binding) or GPU validation failure.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MULTIPASS", "TEST-BINDING"],
  "INPUTS": [
    { "NAME": "timebuf", "TYPE": "storage", "ACCESS": "read_write",
      "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "time", "TYPE": "float" } ]
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
        if(gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0)
            timebuf.time = TIME;
        // Pass 0 output is never sampled downstream, but emit something
        // sensible for RenderDoc debugging.
        isf_FragColor = vec4(uv, 0.0, 1.0);
    }
    else
    {
        float r = fract(timebuf.time * 0.5);
        isf_FragColor = vec4(r, uv.y * 0.4, uv.x * 0.4, 1.0);
    }
}
