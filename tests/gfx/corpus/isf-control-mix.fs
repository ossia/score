/*{
  "DESCRIPTION": "Control-value coverage with TWO control inlets interacting: color 'base' scaled by float 'gain'. Output = vec4(base.rgb * gain, 1). Exercises std140 packing of a color (vec4, 16-byte aligned) followed by a float, and asserts BOTH inlets land at the right offset: inject base=(r,g,b,a), gain=g -> readback rgb == round(255*base.rgb*g).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "base", "TYPE": "color", "DEFAULT": [1.0, 1.0, 1.0, 1.0] },
    { "NAME": "gain", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    gl_FragColor = vec4(base.rgb * gain, 1.0);
}
