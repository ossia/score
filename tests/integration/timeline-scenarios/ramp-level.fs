/*{
  "DESCRIPTION": "Timeline-scenario probe: uniform gray driven by a single float control. An automation ramping `level` 0->1 over the interval makes the expected frame mean at timeline position T computable: mean = T / duration.",
  "CREDIT": "gfx test suite",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-TIMELINE"],
  "INPUTS": [
    { "NAME": "level", "TYPE": "float", "DEFAULT": 0.0, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    gl_FragColor = vec4(level, level, level, 1.0);
}
