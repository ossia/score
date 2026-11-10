/*{
  "DESCRIPTION": "Control-value coverage: a TYPE:float INPUT followed by a TYPE:point4D INPUT, so the vec4 starts on the next 16-byte boundary of the std140 block. Left half: (p4.x, p4.y, p4.z, 1); right half: (p4.w, level, 0, 1). Inject p4=(x,y,z,w) and level=l with components in [0,1]; readback MUST equal round(255*c) per component on each half.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "level", "TYPE": "float", "DEFAULT": 1.0 },
    { "NAME": "p4", "TYPE": "point4D", "DEFAULT": [0.2, 0.4, 0.6, 0.8] }
  ]
}*/

void main()
{
    if(isf_FragNormCoord.x < 0.5)
        gl_FragColor = vec4(p4.x, p4.y, p4.z, 1.0);
    else
        gl_FragColor = vec4(p4.w, level, 0.0, 1.0);
}
