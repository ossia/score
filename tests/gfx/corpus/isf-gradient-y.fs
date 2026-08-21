/*{
  "DESCRIPTION": "Vertical green ramp: green = isf_FragNormCoord.y, with constant red 0.25 / blue 0.75 as garbage detectors. ISF places the origin of isf_FragNormCoord at the BOTTOM-left, so green is 1.0 at the TOP of the delivered image and 0.0 at the BOTTOM. Single color output: this is the reference orientation every other path must match.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(0.25, isf_FragNormCoord.y, 0.75, 1.0);
}
