/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:float INPUT emitted as a grayscale level. Default 0 (black). Inject level=v and every readback pixel MUST equal round(255*v) on RGB, alpha 255. Proves a scalar float control inlet reaches the shader.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "level", "TYPE": "float", "DEFAULT": 0.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    gl_FragColor = vec4(vec3(level), 1.0);
}
