/*{
  "DESCRIPTION": "Persistent-SSBO frame counter. A single read_write PERSISTENT storage buffer holds one float; every frame the shader reads the previous frame's value and writes previous+1, so the stored value advances by EXACTLY ONE per rendered frame. The current count (mod 16, scaled x16) is encoded as a grey level in the output so a readback can measure the per-frame advance. Used to regression-test the ISF persistent-SSBO ping-pong once-per-frame guard (score commit 980fc95b6 / R3-N1): before the fix, a persistent node with >=2 outgoing edges swapped its ping-pong buffers once PER EDGE, advancing the simulation N times per frame. With the guard, the grey level must increase by one step (16/255) per frame regardless of the number of output edges.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "INPUTS": [
    {
      "NAME": "counter",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "PERSISTENT": true,
      "LAYOUT": [
        { "NAME": "value", "TYPE": "float" }
      ]
    }
  ]
}*/

void main()
{
    // counter_prev is the previous frame's (read-only) buffer; counter is the
    // current (read-write) one. Every fragment reads the same prev value and
    // writes the same prev+1, so the result is race-free and deterministic.
    float next = counter_prev.value + 1.0;
    counter.value = next;

    // Encode (count mod 16) * 16 as a grey level: 16 grey units per frame,
    // wrapping every 16 frames. A once-per-frame advance => +16/255 per frame;
    // a double-swap (the bug) => +32/255 per frame — clearly separable at 8-bit.
    float step = mod(next, 16.0);
    float g = step * 16.0 / 255.0;
    gl_FragColor = vec4(g, g, g, 1.0);
}
