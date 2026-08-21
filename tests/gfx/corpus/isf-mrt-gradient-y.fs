/*{
  "DESCRIPTION": "Same vertical green ramp as isf-gradient-y.fs, but emitted through two declared OUTPUTS so the node takes the MRT path (colorCount > 1). Orientation of outA must be identical to isf-gradient-y.fs on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION", "TEST-MRT"],
  "INPUTS": [],
  "OUTPUTS": [
    { "NAME": "outA" },
    { "NAME": "outB" }
  ]
}*/

void main()
{
    outA = vec4(0.25, isf_FragNormCoord.y, 0.75, 1.0);
    outB = vec4(isf_FragNormCoord.y, 0.25, 0.75, 1.0);
}
