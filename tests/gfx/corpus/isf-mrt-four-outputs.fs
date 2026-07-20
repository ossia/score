/*{
  "DESCRIPTION": "Tests ISF OUTPUTS with 4 simultaneous color outputs (maximum typical MRT count). Each output gets a different solid-ish pattern. If all 4 MRT slots work: 4 distinct output ports, each with unique content. If MRT count limited: later outputs are black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MRT"],
  "INPUTS": [],
  "OUTPUTS": [
    { "NAME": "out0" },
    { "NAME": "out1" },
    { "NAME": "out2" },
    { "NAME": "out3" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    // Output 0: red-green gradient
    out0 = vec4(uv.x, uv.y, 0.0, 1.0);

    // Output 1: blue-cyan gradient
    out1 = vec4(0.0, uv.y, uv.x, 1.0);

    // Output 2: concentric rings
    float d = length(uv - 0.5) * 10.0;
    float rings = 0.5 + 0.5 * sin(d);
    out2 = vec4(rings, rings * 0.5, 0.0, 1.0);

    // Output 3: diagonal stripes
    float stripes = 0.5 + 0.5 * sin((uv.x + uv.y) * 20.0);
    out3 = vec4(0.0, stripes * 0.5, stripes, 1.0);
}
