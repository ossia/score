/*{
  "DESCRIPTION": "A uniform_input whose std140 block is larger than 256 bytes (4 mat4 = 256 B, plus a trailing vec4 = 272 B). Left DISCONNECTED on purpose: the node has to allocate its own placeholder UBO, and that placeholder used to be a hardcoded 256 B regardless of what the shader declared. The fragment shader reads `big.tail`, which lives at offset 256 — i.e. entirely past the end of the old placeholder.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BINDING"],
  "INPUTS": [
    { "NAME": "big", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "m0",   "TYPE": "mat4" },
        { "NAME": "m1",   "TYPE": "mat4" },
        { "NAME": "m2",   "TYPE": "mat4" },
        { "NAME": "m3",   "TYPE": "mat4" },
        { "NAME": "tail", "TYPE": "vec4" }
      ]
    }
  ]
}*/

void main()
{
    // `tail` is at byte offset 256: it is only readable if the bound buffer
    // was sized from the LAYOUT rather than from a fixed guess. Disconnected,
    // the placeholder is zero-filled, so this is a deterministic 0.
    float v = big.tail.x + big.tail.w;
    isf_FragColor = vec4(v, 1.0 - v, 0.25, 1.0);
}
