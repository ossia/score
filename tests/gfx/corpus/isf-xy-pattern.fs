/*{
  "DESCRIPTION": "Single declared output carrying the same R = X, G = Y pattern as isf-mrt-pattern.fs, so the single-output and MRT paths can be asserted against one closed form through the window.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(isf_FragNormCoord.x, isf_FragNormCoord.y, 0.0, 1.0);
}
