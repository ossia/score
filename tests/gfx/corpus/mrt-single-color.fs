/*{
  "DESCRIPTION": "Simplest MRT test: single color output declared via OUTPUTS. Should display a red-green gradient identical to what a normal ISF shader would produce. If black = MRT pipeline broken.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MRT"],
  "INPUTS": [],
  "OUTPUTS": [
    { "NAME": "colorOut" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    colorOut = vec4(uv, 0.0, 1.0);
}
