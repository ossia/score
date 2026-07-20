/*{
  "DESCRIPTION": "Storage-visibility binding-drift guard. Declares a storage_input `pad` with VISIBILITY:\"all\" FIRST, then a PERSISTENT read_write storage `counter`. libisf's GLSL codegen only emits (and only advances its binding counter for) the graphics-visibility set {fragment,vertex,vertex+fragment,both,graphics} — it SKIPS `pad` (\"all\"). The runtime SRB assignment must agree; the pre-fix code let \"all\" consume a graphics binding, so `counter` (and its `_prev` twin) drifted one slot away from the layout(binding=...) the shader was compiled with, breaking the persistent ping-pong: `counter` writes go to `pad`'s buffer and `counter_prev` reads a buffer the shader never wrote, so the accumulator is stuck. `pad` is intentionally never referenced in GLSL (the codegen doesn't declare it). Correct engine: `counter` advances +1 per frame. Wire: this -> Window.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BINDING", "TEST-PERSISTENT"],
  "INPUTS": [
    {
      "NAME": "pad",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "VISIBILITY": "all",
      "LAYOUT": [
        { "NAME": "junk", "TYPE": "float" }
      ]
    },
    {
      "NAME": "counter",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "PERSISTENT": true,
      "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "value", "TYPE": "float" }
      ]
    }
  ]
}*/

void main()
{
    // Only `counter` / `counter_prev` are referenced — `pad` is skipped by the
    // codegen (VISIBILITY "all" is not a graphics visibility) so it is not even
    // declared in GLSL; it exists purely to (wrongly, pre-fix) consume a binding.
    float next = counter_prev.value + 1.0;
    counter.value = next;

    float step = mod(next, 16.0);
    float g = step * 16.0 / 255.0;
    gl_FragColor = vec4(g, g, g, 1.0);
}
