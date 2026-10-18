/*{
  "DESCRIPTION": "Single-pass ISF: red when DATE.x holds a year (> 2000), black otherwise. Used by GfxProcessUniforms.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": []
}*/
void main()
{
  gl_FragColor = DATE.x > 2000.0 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
