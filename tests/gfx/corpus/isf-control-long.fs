/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:long INPUT in numeric mode, emitted as a grayscale level = sel/10. Default 0. Inject sel=v (0..10) -> every readback pixel == round(255*v/10). Proves an integer (long) control inlet reaches the shader as an int uniform.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "sel", "TYPE": "long", "DEFAULT": 0, "MIN": 0, "MAX": 10 }
  ]
}*/

void main()
{
    gl_FragColor = vec4(vec3(float(sel) / 10.0), 1.0);
}
