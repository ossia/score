/*{
  "DESCRIPTION": "Control-value coverage: a single TYPE:bool INPUT. Default false (black). Inject on=true -> white, on=false -> black. Proves a bool control inlet (stored as an int uniform) reaches the shader.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS", "TEST-CONTROL"],
  "INPUTS": [
    { "NAME": "on", "TYPE": "bool", "DEFAULT": false }
  ]
}*/

void main()
{
    gl_FragColor = on ? vec4(1.0, 1.0, 1.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
