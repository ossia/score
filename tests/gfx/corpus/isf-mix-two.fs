/*{
  "DESCRIPTION": "Averages two image inputs (a + b) / 2. Used to build a 'diamond' topology where ONE upstream node feeds BOTH image inputs: upstream.out0 -> mix.a AND upstream.out0 -> mix.b. That gives the single upstream renderer TWO outgoing edges inside one render list, which is the exact condition that reproduced the ISF persistent-SSBO double-swap (R3-N1). If the upstream advances consistently, a==b and the average equals that value; if it double-swaps, a and b diverge.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "a", "TYPE": "image" },
    { "NAME": "b", "TYPE": "image" }
  ]
}*/

void main()
{
    vec4 ca = IMG_THIS_PIXEL(a);
    vec4 cb = IMG_THIS_PIXEL(b);
    gl_FragColor = 0.5 * (ca + cb);
}
