/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:point3D INPUT emitted as (x,y,z,1). Inject p3=(x,y,z) with components in [0,1]; readback MUST equal (round(255*x), round(255*y), round(255*z), 255). Proves a vec3 control inlet reaches the shader (std140 vec3 aligns to 16 bytes).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "p3", "TYPE": "point3D", "DEFAULT": [0.0, 0.0, 0.0] }
  ]
}*/

void main()
{
    gl_FragColor = vec4(p3.x, p3.y, p3.z, 1.0);
}
