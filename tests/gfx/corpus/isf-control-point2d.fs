/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:point2D INPUT emitted as (x,y,0,1). Inject pt=(x,y) with x,y in [0,1] and every readback pixel MUST equal (round(255*x), round(255*y), 0, 255). Proves a vec2 control inlet reaches the shader (and that std140 vec2 alignment matches).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "pt", "TYPE": "point2D", "DEFAULT": [0.0, 0.0] }
  ]
}*/

void main()
{
    gl_FragColor = vec4(pt.x, pt.y, 0.0, 1.0);
}
