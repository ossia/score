/*{
  "DESCRIPTION": "Simplest possible ISF shader: solid color output. No inputs, no passes, no special features. If the most basic ISF pipeline works: full-screen magenta. If broken: black or no output.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-BASIC"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
