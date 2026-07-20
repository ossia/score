/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:color INPUT output verbatim. Default is magenta so the shader is analytically distinguishable from an injected value: inject col=(r,g,b,a) and every readback pixel MUST equal round(255*col) (the offscreen target is a plain non-sRGB RGBA8, so the mapping is linear). Used by test_gfx_control to prove control-inlet injection reaches the material UBO.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "col", "TYPE": "color", "DEFAULT": [1.0, 0.0, 1.0, 1.0] }
  ]
}*/

void main()
{
    gl_FragColor = col;
}
