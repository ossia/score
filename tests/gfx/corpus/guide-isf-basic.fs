/*{
  "DESCRIPTION": "Guide example 1 -- a minimal ISF fragment shader. Paints a horizontal ramp in the red channel, scales it by a float INPUT, and keeps green at a constant so a reader can tell 'the control did nothing' from 'the shader did not run'. isf_FragNormCoord is bottom-left origin, which is the ISF convention.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "CATEGORIES": ["GUIDE"],
  "INPUTS": [
    { "NAME": "intensity", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    isf_FragColor = vec4(uv.x * intensity, 0.5, 0.0, 1.0);
}
